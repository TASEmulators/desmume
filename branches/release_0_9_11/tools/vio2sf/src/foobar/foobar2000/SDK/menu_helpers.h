namespace menu_helpers {
#ifdef _WIN32
	void win32_auto_mnemonics(HMENU menu);
#endif

	bool run_command_context(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data);
	bool run_command_context_ex(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data,const GUID & caller);
	bool run_command_context_playlist(const GUID & p_command,const GUID & p_subcommand);
	bool run_command_context_now_playing(const GUID & p_command,const GUID & p_subcommand);

	bool test_command_context(const GUID & p_guid);

	bool is_command_checked_context(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data);
	bool is_command_checked_context_playlist(const GUID & p_command,const GUID & p_subcommand);

	bool find_command_by_name(const char * p_name,service_ptr_t<contextmenu_item> & p_item,unsigned & p_index);
	bool find_command_by_name(const char * p_name,GUID & p_command);
	
	bool context_get_description(const GUID& p_guid,pfc::string_base & out);

	bool guid_from_name(const char * p_name,unsigned p_name_len,GUID & p_out);
	bool name_from_guid(const GUID & p_guid,pfc::string_base & p_out);

	class guid_to_name_table
	{
	public:
		guid_to_name_table();
		~guid_to_name_table();
		const char * search(const GUID & p_guid);
	private:
		struct entry {
			char* m_name;
			GUID m_guid;
		};
		pfc::array_t<entry> m_data;
		bool m_inited;

		static int entry_compare_search(const entry & entry1,const GUID & entry2);
		static int entry_compare(const entry & entry1,const entry & entry2);
	};

	class name_to_guid_table
	{
	public:
		name_to_guid_table();
		~name_to_guid_table();
		bool search(const char * p_name,unsigned p_name_len,GUID & p_out);
	private:
		struct entry {
			char* m_name;
			GUID m_guid;
		};
		pfc::array_t<entry> m_data;
		bool m_inited;
		struct search_entry {
			const char * m_name; unsigned m_name_len;
		};

		static int entry_compare_search(const entry & entry1,const search_entry & entry2);
		static int entry_compare(const entry & entry1,const entry & entry2);
	};

};



class standard_commands
{
public:
	static const GUID
		guid_context_file_properties,	guid_context_file_open_directory,	guid_context_copy_names,
		guid_context_send_to_playlist,	guid_context_reload_info,			guid_context_reload_info_if_changed,
		guid_context_rewrite_info,		guid_context_remove_tags,
		guid_context_convert_run,		guid_context_convert_run_singlefile,guid_context_convert_run_withcue,
		guid_context_write_cd,
		guid_context_rg_scan_track,		guid_context_rg_scan_album,			guid_context_rg_scan_album_multi,
		guid_context_rg_remove,			guid_context_save_playlist,			guid_context_masstag_edit,
		guid_context_masstag_rename,
		guid_main_always_on_top,		guid_main_preferences,				guid_main_about,
		guid_main_exit,					guid_main_restart,					guid_main_activate,
		guid_main_hide,					guid_main_activate_or_hide,			guid_main_titleformat_help,
		guid_main_next,					guid_main_previous,
		guid_main_next_or_random,		guid_main_random,					guid_main_pause,
		guid_main_play,					guid_main_play_or_pause,			guid_main_rg_set_album,
		guid_main_rg_set_track,			guid_main_rg_disable,				guid_main_stop,
		guid_main_stop_after_current,	guid_main_volume_down,				guid_main_volume_up,
		guid_main_volume_mute,			guid_main_add_directory,			guid_main_add_files,
		guid_main_add_location,			guid_main_add_playlist,				guid_main_clear_playlist,
		guid_main_create_playlist,		guid_main_highlight_playing,		guid_main_load_playlist,
		guid_main_next_playlist,		guid_main_previous_playlist,		guid_main_open,
		guid_main_remove_playlist,		guid_main_remove_dead_entries,		guid_main_remove_duplicates,
		guid_main_rename_playlist,		guid_main_save_all_playlists,		guid_main_save_playlist,
		guid_main_playlist_search,		guid_main_playlist_sel_crop,		guid_main_playlist_sel_remove,
		guid_main_playlist_sel_invert,	guid_main_playlist_undo,			guid_main_show_console,
		guid_main_play_cd,				guid_main_restart_resetconfig,		guid_main_record,
		guid_main_playlist_moveback,	guid_main_playlist_moveforward,		guid_main_playlist_redo,
		guid_main_playback_follows_cursor,	guid_main_cursor_follows_playback, guid_main_saveconfig,
		guid_main_playlist_select_all,	guid_main_show_now_playing,

		guid_seek_ahead_1s,				guid_seek_ahead_5s,					guid_seek_ahead_10s,				guid_seek_ahead_30s,
		guid_seek_ahead_1min,			guid_seek_ahead_2min,				guid_seek_ahead_5min,				guid_seek_ahead_10min,

		guid_seek_back_1s,				guid_seek_back_5s,					guid_seek_back_10s,					guid_seek_back_30s,
		guid_seek_back_1min,			guid_seek_back_2min,				guid_seek_back_5min,				guid_seek_back_10min
		;

	static bool run_main(const GUID & guid);
	static inline bool run_context(const GUID & guid,const pfc::list_base_const_t<metadb_handle_ptr> &data) {return menu_helpers::run_command_context(guid,pfc::guid_null,data);}
	static inline bool run_context(const GUID & guid,const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller) {return menu_helpers::run_command_context_ex(guid,pfc::guid_null,data,caller);}

	static inline bool context_file_properties(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_file_properties,data,caller);}
	static inline bool context_file_open_directory(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_file_open_directory,data,caller);}
	static inline bool context_copy_names(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_copy_names,data,caller);}
	static inline bool context_send_to_playlist(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_send_to_playlist,data,caller);}
	static inline bool context_reload_info(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_reload_info,data,caller);}
	static inline bool context_reload_info_if_changed(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_reload_info_if_changed,data,caller);}
	static inline bool context_rewrite_info(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_rewrite_info,data,caller);}
	static inline bool context_remove_tags(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_remove_tags,data,caller);}
	static inline bool context_convert_run(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_convert_run,data,caller);}
	static inline bool context_convert_run_singlefile(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_convert_run_singlefile,data,caller);}
	static inline bool context_write_cd(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_write_cd,data,caller);}
	static inline bool context_rg_scan_track(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_rg_scan_track,data,caller);}
	static inline bool context_rg_scan_album(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_rg_scan_album,data,caller);}
	static inline bool context_rg_scan_album_multi(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_rg_scan_album_multi,data,caller);}
	static inline bool context_rg_remove(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_rg_remove,data,caller);}
	static inline bool context_save_playlist(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_save_playlist,data,caller);}
	static inline bool context_masstag_edit(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_masstag_edit,data,caller);}
	static inline bool context_masstag_rename(const pfc::list_base_const_t<metadb_handle_ptr> &data,const GUID& caller = contextmenu_item::caller_undefined) {return run_context(guid_context_masstag_rename,data,caller);}
	static inline bool main_always_on_top() {return run_main(guid_main_always_on_top);}
	static inline bool main_preferences() {return run_main(guid_main_preferences);}
	static inline bool main_about() {return run_main(guid_main_about);}
	static inline bool main_exit() {return run_main(guid_main_exit);}
	static inline bool main_restart() {return run_main(guid_main_restart);}
	static inline bool main_activate() {return run_main(guid_main_activate);}
	static inline bool main_hide() {return run_main(guid_main_hide);}
	static inline bool main_activate_or_hide() {return run_main(guid_main_activate_or_hide);}
	static inline bool main_titleformat_help() {return run_main(guid_main_titleformat_help);}
	static inline bool main_playback_follows_cursor() {return run_main(guid_main_playback_follows_cursor);}
	static inline bool main_next() {return run_main(guid_main_next);}
	static inline bool main_previous() {return run_main(guid_main_previous);}
	static inline bool main_next_or_random() {return run_main(guid_main_next_or_random);}
	static inline bool main_random() {return run_main(guid_main_random);}
	static inline bool main_pause() {return run_main(guid_main_pause);}
	static inline bool main_play() {return run_main(guid_main_play);}
	static inline bool main_play_or_pause() {return run_main(guid_main_play_or_pause);}
	static inline bool main_rg_set_album() {return run_main(guid_main_rg_set_album);}
	static inline bool main_rg_set_track() {return run_main(guid_main_rg_set_track);}
	static inline bool main_rg_disable() {return run_main(guid_main_rg_disable);}
	static inline bool main_stop() {return run_main(guid_main_stop);}
	static inline bool main_stop_after_current() {return run_main(guid_main_stop_after_current);}
	static inline bool main_volume_down() {return run_main(guid_main_volume_down);}
	static inline bool main_volume_up() {return run_main(guid_main_volume_up);}
	static inline bool main_volume_mute() {return run_main(guid_main_volume_mute);}
	static inline bool main_add_directory() {return run_main(guid_main_add_directory);}
	static inline bool main_add_files() {return run_main(guid_main_add_files);}
	static inline bool main_add_location() {return run_main(guid_main_add_location);}
	static inline bool main_add_playlist() {return run_main(guid_main_add_playlist);}
	static inline bool main_clear_playlist() {return run_main(guid_main_clear_playlist);}
	static inline bool main_create_playlist() {return run_main(guid_main_create_playlist);}
	static inline bool main_highlight_playing() {return run_main(guid_main_highlight_playing);}
	static inline bool main_load_playlist() {return run_main(guid_main_load_playlist);}
	static inline bool main_next_playlist() {return run_main(guid_main_next_playlist);}
	static inline bool main_previous_playlist() {return run_main(guid_main_previous_playlist);}
	static inline bool main_open() {return run_main(guid_main_open);}
	static inline bool main_remove_playlist() {return run_main(guid_main_remove_playlist);}
	static inline bool main_remove_dead_entries() {return run_main(guid_main_remove_dead_entries);}
	static inline bool main_remove_duplicates() {return run_main(guid_main_remove_duplicates);}
	static inline bool main_rename_playlist() {return run_main(guid_main_rename_playlist);}
	static inline bool main_save_all_playlists() {return run_main(guid_main_save_all_playlists);}
	static inline bool main_save_playlist() {return run_main(guid_main_save_playlist);}
	static inline bool main_playlist_search() {return run_main(guid_main_playlist_search);}
	static inline bool main_playlist_sel_crop() {return run_main(guid_main_playlist_sel_crop);}
	static inline bool main_playlist_sel_remove() {return run_main(guid_main_playlist_sel_remove);}
	static inline bool main_playlist_sel_invert() {return run_main(guid_main_playlist_sel_invert);}
	static inline bool main_playlist_undo() {return run_main(guid_main_playlist_undo);}
	static inline bool main_show_console() {return run_main(guid_main_show_console);}
	static inline bool main_play_cd() {return run_main(guid_main_play_cd);}
	static inline bool main_restart_resetconfig() {return run_main(guid_main_restart_resetconfig);}
	static inline bool main_playlist_moveback() {return run_main(guid_main_playlist_moveback);}
	static inline bool main_playlist_moveforward() {return run_main(guid_main_playlist_moveforward);}
	static inline bool main_saveconfig() {return run_main(guid_main_saveconfig);}
};
