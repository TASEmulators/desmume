#include "stdafx.h"

stream_reader_buffered::stream_reader_buffered(stream_reader * p_base,t_size p_buffer) : m_base(p_base)
{
	m_buffer.set_size(p_buffer);
	m_buffer_ptr = 0;
	m_buffer_max = 0;
}

t_size stream_reader_buffered::read(void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
	p_abort.check_e();
	char * output = (char*) p_buffer;
	t_size output_ptr = 0;
	
	while(output_ptr < p_bytes) {
		{
			t_size delta = pfc::min_t(p_bytes - output_ptr, m_buffer_max - m_buffer_ptr);
			if (delta > 0)
			{
				memcpy(output + output_ptr, m_buffer.get_ptr() + m_buffer_ptr, delta);
				output_ptr += delta;
				m_buffer_ptr += delta;
			}
		}

		if (m_buffer_ptr == m_buffer_max)
		{
			t_size bytes_read;
			bytes_read = m_base->read(m_buffer.get_ptr(), m_buffer.get_size(), p_abort);
			m_buffer_ptr = 0;
			m_buffer_max = bytes_read;

			if (m_buffer_max == 0) break;
		}
	}		

	return output_ptr;
}

stream_writer_buffered::stream_writer_buffered(stream_writer * p_base,t_size p_buffer)
	: m_base(p_base)
{
	m_buffer.set_size(p_buffer);
	m_buffer_ptr = 0;
}
	
void stream_writer_buffered::write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
	p_abort.check_e();
	const char * source = (const char*)p_buffer;
	t_size source_remaining = p_bytes;
	const t_size buffer_size = m_buffer.get_size();
	if (source_remaining >= buffer_size)
	{
		flush(p_abort);
		m_base->write_object(source,source_remaining,p_abort);
		return;
	}

	if (m_buffer_ptr + source_remaining >= buffer_size)
	{
		t_size delta = buffer_size - m_buffer_ptr;
		memcpy(m_buffer.get_ptr() + m_buffer_ptr, source,delta);
		source += delta;
		source_remaining -= delta;
		m_buffer_ptr += delta;
		flush(p_abort);
	}

	memcpy(m_buffer.get_ptr() + m_buffer_ptr, source,source_remaining);
	m_buffer_ptr += source_remaining;
}


void stream_writer_buffered::flush(abort_callback & p_abort) {
	if (m_buffer_ptr > 0) {
		m_base->write_object(m_buffer.get_ptr(),m_buffer_ptr,p_abort);
		m_buffer_ptr = 0;
	}
}