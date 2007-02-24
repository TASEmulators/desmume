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

#include "mapview.h"
#include "resource.h"
#include "../MMU.h"
#include "../NDSSystem.h"

#include <stdio.h>

//////////////////////////////////////////////////////////////////////////////

LRESULT MapView_OnPaint(mapview_struct * win, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        char text[80];
        u32 dispcnt = ((volatile u32 *)ARM9Mem.ARM9_REG)[(win->lcd*0x400)];
        u32 bgcnt =  ((volatile u16 *)ARM9Mem.ARM9_REG)[(8 + (win->map<<1) + (win->lcd*0x1000))>>1];
        BITMAPV4HEADER bmi;
        u16 lg;
        u16 ht;
       	BGxPARMS * parms;

        //CreateBitmapIndirect(&bmi);
        memset(&bmi, 0, sizeof(bmi));
        bmi.bV4Size = sizeof(bmi);
        bmi.bV4Planes = 1;
        bmi.bV4BitCount = 16;
        bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
        bmi.bV4RedMask = 0x001F;
        bmi.bV4GreenMask = 0x03E0;
        bmi.bV4BlueMask = 0x7C00;

        if(win->lcd)
        {
             lg = SubScreen.gpu->BGSize[win->map][0];
             ht = SubScreen.gpu->BGSize[win->map][1];
        }
        else
        {
             lg = MainScreen.gpu->BGSize[win->map][0];
             ht = MainScreen.gpu->BGSize[win->map][1];
        }
        bmi.bV4Width = lg;
        bmi.bV4Height = -ht;
     
        hdc = BeginPaint(hwnd, &ps);        
        
        sprintf(text, "%d %08X, %08X", (int)(dispcnt&7), (int)dispcnt, (int)bgcnt);
        SetWindowText(GetDlgItem(hwnd, IDC_MODE), text);
        
        if(!(bgcnt&(1<<7)))
             sprintf(text, "normale 16");
        else
             if(!(dispcnt&(1<<30)))
                  sprintf(text, "normale 256");
             else
                  switch(win->map)
                  {
                       case 0 :
                            sprintf(text, "extended slot %d", (bgcnt&(1<<13))?2:0);
                            break;
                       case 1 :
                            sprintf(text, "extended slot %d", (bgcnt&(1<<13))?3:1);
                            break;
                       default :
                            sprintf(text, "extended slot %d", MainScreen.gpu->BGExtPalSlot[win->map]);
                            break;
                  }
        SetWindowText(GetDlgItem(hwnd, IDC_PAL), text);
        
        sprintf(text, "%d", (int)(bgcnt&3));
        SetWindowText(GetDlgItem(hwnd, IDC_PRIO), text);
        
        sprintf(text, "0x%08X", (int)(0x6000000 + ((bgcnt>>2)&0xF)*0x4000 + win->lcd*0x200000 +((dispcnt>>24)&7)*0x10000));
        SetWindowText(GetDlgItem(hwnd, IDC_CHAR), text);
        
        sprintf(text, "0x%08X", (int)(0x6000000 + 0x800*((bgcnt>>8)&0x1F) + win->lcd*0x200000 + ((dispcnt>>27)&7)*0x10000));
        SetWindowText(GetDlgItem(hwnd, IDC_SCR), text);
        
        //sprintf(text, "%d x %d",  MainScreen.gpu->BGPA[win->map], MainScreen.gpu->BGPB[win->map]);
        sprintf(text, "%d x %d",  (int)MainScreen.gpu->BGSize[win->map][0], (int)MainScreen.gpu->BGSize[win->map][1]);
        SetWindowText(GetDlgItem(hwnd, IDC_MSIZE), text);
        
		if (win->map==2) {
			parms = &(MainScreen.gpu->dispx_st)->dispx_BG2PARMS;
		} else {
			parms = &(MainScreen.gpu->dispx_st)->dispx_BG3PARMS;		
		}
        sprintf(text, "%d x %d", parms->BGxPC, parms->BGxPD);
        SetWindowText(GetDlgItem(hwnd, IDC_SCROLL), text);
        
        if(win->lcd)
             textBG(SubScreen.gpu, win->map, (u8 *)win->bitmap);
             //rotBG(SubScreen.gpu, win->map, win->bitmap);
             //extRotBG(SubScreen.gpu, win->map, win->bitmap);
        else
             textBG(MainScreen.gpu, win->map, (u8 *)win->bitmap);
             //rotBG(MainScreen.gpu, win->map, win->bitmap);
             //extRotBG(MainScreen.gpu, win->map, win->bitmap);
        
        SetDIBitsToDevice(hdc, 200, 4, lg, ht, 0, 0, 0, ht, win->bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
        //SetDIBitsToDevice(hdc, 200, 4, 256, 192, 0, 0, 0, 192, win->bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
        
        EndPaint(hwnd, &ps);

        return 0;
}

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK MapView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     mapview_struct * win = (mapview_struct *)GetWindowLong(hwnd, DWL_USER);
     switch (message)
     {
            case WM_INITDIALOG :
                 {
                      HWND combo = GetDlgItem(hwnd, IDC_BG_SELECT);
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 2");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main BackGround 3");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 2");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub BackGround 3");
                      SendMessage(combo, CB_SETCURSEL, 0, 0);
                 }
                 return 1;
            case WM_CLOSE :
                 CWindow_RemoveFromRefreshList(win);
                 MapView_Deinit(win);
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_PAINT:
                 MapView_OnPaint(win, hwnd, wParam, lParam);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             CWindow_RemoveFromRefreshList(win);
                             MapView_Deinit(win);
                             EndDialog(hwnd, 0);
                             return 1;
                        case IDC_BG_SELECT :
                             switch(HIWORD(wParam))
                             {
                                  case CBN_CLOSEUP :
                                       {
                                            u32 sel= SendMessage(GetDlgItem(hwnd, IDC_BG_SELECT), CB_GETCURSEL, 0, 0);
                                            switch(sel)
                                            {
                                                 case 0 :
                                                 case 1 :
                                                 case 2 :
                                                 case 3 :
                                                      win->map = sel;
                                                      win->lcd = 0;
                                                      break;
                                                 case 4 :
                                                 case 5 :
                                                 case 6 :
                                                 case 7 :
                                                      win->map = sel-4;
                                                      win->lcd = 1;
                                                      break;
                                            }
                                       }
                             CWindow_Refresh(win);
                             return 1;
                             }//switch et case
                 }//switch
                 return 1;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

mapview_struct *MapView_Init(HINSTANCE hInst, HWND parent)
{
   mapview_struct *MapView=NULL;

   if ((MapView = (mapview_struct *)malloc(sizeof(mapview_struct))) == NULL)
      return MapView;

   if (CWindow_Init2(MapView, hInst, parent, "Map viewer", IDD_MAP, MapView_Proc) != 0)
   {
      free(MapView);
      return NULL;
   }

   MapView->map = 0;
   MapView->lcd = 0;

   return MapView;
}

//////////////////////////////////////////////////////////////////////////////

void MapView_Deinit(mapview_struct *MapView)
{
   if (MapView)
      free(MapView);
}

//////////////////////////////////////////////////////////////////////////////
