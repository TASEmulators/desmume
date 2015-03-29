#ifndef _PFC_OTHER_H_
#define _PFC_OTHER_H_

template<class T>
class vartoggle_t
{
	T oldval;
	T & var;
public:
	vartoggle_t(T & p_var,T val) : var(p_var)
	{
		oldval = var;
		var = val;
	}
	~vartoggle_t() {var = oldval;}
};

typedef vartoggle_t<bool> booltoggle;

class order_helper
{
	mem_block_t<int> data;
public:
	explicit order_helper(unsigned size) : data(size)
	{
		unsigned n;
		for(n=0;n<size;n++) data[n]=n;
	}

	int get_item(unsigned ptr) {return data[ptr];}

	int & operator[](unsigned ptr) {return data[ptr];}

	static void g_swap(int * data,unsigned ptr1,unsigned ptr2);
	inline void swap(unsigned ptr1,unsigned ptr2) {g_swap(data,ptr1,ptr2);}

	operator const int * () {return data;}

	static int g_find_reverse(const int * order,int val);
	inline int find_reverse(int val) {return g_find_reverse(data,val);}

	static void g_reverse(int * order,unsigned base,unsigned count);
	inline void reverse(unsigned base,unsigned count) {g_reverse(data,base,count);}
};

class fpu_control
{
	unsigned old_val;
	unsigned mask;
public:
	inline fpu_control(unsigned p_mask,unsigned p_val)
	{
		mask = p_mask;
		old_val = _controlfp(p_val,mask);
	}
	inline ~fpu_control()
	{
		_controlfp(old_val,mask);
	}
};

class fpu_control_roundnearest : private fpu_control
{
public:
	fpu_control_roundnearest() : fpu_control(_MCW_RC,_RC_NEAR) {}
};

class fpu_control_flushdenormal : private fpu_control
{
public:
	fpu_control_flushdenormal() : fpu_control(_MCW_DN,_DN_FLUSH) {}
};

class fpu_control_default : private fpu_control
{
public:
	fpu_control_default() : fpu_control(_MCW_DN|_MCW_RC,_DN_FLUSH|_RC_NEAR) {}
};

#endif