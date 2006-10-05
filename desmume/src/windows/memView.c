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
#include "memView.h"
#include "../MMU.h"
#include "resource.h"

LRESULT CALLBACK MemViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

void InitMemViewBox()
{
     WNDCLASSEX wc;
     
     wc.cbSize         = sizeof(wc);
     wc.lpszClassName  = _T("MemViewBox");
     wc.hInstance      = GetModuleHandle(0);
     wc.lpfnWndProc    = MemViewBoxWndProc;
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

/*
LRESULT MemViewBox_OnPaint(CMemView * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_MEM_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
        SIZE fontsize;
        TCHAR text[80];
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        int lg = rect.right - rect.left;
        int ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, lg, ht);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        SelectObject(mem_dc, GetStockObject(SYSTEM_FIXED_FONT));
        
        GetTextExtentPoint32(mem_dc, "0", 1, &fontsize);
        
        int nbligne = ht/fontsize.cy;
        
        SetTextColor(mem_dc, RGB(0,0,0));
        
        RECT r;
        
        r.top = 3;
        r.left = 3;
        r.bottom = r.top+fontsize.cy;
        r.right = rect.right-3;
        
        u32 adr = win->curr_ligne*0x10;
        
        for(int i=0; i<nbligne; ++i)
        {
                sprintf(text, "%04X:%04X", (int)(adr>>16), (int)(adr&0xFFFF));
                DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                r.left += 11*fontsize.cx;
                int j;

                if(win->representation == 0)
                     for(j=0; j<16; ++j)
                     {
                          sprintf(text, "%02X", MMU_readByte(win->cpu, adr+j));
                          DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                          r.left+=3*fontsize.cx;
                     }
                else
                     if(win->representation == 1)
                          for(j=0; j<16; j+=2)
                          {
                               sprintf(text, "%04X", MMU_readHWord(win->cpu, adr+j));
                               DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                               r.left+=5*fontsize.cx;
                          }
                     else
                          for(j=0; j<16; j+=4)
                          {
                               sprintf(text, "%08X", (int)MMU_readWord(win->cpu, adr+j));
                               DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                               r.left+=9*fontsize.cx;
                          }                     
                
                r.left+=fontsize.cx;
                 
                for(j=0; j<16; ++j)
                {
                        u8 c = MMU_readByte(win->cpu, adr+j);
                        if(c >= 32 && c <= 127) {
                             text[j] = (char)c;
                        }
                        else
                             text[j] = '.';
                }
                text[j] = '\0';
                DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                
                adr+=0x10;
                r.top += fontsize.cy;
                r.bottom += fontsize.cy;
                r.left = 3;
        }
                
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 1;
}

LRESULT CALLBACK MemViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
        CMemView * win = (CMemView *)GetWindowLong(hwnd, 0);
        
        switch(msg)
        {
                   case WM_NCCREATE:
                        SetScrollRange(hwnd, SB_VERT, 0, 0x0FFFFFFF, TRUE);
                        SetScrollPos(hwnd, SB_VERT, 10, TRUE);
                        return 1;
                        
                   case WM_NCDESTROY:
                        return 1;
                   
                   case WM_PAINT:
                        MemViewBox_OnPaint(win, wParam, lParam);
                        return 1;
                   
                   case WM_ERASEBKGND:
		                return 1;
                   case WM_VSCROLL :
                        {
                             RECT rect;
                             SIZE fontsize;
                             GetClientRect(hwnd, &rect);
                             HDC dc = GetDC(hwnd);
                             HFONT old = (HFONT)SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
                             GetTextExtentPoint32(dc, "0", 1, &fontsize);
        
                             int nbligne = (rect.bottom - rect.top)/fontsize.cy;

                             switch LOWORD(wParam)
                             {
                                  case SB_LINEDOWN :
                                       win->curr_ligne = min(0X0FFFFFFFF, win->curr_ligne+1);
                                       break;
                                  case SB_LINEUP :
                                       win->curr_ligne = (u32)max(0, (s32)win->curr_ligne-1);
                                       break;
                                  case SB_PAGEDOWN :
                                       win->curr_ligne = min(0X0FFFFFFFF, win->curr_ligne+nbligne);
                                       break;
                                  case SB_PAGEUP :
                                       win->curr_ligne = (u32)max(0, (s32)win->curr_ligne-nbligne);
                                       break;
                              }
                              
                              SelectObject(dc, old);
                              SetScrollPos(hwnd, SB_VERT, win->curr_ligne, TRUE);
                              InvalidateRect(hwnd, NULL, FALSE);
                              UpdateWindow(hwnd);
                        }
                        return 1;
                                                
                   default:
                           break;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
}


// MEM VIEWER
BOOL CALLBACK mem_view_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     CMemView * win = (CMemView *)GetWindowLong(hwnd, DWL_USER);
     switch (message)
     {
            case WM_INITDIALOG :
                 SendMessage(GetDlgItem(hwnd, IDC_8_BIT), BM_SETCHECK, TRUE, 0);
                 return 1;
            case WM_CLOSE :
                 win->remove2RefreshList();
                 delete win;
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_8_BIT :
                             win->representation = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX));
                             return 1;
                        case IDC_16_BIT :
                             win->representation = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
                        case IDC_32_BIT :
                             win->representation = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
                        case IDC_AUTO_UPDATE :
                             if(win->autoup)
                             {
                                  win->remove2RefreshList();
                                  win->autoup = FALSE;
                                  return 1;
                             }
                             win->add2RefreshList();
                             win->autoup = TRUE;
                             return 1;
                        case IDC_GO :
                             {
                             char tmp[8];
                             int lg = GetDlgItemText(hwnd, IDC_GOTOMEM, tmp, 8);
                             u32 adr = 0;
                             for(u16 i = 0; i<lg; ++i)
                             {
                                  if((tmp[i]>='A')&&(tmp[i]<='F'))
                                  {
                                       adr = adr*16 + (tmp[i]-'A'+10);
                                       continue;
                                  }         
                                  if((tmp[i]>='0')&&(tmp[i]<='9'))
                                  {
                                       adr = adr*16 + (tmp[i]-'0');
                                       continue;
                                  }         
                             } 
                             win->curr_ligne = (adr>>4);
                             InvalidateRect(hwnd, NULL, FALSE);
                             UpdateWindow(hwnd);
                             }
                             return 1;
                        case IDC_FERMER :
                             win->remove2RefreshList();
                             delete win;
                             EndDialog(hwnd, 0);
                             return 1;
                 }
                 return 0;
     }
     return 0;    
}

CMemView::CMemView(HINSTANCE hInst, HWND parent, char * titre, u8 CPU) :
           CWindow(hInst, parent, titre, IDD_MEM_VIEWER, mem_view_proc), cpu(CPU), curr_ligne(0), representation(0)
{
     SetWindowLong(GetDlgItem(hwnd, IDC_MEM_BOX), 0, (LONG)this);
}
*/

//////////////////////////////////////////////////////////////////////////////

LRESULT MemViewBox_OnPaint(memview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_MEM_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
        SIZE fontsize;
        TCHAR text[80];
        int i;
        
        RECT rect;
        GetClientRect(hwnd, &rect);
        int lg = rect.right - rect.left;
        int ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        HDC mem_dc = CreateCompatibleDC(hdc);
        HBITMAP mem_bmp = CreateCompatibleBitmap(hdc, lg, ht);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        SelectObject(mem_dc, GetStockObject(SYSTEM_FIXED_FONT));
        
        GetTextExtentPoint32(mem_dc, "0", 1, &fontsize);
        
        int nbligne = ht/fontsize.cy;
        
        SetTextColor(mem_dc, RGB(0,0,0));
        
        RECT r;
        
        r.top = 3;
        r.left = 3;
        r.bottom = r.top+fontsize.cy;
        r.right = rect.right-3;
        
        u32 adr = win->curr_ligne*0x10;
        
        for(i=0; i<nbligne; ++i)
        {
                sprintf(text, "%04X:%04X", (int)(adr>>16), (int)(adr&0xFFFF));
                DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                r.left += 11*fontsize.cx;
                int j;

                if(win->representation == 0)
                     for(j=0; j<16; ++j)
                     {
                          sprintf(text, "%02X", MMU_readByte(win->cpu, adr+j));
                          DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                          r.left+=3*fontsize.cx;
                     }
                else
                     if(win->representation == 1)
                          for(j=0; j<16; j+=2)
                          {
                               sprintf(text, "%04X", MMU_readHWord(win->cpu, adr+j));
                               DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                               r.left+=5*fontsize.cx;
                          }
                     else
                          for(j=0; j<16; j+=4)
                          {
                               sprintf(text, "%08X", (int)MMU_readWord(win->cpu, adr+j));
                               DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                               r.left+=9*fontsize.cx;
                          }                     
                
                r.left+=fontsize.cx;
                 
                for(j=0; j<16; ++j)
                {
                        u8 c = MMU_readByte(win->cpu, adr+j);
                        if(c >= 32 && c <= 127) {
                             text[j] = (char)c;
                        }
                        else
                             text[j] = '.';
                }
                text[j] = '\0';
                DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                
                adr+=0x10;
                r.top += fontsize.cy;
                r.bottom += fontsize.cy;
                r.left = 3;
        }
                
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 1;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK MemViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
        memview_struct *win = (memview_struct *)GetWindowLong(hwnd, 0);
        
        switch(msg)
        {
                   case WM_NCCREATE:
                        SetScrollRange(hwnd, SB_VERT, 0, 0x0FFFFFFF, TRUE);
                        SetScrollPos(hwnd, SB_VERT, 10, TRUE);
                        return 1;
                        
                   case WM_NCDESTROY:
                        return 1;
                   
                   case WM_PAINT:
                        MemViewBox_OnPaint(win, wParam, lParam);
                        return 1;
                   
                   case WM_ERASEBKGND:
		                return 1;
                   case WM_VSCROLL :
                        {
                             RECT rect;
                             SIZE fontsize;
                             GetClientRect(hwnd, &rect);
                             HDC dc = GetDC(hwnd);
                             HFONT old = (HFONT)SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
                             GetTextExtentPoint32(dc, "0", 1, &fontsize);
        
                             int nbligne = (rect.bottom - rect.top)/fontsize.cy;

                             switch LOWORD(wParam)
                             {
                                  case SB_LINEDOWN :
                                       win->curr_ligne = min(0X0FFFFFFFF, win->curr_ligne+1);
                                       break;
                                  case SB_LINEUP :
                                       win->curr_ligne = (u32)max(0, (s32)win->curr_ligne-1);
                                       break;
                                  case SB_PAGEDOWN :
                                       win->curr_ligne = min(0X0FFFFFFFF, win->curr_ligne+nbligne);
                                       break;
                                  case SB_PAGEUP :
                                       win->curr_ligne = (u32)max(0, (s32)win->curr_ligne-nbligne);
                                       break;
                              }
                              
                              SelectObject(dc, old);
                              SetScrollPos(hwnd, SB_VERT, win->curr_ligne, TRUE);
                              InvalidateRect(hwnd, NULL, FALSE);
                              UpdateWindow(hwnd);
                        }
                        return 1;
                                                
                   default:
                           break;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////

// MEM VIEWER
BOOL CALLBACK MemView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     memview_struct *win = (memview_struct *)GetWindowLong(hwnd, DWL_USER);

     switch (message)
     {
            case WM_INITDIALOG :
                 SendMessage(GetDlgItem(hwnd, IDC_8_BIT), BM_SETCHECK, TRUE, 0);
                 return 1;
            case WM_CLOSE :
                 CWindow_RemoveFromRefreshList(win);
                 MemView_Deinit(win);
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_8_BIT :
                             win->representation = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX));
                             return 1;
                        case IDC_16_BIT :
                             win->representation = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
                        case IDC_32_BIT :
                             win->representation = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
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
                        case IDC_GO :
                             {
                             char tmp[8];
                             int lg = GetDlgItemText(hwnd, IDC_GOTOMEM, tmp, 8);
                             u32 adr = 0;
                             u16 i;

                             for(i = 0; i<lg; ++i)
                             {
                                  if((tmp[i]>='A')&&(tmp[i]<='F'))
                                  {
                                       adr = adr*16 + (tmp[i]-'A'+10);
                                       continue;
                                  }         
                                  if((tmp[i]>='0')&&(tmp[i]<='9'))
                                  {
                                       adr = adr*16 + (tmp[i]-'0');
                                       continue;
                                  }         
                             } 
                             win->curr_ligne = (adr>>4);
                             InvalidateRect(hwnd, NULL, FALSE);
                             UpdateWindow(hwnd);
                             }
                             return 1;
                        case IDC_FERMER :
                             CWindow_RemoveFromRefreshList(win);
                             MemView_Deinit(win);
                             EndDialog(hwnd, 0);
                             return 1;
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

memview_struct *MemView_Init(HINSTANCE hInst, HWND parent, char *title, u8 CPU)
{
   memview_struct *MemView = NULL;

   if ((MemView = (memview_struct *)malloc(sizeof(memview_struct))) == NULL)
      return MemView;

   if (CWindow_Init2(MemView, hInst, parent, title, IDD_MEM_VIEWER, MemView_Proc) != 0)
   {
      free(MemView);
      return NULL;
   }

   MemView->cpu = CPU;
   MemView->curr_ligne = 0;
   MemView->representation = 0;

   SetWindowLong(GetDlgItem(MemView->hwnd, IDC_MEM_BOX), 0, (LONG)MemView);

   return MemView;
}

//////////////////////////////////////////////////////////////////////////////

void MemView_Deinit(memview_struct *MemView)
{
   if (MemView)
      free(MemView);
}

//////////////////////////////////////////////////////////////////////////////
