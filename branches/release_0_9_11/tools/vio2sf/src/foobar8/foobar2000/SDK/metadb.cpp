#include "foobar2000.h"

int metadb::query(file_info * out)
{
	playable_location_i temp(*out->get_location());
	return query(&temp,out);
}

int metadb::query_flush(file_info * out)
{
	playable_location_i temp(*out->get_location());
	return query_flush(&temp,out);
}

int metadb::query_dbonly(file_info * out)
{
	playable_location_i temp(*out->get_location());
	return query_dbonly(&temp,out);
}

int metadb::query_meta_field(const playable_location * src,const char * name,int num,string8 & out)
{
	int rv = 0;
	metadb_handle * ptr = handle_create(src);
	if (ptr)
	{
		rv = ptr->handle_query_meta_field(name,num,out);
		ptr->handle_release();
	}
	return rv;
}

int metadb::update_info(const file_info * info,bool dbonly)//using playable_location from file_info, return -1 on failure, 0 if pending, 1 on immediate success; may take a few good seconds to execute
{
	int rv;
	metadb_handle * handle = handle_create(info->get_location());
	rv = handle->handle_update_info(info,dbonly);
	handle->handle_release();
	return rv;
}

void metadb::user_requested_remove(const playable_location * entry)//use this when user requests files to be removed from database (but not deleted physically)
{
	metadb_handle * ptr = handle_create(entry);
	ptr->handle_user_requested_remove();
	ptr->handle_release();
}

int metadb::query(const playable_location * entry,file_info * out)
{
	int rv;
	metadb_handle * handle = handle_create(entry);
	rv = handle->handle_query(out,false);
	handle->handle_release();
	return rv;
}

int metadb::query_flush(const playable_location * entry,file_info * out)
{
	int rv;
	metadb_handle * handle = handle_create(entry);
	handle->handle_flush_info();
	rv = handle->handle_query(out,false);
	handle->handle_release();
	return rv;
}

int metadb::query_dbonly(const playable_location * entry,file_info * out)
{
	int rv;
	metadb_handle * handle = handle_create(entry);
	rv = handle->handle_query(out,true);
	handle->handle_release();
	return rv;
}

int metadb::precache(const playable_location * entry,reader * r)
{
	int rv;
	metadb_handle * handle = handle_create(entry);
	rv = handle->handle_precache(r);
	handle->handle_release();
	return rv;
}

void metadb::precache(const char * filename,reader * r)
{
	if (r)
	{
		metadb_handle_list temp;
		track_indexer::g_get_tracks_ex(filename,temp,r);
		unsigned n,m=temp.get_count();
		for(n=0;n<m;n++)
		{
			metadb_handle * ptr = temp[n];
			if (!stricmp_utf8(filename,ptr->handle_get_path()))
			{
				r->seek(0);
				ptr->handle_precache(r);
			}
		}
		temp.delete_all();
	}
}


void metadb::hint(const file_info * src)
{
	metadb_handle * handle = handle_create_hint(src);
	handle->handle_release();
}


int metadb::format_title(const playable_location * source,string_base & out,const char * spec,const char * extra_items)
{
	int rv;
	metadb_handle * handle = handle_create(source);
	rv = handle->handle_format_title(out,spec,extra_items);
	handle->handle_release();
	return rv;
}


metadb * metadb::get()//safe not to release it; don't call from DLL startup / static object constructor
{
	static metadb * ptr;
	if (ptr==0) ptr = service_enum_create_t(metadb,0);
	return ptr;
}
#ifdef _DEBUG
int metadb::database_lock_count()
{
	int val = database_try_lock();
	if (val)
		val = database_unlock();
	return val;
}

void metadb::database_delockify(int wanted_count)
{
	assert(wanted_count >= 0);
	if (wanted_count<0) wanted_count = 0;
	int count = database_lock_count();
	while(count < wanted_count) count = database_lock();
	while(count > wanted_count) count = database_unlock();
}
#endif