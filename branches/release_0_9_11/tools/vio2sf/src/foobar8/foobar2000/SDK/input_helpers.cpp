#include "foobar2000.h"

input_helper::input_helper()
{
	seek_to = -1;
	full_buffer_limit = 0;
	p_input=0;
	p_reader=0;
	reader_overridden = false;
	b_no_seek = false;
	b_no_loop = false;
	b_want_info = true;
	b_aborting = false;
}

input_helper::~input_helper()
{
	close();
}

bool input_helper::open_internal(file_info * info,reader * p_reader_override,__int64 *modification_time,__int64 * file_size,bool * new_info)
{
	TRACK_CALL_TEXT("input_helper::open_internal");

	info->meta_remove_all();
	info->info_remove_all();


	if (attempt_reopen_internal(info,p_reader_override,modification_time,file_size,new_info))
	{
		return true;
	}

	const char * path = info->get_file_path();//file_info always has canonic path
	
	sync.enter();
	
	close_input_internal();

	sync.leave();

		
	if (p_reader_override)
	{
		sync.enter();
		close_reader_internal();
		override_reader(p_reader_override);
		sync.leave();
	}
	else if (p_reader && !stricmp_utf8(my_location.get_path(),path) && p_reader->can_seek())
	{
		p_reader->seek(0);
	}
	else
	{
		sync.enter();
		close_reader_internal();
		sync.leave();
	}
	
	if (p_reader == 0)
	{
		input * p_input_temp = input::g_open(0,info,get_open_flags(),modification_time,file_size,new_info);
		if (p_input_temp)
		{
			sync.enter();
			p_input = p_input_temp;
			sync.leave();
			return true;
		}

		sync.enter();

		p_reader = file::g_get_reader(path);

		sync.leave();
		
		if (p_reader==0) return false;

		if (p_reader->open(path,reader::MODE_READ)==0) {close();return false;}

		setup_fullbuffer();
	}


	
	{
		input * temp = input::g_open(p_reader,info,get_open_flags(),modification_time,file_size,new_info);
		if (temp)
		{
			sync.enter();
			p_input = temp;
			sync.leave();
		}
		else {close();return false;}
	}

	my_location.copy(info->get_location());

	seek_to = -1;

	return true;
}

bool input_helper::open(file_info * info,reader * p_reader_override)
{
	b_want_info = true;
	return open_internal(info,p_reader_override,0,0,0);
}

bool input_helper::open_noinfo(file_info * info, reader * p_reader_override)
{
	b_want_info = false;
	return open_internal(info,p_reader_override,0,0,0);
}

bool input_helper::open(const playable_location * src,reader * p_reader_override)
{
	b_want_info = false;
	file_info_i_full info(src);
	return open_internal(&info,p_reader_override,0,0,0);
}

bool input_helper::open(metadb_handle * p_handle, reader * p_reader_override)
{
	bool l_want_info = !p_handle->handle_query(0,true);
	
	__int64 time = p_handle->handle_get_modification_time();
	__int64 newtime = time;
	__int64 file_size = p_handle->handle_get_file_size();
	bool b_new_info = false;

	b_want_info = l_want_info;
	file_info_i_full info(p_handle->handle_get_location());
	bool rv = open_internal(&info,p_reader_override,&newtime,&file_size,&b_new_info);
	if (rv)
	{
		if (b_new_info) p_handle->handle_hint_ex(&info,newtime,file_size,true);
	}
	else
	{
		if (input::is_entry_dead(p_handle->handle_get_location()))
			p_handle->handle_entry_dead();
	}
	return rv;
}

void input_helper::close_input_internal()
{
	if (p_input)
	{
		p_input->service_release();
		p_input = 0;
	}
}

void input_helper::close_reader_internal()
{
	if (p_reader)
	{
		if (!reader_overridden) p_reader->service_release();
		p_reader=0;
		reader_overridden = false;
	}
}


void input_helper::close()
{
	TRACK_CALL_TEXT("input_helper::close");
	insync(sync);
	close_input_internal();
	close_reader_internal();
}

bool input_helper::is_open() {return !!p_input;}

int input_helper::run(audio_chunk * chunk)//fucko: syncing this will break abort
{
	chunk->reset();

	if (p_input==0) return -1;

	if (seek_to>=0)
	{
		double target = seek_to;
		seek_to = -1;

		{
			TRACK_CALL_TEXT("input::seek");
			if (!p_input->seek(target)) return -1;
		}
	}

	int rv;

	{
		TRACK_CALL_TEXT("input::seek::run");
		rv = p_input->run(chunk);
	}

	if (rv>0 && !chunk->is_valid())
	{
		console::error("input::run() produced invalid chunk:");
		console::info_location(&my_location);
		chunk->reset();
		rv = -1;
	}

	return rv;
} 

bool input_helper::seek(double seconds)
{
	insync(sync);
	if (!p_input || b_no_seek) return false;
	seek_to = seconds;
	return true;
}

bool input_helper::can_seek()
{
	insync(sync);
	return p_input ? p_input->can_seek() : 0;
}

void input_helper::abort()
{
	TRACK_CALL_TEXT("input_helper::abort");
	insync(sync);
	b_aborting = true;
	if (p_reader) p_reader->abort();
	if (p_input) p_input->abort();
}

void input_helper::on_idle()
{
	insync(sync);
	if (p_reader) p_reader->on_idle();
}

bool input_helper::get_dynamic_info(file_info * out,double * timestamp_delta, bool * b_track_change)
{
	TRACK_CALL_TEXT("input_helper::get_dynamic_info");
	insync(sync);
	return p_input ? p_input->get_dynamic_info(out,timestamp_delta,b_track_change) : false;
}

bool input_helper::attempt_reopen_internal(file_info * info,reader * p_reader_override,__int64 * modification_time,__int64* file_size,bool * new_info)
{
	TRACK_CALL_TEXT("input_helper::attempt_reopen_internal");
	assert(sync.get_lock_count_check()==0);
	const char * path = info->get_file_path();

	bool b_reopen_available,b_same_file;
	sync.enter();
	b_same_file = p_input && !p_reader_override && my_location.compare(info->get_location())==0 && p_input->can_seek();
	if (!b_same_file)
		b_reopen_available = !(!p_input || (p_reader_override && !p_reader) || !p_input->is_reopen_safe() || !p_input->test_filename(path,string_extension(path)));
	sync.leave();
	if (b_same_file && !b_want_info)
	{
		return p_input->seek(0);
	}
	else if (!b_reopen_available) return false;
	
	if (p_reader_override)
	{//caller wants us to get their reader
		sync.enter();
		close_reader_internal();
		override_reader(p_reader_override);
		sync.leave();
	}
	else if (p_reader && !stricmp_utf8(my_location.get_path(),path) && p_reader->can_seek())
	{//same file
		p_reader->seek(0);
	}
	else if (p_reader)
	{//different file
		sync.enter();
		close_reader_internal();
		p_reader = file::g_get_reader(path);
		sync.leave();
		if (p_reader==0) return false;
		if (p_reader->open(path,reader::MODE_READ) == 0)
		{
			sync.enter();
			close_reader_internal();
			sync.leave();
			return false;
		}
		setup_fullbuffer();
	}
	//else must be readerless input

	if (p_input->open_ex(p_reader,info,get_open_flags(),modification_time,file_size,new_info))
	{
		my_location.copy(info->get_location());
		return true;
	}
	else return false;
}

void input_helper::override_reader(reader* p_reader_override)
{
	assert(sync.get_lock_count_check()>0);
	assert(p_reader == 0);
	p_reader = p_reader_override;
	reader_overridden = !!p_reader_override;
}

void input_helper::setup_fullbuffer()
{
	assert(p_reader);
	assert(sync.get_lock_count_check()==0);
	if (full_buffer_limit>0)
	{
		__int64 size = p_reader->get_length();
		if (size>0 && size<=full_buffer_limit)
		{
			reader * temp = reader_membuffer::create(p_reader,p_reader->get_modification_time());
			if (temp)
			{
				sync.enter();
				p_reader->service_release();
				p_reader = temp;
				sync.leave();
			}
		}
	}
}


unsigned input_helper::get_open_flags()
{
	return input::OPEN_FLAG_DECODE | (b_want_info ? input::OPEN_FLAG_GET_INFO : 0) | (b_no_seek ? input::OPEN_FLAG_NO_SEEKING : 0) | (b_no_loop ? input::OPEN_FLAG_NO_LOOPING : 0);
}

void input_helper::override(const playable_location * new_location,input * new_input,reader * new_reader)
{
	insync(sync);
	close();
	if (new_location) my_location.copy(new_location);
	else my_location.copy(make_playable_location("UNKNOWN",0));
	p_input = new_input;
	p_reader = new_reader;
	seek_to = -1;
}
