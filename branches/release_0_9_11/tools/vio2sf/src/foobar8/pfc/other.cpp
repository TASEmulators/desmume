#include "pfc.h"

void order_helper::g_swap(int * data,unsigned ptr1,unsigned ptr2)
{
	int temp = data[ptr1];
	data[ptr1] = data[ptr2];
	data[ptr2] = temp;
}


int order_helper::g_find_reverse(const int * order,int val)
{
	int prev = val, next = order[val];
	while(next != val)
	{
		prev = next;
		next = order[next];
	}
	return prev;
}


void order_helper::g_reverse(int * order,unsigned base,unsigned count)
{
	unsigned max = count>>1;
	unsigned n;
	unsigned base2 = base+count-1;
	for(n=0;n<max;n++)
		g_swap(order,base+n,base2-n);
}