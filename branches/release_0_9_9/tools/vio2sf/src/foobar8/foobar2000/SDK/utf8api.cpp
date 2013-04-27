#include "foobar2000.h"

HWND uCreateDialog(UINT id,HWND parent,DLGPROC proc,long param)
{
	return uCreateDialog(core_api::get_my_instance(),id,parent,proc,param);
}

int uDialogBox(UINT id,HWND parent,DLGPROC proc,long param)
{
	return uDialogBox(core_api::get_my_instance(),id,parent,proc,param);
}
