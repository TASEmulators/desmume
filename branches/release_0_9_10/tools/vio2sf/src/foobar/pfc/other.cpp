#include "pfc.h"

#include <intrin.h>

namespace pfc {
	bool permutation_is_valid(t_size const * order, t_size count) {
		bit_array_bittable found(count);
		for(t_size walk = 0; walk < count; ++walk) {
			if (order[walk] >= count) return false;
			if (found[walk]) return false;
			found.set(walk,true);
		}
		return true;
	}
	void permutation_validate(t_size const * order, t_size count) {
		if (!permutation_is_valid(order,count)) throw exception_invalid_permutation();
	}

	t_size permutation_find_reverse(t_size const * order, t_size count, t_size value) {
		if (value >= count) return ~0;
		for(t_size walk = 0; walk < count; ++walk) {
			if (order[walk] == value) return walk;
		}
		return ~0;
	}

	void create_move_items_permutation(t_size * p_output,t_size p_count,const bit_array & p_selection,int p_delta) {
		t_size * const order = p_output;
		const t_size count = p_count;

		pfc::array_t<bool> selection; selection.set_size(p_count);
		
		for(t_size walk = 0; walk < count; ++walk) {
			order[walk] = walk;
			selection[walk] = p_selection[walk];
		}

		if (p_delta<0)
		{
			for(;p_delta<0;p_delta++)
			{
				t_size idx;
				for(idx=1;idx<count;idx++)
				{
					if (selection[idx] && !selection[idx-1])
					{
						pfc::swap_t(order[idx],order[idx-1]);
						pfc::swap_t(selection[idx],selection[idx-1]);
					}
				}
			}
		}
		else
		{
			for(;p_delta>0;p_delta--)
			{
				t_size idx;
				for(idx=count-2;(int)idx>=0;idx--)
				{
					if (selection[idx] && !selection[idx+1])
					{
						pfc::swap_t(order[idx],order[idx+1]);
						pfc::swap_t(selection[idx],selection[idx+1]);
					}
				}
			}
		}
	}
}

void order_helper::g_swap(t_size * data,t_size ptr1,t_size ptr2)
{
	t_size temp = data[ptr1];
	data[ptr1] = data[ptr2];
	data[ptr2] = temp;
}


t_size order_helper::g_find_reverse(const t_size * order,t_size val)
{
	t_size prev = val, next = order[val];
	while(next != val)
	{
		prev = next;
		next = order[next];
	}
	return prev;
}


void order_helper::g_reverse(t_size * order,t_size base,t_size count)
{
	t_size max = count>>1;
	t_size n;
	t_size base2 = base+count-1;
	for(n=0;n<max;n++)
		g_swap(order,base+n,base2-n);
}


void pfc::crash() {
#ifdef _MSC_VER
	__debugbreak();
#else
	*(char*)NULL = 0;
#endif
}


void pfc::byteswap_raw(void * p_buffer,const t_size p_bytes) {
	t_uint8 * ptr = (t_uint8*)p_buffer;
	t_size n;
	for(n=0;n<p_bytes>>1;n++) swap_t(ptr[n],ptr[p_bytes-n-1]);
}

#if defined(_DEBUG) && defined(_WIN32)
void pfc::myassert(const wchar_t * _Message, const wchar_t *_File, unsigned _Line) { 
	if (IsDebuggerPresent()) pfc::crash();
	_wassert(_Message,_File,_Line);
}
#endif


t_uint64 pfc::pow_int(t_uint64 base, t_uint64 exp) {
	t_uint64 mul = base;
	t_uint64 val = 1;
	t_uint64 mask = 1;
	while(exp != 0) {
		if (exp & mask) {
			val *= mul;
			exp ^= mask;
		}
		mul = mul * mul;
		mask <<= 1;
	}
	return val;
}
