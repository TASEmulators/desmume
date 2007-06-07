/*  
	Copyright (C) 2006-2007 Normmatt

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef CONFIGKEYS_H
#define CONFIGKEYS_H

extern unsigned long keytab[12];
extern const DWORD tabkey[48];
extern DWORD ds_up;
extern DWORD ds_down;
extern DWORD ds_left;
extern DWORD ds_right;
extern DWORD ds_a;
extern DWORD ds_b;
extern DWORD ds_x;
extern DWORD ds_y;
extern DWORD ds_l;
extern DWORD ds_r;
extern DWORD ds_select;
extern DWORD ds_start;
extern DWORD ds_debug;

void GetINIPath(char *initpath,u16 bufferSize);
void  ReadConfig(void);

BOOL CALLBACK ConfigView_Proc(HWND dialog,UINT komunikat,WPARAM wparam,LPARAM lparam);

#endif
