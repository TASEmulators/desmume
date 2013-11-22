#include "foobar2000.h"

int tag_reader::g_run(reader * r,file_info * info,const char * name)
{
	if (!r->can_seek()) return 0;//dont bother
	__int64 offset = r->get_position();
	int rv=0;
	service_enum_t<tag_reader> e;
	tag_reader * ptr;
	for(ptr=e.first();ptr && !rv;ptr = e.next())
	{
		const char * s_name = ptr->get_name();
		if (!stricmp_utf8(name,s_name))
		{
			r->seek(0);
			rv = ptr->run(r,info);
		}
		ptr->service_release();
	}
	r->seek(offset);
//	if (rv) info->info_set("tagtype",name);
	return rv;
}

int tag_reader::g_run_multi(reader * r,file_info * info,const char * list)
{
	for(;;)
	{
		const char * sep = strchr(list,'|');
		if (sep==0) return *list ? g_run(r,info,list) : 0;
		if (sep>list && g_run(r,info,string8(list,sep-list))) return 1;
		list = sep + 1;
	}
}

int tag_writer::g_run(reader * r,const file_info * info,const char * name)
{
	if (!r->can_seek()) return 0;//dont bother
	__int64 offset = r->get_position();
	int rv=0;
	service_enum_t<tag_writer> e;
	tag_writer * ptr;
	for(ptr=e.first();ptr && !rv;ptr = e.next())
	{
		const char * s_name = ptr->get_name();

		if (!stricmp_utf8(name,s_name))
		{
			r->seek(0);
			rv = ptr->run(r,info);
		}

		ptr->service_release();
	}
	r->seek(offset);
	return rv;

}

void tag_remover::g_run(reader * r)
{
	service_enum_t<tag_remover> e;
	tag_remover * ptr;
	for(ptr=e.first();ptr;ptr = e.next())
	{
		r->seek(0);
		ptr->run(r);
		ptr->service_release();
	}
}

bool tag_reader::g_have_type(const char * name)
{
	service_enum_t<tag_reader> e;
	tag_reader * ptr;
	bool found = false;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (!stricmp_utf8(name,ptr->get_name())) found = true;
		ptr->service_release();
		if (found) break;
	}
	return found;
}

bool tag_writer::g_have_type(const char * name)
{
	service_enum_t<tag_writer> e;
	tag_writer * ptr;
	bool found = false;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (!stricmp_utf8(name,ptr->get_name())) found = true;
		ptr->service_release();
		if (found) break;
	}
	return found;
}