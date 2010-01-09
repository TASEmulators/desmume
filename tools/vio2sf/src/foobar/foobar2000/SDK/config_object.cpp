#include "foobar2000.h"

void config_object_notify_manager::g_on_changed(const service_ptr_t<config_object> & p_object)
{
	if (core_api::assert_main_thread())
	{
		service_enum_t<config_object_notify_manager> e;
		service_ptr_t<config_object_notify_manager> ptr;
		while(e.next(ptr))
			ptr->on_changed(p_object);
	}
}

bool config_object::g_find(service_ptr_t<config_object> & p_out,const GUID & p_guid)
{
	service_ptr_t<config_object> ptr;
	service_enum_t<config_object> e;
	while(e.next(ptr))
	{
		if (ptr->get_guid() == p_guid)
		{
			p_out = ptr;
			return true;
		}
	}
	return false;
}

void config_object::g_get_data_string(const GUID & p_guid,pfc::string_base & p_out)
{
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	ptr->get_data_string(p_out);
}

void config_object::g_set_data_string(const GUID & p_guid,const char * p_data,t_size p_length)
{
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	ptr->set_data_string(p_data,p_length);
}

void config_object::get_data_int32(t_int32 & p_out)
{
	t_int32 temp;
	get_data_struct_t<t_int32>(temp);
	byte_order::order_le_to_native_t(temp);
	p_out = temp;
}

void config_object::set_data_int32(t_int32 p_val)
{
	t_int32 temp = p_val;
	byte_order::order_native_to_le_t(temp);
	set_data_struct_t<t_int32>(temp);
}

bool config_object::get_data_bool_simple(bool p_default) {
	try {
		bool ret = p_default;
		get_data_bool(ret);
		return ret;
	} catch(...) {return p_default;}
}

t_int32 config_object::get_data_int32_simple(t_int32 p_default) {
	try {
		t_int32 ret = p_default;
		get_data_int32(ret);
		return ret;
	} catch(...) {return p_default;}
}

void config_object::g_get_data_int32(const GUID & p_guid,t_int32 & p_out) {
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	ptr->get_data_int32(p_out);
}

void config_object::g_set_data_int32(const GUID & p_guid,t_int32 p_val) {
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	ptr->set_data_int32(p_val);
}

bool config_object::g_get_data_bool_simple(const GUID & p_guid,bool p_default)
{
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	return ptr->get_data_bool_simple(p_default);
}

t_int32 config_object::g_get_data_int32_simple(const GUID & p_guid,t_int32 p_default)
{
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	return ptr->get_data_int32_simple(p_default);
}

void config_object::get_data_bool(bool & p_out) {get_data_struct_t<bool>(p_out);}
void config_object::set_data_bool(bool p_val) {set_data_struct_t<bool>(p_val);}

void config_object::g_get_data_bool(const GUID & p_guid,bool & p_out) {g_get_data_struct_t<bool>(p_guid,p_out);}
void config_object::g_set_data_bool(const GUID & p_guid,bool p_val) {g_set_data_struct_t<bool>(p_guid,p_val);}

namespace {
	class stream_writer_string : public stream_writer {
	public:
		void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
			m_out.add_string((const char*)p_buffer,p_bytes);
		}
		stream_writer_string(pfc::string_base & p_out) : m_out(p_out) {m_out.reset();}
	private:
		pfc::string_base & m_out;
	};

	class stream_writer_fixedbuffer : public stream_writer {
	public:
		void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
			if (p_bytes > 0) {
				if (p_bytes > m_bytes - m_bytes_read) throw pfc::exception_overflow();
				memcpy((t_uint8*)m_out,p_buffer,p_bytes);
				m_bytes_read += p_bytes;
			}
		}
		stream_writer_fixedbuffer(void * p_out,t_size p_bytes,t_size & p_bytes_read) : m_out(p_out), m_bytes(p_bytes), m_bytes_read(p_bytes_read) {m_bytes_read = 0;}
	private:
		void * m_out;
		t_size m_bytes;
		t_size & m_bytes_read;
	};



	class stream_writer_get_length : public stream_writer {
	public:
		void write(const void * p_buffer,t_size p_bytes,abort_callback & p_abort) {
			m_length += p_bytes;
		}
		stream_writer_get_length(t_size & p_length) : m_length(p_length) {m_length = 0;}
	private:
		t_size & m_length;
	};
};

t_size config_object::get_data_raw(void * p_out,t_size p_bytes) {
	t_size ret = 0;
	get_data(&stream_writer_fixedbuffer(p_out,p_bytes,ret),abort_callback_impl());
	return ret;
}

t_size config_object::get_data_raw_length() {
	t_size ret = 0;
	get_data(&stream_writer_get_length(ret),abort_callback_impl());
	return ret;
}

void config_object::set_data_raw(const void * p_data,t_size p_bytes, bool p_notify) {
	set_data(&stream_reader_memblock_ref(p_data,p_bytes),abort_callback_impl(),p_notify);
}

void config_object::set_data_string(const char * p_data,t_size p_length) {
	set_data_raw(p_data,pfc::strlen_max(p_data,p_length));
}

void config_object::get_data_string(pfc::string_base & p_out) {
	get_data(&stream_writer_string(p_out),abort_callback_impl());
}


//config_object_impl stuff


void config_object_impl::get_data(stream_writer * p_stream,abort_callback & p_abort) const {
	insync(m_sync);
	p_stream->write_object(m_data.get_ptr(),m_data.get_size(),p_abort);
}

void config_object_impl::set_data(stream_reader * p_stream,abort_callback & p_abort,bool p_notify) {
	core_api::ensure_main_thread();

	{
		insync(m_sync);
		m_data.set_size(0);
		enum {delta = 1024};
		t_uint8 buffer[delta];
		for(;;)
		{
			t_size delta_done = p_stream->read(buffer,delta,p_abort);
			
			if (delta_done > 0)
			{
				m_data.append_fromptr(buffer,delta_done);
			}
			
			if (delta_done != delta) break;
		}
	}

	if (p_notify) config_object_notify_manager::g_on_changed(this);
}

config_object_impl::config_object_impl(const GUID & p_guid,const void * p_data,t_size p_bytes) : cfg_var(p_guid)
{
	m_data.set_data_fromptr((const t_uint8*)p_data,p_bytes);
}
