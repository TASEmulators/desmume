class file_move_callback_manager
{
public:
	void on_library_add_items(const pfc::list_base_const_t<metadb_handle_ptr> & p_data);
	void on_library_remove_items(const pfc::list_base_const_t<metadb_handle_ptr> & p_data);

	void on_moved(const pfc::string_list_const & p_from,const pfc::string_list_const & p_to);
	void on_copied(const pfc::string_list_const & p_from,const pfc::string_list_const & p_to);

	void on_hint(const pfc::list_base_const_t<metadb_handle_ptr> & p_list,const pfc::list_base_const_t<const file_info*> & p_infos,const pfc::list_base_const_t<t_filestats> & p_stats);

	void run_callback();
private:
	metadb_handle_list m_removed;
	metadb_handle_list m_added;

	pfc::string_list_impl m_copy_from,m_copy_to;
	pfc::string_list_impl m_move_from,m_move_to;

	metadb_handle_list m_hint_handles;
	pfc::list_t<file_info_const_impl> m_hint_infos;
	pfc::list_t<t_filestats> m_hint_stats;
};

class file_move_helper
{
public:
	file_move_helper();
	~file_move_helper();
	file_move_helper(const file_move_helper & p_src);
	bool take_snapshot(const char * p_path,abort_callback & p_abort);
	bool on_moved(const char * p_path,abort_callback & p_abort);
	bool on_copied(const char * p_path,abort_callback & p_abort);
	bool on_moved(const char * p_path,abort_callback & p_abort,file_move_callback_manager & p_cb);
	bool on_copied(const char * p_path,abort_callback & p_abort,file_move_callback_manager & p_cb);
	
	static bool g_on_deleted(const pfc::list_base_const_t<const char *> & p_files);

	static t_size g_filter_dead_files_sorted_make_mask(pfc::list_base_t<metadb_handle_ptr> & p_data,const pfc::list_base_const_t<const char*> & p_dead,bit_array_var & p_mask);
	static t_size g_filter_dead_files_sorted(pfc::list_base_t<metadb_handle_ptr> & p_data,const pfc::list_base_const_t<const char*> & p_dead);
	static t_size g_filter_dead_files(pfc::list_base_t<metadb_handle_ptr> & p_data,const pfc::list_base_const_t<const char*> & p_dead);

private:

	struct t_entry
	{
		playable_location_impl m_location;
		file_info_impl m_info;
		t_filestats m_stats;
		bool m_have_info;
	};
	
	metadb_handle_list m_source_handles;

	pfc::array_t<t_entry> m_data;

	const char * get_source_path() const;

	void make_new_item_list(pfc::list_base_t<metadb_handle_ptr> & p_out,const char * p_path,file_move_callback_manager & p_cb);
};
