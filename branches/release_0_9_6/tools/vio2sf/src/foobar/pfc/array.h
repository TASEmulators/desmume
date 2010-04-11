#ifndef _PFC_ARRAY_H_
#define _PFC_ARRAY_H_

namespace pfc {

	template<typename t_item, template<typename> class t_alloc = alloc_standard> class array_t;


	//! Special simplififed version of array class that avoids stepping on landmines with classes without public copy operators/constructors.
	template<typename _t_item>
	class array_staticsize_t {
	public: typedef _t_item t_item;
	private: typedef array_staticsize_t<t_item> t_self;
	public:
		array_staticsize_t() : m_size(0), m_array(NULL) {}
		array_staticsize_t(t_size p_size) : m_array(new t_item[p_size]), m_size(p_size) {}
		~array_staticsize_t() {__release();}

		//! Copy constructor nonfunctional when data type is not copyable.
		array_staticsize_t(const t_self & p_source) : m_size(0), m_array(NULL) {
			*this = p_source;
		}

		//! Copy operator nonfunctional when data type is not copyable.
		const t_self & operator=(const t_self & p_source) {
			__release();
			
			//m_array = pfc::malloc_copy_t(p_source.get_size(),p_source.get_ptr());
			const t_size newsize = p_source.get_size();
			m_array = new t_item[newsize];
			m_size = newsize;
			for(t_size n = 0; n < newsize; n++) m_array[n] = p_source[n];
			return *this;
		}

		void set_size_discard(t_size p_size) {
			__release();
			if (p_size > 0) {
				m_array = new t_item[p_size];
				m_size = p_size;
			}
		}
		
		t_size get_size() const {return m_size;}
		const t_item * get_ptr() const {return m_array;}
		t_item * get_ptr() {return m_array;}

		const t_item & operator[](t_size p_index) const {PFC_ASSERT(p_index < get_size());return m_array[p_index];}
		t_item & operator[](t_size p_index) {PFC_ASSERT(p_index < get_size());return m_array[p_index];}
		
		template<typename t_source> bool is_owned(const t_source & p_item) {return pfc::is_pointer_in_range(get_ptr(),get_size(),&p_item);}
	private:
		void __release() {
			m_size = 0;
			delete[] pfc::replace_null_t(m_array);
		}
		t_item * m_array;
		t_size m_size;
	};

	template<typename t_to,typename t_from>
	inline void copy_array_t(t_to & p_to,const t_from & p_from) {
		const t_size size = array_size_t(p_from);
		if (p_to.has_owned_items(p_from)) {//avoid landmines with actual array data overlapping, or p_from being same as p_to
			array_staticsize_t<typename t_to::t_item> temp;
			temp.set_size_discard(size);
			pfc::copy_array_loop_t(temp,p_from,size);
			p_to.set_size(size);
			pfc::copy_array_loop_t(p_to,temp,size);
		} else {
			p_to.set_size(size);
			pfc::copy_array_loop_t(p_to,p_from,size);
		}
	}

	template<typename t_array,typename t_value>
	inline void fill_array_t(t_array & p_array,const t_value & p_value) {
		const t_size size = array_size_t(p_array);
		for(t_size n=0;n<size;n++) p_array[n] = p_value;
	}

	template<typename _t_item, template<typename> class t_alloc> class array_t {
	public:	typedef _t_item t_item;
	private: typedef array_t<t_item,t_alloc> t_self;
	public:
		array_t() {}
		array_t(const t_self & p_source) {copy_array_t(*this,p_source);}
		template<typename t_source> array_t(const t_source & p_source) {copy_array_t(*this,p_source);}
		const t_self & operator=(const t_self & p_source) {copy_array_t(*this,p_source); return *this;}
		template<typename t_source> const t_self & operator=(const t_source & p_source) {copy_array_t(*this,p_source); return *this;}
		
		void set_size(t_size p_size) {m_alloc.set_size(p_size);}
		void set_count(t_size p_count) {m_alloc.set_size(p_count);}
		t_size get_size() const {return m_alloc.get_size();}
		t_size get_count() const {return m_alloc.get_size();}
		void force_reset() {m_alloc.force_reset();}
		
		const t_item & operator[](t_size p_index) const {PFC_ASSERT(p_index < get_size());return m_alloc[p_index];}
		t_item & operator[](t_size p_index) {PFC_ASSERT(p_index < get_size());return m_alloc[p_index];}

		//! Warning: buffer pointer must not point to buffer allocated by this array (fixme).
		template<typename t_source>
		void set_data_fromptr(const t_source * p_buffer,t_size p_count) {
			
			set_size(p_count);
			pfc::copy_array_loop_t(*this,p_buffer,p_count);
		}

		template<typename t_array>
		void append(const t_array & p_source) {
			if (has_owned_items(p_source)) append(array_t<t_item>(p_source));
			else {
				const t_size source_size = array_size_t(p_source);
				const t_size base = get_size();
				increase_size(source_size);
				for(t_size n=0;n<source_size;n++) m_alloc[base+n] = p_source[n];
			}
		}

		template<typename t_insert>
		void insert_multi(const t_insert & value, t_size base, t_size count) {
			const t_size oldSize = get_size();
			if (base > oldSize) base = oldSize;
			increase_size(count);
			pfc::memmove_t(get_ptr() + base + count, get_ptr() + base, oldSize - base);
			pfc::fill_ptr_t(get_ptr() + base, count, value);
		}
		template<typename t_append> void append_multi(const t_append & value, t_size count) {insert_multi(value,~0,count);}

		//! Warning: buffer pointer must not point to buffer allocated by this array (fixme).
		template<typename t_append>
		void append_fromptr(const t_append * p_buffer,t_size p_count) {
			PFC_ASSERT( !is_owned(&p_buffer[0]) );
			t_size base = get_size();
			increase_size(p_count);
			for(t_size n=0;n<p_count;n++) m_alloc[base+n] = p_buffer[n];
		}

		void increase_size(t_size p_delta) {
			t_size new_size = get_size() + p_delta;
			if (new_size < p_delta) throw std::bad_alloc();
			set_size(new_size);
		}

		template<typename t_append>
		void append_single(const t_append & p_item) {
			if (is_owned(p_item)) append_single(t_append(p_item));
			else {
				const t_size base = get_size();
				increase_size(1);
				m_alloc[base] = p_item;
			}
		}

		template<typename t_filler>
		void fill(const t_filler & p_filler) {
			const t_size max = get_size();
			for(t_size n=0;n<max;n++) m_alloc[n] = p_filler;
		}

		void fill_null() {
			const t_size max = get_size();
			for(t_size n=0;n<max;n++) m_alloc[n] = 0;
}

		void grow_size(t_size p_size) {
			if (p_size > get_size()) set_size(p_size);
		}

		//not supported by some allocs
		const t_item * get_ptr() const {return m_alloc.get_ptr();}
		t_item * get_ptr() {return m_alloc.get_ptr();}

		void prealloc(t_size p_size) {m_alloc.prealloc(p_size);}

		template<typename t_array>
		bool has_owned_items(const t_array & p_source) {
			if (array_size_t(p_source) == 0) return false;

			//how the hell would we properly check if any of source items is owned by us, in case source array implements some weird mixing of references of items from different sources?
			//the most obvious way means evil bottleneck here (whether it matters or not from caller's point of view which does something O(n) already is another question)
			//at least this will work fine with all standard classes which don't crossreference anyhow and always use own storage
			//perhaps we'll traitify this someday later
			return is_owned(p_source[0]);
		}

		template<typename t_source>
		bool is_owned(const t_source & p_item) {
			return m_alloc.is_ptr_owned(&p_item);
		}

		template<typename t_item>
		void set_single(const t_item & p_item) {
			set_size(1);
			(*this)[0] = p_item;
		}

		template<typename t_callback>
		void enumerate(t_callback & p_callback) const {
			for(t_size n = 0; n < get_size(); n++ ) {
				p_callback((*this)[n]);
			}
		}

	private:
		t_alloc<t_item> m_alloc;
	};

	template<typename t_item,t_size p_width,template<typename> class t_alloc = alloc_standard >
	class array_hybrid_t : public array_t<t_item, pfc::alloc_hybrid<p_width,t_alloc>::template alloc >
	{};


	template<typename t_item> class traits_t<array_staticsize_t<t_item> > : public traits_default_movable {};
	template<typename t_item,template<typename> class t_alloc> class traits_t<array_t<t_item,t_alloc> > : public pfc::traits_t<t_alloc<t_item> > {};


	template<typename t_comparator = comparator_default>
	class comparator_array {
	public:
		template<typename t_array1, typename t_array2>
		static int compare(const t_array1 & p_array1, const t_array2 & p_array2) {
			t_size walk = 0;
			for(;;) {
				if (walk >= p_array1.get_size() && walk >= p_array2.get_size()) return 0;
				else if (walk >= p_array1.get_size()) return -1;
				else if (walk >= p_array2.get_size()) return 1;
				else {
					int state = t_comparator::compare(p_array1[walk],p_array2[walk]);
					if (state != 0) return state;
				}
				++walk;
			}
		}
	};
}


#endif //_PFC_ARRAY_H_
