#ifndef _FOOBAR2000_SERVICE_HELPER_H_
#define _FOOBAR2000_SERVICE_HELPER_H_

#include "service.h"

template<class T>	
class service_list_t : public ptr_list_t<T>
{
public:
	void delete_item(T * ptr)
	{
		remove_item(ptr);
		ptr->service_release();
	} 

	void delete_by_idx(int idx)
	{
		T * ptr = remove_by_idx(idx);
		if (ptr) ptr->service_release();
	}

	void delete_all()
	{
		int n,max=get_count();
		for(n=0;n<max;n++)
		{
			T * ptr = get_item(n);
			if (ptr) ptr->service_release();
		}
		remove_all();
	}

	void delete_mask(const bit_array & mask)
	{
		int n,m=get_count();
		for(n=0;n<m;n++)
			if (mask[n]) get_item(n)->service_release();
		remove_mask(mask);
	}
};

template<class T>
class service_list_autodel_t : public service_list_t<T>
{
public:
	inline ~service_list_autodel_t() {delete_all();}
};

template<class T>
class service_ptr_autodel_t
{
	T * ptr;
public:
	inline T* operator=(T* param) {return ptr = param;}
	inline T* operator->() {return ptr;}
	inline ~service_ptr_autodel_t() {if (ptr) ptr->service_release();}
	inline service_ptr_autodel_t(T * param=0) {ptr=param;}
	inline operator T* () {return ptr;}
	inline void release()
	{
		T* temp = ptr;
		ptr = 0;
		if (temp) temp->service_release_safe();
	}
	inline bool is_empty() {return ptr==0;}
};

#endif