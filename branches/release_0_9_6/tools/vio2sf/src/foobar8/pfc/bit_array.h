#ifndef _PFC_BIT_ARRAY_H_
#define _PFC_BIT_ARRAY_H_

class NOVTABLE bit_array
{
public:
	virtual bool get(int n) const = 0;
	virtual int find(bool val,int start,int count) const//can be overridden for improved speed; returns first occurance of val between start and start+count (excluding start+count), or start+count if not found; count may be negative if we're searching back
	{
		int d, todo, ptr = start;
		if (count==0) return start;
		else if (count<0) {d = -1; todo = -count;}
		else {d = 1; todo = count;}
		while(todo>0 && get(ptr)!=val) {ptr+=d;todo--;}
		return ptr;
	}
	inline bool operator[](int n) const {return get(n);}

	unsigned calc_count(bool val,int start,unsigned count) const//counts number of vals for start<=n<start+count
	{
		unsigned found = 0;
		int max = start+count;
		int ptr;
		for(ptr=find(val,start,count);ptr<max;ptr=find(val,ptr+1,max-ptr-1)) found++;
		return found;
	}
};

class NOVTABLE bit_array_var : public bit_array
{
public:
	virtual void set(int n,bool val)=0;
};


template<class T> class mem_block_t; //blah

template<class T>
class bit_array_table_t : public bit_array
{
	const T * data;
	int count;
	bool before,after;
public:
	inline bit_array_table_t(const T * p_data,int p_count,bool p_before = false,bool p_after = false)
		: data(p_data), count(p_count), before(p_before), after(p_after)
	{
	}

	inline bit_array_table_t(const mem_block_t<T> & p_data,bool p_before = false,bool p_after = false)
		: data(p_data.get_ptr()), count(p_data.get_size()), before(p_before), after(p_after)
	{
	}

	virtual bool get(int n) const
	{
		if (n<0) return before;
		else if (n<count) return !!data[n];
		else return after;
	}
};

typedef bit_array_table_t<bool> bit_array_table;

class bit_array_range : public bit_array
{
	int begin,end;
	bool state;
public:
	bit_array_range(int first,int count,bool p_state = true) : begin(first), end(first+count), state(p_state) {}
	
	virtual bool get(int n) const
	{
		bool rv = n>=begin && n<end;
		if (!state) rv = !rv;
		return rv;
	}
};

class bit_array_and : public bit_array
{
	const bit_array & a1, & a2;
public:
	bit_array_and(const bit_array & p_a1, const bit_array & p_a2) : a1(p_a1), a2(p_a2) {}
	virtual bool get(int n) const {return a1.get(n) && a2.get(n);}
};

class bit_array_or : public bit_array
{
	const bit_array & a1, & a2;
public:
	bit_array_or(const bit_array & p_a1, const bit_array & p_a2) : a1(p_a1), a2(p_a2) {}
	virtual bool get(int n) const {return a1.get(n) || a2.get(n);}
};

class bit_array_xor : public bit_array
{
	const bit_array & a1, & a2;
public:
	bit_array_xor(const bit_array & p_a1, const bit_array & p_a2) : a1(p_a1), a2(p_a2) {}
	virtual bool get(int n) const
	{
		bool v1 = a1.get(n), v2 = a2.get(n);
		return (v1 && !v2) || (!v1 && v2);
	}
};

class bit_array_not : public bit_array
{
	const bit_array & a1;
public:
	bit_array_not(const bit_array & p_a1) : a1(p_a1) {}
	virtual bool get(int n) const {return !a1.get(n);}
	virtual int find(bool val,int start,int count) const
	{return a1.find(!val,start,count);}

};

class bit_array_true : public bit_array
{
public:
	virtual bool get(int n) const {return true;}
	virtual int find(bool val,int start,int count) const
	{return val ? start : start+count;}
};

class bit_array_false : public bit_array
{
public:
	virtual bool get(int n) const {return false;}
	virtual int find(bool val,int start,int count) const
	{return val ? start+count : start;}
};

class bit_array_val : public bit_array
{
	bool val;
public:
	bit_array_val(bool v) : val(v) {}
	virtual bool get(int n) const {return val;}
	virtual int find(bool p_val,int start,int count) const
	{return val==p_val ? start : start+count;}
};

class bit_array_shift : public bit_array
{
	int offset;
	const bit_array & a1;
public:
	bit_array_shift(const bit_array & p_a1,int p_offset) : a1(p_a1), offset(p_offset) {}
	virtual bool get(int n) const {return a1.get(n-offset);}
	virtual int find(bool val,int start,int count) const
	{return a1.find(val,start-offset,count)+offset;}
};

class bit_array_one : public bit_array
{
	int val;
public:
	bit_array_one(int p_val) : val(p_val) {}
	virtual bool get(int n) const {return n==val;}

	virtual int find(bool p_val,int start,int count) const
	{
		if (count==0) return start;
		else if (p_val)
		{
			if (count>0)
				return (val>=start && val<start+count) ? val : start+count;
			else
				return (val<=start && val>start+count) ? val : start+count;
		}
		else
		{
			if (start == val) return count>0 ? start+1 : start-1;
			else return start;
		}
	}
};

class bit_array_bittable : public bit_array_var
{
	unsigned char * data;
	unsigned count;
public:
	//helpers
	inline static bool g_get(const unsigned char * ptr,unsigned idx)
	{
		return !! (ptr[idx>>3] & (1<<(idx&7)));
	}

	inline static void g_set(unsigned char * ptr,unsigned idx,bool val)
	{
		unsigned char & dst = ptr[idx>>3];
		unsigned char mask = 1<<(idx&7);
		dst = val ? dst|mask : dst&~mask;
	}

	inline static unsigned g_estimate_size(unsigned count) {return (count+7)>>3;}

	void resize(unsigned p_count)
	{
		count = p_count;
		unsigned bytes = g_estimate_size(count);
		if (bytes==0)
		{
			if (data) {free(data);data=0;}
		}
		else
		{
			data = data ? (unsigned char*)realloc(data,bytes) : (unsigned char*)malloc(bytes);
			memset(data,0,bytes);
		}
	}

	inline bit_array_bittable(unsigned p_count) {count=0;data=0;resize(p_count);}
	inline ~bit_array_bittable() {free(data);}
	
	virtual void set(int n,bool val)
	{
		if ((unsigned)n<count)
		{
			g_set(data,n,val);
		}
	}

	virtual bool get(int n) const
	{
		bool rv = false;
		if ((unsigned)n<count)
		{
			rv = g_get(data,n);
		}
		return rv;
	}
};

#define for_each_bit_array(var,mask,val,start,count) for(var = mask.find(val,start,count);var<start+count;var=mask.find(val,var+1,count-(var+1-start)))

#endif