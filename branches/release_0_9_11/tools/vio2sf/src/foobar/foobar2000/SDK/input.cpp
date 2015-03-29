#include "foobar2000.h"


bool input_entry::g_find_service_by_path(service_ptr_t<input_entry> & p_out,const char * p_path)
{
	service_ptr_t<input_entry> ptr;
	service_enum_t<input_entry> e;
	pfc::string_extension ext(p_path);
	while(e.next(ptr))
	{
		if (ptr->is_our_path(p_path,ext))
		{
			p_out = ptr;
			return true;
		}
	}
	return false;
}

bool input_entry::g_find_service_by_content_type(service_ptr_t<input_entry> & p_out,const char * p_content_type)
{
	service_ptr_t<input_entry> ptr;
	service_enum_t<input_entry> e;
	while(e.next(ptr))
	{
		if (ptr->is_our_content_type(p_content_type))
		{
			p_out = ptr;
			return true;
		}
	}
	return false;
}



static void prepare_for_open(service_ptr_t<input_entry> & p_service,service_ptr_t<file> & p_file,const char * p_path,filesystem::t_open_mode p_open_mode,abort_callback & p_abort,bool p_from_redirect)
{
	if (p_file.is_empty())
	{
		service_ptr_t<filesystem> fs;
		if (filesystem::g_get_interface(fs,p_path)) {
			if (fs->supports_content_types()) {
				fs->open(p_file,p_path,p_open_mode,p_abort);
			}
		}
	}

	if (p_file.is_valid())
	{
		pfc::string8 content_type;
		if (p_file->get_content_type(content_type))
		{
			if (input_entry::g_find_service_by_content_type(p_service,content_type))
				return;
		}
	}

	if (input_entry::g_find_service_by_path(p_service,p_path))
	{
		if (p_from_redirect && p_service->is_redirect()) throw exception_io_unsupported_format();
		return;
	}

	throw exception_io_unsupported_format();
}

namespace {

	bool g_find_inputs_by_content_type(pfc::list_base_t<service_ptr_t<input_entry> > & p_out,const char * p_content_type,bool p_from_redirect) {
		service_enum_t<input_entry> e;
		service_ptr_t<input_entry> ptr;
		bool ret = false;
		while(e.next(ptr)) {
			if (!(p_from_redirect && ptr->is_redirect())) {
				if (ptr->is_our_content_type(p_content_type)) {p_out.add_item(ptr); ret = true;}
			}
		}
		return ret;
	}

	bool g_find_inputs_by_path(pfc::list_base_t<service_ptr_t<input_entry> > & p_out,const char * p_path,bool p_from_redirect) {
		service_enum_t<input_entry> e;
		service_ptr_t<input_entry> ptr;
		pfc::string_extension extension(p_path);
		bool ret = false;
		while(e.next(ptr)) {
			if (!(p_from_redirect && ptr->is_redirect())) {
				if (ptr->is_our_path(p_path,extension)) {p_out.add_item(ptr); ret = true;}
			}
		}
		return ret;
	}

	template<typename t_service> void g_open_from_list(service_ptr_t<t_service> & p_instance,pfc::list_base_const_t<service_ptr_t<input_entry> > const & p_list,service_ptr_t<file> const & p_filehint,const char * p_path,abort_callback & p_abort) {
		const t_size count = p_list.get_count();
		if (count == 1) {
			p_list[0]->open(p_instance,p_filehint,p_path,p_abort);
		} else {
			bool got_bad_data = false, got_bad_data_multi = false;
			bool done = false;
			pfc::string8 bad_data_message;
			for(t_size n=0;n<count && !done;n++) {
				try {
					p_list[n]->open(p_instance,p_filehint,p_path,p_abort);
					done = true;
				} catch(exception_io_unsupported_format) {
					//do nothing, skip over
				} catch(exception_io_data const & e) {
					if (!got_bad_data) bad_data_message = e.what();
					else got_bad_data_multi = true;
					got_bad_data = true;
				}
			}
			if (!done) {
				if (got_bad_data_multi) throw exception_io_data();
				else if (got_bad_data) throw exception_io_data(bad_data_message);
				else throw exception_io_unsupported_format();
			}
		}
	}

	template<typename t_service> bool needs_write_access() {return false;}
	template<> bool needs_write_access<input_info_writer>() {return true;}

	template<typename t_service> void g_open_t(service_ptr_t<t_service> & p_instance,service_ptr_t<file> const & p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect) {
		service_ptr_t<file> l_file = p_filehint;
		if (l_file.is_empty()) {
			service_ptr_t<filesystem> fs;
			if (filesystem::g_get_interface(fs,p_path)) {
				if (fs->supports_content_types()) {
					fs->open(l_file,p_path,needs_write_access<t_service>() ? filesystem::open_mode_write_existing : filesystem::open_mode_read,p_abort);
				}
			}
		}

		if (l_file.is_valid()) {
			pfc::string8 content_type;
			if (l_file->get_content_type(content_type)) {
				pfc::list_hybrid_t<service_ptr_t<input_entry>,4> list;
				if (g_find_inputs_by_content_type(list,content_type,p_from_redirect)) {
					g_open_from_list(p_instance,list,l_file,p_path,p_abort);
					return;
				}
			}
		}

		{
			pfc::list_hybrid_t<service_ptr_t<input_entry>,4> list;
			if (g_find_inputs_by_path(list,p_path,p_from_redirect)) {
				g_open_from_list(p_instance,list,l_file,p_path,p_abort);
				return;
			}
		}

		throw exception_io_unsupported_format();
	}
};

void input_entry::g_open_for_decoding(service_ptr_t<input_decoder> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect) {
	TRACK_CALL_TEXT("input_entry::g_open_for_decoding");
#if 1
	g_open_t(p_instance,p_filehint,p_path,p_abort,p_from_redirect);
#else
	service_ptr_t<file> filehint = p_filehint;
	service_ptr_t<input_entry> entry;

	prepare_for_open(entry,filehint,p_path,filesystem::open_mode_read,p_abort,p_from_redirect);

	entry->open_for_decoding(p_instance,filehint,p_path,p_abort);
#endif

}

void input_entry::g_open_for_info_read(service_ptr_t<input_info_reader> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect) {
	TRACK_CALL_TEXT("input_entry::g_open_for_info_read");
#if 1
	g_open_t(p_instance,p_filehint,p_path,p_abort,p_from_redirect);
#else
	service_ptr_t<file> filehint = p_filehint;
	service_ptr_t<input_entry> entry;

	prepare_for_open(entry,filehint,p_path,filesystem::open_mode_read,p_abort,p_from_redirect);

	entry->open_for_info_read(p_instance,filehint,p_path,p_abort);
#endif
}

void input_entry::g_open_for_info_write(service_ptr_t<input_info_writer> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,bool p_from_redirect) {
	TRACK_CALL_TEXT("input_entry::g_open_for_info_write");
#if 1
	g_open_t(p_instance,p_filehint,p_path,p_abort,p_from_redirect);
#else
	service_ptr_t<file> filehint = p_filehint;
	service_ptr_t<input_entry> entry;

	prepare_for_open(entry,filehint,p_path,filesystem::open_mode_write_existing,p_abort,p_from_redirect);

	entry->open_for_info_write(p_instance,filehint,p_path,p_abort);
#endif
}

void input_entry::g_open_for_info_write_timeout(service_ptr_t<input_info_writer> & p_instance,service_ptr_t<file> p_filehint,const char * p_path,abort_callback & p_abort,double p_timeout,bool p_from_redirect) {
	pfc::lores_timer timer;
	timer.start();
	for(;;) {
		try {
			g_open_for_info_write(p_instance,p_filehint,p_path,p_abort,p_from_redirect);
			break;
		} catch(exception_io_sharing_violation) {
			if (timer.query() > p_timeout) throw;
			p_abort.sleep(0.01);
		}
	}
}

bool input_entry::g_is_supported_path(const char * p_path)
{
	service_ptr_t<input_entry> ptr;
	service_enum_t<input_entry> e;
	pfc::string_extension ext(p_path);
	while(e.next(ptr))
	{
		if (ptr->is_our_path(p_path,ext)) return true;
	}
	return false;
}



void input_open_file_helper(service_ptr_t<file> & p_file,const char * p_path,t_input_open_reason p_reason,abort_callback & p_abort)
{
	if (p_file.is_empty()) {
		switch(p_reason) {
		default:
			throw pfc::exception_bug_check_v2();
		case input_open_info_read:
		case input_open_decode:
			filesystem::g_open(p_file,p_path,filesystem::open_mode_read,p_abort);
			break;
		case input_open_info_write:
			filesystem::g_open(p_file,p_path,filesystem::open_mode_write_existing,p_abort);
			break;
		}
	} else {
		p_file->reopen(p_abort);
	}
}