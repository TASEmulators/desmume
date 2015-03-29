#include "pfc.h"

#ifdef _WINDOWS
namespace pfc {

profiler_static::profiler_static(const char * p_name)
{
	name = p_name;
	total_time = 0;
	num_called = 0;
}

profiler_static::~profiler_static()
{
	try {
		pfc::string_fixed_t<511> message;
		message << "profiler: " << pfc::format_pad_left<pfc::string_fixed_t<127> >(48,' ',name) << " - " << 
			pfc::format_pad_right<pfc::string_fixed_t<128> >(16,' ',pfc::format_uint(total_time) ) << " cycles";

		if (num_called > 0) {
			message << " (executed " << num_called << " times, " << (total_time / num_called) << " average)";
		}
		message << "\n";
		OutputDebugStringA(message);
	} catch(...) {
		//should never happen
		OutputDebugString(_T("unexpected profiler failure\n"));
	}
}

}
#else
//PORTME
#endif
