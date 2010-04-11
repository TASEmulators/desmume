#include "foobar2000.h"

#ifdef WIN32

static void fix_ampersand(const char * src,string_base & out)
{
	unsigned ptr = 0;
	while(src[ptr])
	{
		if (src[ptr]=='&')
		{
			out.add_string("&&");
			ptr++;
			while(src[ptr]=='&')
			{
				out.add_string("&&");
				ptr++;
			}
		}
		else out.add_byte(src[ptr++]);
	}
}

static unsigned flags_to_win32(unsigned flags)
{
	unsigned ret = 0;
	if (flags & menu_item::FLAG_CHECKED) ret |= MF_CHECKED;
	if (flags & menu_item::FLAG_DISABLED) ret |= MF_DISABLED;
	if (flags & menu_item::FLAG_GRAYED) ret |= MF_GRAYED;
	return ret;
}

void menu_manager::win32_build_menu(HMENU menu,menu_node * parent,int base_id,int max_id)//menu item identifiers are base_id<=N<base_id+max_id (if theres too many items, they will be clipped)
{
	if (parent!=0 && parent->get_type()==menu_node::TYPE_POPUP)
	{
		string8_fastalloc temp;
		int child_idx,child_num = parent->get_num_children();
		for(child_idx=0;child_idx<child_num;child_idx++)
		{
			menu_node * child = parent->get_child(child_idx);
			if (child)
			{
				const char * name = child->get_name();
				if (strchr(name,'&')) {fix_ampersand(name,temp);name=temp;}
				menu_node::type type = child->get_type();
				if (type==menu_node::TYPE_POPUP)
				{
					HMENU new_menu = CreatePopupMenu();
					uAppendMenu(menu,MF_STRING|MF_POPUP | flags_to_win32(child->get_display_flags()),(UINT)new_menu,name);
					win32_build_menu(new_menu,child,base_id,max_id);
				}
				else if (type==menu_node::TYPE_SEPARATOR)
				{
					uAppendMenu(menu,MF_SEPARATOR,0,0);
				}
				else if (type==menu_node::TYPE_COMMAND)
				{
					int id = child->get_id();
					if (id>=0 && (max_id<0 || id<max_id))
					{
						uAppendMenu(menu,MF_STRING | flags_to_win32(child->get_display_flags()),base_id+id,name);
					}
				}
			}
		}
	}
}

#endif

static bool run_command_internal(menu_item::type type,const char * name,const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	service_enum_t<menu_item> e;
	menu_item * ptr;
	bool done = false;
	string8_fastalloc action_name;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_type()==type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				action_name.reset();
				ptr->enum_item(action,action_name);
				if (!stricmp_utf8(name,action_name))
				{
					TRACK_CALL_TEXT(uStringPrintf("menu_manager::run_command()/\"%s\"",action_name.get_ptr()));
					ptr->perform_command(action,data,caller);
					done = true;
					break;
				}
			}
		}
		ptr->service_release();
		if (done) break;
	}
	return done;
}

static bool run_command_internal(menu_item::type type,const GUID & guid,const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	service_enum_t<menu_item> e;
	menu_item * ptr;
	bool done = false;
	GUID action_guid;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_type()==type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				
				if (ptr->enum_item_guid(action,action_guid))
				{
					if (action_guid == guid)
					{
						TRACK_CALL_TEXT("menu_manager::run_command(), by GUID");
						ptr->perform_command(action,data,caller);
						done = true;
						break;
					}
				}
			}
		}
		ptr->service_release();
		if (done) break;
	}
	return done;
}

bool menu_manager::run_command(const GUID & guid)
{
	return run_command_internal(menu_item::TYPE_MAIN,guid,ptr_list_t<metadb_handle>(),menu_item::caller_undefined);
}

bool menu_manager::run_command_context(const GUID & guid,const ptr_list_base<metadb_handle> & data)
{
	return run_command_internal(menu_item::TYPE_CONTEXT,guid,data,menu_item::caller_undefined);
}

bool menu_manager::run_command_context_ex(const GUID & guid,const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	return run_command_internal(menu_item::TYPE_CONTEXT,guid,data,caller);
}


bool menu_manager::run_command(const char * name)
{
	return run_command_internal(menu_item::TYPE_MAIN,name,ptr_list_t<metadb_handle>(),menu_item::caller_undefined);
}

bool menu_manager::run_command_context_ex(const char * name,const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	return run_command_internal(menu_item::TYPE_CONTEXT,name,data,caller);
}

bool menu_manager::run_command_context(const char * name,const ptr_list_base<metadb_handle> &data)
{
	return run_command_internal(menu_item::TYPE_CONTEXT,name,data,menu_item::caller_undefined);
}

bool menu_manager::run_command_context_playlist(const char * name)
{
	metadb_handle_list temp;
	playlist_oper::get()->get_sel_items(temp);
	bool rv = run_command_context_ex(name,temp,menu_item::caller_playlist);
	temp.delete_all();
	return rv;
}


static bool g_test_command(const char * name,menu_item::type m_type)
{
	service_enum_t<menu_item> e;
	menu_item * ptr;
	bool done = false;
	string8_fastalloc action_name;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_type()==m_type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				action_name.reset();
				ptr->enum_item(action,action_name);
				if (!stricmp_utf8(name,action_name))
				{
					done = true;
					break;
				}
			}
		}
		ptr->service_release();
		if (done) break;
	}
	return done;
}

bool menu_manager::test_command(const char * name)
{
	return g_test_command(name,menu_item::TYPE_MAIN);
}

bool menu_manager::test_command_context(const char * name)
{
	return g_test_command(name,menu_item::TYPE_CONTEXT);
}

static bool g_is_checked(const char * name,menu_item::type type, const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	service_enum_t<menu_item> e;
	menu_item * ptr;
	bool done = false, rv = false;
	string8_fastalloc dummystring,action_name;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_type()==type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				action_name.reset();
				ptr->enum_item(action,action_name);
				if (!stricmp_utf8(name,action_name))
				{
					unsigned displayflags = 0;
					if (ptr->get_display_data(action,data,dummystring,displayflags,caller))
					{
						rv = !!(displayflags & menu_item::FLAG_CHECKED);
						done = true;
						break;
					}
				}
			}
		}
		ptr->service_release();
		if (done) break;
	}
	return rv;
}

static bool g_is_checked(const GUID & guid,menu_item::type type, const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	service_enum_t<menu_item> e;
	menu_item * ptr;
	bool done = false, rv = false;
	string8_fastalloc dummystring;
	GUID action_guid;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_type()==type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				if (ptr->enum_item_guid(action,action_guid))
				{
					if (action_guid == guid)
					{
						unsigned displayflags = 0;
						if (ptr->get_display_data(action,data,dummystring,displayflags,caller))
						{
							rv = !!(displayflags & menu_item::FLAG_CHECKED);
							done = true;
							break;
						}
					}
				}
			}
		}
		ptr->service_release();
		if (done) break;
	}
	return rv;
}

bool menu_manager::is_command_checked(const GUID & guid)
{
	return g_is_checked(guid,menu_item::TYPE_MAIN,ptr_list_t<metadb_handle>(),menu_item::caller_undefined);
}

bool menu_manager::is_command_checked(const char * name)
{
	return g_is_checked(name,menu_item::TYPE_MAIN,ptr_list_t<metadb_handle>(),menu_item::caller_undefined);
}

bool menu_manager::is_command_checked_context(const GUID & guid,metadb_handle_list & data)
{
	return g_is_checked(guid,menu_item::TYPE_MAIN,data,menu_item::caller_undefined);
}

bool menu_manager::is_command_checked_context(const char * name,metadb_handle_list & data)
{
	return g_is_checked(name,menu_item::TYPE_MAIN,data,menu_item::caller_undefined);
}

bool menu_manager::is_command_checked_context_playlist(const GUID & guid)
{
	bool rv;
	metadb_handle_list temp;
	playlist_oper::get()->get_data(temp);
	rv = g_is_checked(guid,menu_item::TYPE_MAIN,temp,menu_item::caller_playlist);
	temp.delete_all();
	return rv;
}

bool menu_manager::is_command_checked_context_playlist(const char * name)
{
	bool rv;
	metadb_handle_list temp;
	playlist_oper::get()->get_data(temp);
	rv = g_is_checked(name,menu_item::TYPE_MAIN,temp,menu_item::caller_playlist);
	temp.delete_all();
	return rv;
}

bool menu_manager::execute_by_id(unsigned id)
{
	bool rv = false;
	menu_node * ptr = find_by_id(id);
	if (ptr) {rv=true;ptr->execute();}
	return rv;
}

#ifdef WIN32

void menu_manager::win32_run_menu_popup(HWND parent,const POINT * pt)
{
	enum {ID_CUSTOM_BASE = 1};

	int cmd;

	{
		POINT p;
		if (pt) p = *pt;
		else GetCursorPos(&p);
		HMENU hmenu = CreatePopupMenu();

		v_win32_build_menu(hmenu,ID_CUSTOM_BASE,-1);
		v_win32_auto_mnemonics(hmenu);

		cmd = TrackPopupMenu(hmenu,TPM_RIGHTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,p.x,p.y,0,parent,0);
		
		DestroyMenu(hmenu);
	}			

	if (cmd>0)
	{
		if (cmd>=ID_CUSTOM_BASE)
		{
			execute_by_id(cmd - ID_CUSTOM_BASE);
		}
	}
}

void menu_manager::win32_run_menu_context(HWND parent,const metadb_handle_list & data,const POINT * pt,unsigned flags)
{
	menu_manager * p_manager = menu_manager::create();
	p_manager->init_context(data,flags);
	p_manager->win32_run_menu_popup(parent,pt);
	p_manager->service_release();
}

void menu_manager::win32_run_menu_context_playlist(HWND parent,const POINT * pt,unsigned flags)
{
	menu_manager * p_manager = menu_manager::create();
	p_manager->init_context_playlist(flags);
	p_manager->win32_run_menu_popup(parent,pt);
	p_manager->service_release();
}

void menu_manager::win32_run_menu_main(HWND parent,const char * path,const POINT * pt,unsigned flags)
{
	menu_manager * p_manager = menu_manager::create();
	p_manager->init_main(path,flags);
	p_manager->win32_run_menu_popup(parent,pt);
	p_manager->service_release();
}

namespace {
	class mnemonic_manager
	{
		string8_fastalloc used;
		bool is_used(unsigned c)
		{
			char temp[8];
			temp[utf8_encode_char(char_lower(c),temp)]=0;
			return !!strstr(used,temp);
		}

		static bool is_alphanumeric(char c)
		{
			return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9');
		}




		void insert(const char * src,unsigned idx,string_base & out)
		{
			out.reset();
			out.add_string(src,idx);
			out.add_string("&");
			out.add_string(src+idx);
			used.add_char(char_lower(src[idx]));
		}
	public:
		bool check_string(const char * src)
		{//check for existing mnemonics
			const char * ptr = src;
			while(ptr = strchr(ptr,'&'))
			{
				if (ptr[1]=='&') ptr+=2;
				else
				{
					unsigned c = 0;
					if (utf8_decode_char(ptr+1,&c)>0)
					{
						if (!is_used(c)) used.add_char(char_lower(c));
					}
					return true;
				}
			}
			return false;
		}
		bool process_string(const char * src,string_base & out)//returns if changed
		{
			if (check_string(src)) {out=src;return false;}
			unsigned idx=0;
			while(src[idx]==' ') idx++;
			while(src[idx])
			{
				if (is_alphanumeric(src[idx]) && !is_used(src[idx]))
				{
					insert(src,idx,out);
					return true;
				}

				while(src[idx] && src[idx]!=' ' && src[idx]!='\t') idx++;
				if (src[idx]=='\t') break;
				while(src[idx]==' ') idx++;
			}

			//no success picking first letter of one of words
			idx=0;
			while(src[idx])
			{
				if (src[idx]=='\t') break;
				if (is_alphanumeric(src[idx]) && !is_used(src[idx]))
				{
					insert(src,idx,out);
					return true;
				}
				idx++;
			}

			//giving up
			out = src;
			return false;
		}
	};
}

void menu_manager::win32_auto_mnemonics(HMENU menu)
{
	mnemonic_manager mgr;
	unsigned n, m = GetMenuItemCount(menu);
	string8_fastalloc temp,temp2;
	for(n=0;n<m;n++)//first pass, check existing mnemonics
	{
		unsigned type = uGetMenuItemType(menu,n);
		if (type==MFT_STRING)
		{
			uGetMenuString(menu,n,temp,MF_BYPOSITION);
			mgr.check_string(temp);
		}
	}

	for(n=0;n<m;n++)
	{
		HMENU submenu = GetSubMenu(menu,n);
		if (submenu) win32_auto_mnemonics(submenu);

		{
			unsigned type = uGetMenuItemType(menu,n);
			if (type==MFT_STRING)
			{
				unsigned state = submenu ? 0 : GetMenuState(menu,n,MF_BYPOSITION);
				unsigned id = GetMenuItemID(menu,n);
				uGetMenuString(menu,n,temp,MF_BYPOSITION);
				if (mgr.process_string(temp,temp2))
				{
					uModifyMenu(menu,n,MF_BYPOSITION|MF_STRING|state,id,temp2);
				}
			}
		}
	}
}

#endif

bool menu_manager::get_description(menu_item::type type,const char * name,string_base & out)
{
	service_enum_t<menu_item> e;
	menu_item * ptr;
	string8_fastalloc action_name;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->get_type()==type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				action_name.reset();
				ptr->enum_item(action,action_name);
				if (!stricmp_utf8(name,action_name))
				{
					bool rv = ptr->get_description(action,out);
					ptr->service_release();
					if (!rv) out.reset();
					return rv;
				}
			}
		}
		ptr->service_release();
	}
	return false;
}

static bool test_key(unsigned k)
{
	return (GetKeyState(k) & 0x8000) ? true : false;
}

#define F_SHIFT (HOTKEYF_SHIFT<<8)
#define F_CTRL (HOTKEYF_CONTROL<<8)
#define F_ALT (HOTKEYF_ALT<<8)
#define F_WIN (HOTKEYF_EXT<<8)

static unsigned get_key_code(WPARAM wp)
{
	unsigned code = wp & 0xFF;
	if (test_key(VK_CONTROL)) code|=F_CTRL;
	if (test_key(VK_SHIFT)) code|=F_SHIFT;
	if (test_key(VK_MENU)) code|=F_ALT;
	if (test_key(VK_LWIN) || test_key(VK_RWIN)) code|=F_WIN;
	return code;
}

bool keyboard_shortcut_manager::on_keydown(shortcut_type type,WPARAM wp)
{
	if (type==TYPE_CONTEXT) return false;
	metadb_handle_list dummy;
	return process_keydown(type,dummy,get_key_code(wp));
}

bool keyboard_shortcut_manager::on_keydown_context(const ptr_list_base<metadb_handle> & data,WPARAM wp,const GUID & caller)
{
	if (data.get_count()==0) return false;
	return process_keydown_ex(TYPE_CONTEXT,data,get_key_code(wp),caller);
}

bool keyboard_shortcut_manager::on_keydown_auto(WPARAM wp)
{
	if (on_keydown(TYPE_MAIN,wp)) return true;
	if (on_keydown(TYPE_CONTEXT_PLAYLIST,wp)) return true;
	if (on_keydown(TYPE_CONTEXT_NOW_PLAYING,wp)) return true;
	return false;
}

bool keyboard_shortcut_manager::on_keydown_auto_playlist(WPARAM wp)
{
	metadb_handle_list data;
	bool rv = false;
	playlist_oper::get()->get_sel_items(data);
	rv = on_keydown_auto_context(data,wp,menu_item::caller_playlist);
	data.delete_all();
	return rv;
}

bool keyboard_shortcut_manager::on_keydown_auto_context(const ptr_list_base<metadb_handle> & data,WPARAM wp,const GUID & caller)
{
	if (on_keydown_context(data,wp,caller)) return true;
	else return on_keydown_auto(wp);
}


bool menu_item::get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags,const GUID & caller)
{
	bool rv;
	menu_item_v2 * this_v2 = service_query_t(menu_item_v2,this);
	if (this_v2)
	{
		rv = this_v2->get_display_data(n,data,out,displayflags,caller);
		this_v2->service_release();
	}
	else
	{
		rv = get_display_data(n,data,out,displayflags);
	}
	return rv;
}

void menu_item::perform_command(unsigned n,const ptr_list_base<metadb_handle> & data,const GUID & caller)
{
	menu_item_v2 * this_v2 = service_query_t(menu_item_v2,this);
	if (this_v2)
	{
		this_v2->perform_command(n,data,caller);
		this_v2->service_release();
	}
	else
	{
		perform_command(n,data);
	}
}

bool menu_item::enum_item_guid(unsigned n,GUID & out)
{
	bool rv = false;
	menu_item_v2 * this_v2 = service_query_t(menu_item_v2,this);
	if (this_v2)
	{
		rv = this_v2->enum_item_guid(n,out);
		this_v2->service_release();
	}
	return rv;
}