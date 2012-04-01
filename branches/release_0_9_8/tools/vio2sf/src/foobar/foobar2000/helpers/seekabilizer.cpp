#include "stdafx.h"

enum {backread_on_seek = 1024};

void seekabilizer_backbuffer::initialize(t_size p_size)
{
	m_depth = m_cursor = 0;

	m_buffer.set_size(p_size);
}

void seekabilizer_backbuffer::write(const void * p_buffer,t_size p_bytes)
{
	if (p_bytes >= m_buffer.get_size())
	{
		memcpy(m_buffer.get_ptr(),(const t_uint8*)p_buffer + p_bytes - m_buffer.get_size(),m_buffer.get_size());
		m_cursor = 0;
		m_depth = m_buffer.get_size();
	}
	else
	{
		const t_uint8* sourceptr = (const t_uint8*) p_buffer;
		t_size remaining = p_bytes;
		while(remaining > 0)
		{
			t_size delta = m_buffer.get_size() - m_cursor;
			if (delta > remaining) delta = remaining;

			memcpy(m_buffer.get_ptr() + m_cursor,sourceptr,delta);

			sourceptr += delta;
			remaining -= delta;
			m_cursor = (m_cursor + delta) % m_buffer.get_size();

			m_depth = pfc::min_t<t_size>(m_buffer.get_size(),m_depth + delta);
			
		}
	}
}

void seekabilizer_backbuffer::read(t_size p_backlogdepth,void * p_buffer,t_size p_bytes) const
{
	assert(p_backlogdepth <= m_depth);
	assert(p_backlogdepth >= p_bytes);

		
	t_uint8* targetptr = (t_uint8*) p_buffer;
	t_size remaining = p_bytes;
	t_size cursor = (m_cursor + m_buffer.get_size() - p_backlogdepth) % m_buffer.get_size();
	
	while(remaining > 0)
	{
		t_size delta = m_buffer.get_size() - cursor;
		if (delta > remaining) delta = remaining;
		
		memcpy(targetptr,m_buffer.get_ptr() + cursor,delta);

		targetptr += delta;
		remaining -= delta;
		cursor = (cursor + delta) % m_buffer.get_size();
	}
}

t_size seekabilizer_backbuffer::get_depth() const
{
	return m_depth;
}

t_size seekabilizer_backbuffer::get_max_depth() const
{
	return m_buffer.get_size();
}

void seekabilizer_backbuffer::reset()
{
	m_depth = m_cursor = 0;
}


void seekabilizer::initialize(service_ptr_t<file> p_base,t_size p_buffer_size,abort_callback & p_abort) {
	m_buffer.initialize(p_buffer_size);
	m_file = p_base;
	m_position = m_position_base = 0;
	m_size = m_file->get_size(p_abort);
}

void seekabilizer::g_seekabilize(service_ptr_t<file> & p_reader,t_size p_buffer_size,abort_callback & p_abort) {
	if (p_reader.is_valid() && p_reader->is_remote() && p_buffer_size > 0) {
		service_ptr_t<seekabilizer> instance = new service_impl_t<seekabilizer>();
		instance->initialize(p_reader,p_buffer_size,p_abort);
		p_reader = instance.get_ptr();
	}
}

t_size seekabilizer::read(void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
	p_abort.check_e();

	if (m_position > m_position_base + pfc::max_t<t_size>(m_buffer.get_max_depth(),backread_on_seek) && m_file->can_seek()) {
		t_filesize target = m_position;
		if (target < backread_on_seek) target = 0;
		else target -= backread_on_seek;
		m_file->seek(target,p_abort);
		m_position_base = target;
	}

	//seek ahead
	while(m_position > m_position_base) {
		enum {tempsize = 1024};
		t_uint8 temp[tempsize];
		t_size delta = (t_size) pfc::min_t<t_filesize>(tempsize,m_position - m_position_base);
		t_size bytes_read = 0;
		bytes_read = m_file->read(temp,delta,p_abort);
		m_buffer.write(temp,bytes_read);
		m_position_base += bytes_read;

		if (bytes_read < delta) {
			return 0;
		}
	}

	t_size done = 0;
	t_uint8 * targetptr = (t_uint8*) p_buffer;

	//try to read backbuffer
	if (m_position < m_position_base) {
		if (m_position_base - m_position > (t_filesize)m_buffer.get_depth()) throw exception_io_seek_out_of_range();
		t_size backread_depth = (t_size) (m_position_base - m_position);
		t_size delta = pfc::min_t<t_size>(backread_depth,p_bytes-done);
		m_buffer.read(backread_depth,targetptr,delta);
		done += delta;
		m_position += delta;
	}

	//regular read
	if (done < p_bytes)
	{
		t_size bytes_read;
		bytes_read = m_file->read(targetptr+done,p_bytes-done,p_abort);

		m_buffer.write(targetptr+done,bytes_read);

		done += bytes_read;
		m_position += bytes_read;
		m_position_base += bytes_read;
	}

	return done;
}

t_filesize seekabilizer::get_size(abort_callback & p_abort) {
	p_abort.check_e();
	return m_size;
}

t_filesize seekabilizer::get_position(abort_callback & p_abort) {
	p_abort.check_e();
	return m_position;
}

void seekabilizer::seek(t_filesize p_position,abort_callback & p_abort) {
	assert(m_position_base >= m_buffer.get_depth());
	p_abort.check_e();

	if (m_size != filesize_invalid && p_position > m_size) throw exception_io_seek_out_of_range();

	t_filesize lowest = m_position_base - m_buffer.get_depth();

	if (p_position < lowest) {
		if (m_file->can_seek()) {
			m_buffer.reset();
			t_filesize target = p_position;
			t_size delta = m_buffer.get_max_depth();
			if (delta > backread_on_seek) delta = backread_on_seek;
			if (target > delta) target -= delta;
			else target = 0;
			m_file->seek(target,p_abort);
			m_position_base = target;
		}
		else {
			m_buffer.reset();
			m_file->reopen(p_abort);
			m_position_base = 0;
		}
	}

	m_position = p_position;
}

bool seekabilizer::can_seek()
{
	return true;
}

bool seekabilizer::get_content_type(pfc::string_base & p_out) {return m_file->get_content_type(p_out);}

bool seekabilizer::is_in_memory() {return false;}

void seekabilizer::on_idle(abort_callback & p_abort) {return m_file->on_idle(p_abort);}

t_filetimestamp seekabilizer::get_timestamp(abort_callback & p_abort) {
	p_abort.check_e();
	return m_file->get_timestamp(p_abort);
}

void seekabilizer::reopen(abort_callback & p_abort) {
	if (m_position_base - m_buffer.get_depth() == 0) {
		seek(0,p_abort);
	} else {
		m_position = m_position_base = 0;
		m_buffer.reset();
		m_file->reopen(p_abort);
	}
}

bool seekabilizer::is_remote()
{
	return m_file->is_remote();
}
