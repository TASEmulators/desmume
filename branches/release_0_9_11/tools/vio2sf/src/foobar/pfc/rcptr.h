namespace pfc {

	class rc_container_base {
	public:
		long add_ref() throw() {
			return ++m_counter;
		}
		long release() throw() {
			long ret = --m_counter;
			if (ret == 0) PFC_ASSERT_NO_EXCEPTION( delete this );
			return ret;
		}
	protected:
		virtual ~rc_container_base() {}
	private:
		refcounter m_counter;
	};

	template<typename t_object>
	class rc_container_t : public rc_container_base {
	public:
		TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(rc_container_t,m_object)

		t_object m_object;
	};

	template<typename t_object>
	class rcptr_t {
	private:
		typedef rcptr_t<t_object> t_self;
		typedef rc_container_base t_container;
		typedef rc_container_t<t_object> t_container_impl;
	public:
		rcptr_t() throw() {__init();}
		rcptr_t(const t_self & p_source) throw() {__init(p_source);}
		t_self const & operator=(const t_self & p_source) throw() {__copy(p_source); return *this;}

		template<typename t_source>
		rcptr_t(const rcptr_t<t_source> & p_source) throw() {__init(p_source);}
		template<typename t_source>
		const t_self & operator=(const rcptr_t<t_source> & p_source) throw() {__copy(p_source); return *this;}

/*		template<typename t_object_cast>
		operator rcptr_t<t_object_cast>() const throw() {
			rcptr_t<t_object_cast> temp;
			if (is_valid()) temp.__set_from_cast(this->m_container,this->m_ptr);
			return temp;
		}*/

		
		template<typename t_other>
		bool operator==(const rcptr_t<t_other> & p_other) const throw() {
			return m_container == p_other.__container();
		}

		template<typename t_other>
		bool operator!=(const rcptr_t<t_other> & p_other) const throw() {
			return m_container != p_other.__container();
		}

		void __set_from_cast(t_container * p_container,t_object * p_ptr) throw() {
			//addref first because in rare cases this is the same pointer as the one we currently own
			if (p_container != NULL) p_container->add_ref();
			release();
			m_container = p_container;
			m_ptr = p_ptr;
		}

		bool is_valid() const throw() {return m_container != NULL;}
		bool is_empty() const throw() {return m_container == NULL;}


		~rcptr_t() throw() {release();}

		void release() throw() {
			t_container * temp = m_container;
			m_ptr = NULL;
			m_container = NULL;
			if (temp != NULL) temp->release();
		}


		template<typename t_object_cast>
		rcptr_t<t_object_cast> static_cast_t() const throw() {
			rcptr_t<t_object_cast> temp;
			if (is_valid()) temp.__set_from_cast(this->m_container,static_cast<t_object_cast*>(this->m_ptr));
			return temp;
		}

		void new_t() {
			on_new(new t_container_impl());
		}

		template<typename t_param1>
		void new_t(t_param1 const & p_param1) {
			on_new(new t_container_impl(p_param1));
		}

		template<typename t_param1,typename t_param2>
		void new_t(t_param1 const & p_param1, t_param2 const & p_param2) {
			on_new(new t_container_impl(p_param1,p_param2));
		}

		template<typename t_param1,typename t_param2,typename t_param3>
		void new_t(t_param1 const & p_param1, t_param2 const & p_param2,t_param3 const & p_param3) {
			on_new(new t_container_impl(p_param1,p_param2,p_param3));
		}

		template<typename t_param1,typename t_param2,typename t_param3,typename t_param4>
		void new_t(t_param1 const & p_param1, t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4) {
			on_new(new t_container_impl(p_param1,p_param2,p_param3,p_param4));
		}
		
		template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5>
		void new_t(t_param1 const & p_param1, t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4,t_param5 const & p_param5) {
			on_new(new t_container_impl(p_param1,p_param2,p_param3,p_param4,p_param5));
		}
		
		template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5,typename t_param6>
		void new_t(t_param1 const & p_param1, t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4,t_param5 const & p_param5,t_param6 const & p_param6) {
			on_new(new t_container_impl(p_param1,p_param2,p_param3,p_param4,p_param5,p_param6));
		}

		static t_self g_new_t() {
			t_self temp;
			temp.new_t();
			return temp;
		}

		template<typename t_param1>
		static t_self g_new_t(t_param1 const & p_param1) {
			t_self temp;
			temp.new_t(p_param1);
			return temp;
		}

		template<typename t_param1,typename t_param2>
		static t_self g_new_t(t_param1 const & p_param1,t_param2 const & p_param2) {
			t_self temp;
			temp.new_t(p_param1,p_param2);
			return temp;
		}

		template<typename t_param1,typename t_param2,typename t_param3>
		static t_self g_new_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3) {
			t_self temp;
			temp.new_t(p_param1,p_param2,p_param3);
			return temp;
		}

		template<typename t_param1,typename t_param2,typename t_param3,typename t_param4>
		static t_self g_new_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4) {
			t_self temp;
			temp.new_t(p_param1,p_param2,p_param3,p_param4);
			return temp;
		}

		template<typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5>
		static t_self g_new_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4,t_param5 const & p_param5) {
			t_self temp;
			temp.new_t(p_param1,p_param2,p_param3,p_param4,p_param5);
			return temp;
		}

		t_object & operator*() const throw() {return *this->m_ptr;}

		t_object * operator->() const throw() {return this->m_ptr;}


		t_container * __container() const throw() {return m_container;}
	private:
		void __init() throw() {m_container = NULL; m_ptr = NULL;}

		template<typename t_source>
		void __init(const rcptr_t<t_source> & p_source) throw() {
			m_container = p_source.__container();
			m_ptr = &*p_source;
			if (m_container != NULL) m_container->add_ref();
		}
		template<typename t_source>
		void __copy(const rcptr_t<t_source> & p_source) throw() {
			__set_from_cast(p_source.__container(),&*p_source);
		}
		void on_new(t_container_impl * p_container) throw() {
			this->release();
			p_container->add_ref();
			this->m_ptr = &p_container->m_object;
			this->m_container = p_container;
		}

		t_container * m_container;
		t_object * m_ptr;
	};

	template<typename t_object>
	rcptr_t<t_object> rcnew_t() {
		rcptr_t<t_object> temp;
		temp.new_t();
		return temp;		
	}

	template<typename t_object,typename t_param1>
	rcptr_t<t_object> rcnew_t(t_param1 const & p_param1) {
		rcptr_t<t_object> temp;
		temp.new_t(p_param1);
		return temp;		
	}

	template<typename t_object,typename t_param1,typename t_param2>
	rcptr_t<t_object> rcnew_t(t_param1 const & p_param1,t_param2 const & p_param2) {
		rcptr_t<t_object> temp;
		temp.new_t(p_param1,p_param2);
		return temp;		
	}

	template<typename t_object,typename t_param1,typename t_param2,typename t_param3>
	rcptr_t<t_object> rcnew_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3) {
		rcptr_t<t_object> temp;
		temp.new_t(p_param1,p_param2,p_param3);
		return temp;		
	}

	template<typename t_object,typename t_param1,typename t_param2,typename t_param3,typename t_param4>
	rcptr_t<t_object> rcnew_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4) {
		rcptr_t<t_object> temp;
		temp.new_t(p_param1,p_param2,p_param3,p_param4);
		return temp;		
	}

	template<typename t_object,typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5>
	rcptr_t<t_object> rcnew_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4,t_param5 const & p_param5) {
		rcptr_t<t_object> temp;
		temp.new_t(p_param1,p_param2,p_param3,p_param4,p_param5);
		return temp;
	}

	template<typename t_object,typename t_param1,typename t_param2,typename t_param3,typename t_param4,typename t_param5,typename t_param6>
	rcptr_t<t_object> rcnew_t(t_param1 const & p_param1,t_param2 const & p_param2,t_param3 const & p_param3,t_param4 const & p_param4,t_param5 const & p_param5,t_param6 const & p_param6) {
		rcptr_t<t_object> temp;
		temp.new_t(p_param1,p_param2,p_param3,p_param4,p_param5,p_param6);
		return temp;
	}

	class traits_rcptr : public traits_default {
	public:
		enum { realloc_safe = true, constructor_may_fail = false };
	};

	template<typename T> class traits_t<rcptr_t<T> > : public traits_rcptr {};
}