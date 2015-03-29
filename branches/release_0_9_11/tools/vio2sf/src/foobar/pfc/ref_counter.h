namespace pfc {
	class counter {
	public:
		counter(long p_val = 0) : m_val(p_val) {}
#ifdef _WINDOWS
		long operator++() throw() {return InterlockedIncrement(&m_val);}
		long operator--() throw() {return InterlockedDecrement(&m_val);}
		long operator++(int) throw() {return InterlockedIncrement(&m_val)-1;}
		long operator--(int) throw() {return InterlockedDecrement(&m_val)+1;}
#else
		long operator++() {return ++m_val;}
		long operator--() {return --m_val;}
		long operator++(int) {return m_val++;}
		long operator--(int) {return m_val--;}
#pragma message("PORTME")
#endif
		operator long() const throw() {return m_val;}
		typedef long t_val;
	private:
		volatile long m_val;
	};

	typedef counter refcounter;

	class NOVTABLE refcounted_object_root
	{
	public:
		void refcount_add_ref() throw() {++m_counter;}
		void refcount_release() throw() {if (--m_counter == 0) delete this;}
		void _refcount_release_temporary() throw() {--m_counter;}//for internal use only!
	protected:
		refcounted_object_root() {}
		virtual ~refcounted_object_root() {}
	private:
		refcounter m_counter;
	};

	template<typename T>
	class refcounted_object_ptr_t {
	private:
		typedef refcounted_object_ptr_t<T> t_self;
	public:
		inline refcounted_object_ptr_t() throw() : m_ptr(NULL) {}
		inline refcounted_object_ptr_t(T* p_ptr) throw() : m_ptr(NULL) {copy(p_ptr);}
		inline refcounted_object_ptr_t(const t_self & p_source) throw() : m_ptr(NULL) {copy(p_source);}

		template<typename t_source>
		inline refcounted_object_ptr_t(t_source * p_ptr) throw() : m_ptr(NULL) {copy(p_ptr);}

		template<typename t_source>
		inline refcounted_object_ptr_t(const refcounted_object_ptr_t<t_source> & p_source) throw() : m_ptr(NULL) {copy(p_source);}

		inline ~refcounted_object_ptr_t() throw() {if (m_ptr != NULL) m_ptr->refcount_release();}
		
		template<typename t_source>
		inline void copy(t_source * p_ptr) throw() {
			T* torel = pfc::replace_t(m_ptr,pfc::safe_ptr_cast<T>(p_ptr));
			if (m_ptr != NULL) m_ptr->refcount_add_ref();
			if (torel != NULL) torel->refcount_release();
			
		}

		template<typename t_source>
		inline void copy(const refcounted_object_ptr_t<t_source> & p_source) throw() {copy(p_source.get_ptr());}


		inline const t_self & operator=(const t_self & p_source) throw() {copy(p_source); return *this;}
		inline const t_self & operator=(T * p_ptr) throw() {copy(p_ptr); return *this;}

		template<typename t_source> inline t_self & operator=(const refcounted_object_ptr_t<t_source> & p_source) throw() {copy(p_source); return *this;}
		template<typename t_source> inline t_self & operator=(t_source * p_ptr) throw() {copy(p_ptr); return *this;}
		
		inline void release() throw() {
			T * temp = pfc::replace_t(m_ptr,(T*)NULL);
			if (temp != NULL) temp->refcount_release();
		}


		inline T& operator*() const throw() {return *m_ptr;}

		inline T* operator->() const throw() {PFC_ASSERT(m_ptr != NULL);return m_ptr;}

		inline T* get_ptr() const throw() {return m_ptr;}
		
		inline bool is_valid() const throw() {return m_ptr != NULL;}
		inline bool is_empty() const throw() {return m_ptr == NULL;}

		inline bool operator==(const t_self & p_item) const throw() {return m_ptr == p_item.get_ptr();}
		inline bool operator!=(const t_self & p_item) const throw() {return m_ptr != p_item.get_ptr();}
		inline bool operator>(const t_self & p_item) const throw() {return m_ptr > p_item.get_ptr();}
		inline bool operator<(const t_self & p_item) const throw() {return m_ptr < p_item.get_ptr();}


		inline T* __unsafe_duplicate() const throw()//should not be used ! temporary !
		{
			if (m_ptr) m_ptr->refcount_add_ref();
			return m_ptr;
		}

		inline T* detach() throw() {//should not be used ! temporary !
			T* ret = m_ptr;
			m_ptr = 0;
			return ret;
		}

		inline void attach(T * p_ptr) throw() {//should not be used ! temporary !
			release();
			m_ptr = p_ptr;
		}
		inline t_self & operator<<(t_self & p_source) throw() {attach(p_source.detach());return *this;}
		inline t_self & operator>>(t_self & p_dest) throw() {p_dest.attach(detach());return *this;}
	private:
		T* m_ptr;
	};

	template<typename T>
	class traits_t<refcounted_object_ptr_t<T> > : public traits_default {
	public:
		enum { realloc_safe = true, constructor_may_fail = false};
	};

};