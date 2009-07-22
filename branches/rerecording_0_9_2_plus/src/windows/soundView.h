/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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

#ifndef SOUND_VIEW_H
#define SOUND_VIEW_H

BOOL SoundView_Init();
void SoundView_DeInit();

BOOL SoundView_DlgOpen(HWND hParentWnd);
void SoundView_DlgClose();
BOOL SoundView_IsOpened();
HWND SoundView_GetHWnd();
void SoundView_Refresh();

BOOL CALLBACK SoundView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

#endif
