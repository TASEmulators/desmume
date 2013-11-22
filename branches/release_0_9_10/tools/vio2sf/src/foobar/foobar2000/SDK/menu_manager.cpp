#include "foobar2000.h"

#ifdef WIN32

static void fix_ampersand(const char * src,pfc::string_base & out)
{
	out.reset();
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
	if (flags & contextmenu_item_node::FLAG_RADIOCHECKED) {/* dealt with elsewhere */}
	else if (flags & contextmenu_item_node::FLAG_CHECKED) ret |= MF_CHECKED;
	if (flags & contextmenu_item_node::FLAG_DISABLED) ret |= MF_DISABLED;
	if (flags & contextmenu_item_node::FLAG_GRAYED) ret |= MF_GRAYED;
	return ret;
}

void contextmenu_manager::win32_build_menu(HMENU menu,contextmenu_node * parent,int base_id,int max_id)//menu item identifiers are base_id<=N<base_id+max_id (if theres too many items, they will be clipped)
{
	if (parent!=0 && parent->get_type()==contextmenu_item_node::TYPE_POPUP)
	{
		pfc::string8_fastalloc temp;
		t_size child_idx,child_num = parent->get_num_children();
		for(child_idx=0;child_idx<child_num;child_idx++)
		{
			contextmenu_node * child = parent->get_child(child_idx);
			if (child)
			{
				const char * name = child->get_name();
				if (strchr(name,'&')) {fix_ampersand(name,temp);name=temp;}
				contextmenu_item_node::t_type type = child->get_type();
				if (type==contextmenu_item_node::TYPE_POPUP)
				{
					HMENU new_menu = CreatePopupMenu();
					uAppendMenu(menu,MF_STRING|MF_POPUP | flags_to_win32(child->get_display_flags()),(UINT_PTR)new_menu,name);
					win32_build_menu(new_menu,child,base_id,max_id);
				}
				else if (type==contextmenu_item_node::TYPE_SEPARATOR)
				{
					uAppendMenu(menu,MF_SEPARATOR,0,0);
				}
				else if (type==contextmenu_item_node::TYPE_COMMAND)
				{
					int id = child->get_id();
					if (id>=0 && (max_id<0 || id<max_id))
					{
						const unsigned flags = child->get_display_flags();
						const UINT ID = base_id+id;
						uAppendMenu(menu,MF_STRING | flags_to_win32(flags),ID,name);
						if (flags & contextmenu_item_node::FLAG_RADIOCHECKED) CheckMenuRadioItem(menu,ID,ID,ID,MF_BYCOMMAND);
					}
				}
			}
		}
	}
}

#endif

bool contextmenu_manager::get_description_by_id(unsigned id,pfc::string_base & out) {
	contextmenu_node * ptr = find_by_id(id);
	if (ptr == NULL) return false;
	return ptr->get_description(out);
}
bool contextmenu_manager::execute_by_id(unsigned id) {
	contextmenu_node * ptr = find_by_id(id);
	if (ptr == NULL) return false;
	ptr->execute();
	return true;
}

#ifdef WIN32

void contextmenu_manager::win32_run_menu_popup(HWND parent,const POINT * pt)
{
	enum {ID_CUSTOM_BASE = 1};

	int cmd;

	{
		POINT p;
		if (pt) p = *pt;
		else GetCursorPos(&p);
		
		HMENU hmenu = CreatePopupMenu();
		try {
		
			win32_build_menu(hmenu,ID_CUSTOM_BASE,-1);
			menu_helpers::win32_auto_mnemonics(hmenu);

			cmd = TrackPopupMenu(hmenu,TPM_RIGHTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,p.x,p.y,0,parent,0);
		} catch(...) {DestroyMenu(hmenu); throw;}
		
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

void contextmenu_manager::win32_run_menu_context(HWND parent,const pfc::list_base_const_t<metadb_handle_ptr> & data,const POINT * pt,unsigned flags)
{
	service_ptr_t<contextmenu_manager> manager;
	contextmenu_manager::g_create(manager);
	manager->init_context(data,flags);
	manager->win32_run_menu_popup(parent,pt);
}

void contextmenu_manager::win32_run_menu_context_playlist(HWND parent,const POINT * pt,unsigned flags)
{
	service_ptr_t<contextmenu_manager> manager;
	contextmenu_manager::g_create(manager);
	manager->init_context_playlist(flags);
	manager->win32_run_menu_popup(parent,pt);
}


namespace {
	class mnemonic_manager
	{
		pfc::string8_fastalloc used;
		bool is_used(unsigned c)
		{
			char temp[8];
			temp[pfc::utf8_encode_char(uCharLower(c),temp)]=0;
			return !!strstr(used,temp);
		}

		static bool is_alphanumeric(char c)
		{
			return (c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9');
		}




		void insert(const char * src,unsigned idx,pfc::string_base & out)
		{
			out.reset();
			out.add_string(src,idx);
			out.add_string("&");
			out.add_string(src+idx);
			used.add_char(uCharLower(src[idx]));
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
					if (pfc::utf8_decode_char(ptr+1,c)>0)
					{
						if (!is_used(c)) used.add_char(uCharLower(c));
					}
					return true;
				}
			}
			return false;
		}
		bool process_string(const char * src,pfc::string_base & out)//returns if changed
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

void menu_helpers::win32_auto_mnemonics(HMENU menu)
{
	mnemonic_manager mgr;
	unsigned n, m = GetMenuItemCount(menu);
	pfc::string8_fastalloc temp,temp2;
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

static bool test_key(unsigned k)
{
	return (GetKeyState(k) & 0x8000) ? true : false;
}

#define F_SHIFT (HOTKEYF_SHIFT<<8)
#define F_CTRL (HOTKEYF_CONTROL<<8)
#define F_ALT (HOTKEYF_ALT<<8)
#define F_WIN (HOTKEYF_EXT<<8)

static t_uint32 get_key_code(WPARAM wp) {
	t_uint32 code = (t_uint32)(wp & 0xFF);
	if (test_key(VK_CONTROL)) code|=F_CTRL;
	if (test_key(VK_SHIFT)) code|=F_SHIFT;
	if (test_key(VK_MENU)) code|=F_ALT;
	if (test_key(VK_LWIN) || test_key(VK_RWIN)) code|=F_WIN;
	return code;
}

static t_uint32 get_key_code(WPARAM wp, t_uint32 mods) {
	t_uint32 code = (t_uint32)(wp & 0xFF);
	if (mods & MOD_CONTROL) code|=F_CTRL;
	if (mods & MOD_SHIFT) code|=F_SHIFT;
	if (mods & MOD_ALT) code|=F_ALT;
	if (mods & MOD_WIN) code|=F_WIN;
	return code;
}

bool keyboard_shortcut_manager::on_keydown(shortcut_type type,WPARAM wp)
{
	if (type==TYPE_CONTEXT) return false;
	metadb_handle_list dummy;
	return process_keydown(type,dummy,get_key_code(wp));
}

bool keyboard_shortcut_manager::on_keydown_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,WPARAM wp,const GUID & caller)
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
	static_api_ptr_t<playlist_manager> api;
	api->activeplaylist_get_selected_items(data);
	return on_keydown_auto_context(data,wp,contextmenu_item::caller_playlist);
}

bool keyboard_shortcut_manager::on_keydown_auto_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,WPARAM wp,const GUID & caller)
{
	if (on_keydown_context(data,wp,caller)) return true;
	else return on_keydown_auto(wp);
}

static bool should_relay_key_restricted(UINT p_key) {
	switch(p_key) {
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
		return false;
	default:
		return (p_key >= VK_F1 && p_key <= VK_F24) || IsKeyPressed(VK_CONTROL) || IsKeyPressed(VK_LWIN) || IsKeyPressed(VK_RWIN);
	}
}

bool keyboard_shortcut_manager::on_keydown_restricted_auto(WPARAM wp) {
	if (!should_relay_key_restricted(wp)) return false;
	return on_keydown_auto(wp);
}
bool keyboard_shortcut_manager::on_keydown_restricted_auto_playlist(WPARAM wp) {
	if (!should_relay_key_restricted(wp)) return false;
	return on_keydown_auto_playlist(wp);
}
bool keyboard_shortcut_manager::on_keydown_restricted_auto_context(const pfc::list_base_const_t<metadb_handle_ptr> & data,WPARAM wp,const GUID & caller) {
	if (!should_relay_key_restricted(wp)) return false;
	return on_keydown_auto_context(data,wp,caller);
}

bool keyboard_shortcut_manager_v2::pretranslate_message(const MSG * msg, HWND thisPopupWnd) {
	switch(msg->message) {
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (thisPopupWnd != NULL && FindOwningPopup(msg->hwnd) == thisPopupWnd) {
				const t_uint32 modifiers = GetHotkeyModifierFlags();
				/*if (filterTypableWindowMessage(msg, modifiers))*/ {
					if (process_keydown_simple(get_key_code(msg->wParam,modifiers))) return true;
				}
			}
			return false;
		default:
			return false;
	}
}

bool keyboard_shortcut_manager::is_text_key(t_uint32 vkCode) {
	return vkCode == VK_SPACE
		|| (vkCode >= '0' && vkCode < 0x40)
		|| (vkCode > 0x40 && vkCode < VK_LWIN)
		|| (vkCode >= VK_NUMPAD0 && vkCode <= VK_DIVIDE)
		|| (vkCode >= VK_OEM_1 && vkCode <= VK_OEM_3)
		|| (vkCode >= VK_OEM_4 && vkCode <= VK_OEM_8)
		;
}

bool keyboard_shortcut_manager::is_typing_key(t_uint32 vkCode) {
	return is_text_key(vkCode)
		|| vkCode == VK_BACK
		|| vkCode == VK_RETURN
		|| vkCode == VK_INSERT
		|| (vkCode > VK_SPACE && vkCode < '0');
}

bool keyboard_shortcut_manager::is_typing_key_combo(t_uint32 vkCode, t_uint32 modifiers) {
	if (!is_typing_modifier(modifiers)) return false;
	return is_typing_key(vkCode);
}

bool keyboard_shortcut_manager::is_typing_modifier(t_uint32 flags) {
	flags &= ~MOD_SHIFT;
	return flags == 0 || flags == (MOD_ALT | MOD_CONTROL);
}

bool keyboard_shortcut_manager::is_typing_message(HWND editbox, const MSG * msg) {
	if (msg->hwnd != editbox) return false;
	return is_typing_message(msg);
}
bool keyboard_shortcut_manager::is_typing_message(const MSG * msg) {
	if (msg->message != WM_KEYDOWN && msg->message != WM_SYSKEYDOWN) return false;
	return is_typing_key_combo(msg->wParam, GetHotkeyModifierFlags());
}
