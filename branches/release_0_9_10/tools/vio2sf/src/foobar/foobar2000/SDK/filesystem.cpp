#include "foobar2000.h"


void unpacker::g_open(service_ptr_t<file> & p_out,const service_ptr_t<file> & p,abort_callback & p_abort)
{
	service_enum_t<unpacker> e;
	service_ptr_t<unpacker> ptr;
	if (e.first(ptr)) do {
		p->reopen(p_abort);
		try {
			ptr->open(p_out,p,p_abort);
			return;
		} catch(exception_io_data const &) {}
	} while(e.next(ptr));
	throw exception_io_data();
}

void file::seek_ex(t_sfilesize p_position, file::t_seek_mode p_mode, abort_callback &p_abort) {
	switch(p_mode) {
	case seek_from_beginning:
		seek(p_position,p_abort);
		break;
	case seek_from_current:
		seek(p_position + get_position(p_abort),p_abort);
		break;
	case seek_from_eof:
		seek(p_position + get_size_ex(p_abort),p_abort);
		break;
	default:
		throw exception_io_data();
	}
}

t_filesize file::g_transfer(stream_reader * p_src,stream_writer * p_dst,t_filesize p_bytes,abort_callback & p_abort) {
	enum {BUFSIZE = 1024*1024*8};
	pfc::array_t<t_uint8> temp;
	temp.set_size((t_size)pfc::min_t<t_filesize>(BUFSIZE,p_bytes));
	void* ptr = temp.get_ptr();
	t_filesize done = 0;
	while(done<p_bytes) {
		p_abort.check_e();
		t_size delta = (t_size)pfc::min_t<t_filesize>(BUFSIZE,p_bytes-done);
		delta = p_src->read(ptr,delta,p_abort);
		if (delta<=0) break;
		p_dst->write(ptr,delta,p_abort);
		done += delta;
	}
	return done;
}

void file::g_transfer_object(stream_reader * p_src,stream_writer * p_dst,t_filesize p_bytes,abort_callback & p_abort) {
	if (g_transfer(p_src,p_dst,p_bytes,p_abort) != p_bytes)
		throw exception_io_data_truncation();
}


void filesystem::g_get_canonical_path(const char * path,pfc::string_base & out)
{
	TRACK_CALL_TEXT("filesystem::g_get_canonical_path");

	service_enum_t<filesystem> e;
	service_ptr_t<filesystem> ptr;
	if (e.first(ptr)) do {
		if (ptr->get_canonical_path(path,out)) return;
	} while(e.next(ptr));
	//no one wants to process this, let's copy over
	out = path;
}

void filesystem::g_get_display_path(const char * path,pfc::string_base & out)
{
	TRACK_CALL_TEXT("filesystem::g_get_display_path");
	service_ptr_t<filesystem> ptr;
	if (!g_get_interface(ptr,path))
	{
		//no one wants to process this, let's copy over
		out = path;
	}
	else
	{
		if (!ptr->get_display_path(path,out))
			out = path;
	}
}

bool filesystem::g_get_interface(service_ptr_t<filesystem> & p_out,const char * path)
{
	service_enum_t<filesystem> e;
	service_ptr_t<filesystem> ptr;
	if (e.first(ptr)) do {
		if (ptr->is_our_path(path))
		{
			p_out = ptr;
			return true;
		}
	} while(e.next(ptr));
	return false;
}


void filesystem::g_open(service_ptr_t<file> & p_out,const char * path,t_open_mode mode,abort_callback & p_abort)
{
	TRACK_CALL_TEXT("filesystem::g_open");
	service_ptr_t<filesystem> fs;
	if (!g_get_interface(fs,path)) throw exception_io_no_handler_for_path();
	fs->open(p_out,path,mode,p_abort);
}

void filesystem::g_open_timeout(service_ptr_t<file> & p_out,const char * p_path,t_open_mode p_mode,double p_timeout,abort_callback & p_abort) {
	pfc::lores_timer timer;
	timer.start();
	for(;;) {
		try {
			g_open(p_out,p_path,p_mode,p_abort);
			break;
		} catch(exception_io_sharing_violation) {
			if (timer.query() > p_timeout) throw;
			p_abort.sleep(0.01);
		}
	}
}

bool filesystem::g_exists(const char * p_path,abort_callback & p_abort)
{
	t_filestats stats;
	bool dummy;
	try {
		g_get_stats(p_path,stats,dummy,p_abort);
	} catch(exception_io_not_found) {return false;}
	return true;
}

bool filesystem::g_exists_writeable(const char * p_path,abort_callback & p_abort)
{
	t_filestats stats;
	bool writeable;
	try {
		g_get_stats(p_path,stats,writeable,p_abort);
	} catch(exception_io_not_found) {return false;}
	return writeable;
}

void filesystem::g_remove(const char * p_path,abort_callback & p_abort) {
	service_ptr_t<filesystem> fs;
	if (!g_get_interface(fs,p_path)) throw exception_io_no_handler_for_path();
	fs->remove(p_path,p_abort);
}

void filesystem::g_remove_timeout(const char * p_path,double p_timeout,abort_callback & p_abort) {
	pfc::lores_timer timer;
	timer.start();
	for(;;) {
		try {
			g_remove(p_path,p_abort);
			break;
		} catch(exception_io_sharing_violation) {
			if (timer.query() > p_timeout) throw;
			p_abort.sleep(0.01);
		}
	}
}

void filesystem::g_move_timeout(const char * p_src,const char * p_dst,double p_timeout,abort_callback & p_abort) {
	pfc::lores_timer timer;
	timer.start();
	for(;;) {
		try {
			g_move(p_src,p_dst,p_abort);
			break;
		} catch(exception_io_sharing_violation) {
			if (timer.query() > p_timeout) throw;
			p_abort.sleep(0.01);
		}
	}
}

void filesystem::g_copy_timeout(const char * p_src,const char * p_dst,double p_timeout,abort_callback & p_abort) {
	pfc::lores_timer timer;
	timer.start();
	for(;;) {
		try {
			g_copy(p_src,p_dst,p_abort);
			break;
		} catch(exception_io_sharing_violation) {
			if (timer.query() > p_timeout) throw;
			p_abort.sleep(0.01);
		}
	}
}

void filesystem::g_create_directory(const char * p_path,abort_callback & p_abort)
{
	service_ptr_t<filesystem> fs;
	if (!g_get_interface(fs,p_path)) throw exception_io_no_handler_for_path();
	fs->create_directory(p_path,p_abort);
}

void filesystem::g_move(const char * src,const char * dst,abort_callback & p_abort) {
	service_enum_t<filesystem> e;
	service_ptr_t<filesystem> ptr;
	if (e.first(ptr)) do {
		if (ptr->is_our_path(src) && ptr->is_our_path(dst)) {
			ptr->move(src,dst,p_abort);
			return;
		}
	} while(e.next(ptr));
	throw exception_io_no_handler_for_path();
}

void filesystem::g_list_directory(const char * p_path,directory_callback & p_out,abort_callback & p_abort)
{
	TRACK_CALL_TEXT("filesystem::g_list_directory");
	service_ptr_t<filesystem> ptr;
	if (!g_get_interface(ptr,p_path)) throw exception_io_no_handler_for_path();
	ptr->list_directory(p_path,p_out,p_abort);
}


static void path_pack_string(pfc::string_base & out,const char * src)
{
	out.add_char('|');
	out << strlen(src);
	out.add_char('|');
	out << src;
	out.add_char('|');
}

static int path_unpack_string(pfc::string8 & out,const char * src)
{
	int ptr=0;
	if (src[ptr++]!='|') return -1;
	int len = atoi(src+ptr);
	if (len<=0) return -1;
	while(src[ptr]!=0 && src[ptr]!='|') ptr++;
	if (src[ptr]!='|') return -1;
	ptr++;
	int start = ptr;
	while(ptr-start<len)
	{
		if (src[ptr]==0) return -1;
		ptr++;
	}
	if (src[ptr]!='|') return -1;
	out.add_string(&src[start],len);
	ptr++;	
	return ptr;
}


void filesystem::g_open_precache(service_ptr_t<file> & p_out,const char * p_path,abort_callback & p_abort) {
	service_ptr_t<filesystem> fs;
	if (!g_get_interface(fs,p_path)) throw exception_io_no_handler_for_path();
	if (fs->is_remote(p_path)) throw exception_io_object_is_remote();
	fs->open(p_out,p_path,open_mode_read,p_abort);
}

bool filesystem::g_is_remote(const char * p_path) {
	service_ptr_t<filesystem> fs;
	if (g_get_interface(fs,p_path)) return fs->is_remote(p_path);
	else throw exception_io_no_handler_for_path();
}

bool filesystem::g_is_recognized_and_remote(const char * p_path) {
	service_ptr_t<filesystem> fs;
	if (g_get_interface(fs,p_path)) return fs->is_remote(p_path);
	else return false;
}

bool filesystem::g_is_remote_or_unrecognized(const char * p_path) {
	service_ptr_t<filesystem> fs;
	if (g_get_interface(fs,p_path)) return fs->is_remote(p_path);
	else return true;
}

bool filesystem::g_relative_path_create(const char * file_path,const char * playlist_path,pfc::string_base & out)
{
	
	bool rv = false;
	service_ptr_t<filesystem> fs;

	if (g_get_interface(fs,file_path))
		rv = fs->relative_path_create(file_path,playlist_path,out);
	
	return rv;
}

bool filesystem::g_relative_path_parse(const char * relative_path,const char * playlist_path,pfc::string_base & out)
{
	service_enum_t<filesystem> e;
	service_ptr_t<filesystem> ptr;
	if (e.first(ptr)) do {
		if (ptr->relative_path_parse(relative_path,playlist_path,out)) return true;
	} while(e.next(ptr));
	return false;
}



bool archive_impl::get_canonical_path(const char * path,pfc::string_base & out)
{
	if (is_our_path(path))
	{
		pfc::string8 archive,file,archive_canonical;
		if (g_parse_unpack_path(path,archive,file))
		{
			g_get_canonical_path(archive,archive_canonical);
			make_unpack_path(out,archive_canonical,file);

			return true;
		}
		else return false;
	}
	else return false;
}

bool archive_impl::is_our_path(const char * path)
{
	if (strncmp(path,"unpack://",9)) return false;
	const char * type = get_archive_type();
	path += 9;
	while(*type)
	{
		if (*type!=*path) return false;
		type++;
		path++;
	}
	if (*path!='|') return false;
	return true;
}

bool archive_impl::get_display_path(const char * path,pfc::string_base & out)
{
	pfc::string8 archive,file;
	if (g_parse_unpack_path(path,archive,file))
	{
		g_get_display_path(archive,out);
		out.add_string("|");
		out.add_string(file);
		return true;
	}
	else return false;
}

void archive_impl::open(service_ptr_t<file> & p_out,const char * path,t_open_mode mode, abort_callback & p_abort)
{
	if (mode != open_mode_read) throw exception_io_denied();
	pfc::string8 archive,file;
	if (!g_parse_unpack_path(path,archive,file)) throw exception_io_not_found();
	open_archive(p_out,archive,file,p_abort);
}


void archive_impl::remove(const char * path,abort_callback & p_abort) {
	throw exception_io_denied();
}

void archive_impl::move(const char * src,const char * dst,abort_callback & p_abort) {
	throw exception_io_denied();
}

bool archive_impl::is_remote(const char * src) {
	pfc::string8 archive,file;
	if (g_parse_unpack_path(src,archive,file)) return g_is_remote(archive);
	else throw exception_io_not_found();
}

bool archive_impl::relative_path_create(const char * file_path,const char * playlist_path,pfc::string_base & out) {
	pfc::string8 archive,file;
	if (g_parse_unpack_path(file_path,archive,file))
	{
		pfc::string8 archive_rel;
		if (g_relative_path_create(archive,playlist_path,archive_rel))
		{
			pfc::string8 out_path;
			make_unpack_path(out_path,archive_rel,file);
			out.set_string(out_path);
			return true;
		}
	}
	return false;
}

bool archive_impl::relative_path_parse(const char * relative_path,const char * playlist_path,pfc::string_base & out)
{
	if (!is_our_path(relative_path)) return false;
	pfc::string8 archive_rel,file;
	if (g_parse_unpack_path(relative_path,archive_rel,file))
	{
		pfc::string8 archive;
		if (g_relative_path_parse(archive_rel,playlist_path,archive))
		{
			pfc::string8 out_path;
			make_unpack_path(out_path,archive,file);
			out.set_string(out_path);
			return true;
		}
	}
	return false;
}

bool archive_impl::g_parse_unpack_path(const char * path,pfc::string8 & archive,pfc::string8 & file)
{
	path  = strchr(path,'|');
	if (!path) return false;
	int delta = path_unpack_string(archive,path);
	if (delta<0) return false;
	path += delta;
	file = path;
	return true;
}

void archive_impl::g_make_unpack_path(pfc::string_base & path,const char * archive,const char * file,const char * name)
{
	path = "unpack://";
	path += name;
	path_pack_string(path,archive);
	path += file;
}

void archive_impl::make_unpack_path(pfc::string_base & path,const char * archive,const char * file) {g_make_unpack_path(path,archive,file,get_archive_type());}


FILE * filesystem::streamio_open(const char * path,const char * flags)
{
	FILE * ret = 0;
	pfc::string8 temp;
	g_get_canonical_path(path,temp);
	if (!strncmp(temp,"file://",7))
	{
		ret = _wfopen(pfc::stringcvt::string_wide_from_utf8(path+7),pfc::stringcvt::string_wide_from_utf8(flags));
	}
	return ret;
}


namespace {

	class directory_callback_isempty : public directory_callback
	{
		bool m_isempty;
	public:
		directory_callback_isempty() : m_isempty(true) {}
		bool on_entry(filesystem * owner,abort_callback & p_abort,const char * url,bool is_subdirectory,const t_filestats & p_stats)
		{
			m_isempty = false;
			return false;
		}
		bool isempty() {return m_isempty;}
	};

	class directory_callback_dummy : public directory_callback
	{
	public:
		bool on_entry(filesystem * owner,abort_callback & p_abort,const char * url,bool is_subdirectory,const t_filestats & p_stats) {return false;}
	};

}

bool filesystem::g_is_empty_directory(const char * path,abort_callback & p_abort)
{
	directory_callback_isempty callback;
	try {
		g_list_directory(path,callback,p_abort);
	} catch(exception_io const &) {return false;}
	return callback.isempty();
}

bool filesystem::g_is_valid_directory(const char * path,abort_callback & p_abort) {
	try {
		g_list_directory(path,directory_callback_dummy(),p_abort);
		return true;
	} catch(exception_io const &) {return false;}
}

bool directory_callback_impl::on_entry(filesystem * owner,abort_callback & p_abort,const char * url,bool is_subdirectory,const t_filestats & p_stats) {
	p_abort.check_e();
	if (is_subdirectory) {
		if (m_recur) {
			try {
				owner->list_directory(url,*this,p_abort);
			} catch(exception_io const &) {}
		}
	} else {
		m_data.add_item(pfc::rcnew_t<t_entry>(url,p_stats));
	}
	return true;
}

namespace {
	class directory_callback_impl_copy : public directory_callback
	{
	public:
		directory_callback_impl_copy(const char * p_target)
		{
			m_target = p_target;
			m_target.fix_dir_separator('\\');
		}

		bool on_entry(filesystem * owner,abort_callback & p_abort,const char * url,bool is_subdirectory,const t_filestats & p_stats) {
			const char * fn = url + pfc::scan_filename(url);
			t_size truncat = m_target.length();
			m_target += fn;
			if (is_subdirectory) {
				try {
					filesystem::g_create_directory(m_target,p_abort);
				} catch(exception_io_already_exists) {}
				m_target += "\\";
				owner->list_directory(url,*this,p_abort);
			} else {
				filesystem::g_copy(url,m_target,p_abort);
			}
			m_target.truncate(truncat);
			return true;
		}
	private:
		pfc::string8_fastalloc m_target;
	};
}

void filesystem::g_copy_directory(const char * src,const char * dst,abort_callback & p_abort) {
	//UNTESTED
	filesystem::g_list_directory(src,directory_callback_impl_copy(dst),p_abort);
}

void filesystem::g_copy(const char * src,const char * dst,abort_callback & p_abort) {
	service_ptr_t<file> r_src,r_dst;
	t_filesize size;

	g_open(r_src,src,open_mode_read,p_abort);
	size = r_src->get_size_ex(p_abort);
	g_open(r_dst,dst,open_mode_write_new,p_abort);
	
	if (size > 0) {
		try {
			file::g_transfer_object(r_src,r_dst,size,p_abort);
		} catch(...) {
			r_dst.release();
			try {g_remove(dst,abort_callback_impl());} catch(...) {}
			throw;
		}
	}
}

void stream_reader::read_object(void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
	if (read(p_buffer,p_bytes,p_abort) != p_bytes) throw exception_io_data_truncation();
}

t_filestats file::get_stats(abort_callback & p_abort)
{
	t_filestats temp;
	temp.m_size = get_size(p_abort);
	temp.m_timestamp = get_timestamp(p_abort);
	return temp;
}

t_filesize stream_reader::skip(t_filesize p_bytes,abort_callback & p_abort)
{
	t_uint8 temp[256];
	t_filesize todo = p_bytes, done = 0;
	while(todo > 0) {
		t_size delta,deltadone;
		delta = sizeof(temp);
		if (delta > todo) delta = (t_size) todo;
		deltadone = read(temp,delta,p_abort);
		done += deltadone;
		todo -= deltadone;
		if (deltadone < delta) break;
	}
	return done;
}

void stream_reader::skip_object(t_filesize p_bytes,abort_callback & p_abort) {
	if (skip(p_bytes,p_abort) != p_bytes) throw exception_io_data_truncation();
}

void filesystem::g_open_write_new(service_ptr_t<file> & p_out,const char * p_path,abort_callback & p_abort) {
	g_open(p_out,p_path,open_mode_write_new,p_abort);
}
void file::g_transfer_file(const service_ptr_t<file> & p_from,const service_ptr_t<file> & p_to,abort_callback & p_abort) {
	t_filesize length = p_from->get_size_ex(p_abort);
	p_from->seek(0,p_abort);
	p_to->seek(0,p_abort);
	p_to->set_eof(p_abort);
	if (length > 0) {
		g_transfer_object(p_from,p_to,length,p_abort);
	}
}

void filesystem::g_open_temp(service_ptr_t<file> & p_out,abort_callback & p_abort) {
	g_open(p_out,"tempfile://",open_mode_write_new,p_abort);
}

void filesystem::g_open_tempmem(service_ptr_t<file> & p_out,abort_callback & p_abort) {
	g_open(p_out,"tempmem://",open_mode_write_new,p_abort);
}

void archive_impl::list_directory(const char * p_path,directory_callback & p_out,abort_callback & p_abort) {
	throw exception_io_not_found();
}

void archive_impl::create_directory(const char * path,abort_callback &) {
	throw exception_io_denied();
}

void filesystem::g_get_stats(const char * p_path,t_filestats & p_stats,bool & p_is_writeable,abort_callback & p_abort) {
	TRACK_CALL_TEXT("filesystem::g_get_stats");
	service_ptr_t<filesystem> fs;
	if (!g_get_interface(fs,p_path)) throw exception_io_no_handler_for_path();
	return fs->get_stats(p_path,p_stats,p_is_writeable,p_abort);
}

void archive_impl::get_stats(const char * p_path,t_filestats & p_stats,bool & p_is_writeable,abort_callback & p_abort) {
	pfc::string8 archive,file;
	if (g_parse_unpack_path(p_path,archive,file)) {
		if (g_is_remote(archive)) throw exception_io_object_is_remote();
		p_is_writeable = false;
		p_stats = get_stats_in_archive(archive,file,p_abort);
	}
	else throw exception_io_not_found();
}


bool file::is_eof(abort_callback & p_abort) {
	t_filesize position,size;
	position = get_position(p_abort);
	size = get_size(p_abort);
	if (size == filesize_invalid) return false;
	return position >= size;
}


t_filetimestamp foobar2000_io::filetimestamp_from_system_timer()
{
	t_filetimestamp ret;
	GetSystemTimeAsFileTime((FILETIME*)&ret);
	return ret;
}

void stream_reader::read_string_ex(pfc::string_base & p_out,t_size p_bytes,abort_callback & p_abort) {
	char * ptr = p_out.lock_buffer(p_bytes);
	try {
		read_object(ptr,p_bytes,p_abort);
	} catch(...) {
		p_out.unlock_buffer();
		throw;
	}
	p_out.unlock_buffer();
}
void stream_reader::read_string(pfc::string_base & p_out,abort_callback & p_abort)
{
	t_uint32 length;
	read_lendian_t(length,p_abort);
	read_string_ex(p_out,length,p_abort);
}

void stream_reader::read_string_raw(pfc::string_base & p_out,abort_callback & p_abort) {
	enum {delta = 256};
	char buffer[delta];
	p_out.reset();
	for(;;) {
		t_size delta_done;
		delta_done = read(buffer,delta,p_abort);
		p_out.add_string(buffer,delta_done);
		if (delta_done < delta) break;
	}
}
void stream_writer::write_string(const char * p_string,t_size p_len,abort_callback & p_abort) {
	t_uint32 len = pfc::downcast_guarded<t_uint32>(pfc::strlen_max(p_string,p_len));
	write_lendian_t(len,p_abort);
	write_object(p_string,len,p_abort);
}

void stream_writer::write_string(const char * p_string,abort_callback & p_abort) {
	write_string(p_string,infinite,p_abort);
}

void stream_writer::write_string_raw(const char * p_string,abort_callback & p_abort) {
	write_object(p_string,strlen(p_string),p_abort);
}

void file::truncate(t_uint64 p_position,abort_callback & p_abort) {
	if (p_position < get_size(p_abort)) resize(p_position,p_abort);
}


#ifdef _WIN32
namespace {
	//rare/weird win32 errors that didn't make it to the main API
	PFC_DECLARE_EXCEPTION(exception_io_device_not_ready,		exception_io,"Device not ready");
	PFC_DECLARE_EXCEPTION(exception_io_invalid_drive,			exception_io_not_found,"Drive not found");
	PFC_DECLARE_EXCEPTION(exception_io_win32,					exception_io,"Generic win32 I/O error");
	PFC_DECLARE_EXCEPTION(exception_io_buffer_overflow,			exception_io,"The file name is too long");
	PFC_DECLARE_EXCEPTION(exception_io_invalid_path_syntax,		exception_io,"Invalid path syntax");

	class exception_io_win32_ex : public exception_io_win32 {
	public:
		exception_io_win32_ex(DWORD p_code) : m_msg(pfc::string_formatter() << "I/O error (win32 #" << (t_uint32)p_code << ")") {}
		exception_io_win32_ex(const exception_io_win32_ex & p_other) {*this = p_other;}
		const char * what() const throw() {return m_msg;}
	private:
		pfc::string8 m_msg;
	};
}
void foobar2000_io::exception_io_from_win32(DWORD p_code) {
	switch(p_code) {
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		throw exception_io_already_exists();
	case ERROR_NETWORK_ACCESS_DENIED:
	case ERROR_ACCESS_DENIED:
		throw exception_io_denied();
	case ERROR_WRITE_PROTECT:
		throw exception_io_write_protected();
	case ERROR_BUSY:
	case ERROR_PATH_BUSY:
	case ERROR_SHARING_VIOLATION:
	case ERROR_LOCK_VIOLATION:
		throw exception_io_sharing_violation();
	case ERROR_HANDLE_DISK_FULL:
	case ERROR_DISK_FULL:
		throw exception_io_device_full();
	case ERROR_FILE_NOT_FOUND:
	case ERROR_PATH_NOT_FOUND:
		throw exception_io_not_found();
	case ERROR_BROKEN_PIPE:
	case ERROR_NO_DATA:
		throw exception_io_no_data();
	case ERROR_NETWORK_UNREACHABLE:
	case ERROR_NETNAME_DELETED:
		throw exception_io_network_not_reachable();
	case ERROR_NOT_READY:
		throw exception_io_device_not_ready();
	case ERROR_INVALID_DRIVE:
		throw exception_io_invalid_drive();
	case ERROR_CRC:
	case ERROR_FILE_CORRUPT:
	case ERROR_DISK_CORRUPT:
		throw exception_io_file_corrupted();
	case ERROR_BUFFER_OVERFLOW:
		throw exception_io_buffer_overflow();
	case ERROR_DISK_CHANGE:
		throw exception_io_disk_change();
	case ERROR_DIR_NOT_EMPTY:
		throw exception_io_directory_not_empty();
	case ERROR_INVALID_NAME:
		throw exception_io_invalid_path_syntax();
	default:
		throw exception_io_win32_ex(p_code);
	}
}
#endif

t_filesize file::get_size_ex(abort_callback & p_abort) {
	t_filesize temp = get_size(p_abort);
	if (temp == filesize_invalid) throw exception_io_no_length();
	return temp;
}

void file::ensure_local() {
	if (is_remote()) throw exception_io_object_is_remote();
}

void file::ensure_seekable() {
	if (!can_seek()) throw exception_io_object_not_seekable();
}

bool filesystem::g_is_recognized_path(const char * p_path) {
	return g_get_interface(service_ptr_t<filesystem>(),p_path);
}

t_filesize file::get_remaining(abort_callback & p_abort) {
	t_filesize length = get_size_ex(p_abort);
	t_filesize position = get_position(p_abort);
	pfc::dynamic_assert(position <= length);
	return length - position;
}


t_filesize file::g_transfer(service_ptr_t<file> p_src,service_ptr_t<file> p_dst,t_filesize p_bytes,abort_callback & p_abort) {
	return g_transfer(pfc::safe_cast<stream_reader*>(p_src.get_ptr()),pfc::safe_cast<stream_writer*>(p_dst.get_ptr()),p_bytes,p_abort);
}

void file::g_transfer_object(service_ptr_t<file> p_src,service_ptr_t<file> p_dst,t_filesize p_bytes,abort_callback & p_abort) {
	if (p_bytes > 1024) /* don't bother on small objects */ 
	{
		t_filesize oldsize = p_dst->get_size(p_abort);
		if (oldsize != filesize_invalid) {
			t_filesize newpos = p_dst->get_position(p_abort) + p_bytes;
			if (newpos > oldsize) p_dst->resize(newpos ,p_abort);
		}
	}
	g_transfer_object(pfc::safe_cast<stream_reader*>(p_src.get_ptr()),pfc::safe_cast<stream_writer*>(p_dst.get_ptr()),p_bytes,p_abort);
}


void foobar2000_io::generate_temp_location_for_file(pfc::string_base & p_out, const char * p_origpath,const char * p_extension,const char * p_magic) {
	hasher_md5_result hash;
	{
		static_api_ptr_t<hasher_md5> hasher;
		hasher_md5_state state;
		hasher->initialize(state);
		hasher->process(state,p_origpath,strlen(p_origpath));
		hasher->process(state,p_extension,strlen(p_extension));
		hasher->process(state,p_magic,strlen(p_magic));
		hash = hasher->get_result(state);
	}

	p_out = p_origpath;
	p_out.truncate(p_out.scan_filename());
	p_out += "temp-";
	p_out += pfc::format_hexdump(hash.m_data,sizeof(hash.m_data),"");
	p_out += ".";
	p_out += p_extension;
}


t_filesize file::skip(t_filesize p_bytes,abort_callback & p_abort) {
	if (p_bytes > 1024 && can_seek()) {
		const t_filesize size = get_size(p_abort);
		if (size != filesize_invalid) {
			const t_filesize position = get_position(p_abort);
			const t_filesize toskip = pfc::min_t( p_bytes, size - position );
			seek(position + toskip,p_abort);
			return toskip;
		}
	}
	return stream_reader::skip(p_bytes,p_abort);
}

bool foobar2000_io::_extract_native_path_ptr(const char * & p_fspath) {
	static const char header[] = "file://"; static const t_size headerLen = 7;
	if (strncmp(p_fspath,header,headerLen) != 0) return false;
	p_fspath += headerLen;
	return true;
}
bool foobar2000_io::extract_native_path(const char * p_fspath,pfc::string_base & p_native) {
	if (!_extract_native_path_ptr(p_fspath)) return false;
	p_native = p_fspath;
	return true;
}

bool foobar2000_io::extract_native_path_ex(const char * p_fspath, pfc::string_base & p_native) {
	if (!_extract_native_path_ptr(p_fspath)) return false;
	if (p_fspath[0] != '\\' || p_fspath[0] != '\\') {
		p_native = "\\\\?\\";
		p_native += p_fspath;
	} else {
		p_native = p_fspath;
	}
	return true;
}

pfc::string stream_reader::read_string(abort_callback & p_abort) {
	t_uint32 len;
	read_lendian_t(len,p_abort);
	return read_string_ex(len,p_abort);
}
pfc::string stream_reader::read_string_ex(t_size p_len,abort_callback & p_abort) {
	pfc::rcptr_t<pfc::string8> temp; temp.new_t();
	read_object(temp->lock_buffer(p_len),p_len,p_abort);
	temp->unlock_buffer();
	return pfc::string::t_data(temp);
}
