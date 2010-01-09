class file_wrapper_simple
{
public:
	explicit file_wrapper_simple(const service_ptr_t<file> & p_file,abort_callback & p_abort) : m_file(p_file), m_abort(p_abort), m_has_failed(false) {}

	inline bool has_failed() const {return m_has_failed;}
	inline void reset_status() {m_has_failed = false;}


	t_size read(void * p_buffer,t_size p_bytes);
	t_size write(const void * p_buffer,t_size p_bytes);
	bool seek(t_filesize p_offset);
	t_filesize get_position();
	t_filesize get_size();
	bool can_seek();
	bool truncate();
private:
	service_ptr_t<file> m_file;
	abort_callback & m_abort;
	bool m_has_failed;
};
