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

#include "ioregview.h"
#include <commctrl.h>
#include "debug.h"
#include "resource.h"
#include "../MMU.h"
#include "../armcpu.h"

typedef struct
{
   u32	autoup_secs;
   bool autoup;
} ioregview_struct;

ioregview_struct	*IORegView;

LRESULT Ioreg_OnPaint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        TCHAR        text[80];
        
        hdc = BeginPaint(hwnd, &ps);
        
		// ARM9 registers
        sprintf(text, "0x%08X", (int)((u32 *)ARM9Mem.ARM9_DTCM)[0x3FFC>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_INTHAND9), text);

		sprintf(text, "0x%08X", (int)MMU.reg_IE[ARMCPU_ARM9]);
        SetWindowText(GetDlgItem(hwnd, IDC_IE9), text);

        sprintf(text, "0x%08X", (int)MMU.reg_IF[ARMCPU_ARM9]);
        SetWindowText(GetDlgItem(hwnd, IDC_IF9), text);

        sprintf(text, "0x%08X", (int)MMU.reg_IME[ARMCPU_ARM9]);
        SetWindowText(GetDlgItem(hwnd, IDC_IME9), text);

        sprintf(text, "0x%08X", ((u16 *)ARM9Mem.ARM9_REG)[0x0000>>1]);
        SetWindowText(GetDlgItem(hwnd, IDC_DISPCNTA9), text);

        sprintf(text, "0x%08X",  ((u16 *)ARM9Mem.ARM9_REG)[0x0004>>1]);
        SetWindowText(GetDlgItem(hwnd, IDC_DISPSTATA9), text);

		sprintf(text, "0x%08X", ((u16 *)ARM9Mem.ARM9_REG)[0x1000>>1]);
        SetWindowText(GetDlgItem(hwnd, IDC_DISPCNTB9), text);

        sprintf(text, "0x%08X",  ((u16 *)ARM9Mem.ARM9_REG)[0x1004>>1]);
        SetWindowText(GetDlgItem(hwnd, IDC_DISPSTATB9), text);

        sprintf(text, "0x%08X", (int)((u32 *)ARM9Mem.ARM9_REG)[0x180>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_IPCSYNC9), text);

        sprintf(text, "0x%08X", (int)((u32 *)ARM9Mem.ARM9_REG)[0x184>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_IPCFIFO9), text);

		sprintf(text, "0x%08X", (int)((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x600>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_GXSTAT9), text);

		// ARM7 registers
		sprintf(text, "0x%08X", (int)MMU.reg_IE[ARMCPU_ARM7]);
        SetWindowText(GetDlgItem(hwnd, IDC_IE7), text);

        sprintf(text, "0x%08X", (int)MMU.reg_IF[ARMCPU_ARM7]);
        SetWindowText(GetDlgItem(hwnd, IDC_IF7), text);

        sprintf(text, "0x%08X", (int)MMU.reg_IME[ARMCPU_ARM7]);
        SetWindowText(GetDlgItem(hwnd, IDC_IME7), text);

        sprintf(text, "0x%08X", (int)((u32 *)MMU.ARM7_REG)[0x180>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_IPCSYNC7), text);

        sprintf(text, "0x%08X", (int)((u32 *)MMU.ARM7_REG)[0x184>>2]);
        SetWindowText(GetDlgItem(hwnd, IDC_IPCFIFO7), text);
        EndPaint(hwnd, &ps);

        return 0;
}

BOOL CALLBACK IoregView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     switch (message)
     {
            case WM_INITDIALOG :
				 IORegView = new ioregview_struct;
				 memset(IORegView, 0, sizeof(ioregview_struct));
				 IORegView->autoup_secs = 1;
				 SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
				 SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, IORegView->autoup_secs);
                 return 1;
            case WM_CLOSE :
				if(IORegView->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_IOREG);
					IORegView->autoup = false;
				}

				if (IORegView!=NULL) 
				{
					delete IORegView;
					IORegView = NULL;
				}
				 PostQuitMessage(0);
                 return 1;
            case WM_PAINT:
                 Ioreg_OnPaint(hwnd, wParam, lParam);
                 return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;
            case WM_COMMAND :
				if(IORegView == NULL) return 0;
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
							 SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 1;
						case IDC_AUTO_UPDATE :
							 if(IORegView->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_IOREG);
                                  IORegView->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             IORegView->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_IOREG, IORegView->autoup_secs*20, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (t != IORegView->autoup_secs)
								{
									IORegView->autoup_secs = t;
									if (IORegView->autoup)
										SetTimer(hwnd, IDT_VIEW_IOREG, 
												IORegView->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                 }
                 return 0;
     }
	 return FALSE;
}
