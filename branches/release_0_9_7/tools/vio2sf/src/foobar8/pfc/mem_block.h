#ifndef _PFC_MEM_BLOCK_H_
#define _PFC_MEM_BLOCK_H_

template<class T>
class mem_ops
{
public:
	static void copy(T* dst,const T* src,unsigned count) {memcpy(dst,src,count*sizeof(T));}
	static void move(T* dst,const T* src,unsigned count) {memmove(dst,src,count*sizeof(T));}
	static void set(T* dst,int val,unsigned count) {memset(dst,val,count*sizeof(T));}
	static void setval(T* dst,T val,unsigned count) {for(;count;count--) *(dst++)=val;}
	static T* alloc(unsigned count) {return reinterpret_cast<T*>(malloc(count * sizeof(T)));}
	static T* alloc_zeromem(unsigned count)
	{
		T* ptr = alloc(count);
		if (ptr) set(ptr,0,count);
		return ptr;
	}
	static T* realloc(T* ptr,unsigned count)
	{return ptr ? reinterpret_cast<T*>(::realloc(reinterpret_cast<void*>(ptr),count * sizeof(T))) : alloc(count);}

	static void free(T* ptr) {::free(reinterpret_cast<void*>(ptr)); }
	inline static T make_null_item()
	{
		char item[sizeof(T)];
		memset(&item,0,sizeof(T));
		return * reinterpret_cast<T*>(&item);
	}
	inline static void swap(T& item1,T& item2) {T temp; temp=item1; item1=item2; item2=temp;}
};

class mem_block
{
public:
	enum mem_logic_t {ALLOC_DEFAULT,ALLOC_FAST,ALLOC_FAST_DONTGODOWN};
private:
	void * data;
	unsigned size,used;
	mem_logic_t mem_logic;
public:
	inline void set_mem_logic(mem_logic_t v) {mem_logic=v;}
	inline mem_logic_t get_mem_logic() const {return mem_logic;}

	void prealloc(unsigned size);

	inline mem_block() {data=0;size=0;used=0;mem_logic=ALLOC_DEFAULT;}
	inline ~mem_block() {if (data) free(data);}

	inline unsigned get_size() const {return used;}

	inline const void * get_ptr() const {return data;}
	inline void * get_ptr() {return data;}

	void * set_size(unsigned new_used);

	inline void * check_size(unsigned new_size)
	{
		if (used<new_size) return set_size(new_size);
		else return get_ptr();
	}

	inline operator const void * () const {return get_ptr();}
	inline operator void * () {return get_ptr();}

	void* copy(const void *ptr, unsigned bytes,unsigned start=0);

	inline void* append(const void *ptr, unsigned bytes) {return copy(ptr,bytes,used);}

	inline void zeromemory() {memset(get_ptr(),0,used);}

	inline void force_reset() {if (data) free(data);data=0;size=0;used=0;}

	void * set_data(const void * src,unsigned size)
	{
		void * out = set_size(size);
		if (out) memcpy(out,src,size);
		return out;
	}

	void operator=(const mem_block & src) {set_data(src.get_ptr(),src.get_size());}

	static void g_apply_order(void * data,unsigned size,const int * order,unsigned num);

};

template<class T>
class mem_block_t //: public mem_block	
{
	mem_block theBlock;//msvc7 sucks
public:
	mem_block_t() {}
	mem_block_t(unsigned s) {theBlock.set_size(s*sizeof(T));}

	inline void set_mem_logic(mem_block::mem_logic_t v) {theBlock.set_mem_logic(v);}
	inline mem_block::mem_logic_t get_mem_logic() const {return theBlock.get_mem_logic();}

	inline unsigned get_size() const {return theBlock.get_size()/sizeof(T);}

	inline const T* get_ptr() const {return reinterpret_cast<const T*>(theBlock.get_ptr());}
	inline T* get_ptr() {return reinterpret_cast<T*>(theBlock.get_ptr());}

	inline T* set_size(unsigned new_size) {return reinterpret_cast<T*>(theBlock.set_size(new_size*sizeof(T)));}

	inline T* check_size(unsigned new_size) {return reinterpret_cast<T*>(theBlock.check_size(new_size*sizeof(T)));}

	inline operator const T * () const {return get_ptr();}
	inline operator T * () {return get_ptr();}

	inline T* copy(const T* ptr,unsigned size,unsigned start=0) {return reinterpret_cast<T*>(theBlock.copy(reinterpret_cast<const void*>(ptr),size*sizeof(T),start*sizeof(T)));}
	inline T* append(const T* ptr,unsigned size) {return reinterpret_cast<T*>(theBlock.append(reinterpret_cast<const void*>(ptr),size*sizeof(T)));}
	inline void append(T item) {theBlock.append(reinterpret_cast<const void*>(&item),sizeof(item));}

	inline void swap(unsigned idx1,unsigned idx2)
	{
		T * ptr = get_ptr();
		mem_ops<T>::swap(ptr[idx1],ptr[idx2]);
	}

	T* set_data(const T* src,unsigned count)
	{
		return reinterpret_cast<T*>(theBlock.set_data(reinterpret_cast<const void*>(src),count*sizeof(T)));
	}

	void operator=(const mem_block_t<T> & src) {set_data(src.get_ptr(),src.get_size());}

	unsigned write_circular(unsigned offset,const T* src,unsigned count)
	{//returns new offset
		unsigned total = get_size();
		if ((int)offset<0)
		{
			offset = total - ( (-(int)offset)%total );
		}
		else offset%=total;

		if (count>total)
		{
			src += count - total;
			count = total;
		}

		while(count>0)
		{
			unsigned delta = count;
			if (delta > total - offset) delta = total - offset;
			unsigned n;
			for(n=0;n<delta;n++)
				get_ptr()[n+offset] = *(src++);
			count -= delta;
			offset = (offset + delta) % total;
		}
		return offset;
	}

	unsigned read_circular(unsigned offset,T* dst,unsigned count)
	{
		unsigned total = get_size();
		if ((int)offset<0)
		{
			offset = total - ( (-(int)offset)%total );
		}
		else offset%=total;

		while(count>0)
		{
			unsigned delta = count;
			if (delta > total - offset) delta = total - offset;
			unsigned n;
			for(n=0;n<delta;n++)
				*(dst++) = get_ptr()[n+offset];
			count -= delta;
			offset = (offset + delta) % total;
		}
		return offset;
	}

	inline void zeromemory() {theBlock.zeromemory();}

	void fill(T val)
	{
		unsigned n = get_size();
		T * dst = get_ptr();
		for(;n;n--) *(dst++) = val;
	}

	inline void force_reset() {theBlock.force_reset();}

	void apply_order(const int * order,unsigned num)
	{
		assert(num<=get_size());
		mem_block::g_apply_order(get_ptr(),sizeof(T),order,num);
	}

	inline void prealloc(unsigned size) {theBlock.prealloc(size*sizeof(T));}

	void sort(int (__cdecl *compare )(const T * elem1, const T* elem2 ) )
	{
		qsort((void*)get_ptr(),get_size(),sizeof(T),(int (__cdecl *)(const void *, const void *) )compare);
	}
};

template<class T>
class mem_block_fastalloc : public mem_block_t<T>
{
public:
	mem_block_fastalloc(unsigned initsize=0) {set_mem_logic(mem_block::ALLOC_FAST_DONTGODOWN);if (initsize) prealloc(initsize);}
};

#if 0

#define mem_block_aligned_t mem_block_t

#else

template<class T>
class mem_block_aligned_t
{
	mem_block data;
	T * ptr_aligned;
public:
	mem_block_aligned_t() {ptr_aligned = 0;}
	mem_block_aligned_t(unsigned size) {ptr_aligned = 0;set_size(size);}

	unsigned get_size() const {return data.get_size()/sizeof(T);}

	inline void set_mem_logic(mem_block::mem_logic_t v) {data.set_mem_logic(v);}
	inline mem_block::mem_logic_t get_mem_logic() const {return data.get_mem_logic();}

	T * set_size(unsigned size)
	{
		unsigned size_old = get_size();
		int delta_old = (unsigned)ptr_aligned - (unsigned)data.get_ptr();
		unsigned ptr = (unsigned)data.set_size( (size+1) * sizeof(T) - 1), old_ptr = ptr;
		ptr += sizeof(T) - 1;
		ptr -= ptr % sizeof(T);
		int delta_new = ptr - old_ptr;
		if (delta_new != delta_old)
		{
			unsigned to_move = size_old > size ? size : size_old;
			memmove((char*)ptr,(char*)ptr - (delta_new-delta_old),to_move * sizeof(T));
		}
		ptr_aligned = (T*)ptr;
		return ptr_aligned;
	}

	T* check_size(unsigned size)
	{
		return size > get_size() ? set_size(size) : get_ptr();
	}

	void fill(T val)
	{
		unsigned n = get_size();
		T * dst = get_ptr();
		for(;n;n--) *(dst++) = val;
	}

	inline void zeromemory() {data.zeromemory();}	
	inline const T * get_ptr() const {return ptr_aligned;}
	inline T * get_ptr() {return ptr_aligned;}
	inline operator const T * () const {return get_ptr();}
	inline operator T * () {return get_ptr();}
};
#endif

#endif