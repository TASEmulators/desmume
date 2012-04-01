/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2009 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef GINFO_H
#define GINFO_H

BOOL GInfo_Init();
void GInfo_DeInit();

BOOL GInfo_DlgOpen(HWND hParentWnd);

INT_PTR CALLBACK GInfo_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK GInfo_IconBoxProc(HWND hCtl, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
