#include "stdafx.h"

file_info_update_helper::file_info_update_helper(metadb_handle_ptr p_item)
{
	const t_size count = 1;
	m_data.add_item(p_item);

	m_infos.set_size(count);
	m_mask.set_size(count);
	for(t_size n=0;n<count;n++) m_mask[n] = false;
}

file_info_update_helper::file_info_update_helper(const pfc::list_base_const_t<metadb_handle_ptr> & p_data)
{
	const t_size count = p_data.get_count();
	m_data.add_items(p_data);

	m_infos.set_size(count);
	m_mask.set_size(count);
	for(t_size n=0;n<count;n++) m_mask[n] = false;
}

bool file_info_update_helper::read_infos(HWND p_parent,bool p_show_errors)
{
	static_api_ptr_t<metadb_io> api;
	if (api->load_info_multi(m_data,metadb_io::load_info_default,p_parent,p_show_errors) != metadb_io::load_info_success) return false;
	t_size n; const t_size m = m_data.get_count();
	t_size loaded_count = 0;
	for(n=0;n<m;n++)
	{
		bool val = m_data[n]->get_info(m_infos[n]);
		if (val) loaded_count++;
		m_mask[n] = val;
	}
	return loaded_count == m;
}

file_info_update_helper::t_write_result file_info_update_helper::write_infos(HWND p_parent,bool p_show_errors)
{
	t_size n, outptr = 0; const t_size count = m_data.get_count();
	pfc::array_t<metadb_handle_ptr> items_to_update;
	pfc::array_t<file_info *> infos_to_write;
	items_to_update.set_size(count);
	infos_to_write.set_size(count);
	
	for(n=0;n<count;n++)
	{
		if (m_mask[n])
		{
			items_to_update[outptr] = m_data[n];
			infos_to_write[outptr] = &m_infos[n];
			outptr++;
		}
	}

	if (outptr == 0) return write_ok;
	else
	{
		static_api_ptr_t<metadb_io> api;
		switch(api->update_info_multi(
			pfc::list_const_array_t<metadb_handle_ptr,const pfc::array_t<metadb_handle_ptr>&>(items_to_update,outptr),
			pfc::list_const_array_t<file_info*,const pfc::array_t<file_info*>&>(infos_to_write,outptr),
			p_parent,
			true
			))
		{
		case metadb_io::update_info_success:
			return write_ok;
		case metadb_io::update_info_aborted:
			return write_aborted;
		default:
		case metadb_io::update_info_errors:
			return write_error;
		}
	}
}

t_size file_info_update_helper::get_item_count() const
{
	return m_data.get_count();
}

bool file_info_update_helper::is_item_valid(t_size p_index) const
{
	return m_mask[p_index];
}
	
file_info & file_info_update_helper::get_item(t_size p_index)
{
	return m_infos[p_index];
}

metadb_handle_ptr file_info_update_helper::get_item_handle(t_size p_index) const
{
	return m_data[p_index];
}

void file_info_update_helper::invalidate_item(t_size p_index)
{
	m_mask[p_index] = false;
}
