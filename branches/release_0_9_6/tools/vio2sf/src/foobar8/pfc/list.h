#ifndef _PFC_LIST_H_
#define _PFC_LIST_H_

template<class T>
class mem_block_list_t
{
private:
	mem_block_t<T> buffer;
public:
	mem_block_list_t() {buffer.set_mem_logic(mem_block::ALLOC_FAST);}
	inline void set_mem_logic(mem_block::mem_logic_t v) {buffer.set_mem_logic(v);}

	unsigned add_items_repeat(T item,unsigned count)//returns first index
	{
		int idx = buffer.get_size();
		buffer.set_size(idx+count);
		unsigned n;
		for(n=0;n<count;n++)
			buffer[idx+n]=item;
		return idx;
	}


	void insert_item(T item,unsigned idx)
	{
		unsigned max = buffer.get_size() + 1;
		buffer.set_size(max);
		unsigned n;
		for(n=max-1;n>idx;n--)
			buffer[n]=buffer[n-1];
		buffer[idx]=item;
	}

	void insert_item_ref(const T & item,unsigned idx)
	{
		unsigned max = buffer.get_size() + 1;
		buffer.set_size(max);
		unsigned n;
		for(n=max-1;n>idx;n--)
			buffer[n]=buffer[n-1];
		buffer[idx]=item;
	}

	T remove_by_idx(unsigned idx)
	{
		T ret = buffer[idx];
		int n;
		int max = buffer.get_size();
		for(n=idx+1;n<max;n++)
		{
			buffer[n-1]=buffer[n];
		}
		buffer.set_size(max-1);
		return ret;
	}


	inline T get_item(unsigned n) const
	{
		assert(n>=0);
		assert(n<get_count());
		return buffer[n];
	}
	inline const T & get_item_ref(unsigned n) const
	{
		assert(n>=0);
		assert(n<get_count());
		return buffer[n];
	}
	inline unsigned get_count() const {return buffer.get_size();}
	inline T operator[](unsigned n) const
	{
		assert(n>=0);
		assert(n<get_count());
		return buffer[n];
	}
	inline const T* get_ptr() const {return buffer.get_ptr();}
	inline T& operator[](unsigned n) {return buffer[n];}

	inline void remove_from_idx(unsigned idx,unsigned num)
	{
		remove_mask(bit_array_range(idx,num));
	}

	void insert_items_fromptr(const T* source,int num,int idx)
	{
		int count = get_count();
		if (idx<0 || idx>=count) add_items_fromptr(source,num);
		else
		{
			buffer.set_size(count+num);
			int n;
			for(n=count-1;n>=idx;n--)
			{
				buffer[n+num]=buffer[n];
			}
			for(n=0;n<num;n++)
			{
				buffer[n+idx]=source[n];
			}
			count+=num;
		}
	}

	inline void add_items(const mem_block_list_t<T> & source)
	{
		add_items_fromptr(source.buffer,source.get_count());
	}

	void add_items_fromptr(const T* source,unsigned num)
	{
		unsigned count = get_count();
		buffer.set_size(count+num);		
		unsigned n;
		for(n=0;n<num;n++)
		{
			buffer[count++]=source[n];
		}
	}
	
	void get_items_mask(mem_block_list_t<T> & out,const bit_array & mask)
	{
		unsigned n,count = get_count();
		for_each_bit_array(n,mask,true,0,count)
			out.add_item(buffer[n]);
	}

	void filter_mask(const bit_array & mask)
	{
		unsigned n,count = get_count(), total = 0;

		n = total = mask.find(false,0,count);

		if (n<count)
		{
			for(n=mask.find(true,n+1,count-n-1);n<count;n=mask.find(true,n+1,count-n-1))
				buffer[total++] = buffer[n];

			buffer.set_size(total);
		}
	}

	inline T replace_item(unsigned idx,T item)
	{
		assert(idx>=0);
		assert(idx<get_count());
		T ret = buffer[idx];
		buffer[idx] = item;
		return ret;
	}

	void sort(int (__cdecl *compare )(const T * elem1, const T* elem2 ) )
	{
		qsort((void*)get_ptr(),get_count(),sizeof(T),(int (__cdecl *)(const void *, const void *) )compare);
	}

	bool bsearch(int (__cdecl *compare )(T elem1, T elem2 ),T item,int * index) const 
	{
		int max = get_count();
		int min = 0;
		int ptr;
		while(min<max)
		{
			ptr = min + (max-min) / 2;
			int result = compare(buffer[ptr],item);
			if (result<0) min = ptr + 1;
			else if (result>0) max = ptr;
			else 
			{
				if (index) *index = ptr;
				return 1;
			}
		}
		if (index) *index = min;
		return 0;
	}

	bool bsearch_ref(int (__cdecl *compare )(T const& elem1, T const& elem2 ),T const& item,int * index) const
	{
		int max = get_count();
		int min = 0;
		int ptr;
		while(min<max)
		{
			ptr = min + (max-min) / 2;
			int result = compare(buffer[ptr],item);
			if (result<0) min = ptr + 1;
			else if (result>0) max = ptr;
			else 
			{
				if (index) *index = ptr;
				return 1;
			}
		}
		if (index) *index = min;
		return 0;
	}


	bool bsearch_range(int (__cdecl *compare )(T elem1, T elem2 ),T item,int * index,int * count) const 
	{
		int max = get_count();
		int min = 0;
		int ptr;
		while(min<max)
		{
			ptr = min + (max-min) / 2;		
			T& test = buffer[ptr];
			int result = compare(test,item);
			if (result<0) min = ptr + 1;
			else if (result>0) max = ptr;
			else 
			{
				int num = 1;
				while(ptr>0 && !compare(buffer[ptr-1],item)) {ptr--;num++;}
				while(ptr+num<get_count() && !compare(buffer[ptr+num],item)) num++;
				if (index) *index = ptr;
				if (count) *count = num;
				return 1;
			}
		}
		if (index) *index = min;
		return 0;
	}

	bool bsearch_param(int (__cdecl *compare )(T elem1, const void * param ),const void * param,int * index) const 
	{
		int max = get_count();
		int min = 0;
		int ptr;
		while(min<max)
		{
			ptr = min + (max-min) / 2;
			T test = buffer[ptr];
			int result = compare(test,param);
			if (result<0) min = ptr + 1;
			else if (result>0) max = ptr;
			else 
			{
				if (index) *index = ptr;
				return 1;
			}
		}
		if (index) *index = min;
		return 0;
	}




	inline void apply_order(const int * order,unsigned count)
	{
		assert(count==get_count());
		buffer.apply_order(order,count);
	}
	
	void remove_mask(const bit_array & mask) {filter_mask(bit_array_not(mask));}

	void remove_mask(const bool * mask) {remove_mask(bit_array_table(mask,get_count()));}
	void filter_mask(const bool * mask) {filter_mask(bit_array_table(mask,get_count()));}

	unsigned add_item_ref(const T& item)
	{
		unsigned idx = get_count();
		insert_item_ref(item,idx);
		return idx;
	}

	unsigned add_item(T item)//returns index
	{
		unsigned idx = get_count();
		insert_item(item,idx);
		return idx;
	}

	void remove_all() {remove_mask(bit_array_true());}

	void remove_item(T item)
	{
		unsigned n,max = get_count();
		mem_block_t<bool> mask(max);
		for(n=0;n<max;n++)
			mask[n] = get_item(n)==item;
		remove_mask(bit_array_table(mask));		
	}

	int find_item(T item) const//returns index of first occurance, -1 if not found
	{
		unsigned n,max = get_count();
		for(n=0;n<max;n++)
			if (get_item(n)==item) return n;
		return -1;
	}

	inline bool have_item(T item) const {return find_item(item)>=0;}

	inline void swap(unsigned idx1,unsigned idx2)
	{
		replace_item(idx1,replace_item(idx2,get_item(idx1)));
	}

};

#define mem_block_list mem_block_list_t //for compatibility

#endif //_PFC_LIST_H_