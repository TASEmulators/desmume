#include "foobar2000.h"

static void process_path_internal(const char * p_path,const service_ptr_t<file> & p_reader,playlist_loader_callback_v2 & callback,playlist_loader_callback::t_entry_type type,const t_filestats & p_stats);

namespace {
	class archive_callback_impl : public archive_callback
	{
	public:
		archive_callback_impl(playlist_loader_callback_v2 & p_callback) : m_callback(p_callback) {}
		bool on_entry(archive * owner,const char * p_path,const t_filestats & p_stats,const service_ptr_t<file> & p_reader)
		{
			process_path_internal(p_path,p_reader,m_callback,playlist_loader_callback::entry_directory_enumerated,p_stats);
			return !m_callback.is_aborting();
		}
		bool is_aborting() const {return m_callback.is_aborting();}
		abort_callback_event get_abort_event() const {return m_callback.get_abort_event();}
	private:
		playlist_loader_callback_v2 & m_callback;
	};
}

void playlist_loader::g_load_playlist_filehint(file::ptr fileHint,const char * p_path,playlist_loader_callback & p_callback) {
	TRACK_CALL_TEXT("playlist_loader::g_load_playlist_filehint");
	pfc::string8 filepath;

	filesystem::g_get_canonical_path(p_path,filepath);
	
	pfc::string_extension extension(filepath);

	service_ptr_t<file> l_file = fileHint;

	if (l_file.is_empty()) {
		filesystem::ptr fs;
		if (filesystem::g_get_interface(fs,filepath)) {
			if (fs->supports_content_types()) {
				fs->open(l_file,filepath,filesystem::open_mode_read,p_callback);
			}
		}
	}

	{
		service_enum_t<playlist_loader> e;

		if (l_file.is_valid()) {
			pfc::string8 content_type;
			if (l_file->get_content_type(content_type)) {
				service_ptr_t<playlist_loader> l;
				e.reset(); while(e.next(l)) {
					if (l->is_our_content_type(content_type)) {
						try {
							TRACK_CODE("playlist_loader::open",l->open(filepath,l_file,p_callback));
							return;
						} catch(exception_io_unsupported_format) {
							l_file->reopen(p_callback);
						}
					}
				}
			}
		}

		if (extension.length()>0) {
			service_ptr_t<playlist_loader> l;
			e.reset(); while(e.next(l)) {
				if (stricmp_utf8(l->get_extension(),extension) == 0) {
					if (l_file.is_empty()) filesystem::g_open_read(l_file,filepath,p_callback);
					try {
						TRACK_CODE("playlist_loader::open",l->open(filepath,l_file,p_callback));
						return;
					} catch(exception_io_unsupported_format) {
						l_file->reopen(p_callback);
					}
				}
			}
		}
	}

	throw exception_io_unsupported_format();
}
void playlist_loader::g_load_playlist(const char * p_path,playlist_loader_callback & callback) {
	g_load_playlist_filehint(NULL,p_path,callback);
}

static void index_tracks_helper(const char * p_path,const service_ptr_t<file> & p_reader,const t_filestats & p_stats,playlist_loader_callback::t_entry_type p_type,playlist_loader_callback & p_callback,bool & p_got_input)
{
	TRACK_CALL_TEXT("index_tracks_helper");
	if (p_reader.is_empty() && filesystem::g_is_remote_safe(p_path))
	{
		TRACK_CALL_TEXT("remote");
		metadb_handle_ptr handle;
		p_callback.handle_create(handle,make_playable_location(p_path,0));
		p_got_input = true;
		p_callback.on_entry(handle,p_type,p_stats,true);
	} else {
		TRACK_CALL_TEXT("hintable");
		service_ptr_t<input_info_reader> instance;
		input_entry::g_open_for_info_read(instance,p_reader,p_path,p_callback);

		t_filestats stats = instance->get_file_stats(p_callback);

		t_uint32 subsong,subsong_count = instance->get_subsong_count();
		for(subsong=0;subsong<subsong_count;subsong++)
		{
			TRACK_CALL_TEXT("subsong-loop");
			p_callback.check();
			metadb_handle_ptr handle;
			t_uint32 index = instance->get_subsong(subsong);
			p_callback.handle_create(handle,make_playable_location(p_path,index));

			p_got_input = true;
			if (p_callback.want_info(handle,p_type,stats,true))
			{
				file_info_impl info;
				TRACK_CODE("get_info",instance->get_info(index,info,p_callback));
				p_callback.on_entry_info(handle,p_type,stats,info,true);
			}
			else
			{
				p_callback.on_entry(handle,p_type,stats,true);
			}
		}
	}
}


static void track_indexer__g_get_tracks_wrap(const char * p_path,const service_ptr_t<file> & p_reader,const t_filestats & p_stats,playlist_loader_callback::t_entry_type p_type,playlist_loader_callback & p_callback) {
	bool got_input = false;
	bool fail = false;
	try {
		index_tracks_helper(p_path,p_reader,p_stats,p_type,p_callback,got_input);
	} catch(exception_aborted) {
		throw;
	} catch(exception_io_unsupported_format) {
		fail = true;
	} catch(std::exception const & e) {
		fail = true;
		console::formatter() << "could not enumerate tracks (" << e << ") on:\n" << file_path_display(p_path);
	}
	if (fail) {
		if (!got_input && !p_callback.is_aborting()) {
			if (p_type == playlist_loader_callback::entry_user_requested)
			{
				metadb_handle_ptr handle;
				p_callback.handle_create(handle,make_playable_location(p_path,0));
				p_callback.on_entry(handle,p_type,p_stats,true);
			}
		}
	}
}


static void process_path_internal(const char * p_path,const service_ptr_t<file> & p_reader,playlist_loader_callback_v2 & p_callback,playlist_loader_callback::t_entry_type p_type,const t_filestats & p_stats)
{
	//p_path must be canonical

	p_callback.check();

	p_callback.on_progress(p_path);

	
	{
		if (p_reader.is_empty()) {
			directory_callback_impl directory_results(true);
			try {
				filesystem::g_list_directory(p_path,directory_results,p_callback);
				for(t_size n=0;n<directory_results.get_count();n++) {
					process_path_internal(directory_results.get_item(n),0,p_callback,playlist_loader_callback::entry_directory_enumerated,directory_results.get_item_stats(n));
				}
				return;
			} catch(exception_aborted) {throw;}
			catch(...) {
				//do nothing, fall thru
				//fixme - catch only filesystem exceptions?
			}
		}

		bool found = false;


		{
			archive_callback_impl archive_results(p_callback);
			service_enum_t<filesystem> e;
			service_ptr_t<filesystem> f;
			while(e.next(f)) {
				p_callback.check();
				service_ptr_t<archive> arch;
				if (f->service_query_t(arch)) {
					if (p_reader.is_valid()) p_reader->reopen(p_callback);

					try {
						TRACK_CODE("archive::archive_list",arch->archive_list(p_path,p_reader,archive_results,true));
						return;
					} catch(exception_aborted) {throw;} 
					catch(...) {}
				}
			} 
		}
	}

	

	{
		service_ptr_t<link_resolver> ptr;
		if (link_resolver::g_find(ptr,p_path))
		{
			if (p_reader.is_valid()) p_reader->reopen(p_callback);

			pfc::string8 temp;
			try {
				TRACK_CODE("link_resolver::resolve",ptr->resolve(p_reader,p_path,temp,p_callback));

				track_indexer__g_get_tracks_wrap(temp,0,filestats_invalid,playlist_loader_callback::entry_from_playlist,p_callback);
				return;//success
			} catch(exception_aborted) {throw;}
			catch(...) {}
		}
	}

	if (p_callback.is_path_wanted(p_path,p_type)) {
		track_indexer__g_get_tracks_wrap(p_path,p_reader,p_stats,p_type,p_callback);
	}
}

void playlist_loader::g_process_path(const char * p_filename,playlist_loader_callback_v2 & callback,playlist_loader_callback::t_entry_type type)
{
	TRACK_CALL_TEXT("playlist_loader::g_process_path");

	file_path_canonical filename(p_filename);

	process_path_internal(filename,0,callback,type,filestats_invalid);
}

void playlist_loader::g_save_playlist(const char * p_filename,const pfc::list_base_const_t<metadb_handle_ptr> & data,abort_callback & p_abort)
{
	TRACK_CALL_TEXT("playlist_loader::g_save_playlist");
	pfc::string8 filename;
	filesystem::g_get_canonical_path(p_filename,filename);
	try {
		service_ptr_t<file> r;
		filesystem::g_open(r,filename,filesystem::open_mode_write_new,p_abort);

		pfc::string_extension ext(filename);
		
		service_enum_t<playlist_loader> e;
		service_ptr_t<playlist_loader> l;
		if (e.first(l)) do {
			if (l->can_write() && !stricmp_utf8(ext,l->get_extension())) {
				try {
					TRACK_CODE("playlist_loader::write",l->write(filename,r,data,p_abort));
					return;
				} catch(exception_io_data) {}
			}
		} while(e.next(l));
		throw exception_io_data();
	} catch(...) {
		try {filesystem::g_remove(filename,p_abort);} catch(...) {}
		throw;
	}
}


bool playlist_loader::g_process_path_ex(const char * filename,playlist_loader_callback_v2 & callback,playlist_loader_callback::t_entry_type type)
{
	try {
		g_load_playlist(filename,callback);
		return true;
	} catch(exception_io_unsupported_format) {//not a playlist format
		g_process_path(filename,callback,type);
		return false;
	}
}
