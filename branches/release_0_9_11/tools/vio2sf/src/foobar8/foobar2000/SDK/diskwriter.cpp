#include "foobar2000.h"


diskwriter * diskwriter::instantiate(const GUID & guid)
{
	service_enum_t<diskwriter> e;
	diskwriter * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_guid() == guid) return ptr;
		ptr->service_release();
	}
	return 0;
}

bool diskwriter::instantiate_test(const GUID & guid)
{
	service_enum_t<diskwriter> e;
	diskwriter * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		bool found = (ptr->get_guid() == guid) ? true : false;
		ptr->service_release();
		if (found) return true;
	}
	return false;
}
