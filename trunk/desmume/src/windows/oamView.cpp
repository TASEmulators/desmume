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

#include "oamView.h"
#include <commctrl.h>
#include "debug.h"
#include "resource.h"
#include "../MMU.h"
#include "../GPU.h"
#include "../NDSSystem.h"
#include "windriver.h"

typedef struct
{
	u32	autoup_secs;
	bool autoup;

	s16 num;
	OAM *oam;
	GPU *gpu;
} oamview_struct;

oamview_struct	*OAMView = NULL;

const char dimm[4][4][8] = 
{
     {"8 x 8", "16 x 8", "8 x 16", "- x -"},
     {"16 x 16", "32 x 8", "8 x 32", "- x -"},
     {"32 x 32", "32 x 16", "16 x 32", "- x -"},
     {"64 x 64", "64 x 32", "32 x 64", "- x -"},
};

LRESULT OAMViewBox_OnPaint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
	Lock lock;

        //HWND         hwnd = GetDlgItem(win->hwnd, IDC_OAM_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
//        TCHAR text[80];
        RECT rect;
        int lg;
        int ht;
        HDC mem_dc;
        HBITMAP mem_bmp;

        GetClientRect(hwnd, &rect);
        lg = rect.right - rect.left;
        ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        mem_dc = CreateCompatibleDC(hdc);
        mem_bmp = CreateCompatibleBitmap(hdc, lg, ht);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);

        return 0;
}

LRESULT OamView_OnPaint(HWND hwnd, oamview_struct *win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        OAM * oam = &win->oam[win->num];
        char text[80];
        u16 bitmap[256*192];
		u8 bitmap_alpha[256*192];
		u8 type[256*192];
        u8 prio[256*192];
        BITMAPV4HEADER bmi;
        u16 i;
        s16 x;

        //CreateBitmapIndirect(&bmi);
        memset(&bmi, 0, sizeof(bmi));
        bmi.bV4Size = sizeof(bmi);
        bmi.bV4Planes = 1;
        bmi.bV4BitCount = 16;
        bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
        bmi.bV4RedMask = 0x001F;
        bmi.bV4GreenMask = 0x03E0;
        bmi.bV4BlueMask = 0x7C00;
        bmi.bV4Width = 256;
        bmi.bV4Height = -192;

        for(i = 0; i < 256*192; ++i)
        {
           bitmap[i] = 0x7F0F;
		   bitmap_alpha[i] = 0;
		   type[i] = 0;
           prio[i] = 4;
        }
     
        hdc = BeginPaint(hwnd, &ps);
        
        sprintf(text, "OAM : %d", win->num);
        SetWindowText(GetDlgItem(hwnd, IDC_OAMNUM), text);
        
        switch(oam->attr0&(3<<10))
        {
             case 0 :
                  SetWindowText(GetDlgItem(hwnd, IDC_MODE), "Normal");
                  break;
             case (1<<10) :
                  SetWindowText(GetDlgItem(hwnd, IDC_MODE), "Smi-transp");
                  break;
             case (2<<10) :
                  SetWindowText(GetDlgItem(hwnd, IDC_MODE), "OBJ Window");
                  break;
             case (3<<10) :
                  SetWindowText(GetDlgItem(hwnd, IDC_MODE), "Bitmap");
        }
        
        sprintf(text, "0x%08X", oam->attr0/*oam->attr2&0x3FF*/);
        SetWindowText(GetDlgItem(hwnd, IDC_TILE), text);
        
        sprintf(text, "0x%08X", oam->attr1/*oam->attr2&0x3FF*/);
        SetWindowText(GetDlgItem(hwnd, IDC_PAL), text);
        
        //SetWindowText(GetDlgItem(hwnd, IDC_PAL), (oam->attr0&(1<<13))?"256 couleurs": "16 couleurs");
        
        sprintf(text, "%d 0x%08X", (oam->attr2>>10)&3, oam->attr2);
        SetWindowText(GetDlgItem(hwnd, IDC_PRIO), text);
        
        x = oam->attr1&0x1FF;
        x = ((s16)(x<<7)>>7);
        sprintf(text, "%d x %d", x, oam->attr0&0xFF);
        SetWindowText(GetDlgItem(hwnd, IDC_COOR), text);
        
        SetWindowText(GetDlgItem(hwnd, IDC_DIM), dimm[oam->attr1>>14][oam->attr0>>14]);
        
        SetWindowText(GetDlgItem(hwnd, IDC_ROT), oam->attr0&(1<<8)?"ON" : "OFF");
        
        SetWindowText(GetDlgItem(hwnd, IDC_MOS), oam->attr0&(1<<12)?"ON" : "OFF");
        
        if(oam->attr0&(1<<8))
        {
             sprintf(text, "Rot param : %d", (oam->attr1>>9)&0x1F);
             SetWindowText(GetDlgItem(hwnd, IDC_PROP0), text);
             
             SetWindowText(GetDlgItem(hwnd, IDC_PROP1), (oam->attr0&(1<<9))?"Double size": "");
        }
        else
        {
             if(oam->attr0&(1<<9))
                  sprintf(text, "INVISIBLE");
             else
                  sprintf(text, "%s %s", oam->attr0&(1<<12)?"H FLIP":"",  oam->attr0&(1<<13)?"V FLIP":"");
                  
             SetWindowText(GetDlgItem(hwnd, IDC_PROP0), text);
             
             SetWindowText(GetDlgItem(hwnd, IDC_PROP1), "");
        }
        
		GPU copy = *win->gpu;
        for(i = 0; i < 192; ++i)
        {
			copy.currLine = i;
             copy.spriteRender((u8*)(bitmap + i*256), bitmap_alpha + i*256, type + i*256, prio + i*256);
        }
        
        SetDIBitsToDevice(hdc, 180, 4, 256, 192, 0, 0, 0, 192, bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);

        EndPaint(hwnd, &ps);

        return 0;
}

LRESULT CALLBACK ViewOAMBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		   case WM_NCCREATE:
				return 1;
		   case WM_NCDESTROY:
				return 1;
		   case WM_PAINT:
				OAMViewBox_OnPaint(hwnd, wParam, lParam);
				return 1;
		   case WM_ERASEBKGND:
				return 1;
		   default:
				   break;
	}  
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK ViewOAMProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//bail out early if the dialog isnt initialized
	if(!OAMView && message != WM_INITDIALOG)
		return false;

	switch (message)
     {
            case WM_INITDIALOG :
                 {
						OAMView = new oamview_struct;
						memset(OAMView, 0, sizeof(oamview_struct));
						OAMView->oam = (OAM *)(ARM9Mem.ARM9_OAM);
						OAMView->gpu = MainScreen.gpu;

						OAMView->autoup_secs = 1;
						SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
						SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, OAMView->autoup_secs);

                      HWND combo = GetDlgItem(hwnd, IDC_SCR_SELECT);
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen sprite");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen sprite");
                      SendMessage(combo, CB_SETCURSEL, 0, 0);
                 }
                 return 1;
            case WM_CLOSE :
			{
				if(OAMView->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_OAM);
					OAMView->autoup = false;
				}

				if (OAMView!=NULL) 
				{
					delete OAMView;
					OAMView = NULL;
				}
				//INFO("Close OAM viewer dialog\n");
				PostQuitMessage(0);
				return 0;
			}
            case WM_PAINT:
                 OamView_OnPaint(hwnd, OAMView, wParam, lParam);
                 return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;
            case WM_HSCROLL :
                 switch LOWORD(wParam)
                 {
                      case SB_LINERIGHT :
                           ++(OAMView->num);
                           if(OAMView->num>127)
                                OAMView->num = 127;
                           break;
                      case SB_LINELEFT :
                           --(OAMView->num);
                           if(OAMView->num<0)
                                OAMView->num = 0;
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
							 if(OAMView->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_OAM);
                                  OAMView->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             OAMView->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_OAM, OAMView->autoup_secs*20, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (!OAMView) 
								{
									SendMessage(hwnd, WM_INITDIALOG, 0, 0);
								}
								if (t != OAMView->autoup_secs)
								{
									OAMView->autoup_secs = t;
									if (OAMView->autoup)
										SetTimer(hwnd, IDT_VIEW_OAM, 
												OAMView->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                        case IDC_SCR_SELECT :
                             switch(HIWORD(wParam))
                             {
                                  case CBN_SELCHANGE :
                                       {
                                            u32 sel = SendMessage(GetDlgItem(hwnd, IDC_SCR_SELECT), CB_GETCURSEL, 0, 0);
                                            switch(sel)
                                            {
                                                 case 0 :
                                                      OAMView->oam = (OAM *)ARM9Mem.ARM9_OAM;
                                                      OAMView->num = 0;
                                                      OAMView->gpu = MainScreen.gpu;
                                                      break;
                                                 case 1 :
                                                      OAMView->oam = (OAM *)(ARM9Mem.ARM9_OAM+0x400);
                                                      OAMView->num = 0;
                                                      OAMView->gpu = SubScreen.gpu;
                                                      break;
                                            }
                                       }
                                       InvalidateRect(hwnd, NULL, FALSE);
                                       return 1;
                            }
                             return 1;
                 }
                 return 0;
     }
	return FALSE;
}
