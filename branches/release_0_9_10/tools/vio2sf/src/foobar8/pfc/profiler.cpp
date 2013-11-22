#include "pfc.h"


__declspec(naked) __int64 profiler_local::get_timestamp()
{
	__asm
	{
		rdtsc
		ret
	}
}