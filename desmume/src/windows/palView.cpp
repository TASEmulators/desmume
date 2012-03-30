/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2011 DeSmuME team

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

#include "palView.h"
#include <commctrl.h>
#include "main.h"
#include "debug.h"
#include "resource.h"
#include "../MMU.h"

typedef struct
{
	u32	autoup_secs;
	bool autoup;

	u16 *adr;
	s16 palnum;
} palview_struct;

palview_struct	*PalView = NULL;

LRESULT PalView_OnPaint(const u16 * adr, u16 num, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        RECT         rect;
        HBRUSH       brush;
        u16 c;
        char tmp[80];
        
        rect.left = 3;    
        rect.top = 55;    
        rect.right = 13;    
        rect.bottom = 65;    
        hdc = BeginPaint(hwnd, &ps);
        
        if(adr)
        {
             u32 y;

             for(y = 0; y < 16; ++y)
             {
                  u32 x;

                  for(x = 0; x < 16; ++x)
                  {
                       c = adr[(y<<4)+x+0x100*num];
                       brush = CreateSolidBrush(RGB((c&0x1F)<<3, (c&0x3E0)>>2, (c&0x7C00)>>7));
                       FillRect(hdc, &rect, brush);
                       DeleteObject(brush);
                       rect.left += 11;
                       rect.right += 11;
                  }
                  rect.top += 11;
                  rect.bottom += 11;
                  rect.left = 3;
                  rect.right = 13;
             }
             sprintf(tmp, "Pal : %d", num);
             SetWindowText(GetDlgItem(hwnd, IDC_PALNUM), tmp);
        }
        else
             TextOut(hdc, 3, 55, "Pas de palette", 14);
        EndPaint(hwnd, &ps);

        return 0;
}

BOOL CALLBACK ViewPalProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//bail out early if the dialog isnt initialized
	if(!PalView && message != WM_INITDIALOG)
		return false;

	switch (message)
     {
            case WM_INITDIALOG :
                 {
					PalView = new palview_struct;
					memset(PalView, 0, sizeof(palview_struct));
					PalView->adr = (u16 *)MMU.ARM9_VMEM;
					PalView->autoup_secs = 1;
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, PalView->autoup_secs);

                      HWND combo = GetDlgItem(hwnd, IDC_PAL_SELECT);
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen BG PAL");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen BG PAL");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen SPR PAL");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen SPR PAL");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen ExtPAL 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen ExtPAL 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen ExtPAL 2");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen ExtPAL 3");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen ExtPAL 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen ExtPAL 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen ExtPAL 2");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen ExtPAL 3");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main spr ExtPAL 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main spr ExtPAL 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub spr ExtPAL 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub spr ExtPAL 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture pal 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture pal 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture pal 2");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture pal 3");
                      SendMessage(combo, CB_SETCURSEL, 0, 0);
                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                 }
                 return 1;
            case WM_CLOSE :
			{
				if(PalView->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_DISASM7);
					PalView->autoup = false;
				}
				delete PalView;
				PalView = NULL;
				//INFO("Close Palette view dialog\n");
				PostQuitMessage(0);
				return 0;
			}
            case WM_PAINT:
                 PalView_OnPaint(PalView->adr, PalView->palnum, hwnd, wParam, lParam);
                 return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;
            case WM_HSCROLL :
                 switch LOWORD(wParam)
                 {
                      case SB_LINERIGHT :
                           ++(PalView->palnum);
                           if(PalView->palnum>15)
                                PalView->palnum = 15;
                           break;
                      case SB_LINELEFT :
                           --(PalView->palnum);
                           if(PalView->palnum<0)
                                PalView->palnum = 0;
                           break;
                 }
				 InvalidateRect(hwnd, NULL, FALSE);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
							 SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 1;
                        case IDC_AUTO_UPDATE :
							 if(PalView->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_PAL);
                                  PalView->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             PalView->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_PAL, PalView->autoup_secs*20, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (!PalView) 
								{
									SendMessage(hwnd, WM_INITDIALOG, 0, 0);
								}
								if (t != PalView->autoup_secs)
								{
									PalView->autoup_secs = t;
									if (PalView->autoup)
										SetTimer(hwnd, IDT_VIEW_PAL, 
												PalView->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                        case IDC_PAL_SELECT :
                             switch(HIWORD(wParam))
                             {
                                  case CBN_SELCHANGE :
                                  case CBN_CLOSEUP :
                                       {
                                            u32 sel = SendMessage(GetDlgItem(hwnd, IDC_PAL_SELECT), CB_GETCURSEL, 0, 0);
                                            switch(sel)
                                            {
                                                 case 0 :
                                                      PalView->adr = (u16 *)MMU.ARM9_VMEM;
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 1 :
                                                      PalView->adr = ((u16 *)MMU.ARM9_VMEM) + 0x200;
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 2 :
                                                      PalView->adr = (u16 *)MMU.ARM9_VMEM + 0x100;
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 3 :
                                                      PalView->adr = ((u16 *)MMU.ARM9_VMEM) + 0x300;
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 4 :
                                                 case 5 :
                                                 case 6 :
                                                 case 7 :
                                                      PalView->adr = ((u16 *)(MMU.ExtPal[0][sel-4]));
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 8 :
                                                 case 9 :
                                                 case 10 :
                                                 case 11 :
                                                      PalView->adr = ((u16 *)(MMU.ExtPal[1][sel-8]));
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 12 :
                                                 case 13 :
                                                      PalView->adr = ((u16 *)(MMU.ObjExtPal[0][sel-12]));
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 14 :
                                                 case 15 :
                                                      PalView->adr = ((u16 *)(MMU.ObjExtPal[1][sel-14]));
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 16 :
                                                 case 17 :
                                                 case 18 :
                                                 case 19 :
                                                      PalView->adr = ((u16 *)(MMU.texInfo.texPalSlot[sel-16]));
                                                      PalView->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 default :
                                                         return 1;
                                            }
                                            InvalidateRect(hwnd, NULL, FALSE);
                                            return 1;
                                       }
                             }
                             return 1;
                 }
                 return 0;
     }
	 return false;
}
