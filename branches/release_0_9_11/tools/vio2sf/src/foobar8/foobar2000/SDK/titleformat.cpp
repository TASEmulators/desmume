#include "foobar2000.h"

void titleformat::g_run(const file_info * source,string_base & out,const char * spec,const char * extra_items)
{
	titleformat * ptr = service_enum_create_t(titleformat,0);
	if (ptr)
	{
		ptr->run(source,out,spec,extra_items);
		ptr->service_release();//actually safe not to release it
	}
}

void titleformat::remove_color_marks(const char * src,string8 & out)//helper
{
	out.reset();
	while(*src)
	{
		if (*src==3)
		{
			src++;
			while(*src && *src!=3) src++;
			if (*src==3) src++;
		}
		else out.add_byte(*src++);
	}
}