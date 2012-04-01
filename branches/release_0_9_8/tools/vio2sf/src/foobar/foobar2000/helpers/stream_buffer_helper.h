class stream_reader_buffered : public stream_reader
{
public:
	stream_reader_buffered(stream_reader * p_base,t_size p_buffer);
	t_size read(void * p_buffer,t_size p_bytes,abort_callback & p_abort);
private:
	stream_reader * m_base;
	pfc::array_t<char> m_buffer;
	t_size m_buffer_ptr, m_buffer_max;
};

class stream_writer_buffered : public stream_writer
{
public:
	stream_writer_buffered(stream_writer * p_base,t_size p_buffer);
	
	void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort);

	void flush(abort_callback & p_abort);

private:
	stream_writer * m_base;
	pfc::array_t<char> m_buffer;
	t_size m_buffer_ptr;
};

