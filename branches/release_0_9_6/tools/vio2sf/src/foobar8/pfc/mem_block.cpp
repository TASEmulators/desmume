#include "pfc.h"

#define HAVE_ALLOC_FAST


void * mem_block::set_size(UINT new_used)
{
	if (new_used==0)
	{
#ifdef HAVE_ALLOC_FAST
		if (mem_logic != ALLOC_FAST_DONTGODOWN)
#endif
		{
			if (data!=0) {free(data);data=0;}
			size = 0;
		}
	}
	else
	{
		UINT new_size;
#ifdef HAVE_ALLOC_FAST
		if (mem_logic == ALLOC_FAST || mem_logic == ALLOC_FAST_DONTGODOWN)
		{
			new_size = size;
			if (new_size < 1) new_size = 1;
			while(new_size < new_used) new_size <<= 1; 
			if (mem_logic!=ALLOC_FAST_DONTGODOWN) while(new_size>>1 > new_used) new_size >>= 1;
		}
		else
#endif
		{
			new_size = new_used;
		}

		if (new_size!=size)
		{
			if (data==0)
			{
				data = malloc(new_size);
			}
			else
			{
				void * new_data;
				new_data = realloc(data,new_size);
				if (new_data==0) free(data);
				data = new_data;

			}
			size = new_size;
		}
	}
	used = new_used;
	return data;
}

void mem_block::prealloc(UINT num)
{
#ifdef HAVE_ALLOC_FAST
	if (size<num && mem_logic==ALLOC_FAST_DONTGODOWN)
	{
		int old_used = used;
		set_size(num);
		used = old_used;
	}
#endif
}

void* mem_block::copy(const void *ptr, unsigned bytes,unsigned start)
{
	check_size(bytes+start);

	if (ptr) 
		memcpy((char*)get_ptr()+start,ptr,bytes);
	else 
		memset((char*)get_ptr()+start,0,bytes);
	
	return (char*)get_ptr()+start;
}

inline static void apply_order_swap(unsigned char * base,unsigned size,unsigned ptr1,unsigned ptr2,unsigned char * temp)
{
	unsigned char * realptr1 = base + ptr1 * size, * realptr2 = base + ptr2 * size;
	memcpy(temp,realptr1,size);
	memcpy(realptr1,realptr2,size);
	memcpy(realptr2,temp,size);
}

void mem_block::g_apply_order(void * data,unsigned size,const int * order,unsigned num)
{
	unsigned char * base = (unsigned char *) data;
	unsigned char * temp = (unsigned char*)alloca(size);
	unsigned done_size = bit_array_bittable::g_estimate_size(num);
	mem_block_t<unsigned char> done_block;
	unsigned char * done = done_size > PFC_ALLOCA_LIMIT ? done_block.set_size(done_size) : (unsigned char*)alloca(done_size);
	memset(done,0,done_size);
	unsigned n;
	for(n=0;n<num;n++)
	{
		unsigned next = order[n];
		if (next!=n && !bit_array_bittable::g_get(done,n))
		{
			unsigned prev = n;
			do
			{
				assert(!bit_array_bittable::g_get(done,next));
				assert(next>n);
				assert(n<num);
				apply_order_swap(base,size,prev,next,temp);
				bit_array_bittable::g_set(done,next,true);
				prev = next;
				next = order[next];
			} while(next!=n);
			//bit_array_bittable::g_set(done,n,true);
		}
	}
}
