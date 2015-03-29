//! Deprecated - relies on old blocking metadb_io methods - use metadb_io_v2::update_info_async w/ file_info_filter whenever possible.
class __declspec(deprecated("Use metadb_io_v2::update_info_async instead whenever possible.")) file_info_update_helper {
public:
	file_info_update_helper(const pfc::list_base_const_t<metadb_handle_ptr> & p_data);
	file_info_update_helper(metadb_handle_ptr p_item);

	bool read_infos(HWND p_parent,bool p_show_errors);

	enum t_write_result
	{
		write_ok,
		write_aborted,
		write_error
	};
	t_write_result write_infos(HWND p_parent,bool p_show_errors);

	t_size get_item_count() const;
	bool is_item_valid(t_size p_index) const;//returns false where info reading failed
	
	file_info & get_item(t_size p_index);
	metadb_handle_ptr get_item_handle(t_size p_index) const;

	void invalidate_item(t_size p_index);

private:
	metadb_handle_list m_data;
	pfc::array_t<file_info_impl> m_infos;
	pfc::array_t<bool> m_mask;
};
