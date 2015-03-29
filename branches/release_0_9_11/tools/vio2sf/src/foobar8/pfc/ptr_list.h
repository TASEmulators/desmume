#ifndef __PFC_PTR_LIST_H_
#define __PFC_PTR_LIST_H_

class NOVTABLE ptr_list_root//base interface (avoids bloating code with templated vtables)
{
protected:
	virtual unsigned _ptr_list_get_count() const = 0;
	virtual void* _ptr_list_get_item(unsigned n) const = 0;
	virtual void* _ptr_list_remove_by_idx(unsigned idx) = 0;
	virtual void _ptr_list_filter_mask(const bit_array & mask) = 0;
	virtual void _ptr_list_insert_item(void* item,unsigned idx) = 0;
};

template<class T>
class NOVTABLE ptr_list_interface : public ptr_list_root//cross-dll safe interface, use this for passing data around between components; you can typecast ptr_list_t to it
{
public:
	inline unsigned get_count() const {return _ptr_list_get_count();}
	inline T* get_item(unsigned idx) const {return reinterpret_cast<T*>(_ptr_list_get_item(idx));}
	inline T* remove_by_idx(unsigned idx) {return reinterpret_cast<T*>(_ptr_list_remove_by_idx(idx));}
	inline void filter_mask(const bit_array & mask) {_ptr_list_filter_mask(mask);}
	inline void insert_item(T* item,unsigned idx) {_ptr_list_insert_item(const_cast<void*>(reinterpret_cast<const void*>(item)),idx);}
	
	inline unsigned add_item(T* item) {unsigned idx = get_count();insert_item(item,idx);return idx;}
	inline void remove_mask(const bit_array & mask) {filter_mask(bit_array_not(mask));}
	inline void remove_all() {filter_mask(bit_array_false());}

	inline T* operator[](unsigned n) const {return get_item(n);}
};

#define ptr_list_base ptr_list_interface

#define void_cast(blah) const_cast<void*>(reinterpret_cast<const void*>(blah))

class ptr_list : public mem_block_list<void*>, virtual public ptr_list_root
{
private:
	virtual unsigned _ptr_list_get_count() const {return get_count();}
	virtual void* _ptr_list_get_item(unsigned idx) const {return void_cast(get_item(idx));}
	virtual void* _ptr_list_remove_by_idx(unsigned idx) {return void_cast(remove_by_idx(idx));}
	virtual void _ptr_list_filter_mask(const bit_array & mask) {filter_mask(mask);}
	virtual void _ptr_list_insert_item(void* item,unsigned idx) {insert_item(item,idx);}
public:	
	using mem_block_list<void*>::get_count;
	using mem_block_list<void*>::get_item;
	using mem_block_list<void*>::remove_by_idx;
	using mem_block_list<void*>::filter_mask;
	using mem_block_list<void*>::insert_item;
	using mem_block_list<void*>::apply_order;

	void * operator[](unsigned n) const {return get_item(n);}

	void free_by_idx(unsigned n)
	{
		void * ptr = remove_by_idx(n);
		assert(ptr);
		free(ptr);
	}
	void free_all()
	{
		unsigned n,max=get_count();
		for(n=0;n<max;n++) free(get_item(n));
		remove_all();
	}

	inline ptr_list_root & get_interface() {return *this;}
	inline const ptr_list_root & get_interface_const() const {return *this;}
};

template<class T>
class ptr_list_t : public ptr_list
{
public:
	using ptr_list::get_count;
	inline T* get_item(unsigned idx) const {return reinterpret_cast<T*>(ptr_list::get_item(idx));}
	inline T* remove_by_idx(unsigned idx) {return reinterpret_cast<T*>(ptr_list::remove_by_idx(idx));}
	using ptr_list::filter_mask;
	inline void insert_item(T* item,unsigned idx) {ptr_list::insert_item(void_cast(item),idx);}

	inline unsigned add_item(T* item) {return ptr_list::add_item(void_cast(item));}

	void sort(int (__cdecl *compare )(const T * &elem1, const T* &elem2 ) )
	{
		ptr_list::sort(reinterpret_cast< int (__cdecl *)(void* const*, void* const*) >(compare));
	}

	void add_items_copy(const ptr_list_t<T> & source)
	{
		unsigned n;
		for(n=0;n<source.get_count();n++) add_item(new T(*source.get_item(n)));
	}

	inline T* replace_item(unsigned idx,T* item) {return reinterpret_cast<T*>(ptr_list::replace_item(idx,void_cast(item)));}

	inline void delete_item(T* ptr) {remove_item(ptr);delete ptr;}

	inline void delete_by_idx(unsigned idx) 
	{
		T* ptr = remove_by_idx(idx);
		assert(ptr);
		delete ptr;
	}

	void delete_all()
	{
		unsigned n,max=get_count();
		for(n=0;n<max;n++)
		{
			T* ptr = get_item(n);
			assert(ptr);
			delete ptr;
		}
		remove_all();
	}

	void delete_mask(const bit_array & mask)
	{
		unsigned n,max=get_count();
		for(n=mask.find(true,0,max);n<max;n=mask.find(true,n+1,max-n-1))
		{
			T* ptr = get_item(n);
			assert(ptr);
			delete ptr;
		}
		remove_mask(mask);
	}

	inline void delete_mask(const bool * mask) {delete_mask(bit_array_table(mask,get_count()));}

	inline void sort(int (__cdecl *compare )(const T ** elem1, const T** elem2 ) )
	{
		ptr_list::sort((int (__cdecl *)(const void ** elem1, const void ** elem2 )) compare);
	}
	
	inline bool bsearch(int (__cdecl *compare )(T* elem1, T* elem2 ),T* item,int * index) const 
	{
		return ptr_list::bsearch((int (__cdecl *)(void* elem1, void* elem2 ))compare,void_cast(item),index);
	}

	inline bool bsearch_ref(int (__cdecl *compare )(T* &elem1, T* &elem2 ),T* &item,int * index) const 
	{
		return ptr_list::bsearch_ref((int (__cdecl *)(void* &elem1, void* &elem2 ))compare,*(const void**)&item,index);
	}

	inline bool bsearch_range(int (__cdecl *compare )(T* elem1, T* elem2 ),T* item,int * index,int * count) const 
	{
		return ptr_list::bsearch_range((int (__cdecl *)(void* elem1, void* elem2 ))compare,void_cast(item),index,count);
	}

	inline bool bsearch_param(int (__cdecl *compare )(T* elem1, const void * param ),const void * param,int * index) const 
	{
		return ptr_list::bsearch_param((int (__cdecl *)(void* elem1, const void* param))compare,param,index);
	}

	inline void insert_items_fromptr(T*const* source,int num,int idx)
	{
		ptr_list::insert_items_fromptr((void*const*)source,num,idx);
	}

	inline void add_items(const ptr_list_t<T> & source)
	{
		add_items_fromptr(source.get_ptr(),source.get_count());
	}

	inline void add_items_fromptr(T*const* source,unsigned num) {ptr_list::add_items_fromptr((void*const*)source,num);}

	inline T* const * get_ptr() const {return (T*const*)ptr_list::get_ptr();}

	inline void remove_item(T * item) {ptr_list::remove_item(void_cast(item));}
	inline int find_item(T* item) const {return ptr_list::find_item(void_cast(item));}
	inline bool have_item(T* item) const {return ptr_list::have_item(void_cast(item));}

	inline T* operator[](unsigned n) const {return get_item(n);}

	inline ptr_list_interface<T> & get_interface() {return * static_cast<ptr_list_interface<T> * > (&ptr_list::get_interface());}
	inline const ptr_list_interface<T> & get_interface_const() const {return * static_cast<const ptr_list_interface<T> * > (&ptr_list::get_interface_const());}

	inline operator ptr_list_interface<T> & () {return get_interface();}
	inline operator const ptr_list_interface<T> & () const {return get_interface_const();}

	using ptr_list::apply_order;
};

#define ptr_list_simple ptr_list_t

template<class T>
class ptr_list_autodel_t : public ptr_list_t<T>
{
public:
	~ptr_list_autodel_t() {delete_all();}
};


template<class T>
class ptr_list_autofree_t : public ptr_list_t<T>
{
public:
	~ptr_list_autofree_t() {free_all();}
};

#endif //__PFC_PTR_LIST_H_