class seekabilizer_backbuffer
{
public:
	void initialize(t_size p_size);
	void write(const void * p_buffer,t_size p_bytes);
	void read(t_size p_backlogdepth,void * p_buffer,t_size p_bytes) const;
	t_size get_depth() const;
	void reset();
	t_size get_max_depth() const;
private:
	pfc::array_t<t_uint8> m_buffer;
	t_size m_depth,m_cursor;
};

class seekabilizer : public file_readonly {
public:
	void initialize(service_ptr_t<file> p_base,t_size p_buffer_size,abort_callback & p_abort);
	
	static void g_seekabilize(service_ptr_t<file> & p_reader,t_size p_buffer_size,abort_callback & p_abort);

	t_size read(void * p_buffer,t_size p_bytes,abort_callback & p_abort);
	t_filesize get_size(abort_callback & p_abort);
	t_filesize get_position(abort_callback & p_abort);
	void seek(t_filesize p_position,abort_callback & p_abort);
	bool can_seek();
	bool get_content_type(pfc::string_base & p_out);
	bool is_in_memory();
	void on_idle(abort_callback & p_abort);
	t_filetimestamp get_timestamp(abort_callback & p_abort);
	void reopen(abort_callback & p_abort);
	bool is_remote();
private:
	service_ptr_t<file> m_file;
	seekabilizer_backbuffer m_buffer;
	t_filesize m_size,m_position,m_position_base;
};