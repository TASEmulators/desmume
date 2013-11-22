/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2012 DeSmuME team

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

#include "oamView.h"
#include <commctrl.h>
#include "main.h"
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
	void*oam;
	GPU *gpu;
	u8 scale;
	bool border;
} oamview_struct;

oamview_struct	*OAMView = NULL;

HBRUSH bhScyx = NULL;
HBRUSH bhBack = NULL;
RECT	rcThumb = {0, 0, 0, 0};

const char dimm[4][4][8] = 
{
     {"8 x 8", "16 x 8", "8 x 16", "- x -"},
     {"16 x 16", "32 x 8", "8 x 32", "- x -"},
     {"32 x 32", "32 x 16", "16 x 32", "- x -"},
     {"64 x 64", "64 x 32", "32 x 64", "- x -"}
};

const u8 dimm_int[4][4][2] =
{
	{{8,8}, {16,8}, {8,16}, {0,0}},
	{{16,16}, {32,8}, {8,32}, {0,0}},
	{{32,32}, {32,16}, {16,32}, {0,0}},
	{{64,64}, {64,32}, {32,64}, {0,0}}
};

#define SIZE_THUMB 128
const u8 max_scale[4][4] =
{
	{SIZE_THUMB/8,  SIZE_THUMB/16, SIZE_THUMB/16, 0},
	{SIZE_THUMB/16, SIZE_THUMB/32, SIZE_THUMB/32, 0},
	{SIZE_THUMB/32, SIZE_THUMB/32, SIZE_THUMB/32, 0},
	{SIZE_THUMB/64, SIZE_THUMB/64, SIZE_THUMB/64, 0},
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
				//_OAM_ _oam;
				//_OAM_* oam = &_oam;
				//SlurpOAM(oam,win->oam,win->num);

				struct MyOam
				{
					u16 attr0,attr1,attr2,attr3;
				} myOam;

				MyOam* oam = &myOam;
				memcpy(oam,(u8*)win->oam + 8*win->num,8);

        char text[80];
        u16 bitmap[256*192];
		u8 bitmap_alpha[256*192];
		u8 type[256*192];
        u8 prio[256*192];
        BITMAPV4HEADER bmi;
        u16 i;
        s16 x = 0, y = 0;

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
        
        sprintf(text, "%04X", oam->attr2&0x3FF);
        SetWindowText(GetDlgItem(hwnd, IDC_TILE), text);
        
		if (oam->attr0&(1<<13))
		{
			sprintf(text, "256 colors");
		}
		else
		{
			sprintf(text, "16 colors:%2i", (oam->attr2>>12)&0xFF);
		}
        SetWindowText(GetDlgItem(hwnd, IDC_PAL), text);
        
        sprintf(text, "%d", (oam->attr2>>10)&3);
        SetWindowText(GetDlgItem(hwnd, IDC_PRIO), text);
        
        x = oam->attr1&0x1FF;
        x = ((s16)(x<<7)>>7);
		y=oam->attr0&0xFF;
        sprintf(text, "%d x %d", x, y);
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
                  sprintf(text, "%s %s", oam->attr1&(1<<12)?"H FLIP":"",  oam->attr1&(1<<13)?"V FLIP":"");
                  
             SetWindowText(GetDlgItem(hwnd, IDC_PROP0), text);
             
             SetWindowText(GetDlgItem(hwnd, IDC_PROP1), "");
        }
        
		GPU copy = *win->gpu;
        for(i = 0; i < 192; ++i)
        {
			copy.currLine = i;
             copy.spriteRender((u8*)(bitmap + i*256), bitmap_alpha + i*256, type + i*256, prio + i*256);
        }

		u32 width = dimm_int[(oam->attr1>>14)][(oam->attr0>>14)][0];
		u32 height = dimm_int[(oam->attr1>>14)][(oam->attr0>>14)][1];
		RECT rc = {180 + x, 4 + y, 180 + x + width, 4 + y + height};
        
        SetDIBitsToDevice(hdc, 180, 4, 256, 192, 0, 0, 0, 192, bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
		
		u8 max = max_scale[oam->attr1>>14][oam->attr0>>14];
		u8 tmp_scale = (win->scale > max)?max:win->scale;
		u32 w2 = width*tmp_scale;
		u32 h2 = height*tmp_scale; 
		FillRect(hdc, &rcThumb, bhBack);
		StretchBlt(hdc, (436-SIZE_THUMB/2)-(w2/2), (200+SIZE_THUMB/2)-(h2/2), w2, h2, hdc, rc.left, rc.top, width, height, SRCCOPY);

		if (win->border)
			FrameRect(hdc, &rc, bhScyx);

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
						OAMView->oam = MMU.ARM9_OAM;
						OAMView->gpu = MainScreen.gpu;
						OAMView->scale = 2;
						OAMView->border = true;

						OAMView->autoup_secs = 1;
						SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
						SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, OAMView->autoup_secs);

                      HWND combo = GetDlgItem(hwnd, IDC_SCR_SELECT);
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Main screen sprite");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Sub screen sprite");
                      SendMessage(combo, CB_SETCURSEL, 0, 0);

					  bhScyx = CreateSolidBrush(RGB(255,0,0));
					  bhBack = CreateSolidBrush(RGB(123,198,255));
					  SetRect(&rcThumb, 436-SIZE_THUMB, 200, 436, 200+SIZE_THUMB);

					  SendMessage(GetDlgItem(hwnd, IDC_S2X), BM_SETCHECK, true, 0);
					  SendMessage(GetDlgItem(hwnd, IDC_BORDER), BM_SETCHECK, true, 0);
                 }
                 return 1;
            case WM_CLOSE :
			{
				if(OAMView->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_OAM);
					OAMView->autoup = false;
				}
				delete OAMView;
				OAMView = NULL;
				DeleteObject(bhScyx);
				DeleteObject(bhBack);
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
						   InvalidateRect(hwnd, NULL, FALSE);
                           break;
                      case SB_LINELEFT :
                           --(OAMView->num);
                           if(OAMView->num<0)
                                OAMView->num = 0;
						   InvalidateRect(hwnd, NULL, FALSE);
                           break;
                 }
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
                                                      OAMView->oam = MMU.ARM9_OAM;
                                                      OAMView->num = 0;
                                                      OAMView->gpu = MainScreen.gpu;
                                                      break;
                                                 case 1 :
                                                      OAMView->oam = (MMU.ARM9_OAM+0x400);
                                                      OAMView->num = 0;
                                                      OAMView->gpu = SubScreen.gpu;
                                                      break;
                                            }
                                       }
                                       InvalidateRect(hwnd, NULL, FALSE);
                                       return 1;
                            }
                             return 1;

						case IDC_S2X:
							OAMView->scale = 2;
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
						case IDC_S4X:
							OAMView->scale = 4;
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
						case IDC_S8X:
							OAMView->scale = 8;
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
						case IDC_S16X:
							OAMView->scale = 16;
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
						case IDC_BORDER:
							OAMView->border = (IsDlgButtonChecked(hwnd, IDC_BORDER) == BST_CHECKED);
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                 }
                 return 0;
     }
	return FALSE;
}
