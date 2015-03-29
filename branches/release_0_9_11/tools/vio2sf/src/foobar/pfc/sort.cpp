#include "pfc.h"

#if defined(_M_IX86) || defined(_M_IX64)
#include <intrin.h>
#define PFC_HAVE_RDTSC
#endif

namespace pfc {

void swap_void(void * item1,void * item2,t_size width)
{
	unsigned char * ptr1 = (unsigned char*)item1, * ptr2 = (unsigned char*)item2;
	t_size n;
	unsigned char temp;
	for(n=0;n<width;n++)
	{
		temp = *ptr2;
		*ptr2 = *ptr1;
		*ptr1 = temp;
		ptr1++;
		ptr2++;
	}
}

void reorder(reorder_callback & p_callback,const t_size * p_order,t_size p_count)
{
	t_size done_size = bit_array_bittable::g_estimate_size(p_count);
	pfc::array_hybrid_t<unsigned char,1024> done;
	done.set_size(done_size);
	pfc::memset_t(done,(unsigned char)0);
	t_size n;
	for(n=0;n<p_count;n++)
	{
		t_size next = p_order[n];
		if (next!=n && !bit_array_bittable::g_get(done,n))
		{
			t_size prev = n;
			do
			{
				assert(!bit_array_bittable::g_get(done,next));
				assert(next>n);
				assert(n<p_count);
				p_callback.swap(prev,next);
				bit_array_bittable::g_set(done,next,true);
				prev = next;
				next = p_order[next];
			} while(next!=n);
			//bit_array_bittable::g_set(done,n,true);
		}
	}
}

void reorder_void(void * data,t_size width,const t_size * order,t_size num,void (*swapfunc)(void * item1,void * item2,t_size width))
{
	unsigned char * base = (unsigned char *) data;
	t_size done_size = bit_array_bittable::g_estimate_size(num);
	pfc::array_hybrid_t<unsigned char,1024> done;
	done.set_size(done_size);
	pfc::memset_t(done,(unsigned char)0);
	t_size n;
	for(n=0;n<num;n++)
	{
		t_size next = order[n];
		if (next!=n && !bit_array_bittable::g_get(done,n))
		{
			t_size prev = n;
			do
			{
				assert(!bit_array_bittable::g_get(done,next));
				assert(next>n);
				assert(n<num);
				swapfunc(base+width*prev,base+width*next,width);
				bit_array_bittable::g_set(done,next,true);
				prev = next;
				next = order[next];
			} while(next!=n);
			//bit_array_bittable::g_set(done,n,true);
		}
	}
}

namespace {

class sort_callback_impl_legacy : public sort_callback
{
public:
	sort_callback_impl_legacy(
		void * p_base,t_size p_width, 
		int (*p_comp)(const void *, const void *),
		void (*p_swap)(void *, void *, t_size)
		) :
	m_base((char*)p_base), m_width(p_width),
	m_comp(p_comp), m_swap(p_swap)
	{
	}


	int compare(t_size p_index1, t_size p_index2) const
	{
		return m_comp(m_base + p_index1 * m_width, m_base + p_index2 * m_width);
	}
	
	void swap(t_size p_index1, t_size p_index2)
	{
		m_swap(m_base + p_index1 * m_width, m_base + p_index2 * m_width, m_width);
	}

private:
	char * m_base;
	t_size m_width;
    int (*m_comp)(const void *, const void *);
	void (*m_swap)(void *, void *, t_size);
};
}

void sort_void_ex (
    void *base,
    t_size num,
    t_size width,
    int (*comp)(const void *, const void *),
	void (*swap)(void *, void *, t_size) )
{
	sort(sort_callback_impl_legacy(base,width,comp,swap),num);
}

static void squaresort(pfc::sort_callback & p_callback,t_size const p_base,t_size const p_count) {
	const t_size max = p_base + p_count;
	for(t_size walk = p_base + 1; walk < max; ++walk) {
		for(t_size prev = p_base; prev < walk; ++prev) {
			p_callback.swap_check(prev,walk);
		}
	}
}


inline static void __sort_2elem_helper(pfc::sort_callback & p_callback,t_size & p_elem1,t_size & p_elem2) {
	if (p_callback.compare(p_elem1,p_elem2) > 0) pfc::swap_t(p_elem1,p_elem2);
}


#ifdef PFC_HAVE_RDTSC
static inline t_uint64 uniqueVal() {return __rdtsc();}
#else
static counter::t_val uniqueVal() {
	static counter c; return ++c;
}
#endif

static t_size myrand(t_size count) {
	static_assert<RAND_MAX == 0x7FFF>();

	t_uint64 val;
	val = (t_uint64) rand() | (t_uint64)( (t_uint32)rand() << 16 );

	val ^= uniqueVal();

	return (t_size)(val % count);
}

inline static t_size __pivot_helper(pfc::sort_callback & p_callback,t_size const p_base,t_size const p_count) {
	PFC_ASSERT(p_count > 2);
	
	//t_size val1 = p_base, val2 = p_base + (p_count / 2), val3 = p_base + (p_count - 1);
	
	t_size val1 = myrand(p_count), val2 = myrand(p_count-1), val3 = myrand(p_count-2);
	if (val2 >= val1) val2++;
	if (val3 >= val1) val3++;
	if (val3 >= val2) val3++;

	val1 += p_base; val2 += p_base; val3 += p_base;
	
	__sort_2elem_helper(p_callback,val1,val2);
	__sort_2elem_helper(p_callback,val1,val3);
	__sort_2elem_helper(p_callback,val2,val3);

	return val2;
}

static void newsort(pfc::sort_callback & p_callback,t_size const p_base,t_size const p_count) {
	if (p_count <= 4) {
		squaresort(p_callback,p_base,p_count);
		return;
	}

	t_size pivot = __pivot_helper(p_callback,p_base,p_count);

	{
		const t_size target = p_base + p_count - 1;
		if (pivot != target) {
			p_callback.swap(pivot,target); pivot = target;
		}
	}


	t_size partition = p_base;
	{
		bool asdf = false;
		for(t_size walk = p_base; walk < pivot; ++walk) {
			const int comp = p_callback.compare(walk,pivot);
			bool trigger = false;
			if (comp == 0) {
				trigger = asdf;
				asdf = !asdf;
			} else if (comp < 0) {
				trigger = true;
			}
			if (trigger) {
				if (partition != walk) p_callback.swap(partition,walk);
				partition++;
			}
		}
	}	
	if (pivot != partition) {
		p_callback.swap(pivot,partition); pivot = partition;
	}

	newsort(p_callback,p_base,pivot-p_base);
	newsort(p_callback,pivot+1,p_count-(pivot+1-p_base));
}

void sort(pfc::sort_callback & p_callback,t_size p_num) {
	srand((unsigned int)(uniqueVal() ^ p_num));
	newsort(p_callback,0,p_num);
}


void sort_void(void * base,t_size num,t_size width,int (*comp)(const void *, const void *) )
{
	sort_void_ex(base,num,width,comp,swap_void);
}




sort_callback_stabilizer::sort_callback_stabilizer(sort_callback & p_chain,t_size p_count)
: m_chain(p_chain)
{
	m_order.set_size(p_count);
	t_size n;
	for(n=0;n<p_count;n++) m_order[n] = n;
}

int sort_callback_stabilizer::compare(t_size p_index1, t_size p_index2) const
{
	int ret = m_chain.compare(p_index1,p_index2);
	if (ret == 0) ret = pfc::sgn_t((t_ssize)m_order[p_index1] - (t_ssize)m_order[p_index2]);
	return ret;
}

void sort_callback_stabilizer::swap(t_size p_index1, t_size p_index2)
{
	m_chain.swap(p_index1,p_index2);
	pfc::swap_t(m_order[p_index1],m_order[p_index2]);
}


void sort_stable(sort_callback & p_callback,t_size p_count)
{
	sort(sort_callback_stabilizer(p_callback,p_count),p_count);
}

}

