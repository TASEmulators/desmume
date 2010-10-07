#include "stdafx.h"
#include "dropdown_helper.h"


void cfg_dropdown_history::build_list(pfc::ptr_list_t<char> & out)
{
	const char * src = data;
	while(*src)
	{
		int ptr = 0;
		while(src[ptr] && src[ptr]!=separator) ptr++;
		if (ptr>0)
		{
			out.add_item(pfc::strdup_n(src,ptr));
			src += ptr;
		}
		while(*src==separator) src++;
	}
}

void cfg_dropdown_history::parse_list(const pfc::ptr_list_t<char> & src)
{
	t_size n;
	pfc::string8_fastalloc temp;
	for(n=0;n<src.get_count();n++)
	{
		temp.add_string(src[n]);
		temp.add_char(separator);		
	}
	data = temp;
}

static void g_setup_dropdown_fromlist(HWND wnd,const pfc::ptr_list_t<char> & list)
{
	t_size n, m = list.get_count();
	uSendMessage(wnd,CB_RESETCONTENT,0,0);
	for(n=0;n<m;n++) {
		uSendMessageText(wnd,CB_ADDSTRING,0,list[n]);
	}
}

void cfg_dropdown_history::setup_dropdown(HWND wnd)
{
	pfc::ptr_list_t<char> list;
	build_list(list);
	g_setup_dropdown_fromlist(wnd,list);
	list.free_all();
}

void cfg_dropdown_history::add_item(const char * item)
{
	if (!item || !*item) return;
	pfc::string8 meh;
	if (strchr(item,separator))
	{
		uReplaceChar(meh,item,-1,separator,'|',false);
		item = meh;
	}
	pfc::ptr_list_t<char> list;
	build_list(list);
	unsigned n;
	bool found = false;
	for(n=0;n<list.get_count();n++)
	{
		if (!strcmp(list[n],item))
		{
			char* temp = list.remove_by_idx(n);
			list.insert_item(temp,0);
			found = true;
		}
	}

	if (!found)
	{
		while(list.get_count() > max) list.delete_by_idx(list.get_count()-1);
		list.insert_item(_strdup(item),0);
	}
	parse_list(list);
	list.free_all();
}

bool cfg_dropdown_history::is_empty()
{
	const char * src = data;
	while(*src)
	{
		if (*src!=separator) return false;
		src++;
	}
	return true;
}

void cfg_dropdown_history::on_context(HWND wnd,LPARAM coords) {
	try {
		int coords_x = (short)LOWORD(coords), coords_y = (short)HIWORD(coords);
		if (coords_x == -1 && coords_y == -1)
		{
			RECT asdf;
			GetWindowRect(wnd,&asdf);
			coords_x = (asdf.left + asdf.right) / 2;
			coords_y = (asdf.top + asdf.bottom) / 2;
		}
		enum {ID_ERASE_ALL = 1, ID_ERASE_ONE };
		HMENU menu = CreatePopupMenu();
		uAppendMenu(menu,MF_STRING,ID_ERASE_ALL,"Wipe history");
		{
			pfc::string8 tempvalue;
			uGetWindowText(wnd,tempvalue);
			if (!tempvalue.is_empty())
				uAppendMenu(menu,MF_STRING,ID_ERASE_ONE,"Remove this history item");
		}
		int cmd = TrackPopupMenu(menu,TPM_RIGHTBUTTON|TPM_NONOTIFY|TPM_RETURNCMD,coords_x,coords_y,0,wnd,0);
		DestroyMenu(menu);
		switch(cmd)
		{
		case ID_ERASE_ALL:
			{
				data = "";
				pfc::string8 value;//preserve old value while wiping dropdown list
				uGetWindowText(wnd,value);
				uSendMessage(wnd,CB_RESETCONTENT,0,0);
				uSetWindowText(wnd,value);
			}
			break;
		case ID_ERASE_ONE:
			{
				pfc::string8 value;
				uGetWindowText(wnd,value);

				pfc::ptr_list_t<char> list;
				t_size n,m;
				bool found;
				build_list(list);
				m = list.get_count();
				found = false;
				for(n=0;n<m;n++)
				{
					if (!strcmp(value,list[n]))
					{
						free(list[n]);
						list.remove_by_idx(n);
						found = true;
						break;
					}
				}
				if (found) parse_list(list);
				g_setup_dropdown_fromlist(wnd,list);
				list.free_all();
			}
			break;
		}
	} catch(...) {}
}
