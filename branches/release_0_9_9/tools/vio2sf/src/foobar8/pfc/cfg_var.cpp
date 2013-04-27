#include "pfc.h"


static DWORD read_dword_le_fromptr(const void * src) {return byte_order_helper::dword_le_to_native(*(DWORD*)src);}

cfg_var * cfg_var::list=0;
#if 0
void cfg_var::config_reset()
{
	cfg_var * ptr;
	for(ptr = list; ptr; ptr=ptr->next) ptr->reset();
}
#endif



void cfg_var::config_read_file(const void * p_src,unsigned p_size)
{
	read_config_helper r(p_src,p_size);
	string8_fastalloc name;
	DWORD size;
	while(r.get_remaining()>4)
	{
		if (!r.read_string(name)) break;
		if (!r.read_dword_le(size)) break;
		if (size > r.get_remaining()) break;

		cfg_var * ptr;
		for(ptr = list; ptr; ptr=ptr->next)
		{
			if (!strcmp(ptr->var_name,name))
			{
				ptr->set_raw_data(r.get_ptr(),size);
				break;
			}
		}
		r.advance(size);
	}
}

void cfg_var::config_write_file(write_config_callback * out)
{
	cfg_var * ptr;
	write_config_callback_i temp;
	for(ptr = list; ptr; ptr=ptr->next)
	{
		temp.data.set_size(0);
		if (ptr->get_raw_data(&temp))
		{
			out->write_string(ptr->var_name);
			unsigned size = temp.data.get_size();
			out->write_dword_le(size);
			if (size>0) out->write(temp.data.get_ptr(),size);
		}
	}
}

void cfg_var::config_on_app_init()
{
	cfg_var_notify::on_app_init();
}

void cfg_var_notify::on_app_init()
{
	cfg_var_notify * ptr = list;
	while(ptr)
	{
		if (ptr->my_var)
			ptr->my_var->add_notify(ptr);
		ptr=ptr->next;
	}
}

void cfg_var::on_change()
{
	cfg_var_notify * ptr = notify_list;
	while(ptr)
	{
		ptr->on_var_change(var_get_name(),this);
		ptr = ptr->var_next;
	}
	
}

cfg_var_notify * cfg_var_notify::list=0;

void cfg_var::add_notify(cfg_var_notify * ptr)
{
	ptr->var_next = notify_list;
	notify_list = ptr;
}


void cfg_var::write_config_callback::write_dword_le(DWORD param)
{
	DWORD temp = byte_order_helper::dword_native_to_le(param);
	write(&temp,sizeof(temp));
}

void cfg_var::write_config_callback::write_dword_be(DWORD param)
{
	DWORD temp = byte_order_helper::dword_native_to_be(param);
	write(&temp,sizeof(temp));
}

void cfg_var::write_config_callback::write_string(const char * param)
{
	unsigned len = strlen(param);
	write_dword_le(len);
	write(param,len);
}

bool cfg_int::get_raw_data(write_config_callback * out)
{
	out->write_dword_le(val);
	return true;
}

void cfg_int::set_raw_data(const void * data,int size)
{
	if (size==sizeof(long))
	{
		val = (long)read_dword_le_fromptr(data);
		on_change();
	}
}


bool cfg_var::read_config_helper::read(void * out,unsigned bytes)
{
	if (bytes>remaining) return false;
	memcpy(out,ptr,bytes);
	advance(bytes);
	return true;
}

bool cfg_var::read_config_helper::read_dword_le(DWORD & val)
{
	DWORD temp;
	if (!read(&temp,sizeof(temp))) return false;
	val = byte_order_helper::dword_le_to_native(temp);
	return true;
}
bool cfg_var::read_config_helper::read_dword_be(DWORD & val)
{
	DWORD temp;
	if (!read(&temp,sizeof(temp))) return false;
	val = byte_order_helper::dword_be_to_native(temp);
	return true;
}

bool cfg_var::read_config_helper::read_string(string_base & out)
{
	DWORD len;
	if (!read_dword_le(len)) return false;
	if (remaining<len) return false;
	out.set_string((const char*)ptr,len);
	advance(len);
	return true;
}

void cfg_var::write_config_callback::write_guid_le(const GUID& param)
{
	GUID temp = param;
	byte_order_helper::guid_native_to_le(temp);
	write(&temp,sizeof(temp));
}

bool cfg_var::read_config_helper::read_guid_le(GUID & val)
{
	if (!read(&val,sizeof(val))) return false;
	byte_order_helper::guid_le_to_native(val);
	return true;
}