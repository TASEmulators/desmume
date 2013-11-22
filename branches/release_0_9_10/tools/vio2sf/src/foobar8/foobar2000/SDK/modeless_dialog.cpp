#include "foobar2000.h"

void modeless_dialog_manager::add(HWND wnd)
{
	service_enum_t<modeless_dialog_manager> e;
	modeless_dialog_manager * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		ptr->_add(wnd);
		ptr->service_release();
	}
}

void modeless_dialog_manager::remove(HWND wnd)
{
	service_enum_t<modeless_dialog_manager> e;
	modeless_dialog_manager * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		ptr->_remove(wnd);
		ptr->service_release();
	}
}