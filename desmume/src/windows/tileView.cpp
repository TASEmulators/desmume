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

#include "tileView.h"
#include "commctrl.h"
#include "resource.h"
#include "debug.h"
#include "main.h"
#include "../MMU.h"
#include "../gpu.h"

class tileview_struct
{
public:
	tileview_struct()
		: hwnd(NULL)
	{}

	u32	autoup_secs;
	bool autoup;

	HWND	hwnd;
	u32 target;
	u16 * pal;
	void setPalnum(s16 num)
	{
		palnum = num;
		if(hwnd == NULL) return;
		char text[100];
        sprintf(text, "Pal : %d", palnum);
		SetDlgItemText(hwnd, IDC_PALNUM, text);
	}
	u16 tilenum;
	u8 coul;
	u32 x;
	u32 y;
	s16 palnum;
};

tileview_struct		*TileView = NULL;

LRESULT TileViewBox_Direct(HWND hwnd, tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
//        SIZE fontsize;
//        TCHAR text[80];
        BITMAPV4HEADER bmi;
        RECT rect;
        HDC mem_dc;
        HBITMAP mem_bmp;
        int lg;
        int ht;
        
        memset(&bmi, 0, sizeof(bmi));
        bmi.bV4Size = sizeof(bmi);
        bmi.bV4Planes = 1;
        bmi.bV4BitCount = 16;
        bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
        bmi.bV4RedMask = 0x001F;
        bmi.bV4GreenMask = 0x03E0;
        bmi.bV4BlueMask = 0x7C00;
        bmi.bV4Width = 256;
        bmi.bV4Height = -256;
        
        GetClientRect(hwnd, &rect);
        lg = rect.right - rect.left;
        ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        mem_dc = CreateCompatibleDC(hdc);
        mem_bmp = CreateCompatibleBitmap(hdc, lg, ht);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
		u8* mem;
		if(win->target >= MMU_LCDC)
			mem = MMU.ARM9_LCD + win->target - MMU_LCDC;
		else
			mem = (u8*)MMU_gpu_map(win->target);
		if(mem)
			SetDIBitsToDevice(mem_dc, 0, 0, 256, 256, 0, 0, 0, 256, mem, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
        
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT TileViewBox_Pal256(HWND hwnd, tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
//        SIZE fontsize;
        u16 bitmap[256*256];
        u16 * pal = ((u16 *)win->pal) + win->palnum*256;
        BITMAPV4HEADER bmi;
        RECT rect;
        int lg;
        int ht;
        HDC mem_dc;
        HBITMAP mem_bmp;

        memset(&bmi, 0, sizeof(bmi));
        bmi.bV4Size = sizeof(bmi);
        bmi.bV4Planes = 1;
        bmi.bV4BitCount = 16;
        bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
        bmi.bV4RedMask = 0x001F;
        bmi.bV4GreenMask = 0x03E0;
        bmi.bV4BlueMask = 0x7C00;
        bmi.bV4Width = 256;
        bmi.bV4Height = -256;
        
        GetClientRect(hwnd, &rect);
        lg = rect.right - rect.left;
        ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        mem_dc = CreateCompatibleDC(hdc);
        mem_bmp = CreateCompatibleBitmap(hdc, lg, ht);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        if(win->pal)
        {
             u32 num2, num, y, x;

            u8* mem;
			if(win->target >= MMU_LCDC)
				mem = MMU.ARM9_LCD + win->target - MMU_LCDC;
			else
				mem = (u8*)MMU_gpu_map(win->target);
			 if(mem)
			 {
				 for(num2 = 0; num2<32; ++num2)
				 for(num = 0; num<32; ++num)
				 for(y = 0; y<8; ++y)
				 for(x = 0; x<8; ++x)
					  bitmap[x + (y*256) + (num*8) +(num2*256*8)] = pal[mem[x + (y*8) + (num*64) +(num2*2048)]];        
				 SetDIBitsToDevice(mem_dc, 0, 0, 256, 256, 0, 0, 0, 256, bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
			 }
        }
        else
             TextOut(mem_dc, 3, 3, "Il n'y a pas de palette", 23);
             
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT TileViewBox_Pal16(HWND hwnd, tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
//        SIZE fontsize;
        u16 bitmap[512*512];
        u16 * pal = ((u16 *)win->pal) + win->palnum*16;
        BITMAPV4HEADER bmi;
        RECT rect;
        int lg;
        int ht;
        HDC mem_dc;
        HBITMAP mem_bmp;

        memset(&bmi, 0, sizeof(bmi));
        bmi.bV4Size = sizeof(bmi);
        bmi.bV4Planes = 1;
        bmi.bV4BitCount = 16;
        bmi.bV4V4Compression = BI_RGB|BI_BITFIELDS;
        bmi.bV4RedMask = 0x001F;
        bmi.bV4GreenMask = 0x03E0;
        bmi.bV4BlueMask = 0x7C00;
        bmi.bV4Width = 512;
        bmi.bV4Height = -256;
        
        GetClientRect(hwnd, &rect);
        lg = rect.right - rect.left;
        ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        mem_dc = CreateCompatibleDC(hdc);
        mem_bmp = CreateCompatibleBitmap(hdc, 512, 256);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        if(win->pal)
        {
			u8* mem;
			if(win->target >= MMU_LCDC)
				mem = MMU.ARM9_LCD + win->target - MMU_LCDC;
			else
				mem = (u8*)MMU_gpu_map(win->target);

			if(mem)
				{
				 u32 num2, num, y, x;
				 for(num2 = 0; num2<32; ++num2)
				 for(num = 0; num<64; ++num)
				 for(y = 0; y<8; ++y)
				 for(x = 0; x<4; ++x)
				 {
					  bitmap[(x<<1) + (y*512) + (num*8) +(num2*512*8)] = pal[mem[x + (y*4) + (num*32) +(num2*2048)]&0xF];
					  bitmap[(x<<1)+1 + (y*512) + (num*8) +(num2*512*8)] = pal[mem[x + (y*4) + (num*32) +(num2*2048)]>>4];
				 }        
				 SetDIBitsToDevice(mem_dc, 0, 0, 512, 256, 0, 0, 0, 256, bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
			}
        }
        else
             TextOut(mem_dc, 3, 3, "Il n'y a pas de palette", 23);
        
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK TileViewBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_INITDIALOG:
			return 1;
		case WM_NCCREATE:
			return 1;
		case WM_NCDESTROY:
			return 1;
		case WM_PAINT:
			switch(TileView->coul)
			{
				 case 0 :
					  TileViewBox_Direct(hwnd, TileView, wParam, lParam);
					  break;
				 case 1 :
					  TileViewBox_Pal256(hwnd, TileView, wParam, lParam);
					  break;
				 case 2 :
					  TileViewBox_Pal16(hwnd, TileView, wParam, lParam);
					  break;
			}
			break;
		case WM_LBUTTONDOWN :
			switch(TileView->coul)
			{
				 case 0 :
				 case 1 :
					  if(LOWORD(lParam)<(32*8))
					  {
						   TileView->x = ((LOWORD(lParam)>>3)<<3);
						   TileView->y = (HIWORD(lParam)>>3)<<3;
						   TileView->tilenum = (LOWORD(lParam)>>3) + (HIWORD(lParam)>>3)*32;
					  }
					  break;
				 case 2 :
					  TileView->x = ((LOWORD(lParam)>>3)<<3);
					  TileView->y = (HIWORD(lParam)>>3)<<3;
					  TileView->tilenum = (LOWORD(lParam)>>3) + (HIWORD(lParam)>>3)*64;
					  break;
			}
			InvalidateRect(GetDlgItem(hwnd, IDC_MINI_TILE), NULL, FALSE);
			return 1;
		case WM_ERASEBKGND:
			return 1;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT MiniTileViewBox_Paint(HWND hwnd, tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd_src = GetDlgItem(GetParent(hwnd), IDC_Tile_BOX);
        HDC          hdc_src;
        HDC          hdc_dst;
        char txt[80];
        
        PAINTSTRUCT  ps;

        hdc_dst = BeginPaint(hwnd, &ps);
        hdc_src = GetDC(hwnd_src);
        StretchBlt(hdc_dst, 0, 0, 80, 80, hdc_src, win->x, win->y, 8, 8, SRCCOPY);
        sprintf(txt, "Tile num : 0x%X", win->tilenum);
        SetWindowText(GetDlgItem(win->hwnd, IDC_TILENUM), txt);        
        EndPaint(hwnd, &ps);
        return 0;
}

LRESULT CALLBACK MiniTileViewBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_NCCREATE:
			return 1;
		case WM_NCDESTROY:
			return 1;
		case WM_PAINT :
			MiniTileViewBox_Paint(hwnd, TileView, wParam, lParam);
			break;
		case WM_ERASEBKGND:
			return 1;
		default:
			   break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

BOOL CALLBACK ViewTilesProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//bail out early if the dialog isnt initialized
	if(!TileView && message != WM_INITDIALOG)
		return false;

	switch (message)
     {
            case WM_INITDIALOG :
                 {
						TileView = new tileview_struct;
						memset(TileView, 0, sizeof(tileview_struct));
						TileView->hwnd = hwnd;
						TileView->target = MMU_ABG;
						TileView->pal = ((u16 *)MMU.ARM9_VMEM);
						TileView->autoup_secs = 1;
						SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
								UDM_SETRANGE, 0, MAKELONG(99, 1));
						SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
								UDM_SETPOS32, 0, TileView->autoup_secs);

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
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture PAL 0");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture PAL 1");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture PAL 2");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"Texture PAL 3");
                      SendMessage(combo, CB_SETCURSEL, 0, 0);

                      combo = GetDlgItem(hwnd, IDC_MEM_SELECT);
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6000000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6010000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6020000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6030000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6040000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6050000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6060000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-BG - 0x6070000");
                      
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"B-BG - 0x6200000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"B-BG - 0x6210000");

                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-OBJ- 0x6400000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-OBJ- 0x6410000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-OBJ- 0x6420000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"A-OBJ- 0x6430000");

                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"B-OBJ- 0x6600000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"B-OBJ- 0x6610000");

                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6800000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6810000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6820000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6830000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6840000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6850000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6860000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6870000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6880000");
                      SendMessage(combo, CB_ADDSTRING, 0,(LPARAM)"LCD - 0x6890000");
                      SendMessage(combo, CB_SETCURSEL, 0, 0);
                      SendMessage(GetDlgItem(hwnd, IDC_BITMAP), BM_SETCHECK, TRUE, 0);
                 }
                 return 1;
            case WM_CLOSE :
				if(TileView->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_TILE);
					TileView->autoup = false;
				}
				delete TileView;
				TileView = NULL;
				//INFO("Close Tile view dialog\n");
				PostQuitMessage(0);
                 return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;
            case WM_HSCROLL :
                 switch LOWORD(wParam)
                 {
                      case SB_LINERIGHT :
                           ++(TileView->palnum);
                           if(TileView->palnum>15)
                                TileView->palnum = 15;
						   TileView->setPalnum(TileView->palnum);
                           break;
                      case SB_LINELEFT :
                           --(TileView->palnum);
                           if(TileView->palnum<0)
                                TileView->palnum = 0;
						   TileView->setPalnum(TileView->palnum);
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
							 if(TileView->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_TILE);
                                  TileView->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             TileView->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_TILE, TileView->autoup_secs*20, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (!TileView) 
								{
									SendMessage(hwnd, WM_INITDIALOG, 0, 0);
								}
								if (t != TileView->autoup_secs)
								{
									TileView->autoup_secs = t;
									if (TileView->autoup)
										SetTimer(hwnd, IDT_VIEW_TILE, 
												TileView->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
                        case IDC_BITMAP :
                             TileView->coul = 0;
                             InvalidateRect(hwnd, NULL, FALSE);
                             return 1;
                        case IDC_256COUL :
                             TileView->coul = 1;
                             InvalidateRect(hwnd, NULL, FALSE);
                             return 1;
                        case IDC_16COUL :
                             TileView->coul = 2;
                             InvalidateRect(hwnd, NULL, FALSE);
                             return 1;
                        case IDC_MEM_SELECT :
                             switch(HIWORD(wParam))
                             {
                                  case CBN_SELCHANGE :
                                  case CBN_CLOSEUP :
                                       {
                                            u32 sel = SendMessage(GetDlgItem(hwnd, IDC_MEM_SELECT), CB_GETCURSEL, 0, 0);
                                            switch(sel)
                                            {
                                                 case 0 :
                                                 case 1 :
                                                 case 2 :
                                                 case 3 :
                                                 case 4 :
                                                 case 5 :
                                                 case 6 :
                                                 case 7 :
                                                      TileView->target = MMU_ABG + 0x10000*sel;
                                                      break;
                                                 case 8 :
                                                 case 9 :
                                                      TileView->target = MMU_BBG + 0x10000*(sel-8);
                                                      break;
                                                 case 10 :
                                                 case 11 :
                                                 case 12 :
                                                 case 13 :
                                                      TileView->target = MMU_AOBJ + 0x10000*(sel-10);
                                                      break;
                                                 case 14 :
                                                 case 15 :
                                                      TileView->target = MMU_BOBJ + 0x10000*(sel-14);
                                                      break;
                                                 case 16 :
                                                 case 17 :
                                                 case 18 :
                                                 case 19 :
                                                 case 20 :
                                                 case 21 :
                                                 case 22 :
                                                 case 23 :
                                                 case 24 :
                                                 case 25 :
                                                      TileView->target = MMU_LCDC + 0x10000*(sel-16);
                                                      break;
                                                 default :
                                                         return 1;
                                            }
                                            InvalidateRect(hwnd, NULL, FALSE);
                                            return 1;
                                       }
                             }
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
                                                      TileView->pal = (u16 *)MMU.ARM9_VMEM;
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 1 :
                                                      TileView->pal = ((u16 *)MMU.ARM9_VMEM) + 0x200;
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 2 :
                                                      TileView->pal = (u16 *)MMU.ARM9_VMEM + 0x100;
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 3 :
                                                      TileView->pal = ((u16 *)MMU.ARM9_VMEM) + 0x300;
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 4 :
                                                 case 5 :
                                                 case 6 :
                                                 case 7 :
                                                      TileView->pal = ((u16 *)(MMU.ExtPal[0][sel-4]));
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), FALSE);
                                                      if(TileView->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           TileView->coul = 1;
                                                      }
                                                      break;
                                                 case 8 :
                                                 case 9 :
                                                 case 10 :
                                                 case 11 :
                                                      TileView->pal = ((u16 *)(MMU.ExtPal[1][sel-8]));
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), FALSE);
                                                      if(TileView->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           TileView->coul = 1;
                                                      }
                                                      break;
                                                 case 12 :
                                                 case 13 :
                                                      TileView->pal = ((u16 *)(MMU.ObjExtPal[0][sel-12]));
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      if(TileView->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           TileView->coul = 1;
                                                      }
                                                      break;
                                                 case 14 :
                                                 case 15 :
                                                      TileView->pal = ((u16 *)(MMU.ObjExtPal[1][sel-14]));
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      if(TileView->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           TileView->coul = 1;
                                                      }
                                                      break;
                                                 case 16 :
                                                 case 17 :
                                                 case 18 :
                                                 case 19 :
                                                      TileView->pal = ((u16 *)(MMU.texInfo.texPalSlot[sel-16]));
                                                      TileView->palnum = 0;
													  TileView->setPalnum(TileView->palnum);
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 default :
                                                         return 1;
                                            }
                                            InvalidateRect(hwnd, NULL, FALSE);
                                            return 1;
                                       }
                             }
                 }
                 return 0;
     }
	return FALSE;
}
