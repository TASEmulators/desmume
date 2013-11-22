#ifndef _MAP_T_H_INCLUDED_
#define _MAP_T_H_INCLUDED_

namespace pfc {
	PFC_DECLARE_EXCEPTION(exception_map_entry_not_found,exception,"Map entry not found");

	template<typename t_destination> class __map_overwrite_wrapper {
	public:
		__map_overwrite_wrapper(t_destination & p_destination) : m_destination(p_destination) {}
		template<typename t_key,typename t_value> void operator() (const t_key & p_key,const t_value & p_value) {m_destination.set(p_key,p_value);}
	private:
		t_destination & m_destination;
	};

	template<typename t_storage_key, typename t_storage_value, typename t_comparator = comparator_default>
	class map_t {
	private:
		typedef map_t<t_storage_key,t_storage_value,t_comparator> t_self;
	public:
		typedef t_storage_key t_key; typedef t_storage_value t_value;
		template<typename _t_key,typename _t_value>
		void set(const _t_key & p_key, const _t_value & p_value) {
			bool isnew;
			t_storage & storage = m_data.add_ex(t_search_set<_t_key,_t_value>(p_key,p_value), isnew);
			if (!isnew) storage.m_value = p_value;
		}

		template<typename _t_key>
		t_storage_value & find_or_add(_t_key const & p_key) {
			return m_data.add(t_search_query<_t_key>(p_key)).m_value;
		}

		template<typename _t_key>
		t_storage_value & find_or_add_ex(_t_key const & p_key,bool & p_isnew) {
			return m_data.add_ex(t_search_query<_t_key>(p_key),p_isnew).m_value;
		}

		template<typename _t_key>
		bool have_item(const _t_key & p_key) const {
			return m_data.have_item(t_search_query<_t_key>(p_key));
		}

		template<typename _t_key,typename _t_value>
		bool query(const _t_key & p_key,_t_value & p_value) const {
			const t_storage * storage = m_data.find_ptr(t_search_query<_t_key>(p_key));
			if (storage == NULL) return false;
			p_value = storage->m_value;
			return true;
		}

		template<typename _t_key>
		const t_storage_value & operator[] (const _t_key & p_key) const {
			const t_storage_value * ptr = query_ptr(p_key);
			if (ptr == NULL) throw exception_map_entry_not_found();
			return *ptr;
		}

		template<typename _t_key>
		t_storage_value & operator[] (const _t_key & p_key) {
			return find_or_add(p_key);
		}

		template<typename _t_key>
		const t_storage_value * query_ptr(const _t_key & p_key) const {
			const t_storage * storage = m_data.find_ptr(t_search_query<_t_key>(p_key));
			if (storage == NULL) return NULL;
			return &storage->m_value;
		}

		template<typename _t_key>
		t_storage_value * query_ptr(const _t_key & p_key) {
			t_storage * storage = m_data.find_ptr(t_search_query<_t_key>(p_key));
			if (storage == NULL) return NULL;
			return &storage->m_value;
		}

		template<bool inclusive,bool above,typename _t_key>
		const t_storage_value * query_nearest_ptr(_t_key & p_key) const {
			const t_storage * storage = m_data.find_nearest_item<inclusive,above>(t_search_query<_t_key>(p_key));
			if (storage == NULL) return NULL;
			p_key = storage->m_key;
			return &storage->m_value;
		}

		template<bool inclusive,bool above,typename _t_key>
		t_storage_value * query_nearest_ptr(_t_key & p_key) {
			t_storage * storage = m_data.find_nearest_item<inclusive,above>(t_search_query<_t_key>(p_key));
			if (storage == NULL) return NULL;
			p_key = storage->m_key;
			return &storage->m_value;
		}

		template<bool inclusive,bool above,typename _t_key,typename _t_value>
		bool query_nearest(_t_key & p_key,_t_value & p_value) const {
			const t_storage * storage = m_data.find_nearest_item<inclusive,above>(t_search_query<_t_key>(p_key));
			if (storage == NULL) return false;
			p_key = storage->m_key;
			p_value = storage->m_value;
			return true;
		}

		
		template<typename _t_key>
		bool remove(const _t_key & p_key) {
			return m_data.remove_item(t_search_query<_t_key>(p_key));
		}

		template<typename t_callback>
		void enumerate(t_callback & p_callback) const {
			m_data.enumerate(enumeration_wrapper<t_callback>(p_callback));
		}

		template<typename t_callback>
		void enumerate(t_callback & p_callback) {
			m_data._enumerate_var(enumeration_wrapper_var<t_callback>(p_callback));
		}


		t_size get_count() const throw() {return m_data.get_count();}

		void remove_all() throw() {m_data.remove_all();}

		template<typename t_source>
		void overwrite(const t_source & p_source) {
			__map_overwrite_wrapper<t_self> wrapper(*this);
			p_source.enumerate(wrapper);
		}

		//backwards compatibility method wrappers
		template<typename _t_key> bool exists(const _t_key & p_key) const {return have_item(p_key);}


		template<typename _t_key> bool get_first(_t_key & p_out) const {
			return m_data.get_first(t_retrieve_key<_t_key>(p_out));
		}

		template<typename _t_key> bool get_last(_t_key & p_out) const {
			return m_data.get_last(t_retrieve_key<_t_key>(p_out));
		}

	private:
		template<typename _t_key>
		struct t_retrieve_key {
			typedef t_retrieve_key<_t_key> t_self;
			t_retrieve_key(_t_key & p_key) : m_key(p_key) {}
			template<typename t_what> const t_self & operator=(const t_what & p_what) {m_key = p_what.m_key; return *this;}
			_t_key & m_key;
		};
		template<typename _t_key>
		struct t_search_query {
			t_search_query(const _t_key & p_key) : m_key(p_key) {}
			_t_key const & m_key;
		};
		template<typename _t_key,typename _t_value>
		struct t_search_set {
			t_search_set(const _t_key & p_key, const _t_value & p_value) : m_key(p_key), m_value(p_value) {}

			_t_key const & m_key;
			_t_value const & m_value;
		};

		struct t_storage {
			const t_storage_key m_key;
			t_storage_value m_value;
			
			template<typename _t_key>
			t_storage(t_search_query<_t_key> const & p_source) : m_key(p_source.m_key), m_value() {}

			template<typename _t_key,typename _t_value>
			t_storage(t_search_set<_t_key,_t_value> const & p_source) : m_key(p_source.m_key), m_value(p_source.m_value) {}

			static bool equals(const t_storage & v1, const t_storage & v2) {return v1.m_key == v2.m_key && v1.m_value == v2.m_value;}
			bool operator==(const t_storage & other) const {return equals(*this,other);}
			bool operator!=(const t_storage & other) const {return !equals(*this,other);}
		};

		class comparator_wrapper {
		public:
			template<typename t1,typename t2>
			inline static int compare(const t1 & p_item1,const t2 & p_item2) {
				return t_comparator::compare(p_item1.m_key,p_item2.m_key);
			}
		};

		template<typename t_callback>
		class enumeration_wrapper {
		public:
			enumeration_wrapper(t_callback & p_callback) : m_callback(p_callback) {}
			void operator()(const t_storage & p_item) {m_callback(p_item.m_key,p_item.m_value);}
		private:
			t_callback & m_callback;
		};

		template<typename t_callback>
		class enumeration_wrapper_var {
		public:
			enumeration_wrapper_var(t_callback & p_callback) : m_callback(p_callback) {}
			void operator()(t_storage & p_item) {m_callback(safe_cast<t_storage_key const&>(p_item.m_key),p_item.m_value);}
		private:
			t_callback & m_callback;
		};

		typedef avltree_t<t_storage,comparator_wrapper> t_content;

		t_content m_data;
	public:
		typedef traits_t<t_content> traits;
		typedef typename t_content::const_iterator const_iterator;
		typedef typename t_content::iterator iterator;

		iterator first() throw() {return m_data._first_var();}
		iterator last() throw() {return m_data._last_var();}
		const_iterator first() const throw() {return m_data.first();}
		const_iterator last() const throw() {return m_data.last();}

		static bool equals(const t_self & v1, const t_self & v2) {
			return t_content::equals(v1.m_data,v2.m_data);
		}
		bool operator==(const t_self & other) const {return equals(*this,other);}
		bool operator!=(const t_self & other) const {return !equals(*this,other);}
		
		bool remove(iterator const& iter) {
			PFC_ASSERT(iter.is_valid());
			return m_data.remove(iter);
			//should never return false unless there's a bug in calling code
		}
		bool remove(const_iterator const& iter) {
			PFC_ASSERT(iter.is_valid());
			return m_data.remove(iter);
			//should never return false unless there's a bug in calling code
		}
	};
}

#endif //_MAP_T_H_INCLUDED_
