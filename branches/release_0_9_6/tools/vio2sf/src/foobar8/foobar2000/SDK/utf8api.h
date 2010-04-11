#ifndef _FOOBAR2000_UTF8API_H_
#define _FOOBAR2000_UTF8API_H_

#include "../utf8api/utf8api.h"

HWND uCreateDialog(UINT id,HWND parent,DLGPROC proc,long param=0);
int uDialogBox(UINT id,HWND parent,DLGPROC proc,long param=0);

#endif