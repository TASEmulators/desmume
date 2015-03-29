class NOVTABLE keyboard_shortcut_manager : public service_base
{
public:
	static bool g_get(service_ptr_t<keyboard_shortcut_manager> & p_out) {return service_enum_create_t(p_out,0);}

	enum shortcut_type
	{
		TYPE_MAIN,
		TYPE_CONTEXT,
		TYPE_CONTEXT_PLAYLIST,
		TYPE_CONTEXT_NOW_PLAYING,
	};


	virtual bool process_keydown(shortcut_type type,const pfc::list_base_const_t<metadb_handle_ptr> & data,unsigned keycode)=0;
	virtual bool process_keydown_ex(shortcut_type type,const pfc::list_base_const_t<metadb_handle_ptr> & data,unsigned keycode,const GUID & caller)=0;
	bool on_keydown(shortcut_type type,WPARAM wp);
	bool on_keydown_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,WPARAM wp,const GUID & caller);
	
	bool on_keydown_auto(WPARAM wp);
	bool on_keydown_auto_playlist(WPARAM wp);
	bool on_keydown_auto_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,WPARAM wp,const GUID & caller);

	bool on_keydown_restricted_auto(WPARAM wp);
	bool on_keydown_restricted_auto_playlist(WPARAM wp);
	bool on_keydown_restricted_auto_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,WPARAM wp,const GUID & caller);

	virtual bool get_key_description_for_action(const GUID & p_command,const GUID & p_subcommand, pfc::string_base & out, shortcut_type type, bool is_global)=0;

	static bool is_text_key(t_uint32 vkCode);
	static bool is_typing_key(t_uint32 vkCode);
	static bool is_typing_key_combo(t_uint32 vkCode, t_uint32 modifiers);
	static bool is_typing_modifier(t_uint32 flags);
	static bool is_typing_message(HWND editbox, const MSG * msg);
	static bool is_typing_message(const MSG * msg);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(keyboard_shortcut_manager);
};


//! New in 0.9.5.
class keyboard_shortcut_manager_v2 : public keyboard_shortcut_manager {
public:
	//! Deprecates old keyboard_shortcut_manager methods. If the action requires selected items, they're obtained from ui_selection_manager API automatically.
	virtual bool process_keydown_simple(t_uint32 keycode) = 0;

	//! Helper for use with message filters.
	bool pretranslate_message(const MSG * msg, HWND thisPopupWnd);

	FB2K_MAKE_SERVICE_INTERFACE(keyboard_shortcut_manager_v2,keyboard_shortcut_manager);
};

class NOVTABLE contextmenu_node {
public:
	virtual contextmenu_item_node::t_type get_type()=0;
	virtual const char * get_name()=0;
	virtual t_size get_num_children()=0;//TYPE_POPUP only
	virtual contextmenu_node * get_child(t_size n)=0;//TYPE_POPUP only
	virtual unsigned get_display_flags()=0;//TYPE_COMMAND/TYPE_POPUP only, see contextmenu_item::FLAG_*
	virtual unsigned get_id()=0;//TYPE_COMMAND only, returns zero-based index (helpful for win32 menu command ids)
	virtual void execute()=0;//TYPE_COMMAND only
	virtual bool get_description(pfc::string_base & out)=0;//TYPE_COMMAND only
	virtual bool get_full_name(pfc::string_base & out)=0;//TYPE_COMMAND only
	virtual void * get_glyph()=0;//RESERVED, do not use
protected:
	contextmenu_node() {}
	~contextmenu_node() {}
};



class NOVTABLE contextmenu_manager : public service_base
{
public:
	enum
	{
		FLAG_SHOW_SHORTCUTS = 1,
		FLAG_SHOW_SHORTCUTS_GLOBAL = 2,
	};
	virtual void init_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,unsigned flags)=0;//flags - see FLAG_* above
	virtual void init_context_playlist(unsigned flags)=0;
	virtual contextmenu_node * get_root()=0;//releasing contextmenu_manager service releaases nodes; root may be null in case of error or something
	virtual contextmenu_node * find_by_id(unsigned id)=0;
	virtual void set_shortcut_preference(const keyboard_shortcut_manager::shortcut_type * data,unsigned count)=0;
	


	static void g_create(service_ptr_t<contextmenu_manager> & p_out) {p_out = standard_api_create_t<contextmenu_manager>();}

#ifdef WIN32
	static void win32_build_menu(HMENU menu,contextmenu_node * parent,int base_id,int max_id);//menu item identifiers are base_id<=N<base_id+max_id (if theres too many items, they will be clipped)
	static void win32_run_menu_context(HWND parent,const pfc::list_base_const_t<metadb_handle_ptr> & data, const POINT * pt = 0,unsigned flags = 0);
	static void win32_run_menu_context_playlist(HWND parent,const POINT * pt = 0,unsigned flags = 0);
	void win32_run_menu_popup(HWND parent,const POINT * pt = 0);
	void win32_build_menu(HMENU menu,int base_id,int max_id) {win32_build_menu(menu,get_root(),base_id,max_id);}
	
	
#endif

	virtual void init_context_ex(const pfc::list_base_const_t<metadb_handle_ptr> & data,unsigned flags,const GUID & caller)=0;
	virtual bool init_context_now_playing(unsigned flags)=0;//returns false if not playing

	bool execute_by_id(unsigned id);

	bool get_description_by_id(unsigned id,pfc::string_base & out);


	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(contextmenu_manager);
};
