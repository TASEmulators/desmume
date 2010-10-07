#include "foobar2000.h"

bool ui_drop_item_callback::g_on_drop(interface IDataObject * pDataObject)
{
	service_enum_t<ui_drop_item_callback> e;
	service_ptr_t<ui_drop_item_callback> ptr;
	if (e.first(ptr)) do {
		if (ptr->on_drop(pDataObject)) return true;
	} while(e.next(ptr));
	return false;
}

bool ui_drop_item_callback::g_is_accepted_type(interface IDataObject * pDataObject, DWORD * p_effect)
{
	service_enum_t<ui_drop_item_callback> e;
	service_ptr_t<ui_drop_item_callback> ptr;
	if (e.first(ptr)) do {
		if (ptr->is_accepted_type(pDataObject,p_effect)) return true;
	} while(e.next(ptr));
	return false;
}

bool user_interface::g_find(service_ptr_t<user_interface> & p_out,const GUID & p_guid)
{
	service_enum_t<user_interface> e;
	service_ptr_t<user_interface> ptr;
	if (e.first(ptr)) do {
		if (ptr->get_guid() == p_guid)
		{
			p_out = ptr;
			return true;
		}
	} while(e.next(ptr));
	return false;
}
