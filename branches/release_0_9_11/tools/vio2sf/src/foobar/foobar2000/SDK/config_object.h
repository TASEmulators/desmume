#ifndef _CONFIG_OBJECT_H_
#define _CONFIG_OBJECT_H_

class config_object;

class NOVTABLE config_object_notify_manager : public service_base
{
public:
	virtual void on_changed(const service_ptr_t<config_object> & p_object) = 0;
	static void g_on_changed(const service_ptr_t<config_object> & p_object);


	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(config_object_notify_manager);
};

class NOVTABLE config_object : public service_base
{
public:
	//interface
	virtual GUID get_guid() const = 0;
	virtual void get_data(stream_writer * p_stream,abort_callback & p_abort) const = 0;
	virtual void set_data(stream_reader * p_stream,abort_callback & p_abort,bool p_sendnotify = true) = 0;

	//helpers
	static bool g_find(service_ptr_t<config_object> & p_out,const GUID & p_guid);

	void set_data_raw(const void * p_data,t_size p_bytes,bool p_sendnotify = true);
	t_size get_data_raw(void * p_out,t_size p_bytes);
	t_size get_data_raw_length();

	template<class T> void get_data_struct_t(T& p_out);
	template<class T> void set_data_struct_t(const T& p_in);
	template<class T> static void g_get_data_struct_t(const GUID & p_guid,T & p_out);
	template<class T> static void g_set_data_struct_t(const GUID & p_guid,const T & p_in);

	void set_data_string(const char * p_data,t_size p_length);
	void get_data_string(pfc::string_base & p_out);
	
	void get_data_bool(bool & p_out);
	void set_data_bool(bool p_val);
	void get_data_int32(t_int32 & p_out);
	void set_data_int32(t_int32 p_val);
	bool get_data_bool_simple(bool p_default);
	t_int32 get_data_int32_simple(t_int32 p_default);

	static void g_get_data_string(const GUID & p_guid,pfc::string_base & p_out);
	static void g_set_data_string(const GUID & p_guid,const char * p_data,t_size p_length = ~0);

	static void g_get_data_bool(const GUID & p_guid,bool & p_out);
	static void g_set_data_bool(const GUID & p_guid,bool p_val);
	static void g_get_data_int32(const GUID & p_guid,t_int32 & p_out);
	static void g_set_data_int32(const GUID & p_guid,t_int32 p_val);
	static bool g_get_data_bool_simple(const GUID & p_guid,bool p_default);
	static t_int32 g_get_data_int32_simple(const GUID & p_guid,t_int32 p_default);

	
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(config_object);
};

class standard_config_objects
{
public:
	static const GUID bool_remember_window_positions, bool_ui_always_on_top,bool_playlist_stop_after_current;
	static const GUID bool_playback_follows_cursor, bool_cursor_follows_playback;
	static const GUID bool_show_keyboard_shortcuts_in_menus;
	static const GUID string_gui_last_directory_media,string_gui_last_directory_playlists;
	static const GUID int32_dynamic_bitrate_display_rate;


	inline static bool query_show_keyboard_shortcuts_in_menus() {return config_object::g_get_data_bool_simple(standard_config_objects::bool_show_keyboard_shortcuts_in_menus,true);}
	inline static bool query_remember_window_positions() {return config_object::g_get_data_bool_simple(standard_config_objects::bool_remember_window_positions,true);}
	
};

class config_object_notify : public service_base
{
public:
	virtual t_size get_watched_object_count() = 0;
	virtual GUID get_watched_object(t_size p_index) = 0;
	virtual void on_watched_object_changed(const service_ptr_t<config_object> & p_object) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(config_object_notify);
};

#endif _CONFIG_OBJECT_H_
