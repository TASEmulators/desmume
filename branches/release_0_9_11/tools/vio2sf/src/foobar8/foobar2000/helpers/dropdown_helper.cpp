#include "stdafx.h"
#include "dropdown_helper.h"


void cfg_dropdown_history::build_list(ptr_list_simple<char> & out)
{
	const char * src = data;
	while(*src)
	{
		int ptr = 0;
		while(src[ptr] && src[ptr]!=separator) ptr++;
		if (ptr>0)
		{
			out.add_item(strdup_n(src,ptr));
			src += ptr;
		}
		while(*src==separator) src++;
	}
}

void cfg_dropdown_history::parse_list(const ptr_list_simple<char> & src)
{
	unsigned n;
	string8 temp;
	temp.set_mem_logic(mem_block::ALLOC_FAST);
	for(n=0;n<src.get_count();n++)
	{
		temp.add_string(src[n]);
		temp.add_char(separator);		
	}
	data = temp;
}

void cfg_dropdown_history::setup_dropdown(HWND wnd)
{
	ptr_list_simple<char> list;
	build_list(list);
	unsigned n;
	for(n=0;n<list.get_count();n++)
	{
		uSendMessageText(wnd,CB_ADDSTRING,0,list[n]);
	}
	list.free_all();
}

void cfg_dropdown_history::add_item(const char * item)
{
	if (!item || !*item) return;
	string8 meh;
	if (strchr(item,separator))
	{
		uReplaceChar(meh,item,-1,separator,'|',false);
		item = meh;
	}
	ptr_list_simple<char> list;
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
		list.insert_item(strdup(item),0);
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