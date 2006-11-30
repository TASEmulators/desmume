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
#include <tchar.h>
#include <stdio.h>
#include "tileView.h"
#include "resource.h"
#include "../MMU.h"

LRESULT CALLBACK TileViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK MiniTileViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

//////////////////////////////////////////////////////////////////////////////

void InitTileViewBox()
{
     WNDCLASSEX wc;
     
     wc.cbSize         = sizeof(wc);
     wc.lpszClassName  = _T("TileViewBox");
     wc.hInstance      = GetModuleHandle(0);
     wc.lpfnWndProc    = TileViewBoxWndProc;
     wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
     wc.hIcon          = 0;
     wc.lpszMenuName   = 0;
     wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
     wc.style          = 0;
     wc.cbClsExtra     = 0;
     wc.cbWndExtra     = sizeof(cwindow_struct *);
     wc.hIconSm        = 0;
     
     RegisterClassEx(&wc);
     
     wc.cbSize         = sizeof(wc);
     wc.lpszClassName  = _T("MiniTileViewBox");
     wc.hInstance      = GetModuleHandle(0);
     wc.lpfnWndProc    = MiniTileViewBoxWndProc;
     wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
     wc.hIcon          = 0;
     wc.lpszMenuName   = 0;
     wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
     wc.style          = 0;
     wc.cbClsExtra     = 0;
     wc.cbWndExtra     = sizeof(cwindow_struct *);
     wc.hIconSm        = 0;
     
     RegisterClassEx(&wc);
}

//////////////////////////////////////////////////////////////////////////////

LRESULT MiniTileViewBox_Paint(tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd_dst = GetDlgItem(win->hwnd, IDC_MINI_TILE);
        HWND         hwnd_src = GetDlgItem(win->hwnd, IDC_Tile_BOX);
        HDC          hdc_src;
        HDC          hdc_dst;
        char txt[80];
        
        PAINTSTRUCT  ps;

        hdc_dst = BeginPaint(hwnd_dst, &ps);
        hdc_src = GetDC(hwnd_src);
        StretchBlt(hdc_dst, 0, 0, 80, 80, hdc_src, win->x, win->y, 8, 8, SRCCOPY);
        sprintf(txt, "Tile num : 0x%X", win->tilenum);
        SetWindowText(GetDlgItem(win->hwnd, IDC_TILENUM), txt);        
        EndPaint(hwnd_dst, &ps);
        return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK MiniTileViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
        tileview_struct * win = (tileview_struct *)GetWindowLong(hwnd, 0);
        
        switch(msg)
        {
                   case WM_NCCREATE:
                        return 1;
                   case WM_NCDESTROY:
                        return 1;
                   case WM_PAINT :
                        MiniTileViewBox_Paint(win, wParam, lParam);
                        return 1;
                   case WM_ERASEBKGND:
		                return 1;
                   default:
                           break;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////

LRESULT TileViewBox_Direct(tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_Tile_BOX);
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
        
        SetDIBitsToDevice(mem_dc, 0, 0, 256, 256, 0, 0, 0, 256, win->mem, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
        
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT TileViewBox_Pal256(tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_Tile_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
//        SIZE fontsize;
        TCHAR text[80];
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
             u32 i, num2, num, y, x;

             //for(i = 0; i<256*256; ++i)
             //     bitmap[i] = pal[win->mem[i]];
             for(num2 = 0; num2<32; ++num2)
             for(num = 0; num<32; ++num)
             for(y = 0; y<8; ++y)
             for(x = 0; x<8; ++x)
                  bitmap[x + (y*256) + (num*8) +(num2*256*8)] = pal[win->mem[x + (y*8) + (num*64) +(num2*2048)]];        
             SetDIBitsToDevice(mem_dc, 0, 0, 256, 256, 0, 0, 0, 256, bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
             sprintf(text, "Pal : %d", win->palnum);
             SetWindowText(GetDlgItem(win->hwnd, IDC_PALNUM), text);
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

LRESULT TileViewBox_Pal16(tileview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_Tile_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
//        SIZE fontsize;
        TCHAR text[80];
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
             u32 num2, num, y, x;
             for(num2 = 0; num2<32; ++num2)
             for(num = 0; num<64; ++num)
             for(y = 0; y<8; ++y)
             for(x = 0; x<4; ++x)
             {
                  bitmap[(x<<1) + (y*512) + (num*8) +(num2*512*8)] = pal[win->mem[x + (y*4) + (num*32) +(num2*2048)]&0xF];
                  bitmap[(x<<1)+1 + (y*512) + (num*8) +(num2*512*8)] = pal[win->mem[x + (y*4) + (num*32) +(num2*2048)]>>4];
             }        
             SetDIBitsToDevice(mem_dc, 0, 0, 512, 256, 0, 0, 0, 256, bitmap, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
             sprintf(text, "Pal : %d", win->palnum);
             SetWindowText(GetDlgItem(win->hwnd, IDC_PALNUM), text);
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

LRESULT CALLBACK TileViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
        tileview_struct * win = (tileview_struct *)GetWindowLong(hwnd, 0);
        
        switch(msg)
        {
                   case WM_NCCREATE:
                        return 1;
                   case WM_NCDESTROY:
                        return 1;
                   case WM_PAINT:
                        switch(win->coul)
                        {
                             case 0 :
                                  TileViewBox_Direct(win, wParam, lParam);
                                  return 1;
                             case 1 :
                                  TileViewBox_Pal256(win, wParam, lParam);
                                  return 1;
                             case 2 :
                                  TileViewBox_Pal16(win, wParam, lParam);
                                  return 1;
                        }
                        return 1;
                   case WM_LBUTTONDOWN :
                        switch(win->coul)
                        {
                             case 0 :
                             case 1 :
                                  if(LOWORD(lParam)<(32*8))
                                  {
                                       win->x = ((LOWORD(lParam)>>3)<<3);
                                       win->y = (HIWORD(lParam)>>3)<<3;
                                       win->tilenum = (LOWORD(lParam)>>3) + (HIWORD(lParam)>>3)*32;
                                  }
                                  break;
                             case 2 :
                                  win->x = ((LOWORD(lParam)>>3)<<3);
                                  win->y = (HIWORD(lParam)>>3)<<3;
                                  win->tilenum = (LOWORD(lParam)>>3) + (HIWORD(lParam)>>3)*64;
                                  break;
                        }
                        InvalidateRect(GetDlgItem(win->hwnd, IDC_MINI_TILE), NULL, FALSE);
                        UpdateWindow(GetDlgItem(win->hwnd, IDC_MINI_TILE)); 
                        //CWindow_Refresh(win);
                        return 1;
                   case WM_ERASEBKGND:
		                return 1;
                   default:
                           break;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK TileView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     tileview_struct * win = (tileview_struct *)GetWindowLong(hwnd, DWL_USER);
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
                 CWindow_RemoveFromRefreshList(win);
                 TileView_Deinit(win);
                 EndDialog(hwnd, 0);
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
                             TileView_Deinit(win);
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
                        case IDC_BITMAP :
                             win->coul = 0;
                             CWindow_Refresh(win);
                             return 1;
                        case IDC_256COUL :
                             win->coul = 1;
                             CWindow_Refresh(win);
                             return 1;
                        case IDC_16COUL :
                             win->coul = 2;
                             CWindow_Refresh(win);
                             return 1;
                        case IDC_MEM_SELECT :
                             switch(HIWORD(wParam))
                             {
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
                                                      win->mem = ARM9Mem.ARM9_ABG + 0x10000*sel;
                                                      break;
                                                 case 8 :
                                                 case 9 :
                                                      win->mem = ARM9Mem.ARM9_BBG + 0x10000*(sel-8);
                                                      break;
                                                 case 10 :
                                                 case 11 :
                                                 case 12 :
                                                 case 13 :
                                                      win->mem = ARM9Mem.ARM9_AOBJ + 0x10000*(sel-10);
                                                      break;
                                                 case 14 :
                                                 case 15 :
                                                      win->mem = ARM9Mem.ARM9_BOBJ + 0x10000*(sel-14);
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
                                                      win->mem = ARM9Mem.ARM9_LCD + 0x10000*(sel-16);
                                                      break;
                                                 default :
                                                         return 1;
                                            }
                                            CWindow_Refresh(win);
                                            return 1;
                                       }
                             }
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
                                                      win->pal = (u16 *)ARM9Mem.ARM9_VMEM;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 1 :
                                                      win->pal = ((u16 *)ARM9Mem.ARM9_VMEM) + 0x200;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 2 :
                                                      win->pal = (u16 *)ARM9Mem.ARM9_VMEM + 0x100;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 3 :
                                                      win->pal = ((u16 *)ARM9Mem.ARM9_VMEM) + 0x300;
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 case 4 :
                                                 case 5 :
                                                 case 6 :
                                                 case 7 :
                                                      win->pal = ((u16 *)(ARM9Mem.ExtPal[0][sel-4]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), FALSE);
                                                      if(win->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           win->coul = 1;
                                                      }
                                                      break;
                                                 case 8 :
                                                 case 9 :
                                                 case 10 :
                                                 case 11 :
                                                      win->pal = ((u16 *)(ARM9Mem.ExtPal[1][sel-8]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_HIDE);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), FALSE);
                                                      if(win->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           win->coul = 1;
                                                      }
                                                      break;
                                                 case 12 :
                                                 case 13 :
                                                      win->pal = ((u16 *)(ARM9Mem.ObjExtPal[0][sel-12]));
                                                      win->palnum = 0;
                                                      if(win->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           win->coul = 1;
                                                      }
                                                      break;
                                                 case 14 :
                                                 case 15 :
                                                      win->pal = ((u16 *)(ARM9Mem.ObjExtPal[1][sel-14]));
                                                      win->palnum = 0;
                                                      if(win->coul == 2)
                                                      {
                                                           SendMessage(GetDlgItem(hwnd, IDC_256COUL), BM_SETCHECK, TRUE, 0);
                                                           SendMessage(GetDlgItem(hwnd, IDC_16COUL), BM_SETCHECK, FALSE, 0);
                                                           win->coul = 1;
                                                      }
                                                      break;
                                                 case 16 :
                                                 case 17 :
                                                 case 18 :
                                                 case 19 :
                                                      win->pal = ((u16 *)(ARM9Mem.texPalSlot[sel-16]));
                                                      win->palnum = 0;
                                                      ShowWindow(GetDlgItem(hwnd, IDC_16COUL), SW_SHOW);
                                                      EnableWindow(GetDlgItem(hwnd, IDC_16COUL), TRUE);
                                                      break;
                                                 default :
                                                         return 1;
                                            }
                                            CWindow_Refresh(win);
                                            return 1;
                                       }
                             }
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

tileview_struct *TileView_Init(HINSTANCE hInst, HWND parent)
{
   tileview_struct *TileView=NULL;

   if ((TileView = (tileview_struct *)malloc(sizeof(tileview_struct))) == NULL)
      return TileView;

   if (CWindow_Init2(TileView, hInst, parent, "Tile viewer", IDD_TILE, TileView_Proc) != 0)
   {
      free(TileView);
      return NULL;
   }

   TileView->mem = ARM9Mem.ARM9_ABG;
   TileView->pal = ((u16 *)ARM9Mem.ARM9_VMEM);
   TileView->palnum = 0;
   TileView->coul = 0;
   TileView->x = 0;
   TileView->y = 0;

   SetWindowLong(GetDlgItem(TileView->hwnd, IDC_Tile_BOX), 0, (LONG)TileView);
   SetWindowLong(GetDlgItem(TileView->hwnd, IDC_MINI_TILE), 0, (LONG)TileView);

   return TileView;
}

//////////////////////////////////////////////////////////////////////////////

void TileView_Deinit(tileview_struct *TileView)
{
   if (TileView)
      free(TileView);
}

//////////////////////////////////////////////////////////////////////////////
