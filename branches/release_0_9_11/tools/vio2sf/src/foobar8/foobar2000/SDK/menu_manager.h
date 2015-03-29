#ifndef _FOOBAR2000_MENU_MANAGER_H_
#define _FOOBAR2000_MENU_MANAGER_H_

#include "menu_item.h"
#include "metadb_handle.h"



class NOVTABLE keyboard_shortcut_manager : public service_base
{
public:
	static keyboard_shortcut_manager * get() {return service_enum_create_t(keyboard_shortcut_manager,0);}

	enum shortcut_type
	{
		TYPE_MAIN,
		TYPE_CONTEXT,
		TYPE_CONTEXT_PLAYLIST,
		TYPE_CONTEXT_NOW_PLAYING,
	};


	virtual bool process_keydown(shortcut_type type,const ptr_list_base<metadb_handle> & data,unsigned keycode)=0;
	virtual bool process_keydown_ex(shortcut_type type,const ptr_list_base<metadb_handle> & data,unsigned keycode,const GUID & caller)=0;
//caller guid - see menu_item_v2, menu_item::caller_*
	bool on_keydown(shortcut_type type,WPARAM wp);
	bool on_keydown_context(const ptr_list_base<metadb_handle> & data,WPARAM wp,const GUID & caller);
	bool on_keydown_auto(WPARAM wp);
	bool on_keydown_auto_playlist(WPARAM wp);
	bool on_keydown_auto_context(const ptr_list_base<metadb_handle> & data,WPARAM wp,const GUID & caller);

	virtual bool get_key_description_for_action(const char * action_name, string_base & out, shortcut_type type, bool is_global)=0;

	
	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

/*
usage:

  in a windowproc:

	case WM_KEYDOWN:
		keyboard_shortcut_manager::get()->on_keydown(wparam);
		break;
	case WM_SYSKEYDOWN:
		keyboard_shortcut_manager::get()->on_keydown(wparam);
		break;

  return value is true if key got translated to one of user-configured actions, false if not
  */
};




class NOVTABLE menu_node
{
public:
	enum type
	{
		TYPE_POPUP,TYPE_COMMAND,TYPE_SEPARATOR
	};
	virtual type get_type()=0;
	virtual const char * get_name()=0;
	virtual unsigned get_num_children()=0;//TYPE_POPUP only
	virtual menu_node * get_child(unsigned n)=0;//TYPE_POPUP only
	virtual unsigned get_display_flags()=0;//TYPE_COMMAND/TYPE_POPUP only, see menu_item::FLAG_*
	virtual unsigned get_id()=0;//TYPE_COMMAND only, returns zero-based index (helpful for win32 menu command ids)
	virtual void execute()=0;//TYPE_COMMAND only
	virtual bool get_description(string_base & out)=0;//TYPE_COMMAND only
	virtual bool get_full_name(string_base & out)=0;//TYPE_COMMAND only
	virtual void * get_glyph()=0;//RESERVED, do not use
};



class NOVTABLE menu_manager : public service_base
{
public:
	enum
	{
		FLAG_SHOW_SHORTCUTS = 1,
		FLAG_SHOW_SHORTCUTS_GLOBAL = 2,
	};
	virtual void init_context(const ptr_list_base<metadb_handle> & data,unsigned flags)=0;//flags - see FLAG_* above
	virtual void init_context_playlist(unsigned flags)=0;
	virtual void init_main(const char * path,unsigned flags)=0;
	virtual menu_node * get_root()=0;//releasing menu_manager service releaases nodes; root may be null in case of error or something
	virtual menu_node * find_by_id(unsigned id)=0;
	virtual void set_shortcut_preference(const keyboard_shortcut_manager::shortcut_type * data,unsigned count)=0;
	

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static menu_manager * create() {return service_enum_create_t(menu_manager,0);}//you must service_release pointers you obtained

#ifdef WIN32
	static void win32_build_menu(HMENU menu,menu_node * parent,int base_id,int max_id);//menu item identifiers are base_id<=N<base_id+max_id (if theres too many items, they will be clipped)
	static void win32_run_menu_context(HWND parent,const metadb_handle_list & data, const POINT * pt = 0,unsigned flags = 0);
	static void win32_run_menu_context_playlist(HWND parent,const POINT * pt = 0,unsigned flags = 0);
	static void win32_run_menu_main(HWND parent,const char * path,const POINT * pt = 0,unsigned flags = 0);
	void win32_run_menu_popup(HWND parent,const POINT * pt = 0);
	void win32_build_menu(HMENU menu,int base_id,int max_id) {win32_build_menu(menu,get_root(),base_id,max_id);}
	static void win32_auto_mnemonics(HMENU menu);
	//virtual versions, calling them generates smaller code (you use implementation in core instead of statically linking to one from sdk)
	virtual void v_win32_build_menu(HMENU menu,int base_id,int max_id) {win32_build_menu(menu,base_id,max_id);}
	virtual void v_run_command(const char * name) {run_command(name);}
	virtual void v_run_command_context(const char * name,const ptr_list_base<metadb_handle> & list) {run_command_context(name,list);}
	virtual void v_run_command_context_playlist(const char * name) {run_command_context_playlist(name);}

	virtual void v_win32_auto_mnemonics(HMENU menu) {win32_auto_mnemonics(menu);}
#endif

	//new (0.8 beta3)
	virtual void init_context_ex(const ptr_list_base<metadb_handle> & data,unsigned flags,const GUID & caller)=0;
	virtual bool init_context_now_playing(unsigned flags)=0;//returns false if not playing
	virtual void init_main_ex(const char * path,unsigned flags,const GUID & caller)=0;

	bool execute_by_id(unsigned id);

	static bool run_command(const GUID & guid);
	static bool run_command_context(const GUID & guid,const ptr_list_base<metadb_handle> & data);
	static bool run_command_context_ex(const GUID & guid,const ptr_list_base<metadb_handle> & data,const GUID & caller);

	static bool run_command(const char * name);//returns false if not found
	static bool run_command_context(const char * name,const ptr_list_base<metadb_handle> & data);
	static bool run_command_context_playlist(const char * name);
	static bool run_command_context_ex(const char * name,const ptr_list_base<metadb_handle> & data,const GUID & caller);

	static bool test_command(const char * name);//checks if a command of this name exists
	static bool test_command_context(const char * name);

	static bool is_command_checked(const char * name);
	static bool is_command_checked(const GUID & guid);
	static bool is_command_checked_context(const char * name,metadb_handle_list & data);
	static bool is_command_checked_context(const GUID & guid,metadb_handle_list & data);
	static bool is_command_checked_context_playlist(const char * name);
	static bool is_command_checked_context_playlist(const GUID & guid);
	
	static bool get_description(menu_item::type type,const char * path,string_base & out);

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}


	

};

class NOVTABLE menu_manager_defaults_callback
{
public:
	virtual void set_path(const char * path)=0;
	virtual void add_command(const char * name)=0;//full name/path of a command, according to menu_item::enum_item, case-sensitive!
	virtual void add_separator()=0;
	virtual bool is_command_present(const char * name)=0;
};

class NOVTABLE menu_manager_defaults : public service_base
{
public:
	virtual GUID get_guid()=0;
	virtual menu_item::type get_type()=0;
	virtual void run(menu_manager_defaults_callback * callback)=0;

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};



class standard_commands
{
public:
	static const GUID
		guid_context_file_properties,	guid_context_file_open_directory,	guid_context_copy_names,
		guid_context_send_to_playlist,	guid_context_reload_info,			guid_context_reload_info_if_changed,
		guid_context_rewrite_info,		guid_context_remove_tags,			guid_context_remove_from_database,
		guid_context_convert_run,		guid_context_convert_run_singlefile,guid_context_write_cd,
		guid_context_rg_scan_track,		guid_context_rg_scan_album,			guid_context_rg_scan_album_multi,
		guid_context_rg_remove,			guid_context_save_playlist,			guid_context_masstag_edit,
		guid_context_masstag_rename,
		guid_main_always_on_top,		guid_main_preferences,				guid_main_about,
		guid_main_exit,					guid_main_restart,					guid_main_activate,
		guid_main_hide,					guid_main_activate_or_hide,			guid_main_titleformat_help,
		guid_main_follow_cursor,		guid_main_next,						guid_main_previous,
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
		guid_main_playlist_moveback,	guid_main_playlist_moveforward
		;

	static inline bool run_main(const GUID & guid) {return menu_manager::run_command(guid);}
	static inline bool run_context(const GUID & guid,const ptr_list_base<metadb_handle> &data) {return menu_manager::run_command_context(guid,data);}
	static inline bool run_context(const GUID & guid,const ptr_list_base<metadb_handle> &data,const GUID& caller) {return menu_manager::run_command_context_ex(guid,data,caller);}

	static inline bool context_file_properties(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_file_properties,data,caller);}
	static inline bool context_file_open_directory(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_file_open_directory,data,caller);}
	static inline bool context_copy_names(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_copy_names,data,caller);}
	static inline bool context_send_to_playlist(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_send_to_playlist,data,caller);}
	static inline bool context_reload_info(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_reload_info,data,caller);}
	static inline bool context_reload_info_if_changed(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_reload_info_if_changed,data,caller);}
	static inline bool context_rewrite_info(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_rewrite_info,data,caller);}
	static inline bool context_remove_tags(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_remove_tags,data,caller);}
	static inline bool context_remove_from_database(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_remove_from_database,data,caller);}
	static inline bool context_convert_run(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_convert_run,data,caller);}
	static inline bool context_convert_run_singlefile(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_convert_run_singlefile,data,caller);}
	static inline bool context_write_cd(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_write_cd,data,caller);}
	static inline bool context_rg_scan_track(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_rg_scan_track,data,caller);}
	static inline bool context_rg_scan_album(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_rg_scan_album,data,caller);}
	static inline bool context_rg_scan_album_multi(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_rg_scan_album_multi,data,caller);}
	static inline bool context_rg_remove(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_rg_remove,data,caller);}
	static inline bool context_save_playlist(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_save_playlist,data,caller);}
	static inline bool context_masstag_edit(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_masstag_edit,data,caller);}
	static inline bool context_masstag_rename(const ptr_list_base<metadb_handle> &data,const GUID& caller = menu_item::caller_undefined) {return run_context(guid_context_masstag_rename,data,caller);}
	static inline bool main_always_on_top() {return run_main(guid_main_always_on_top);}
	static inline bool main_preferences() {return run_main(guid_main_preferences);}
	static inline bool main_about() {return run_main(guid_main_about);}
	static inline bool main_exit() {return run_main(guid_main_exit);}
	static inline bool main_restart() {return run_main(guid_main_restart);}
	static inline bool main_activate() {return run_main(guid_main_activate);}
	static inline bool main_hide() {return run_main(guid_main_hide);}
	static inline bool main_activate_or_hide() {return run_main(guid_main_activate_or_hide);}
	static inline bool main_titleformat_help() {return run_main(guid_main_titleformat_help);}
	static inline bool main_follow_cursor() {return run_main(guid_main_follow_cursor);}
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
};

#endif //_FOOBAR2000_MENU_MANAGER_H_