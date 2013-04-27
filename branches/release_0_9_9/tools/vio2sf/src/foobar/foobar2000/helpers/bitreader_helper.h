namespace bitreader_helper {

	inline static size_t extract_bit(const t_uint8 * p_stream,size_t p_offset) {
		return (p_stream[p_offset>>3] >> (7-(p_offset&7)))&1;
	}

	static size_t extract_int(const t_uint8 * p_stream,size_t p_base,size_t p_width) {
		size_t ret = 0;
		size_t offset = p_base;
		for(size_t bit=0;bit<p_width;bit++) {
			ret <<= 1;
			ret |= extract_bit(p_stream,offset++);
		}
		return ret;
	}

class bitreader
{
public:
	inline bitreader(const t_uint8 * p_ptr,t_size p_base)
		: m_ptr(p_ptr), m_bitptr(p_base)
	{
	}

	inline void skip(t_size p_bits)
	{
		m_bitptr += p_bits;
	}

	template<typename t_ret>
	t_ret read_t(t_size p_bits) {
		t_ret ret = 0;
		for(t_size bit=0;bit<p_bits;bit++)
		{
			ret <<= 1;
			ret |= (m_ptr[m_bitptr>>3] >> (7-(m_bitptr&7)))&1;
			m_bitptr++;
		}
		return ret;
	}

	t_size read(t_size p_bits) {return read_t<t_size>(p_bits);}

	inline t_size get_bitptr() const {return m_bitptr;}

	inline bool read_bit() {
		bool state = ( (m_ptr[m_bitptr>>3] >> (7-(m_bitptr&7)))&1 ) != 0;
		m_bitptr++;
		return state;
	}

private:

	const t_uint8 * m_ptr;
	t_size m_bitptr;
};

class bitreader_fromfile
{
public:
	inline bitreader_fromfile(service_ptr_t<file> const& p_file) : m_file(p_file), m_buffer_ptr(0) {}
	
	t_size read(t_size p_bits,abort_callback & p_abort) {
		t_size ret = 0;
		for(t_size bit=0;bit<p_bits;bit++) {
			if (m_buffer_ptr == 0)
				m_file->read_object(&m_buffer,1,p_abort);

			ret <<= 1;
			ret |= (m_buffer >> (7-m_buffer_ptr))&1;
			m_buffer_ptr = (m_buffer_ptr+1) & 7;
		}
		return ret;
	}

	void skip(t_size p_bits,abort_callback & p_abort) {
		for(t_size bit=0;bit<p_bits;bit++) {
			if (m_buffer_ptr == 0) m_file->read_object(&m_buffer,1,p_abort);
			m_buffer_ptr = (m_buffer_ptr+1) & 7;
		}
	}

	inline void byte_align() {m_buffer_ptr = 0;}

private:
	service_ptr_t<file> m_file;
	t_size m_buffer_ptr;
	t_uint8 m_buffer;
};

class bitreader_limited
{
public:
	inline bitreader_limited(const t_uint8 * p_ptr,t_size p_base,t_size p_remaining) : m_reader(p_ptr,p_base), m_remaining(p_remaining) {}

	inline t_size get_bitptr() const {return m_reader.get_bitptr();}

	inline t_size get_remaining() const {return m_remaining;}

	inline void skip(t_size p_bits) {
		if (p_bits > m_remaining) throw exception_io_data_truncation();
		m_remaining -= p_bits;
		m_reader.skip(p_bits);
	}

	t_size read(t_size p_bits)
	{
		if (p_bits > m_remaining) throw exception_io_data_truncation();
		m_remaining -= p_bits;
		return m_reader.read(p_bits);
	}

private:
	bitreader m_reader;
	t_size m_remaining;
};

inline static t_size extract_bits(const t_uint8 * p_buffer,t_size p_base,t_size p_count) {
	return bitreader(p_buffer,p_base).read(p_count);
}

}