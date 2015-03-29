#ifndef _PFC_BIT_ARRAY_H_
#define _PFC_BIT_ARRAY_H_

class NOVTABLE bit_array {
public:
	virtual bool get(t_size n) const = 0;
	virtual t_size find(bool val,t_size start,t_ssize count) const//can be overridden for improved speed; returns first occurance of val between start and start+count (excluding start+count), or start+count if not found; count may be negative if we're searching back
	{
		t_ssize d, todo, ptr = start;
		if (count==0) return start;
		else if (count<0) {d = -1; todo = -count;}
		else {d = 1; todo = count;}
		while(todo>0 && get(ptr)!=val) {ptr+=d;todo--;}
		return ptr;
	}
	inline bool operator[](t_size n) const {return get(n);}

	t_size calc_count(bool val,t_size start,t_size count,t_size count_max = ~0) const//counts number of vals for start<=n<start+count
	{
		t_size found = 0;
		t_size max = start+count;
		t_size ptr;
		for(ptr=find(val,start,count);found<count_max && ptr<max;ptr=find(val,ptr+1,max-ptr-1)) found++;
		return found;
	}

	inline t_size find_first(bool val,t_size start,t_size max) const {return find(val,start,max-start);}
	inline t_size find_next(bool val,t_size previous,t_size max) const {return find(val,previous+1,max-(previous+1));}
	//for(n = mask.find_first(true,0,m); n < m; n = mask.find_next(true,n,m) )
protected:
	bit_array() {}
	~bit_array() {}
};

class NOVTABLE bit_array_var : public bit_array {
public:
	virtual void set(t_size n,bool val)=0;
protected:
	bit_array_var() {}
	~bit_array_var() {}
};


class bit_array_wrapper_permutation : public bit_array {
public:
	bit_array_wrapper_permutation(const t_size * p_permutation, t_size p_size) : m_permutation(p_permutation), m_size(p_size) {}
	bool get(t_size n) const {
		if (n < m_size) {
			return m_permutation[n] != n;
		} else {
			return false;
		}
	}
private:
	const t_size * const m_permutation;
	const t_size m_size;
};

#endif
