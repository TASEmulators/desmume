#include "foobar2000.h"


t_size file_info_impl::meta_get_count() const
{
	return m_meta.get_count();
}

const char* file_info_impl::meta_enum_name(t_size p_index) const
{
	return m_meta.get_name(p_index);
}

t_size file_info_impl::meta_enum_value_count(t_size p_index) const
{
	return m_meta.get_value_count(p_index);
}

const char* file_info_impl::meta_enum_value(t_size p_index,t_size p_value_number) const
{
	return m_meta.get_value(p_index,p_value_number);
}

t_size file_info_impl::meta_set_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length)
{
	meta_remove_field_ex(p_name,p_name_length);
	return meta_set_nocheck_ex(p_name,p_name_length,p_value,p_value_length);
}

t_size file_info_impl::meta_set_nocheck_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length)
{
	return m_meta.add_entry(p_name,p_name_length,p_value,p_value_length);
}

void file_info_impl::meta_insert_value_ex(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length)
{
	m_meta.insert_value(p_index,p_value_index,p_value,p_value_length);
}

void file_info_impl::meta_remove_mask(const bit_array & p_mask)
{
	m_meta.remove_mask(p_mask);
}

void file_info_impl::meta_reorder(const t_size * p_order)
{
	m_meta.reorder(p_order);
}

void file_info_impl::meta_remove_values(t_size p_index,const bit_array & p_mask)
{
	m_meta.remove_values(p_index,p_mask);
	if (m_meta.get_value_count(p_index) == 0)
		m_meta.remove_mask(bit_array_one(p_index));
}

t_size file_info_impl::info_get_count() const
{
	return m_info.get_count();
}

const char* file_info_impl::info_enum_name(t_size p_index) const
{
	return m_info.get_name(p_index);
}

const char* file_info_impl::info_enum_value(t_size p_index) const
{
	return m_info.get_value(p_index);
}

t_size file_info_impl::info_set_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length)
{
	info_remove_ex(p_name,p_name_length);
	return info_set_nocheck_ex(p_name,p_name_length,p_value,p_value_length);
}

t_size file_info_impl::info_set_nocheck_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length)
{
	return m_info.add_item(p_name,p_name_length,p_value,p_value_length);
}

void file_info_impl::info_remove_mask(const bit_array & p_mask)
{
	m_info.remove_mask(p_mask);
}


file_info_impl::file_info_impl(const file_info & p_source) : m_length(0)
{
	copy(p_source);
}

file_info_impl::file_info_impl(const file_info_impl & p_source) : m_length(0)
{
	copy(p_source);
}

const file_info_impl & file_info_impl::operator=(const file_info_impl & p_source)
{
	copy(p_source);
	return *this;
}

file_info_impl::file_info_impl() : m_length(0)
{
	m_replaygain.reset();
}

double file_info_impl::get_length() const
{
	return m_length;
}

void file_info_impl::set_length(double p_length)
{
	m_length = p_length;
}

void file_info_impl::meta_modify_value_ex(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length)
{
	m_meta.modify_value(p_index,p_value_index,p_value,p_value_length);
}

replaygain_info file_info_impl::get_replaygain() const
{
	return m_replaygain;
}

void file_info_impl::set_replaygain(const replaygain_info & p_info)
{
	m_replaygain = p_info;
}




file_info_impl::~file_info_impl()
{
}

t_size file_info_impl_utils::info_storage::add_item(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length) {
	t_size index = m_info.get_size();
	m_info.set_size(index + 1);
	m_info[index].init(p_name,p_name_length,p_value,p_value_length);
	return index;
}

void file_info_impl_utils::info_storage::remove_mask(const bit_array & p_mask) {
	pfc::remove_mask_t(m_info,p_mask);
}



t_size file_info_impl_utils::meta_storage::add_entry(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length)
{
	meta_entry temp(p_name,p_name_length,p_value,p_value_length);
	return pfc::append_swap_t(m_data,temp);
}

void file_info_impl_utils::meta_storage::insert_value(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length)
{
	m_data[p_index].insert_value(p_value_index,p_value,p_value_length);
}

void file_info_impl_utils::meta_storage::modify_value(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length)
{
	m_data[p_index].modify_value(p_value_index,p_value,p_value_length);
}

void file_info_impl_utils::meta_storage::remove_values(t_size p_index,const bit_array & p_mask)
{
	m_data[p_index].remove_values(p_mask);
}

void file_info_impl_utils::meta_storage::remove_mask(const bit_array & p_mask)
{
	pfc::remove_mask_t(m_data,p_mask);
}
	

file_info_impl_utils::meta_entry::meta_entry(const char * p_name,t_size p_name_len,const char * p_value,t_size p_value_len)
{
	m_name.set_string(p_name,p_name_len);
	m_values.set_size(1);
	m_values[0].set_string(p_value,p_value_len);
}


void file_info_impl_utils::meta_entry::remove_values(const bit_array & p_mask)
{
	pfc::remove_mask_t(m_values,p_mask);
}

void file_info_impl_utils::meta_entry::insert_value(t_size p_value_index,const char * p_value,t_size p_value_length)
{
	pfc::string_simple temp;
	temp.set_string(p_value,p_value_length);
	pfc::insert_swap_t(m_values,temp,p_value_index);
}

void file_info_impl_utils::meta_entry::modify_value(t_size p_value_index,const char * p_value,t_size p_value_length)
{
	m_values[p_value_index].set_string(p_value,p_value_length);
}

void file_info_impl_utils::meta_storage::reorder(const t_size * p_order)
{
	pfc::reorder_t(m_data,p_order,m_data.get_size());
}

void file_info_impl::copy_meta(const file_info & p_source)
{
	m_meta.copy_from(p_source);
}

void file_info_impl::copy_info(const file_info & p_source)
{
	m_info.copy_from(p_source);
}

void file_info_impl_utils::meta_storage::copy_from(const file_info & p_info)
{
	t_size meta_index,meta_count = p_info.meta_get_count();
	m_data.set_size(meta_count);
	for(meta_index=0;meta_index<meta_count;meta_index++)
	{
		meta_entry & entry = m_data[meta_index];
		t_size value_index,value_count = p_info.meta_enum_value_count(meta_index);
		entry.m_name = p_info.meta_enum_name(meta_index);
		entry.m_values.set_size(value_count);
		for(value_index=0;value_index<value_count;value_index++)
			entry.m_values[value_index] = p_info.meta_enum_value(meta_index,value_index);
	}
}

void file_info_impl_utils::info_storage::copy_from(const file_info & p_info)
{
	t_size n, count;
	count = p_info.info_get_count();
	m_info.set_count(count);
	for(n=0;n<count;n++) m_info[n].init(p_info.info_enum_name(n),infinite,p_info.info_enum_value(n),infinite);	
}