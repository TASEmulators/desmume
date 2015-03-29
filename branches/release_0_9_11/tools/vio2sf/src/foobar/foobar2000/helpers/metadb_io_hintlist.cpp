#include "stdafx.h"

void metadb_io_hintlist::run() {
	if (m_entries.get_count() > 0) {
		static_api_ptr_t<metadb_io>()->hint_multi_async(
			metadb_io_hintlist_wrapper_part1(m_entries),
			metadb_io_hintlist_wrapper_part2(m_entries),
			metadb_io_hintlist_wrapper_part3(m_entries),
			metadb_io_hintlist_wrapper_part4(m_entries)
			);
	}
	m_entries.remove_all();
}

void metadb_io_hintlist::add(metadb_handle_ptr const & p_handle,const file_info & p_info,t_filestats const & p_stats,bool p_fresh) {
	t_entry entry;
	entry.m_handle = p_handle;
	entry.m_info.new_t(p_info);
	entry.m_stats = p_stats;
	entry.m_fresh = p_fresh;
	m_entries.add_item(entry);
}

void metadb_io_hintlist::hint_reader(service_ptr_t<input_info_reader> p_reader, const char * p_path,abort_callback & p_abort) {
	static_api_ptr_t<metadb> api;
	const t_uint32 subsongcount = p_reader->get_subsong_count();
	t_filestats stats = p_reader->get_file_stats(p_abort);
	for(t_uint32 subsong = 0; subsong < subsongcount; subsong++) {
		t_uint32 subsong_id = p_reader->get_subsong(subsong);
		metadb_handle_ptr handle;
		api->handle_create(handle,make_playable_location(p_path,subsong_id));
		if (handle->should_reload(stats,true)) {
			file_info_impl temp;
			p_reader->get_info(subsong_id,temp,p_abort);

			add(handle,temp,stats,true);
		}
	}
}
