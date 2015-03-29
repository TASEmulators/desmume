namespace pfc {

	template<typename t_storage>
	class _avltree_node : public _list_node<t_storage> {
	public:
		typedef _list_node<t_storage> t_node;
		typedef _avltree_node<t_storage> t_self;
		template<typename t_param> _avltree_node(t_param const& param) : t_node(param), m_left(), m_right(), m_depth() {}

		typedef refcounted_object_ptr_t<t_self> t_ptr;
		typedef t_self* t_rawptr;
		
		t_ptr m_left, m_right;
		t_rawptr m_parent;

		t_size m_depth;

		void link_left(t_self* ptr) throw() {
			m_left = ptr;
			if (ptr != NULL) ptr->m_parent = this;
		}
		void link_right(t_self* ptr) throw() {
			m_right = ptr;
			if (ptr != NULL) ptr->m_parent = this;
		}

		void link_child(bool which,t_self* ptr) throw() {
			(which ? m_right : m_left) = ptr;
			if (ptr != NULL) ptr->m_parent = this;
		}

		void unlink() throw() {
			m_left.release(); m_right.release(); m_parent = NULL; m_depth = 0;
		}

		inline void add_ref() throw() {this->refcount_add_ref();}
		inline void release() throw() {this->refcount_release();}

		inline t_rawptr child(bool which) const throw() {return which ? &*m_right : &*m_left;}
		inline bool which_child(const t_self* ptr) const throw() {return ptr == &*m_right;}

		

		t_rawptr step(bool direction) throw() {
			t_self* walk = this;
			for(;;) {
				t_self* t = walk->child(direction);
				if (t != NULL) return t->peakchild(!direction);
				for(;;) {
					t = walk->m_parent;
					if (t == NULL) return NULL;
					if (t->which_child(walk) != direction) return t;
					walk = t;
				}
			}
		}
		t_rawptr peakchild(bool direction) throw() {
			t_self* walk = this;
			for(;;) {
				t_rawptr next = walk->child(direction);
				if (next == NULL) return walk;
				walk = next;
			}
		}
		t_node * prev() throw() {return step(false);}
		t_node * next() throw() {return step(true);}
	private:
		~_avltree_node() throw() {}
	};

	
	template<typename t_storage,typename t_comparator = comparator_default>
	class avltree_t {
	public:
		typedef avltree_t<t_storage,t_comparator> t_self;
		typedef pfc::const_iterator<t_storage> const_iterator;
		typedef pfc::iterator<t_storage> iterator;
		typedef t_storage t_item;
	private:
		typedef _avltree_node<t_storage> t_node;
#if 1//MSVC8 bug fix
		typedef refcounted_object_ptr_t<t_node> t_nodeptr;
		typedef t_node * t_noderawptr;
#else
		typedef typename t_node::t_ptr t_nodeptr;
		typedef typename t_node::t_rawptr t_noderawptr;
#endif


		template<typename t_item1,typename t_item2>
		inline static int compare(const t_item1 & p_item1, const t_item2 & p_item2) {
			return t_comparator::compare(p_item1,p_item2);
		}

		t_nodeptr m_root;

		static t_size calc_depth(const t_nodeptr & ptr)
		{
			return ptr.is_valid() ? 1+ptr->m_depth : 0;
		}

		static void recalc_depth(t_nodeptr const& ptr) {
			ptr->m_depth = pfc::max_t(calc_depth(ptr->m_left), calc_depth(ptr->m_right));
		}

		static void assert_children(t_nodeptr ptr) {
			PFC_ASSERT(ptr->m_depth == pfc::max_t(calc_depth(ptr->m_left),calc_depth(ptr->m_right)) );
		}

		static t_ssize test_depth(t_nodeptr const& ptr)
		{
			if (ptr==0) return 0;
			else return calc_depth(ptr->m_right) - calc_depth(ptr->m_left);
		}

		static t_nodeptr extract_left_leaf(t_nodeptr & p_base) {
			if (p_base->m_left != NULL) {
				t_nodeptr ret = extract_left_leaf(p_base->m_left);
				recalc_depth(p_base);
				g_rebalance(p_base);
				return ret;
			} else {
				t_nodeptr node = p_base;
				p_base = node->m_right;
				if (p_base.is_valid()) p_base->m_parent = node->m_parent;
				node->m_right.release();
				node->m_depth = 0;
				node->m_parent = NULL;
				return node;
			}
		}

		static t_nodeptr extract_right_leaf(t_nodeptr & p_base) {
			if (p_base->m_right != NULL) {
				t_nodeptr ret = extract_right_leaf(p_base->m_right);
				recalc_depth(p_base);
				g_rebalance(p_base);
				return ret;
			} else {
				t_nodeptr node = p_base;
				p_base = node->m_left;
				if (p_base.is_valid()) p_base->m_parent = node->m_parent;
				node->m_left.release();
				node->m_depth = 0;
				node->m_parent = NULL;
				return node;
			}
		}

		static void remove_internal(t_nodeptr & p_node) {
			t_nodeptr oldval = p_node;
			if (p_node->m_left.is_empty()) {
				p_node = p_node->m_right;
				if (p_node.is_valid()) p_node->m_parent = oldval->m_parent;
			} else if (p_node->m_right.is_empty()) {
				p_node = p_node->m_left;
				if (p_node.is_valid()) p_node->m_parent = oldval->m_parent;
			} else {
				t_nodeptr swap = extract_left_leaf(p_node->m_right);

				swap->link_left(&*oldval->m_left);
				swap->link_right(&*oldval->m_right);
				swap->m_parent = oldval->m_parent;
				recalc_depth(swap);
				p_node = swap;
			}
			oldval->unlink();
		}

		template<typename t_nodewalk,typename t_callback>
		static void __enum_items_recur(t_nodewalk * p_node,t_callback & p_callback) {
			if (p_node != NULL) {
				__enum_items_recur<t_nodewalk>(&*p_node->m_left,p_callback);
				p_callback (p_node->m_content);
				__enum_items_recur<t_nodewalk>(&*p_node->m_right,p_callback);			
			}
		}
		template<typename t_search>
		static t_node * g_find_or_add_node(t_nodeptr & p_base,t_node * parent,t_search const & p_search,bool & p_new)
		{
			if (p_base.is_empty()) {
				p_base = new t_node(p_search);
				p_base->m_parent = parent;
				p_new = true;
				return p_base.get_ptr();
			}

			PFC_ASSERT( p_base->m_parent == parent );

			int result = compare(p_base->m_content,p_search);
			if (result > 0) {
				t_node * ret = g_find_or_add_node<t_search>(p_base->m_left,&*p_base,p_search,p_new);
				if (p_new) {
					recalc_depth(p_base);
					g_rebalance(p_base);
				}
				return ret;
			} else if (result < 0) {
				t_node * ret = g_find_or_add_node<t_search>(p_base->m_right,&*p_base,p_search,p_new);
				if (p_new) {
					recalc_depth(p_base);
					g_rebalance(p_base);
				}
				return ret;
			} else {
				p_new = false;
				return p_base.get_ptr();
			}
		}



		template<typename t_search>
		static t_storage * g_find_or_add(t_nodeptr & p_base,t_node * parent,t_search const & p_search,bool & p_new) {
			return &g_find_or_add_node(p_base,parent,p_search,p_new)->m_content;
		}


		static void g_rotate_right(t_nodeptr & p_node) {
			t_nodeptr oldroot = p_node;
			t_nodeptr newroot = oldroot->m_right;
			oldroot->link_child(true, &*newroot->m_left);
			newroot->m_left   = oldroot;
			newroot->m_parent = oldroot->m_parent;
			oldroot->m_parent = &*newroot;
			recalc_depth(oldroot);
			recalc_depth(newroot);
			p_node = newroot;
		}

		static void g_rotate_left(t_nodeptr & p_node) {
			t_nodeptr oldroot = p_node;
			t_nodeptr newroot = oldroot->m_left;
			oldroot->link_child(false, &*newroot->m_right);
			newroot->m_right  = oldroot;
			newroot->m_parent = oldroot->m_parent;
			oldroot->m_parent = &*newroot;
			recalc_depth(oldroot);
			recalc_depth(newroot);
			p_node = newroot;
		}

		static void g_rebalance(t_nodeptr & p_node) {
			t_ssize balance = test_depth(p_node);
			if (balance > 1) {
				//right becomes root
				if (test_depth(p_node->m_right) < 0) {
					g_rotate_left(p_node->m_right);
				}
				g_rotate_right(p_node);
			} else if (balance < -1) {
				//left becomes root
				if (test_depth(p_node->m_left) > 0) {
					g_rotate_right(p_node->m_left);
				}
				g_rotate_left(p_node);
			}
			selftest(p_node);
		}

		template<typename t_search>
		static bool g_remove(t_nodeptr & p_node,t_search const & p_search) {
			if (p_node.is_empty()) return false;

			int result = compare(p_node->m_content,p_search);
			if (result == 0) {
				remove_internal(p_node);
				if (p_node != NULL) {
					recalc_depth(p_node);
					g_rebalance(p_node);
				}
				return true;
			} else {
				if (g_remove<t_search>(result > 0 ? p_node->m_left : p_node->m_right,p_search)) {
					recalc_depth(p_node);
					g_rebalance(p_node);
					return true;
				} else {
					return false;
				}
			}
		}

		static void selftest(t_nodeptr const& p_node) {
	#if 0 //def _DEBUG//SLOW!
			if (p_node != NULL) {
				selftest(p_node->m_left);
				selftest(p_node->m_right);
				assert_children(p_node);
				t_ssize delta = test_depth(p_node);
				PFC_ASSERT(delta >= -1 && delta <= 1);

				if (p_node->m_left.is_valid()) {
					PFC_ASSERT( p_node.get_ptr() == p_node->m_left->m_parent );
				}
				if (p_node->m_right.is_valid()) {
					PFC_ASSERT( p_node.get_ptr() == p_node->m_right->m_parent );
				}

				if (p_node->m_parent != NULL) {
					PFC_ASSERT(p_node == p_node->m_parent->m_left || p_node == p_node->m_parent->m_right);
				}
			}
	#endif
		}


		static t_size calc_count(const t_node * p_node) throw() {
			if (p_node != NULL) {
				return 1 + calc_count(&*p_node->m_left) + calc_count(&*p_node->m_right);
			} else {
				return 0;
			}
		}

		template<typename t_param>
		t_storage * __find_item_ptr(t_param const & p_item) const {
			t_node* ptr = &*m_root;
			while(ptr != NULL) {
				int result = compare(ptr->m_content,p_item);
				if (result > 0) ptr=&*ptr->m_left;
				else if (result < 0) ptr=&*ptr->m_right;
				else return &ptr->m_content;
			}
			return NULL;
		}

		template<bool inclusive,bool above,typename t_search> t_storage * __find_nearest(const t_search & p_search) const {
			t_node * ptr = &*m_root;
			t_storage * found = NULL;
			while(ptr != NULL) {
				int result = compare(ptr->m_content,p_search);
				if (above) result = -result;
				if (inclusive && result == 0) {
					//direct hit
					found = &ptr->m_content;
					break;
				} else if (result < 0) {
					//match
					found = &ptr->m_content;
					ptr = ptr->child(!above);
				} else {
					//mismatch
					ptr = ptr->child(above);
				}
			}
			return found;
		}
	public:
		avltree_t() : m_root(NULL) {}
		~avltree_t() {reset();}
		const t_self & operator=(const t_self & p_other) {__copy(p_other);return *this;}
		avltree_t(const t_self & p_other) : m_root(NULL) {try{__copy(p_other);} catch(...) {remove_all(); throw;}}

		template<typename t_other> const t_self & operator=(const t_other & p_other) {copy_list_enumerated(*this,p_other);return *this;}
		template<typename t_other> avltree_t(const t_other & p_other) : m_root(NULL) {try{copy_list_enumerated(*this,p_other);}catch(...){remove_all(); throw;}}


		template<bool inclusive,bool above,typename t_search> const t_storage * find_nearest_item(const t_search & p_search) const {
			return __find_nearest<inclusive,above>(p_search);
		}

		template<bool inclusive,bool above,typename t_search> t_storage * find_nearest_item(const t_search & p_search) {
			return __find_nearest<inclusive,above>(p_search);
		}

		template<typename t_param>
		t_storage & add_item(t_param const & p_item) {
			bool dummy;
			return add_item_ex(p_item,dummy);
		}

		template<typename t_param>
		t_self & operator+=(const t_param & p_item) {add_item(p_item);return *this;}

		template<typename t_param>
		t_self & operator-=(const t_param & p_item) {remove_item(p_item);return *this;}

		//! Returns true when the list has been altered, false when the item was already present before.
		template<typename t_param>
		bool add_item_check(t_param const & item) {
			bool isNew = false;
			g_find_or_add(m_root,NULL,item,isNew);
			selftest(m_root);
			return isNew;
		}
		template<typename t_param>
		t_storage & add_item_ex(t_param const & p_item,bool & p_isnew) {
			t_storage * ret = g_find_or_add(m_root,NULL,p_item,p_isnew);
			selftest(m_root);
			return *ret;
		}

		template<typename t_param>
		void set_item(const t_param & p_item) {
			bool isnew;
			t_storage & found = add_item_ex(p_item,isnew);
			if (isnew) found = p_item;
		}

		template<typename t_param>
		const t_storage * find_item_ptr(t_param const & p_item) const {
			return __find_item_ptr(p_item);
		}

		//! WARNING: caller must not alter the item in a way that changes the sort order.
		template<typename t_param>
		t_storage * find_item_ptr(t_param const & p_item) {
			return __find_item_ptr(p_item);
		}

		template<typename t_param>
		bool contains(const t_param & p_item) const {
			return find_item_ptr(p_item) != NULL;
		}
		
		//! Same as contains().
		template<typename t_param>
		bool have_item(const t_param & p_item) const {return contains(p_item);}

		void remove_all() throw() {
			_unlink_recur(m_root);
			m_root.release();
		}

		bool remove(const_iterator const& iter) {
			PFC_ASSERT(iter.is_valid());
			return remove_item(*iter);//OPTIMIZEME
			//should never return false unless there's a bug in calling code
		}

		template<typename t_param>
		bool remove_item(t_param const & p_item) {
			bool ret = g_remove<t_param>(m_root,p_item);
			selftest(m_root);
			return ret;
		}

		t_size get_count() const throw() {
			return calc_count(&*m_root);
		}

		template<typename t_callback>
		void enumerate(t_callback & p_callback) const {
			__enum_items_recur<const t_node>(&*m_root,p_callback);
		}

		//! Allows callback to modify the tree content.
		//! WARNING: items must not be altered in a way that changes their sort order.
		template<typename t_callback>
		void _enumerate_var(t_callback & p_callback) { __enum_items_recur<t_node>(&*m_root,p_callback); }

		template<typename t_param> iterator insert(const t_param & p_item) {
			bool isNew;
			t_node * ret = g_find_or_add_node(m_root,NULL,p_item,isNew);
			selftest(m_root);
			return ret;
		}

		//deprecated backwards compatibility method wrappers
		template<typename t_param> t_storage & add(const t_param & p_item) {return add_item(p_item);}
		template<typename t_param> t_storage & add_ex(const t_param & p_item,bool & p_isnew) {return add_item_ex(p_item,p_isnew);}
		template<typename t_param> const t_storage * find_ptr(t_param const & p_item) const {return find_item_ptr(p_item);}
		template<typename t_param> t_storage * find_ptr(t_param const & p_item) {return find_item_ptr(p_item);}
		template<typename t_param> bool exists(t_param const & p_item) const {return have_item(p_item);}
		void reset() {remove_all();}

		
		
		
		const_iterator first() const throw() {return _firstlast(false);}
		const_iterator last() const throw() {return _firstlast(true);}
		//Unsafe! Caller must not modify items in a way that changes sort order!
		iterator _first_var() { return _firstlast(false); }
		//Unsafe! Caller must not modify items in a way that changes sort order!
		iterator _last_var() { return _firstlast(true); }

		template<typename t_param> bool get_first(t_param & p_item) const throw() {
			const_iterator iter = first();
			if (!iter.is_valid()) return false;
			p_item = *iter;
			return true;
		}
		template<typename t_param> bool get_last(t_param & p_item) const throw() {
			const_iterator iter = last();
			if (!iter.is_valid()) return false;
			p_item = *iter;
			return true;
		}

		static bool equals(const t_self & v1, const t_self & v2) {
			return listEquals(v1,v2);
		}
		bool operator==(const t_self & other) const {return equals(*this,other);}
		bool operator!=(const t_self & other) const {return !equals(*this,other);}

	private:
		static void _unlink_recur(t_nodeptr & node) {
			if (node.is_valid()) {
				_unlink_recur(node->m_left);
				_unlink_recur(node->m_right);
				node->unlink();
			}
		}
		t_node* _firstlast(bool which) const throw() {
			if (m_root.is_empty()) return NULL;
			for(t_node * walk = &*m_root; ; ) {
				t_node * next = walk->child(which);
				if (next == NULL) return walk;
				PFC_ASSERT( next->m_parent == walk );
				walk = next;
			}
		}
		static t_nodeptr __copy_recur(t_node * p_source,t_node * parent) {
			if (p_source == NULL) {
				return NULL;
			} else {
				t_nodeptr newnode = new t_node(p_source->m_content);
				newnode->m_depth = p_source->m_depth;
				newnode->m_left = __copy_recur(&*p_source->m_left,&*newnode);
				newnode->m_right = __copy_recur(&*p_source->m_right,&*newnode);
				newnode->m_parent = parent;
				return newnode;
			}
		}

		void __copy(const t_self & p_other) {
			reset();
			m_root = __copy_recur(&*p_other.m_root,NULL);
			selftest(m_root);
		}
	};


	template<typename t_storage,typename t_comparator>
	class traits_t<avltree_t<t_storage,t_comparator> > : public traits_default_movable {};
}
