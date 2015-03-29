/*
	Copyright (C) 2009-2015 DeSmuME team

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

#ifndef SOUND_VIEW_H
#define SOUND_VIEW_H

#include <windows.h>

BOOL SoundView_Init();
void SoundView_DeInit();

BOOL SoundView_DlgOpen(HWND hParentWnd);
void SoundView_DlgClose();
BOOL SoundView_IsOpened();
HWND SoundView_GetHWnd();
void SoundView_Refresh(bool forceRedraw = false);

INT_PTR CALLBACK SoundView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
