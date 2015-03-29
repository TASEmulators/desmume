#ifdef _WIN32
namespace pfc {

	static void _COM_AddRef(IUnknown * ptr) {
		if (ptr != NULL) ptr->AddRef();
	}
	static void _COM_Release(IUnknown * ptr) {
		if (ptr != NULL) ptr->Release();
	}

	template<class T>
	class com_ptr_t {
	public:
		typedef com_ptr_t<T> t_self;

		inline com_ptr_t() throw() : m_ptr() {}
		template<typename source> inline com_ptr_t(source * p_ptr) throw() : m_ptr(p_ptr) {_COM_AddRef(m_ptr);;}
		inline com_ptr_t(const t_self & p_source) throw() : m_ptr(p_source.m_ptr) {_COM_AddRef(m_ptr);}
		template<typename source> inline com_ptr_t(const com_ptr_t<source> & p_source) throw() : m_ptr(p_source.get_ptr()) {_COM_AddRef(m_ptr);}

		inline ~com_ptr_t() throw() {_COM_Release(m_ptr);}
		
		inline void copy(T * p_ptr) throw() {
			_COM_Release(m_ptr);
			m_ptr = p_ptr;
			_COM_AddRef(m_ptr);
		}

		template<typename source> inline void copy(const com_ptr_t<source> & p_source) throw() {copy(p_source.get_ptr());}

		inline void attach(T * p_ptr) throw() {
			_COM_Release(m_ptr);
			m_ptr = p_ptr;
		}

		inline const t_self & operator=(const t_self & p_source) throw() {copy(p_source); return *this;}
		inline const t_self & operator=(T* p_source) throw() {copy(p_source); return *this;}
		template<typename source> inline const t_self & operator=(const com_ptr_t<source> & p_source) throw() {copy(p_source); return *this;}
		template<typename source> inline const t_self & operator=(source * p_ptr) throw() {copy(p_ptr); return *this;}

		inline void release() throw() {
			_COM_Release(m_ptr);
			m_ptr = NULL;
		}


		inline T* operator->() const throw() {assert(m_ptr);return m_ptr;}

		inline T* get_ptr() const throw() {return m_ptr;}
		
		inline T* duplicate_ptr() const throw() //should not be used ! temporary !
		{
			_COM_AddRef(m_ptr);
			return m_ptr;
		}

		inline T* detach() throw() {
			return replace_null_t(m_ptr);
		}

		inline bool is_valid() const throw() {return m_ptr != 0;}
		inline bool is_empty() const throw() {return m_ptr == 0;}

		inline bool operator==(const com_ptr_t<T> & p_item) const throw() {return m_ptr == p_item.m_ptr;}
		inline bool operator!=(const com_ptr_t<T> & p_item) const throw() {return m_ptr != p_item.m_ptr;}
		inline bool operator>(const com_ptr_t<T> & p_item) const throw() {return m_ptr > p_item.m_ptr;}
		inline bool operator<(const com_ptr_t<T> & p_item) const throw() {return m_ptr < p_item.m_ptr;}

		inline static void g_swap(com_ptr_t<T> & item1, com_ptr_t<T> & item2) throw() {
			pfc::swap_t(item1.m_ptr,item2.m_ptr);
		}

		inline T** receive_ptr() throw() {release();return &m_ptr;}

		inline t_self & operator<<(t_self & p_source) throw() {attach(p_source.detach());return *this;}
		inline t_self & operator>>(t_self & p_dest) throw() {p_dest.attach(detach());return *this;}
	private:
		T* m_ptr;
	};

}
#endif
