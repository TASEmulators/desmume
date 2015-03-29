#include "foobar2000.h"

bool app_close_blocker::g_query()
{
	service_ptr_t<app_close_blocker> ptr;
	service_enum_t<app_close_blocker> e;
	while(e.next(ptr))
	{
		if (!ptr->query()) return false;
	}
	return true;
}