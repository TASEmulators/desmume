#include "foobar2000.h"


#define tf_profiler(x) // profiler(x)

void titleformat_compiler::remove_color_marks(const char * src,pfc::string_base & out)//helper
{
	out.reset();
	while(*src)
	{
		if (*src==3)
		{
			src++;
			while(*src && *src!=3) src++;
			if (*src==3) src++;
		}
		else out.add_byte(*src++);
	}
}

static bool test_for_bad_char(const char * source,t_size source_char_len,const char * reserved)
{
	return pfc::strstr_ex(reserved,(t_size)(-1),source,source_char_len) != (t_size)(-1);
}

void titleformat_compiler::remove_forbidden_chars(titleformat_text_out * p_out,const GUID & p_inputtype,const char * p_source,t_size p_source_len,const char * p_reserved_chars)
{
	if (p_reserved_chars == 0 || *p_reserved_chars == 0)
	{
		p_out->write(p_inputtype,p_source,p_source_len);
	}
	else
	{
		p_source_len = pfc::strlen_max(p_source,p_source_len);
		t_size index = 0;
		t_size good_byte_count = 0;
		while(index < p_source_len)
		{
			t_size delta = pfc::utf8_char_len(p_source + index,p_source_len - index);
			if (delta == 0) break;
			if (test_for_bad_char(p_source+index,delta,p_reserved_chars))
			{
				if (good_byte_count > 0) {p_out->write(p_inputtype,p_source+index-good_byte_count,good_byte_count);good_byte_count=0;}
				p_out->write(p_inputtype,"_",1);
			}
			else
			{
				good_byte_count += delta;
			}
			index += delta;
		}
		if (good_byte_count > 0) {p_out->write(p_inputtype,p_source+index-good_byte_count,good_byte_count);good_byte_count=0;}
	}
}

void titleformat_compiler::remove_forbidden_chars_string_append(pfc::string_receiver & p_out,const char * p_source,t_size p_source_len,const char * p_reserved_chars)
{
	remove_forbidden_chars(&titleformat_text_out_impl_string(p_out),pfc::guid_null,p_source,p_source_len,p_reserved_chars);
}

void titleformat_compiler::remove_forbidden_chars_string(pfc::string_base & p_out,const char * p_source,t_size p_source_len,const char * p_reserved_chars)
{
	p_out.reset();
	remove_forbidden_chars_string_append(p_out,p_source,p_source_len,p_reserved_chars);
}

bool titleformat_hook_impl_file_info::process_field(titleformat_text_out * p_out,const char * p_name,t_size p_name_length,bool & p_found_flag) {
	return m_api->process_field(*m_info,m_location,p_out,p_name,p_name_length,p_found_flag);
}
bool titleformat_hook_impl_file_info::process_function(titleformat_text_out * p_out,const char * p_name,t_size p_name_length,titleformat_hook_function_params * p_params,bool & p_found_flag) {
	return m_api->process_function(*m_info,m_location,p_out,p_name,p_name_length,p_params,p_found_flag);
}

void titleformat_object::run_hook(const playable_location & p_location,const file_info * p_source,titleformat_hook * p_hook,pfc::string_base & p_out,titleformat_text_filter * p_filter)
{
	if (p_hook)
	{
		run(
			&titleformat_hook_impl_splitter(
			p_hook,
			&titleformat_hook_impl_file_info(p_location,p_source)
			),
			p_out,p_filter);
	}
	else
	{
		run(
			&titleformat_hook_impl_file_info(p_location,p_source),
			p_out,p_filter);
	}
}

void titleformat_object::run_simple(const playable_location & p_location,const file_info * p_source,pfc::string_base & p_out)
{
	run(&titleformat_hook_impl_file_info(p_location,p_source),p_out,NULL);
}

t_size titleformat_hook_function_params::get_param_uint(t_size index)
{
	const char * str;
	t_size str_len;
	get_param(index,str,str_len);
	return pfc::atoui_ex(str,str_len);
}


void titleformat_text_out_impl_filter_chars::write(const GUID & p_inputtype,const char * p_data,t_size p_data_length)
{
	titleformat_compiler::remove_forbidden_chars(m_chain,p_inputtype,p_data,p_data_length,m_restricted_chars);
}

bool titleformat_hook_impl_splitter::process_field(titleformat_text_out * p_out,const char * p_name,t_size p_name_length,bool & p_found_flag)
{
	p_found_flag = false;
	if (m_hook1 && m_hook1->process_field(p_out,p_name,p_name_length,p_found_flag)) return true;
	p_found_flag = false;
	if (m_hook2 && m_hook2->process_field(p_out,p_name,p_name_length,p_found_flag)) return true;
	p_found_flag = false;
	return false;
}

bool titleformat_hook_impl_splitter::process_function(titleformat_text_out * p_out,const char * p_name,t_size p_name_length,titleformat_hook_function_params * p_params,bool & p_found_flag)
{
	p_found_flag = false;
	if (m_hook1 && m_hook1->process_function(p_out,p_name,p_name_length,p_params,p_found_flag)) return true;
	p_found_flag = false;
	if (m_hook2 && m_hook2->process_function(p_out,p_name,p_name_length,p_params,p_found_flag)) return true;
	p_found_flag = false;
	return false;
}

void titleformat_text_out::write_int_padded(const GUID & p_inputtype,t_int64 val,t_int64 maxval)
{
	unsigned width = 0;
	while(maxval > 0) {maxval/=10;width++;}
	write(p_inputtype,pfc::format_int(val,width));
}

void titleformat_text_out::write_int(const GUID & p_inputtype,t_int64 val)
{
	write(p_inputtype,pfc::format_int(val));
}
void titleformat_text_filter_impl_reserved_chars::write(const GUID & p_inputtype,pfc::string_receiver & p_out,const char * p_data,t_size p_data_length)
{
	if (p_inputtype == titleformat_inputtypes::meta) titleformat_compiler::remove_forbidden_chars_string_append(p_out,p_data,p_data_length,m_reserved_chars);
	else p_out.add_string(p_data,p_data_length);
}

void titleformat_compiler::run(titleformat_hook * p_source,pfc::string_base & p_out,const char * p_spec)
{
	service_ptr_t<titleformat_object> ptr;
	if (!compile(ptr,p_spec)) p_out = "[COMPILATION ERROR]";
	else ptr->run(p_source,p_out,NULL);
}

void titleformat_compiler::compile_safe(service_ptr_t<titleformat_object> & p_out,const char * p_spec)
{
	if (!compile(p_out,p_spec)) {
		if (!compile(p_out,"%filename%"))
			throw pfc::exception_bug_check_v2();
	}
}


namespace titleformat_inputtypes {
	const GUID meta = { 0xcd839c8e, 0x5c66, 0x4ae1, { 0x8d, 0xad, 0x71, 0x1f, 0x86, 0x0, 0xa, 0xe3 } };
	const GUID unknown = { 0x673aa1cd, 0xa7a8, 0x40c8, { 0xbf, 0x9b, 0x34, 0x37, 0x99, 0x29, 0x16, 0x3b } };
};

void titleformat_text_filter_impl_filename_chars::write(const GUID & p_inputType,pfc::string_receiver & p_out,const char * p_data,t_size p_dataLength) {
	if (p_inputType == titleformat_inputtypes::meta) {
		//slightly inefficient...
		p_out.add_string( pfc::io::path::replaceIllegalNameChars(pfc::string(p_data,p_dataLength)).ptr());
	} else p_out.add_string(p_data,p_dataLength);
}

void titleformat_compiler::compile_safe_ex(titleformat_object::ptr & p_out,const char * p_spec,const char * p_fallback) {
	if (!compile(p_out,p_spec)) compile_force(p_out,p_fallback);
}


void titleformat_text_filter_nontext_chars::write(const GUID & p_inputtype,pfc::string_receiver & p_out,const char * p_data,t_size p_data_length) {
	for(t_size walk = 0;;) {
		t_size base = walk;
		while(walk < p_data_length && !isReserved(p_data[walk]) && p_data[walk] != 0) walk++;
		p_out.add_string(p_data+base,walk-base);
		if (walk >= p_data_length || p_data[walk] == 0) break;
		p_out.add_byte('_'); walk++;
	}
}
