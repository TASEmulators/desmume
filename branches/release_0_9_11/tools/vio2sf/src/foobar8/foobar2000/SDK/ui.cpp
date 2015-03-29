#include "foobar2000.h"

bool ui_drop_item_callback::g_on_drop(interface IDataObject * pDataObject)
{
	service_enum_t<ui_drop_item_callback> e;
	ui_drop_item_callback * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		bool status = ptr->on_drop(pDataObject);
		ptr->service_release();
		if (status) return true;
	}
	return false;
}

bool ui_drop_item_callback::g_is_accepted_type(interface IDataObject * pDataObject)
{
	service_enum_t<ui_drop_item_callback> e;
	ui_drop_item_callback * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		bool status = ptr->is_accepted_type(pDataObject);
		ptr->service_release();
		if (status) return true;
	}
	return false;
}