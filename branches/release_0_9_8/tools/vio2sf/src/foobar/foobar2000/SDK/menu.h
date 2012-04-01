class NOVTABLE mainmenu_group : public service_base {
public:
	virtual GUID get_guid() = 0;
	virtual GUID get_parent() = 0;
	virtual t_uint32 get_sort_priority() = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(mainmenu_group);
};

class NOVTABLE mainmenu_group_popup : public mainmenu_group {
public:
	virtual void get_display_string(pfc::string_base & p_out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(mainmenu_group_popup,mainmenu_group);
};

class NOVTABLE mainmenu_commands : public service_base {
public:
	enum {
		flag_disabled = 1<<0,
		flag_checked =	1<<1,
		flag_radiochecked = 1<<2,
		sort_priority_base = 0x10000,
		sort_priority_dontcare = 0x80000000,
		sort_priority_last = infinite,
	};

	//! Retrieves number of implemented commands. Index parameter of other methods must be in 0....command_count-1 range.
	virtual t_uint32 get_command_count() = 0;
	//! Retrieves GUID of specified command.
	virtual GUID get_command(t_uint32 p_index) = 0;
	//! Retrieves name of item, for list of commands to assign keyboard shortcuts to etc.
	virtual void get_name(t_uint32 p_index,pfc::string_base & p_out) = 0;
	//! Retrieves item's description for statusbar etc.
	virtual bool get_description(t_uint32 p_index,pfc::string_base & p_out) = 0;
	//! Retrieves GUID of owning menu group.
	virtual GUID get_parent() = 0;
	//! Retrieves sorting priority of the command; the lower the number, the upper in the menu your commands will appear. Third party components should use sorting_priority_base and up (values below are reserved for internal use). In case of equal priority, order is undefined.
	virtual t_uint32 get_sort_priority() {return sort_priority_dontcare;}
	//! Retrieves display string and display flags to use when menu is about to be displayed. If returns false, menu item won't be displayed. You can create keyboard-shortcut-only commands by always returning false from get_display().
	virtual bool get_display(t_uint32 p_index,pfc::string_base & p_text,t_uint32 & p_flags) {p_flags = 0;get_name(p_index,p_text);return true;}
	//! Executes the command. p_callback parameter is reserved for future use and should be ignored / set to null pointer.
	virtual void execute(t_uint32 p_index,service_ptr_t<service_base> p_callback) = 0;

	static bool g_execute(const GUID & p_guid,service_ptr_t<service_base> p_callback = service_ptr_t<service_base>());
	static bool g_find_by_name(const char * p_name,GUID & p_guid);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(mainmenu_commands);
};

class NOVTABLE mainmenu_manager : public service_base {
public:
	enum {
		flag_show_shortcuts = 1 << 0,
		flag_show_shortcuts_global = 1 << 1,
	};

	virtual void instantiate(const GUID & p_root) = 0;
	
#ifdef _WIN32
	virtual void generate_menu_win32(HMENU p_menu,t_uint32 p_id_base,t_uint32 p_id_count,t_uint32 p_flags) = 0;
#else
#error portme
#endif
	//@param p_id Identifier of command to execute, relative to p_id_base of generate_menu_*()
	//@returns true if command was executed successfully, false if not (e.g. command with given identifier not found).
	virtual bool execute_command(t_uint32 p_id,service_ptr_t<service_base> p_callback = service_ptr_t<service_base>()) = 0;

	virtual bool get_description(t_uint32 p_id,pfc::string_base & p_out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(mainmenu_manager);
};

class mainmenu_groups {
public:
	static const GUID file,view,edit,playback,library,help;
	static const GUID file_open,file_add,file_playlist,file_etc;
	static const GUID playback_controls,playback_etc;
	static const GUID view_visualisations, view_alwaysontop;
	static const GUID edit_part1,edit_part2,edit_part3;
	static const GUID edit_part2_selection,edit_part2_sort,edit_part2_selection_sort;
	static const GUID file_etc_preferences, file_etc_exit;
	static const GUID help_about;
	static const GUID library_refresh;

	enum {priority_edit_part1,priority_edit_part2,priority_edit_part3};
	enum {priority_edit_part2_commands,priority_edit_part2_selection,priority_edit_part2_sort};
	enum {priority_edit_part2_selection_commands,priority_edit_part2_selection_sort};
	enum {priority_file_open,priority_file_add,priority_file_playlist,priority_file_etc = mainmenu_commands::sort_priority_last};
};


class mainmenu_group_impl : public mainmenu_group {
public:
	mainmenu_group_impl(const GUID & p_guid,const GUID & p_parent,t_uint32 p_priority) : m_guid(p_guid), m_parent(p_parent), m_priority(p_priority) {}
	GUID get_guid() {return m_guid;}
	GUID get_parent() {return m_parent;}
	t_uint32 get_sort_priority() {return m_priority;}
private:
	GUID m_guid,m_parent; t_uint32 m_priority;
};

class mainmenu_group_popup_impl : public mainmenu_group_popup {
public:
	mainmenu_group_popup_impl(const GUID & p_guid,const GUID & p_parent,t_uint32 p_priority,const char * p_name) : m_guid(p_guid), m_parent(p_parent), m_priority(p_priority), m_name(p_name) {}
	GUID get_guid() {return m_guid;}
	GUID get_parent() {return m_parent;}
	t_uint32 get_sort_priority() {return m_priority;}
	void get_display_string(pfc::string_base & p_out) {p_out = m_name;}
private:
	GUID m_guid,m_parent; t_uint32 m_priority; pfc::string8 m_name;
};

typedef service_factory_single_t<mainmenu_group_impl> __mainmenu_group_factory;
typedef service_factory_single_t<mainmenu_group_popup_impl> __mainmenu_group_popup_factory;

class mainmenu_group_factory : public __mainmenu_group_factory {
public:
	inline mainmenu_group_factory(const GUID & p_guid,const GUID & p_parent,t_uint32 p_priority) : __mainmenu_group_factory(p_guid,p_parent,p_priority) {}
};

class mainmenu_group_popup_factory : public __mainmenu_group_popup_factory {
public:
	inline mainmenu_group_popup_factory(const GUID & p_guid,const GUID & p_parent,t_uint32 p_priority,const char * p_name) : __mainmenu_group_popup_factory(p_guid,p_parent,p_priority,p_name) {}
};

template<typename T>
class mainmenu_commands_factory_t : public service_factory_single_t<T> {};
