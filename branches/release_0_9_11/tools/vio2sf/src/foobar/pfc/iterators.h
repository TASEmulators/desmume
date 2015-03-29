namespace pfc {
	//! Base class for list nodes. Implemented by list implementers.
	template<typename t_item> class _list_node : public refcounted_object_root {
	public:
		typedef _list_node<t_item> t_self;

		TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(_list_node,m_content)

		t_item m_content;

		virtual t_self * prev() throw() {return NULL;}
		virtual t_self * next() throw() {return NULL;}

		t_self * walk(bool forward) throw() {return forward ? next() : prev();}
	};

	template<typename t_item> class const_iterator {
	public:
		typedef _list_node<t_item> t_node;
		typedef refcounted_object_ptr_t<t_node> t_nodeptr;
		typedef const_iterator<t_item> t_self;
		
		bool is_empty() const throw() {return m_content.is_empty();}
		bool is_valid() const throw() {return m_content.is_valid();}
		void invalidate() throw() {m_content = NULL;}

		void walk(bool forward) throw() {m_content = m_content->walk(forward);}
		void prev() throw() {m_content = m_content->prev();}
		void next() throw() {m_content = m_content->next();}

		//! For internal use / list implementations only! Do not call!
		t_node* _node() const throw() {return m_content.get_ptr();}

		const_iterator() {}
		const_iterator(t_node* source) : m_content(source) {}
		const_iterator(t_nodeptr const & source) : m_content(source) {}

		const t_item& operator*() const throw() {return m_content->m_content;}
		const t_item* operator->() const throw() {return &m_content->m_content;}

		const t_self & operator++() throw() {this->next(); return *this;}
		const t_self & operator--() throw() {this->prev(); return *this;}
		t_self operator++(int) throw() {t_self old = *this; this->next(); return old;}
		t_self operator--(int) throw() {t_self old = *this; this->prev(); return old;}

		bool operator==(const t_self & other) const throw() {return this->m_content == other.m_content;}
		bool operator!=(const t_self & other) const throw() {return this->m_content != other.m_content;}
		bool operator> (const t_self & other) const throw() {return this->m_content >  other.m_content;}
		bool operator< (const t_self & other) const throw() {return this->m_content <  other.m_content;}
		bool operator>=(const t_self & other) const throw() {return this->m_content >= other.m_content;}
		bool operator<=(const t_self & other) const throw() {return this->m_content <= other.m_content;}
	protected:
		t_nodeptr m_content;
	};
	template<typename t_item> class iterator : public const_iterator<t_item> {
	public:
		typedef const_iterator<t_item> t_selfConst;
		typedef iterator<t_item> t_self;
		typedef _list_node<t_item> t_node;
		typedef refcounted_object_ptr_t<t_node> t_nodeptr;

		iterator() {}
		iterator(t_node* source) : t_selfConst(source) {}
		iterator(t_nodeptr const & source) : t_selfConst(source) {}

		t_item& operator*() const throw() {return this->m_content->m_content;}
		t_item* operator->() const throw() {return &this->m_content->m_content;}

		const t_self & operator++() throw() {this->next(); return *this;}
		const t_self & operator--() throw() {this->prev(); return *this;}
		t_self operator++(int) throw() {t_self old = *this; this->next(); return old;}
		t_self operator--(int) throw() {t_self old = *this; this->prev(); return old;}

		bool operator==(const t_self & other) const throw() {return this->m_content == other.m_content;}
		bool operator!=(const t_self & other) const throw() {return this->m_content != other.m_content;}
		bool operator> (const t_self & other) const throw() {return this->m_content >  other.m_content;}
		bool operator< (const t_self & other) const throw() {return this->m_content <  other.m_content;}
		bool operator>=(const t_self & other) const throw() {return this->m_content >= other.m_content;}
		bool operator<=(const t_self & other) const throw() {return this->m_content <= other.m_content;}
	};

	template<typename t_comparator = comparator_default>
	class comparator_list {
	public:
		template<typename t_list1, typename t_list2>
		static int compare(const t_list1 & p_list1, const t_list2 p_list2) {
			typename t_list1::const_iterator iter1 = p_list1.first();
			typename t_list2::const_iterator iter2 = p_list2.first();
			for(;;) {
				if (iter1.is_empty() && iter2.is_empty()) return 0;
				else if (iter1.is_empty()) return -1;
				else if (iter2.is_empty()) return 1;
				else {
					int state = t_comparator::compare(*iter1,*iter2);
					if (state != 0) return state;
				}
				++iter1; ++iter2;
			}
		}
	};

	template<typename t_list1, typename t_list2>
	static bool listEquals(const t_list1 & p_list1, const t_list2 & p_list2) {
		typename t_list1::const_iterator iter1 = p_list1.first();
		typename t_list2::const_iterator iter2 = p_list2.first();
		for(;;) {
			if (iter1.is_empty() && iter2.is_empty()) return true;
			else if (iter1.is_empty() || iter2.is_empty()) return false;
			else if (*iter1 != *iter2) return false;
			++iter1; ++iter2;
		}
	}
}
