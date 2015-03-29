#include "foobar2000.h"

int playlist_loader::load_playlist(const char * p_filename,playlist_loader_callback * callback)
{
	TRACK_CALL_TEXT("playlist_loader::load_playlist");
	string8 filename = file_path_canonical(p_filename);
	service_enum_t<playlist_loader> e;
	playlist_loader * l;

	string_extension_8 extension(filename);

	bool found = 0;
	reader * r = 0;

	for(l=e.first();l && !found;l = e.next())
	{
		if (!stricmp_utf8(l->get_extension(),extension))
		{
			found = 1;
			if (r==0)
				r = file::g_open(filename,reader::MODE_READ);
			if (r)
			{
				r->seek(0);
				int rv=l->open(filename,r,callback);
				if (!rv) found=0;
			}
		}
		l->service_release();
	}
	if (r) r->reader_release();
	return found ? 1 : 0;
}

void playlist_loader::process_path(const char * p_filename,playlist_loader_callback * callback,playlist_loader_callback::entry_type type)
{
	TRACK_CALL_TEXT("playlist_loader::process_path");
	file_path_canonical filename(p_filename);
	if (!callback->on_progress(0)) return;
	
	{
		directory_callback_i dir_list;
		if (directory::g_list(filename,&dir_list,callback))
		{
			unsigned n;
			for(n=0;n<dir_list.get_count();n++)
			{
				const char * param = dir_list[n];
				process_path(param,callback,playlist_loader_callback::DIRECTORY_ENUMERATED);
				if (!callback->on_progress(0)) break;
			}
			return;
		}

		bool found = false;


		service_enum_t<archive> e;
		archive * d;
		for(d=e.first();d;d=e.next())
		{
			int rv;
			rv = d->list(filename,&dir_list,callback);

			if (rv)
			{
				unsigned n;
				for(n=0;n<dir_list.get_count();n++)
				{
					const char * param = dir_list[n];
					process_path(param,callback,playlist_loader_callback::DIRECTORY_ENUMERATED);
					if (!callback->on_progress(0)) break;
				}

				if (!callback->on_progress(0)) break;
				d->precache(filename,callback);

				found = true;
			}
			d->service_release();
			if (found) break;

			if (!callback->on_progress(0)) break;
		}

		if (found || !callback->on_progress(0)) return;

	}

//	if (input::g_test_filename(filename))
	{
		track_indexer::g_get_tracks_wrap(filename,callback,type,0);
	}
}

int playlist_loader::save_playlist(const char * p_filename,const ptr_list_base<metadb_handle> & data)
{
	TRACK_CALL_TEXT("playlist_loader::save_playlist");
	string8 filename = file_path_canonical(p_filename);
	reader * r = file::g_open(filename,reader::MODE_WRITE_NEW);
	if (!r) return 0;
	int rv = 0;

	string_extension ext(filename);
	
	service_enum_t<playlist_loader> e;
	playlist_loader * l;
	for(l=e.first();l;l = e.next())
	{
		if (l->can_write() && !stricmp_utf8(ext,l->get_extension()) && l->write(filename,r,data))
		{
			rv=1;
			break;
		}
		l->service_release();
	}

	r->reader_release();
	
	if (!rv) file::g_remove(filename);

	return rv;
}

int track_indexer::g_get_tracks_callback(const char * filename,callback * out,reader * r)
{
	TRACK_CALL_TEXT("track_indexer::g_get_tracks_callback");
	service_enum_t<track_indexer> e;
	track_indexer * t;
	
	for(t=e.first();t;t = e.next())
	{
		int rv;
		rv = t->get_tracks(filename,out,r);
		t->service_release();
		if (rv>0) return rv;
	}
	out->on_entry(make_playable_location(filename,0));//assume single track if noone reports
	return 0;
}


int track_indexer::get_tracks_ex(const char * filename,ptr_list_base<metadb_handle> & out,reader * r)
{
	{
		track_indexer_v2 * this_v2 = service_query_t(track_indexer_v2,this);
		if (this_v2)
		{
			int rv = this_v2->get_tracks_ex(filename,out,r);
			this_v2->service_release();
			return rv;
		}
	}

	return get_tracks(filename,&callback_i_metadb(out),r);
}

int track_indexer::g_get_tracks_ex(const char * filename,ptr_list_base<metadb_handle> & out,reader * r)
{
	TRACK_CALL_TEXT("track_indexer::g_get_tracks_ex");
	service_enum_t<track_indexer> e;
	track_indexer * t;
	
	for(t=e.first();t;t = e.next())
	{
		int rv;
		rv = t->get_tracks_ex(filename,out,r);
		t->service_release();
		if (rv>0) return rv;
	}
	out.add_item(metadb::get()->handle_create(make_playable_location(filename,0)));//assume single track if noone reports
	return 0;
}


int track_indexer::g_get_tracks_wrap(const char * filename,playlist_loader_callback * out,playlist_loader_callback::entry_type type,reader * r)
{
	service_enum_t<track_indexer> e;
	track_indexer * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		int rv = 0;
		track_indexer_v2 * ptr_v2 = service_query_t(track_indexer_v2,ptr);
		if (ptr_v2)
		{
			metadb_handle_list temp;
			rv = ptr_v2->get_tracks_ex(filename,temp,r);
			ptr_v2->service_release();
			if (rv>0)
			{
				unsigned n, m = temp.get_count();
				for(n=0;n<m;n++) out->on_entry_dbhandle(temp[n],type);
			}
			temp.delete_all();
		}
		else
		{
			rv = ptr->get_tracks(filename,&callback_i_wrap(out,type),r);
		}
		ptr->service_release();
		if (rv>0) return rv;
	}
	
	out->on_entry(make_playable_location(filename,0),type);
	return 0;
		
}


bool playlist_loader::is_our_content_type(const char * param)
{
	bool rv = false;
	playlist_loader_v2 * this_v2 = service_query_t(playlist_loader_v2,this);
	if (this_v2)
	{
		rv = this_v2->is_our_content_type(param);
		this_v2->service_release();
	}
	return rv;
}