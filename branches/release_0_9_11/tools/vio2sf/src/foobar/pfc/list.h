#ifndef _PFC_LIST_H_
#define _PFC_LIST_H_

namespace pfc {

template<typename T>
class NOVTABLE list_base_const_t {
private: typedef list_base_const_t<T> t_self;
public:
	typedef T t_item;
	virtual t_size get_count() const = 0;
	virtual void get_item_ex(T& p_out, t_size n) const = 0;

	inline t_size get_size() const {return get_count();}

	inline T get_item(t_size n) const {T temp; get_item_ex(temp,n); return temp;}
	inline T operator[](t_size n) const {T temp; get_item_ex(temp,n); return temp;}

	template<typename t_compare>
	t_size find_duplicates_sorted_t(t_compare p_compare,bit_array_var & p_out) const
	{
		return pfc::find_duplicates_sorted_t<list_base_const_t<T> const &,t_compare>(*this,get_count(),p_compare,p_out);
	}

	template<typename t_compare,typename t_permutation>
	t_size find_duplicates_sorted_permutation_t(t_compare p_compare,t_permutation const & p_permutation,bit_array_var & p_out)
	{
		return pfc::find_duplicates_sorted_permutation_t<list_base_const_t<T> const &,t_compare,t_permutation>(*this,get_count(),p_compare,p_permutation,p_out);
	}

	template<typename t_search>
	t_size find_item(const t_search & p_item) const//returns index of first occurance, infinite if not found
	{
		t_size n,max = get_count();
		for(n=0;n<max;n++)
			if (get_item(n)==p_item) return n;
		return ~0;
	}

	template<typename t_search>
	inline bool have_item(const t_search & p_item) const {return find_item<t_search>(p_item)!=~0;}


	template<typename t_compare, typename t_param>
	bool bsearch_t(t_compare p_compare,t_param const & p_param,t_size &p_index) const {
		return pfc::bsearch_t(get_count(),*this,p_compare,p_param,p_index);
	}

	template<typename t_compare,typename t_param,typename t_permutation>
	bool bsearch_permutation_t(t_compare p_compare,t_param const & p_param,const t_permutation & p_permutation,t_size & p_index) const {
		return pfc::bsearch_permutation_t(get_count(),*this,p_compare,p_param,p_permutation,p_index);
	}

	template<typename t_compare,typename t_permutation>
	void sort_get_permutation_t(t_compare p_compare,t_permutation const & p_permutation) const {
		pfc::sort_get_permutation_t<list_base_const_t<T>,t_compare,t_permutation>(*this,p_compare,get_count(),p_permutation);
	}

	template<typename t_compare,typename t_permutation>
	void sort_stable_get_permutation_t(t_compare p_compare,t_permutation const & p_permutation) const {
		pfc::sort_stable_get_permutation_t<list_base_const_t<T>,t_compare,t_permutation>(*this,p_compare,get_count(),p_permutation);
	}

	template<typename t_callback>
	void enumerate(t_callback & p_callback) const {
		for(t_size n = 0, m = get_count(); n < m; ++n ) {
			p_callback( (*this)[n] );
		}
	}

	static bool g_equals(const t_self & item1, const t_self & item2) {
		const t_size count = item1.get_count();
		if (count != item2.get_count()) return false;
		for(t_size walk = 0; walk < count; ++walk) if (item1[walk] != item2[walk]) return false;
		return true;
	}
	bool operator==(const t_self & item2) const {return g_equals(*this,item2);}
	bool operator!=(const t_self & item2) const {return !g_equals(*this,item2);}
	
protected:
	list_base_const_t() {}
	~list_base_const_t() {}
private:
	const t_self & operator=(const t_self &) {throw pfc::exception_not_implemented();}
};


template<typename T>
class list_single_ref_t : public list_base_const_t<T>
{
public:
	list_single_ref_t(const T & p_item,t_size p_count = 1) : m_item(p_item), m_count(p_count) {}
	t_size get_count() const {return m_count;}
	void get_item_ex(T& p_out,t_size n) const {PFC_ASSERT(n<m_count); p_out = m_item;}
private:
	const T & m_item;
	t_size m_count;
};

template<typename T>
class list_partial_ref_t : public list_base_const_t<T>
{
public:
	list_partial_ref_t(const list_base_const_t<T> & p_list,t_size p_base,t_size p_count)
		: m_list(p_list), m_base(p_base), m_count(p_count)
	{
		PFC_ASSERT(m_base + m_count <= m_list.get_count());
	}

private:
	const list_base_const_t<T> & m_list;
	t_size m_base,m_count;

	t_size get_count() const {return m_count;}
	void get_item_ex(T & p_out,t_size n) const {m_list.get_item_ex(p_out,n+m_base);}
};

template<typename T,typename A>
class list_const_array_t : public list_base_const_t<T>
{
public:
	inline list_const_array_t(A p_data,t_size p_count) : m_data(p_data), m_count(p_count) {}
	t_size get_count() const {return m_count;}
	void get_item_ex(T & p_out,t_size n) const {p_out = m_data[n];}
private:
	A m_data;
	t_size m_count;
};

template<typename to,typename from>
class list_const_cast_t : public list_base_const_t<to>
{
public:
	list_const_cast_t(const list_base_const_t<from> & p_from) : m_from(p_from) {}
	t_size get_count() const {return m_from.get_count();}
	void get_item_ex(to & p_out,t_size n) const
	{
		from temp;
		m_from.get_item_ex(temp,n);
		p_out = temp;
	}
private:
	const list_base_const_t<from> & m_from;
};

template<typename T,typename A>
class ptr_list_const_array_t : public list_base_const_t<T*>
{
public:
	inline ptr_list_const_array_t(A p_data,t_size p_count) : m_data(p_data), m_count(p_count) {}
	t_size get_count() const {return m_count;}
	void get_item_ex(T* & p_out,t_size n) const {p_out = &m_data[n];}
private:
	A m_data;
	t_size m_count;
};
template<typename T>
class list_const_ptr_t : public list_base_const_t<T>
{
public:
	inline list_const_ptr_t(const T * p_data,t_size p_count) : m_data(p_data), m_count(p_count) {}
	t_size get_count() const {return m_count;}
	void get_item_ex(T & p_out,t_size n) const {p_out = m_data[n];}
private:
	const T * m_data;
	t_size m_count;
};

template<typename T>
class NOVTABLE list_base_t : public list_base_const_t<T> {
private:
	typedef list_base_t<T> t_self;
	typedef const list_base_const_t<T> t_self_const;
public:
	class NOVTABLE sort_callback
	{
	public:
		virtual int compare(const T& p_item1,const T& p_item2) = 0;
	};

	virtual void filter_mask(const bit_array & mask) = 0;
	virtual t_size insert_items(const list_base_const_t<T> & items,t_size base) = 0;
	virtual void reorder_partial(t_size p_base,const t_size * p_data,t_size p_count) = 0;
	virtual void sort(sort_callback & p_callback) = 0;
	virtual void sort_stable(sort_callback & p_callback) = 0;
	virtual void replace_item(t_size p_index,const T& p_item) = 0;
	virtual void swap_item_with(t_size p_index,T & p_item) = 0;
	virtual void swap_items(t_size p_index1,t_size p_index2) = 0;

	inline void reorder(const t_size * p_data) {reorder_partial(0,p_data,this->get_count());}

	inline t_size insert_item(const T & item,t_size base) {return insert_items(list_single_ref_t<T>(item),base);}
	t_size insert_items_repeat(const T & item,t_size num,t_size base) {return insert_items(list_single_ref_t<T>(item,num),base);}
	inline t_size add_items_repeat(T item,t_size num) {return insert_items_repeat(item,num,this->get_count());}
	t_size insert_items_fromptr(const T* source,t_size num,t_size base) {return insert_items(list_const_ptr_t<T>(source,num),base);}
	inline t_size add_items_fromptr(const T* source,t_size num) {return insert_items_fromptr(source,num,this->get_count());}

	inline t_size add_items(const list_base_const_t<T> & items) {return insert_items(items,this->get_count());}
	inline t_size add_item(const T& item) {return insert_item(item,this->get_count());}

	inline void remove_mask(const bit_array & mask) {filter_mask(bit_array_not(mask));}
	inline void remove_all() {filter_mask(bit_array_false());}
	inline void truncate(t_size val) {if (val < this->get_count()) remove_mask(bit_array_range(val,this->get_count()-val,true));}
	
	inline T replace_item_ex(t_size p_index,const T & p_item) {T ret = p_item;swap_item_with(p_index,ret);return ret;}

	inline T operator[](t_size n) const {return this->get_item(n);}

	template<typename t_compare>
	class sort_callback_impl_t : public sort_callback
	{
	public:
		sort_callback_impl_t(t_compare p_compare) : m_compare(p_compare) {}
		int compare(const T& p_item1,const T& p_item2) {return m_compare(p_item1,p_item2);}
	private:
		t_compare m_compare;
	};

	class sort_callback_auto : public sort_callback
	{
	public:
		int compare(const T& p_item1,const T& p_item2) {return pfc::compare_t(p_item1,p_item2);}
	};

	void sort() {sort(sort_callback_auto());}
	template<typename t_compare> void sort_t(t_compare p_compare) {sort(sort_callback_impl_t<t_compare>(p_compare));}
	template<typename t_compare> void sort_stable_t(t_compare p_compare) {sort_stable(sort_callback_impl_t<t_compare>(p_compare));}

	template<typename t_compare> void sort_remove_duplicates_t(t_compare p_compare)
	{
		sort_t<t_compare>(p_compare);
		bit_array_bittable array(this->get_count());		
		if (this->template find_duplicates_sorted_t<t_compare>(p_compare,array) > 0)
			remove_mask(array);
	}

	template<typename t_compare> void sort_stable_remove_duplicates_t(t_compare p_compare)
	{
		sort_stable_t<t_compare>(p_compare);
		bit_array_bittable array(this->get_count());		
		if (this->template find_duplicates_sorted_t<t_compare>(p_compare,array) > 0)
			remove_mask(array);
	}


	template<typename t_compare> void remove_duplicates_t(t_compare p_compare)
	{
		order_helper order(this->get_count());
		sort_get_permutation_t<t_compare,order_helper&>(p_compare,order);
		bit_array_bittable array(this->get_count());
		if (this->template find_duplicates_sorted_permutation_t<t_compare,order_helper const&>(p_compare,order,array) > 0)
			remove_mask(array);
	}

	template<typename t_func>
	void for_each(t_func p_func) {
		t_size n,max=this->get_count();
		for(n=0;n<max;n++) p_func(this->get_item(n));
	}

	template<typename t_func>
	void for_each(t_func p_func,const bit_array & p_mask) {
		t_size n,max=this->get_count();
		for(n=p_mask.find(true,0,max);n<max;n=p_mask.find(true,n+1,max-n-1)) {
			p_func(this->get_item(n));
		}
	}

	template<typename t_releasefunc>
	void remove_mask_ex(const bit_array & p_mask,t_releasefunc p_func) {
		this->template for_each<t_releasefunc>(p_func,p_mask);
		remove_mask(p_mask);
	}

	template<typename t_releasefunc>
	void remove_all_ex(t_releasefunc p_func) {
		this->template for_each<t_releasefunc>(p_func);
		remove_all();
	}

	const t_self & operator=(const t_self & p_source) {remove_all(); add_items(p_source);return *this;}
	const t_self & operator=(t_self_const & source) {remove_all(); add_items(source); return *this;}
	const t_self & operator+=(t_self_const & p_source) {add_items(p_source); return *this;}
	
protected:
	list_base_t() {}
	~list_base_t() {}
};


template<typename T,typename t_storage>
class list_impl_t : public list_base_t<T>
{
public:
	list_impl_t() {}
	list_impl_t(const list_impl_t<T,t_storage> & p_source) { *this = p_source; }

	void prealloc(t_size count) {m_buffer.prealloc(count);}

	void set_count(t_size p_count) {m_buffer.set_size(p_count);}
	void set_size(t_size p_count) {m_buffer.set_size(p_count);}

	t_size insert_item(const T& item,t_size idx)
	{
		t_size max = m_buffer.get_size();
		if (idx > max) idx = max;
		max++;
		m_buffer.set_size(max);
		t_size n;
		for(n=max-1;n>idx;n--)
			m_buffer[n]=m_buffer[n-1];
		m_buffer[idx]=item;
		return idx;
	}

	T remove_by_idx(t_size idx)
	{
		T ret = m_buffer[idx];
		t_size n;
		t_size max = m_buffer.get_size();
		for(n=idx+1;n<max;n++)
		{
			pfc::swap_t(m_buffer[n-1],m_buffer[n]);
		}
		m_buffer.set_size(max-1);
		return ret;
	}


	inline void get_item_ex(T& p_out,t_size n) const
	{
		PFC_ASSERT(n>=0);
		PFC_ASSERT(n<get_count());
		p_out = m_buffer[n];
	}

	inline const T& get_item_ref(t_size n) const
	{
		PFC_ASSERT(n>=0);
		PFC_ASSERT(n<get_count());
		return m_buffer[n];
	}

	inline T get_item(t_size n) const
	{
		PFC_ASSERT(n >= 0);
		PFC_ASSERT(n < get_count() );
		return m_buffer[n];
	};

	inline t_size get_count() const {return m_buffer.get_size();}
	inline t_size get_size() const {return get_count();}

	inline const T & operator[](t_size n) const
	{
		PFC_ASSERT(n>=0);
		PFC_ASSERT(n<get_count());
		return m_buffer[n];
	}

	inline const T* get_ptr() const {return m_buffer.get_ptr();}
	inline T* get_ptr() {return m_buffer.get_ptr();}

	inline T& operator[](t_size n) {return m_buffer[n];}

	inline void remove_from_idx(t_size idx,t_size num)
	{
		remove_mask(bit_array_range(idx,num));
	}

	t_size insert_items(const list_base_const_t<T> & source,t_size base)
	{
		t_size count = get_count();
		if (base>count) base = count;
		t_size num = source.get_count();
		m_buffer.set_size(count+num);
		if (count > base)
		{
			t_size n;
			for(n=count-1;(int)n>=(int)base;n--)
			{
				pfc::swap_t(m_buffer[n+num],m_buffer[n]);
			}
		}

		{
			t_size n;
			for(n=0;n<num;n++)
			{
				source.get_item_ex(m_buffer[n+base],n);
			}
		}
		return base;

	}

	void get_items_mask(list_impl_t<T,t_storage> & out,const bit_array & mask)
	{
		t_size n,count = get_count();
		for_each_bit_array(n,mask,true,0,count)
			out.add_item(m_buffer[n]);
	}

	void filter_mask(const bit_array & mask)
	{
		t_size n,count = get_count(), total = 0;

		n = total = mask.find(false,0,count);

		if (n<count)
		{
			for(n=mask.find(true,n+1,count-n-1);n<count;n=mask.find(true,n+1,count-n-1))
				pfc::swap_t(m_buffer[total++],m_buffer[n]);

			m_buffer.set_size(total);
		}
	}

	void replace_item(t_size idx,const T& item)
	{
		PFC_ASSERT(idx>=0);
		PFC_ASSERT(idx<get_count());
		m_buffer[idx] = item;
	}

	void sort()
	{
		pfc::sort_callback_impl_auto_wrap_t<t_storage> wrapper(m_buffer);
		pfc::sort(wrapper,get_count());
	}

	template<typename t_compare>
	void sort_t(t_compare p_compare)
	{
		pfc::sort_callback_impl_simple_wrap_t<t_storage,t_compare> wrapper(m_buffer,p_compare);
		pfc::sort(wrapper,get_count());
	}

	template<typename t_compare>
	void sort_stable_t(t_compare p_compare)
	{
		pfc::sort_callback_impl_simple_wrap_t<t_storage,t_compare> wrapper(m_buffer,p_compare);
		pfc::sort_stable(wrapper,get_count());
	}
	inline void reorder_partial(t_size p_base,const t_size * p_order,t_size p_count)
	{
		PFC_ASSERT(p_base+p_count<=get_count());
		pfc::reorder_partial_t(m_buffer,p_base,p_order,p_count);
	}

	template<typename t_compare>
	t_size find_duplicates_sorted_t(t_compare p_compare,bit_array_var & p_out) const
	{
		return pfc::find_duplicates_sorted_t<list_impl_t<T,t_storage> const &,t_compare>(*this,get_count(),p_compare,p_out);
	}

	template<typename t_compare,typename t_permutation>
	t_size find_duplicates_sorted_permutation_t(t_compare p_compare,t_permutation p_permutation,bit_array_var & p_out)
	{
		return pfc::find_duplicates_sorted_permutation_t<list_impl_t<T,t_storage> const &,t_compare,t_permutation>(*this,get_count(),p_compare,p_permutation,p_out);
	}


private:
	class sort_callback_wrapper
	{
	public:
		explicit inline sort_callback_wrapper(sort_callback & p_callback) : m_callback(p_callback) {}
		inline int operator()(const T& item1,const T& item2) const {return m_callback.compare(item1,item2);}
	private:
		sort_callback & m_callback;
	};
public:
	void sort(sort_callback & p_callback)
	{
		sort_t(sort_callback_wrapper(p_callback));
	}
	
	void sort_stable(sort_callback & p_callback)
	{
		sort_stable_t(sort_callback_wrapper(p_callback));
	}

	void remove_mask(const bit_array & mask) {filter_mask(bit_array_not(mask));}

	void remove_mask(const bool * mask) {remove_mask(bit_array_table(mask,get_count()));}
	void filter_mask(const bool * mask) {filter_mask(bit_array_table(mask,get_count()));}

	t_size add_item(const T& item)
	{
		t_size idx = get_count();
		insert_item(item,idx);
		return idx;
	}

	void remove_all() {remove_mask(bit_array_true());}

	void remove_item(const T& item)
	{
		t_size n,max = get_count();
		bit_array_bittable mask(max);
		for(n=0;n<max;n++)
			mask.set(n,get_item(n)==item);
		remove_mask(mask);		
	}

	void swap_item_with(t_size p_index,T & p_item)
	{
		PFC_ASSERT(p_index < get_count());
		pfc::swap_t(m_buffer[p_index],p_item);
	}

	void swap_items(t_size p_index1,t_size p_index2) 
	{
		PFC_ASSERT(p_index1 < get_count());
		PFC_ASSERT(p_index2 < get_count());
		pfc::swap_t(m_buffer[p_index1],m_buffer[p_index2]);
	}

	inline static void g_swap(list_impl_t<T,t_storage> & p_item1,list_impl_t<T,t_storage> & p_item2)
	{
		pfc::swap_t(p_item1.m_buffer,p_item2.m_buffer);
	}

	template<typename t_search>
	t_size find_item(const t_search & p_item) const//returns index of first occurance, infinite if not found
	{
		t_size n,max = get_count();
		for(n=0;n<max;n++)
			if (m_buffer[n]==p_item) return n;
		return ~0;
	}

	template<typename t_search>
	inline bool have_item(const t_search & p_item) const {return this->template find_item<t_search>(p_item)!=~0;}

protected:
	t_storage m_buffer;
};

template<typename t_item, template<typename> class t_alloc = pfc::alloc_fast >
class list_t : public list_impl_t<t_item,pfc::array_t<t_item,t_alloc> > { };

template<typename t_item, t_size p_fixed_count, template<typename> class t_alloc = pfc::alloc_fast >
class list_hybrid_t : public list_impl_t<t_item,pfc::array_hybrid_t<t_item,p_fixed_count,t_alloc> > {};

template<typename T>
class ptr_list_const_cast_t : public list_base_const_t<const T *>
{
public:
	inline ptr_list_const_cast_t(const list_base_const_t<T*> & p_param) : m_param(p_param) {}
	t_size get_count() const {return m_param.get_count();}
	void get_item_ex(const T * & p_out,t_size n) const {T* temp; m_param.get_item_ex(temp,n); p_out = temp;}
private:
	const list_base_const_t<T*> & m_param;

};


template<typename T,typename P>
class list_const_permutation_t : public list_base_const_t<T>
{
public:
	inline list_const_permutation_t(const list_base_const_t<T> & p_list,P p_permutation) : m_list(p_list), m_permutation(p_permutation) {}
	t_size get_count() const {return m_list.get_count();}
	void get_item_ex(T & p_out,t_size n) const {m_list.get_item_ex(p_out,m_permutation[n]);}
private:
	P m_permutation;
	const list_base_const_t<T> & m_list;
};


template<class T>
class list_permutation_t : public list_base_const_t<T>
{
public:
	t_size get_count() const {return m_count;}
	void get_item_ex(T & p_out,t_size n) const {m_base.get_item_ex(p_out,m_order[n]);}
	list_permutation_t(const list_base_const_t<T> & p_base,const t_size * p_order,t_size p_count)
		: m_base(p_base), m_order(p_order), m_count(p_count)
	{
		PFC_ASSERT(m_base.get_count() >= m_count);
	}
private:
	const list_base_const_t<T> & m_base;
	const t_size * m_order;
	t_size m_count;
};

}
#endif //_PFC_LIST_H_
