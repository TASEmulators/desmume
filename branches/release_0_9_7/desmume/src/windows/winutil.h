/*	winutil.h

    Copyright (C) 2008-2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#ifndef __WINUTIL_H_
#define __WINUTIL_H_

#include <stdio.h>
#include <windows.h>

#include <string>

extern char IniName[MAX_PATH];

void GetINIPath();
void PreventScreensaver();
void DesEnableMenuItem(HMENU hMenu, UINT uIDEnableItem, bool enable);
std::string GetPrivateProfileStdString(LPCSTR lpAppName,LPCSTR lpKeyName,LPCSTR lpDefault);
void CheckDlgItem(HWND hDlg, int id, bool checked);
bool IsDlgCheckboxChecked(HWND hDlg, int id);
HMENU GetSubMenuByIdOfFirstChild(HMENU menu, UINT child);
HMENU GetSubMenuById(HMENU menu, UINT id); //untested
int GetSubMenuIndexByHMENU(HMENU menu, HMENU sub);


//returns the specified resource string ID as a std::wstring
std::wstring STRW(UINT id);

inline bool operator==(const RECT& lhs, const RECT& rhs)
{
	return lhs.left == rhs.left && lhs.top == rhs.top && lhs.right == rhs.right && lhs.bottom == rhs.bottom;
}

#endif
