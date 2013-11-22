#include "foobar2000.h"


bool menu_helpers::context_get_description(const GUID& p_guid,pfc::string_base & out) {
	service_ptr_t<contextmenu_item> ptr; t_uint32 index;
	if (!menu_item_resolver::g_resolve_context_command(p_guid, ptr, index)) return false;
	bool rv = ptr->get_item_description(index, out);
	if (!rv) out.reset();
	return rv;
}

static bool run_context_command_internal(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data,const GUID & caller) {
	if (data.get_count() == 0) return false;
	service_ptr_t<contextmenu_item> ptr; t_uint32 index;
	if (!menu_item_resolver::g_resolve_context_command(p_command, ptr, index)) return false;
	
	{
		TRACK_CALL_TEXT("menu_helpers::run_command(), by GUID");
		ptr->item_execute_simple(index, p_subcommand, data, caller);
	}
	
	return true;
}

bool menu_helpers::run_command_context(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data)
{
	return run_context_command_internal(p_command,p_subcommand,data,contextmenu_item::caller_undefined);
}

bool menu_helpers::run_command_context_ex(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data,const GUID & caller)
{
	return run_context_command_internal(p_command,p_subcommand,data,caller);
}

bool menu_helpers::test_command_context(const GUID & p_guid)
{
	service_ptr_t<contextmenu_item> ptr; t_uint32 index;
	return menu_item_resolver::g_resolve_context_command(p_guid, ptr, index);
}

static bool g_is_checked(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data,const GUID & caller)
{
	service_ptr_t<contextmenu_item> ptr; t_uint32 index;
	if (!menu_item_resolver::g_resolve_context_command(p_command, ptr, index)) return false;

	unsigned displayflags = 0;
	pfc::string_formatter dummystring;
	if (!ptr->item_get_display_data(dummystring,displayflags,index,p_subcommand,data,caller)) return false;
	return (displayflags & contextmenu_item_node::FLAG_CHECKED) != 0;

}

bool menu_helpers::is_command_checked_context(const GUID & p_command,const GUID & p_subcommand,const pfc::list_base_const_t<metadb_handle_ptr> & data)
{
	return g_is_checked(p_command,p_subcommand,data,contextmenu_item::caller_undefined);
}

bool menu_helpers::is_command_checked_context_playlist(const GUID & p_command,const GUID & p_subcommand)
{
	metadb_handle_list temp;
	static_api_ptr_t<playlist_manager> api;
	api->activeplaylist_get_selected_items(temp);
	return g_is_checked(p_command,p_subcommand,temp,contextmenu_item::caller_playlist);
}







bool menu_helpers::run_command_context_playlist(const GUID & p_command,const GUID & p_subcommand)
{
	metadb_handle_list temp;
	static_api_ptr_t<playlist_manager> api;
	api->activeplaylist_get_selected_items(temp);
	return run_command_context_ex(p_command,p_subcommand,temp,contextmenu_item::caller_playlist);
}

bool menu_helpers::run_command_context_now_playing(const GUID & p_command,const GUID & p_subcommand)
{
	metadb_handle_ptr item;
	if (!static_api_ptr_t<playback_control>()->get_now_playing(item)) return false;//not playing
	return run_command_context_ex(p_command,p_subcommand,pfc::list_single_ref_t<metadb_handle_ptr>(item),contextmenu_item::caller_now_playing);
}


bool menu_helpers::guid_from_name(const char * p_name,unsigned p_name_len,GUID & p_out)
{
	service_enum_t<contextmenu_item> e;
	service_ptr_t<contextmenu_item> ptr;
	pfc::string8_fastalloc nametemp;
	while(e.next(ptr))
	{
		unsigned n, m = ptr->get_num_items();
		for(n=0;n<m;n++)
		{
			ptr->get_item_name(n,nametemp);
			if (!strcmp_ex(nametemp,infinite,p_name,p_name_len))
			{
				p_out = ptr->get_item_guid(n);
				return true;
			}
		}
	}
	return false;
}

bool menu_helpers::name_from_guid(const GUID & p_guid,pfc::string_base & p_out) {
	service_ptr_t<contextmenu_item> ptr; t_uint32 index;
	if (!menu_item_resolver::g_resolve_context_command(p_guid, ptr, index)) return false;
	ptr->get_item_name(index, p_out);
	return true;
}


static unsigned calc_total_action_count()
{
	service_enum_t<contextmenu_item> e;
	service_ptr_t<contextmenu_item> ptr;
	unsigned ret = 0;
	while(e.next(ptr))
		ret += ptr->get_num_items();
	return ret;
}


const char * menu_helpers::guid_to_name_table::search(const GUID & p_guid)
{
	if (!m_inited)
	{
		m_data.set_size(calc_total_action_count());
		t_size dataptr = 0;
		pfc::string8_fastalloc nametemp;

		service_enum_t<contextmenu_item> e;
		service_ptr_t<contextmenu_item> ptr;
		while(e.next(ptr))
		{
			unsigned n, m = ptr->get_num_items();
			for(n=0;n<m;n++)
			{
				assert(dataptr < m_data.get_size());

				ptr->get_item_name(n,nametemp);
				m_data[dataptr].m_name = _strdup(nametemp);
				m_data[dataptr].m_guid = ptr->get_item_guid(n);
				dataptr++;
			}
		}
		assert(dataptr == m_data.get_size());

		pfc::sort_t(m_data,entry_compare,m_data.get_size());
		m_inited = true;
	}
	t_size index;
	if (pfc::bsearch_t(m_data.get_size(),m_data,entry_compare_search,p_guid,index))
		return m_data[index].m_name;
	else
		return 0;
}

int menu_helpers::guid_to_name_table::entry_compare_search(const entry & entry1,const GUID & entry2)
{
	return pfc::guid_compare(entry1.m_guid,entry2);
}

int menu_helpers::guid_to_name_table::entry_compare(const entry & entry1,const entry & entry2)
{
	return pfc::guid_compare(entry1.m_guid,entry2.m_guid);
}

menu_helpers::guid_to_name_table::guid_to_name_table()
{
	m_inited = false;
}

menu_helpers::guid_to_name_table::~guid_to_name_table()
{
	t_size n, m = m_data.get_size();
	for(n=0;n<m;n++) free(m_data[n].m_name);

}


int menu_helpers::name_to_guid_table::entry_compare_search(const entry & entry1,const search_entry & entry2)
{
	return stricmp_utf8_ex(entry1.m_name,infinite,entry2.m_name,entry2.m_name_len);
}

int menu_helpers::name_to_guid_table::entry_compare(const entry & entry1,const entry & entry2)
{
	return stricmp_utf8(entry1.m_name,entry2.m_name);
}

bool menu_helpers::name_to_guid_table::search(const char * p_name,unsigned p_name_len,GUID & p_out)
{
	if (!m_inited)
	{
		m_data.set_size(calc_total_action_count());
		t_size dataptr = 0;
		pfc::string8_fastalloc nametemp;

		service_enum_t<contextmenu_item> e;
		service_ptr_t<contextmenu_item> ptr;
		while(e.next(ptr))
		{
			unsigned n, m = ptr->get_num_items();
			for(n=0;n<m;n++)
			{
				assert(dataptr < m_data.get_size());

				ptr->get_item_name(n,nametemp);
				m_data[dataptr].m_name = _strdup(nametemp);
				m_data[dataptr].m_guid = ptr->get_item_guid(n);
				dataptr++;
			}
		}
		assert(dataptr == m_data.get_size());

		pfc::sort_t(m_data,entry_compare,m_data.get_size());
		m_inited = true;
	}
	t_size index;
	search_entry temp = {p_name,p_name_len};
	if (pfc::bsearch_t(m_data.get_size(),m_data,entry_compare_search,temp,index))
	{
		p_out = m_data[index].m_guid;
		return true;
	}
	else
		return false;
}

menu_helpers::name_to_guid_table::name_to_guid_table()
{
	m_inited = false;
}

menu_helpers::name_to_guid_table::~name_to_guid_table()
{
	t_size n, m = m_data.get_size();
	for(n=0;n<m;n++) free(m_data[n].m_name);

}

bool menu_helpers::find_command_by_name(const char * p_name,service_ptr_t<contextmenu_item> & p_item,unsigned & p_index)
{
	pfc::string8_fastalloc path,name;
	service_enum_t<contextmenu_item> e;
	service_ptr_t<contextmenu_item> ptr;
	if (e.first(ptr)) do {
//		if (ptr->get_type()==type)
		{
			unsigned action,num_actions = ptr->get_num_items();
			for(action=0;action<num_actions;action++)
			{
				ptr->get_item_default_path(action,path); ptr->get_item_name(action,name);
				if (!path.is_empty()) path += "/";
				path += name;
				if (!stricmp_utf8(p_name,path))
				{
					p_item = ptr;
					p_index = action;
					return true;
				}
			}
		}
	} while(e.next(ptr));
	return false;

}

bool menu_helpers::find_command_by_name(const char * p_name,GUID & p_command)
{
	service_ptr_t<contextmenu_item> item;
	unsigned index;
	bool ret = find_command_by_name(p_name,item,index);
	if (ret) p_command = item->get_item_guid(index);
	return ret;
}


bool standard_commands::run_main(const GUID & p_guid) {
	t_uint32 index;
	mainmenu_commands::ptr ptr;
	if (!menu_item_resolver::g_resolve_main_command(p_guid, ptr, index)) return false;
	ptr->execute(index,service_ptr_t<service_base>());
	return true;
}

bool menu_item_resolver::g_resolve_context_command(const GUID & id, contextmenu_item::ptr & out, t_uint32 & out_index) {
	menu_item_resolver::ptr api;
	if (service_enum_t<menu_item_resolver>().first(api)) {
		return api->resolve_context_command(id, out, out_index);
	} else {
		service_enum_t<contextmenu_item> e; service_ptr_t<contextmenu_item> ptr;
		while(e.next(ptr)) {
			const unsigned num_actions = ptr->get_num_items();
			for(unsigned action=0;action<num_actions;action++) {
				if (id == ptr->get_item_guid(action)) {
					out_index = action; out = ptr; return true;
				}
			}
		}
		return false;
	}
}
bool menu_item_resolver::g_resolve_main_command(const GUID & id, mainmenu_commands::ptr & out, t_uint32 & out_index) {
	menu_item_resolver::ptr api;
	if (service_enum_t<menu_item_resolver>().first(api)) {
		return api->resolve_main_command(id, out, out_index);
	} else {
		service_enum_t<mainmenu_commands> e; service_ptr_t<mainmenu_commands> ptr;
		while(e.next(ptr)) {
			const t_uint32 total = ptr->get_command_count();
			for(t_uint32 walk = 0; walk < total; ++walk) {
				if (id == ptr->get_command(walk)) {
					out_index = walk; out = ptr; return true;
				}
			}
		}
		return false;
	}
}

