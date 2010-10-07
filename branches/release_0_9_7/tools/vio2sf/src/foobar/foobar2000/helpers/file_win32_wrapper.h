namespace file_win32_helpers {
	static t_filesize get_size(HANDLE p_handle) {
		union {
			t_uint64 val64;
			t_uint32 val32[2];
		} u;

		u.val64 = 0;
		SetLastError(NO_ERROR);
		u.val32[0] = GetFileSize(p_handle,reinterpret_cast<DWORD*>(&u.val32[1]));
		if (GetLastError()!=NO_ERROR) exception_io_from_win32(GetLastError());
		return u.val64;
	}
	static void seek(HANDLE p_handle,t_sfilesize p_position,file::t_seek_mode p_mode) {
		union  {
			t_int64 temp64;
			struct {
				DWORD temp_lo;
				LONG temp_hi;
			};
		};

		temp64 = p_position;
		SetLastError(ERROR_SUCCESS);		
		temp_lo = SetFilePointer(p_handle,temp_lo,&temp_hi,(DWORD)p_mode);
		if (GetLastError() != ERROR_SUCCESS) exception_io_from_win32(GetLastError());
	}
}

template<bool p_seekable,bool p_writeable>
class file_win32_wrapper_t : public file {
public:
	file_win32_wrapper_t(HANDLE p_handle) : m_handle(p_handle), m_position(0)
	{
	}

	static service_ptr_t<file> g_CreateFile(const char * p_path,DWORD p_access,DWORD p_sharemode,LPSECURITY_ATTRIBUTES p_security_attributes,DWORD p_createmode,DWORD p_flags,HANDLE p_template) {
		SetLastError(NO_ERROR);
		HANDLE handle = uCreateFile(p_path,p_access,p_sharemode,p_security_attributes,p_createmode,p_flags,p_template);
		if (handle == INVALID_HANDLE_VALUE) exception_io_from_win32(GetLastError());
		try {
			return g_create_from_handle(handle);
		} catch(...) {CloseHandle(handle); throw;}
	}

	static service_ptr_t<file> g_create_from_handle(HANDLE p_handle) {
		return new service_impl_t<file_win32_wrapper_t<p_seekable,p_writeable> >(p_handle);
	}


	void reopen(abort_callback & p_abort) {seek(0,p_abort);}

	void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
		if (!p_writeable) throw exception_io_denied();

		pfc::static_assert_t< (sizeof(t_size) >= sizeof(DWORD)) >();

		t_size bytes_written_total = 0;

		if (sizeof(t_size) == sizeof(DWORD)) {
			p_abort.check_e();
			DWORD bytes_written = 0;
			SetLastError(ERROR_SUCCESS);
			if (!WriteFile(m_handle,p_buffer,(DWORD)p_bytes,&bytes_written,0)) exception_io_from_win32(GetLastError());
			if (bytes_written != p_bytes) throw exception_io("Write failure");
			bytes_written_total = bytes_written;
			m_position += bytes_written;
		} else {
			while(bytes_written_total < p_bytes) {
				p_abort.check_e();
				DWORD bytes_written = 0;
				DWORD delta = (DWORD) pfc::min_t<t_size>(p_bytes - bytes_written_total, 0x80000000);
				SetLastError(ERROR_SUCCESS);
				if (!WriteFile(m_handle,(const t_uint8*)p_buffer + bytes_written_total,delta,&bytes_written,0)) exception_io_from_win32(GetLastError());
				if (bytes_written != delta) throw exception_io("Write failure");
				bytes_written_total += bytes_written;
				m_position += bytes_written;
			}
		}
	}
	
	t_size read(void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
		pfc::static_assert_t< (sizeof(t_size) >= sizeof(DWORD)) >();
		
		t_size bytes_read_total = 0;
		if (sizeof(t_size) == sizeof(DWORD)) {
			p_abort.check_e();
			DWORD bytes_read = 0;
			SetLastError(ERROR_SUCCESS);
			if (!ReadFile(m_handle,p_buffer,pfc::downcast_guarded<DWORD>(p_bytes),&bytes_read,0)) exception_io_from_win32(GetLastError());
			bytes_read_total = bytes_read;
			m_position += bytes_read;
		} else {
			while(bytes_read_total < p_bytes) {
				p_abort.check_e();
				DWORD bytes_read = 0;
				DWORD delta = (DWORD) pfc::min_t<t_size>(p_bytes - bytes_read_total, 0x80000000);
				SetLastError(ERROR_SUCCESS);
				if (!ReadFile(m_handle,(t_uint8*)p_buffer + bytes_read_total,delta,&bytes_read,0)) exception_io_from_win32(GetLastError());
				bytes_read_total += bytes_read;
				m_position += bytes_read;
				if (bytes_read != delta) break;
			}
		}
		return bytes_read_total;
	}


	t_filesize get_size(abort_callback & p_abort) {
		p_abort.check_e();
		return file_win32_helpers::get_size(m_handle);
	}

	t_filesize get_position(abort_callback & p_abort) {
		p_abort.check_e();
		return m_position;
	}
	
	void resize(t_filesize p_size,abort_callback & p_abort) {
		if (!p_writeable) throw exception_io_denied();
		p_abort.check_e();
		if (m_position != p_size) {
			file_win32_helpers::seek(m_handle,p_size,file::seek_from_beginning);
		}
		SetLastError(ERROR_SUCCESS);
		if (!SetEndOfFile(m_handle)) {
			DWORD code = GetLastError();
			if (m_position != p_size) try {file_win32_helpers::seek(m_handle,m_position,file::seek_from_beginning);} catch(...) {}
			exception_io_from_win32(code);
		}
		if (m_position > p_size) m_position = p_size;
		if (m_position != p_size) file_win32_helpers::seek(m_handle,m_position,file::seek_from_beginning);
	}

#if 0
	void set_eof(abort_callback & p_abort) {
		if (!p_writeable) throw exception_io_denied();
		p_abort.check_e();

		SetLastError(ERROR_SUCCESS);
		if (!SetEndOfFile(m_handle)) exception_io_from_win32(GetLastError());
	}
#endif

	void seek(t_filesize p_position,abort_callback & p_abort) {
		if (!p_seekable) throw exception_io_object_not_seekable();
		p_abort.check_e();
		if (p_position > file_win32_helpers::get_size(m_handle)) throw exception_io_seek_out_of_range();
		file_win32_helpers::seek(m_handle,p_position,file::seek_from_beginning);
		m_position = p_position;
		//return seek_ex((t_sfilesize)p_position,file::seek_from_beginning,p_abort);
	}
#if 0
	void seek_ex(t_sfilesize p_position,file::t_seek_mode p_mode,abort_callback & p_abort) {
		if (!p_seekable) throw exception_io_object_not_seekable();
		p_abort.check_e();

		file_win32_helpers::seek(m_handle,p_position,p_mode);

        m_position = (t_filesize) temp64;
	}
#endif

	bool can_seek() {return p_seekable;}
	bool get_content_type(pfc::string_base & out) {return false;}
	bool is_in_memory() {return false;}
	void on_idle(abort_callback & p_abort) {p_abort.check_e();}
	
	t_filetimestamp get_timestamp(abort_callback & p_abort) {
		p_abort.check_e();
		FlushFileBuffers(m_handle);
		SetLastError(ERROR_SUCCESS);
		t_filetimestamp temp;
		if (!GetFileTime(m_handle,0,0,(FILETIME*)&temp)) exception_io_from_win32(GetLastError());
		return temp;
	}

	bool is_remote() {return false;}
	~file_win32_wrapper_t() {CloseHandle(m_handle);}
private:
	HANDLE m_handle;
	t_filesize m_position;
};
