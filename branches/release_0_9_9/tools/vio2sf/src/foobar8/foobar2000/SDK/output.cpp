#include "foobar2000.h"

output * output::create(GUID g)
{
	output * ptr;
	service_enum_t<output> e;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_guid()==g) return ptr;
		ptr->service_release();
	}
	return 0;
}


void output_i_base::set_error(const char * msg)
{
	console::error(msg);
}