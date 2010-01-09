namespace pfc {

	static void * raw_malloc(t_size p_size) {
		return p_size > 0 ? new_ptr_check_t(malloc(p_size)) : NULL;
	}

	static void raw_free(void * p_block) throw() {free(p_block);}

	static void* raw_realloc(void * p_ptr,t_size p_size) {
		if (p_size == 0) {raw_free(p_ptr); return NULL;}
		else if (p_ptr == NULL) return raw_malloc(p_size);
		else return pfc::new_ptr_check_t(::realloc(p_ptr,p_size));
	}

	static bool raw_realloc_inplace(void * p_block,t_size p_size) throw() {
		if (p_block == NULL) return p_size == 0;
#ifdef _MSC_VER
		if (p_size == 0) return false;
		return _expand(p_block,p_size) != NULL;
#else
		return false;
#endif
	}

	template<typename T>
	t_size calc_array_width(t_size p_width) {
		return pfc::mul_safe_t<std::bad_alloc,t_size>(p_width,sizeof(T));
	}

	template<typename T>
	T * __raw_malloc_t(t_size p_size) {
		return reinterpret_cast<T*>(raw_malloc(calc_array_width<T>(p_size)));
	}

	template<typename T>
	void __raw_free_t(T * p_block) throw() {
		raw_free(reinterpret_cast<void*>(p_block));
	}

	template<typename T>
	T * __raw_realloc_t(T * p_block,t_size p_size) {
		return reinterpret_cast<T*>(raw_realloc(p_block,calc_array_width<T>(p_size)));
	}
	
	template<typename T>
	bool __raw_realloc_inplace_t(T * p_block,t_size p_size) {
		return raw_realloc_inplace(p_block,calc_array_width<T>(p_size));
	}


	template<typename t_exception,typename t_int>
	inline t_int safe_shift_left_t(t_int p_val,t_size p_shift = 1) {
		t_int newval = p_val << p_shift;
		if (newval >> p_shift != p_val) throw t_exception();
		return newval;
	}

	template<typename t_item> class alloc_dummy {
	private: typedef alloc_dummy<t_item> t_self;
	public:
		alloc_dummy() {}
		void set_size(t_size p_size) {throw pfc::exception_not_implemented();}
		t_size get_size() const {throw pfc::exception_not_implemented();}
		const t_item & operator[](t_size p_index) const {throw pfc::exception_not_implemented();}
		t_item & operator[](t_size p_index) {throw pfc::exception_not_implemented();}

		bool is_ptr_owned(const void * p_item) const {return false;}

		//set to true when we prioritize speed over memory usage
		enum { alloc_prioritizes_speed = false };

		//not mandatory
		const t_item * get_ptr() const {throw pfc::exception_not_implemented();}
		t_item * get_ptr() {throw pfc::exception_not_implemented();}
		void prealloc(t_size) {throw pfc::exception_not_implemented();}
		void force_reset() {throw pfc::exception_not_implemented();}
	private:
		const t_self & operator=(const t_self &) {throw pfc::exception_not_implemented();}
		alloc_dummy(const t_self&) {throw pfc::exception_not_implemented();}
	};

	template<typename t_item>
	bool is_pointer_in_range(const t_item * p_buffer,t_size p_buffer_size,const void * p_pointer) {
		return p_pointer >= reinterpret_cast<const void*>(p_buffer) && p_pointer < reinterpret_cast<const void*>(p_buffer + p_buffer_size);
	}


	//! Simple inefficient fully portable allocator.
	template<typename t_item> class alloc_simple {
	private: typedef alloc_simple<t_item> t_self;
	public:
		alloc_simple() : m_data(NULL), m_size(0) {}
		void set_size(t_size p_size) {
			if (p_size != m_size) {
				t_item * l_data = NULL;
				if (p_size > 0) l_data = new t_item[p_size];
				try {
					pfc::memcpy_t(l_data,m_data,pfc::min_t(m_size,p_size));
				} catch(...) {
					delete[] l_data;
					throw;
				}
				delete[] m_data;
				m_data = l_data;
				m_size = p_size;
			}
		}
		t_size get_size() const {return m_size;}
		const t_item & operator[](t_size p_index) const {PFC_ASSERT(p_index < m_size); return m_data[p_index];}
		t_item & operator[](t_size p_index) {PFC_ASSERT(p_index < m_size); return m_data[p_index];}
		bool is_ptr_owned(const void * p_item) const {return is_pointer_in_range(get_ptr(),get_size(),p_item);}

		enum { alloc_prioritizes_speed = false };

		t_item * get_ptr() {return m_data;}
		const t_item * get_ptr() const {return m_data;}

		void prealloc(t_size) {}
		void force_reset() {set_size(0);}

		~alloc_simple() {delete[] m_data;}
	private:
		const t_self & operator=(const t_self &) {throw pfc::exception_not_implemented();}
		alloc_simple(const t_self&) {throw pfc::exception_not_implemented();}

		t_item * m_data;
		t_size m_size;
	};

	template<typename t_item> class __array_fast_helper_t {
	private:
		typedef __array_fast_helper_t<t_item> t_self;
	public:
		__array_fast_helper_t() : m_buffer(NULL), m_size_total(0), m_size(0) {}
		

		void set_size(t_size p_size,t_size p_size_total) {
			PFC_ASSERT(p_size <= p_size_total);
			PFC_ASSERT(m_size <= m_size_total);
			if (p_size_total > m_size_total) {
				resize_storage(p_size_total);
				resize_content(p_size);
			} else {
				resize_content(p_size);
				resize_storage(p_size_total);
			}
		}



		t_size get_size() const {return m_size;}
		t_size get_size_total() const {return m_size_total;}
		const t_item & operator[](t_size p_index) const {PFC_ASSERT(p_index < m_size); return m_buffer[p_index];}
		t_item & operator[](t_size p_index) {PFC_ASSERT(p_index < m_size); return m_buffer[p_index];}
		~__array_fast_helper_t() {
			set_size(0,0);
		}
		t_item * get_ptr() {return m_buffer;}
		const t_item * get_ptr() const {return m_buffer;}
		bool is_ptr_owned(const void * p_item) const {return is_pointer_in_range(m_buffer,m_size_total,p_item);}
	private:
		const t_self & operator=(const t_self &) {throw pfc::exception_not_implemented();}
		__array_fast_helper_t(const t_self &) {throw pfc::exception_not_implemented();}


		void resize_content(t_size p_size) {
			if (traits_t<t_item>::needs_constructor || traits_t<t_item>::needs_destructor) {
				if (p_size > m_size) {//expand
					do {
						__unsafe__in_place_constructor_t(m_buffer[m_size]);
						m_size++;
					} while(m_size < p_size);
				} else if (p_size < m_size) {
					__unsafe__in_place_destructor_array_t(m_buffer + p_size, m_size - p_size);
					m_size = p_size;
				}
			} else {
				m_size = p_size;
			}
		}

		void resize_storage(t_size p_size) {
			PFC_ASSERT( m_size <= m_size_total );
			PFC_ASSERT( m_size <= p_size );
			if (m_size_total != p_size) {
				if (pfc::traits_t<t_item>::realloc_safe) {
					m_buffer = pfc::__raw_realloc_t(m_buffer,p_size);
					m_size_total = p_size;
				} else if (__raw_realloc_inplace_t(m_buffer,p_size)) {
					//success
					m_size_total = p_size;
				} else {
					t_item * newbuffer = pfc::__raw_malloc_t<t_item>(p_size);
					try {
						pfc::__unsafe__in_place_constructor_array_copy_t(newbuffer,m_size,m_buffer);
					} catch(...) {
						pfc::__raw_free_t(newbuffer);
						throw;
					}
					pfc::__unsafe__in_place_destructor_array_t(m_buffer,m_size);
					pfc::__raw_free_t(m_buffer);
					m_buffer = newbuffer;
					m_size_total = p_size;
				}
			}
		}

		t_item * m_buffer;
		t_size m_size,m_size_total;
	};

	template<typename t_item> class alloc_standard {
	private: typedef alloc_standard<t_item> t_self;
	public:
		alloc_standard() {}
		void set_size(t_size p_size) {m_content.set_size(p_size,p_size);}
		
		t_size get_size() const {return m_content.get_size();}
		
		const t_item & operator[](t_size p_index) const {return m_content[p_index];}
		t_item & operator[](t_size p_index) {return m_content[p_index];}

		const t_item * get_ptr() const {return m_content.get_ptr();}
		t_item * get_ptr() {return m_content.get_ptr();}

		bool is_ptr_owned(const void * p_item) const {return m_content.is_ptr_owned(p_item);}
		void prealloc(t_size p_size) {}
		void force_reset() {set_size(0);}
		
		enum { alloc_prioritizes_speed = false };
	private:
		alloc_standard(const t_self &) {throw pfc::exception_not_implemented();}
		const t_self & operator=(const t_self&) {throw pfc::exception_not_implemented();}

		__array_fast_helper_t<t_item> m_content;
	};

	template<typename t_item> class alloc_fast {
	private: typedef alloc_fast<t_item> t_self;
	public:
		alloc_fast() {}

		void set_size(t_size p_size) {
			t_size size_base = m_data.get_size_total();
			if (size_base == 0) size_base = 1;
			while(size_base < p_size) {
				size_base = safe_shift_left_t<std::bad_alloc,t_size>(size_base,1);
			}
			while(size_base >> 2 > p_size) {
				size_base >>= 1;
			}
			m_data.set_size(p_size,size_base);
		}
		
		t_size get_size() const {return m_data.get_size();}
		const t_item & operator[](t_size p_index) const {return m_data[p_index];}
		t_item & operator[](t_size p_index) {return m_data[p_index];}

		const t_item * get_ptr() const {return m_data.get_ptr();}
		t_item * get_ptr() {return m_data.get_ptr();}
		bool is_ptr_owned(const void * p_item) const {return m_data.is_ptr_owned(p_item);}
		void prealloc(t_size) {}
		void force_reset() {m_data.set_size(0,0);}
		
		enum { alloc_prioritizes_speed = true };
	private:
		alloc_fast(const t_self &) {throw pfc::exception_not_implemented();}
		const t_self & operator=(const t_self&) {throw pfc::exception_not_implemented();}
		__array_fast_helper_t<t_item> m_data;
	};

	template<typename t_item> class alloc_fast_aggressive {
	private: typedef alloc_fast_aggressive<t_item> t_self;
	public:
		alloc_fast_aggressive() {}

		void set_size(t_size p_size) {
			t_size size_base = m_data.get_size_total();
			if (size_base == 0) size_base = 1;
			while(size_base < p_size) {
				size_base = safe_shift_left_t<std::bad_alloc,t_size>(size_base,1);
			}
			m_data.set_size(p_size,size_base);
		}

		void prealloc(t_size p_size) {
			if (p_size > 0) {
				t_size size_base = m_data.get_size_total();
				if (size_base == 0) size_base = 1;
				while(size_base < p_size) {
					size_base = safe_shift_left_t<std::bad_alloc,t_size>(size_base,1);
				}
				m_data.set_size(m_data.get_size(),size_base);
			}
		}
		
		t_size get_size() const {return m_data.get_size();}
		const t_item & operator[](t_size p_index) const {;return m_data[p_index];}
		t_item & operator[](t_size p_index) {return m_data[p_index];}

		const t_item * get_ptr() const {return m_data.get_ptr();}
		t_item * get_ptr() {return m_data.get_ptr();}
		bool is_ptr_owned(const void * p_item) const {return m_data.is_ptr_owned(p_item);}
		void force_reset() {m_data.set_size(0,0);}

		enum { alloc_prioritizes_speed = true };
	private:
		alloc_fast_aggressive(const t_self &) {throw pfc::exception_not_implemented();}
		const t_self & operator=(const t_self&) {throw pfc::exception_not_implemented();}
		__array_fast_helper_t<t_item> m_data;
	};

	template<t_size p_width> class alloc_fixed {
	public:
		template<typename t_item> class alloc {
		private: typedef alloc<t_item> t_self;
		public:
			alloc() : m_size(0) {}

			void set_size(t_size p_size) {
				static_assert<sizeof(m_array) == sizeof(t_item[p_width])>();

				if (p_size > p_width) throw pfc::exception_overflow();
				else if (p_size > m_size) {
					__unsafe__in_place_constructor_array_t(get_ptr()+m_size,p_size-m_size);
					m_size = p_size;
				} else if (p_size < m_size) {
					__unsafe__in_place_destructor_array_t(get_ptr()+p_size,m_size-p_size);
					m_size = p_size;
				}
			}

			~alloc() {
				if (pfc::traits_t<t_item>::needs_destructor) set_size(0);
			}

			t_size get_size() const {return m_size;}

			t_item * get_ptr() {return reinterpret_cast<t_item*>(&m_array);}
			const t_item * get_ptr() const {return reinterpret_cast<const t_item*>(&m_array);}

			const t_item & operator[](t_size n) const {return get_ptr()[n];}
			t_item & operator[](t_size n) {return get_ptr()[n];}
			bool is_ptr_owned(const void * p_item) const {return is_pointer_in_range(get_ptr(),p_width,p_item);}
			void prealloc(t_size) {}
			void force_reset() {set_size(0);}

			enum { alloc_prioritizes_speed = false };
		private:
			alloc(const t_self&) {throw pfc::exception_not_implemented();}
			const t_self& operator=(const t_self&) {throw pfc::exception_not_implemented();}

			t_uint8 m_array[sizeof(t_item[p_width])];
			t_size m_size;
		};
	};

	template<t_size p_width, template<typename> class t_alloc = alloc_standard > class alloc_hybrid {
	public:
		template<typename t_item> class alloc {
		private: typedef alloc<t_item> t_self;
		public:
			alloc() {}

			void set_size(t_size p_size) {
				if (p_size > p_width) {
					m_fixed.set_size(p_width);
					m_variable.set_size(p_size - p_width);
				} else {
					m_fixed.set_size(p_size);
					m_variable.set_size(0);
				}
			}

			t_item & operator[](t_size p_index) {
				PFC_ASSERT(p_index < get_size());
				if (p_index < p_width) return m_fixed[p_index];
				else return m_variable[p_index - p_width];
			}

			const t_item & operator[](t_size p_index) const {
				PFC_ASSERT(p_index < get_size());
				if (p_index < p_width) return m_fixed[p_index];
				else return m_variable[p_index - p_width];
			}

			t_size get_size() const {return m_fixed.get_size() + m_variable.get_size();}
			bool is_ptr_owned(const void * p_item) const {return m_fixed.is_ptr_owned(p_item) || m_variable.is_ptr_owned(p_item);}
			void prealloc(t_size p_size) {
				if (p_size > p_width) m_variable.prealloc(p_size - p_width);
			}
			void force_reset() {
				m_fixed.force_reset(); m_variable.force_reset();
			}
			enum { alloc_prioritizes_speed = t_alloc<t_item>::alloc_prioritizes_speed };
		private:
			alloc(const t_self&) {throw pfc::exception_not_implemented();}
			const t_self& operator=(const t_self&) {throw pfc::exception_not_implemented();}

			typename alloc_fixed<p_width>::template alloc<t_item> m_fixed;
			t_alloc<t_item> m_variable;
		};
	};

	template<typename t_item> class traits_t<alloc_simple<t_item> > : public traits_default_movable {};
	template<typename t_item> class traits_t<__array_fast_helper_t<t_item> > : public traits_default_movable {};
	template<typename t_item> class traits_t<alloc_standard<t_item> > : public pfc::traits_t<__array_fast_helper_t<t_item> > {};
	template<typename t_item> class traits_t<alloc_fast<t_item> > : public pfc::traits_t<__array_fast_helper_t<t_item> > {};
	template<typename t_item> class traits_t<alloc_fast_aggressive<t_item> > : public pfc::traits_t<__array_fast_helper_t<t_item> > {};

#if 0//not working (compiler bug?)
	template<t_size p_width,typename t_item> class traits_t<typename alloc_fixed<p_width>::template alloc<t_item> > : public pfc::traits_t<t_item> {
	public:
		enum {
			needs_constructor = true, 
		};
	};

	template<t_size p_width,template<typename> class t_alloc,typename t_item>
	class traits_t<typename alloc_hybrid<p_width,t_alloc>::template alloc<t_item> > : public traits_combined<t_alloc,typename alloc_fixed<p_width>::template alloc<t_item> > {};
#endif
};
