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
#include "resource.h"
#include "palView.h"
#include "../MMU.h"
#include <stdio.h>

//////////////////////////////////////////////////////////////////////////////

LRESULT PalView_OnPaint(u16 * adr, u16 num, HWND hwnd, WPARAM wParam, LPARAM lParam)
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

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK PalView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     //ATTENTION null Ela creation donc la boite ne doit pas être visible a la création
     palview_struct *win = (palview_struct *)GetWindowLong(hwnd, DWL_USER);

     switch (message)
     {
            case WM_INITDIALOG :
                 {
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
                 CWindow_RemoveFromRefreshList(win);
                 PalView_Deinit(win);
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_PAINT:
                 PalView_OnPaint(win->adr, win->palnum, hwnd, wParam, lParam);
                 return 1;
            case WM_HSCROLL :
                 switch LOWORD(wParam)
                 {
                      case SB_LINERIGHT :
                           ++(win->palnum);
                           if(win->palnum>15)
                                win->palnum = 15;
                           break;
                      case SB_LINELEFT :
                           --(win->palnum);
                           if(win->palnum<0)
                                win->palnum = 0;
                           break;
                 }
                 CWindow_Refresh(win);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             CWindow_RemoveFromRefreshList(win);
                             PalView_Deinit(win);
                             EndDialog(hwnd, 0);
                             return 1;
                        case IDC_AUTO_UPDATE :
                             if(win->autoup)
                             {
                                  CWindow_RemoveFromRefreshList(win);
                                  win->autoup = FALSE;
                                  return 1;
                             }
                             CWindow_AddToRefreshList(win);
                             win->autoup = TRUE;
                             return 1;
                        case IDC_PAL_SELECT :
                             switch(HIWORD(wParam))
                             {
                                  case CBN_CLOSEUP :
                                       {
                                            u32 sel = SendMessage(GetDlgItem(hwnd, IDC_PAL_SELECT), CB_GETCURSEL, 0, 0);
                                            switch(sel)
                                            {
                                                 case 0 :
                                                      win->adr = (u16 *)ARM9Mem.ARM9_VMEM;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 1 :
                                                      win->adr = ((u16 *)ARM9Mem.ARM9_VMEM) + 0x200;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 2 :
                                                      win->adr = (u16 *)ARM9Mem.ARM9_VMEM + 0x100;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 3 :
                                                      win->adr = ((u16 *)ARM9Mem.ARM9_VMEM) + 0x300;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), FALSE);
                                                      break;
                                                 case 4 :
                                                 case 5 :
                                                 case 6 :
                                                 case 7 :
                                                      win->adr = ((u16 *)(ARM9Mem.ExtPal[0][sel-4]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 8 :
                                                 case 9 :
                                                 case 10 :
                                                 case 11 :
                                                      win->adr = ((u16 *)(ARM9Mem.ExtPal[1][sel-8]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 12 :
                                                 case 13 :
                                                      win->adr = ((u16 *)(ARM9Mem.ObjExtPal[0][sel-12]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 14 :
                                                 case 15 :
                                                      win->adr = ((u16 *)(ARM9Mem.ObjExtPal[1][sel-14]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 case 16 :
                                                 case 17 :
                                                 case 18 :
                                                 case 19 :
                                                      win->adr = ((u16 *)(ARM9Mem.texPalSlot[sel-16]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_SCROLLER), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_SCROLLER), TRUE);
                                                      break;
                                                 default :
                                                         return 1;
                                            }
                                            CWindow_Refresh(win);
                                            return 1;
                                       }
                             }
                             return 1;
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

palview_struct *PalView_Init(HINSTANCE hInst, HWND parent)
{
   palview_struct *PalView=NULL;

   if ((PalView = (palview_struct *)malloc(sizeof(palview_struct))) == NULL)
      return PalView;

   if (CWindow_Init2(PalView, hInst, parent, "Palette viewer", IDD_PAL, PalView_Proc) != 0)
   {
      free(PalView);
      return NULL;
   }

   PalView->palnum = 0;
   PalView->adr = (u16 *)ARM9Mem.ARM9_VMEM;

   return PalView;
}

//////////////////////////////////////////////////////////////////////////////

void PalView_Deinit(palview_struct *PalView)
{
   if (PalView)
      free(PalView);
}

//////////////////////////////////////////////////////////////////////////////
