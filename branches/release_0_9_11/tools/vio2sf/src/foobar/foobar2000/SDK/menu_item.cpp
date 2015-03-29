#include "foobar2000.h"



bool contextmenu_item::item_get_display_data_root(pfc::string_base & p_out,unsigned & p_displayflags,unsigned p_index,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const GUID & p_caller)
{
	bool status = false;
	pfc::ptrholder_t<contextmenu_item_node_root> root ( instantiate_item(p_index,p_data,p_caller) );
	if (root.is_valid()) status = root->get_display_data(p_out,p_displayflags,p_data,p_caller);
	return status;
}


static contextmenu_item_node * g_find_node(const GUID & p_guid,contextmenu_item_node * p_parent)
{
	if (p_parent->get_guid() == p_guid) return p_parent;
	else if (p_parent->get_type() == contextmenu_item_node::TYPE_POPUP)
	{
		t_size n, m = p_parent->get_children_count();
		for(n=0;n<m;n++)
		{
			contextmenu_item_node * temp = g_find_node(p_guid,p_parent->get_child(n));
			if (temp) return temp;
		}
		return 0;
	}
	else return 0;
}

bool contextmenu_item::item_get_display_data(pfc::string_base & p_out,unsigned & p_displayflags,unsigned p_index,const GUID & p_node,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const GUID & p_caller)
{
	bool status = false;
	pfc::ptrholder_t<contextmenu_item_node_root> root ( instantiate_item(p_index,p_data,p_caller) );
	if (root.is_valid())
	{
		contextmenu_item_node * node = g_find_node(p_node,root.get_ptr());
		if (node) status = node->get_display_data(p_out,p_displayflags,p_data,p_caller);
	}
	return status;
}
