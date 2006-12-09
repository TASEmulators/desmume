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

#include <windows.h>
#include "CWindow.h"
#include "resource.h"
#include "../MMU.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////////

LRESULT Ioreg_OnPaint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        TCHAR        text[80];
        
        hdc = BeginPaint(hwnd, &ps);
        
        sprintf(text, "%08X", (int)((u32 *)ARM9Mem.ARM9_DTCM)[0x3FFC>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_INTHAND), text);

        sprintf(text, "%08X", (int)MMU.reg_IF[0]);
        SetWindowText(GetDlgItem(hwnd, IDC_IE), text);

        sprintf(text, "%08X", (int)MMU.reg_IME[0]);
        SetWindowText(GetDlgItem(hwnd, IDC_IME), text);

        sprintf(text, "%08X", ((u16 *)ARM9Mem.ARM9_REG)[0x0004>>1]);//((u32 *)ARM9.ARM9_REG)[0x10>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_DISPCNT), text);

        sprintf(text, "%08X",  ((u16 *)MMU.ARM7_REG)[0x0004>>1]);//MMU.DMACycle[0][1]);//((u16 *)ARM9.ARM9_REG)[0x16>>1]);
        SetWindowText(GetDlgItem(hwnd, IDC_DISPSTAT), text);

        sprintf(text, "%08X", (int)((u32 *)ARM9Mem.ARM9_REG)[0x180>>2]);//MMU.DMACycle[0][2]);//((u32 *)ARM9.ARM9_REG)[0x001C>>2]);//MMU.fifos[0].data[MMU.fifos[0].begin]);//((u32 *)MMU.ARM7_REG)[0x210>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_IPCSYNC), text);

        sprintf(text, "%08X", (int)((u32 *)MMU.ARM7_REG)[0x180>>2]);//MMU.DMACycle[0][3]);//nds.ARM9.SPSR.bits.I);//((u32 *)MMU.ARM7_REG)[0x184>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_IPCFIFO), text);

        EndPaint(hwnd, &ps);

        return 0;
}

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK IoregView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     cwindow_struct *win = (cwindow_struct *)GetWindowLong(hwnd, DWL_USER);
     switch (message)
     {
            case WM_INITDIALOG :
                 return 1;
            case WM_CLOSE :
                 CWindow_RemoveFromRefreshList(win);
                 if (win)
                    free(win);
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_PAINT:
                 Ioreg_OnPaint(hwnd, wParam, lParam);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             CWindow_RemoveFromRefreshList(win);
                             if (win)
                                free(win);
                             EndDialog(hwnd, 0);
                             return 1;
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////
