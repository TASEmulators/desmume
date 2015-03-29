#include "stdafx.h"

t_size file_wrapper_simple::read(void * p_buffer,t_size p_bytes) {
	if (m_has_failed) return 0;
	try {
		return m_file->read(p_buffer,p_bytes,m_abort);
	} catch(std::exception const &) {
		m_has_failed = true;
		return 0;
	}
}

t_size file_wrapper_simple::write(const void * p_buffer,t_size p_bytes) {
	if (m_has_failed) return 0;
	try {
		m_file->write(p_buffer,p_bytes,m_abort);
		return p_bytes;
	} catch(std::exception const &) {
		m_has_failed = true;
		return 0;
	}
}

bool file_wrapper_simple::seek(t_filesize p_offset) {
	if (m_has_failed) return false;
	try {
		m_file->seek(p_offset,m_abort);
		return true;
	} catch(std::exception const &) {
		m_has_failed = true;
		return false;
	}
}

t_filesize file_wrapper_simple::get_position() {
	if (m_has_failed) return filesize_invalid;
	try {
		return m_file->get_position(m_abort);
	} catch(std::exception const &) {
		m_has_failed = true;
		return filesize_invalid;
	}
}

bool file_wrapper_simple::truncate() {
	if (m_has_failed) return false;
	try {
		m_file->set_eof(m_abort);
		return true;
	} catch(std::exception) {
		m_has_failed = true;
		return true;
	}
}


t_filesize file_wrapper_simple::get_size() {
	if (m_has_failed) return filesize_invalid;
	try {
		return m_file->get_size(m_abort);
	} catch(std::exception const &) {
		m_has_failed = true;
		return filesize_invalid;
	}
}

bool file_wrapper_simple::can_seek() {
	if (m_has_failed) return false;
	try {
		return m_file->can_seek();
	} catch(std::exception const &) {
		m_has_failed = true;
		return false;
	}
}
