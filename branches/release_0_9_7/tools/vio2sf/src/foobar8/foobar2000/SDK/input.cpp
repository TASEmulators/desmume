#include "foobar2000.h"
#include <math.h>

input* input::g_open(reader * r,file_info * info,unsigned flags,__int64 * modtime,__int64 * filesize,bool * new_info)
{
	TRACK_CALL_TEXT("input::g_open");
	
	service_enum_t<input> e;
	input * i;
	const char * filename = info->get_file_path();

	if (r)//check mime types first
	{
		string8 content_type;
		if (r->get_content_type(content_type))
		{
			for(i=e.first();i;i = e.next())
			{
				if (i->needs_reader() && i->is_our_content_type(filename,content_type))
				{
					if (i->open_ex(r,info,flags,modtime,filesize,new_info)) return i;
					if (!r->seek(0)) {i->service_release(); return 0;}
				}
				i->service_release();
			}
		}
	}

	string_extension_8 extension(filename);
	for(i=e.first();i;i = e.next())
	{
		if (!!r == !!i->needs_reader())
		{
			if (i->test_filename(filename,extension))
			{
				if (i->open_ex(r,info,flags,modtime,filesize,new_info)) return i;
				if (r)
				{
					if (!r->seek(0)) {i->service_release();return 0;}
				}
			}
		}
		i->service_release();
	}
	return 0;
}

static bool g_get_info_internal(unsigned flags,file_info * info,reader * r,__int64 * modtime,__int64 * filesize,bool * b_new_info)
{
	//return 0 on failure, 1 on success
	bool my_reader = false;
	const char * filename = info->get_file_path();
	string_extension_8 extension(filename);
	bool rv = false;

	if (r==0)	//try readerless inputs first
	{
		service_enum_t<input> e;
		input * i;
		for(i=e.first();i;i = e.next())
		{
			rv = false;
			if (!i->needs_reader())
			{
				if (i->test_filename(filename,extension))
					rv = i->open_ex(0,info,flags,modtime,filesize,b_new_info);
			}
			i->service_release();
			if (rv) return true;
		}

		r = file::g_open(info->get_file_path(),reader::MODE_READ);
		if (r==0)
		{
			return false;
		}
		my_reader = true;
	}

	service_enum_t<input> e;
	input * i;

	{//check content type
		string8 content_type;
		if (r->get_content_type(content_type))
		{
			for(i=e.first();i;i = e.next())
			{
				rv = false;
				if (i->needs_reader() && i->is_our_content_type(info->get_file_path(),content_type))
				{
					if (!r->seek(0))
					{
						i->service_release();
						if (my_reader) r->reader_release();
						return false;
					}
					rv = i->open_ex(r,info,flags,modtime,filesize,b_new_info);
				}
				i->service_release();
				if (rv) break;
			}
		}
	}


	if (!rv)//if no luck with content type, proceed with classic open method
	{
		for(i=e.first();i;i = e.next())
		{
			rv = false;
			if (i->needs_reader())
			{
				if (!r->seek(0))
				{
					i->service_release();
					if (my_reader) r->reader_release();
					return false;
				}
				if (i->test_filename(filename,extension))
					rv= i->open_ex(r,info,flags,modtime,filesize,b_new_info);
			}
			i->service_release();
			if (rv) break;
		}
	}

	if (my_reader) r->reader_release();

	return rv;
}

bool input::g_check_info(file_info * info,reader * r,__int64 * modtime,__int64 * filesize)
{
	if (modtime==0) return false;
	__int64 time = *modtime;
	__int64 size = -1;
	bool new_info = false;
	if (!g_get_info_internal(0,info,r,&time,&size,&new_info)) return false;
	if (new_info)
	{
		*modtime = time;
		*filesize = size;
		return true;
	}
	return false;
}

bool input::g_get_info(file_info * info,reader * r,__int64 * modtime,__int64 * filesize)
{
	return g_get_info_internal(OPEN_FLAG_GET_INFO,info,r,modtime,filesize,0);
}

bool input::g_test_filename(const char * filename)
{
	service_enum_t<input> e;
	input * i;
	string_extension_8 extension(filename);
	for(i=e.first();i;i = e.next())
	{
		bool rv = false;
		rv = i->test_filename(filename,extension);
		i->service_release();
		if (rv) return true;
	}
	return false;
}

int input_pcm::run(audio_chunk * chunk)
{
	int bps,srate,nch;
	void * source;
	int size;
	int rv = get_samples_pcm(&source,&size,&srate,&bps,&nch);
	if (rv>0) chunk->set_data_fixedpoint(source,size,srate,nch,bps);
	return rv;
}

input::set_info_t input::g_set_info_readerless(const file_info * info,__int64 * modtime,__int64 * filesize)
{
	set_info_t rv = SET_INFO_FAILURE;
	const char * filename = info->get_file_path();
	string_extension_8 extension(filename);
	service_enum_t<input> e;
	input * i;
	for(i=e.first();i;i = e.next())
	{
		rv = SET_INFO_FAILURE;
		if (!i->needs_reader())
		{
			if (i->test_filename(filename,extension))
				rv = i->set_info_ex(0,info,modtime,filesize);
		}
		i->service_release();
		if (rv != SET_INFO_FAILURE) return rv;
	}
	return SET_INFO_FAILURE;
}

input::set_info_t input::g_set_info_reader(const file_info * info,reader * r,__int64 * modtime,__int64 * filesize)
{
	const char * filename = info->get_file_path();
	string_extension_8 extension(filename);
	set_info_t rv = SET_INFO_FAILURE;

	service_enum_t<input> e;
	input * i;
	bool got_busy = false;
	for(i=e.first();i;i = e.next())
	{
		rv = SET_INFO_FAILURE;
		if (i->needs_reader())
		{
			r->seek(0);
			if (i->test_filename(filename,extension))
				rv = i->set_info_ex(r,info,modtime,filesize);
		}
		i->service_release();
		if (rv==SET_INFO_SUCCESS) break;
		else if (rv==SET_INFO_BUSY) got_busy = true;
	}

	return rv == SET_INFO_SUCCESS ? SET_INFO_SUCCESS : got_busy ? SET_INFO_BUSY : SET_INFO_FAILURE;
}


input::set_info_t input::g_set_info(const file_info * info,reader * r,__int64 * modtime,__int64 * filesize)
{
	bool my_reader = false;
	set_info_t rv = SET_INFO_FAILURE;

	if (r==0)
	{
		rv = g_set_info_readerless(info,modtime,filesize);
		if (rv != SET_INFO_FAILURE) return rv;

		r = file::g_open(info->get_file_path(),reader::MODE_WRITE_EXISTING);
		if (r==0)
		{
			int flags = file::g_exists(info->get_file_path());
			if (flags & file::FILE_EXISTS_WRITEABLE) return SET_INFO_BUSY;
			else return SET_INFO_FAILURE;
		}
		my_reader = true;
	}


	rv = g_set_info_reader(info,r,modtime,filesize);

	if (my_reader) r->reader_release();

	return rv;
}


bool input::is_entry_dead(const playable_location * entry)
{
	const char * path = entry->get_path();
	if (file::g_dont_read_infos(path)) return false;
	int exists = file::g_exists(path);
	if (exists)
	{
		return !g_test_filename(path);
	}
	else
	{
		//special fix to keep bloody flac tag updating hack from breaking things
		//allow non-existent files ONLY when some readerless input accepts them and no reader-based inputs accept them
		string_extension_8 extension(path);
		service_enum_t<input> e;
		input * i;
		bool found_reader = false, found_readerless = false;
		for(i=e.first();i;i = e.next())
		{
			if (i->test_filename(path,extension))
			{
				if (i->needs_reader())
					found_reader = true;
				else
					found_readerless = true;
			}
			i->service_release();
		}

		return ! ( found_readerless && !found_reader );
	}
}

bool input_test_filename_helper::test_filename(const char * filename)
{
	if (!inited)
	{
		service_enum_t<input> e;
		input * i;
		for(i=e.first();i;i = e.next())
			inputs.add_item(i);
		inited=true;
	}

	string_extension_8 extension(filename);

	int n,m=inputs.get_count();
	for(n=0;n<m;n++)
	{
		if (inputs[n]->test_filename(filename,extension)) return true;
	}
	return false;
}

input_test_filename_helper::~input_test_filename_helper()
{
	unsigned n;
	for(n=0;n<inputs.get_count();n++)
		inputs[n]->service_release();
	inputs.remove_all();
}

void input_file_type::build_openfile_mask(string_base & out, bool b_include_playlists)
{
	string8_fastalloc name,mask,mask_alltypes,out_temp;
	
	if (b_include_playlists)
	{
		playlist_loader * ptr;
		service_enum_t<playlist_loader> e;
		for(ptr=e.first();ptr;ptr=e.next())
		{
			if (!mask.is_empty()) mask += ";";
			mask += "*.";
			mask += ptr->get_extension();
		}
		out_temp += "Playlists";
		out_temp += "|";
		out_temp += mask;
		out_temp += "|";

		if (!mask_alltypes.is_empty())
		{
			if (mask_alltypes[mask_alltypes.length()-1]!=';')
				mask_alltypes += ";";
		}
		mask_alltypes += mask;
	}

	{
		input_file_type * ptr;
		service_enum_t<input_file_type> e;
		for(ptr=e.first();ptr;ptr=e.next())
		{
			unsigned n,m = ptr->get_count();
			for(n=0;n<m;n++)
			{
				name.reset();
				mask.reset();
				if (ptr->get_name(n,name) && ptr->get_mask(n,mask))
				{
					if (!strchr(name,'|') && !strchr(mask,'|'))
					{
						out_temp += name;
						out_temp += "|";
						out_temp += mask;
						out_temp += "|";
						if (!mask_alltypes.is_empty())
						{
							if (mask_alltypes[mask_alltypes.length()-1]!=';')
								mask_alltypes += ";";
						}
						mask_alltypes += mask;
					}
				}
			}
			ptr->service_release();
		}
	}
	out.reset();
	out += "All files|*.*|";
	if (!mask_alltypes.is_empty())
	{
		out += "All supported types|";
		out += mask_alltypes;
		out += "|";
	}
	out += out_temp;
}

static bool open_ex_internal(input * i,reader * r,file_info * info,unsigned flags,__int64 * modtime,__int64 * filesize,bool * b_new_info)
{
	if (!!r != !!i->needs_reader()) return false;
	if (r)
	{
		__int64 newmodtime;
		__int64 newfilesize;
		if (modtime)
		{
			newmodtime = r->get_modification_time();
			if (newmodtime != *modtime) flags |= input::OPEN_FLAG_GET_INFO;
		}
		if (filesize)
		{
			newfilesize = r->get_length();
			if (newfilesize != *filesize) flags |= input::OPEN_FLAG_GET_INFO;
		}

		if (!(flags&(input::OPEN_FLAG_GET_INFO|input::OPEN_FLAG_DECODE))) return true;

		if (!i->open(r,info,flags)) return false;

		if (b_new_info && (flags & input::OPEN_FLAG_GET_INFO)) *b_new_info = true;

		if (filesize) *filesize = r->get_length();
		if (modtime) *modtime = newmodtime;

		return true;
	}
	else
	{

		if (!i->open(0,info,flags)) return false;

		if (b_new_info && (flags & input::OPEN_FLAG_GET_INFO)) *b_new_info = true;

		if (filesize) *filesize = -1;
		if (modtime) *modtime = 0;
		
		

		return true;
	}
}

bool input::open_ex(reader * r,file_info * info,unsigned flags,__int64 * modtime,__int64 * filesize,bool * b_new_info)
{
	{
		input_v2 * this_v2 = service_query_t(input_v2,this);
		if (this_v2)
		{
			bool rv = this_v2->open_ex(r,info,flags,modtime,filesize,b_new_info);
			this_v2->service_release();
			return rv;
		}
	}

	return open_ex_internal(this,r,info,flags,modtime,filesize,b_new_info);
}

bool input_v2::open_ex(reader * r,file_info * info,unsigned flags,__int64 * modtime,__int64 * filesize,bool * b_new_info)
{//to be overridden if needed (possibly needed for weird readerless inputs), this implementation is the default behavior
	return open_ex_internal(this,r,info,flags,modtime,filesize,b_new_info);
}

static input::set_info_t set_info_ex_internal(input * i,reader *r,const file_info * info,__int64 * modtime,__int64 * filesize)
{
	input::set_info_t rv = i->set_info(r,info);
	if (r)
	{
		if (modtime) *modtime = r->get_modification_time();
		if (filesize) *filesize = r->get_length();
	}
	else
	{
		if (modtime) *modtime = 0;
		if (filesize) *filesize = -1;
	}
	return rv;
}

input::set_info_t input::set_info_ex(reader *r,const file_info * info,__int64 * modtime,__int64 * filesize)
{
	{
		input_v2 * this_v2 = service_query_t(input_v2,this);
		if (this_v2)
		{
			set_info_t rv = this_v2->set_info_ex(r,info,modtime,filesize);
			this_v2->service_release();
			return rv;
		}
	}

	return set_info_ex_internal(this,r,info,modtime,filesize);
}

input::set_info_t input_v2::set_info_ex(reader *r,const file_info * info,__int64 * modtime,__int64 * filesize)
{
	return set_info_ex_internal(this,r,info,modtime,filesize);
}

bool input::is_reopen_safe()
{
	bool rv = false;
	input_v2 * this_v2 = service_query_t(input_v2,this);
	if (this_v2)
	{
		rv = this_v2->is_reopen_safe();
		this_v2->service_release();
	}
	return rv;
}