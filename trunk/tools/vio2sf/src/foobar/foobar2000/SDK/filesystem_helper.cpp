#include "foobar2000.h"

void stream_writer_chunk::write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
	t_size remaining = p_bytes, written = 0;
	while(remaining > 0) {
		t_size delta = sizeof(m_buffer) - m_buffer_state;
		if (delta > remaining) delta = remaining;
		memcpy(m_buffer,(const t_uint8*)p_buffer + written,delta);
		written += delta;
		remaining -= delta;

		if (m_buffer_state == sizeof(m_buffer)) {
			m_writer->write_lendian_t((t_uint8)m_buffer_state,p_abort);
			m_writer->write_object(m_buffer,m_buffer_state,p_abort);
			m_buffer_state = 0;
		}
	}
}

void stream_writer_chunk::flush(abort_callback & p_abort)
{
	m_writer->write_lendian_t((t_uint8)m_buffer_state,p_abort);
	if (m_buffer_state > 0) {
		m_writer->write_object(m_buffer,m_buffer_state,p_abort);
		m_buffer_state = 0;
	}
}
	
/*
	stream_writer * m_writer;
	unsigned m_buffer_state;
	unsigned char m_buffer[255];
*/

t_size stream_reader_chunk::read(void * p_buffer,t_size p_bytes,abort_callback & p_abort)
{
	t_size todo = p_bytes, done = 0;
	while(todo > 0) {
		if (m_buffer_size == m_buffer_state) {
			if (m_eof) break;
			t_uint8 temp;
			m_reader->read_lendian_t(temp,p_abort);
			m_buffer_size = temp;
			if (temp != sizeof(m_buffer)) m_eof = true;
			m_buffer_state = 0;
			if (m_buffer_size>0) {
				m_reader->read_object(m_buffer,m_buffer_size,p_abort);
			}
		}


		t_size delta = m_buffer_size - m_buffer_state;
		if (delta > todo) delta = todo;
		if (delta > 0) {
			memcpy((unsigned char*)p_buffer + done,m_buffer + m_buffer_state,delta);
			todo -= delta;
			done += delta;
			m_buffer_state += delta;
		}
	}
	return done;
}

void stream_reader_chunk::flush(abort_callback & p_abort) {
	while(!m_eof) {
		p_abort.check_e();
		t_uint8 temp;
		m_reader->read_lendian_t(temp,p_abort);
		m_buffer_size = temp;
		if (temp != sizeof(m_buffer)) m_eof = true;
		m_buffer_state = 0;
		if (m_buffer_size>0) {
			m_reader->skip_object(m_buffer_size,p_abort);
		}
	}
}

/*
	stream_reader * m_reader;
	unsigned m_buffer_state, m_buffer_size;
	bool m_eof;
	unsigned char m_buffer[255];
*/

void stream_reader_chunk::g_skip(stream_reader * p_stream,abort_callback & p_abort) {
	stream_reader_chunk(p_stream).flush(p_abort);
}

t_size reader_membuffer_base::read(void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
	p_abort.check_e();
	t_size max = get_buffer_size();
	if (max < m_offset) throw pfc::exception_bug_check_v2();
	max -= m_offset;
	t_size delta = p_bytes;
	if (delta > max) delta = max;
	memcpy(p_buffer,(char*)get_buffer() + m_offset,delta);
	m_offset += delta;
	return delta;
}

void reader_membuffer_base::seek(t_filesize position,abort_callback & p_abort) {
	p_abort.check_e();
	t_filesize max = get_buffer_size();
	if (position == filesize_invalid || position > max) throw exception_io_seek_out_of_range();
	m_offset = (t_size)position;
}
