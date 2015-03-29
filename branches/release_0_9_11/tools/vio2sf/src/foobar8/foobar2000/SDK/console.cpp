#include "foobar2000.h"

bool console::present()
{
	bool rv = false;
	console * ptr = service_enum_create_t(console,0);
	if (ptr)
	{
		rv = true;
		ptr->service_release();
	}
	return rv;
}

void console::popup()
{
	console * ptr = service_enum_create_t(console,0);
	if (ptr)
	{
		ptr->_popup();
		ptr->service_release();
	}	
}

void console::beep()
{
	console * ptr = service_enum_create_t(console,0);
	if (ptr)
	{
		ptr->_beep();
		ptr->service_release();
	}	
}

void console::output(int severity,const char * message,const char * source)
{
	console * ptr = service_enum_create_t(console,0);
	if (ptr)
	{
		ptr->_output(severity,message,source);
		ptr->service_release();
	}
}

void console::output(int severity,const char * message)
{
	output(severity,message,core_api::get_my_file_name());
}

void console::info_location(const class playable_location * src)
{
	const char * path = src->get_path();
	int number = src->get_subsong_index();
	info(uStringPrintf("location: \"%s\" (%i)",path,number));
}

void console::info_location(class metadb_handle * src)
{
	info_location(src->handle_get_location());
}

void console::info_location(const class file_info * src)
{
	info_location(src->get_location());
}

void console::info_full(const class file_info * src)
{
	info_location(src);
	string8_fastalloc temp;
	unsigned n,m;
	m = src->meta_get_count();
	info("meta:");
	for(n=0;n<m;n++)
	{
		temp.reset();
		temp += src->meta_enum_name(n);
		temp += "=";
		temp += src->meta_enum_value(n);
		info(temp);
	}
	info(temp);
	m = src->info_get_count();
	info("info:");
	for(n=0;n<m;n++)
	{
		temp.reset();
		temp += src->info_enum_name(n);
		temp += "=";
		temp += src->info_enum_value(n);
		info(temp);
	}
	info(temp);
}
