//! Reserved for future use.
typedef void * t_glyph;


class NOVTABLE contextmenu_item_node {
public:
	enum t_flags {
		FLAG_CHECKED = 1,
		FLAG_DISABLED = 2,
		FLAG_GRAYED = 4,
		FLAG_DISABLED_GRAYED = FLAG_DISABLED|FLAG_GRAYED,
		FLAG_RADIOCHECKED = 8, //new in 0.9.5.2 - overrides FLAG_CHECKED, set together with FLAG_CHECKED for backwards compatibility.
	};

	enum t_type {
		TYPE_POPUP,TYPE_COMMAND,TYPE_SEPARATOR
	};

	virtual bool get_display_data(pfc::string_base & p_out,unsigned & p_displayflags,metadb_handle_list_cref p_data,const GUID & p_caller) = 0;
	virtual t_type get_type() = 0;
	virtual void execute(metadb_handle_list_cref p_data,const GUID & p_caller) = 0;
	virtual t_glyph get_glyph(metadb_handle_list_cref p_data,const GUID & p_caller) {return 0;}//RESERVED
	virtual t_size get_children_count() = 0;
	virtual contextmenu_item_node * get_child(t_size p_index) = 0;
	virtual bool get_description(pfc::string_base & p_out) = 0;
	virtual GUID get_guid() = 0;
	virtual bool is_mappable_shortcut() = 0;

protected:
	contextmenu_item_node() {}
	~contextmenu_item_node() {}
};

class NOVTABLE contextmenu_item_node_root : public contextmenu_item_node
{
public:
	virtual ~contextmenu_item_node_root() {}
};

class NOVTABLE contextmenu_item_node_leaf : public contextmenu_item_node
{
public:
	t_type get_type() {return TYPE_COMMAND;}
	t_size get_children_count() {return 0;}
	contextmenu_item_node * get_child(t_size) {return NULL;}
};

class NOVTABLE contextmenu_item_node_root_leaf : public contextmenu_item_node_root
{
public:
	t_type get_type() {return TYPE_COMMAND;}
	t_size get_children_count() {return 0;}
	contextmenu_item_node * get_child(t_size) {return NULL;}
};

class NOVTABLE contextmenu_item_node_popup : public contextmenu_item_node
{
public:
	t_type get_type() {return TYPE_POPUP;}
	void execute(metadb_handle_list_cref data,const GUID & caller) {}
	bool get_description(pfc::string_base & p_out) {return false;}
};

class NOVTABLE contextmenu_item_node_root_popup : public contextmenu_item_node_root
{
public:
	t_type get_type() {return TYPE_POPUP;}
	void execute(metadb_handle_list_cref data,const GUID & caller) {}
	bool get_description(pfc::string_base & p_out) {return false;}
};

class contextmenu_item_node_separator : public contextmenu_item_node
{
public:
	t_type get_type() {return TYPE_SEPARATOR;}
	void execute(metadb_handle_list_cref data,const GUID & caller) {}
	bool get_description(pfc::string_base & p_out) {return false;}
	t_size get_children_count() {return 0;}
	bool get_display_data(pfc::string_base & p_out,unsigned & p_displayflags,metadb_handle_list_cref p_data,const GUID & p_caller) 
	{
		p_displayflags = 0;
		p_out = "---";
		return true;
	}
	contextmenu_item_node * get_child(t_size) {return NULL;}
	GUID get_guid() {return pfc::guid_null;}
	bool is_mappable_shortcut() {return false;}
};

/*!
Service class for declaring context menu commands.\n
See contextmenu_item_simple for implementation helper without dynamic menu generation features.\n
All methods are valid from main app thread only.
*/
class NOVTABLE contextmenu_item : public service_base {
public:
	enum t_enabled_state {
		FORCE_OFF,
		DEFAULT_OFF,
		DEFAULT_ON,
	};

	//! Retrieves number of menu items provided by this contextmenu_item implementation.
	virtual unsigned get_num_items() = 0;
	//! Instantiates a context menu item (including sub-node tree for items that contain dynamically-generated sub-items).
	virtual contextmenu_item_node_root * instantiate_item(unsigned p_index,metadb_handle_list_cref p_data,const GUID & p_caller) = 0;
	//! Retrieves GUID of the context menu item.
	virtual GUID get_item_guid(unsigned p_index) = 0;
	//! Retrieves human-readable name of the context menu item.
	virtual void get_item_name(unsigned p_index,pfc::string_base & p_out) = 0;
	//! Retrieves default path of the context menu item ("" for root).
	virtual void get_item_default_path(unsigned p_index,pfc::string_base & p_out) = 0;
	//! Retrieves item's description to show in the status bar. Set p_out to the string to be displayed and return true if you provide a description, return false otherwise.
	virtual bool get_item_description(unsigned p_index,pfc::string_base & p_out) = 0;
	//! Signals whether the item should be forcefully hidden (FORCE_OFF), hidden by default but possible to add (DEFAULT_OFF) or shown by default (DEFAULT_ON).
	virtual t_enabled_state get_enabled_state(unsigned p_index) = 0;
	//! Executes the menu item command without going thru the instantiate_item path. For items with dynamically-generated sub-items, p_node is identifies of the sub-item command to execute.
	virtual void item_execute_simple(unsigned p_index,const GUID & p_node,metadb_handle_list_cref p_data,const GUID & p_caller) = 0;

	bool item_get_display_data_root(pfc::string_base & p_out,unsigned & displayflags,unsigned p_index,metadb_handle_list_cref p_data,const GUID & p_caller);
	bool item_get_display_data(pfc::string_base & p_out,unsigned & displayflags,unsigned p_index,const GUID & p_node,metadb_handle_list_cref p_data,const GUID & p_caller);
	
	//! Deprecated - use caller_active_playlist_selection instead.
	static const GUID caller_playlist;

	static const GUID caller_active_playlist_selection, caller_active_playlist, caller_playlist_manager, caller_now_playing, caller_keyboard_shortcut_list, caller_media_library_viewer;
	static const GUID caller_undefined;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(contextmenu_item);
};

//! contextmenu_item implementation helper for implementing non-dynamically-generated context menu items; derive from this instead of from contextmenu_item directly if your menu items are static.
class NOVTABLE contextmenu_item_simple : public contextmenu_item {
private:
public:
	//! Same as contextmenu_item_node::t_flags.
	enum t_flags
	{
		FLAG_CHECKED = 1,
		FLAG_DISABLED = 2,
		FLAG_GRAYED = 4,
		FLAG_DISABLED_GRAYED = FLAG_DISABLED|FLAG_GRAYED,
		FLAG_RADIOCHECKED = 8, //new in 0.9.5.2 - overrides FLAG_CHECKED, set together with FLAG_CHECKED for backwards compatibility.
	};

	
	// Functions to be overridden by implementers (some are not mandatory).
	virtual t_enabled_state get_enabled_state(unsigned p_index) {return contextmenu_item::DEFAULT_ON;}
	virtual unsigned get_num_items() = 0;
	virtual void get_item_name(unsigned p_index,pfc::string_base & p_out) = 0;
	virtual void get_item_default_path(unsigned p_index,pfc::string_base & p_out) = 0;
	virtual void context_command(unsigned p_index,metadb_handle_list_cref p_data,const GUID& p_caller) = 0;
	virtual bool context_get_display(unsigned p_index,metadb_handle_list_cref p_data,pfc::string_base & p_out,unsigned & p_displayflags,const GUID & p_caller) {
		PFC_ASSERT(p_index>=0 && p_index<get_num_items());
		get_item_name(p_index,p_out);
		return true;
	}
	virtual GUID get_item_guid(unsigned p_index) = 0;
	virtual bool get_item_description(unsigned p_index,pfc::string_base & p_out) = 0;


private:
	class contextmenu_item_node_impl : public contextmenu_item_node_root_leaf {
	public:
		contextmenu_item_node_impl(contextmenu_item_simple * p_owner,unsigned p_index) : m_owner(p_owner), m_index(p_index) {}
		bool get_display_data(pfc::string_base & p_out,unsigned & p_displayflags,metadb_handle_list_cref p_data,const GUID & p_caller) {return m_owner->get_display_data(m_index,p_data,p_out,p_displayflags,p_caller);}
		void execute(metadb_handle_list_cref p_data,const GUID & p_caller) {m_owner->context_command(m_index,p_data,p_caller);}
		bool get_description(pfc::string_base & p_out) {return m_owner->get_item_description(m_index,p_out);}
		GUID get_guid() {return pfc::guid_null;}
		bool is_mappable_shortcut() {return m_owner->item_is_mappable_shortcut(m_index);}
	private:
		service_ptr_t<contextmenu_item_simple> m_owner;
		unsigned m_index;
	};

	contextmenu_item_node_root * instantiate_item(unsigned p_index,metadb_handle_list_cref p_data,const GUID & p_caller)
	{
		return new contextmenu_item_node_impl(this,p_index);
	}


	void item_execute_simple(unsigned p_index,const GUID & p_node,metadb_handle_list_cref p_data,const GUID & p_caller)
	{
		if (p_node == pfc::guid_null)
			context_command(p_index,p_data,p_caller);
	}

	virtual bool item_is_mappable_shortcut(unsigned p_index)
	{
		return true;
	}


	virtual bool get_display_data(unsigned n,metadb_handle_list_cref data,pfc::string_base & p_out,unsigned & displayflags,const GUID & caller)
	{
		bool rv = false;
		assert(n>=0 && n<get_num_items());
		if (data.get_count()>0)
		{
			rv = context_get_display(n,data,p_out,displayflags,caller);
		}
		return rv;
	}

};


//! Helper.
template<typename T>
class contextmenu_item_factory_t : public service_factory_single_t<T> {};


//! Helper.
#define DECLARE_CONTEXT_MENU_ITEM(P_CLASSNAME,P_NAME,P_DEFAULTPATH,P_FUNC,P_GUID,P_DESCRIPTION)	\
	namespace { \
		class P_CLASSNAME : public contextmenu_item_simple {	\
		public:	\
			unsigned get_num_items() {return 1;}	\
			void get_item_name(unsigned p_index,pfc::string_base & p_out) {p_out = P_NAME;}	\
			void get_item_default_path(unsigned p_index,pfc::string_base & p_out) {p_out = P_DEFAULTPATH;}	\
			void context_command(unsigned p_index,metadb_handle_list_cref p_data,const GUID& p_caller) {P_FUNC(p_data);}	\
			GUID get_item_guid(unsigned p_index) {return P_GUID;}	\
			bool get_item_description(unsigned p_index,pfc::string_base & p_out) {if (P_DESCRIPTION[0] == 0) return false;p_out = P_DESCRIPTION; return true;}	\
		};	\
		static contextmenu_item_factory_t<P_CLASSNAME> g_##P_CLASSNAME##_factory;	\
	}




//! New in 0.9.5.1. Static methods safe to use in prior versions as it will use slow fallback mode when the service isn't present. \n
//! Functionality provided by menu_item_resolver methods isn't much different from just walking all registered contextmenu_item / mainmenu_commands implementations to find the command we want, but it uses a hint map to locate the service we're looking for without walking all of them which may be significantly faster in certain scenarios.
class menu_item_resolver : public service_base {
public:
	virtual bool resolve_context_command(const GUID & id, service_ptr_t<class contextmenu_item> & out, t_uint32 & out_index) = 0;
	virtual bool resolve_main_command(const GUID & id, service_ptr_t<class mainmenu_commands> & out, t_uint32 & out_index) = 0;

	static bool g_resolve_context_command(const GUID & id, service_ptr_t<class contextmenu_item> & out, t_uint32 & out_index);
	static bool g_resolve_main_command(const GUID & id, service_ptr_t<class mainmenu_commands> & out, t_uint32 & out_index);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(menu_item_resolver)
};
