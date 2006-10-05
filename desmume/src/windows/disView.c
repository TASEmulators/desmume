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
#include "../MMU.h"
#include "../Disassembler.h"
#include "disView.h"
#include "resource.h"

#define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))

extern HDC  hdc;

LRESULT CALLBACK DisViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

u32 cycles = 1;

void InitDesViewBox()
{
     WNDCLASSEX wc;
     
     wc.cbSize         = sizeof(wc);
     wc.lpszClassName  = _T("DesViewBox");
     wc.hInstance      = GetModuleHandle(0);
     wc.lpfnWndProc    = DisViewBoxWndProc;
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
LRESULT DesViewBox_OnPaint(CDesView * win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_DES_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
        SIZE fontsize;
        TCHAR text[100];
        TCHAR txt[100];
        
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
        
        u32  nbligne = ht/fontsize.cy;
        
        SetTextColor(mem_dc, RGB(0,0,0));
        
        if((win->mode==1) || ((win->mode==0) && (win->cpu->CPSR.bits.T == 0)))
        {
             u32 adr = win->curr_ligne*4;
        
             for(u32 i = 0; i < nbligne; ++i)
             {
                  u32 ins = MMU_readWord(win->cpu->proc_ID, adr);
                  des_arm_instructions_set[INDEX(ins)](adr, ins, txt);
                  sprintf(text, "%04X:%04X  %08X  %s", (int)(adr>>16), (int)(adr&0xFFFF), (int)ins, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  rect.top+=fontsize.cy;
                  adr += 4;
             }
        
             if(((win->cpu->instruct_adr&0x0FFFFFFF) >= win->curr_ligne<<2)&&((win->cpu->instruct_adr&0x0FFFFFFF) <= (win->curr_ligne+nbligne<<2)))
             {
                  HBRUSH brjaune = CreateSolidBrush(RGB(255, 255, 0));
                  SetBkColor(mem_dc, RGB(255, 255, 0));
                  rect.top = (((win->cpu->instruct_adr&0x0FFFFFFF)>>2) - win->curr_ligne)*fontsize.cy;
                  rect.bottom = rect.top + fontsize.cy;
                  FillRect(mem_dc, &rect, brjaune);
                  des_arm_instructions_set[INDEX(win->cpu->instruction)](win->cpu->instruct_adr, win->cpu->instruction, txt);
                  sprintf(text, "%04X:%04X  %08X  %s", (int)((win->cpu->instruct_adr&0x0FFFFFFF)>>16), (int)(win->cpu->instruct_adr&0xFFFF), (int)win->cpu->instruction, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  DeleteObject(brjaune);
             }
        } 
        else
        {
             u32 adr = win->curr_ligne*2;
        
             for(u32 i = 0; i < nbligne; ++i)
             {
                  u32 ins = MMU_readHWord(win->cpu->proc_ID, adr);
                  des_thumb_instructions_set[ins>>6](adr, ins, txt);
                  sprintf(text, "%04X:%04X  %04X  %s", (int)(adr>>16), (int)(adr&0xFFFF), (int)ins, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  rect.top+=fontsize.cy;
                  adr += 2;
             }
        
             if(((win->cpu->instruct_adr&0x0FFFFFFF) >= win->curr_ligne<<1)&&((win->cpu->instruct_adr&0x0FFFFFFF) <= (win->curr_ligne+nbligne<<1)))
             {
                  HBRUSH brjaune = CreateSolidBrush(RGB(255, 255, 0));
                  SetBkColor(mem_dc, RGB(255, 255, 0));
                  
                  rect.top = (((win->cpu->instruct_adr&0x0FFFFFFF)>>1) - win->curr_ligne)*fontsize.cy;
                  rect.bottom = rect.top + fontsize.cy;
                  FillRect(mem_dc, &rect, brjaune);
                  des_thumb_instructions_set[((win->cpu->instruction)&0xFFFF)>>6](win->cpu->instruct_adr, win->cpu->instruction&0xFFFF, txt);
                  sprintf(text, "%04X:%04X  %04X  %s", (int)((win->cpu->instruct_adr&0x0FFFFFFF)>>16), (int)(win->cpu->instruct_adr&0xFFFF), (int)(win->cpu->instruction&0xFFFF), txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  DeleteObject(brjaune);
             }
        }
        
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 1;
}

LRESULT DesViewDialog_OnPaint(HWND hwnd, CDesView * win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        TCHAR        text[80];
        
        hdc = BeginPaint(hwnd, &ps);
        
        for(u32 i = 0; i < 16; ++i)
        {
             sprintf(text, "%08X", (int)win->cpu->R[i]);
             SetWindowText(GetDlgItem(hwnd, IDC_R0+i), text);
        }
        
        #define OFF 16
        
        SetBkMode(hdc, TRANSPARENT);
        if(win->cpu->CPSR.bits.N)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 452+OFF, 238, "N", 1);
        
        if(win->cpu->CPSR.bits.Z)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 464+OFF, 238, "Z", 1);
        
        if(win->cpu->CPSR.bits.C)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 475+OFF, 238, "C", 1);
        
        if(win->cpu->CPSR.bits.V)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 486+OFF, 238, "V", 1);
        
        if(win->cpu->CPSR.bits.Q)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 497+OFF, 238, "Q", 1);
        
        if(!win->cpu->CPSR.bits.I)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 508+OFF, 238, "I", 1);
        
        sprintf(text, "%02X", (int)win->cpu->CPSR.bits.mode);
        SetWindowText(GetDlgItem(hwnd, IDC_MODE), text);

        sprintf(text, "%08X", MMU.timer[0][0]);//win->cpu->SPSR);
        SetWindowText(GetDlgItem(hwnd, IDC_TMP), text);
        
        EndPaint(hwnd, &ps);
        return 1;
}

LRESULT CALLBACK DesViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
        CDesView * win = (CDesView *)GetWindowLong(hwnd, 0);
        
        switch(msg)
        {
                   case WM_NCCREATE:
                        SetScrollRange(hwnd, SB_VERT, 0, 0x3FFFFF7, TRUE);
                        SetScrollPos(hwnd, SB_VERT, 10, TRUE);
                        return 1;
                        
                   case WM_NCDESTROY:
                        free(win);
                        return 1;
                   
                   case WM_PAINT:
                        DesViewBox_OnPaint(win, wParam, lParam);
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
                                       win->curr_ligne = min(0x3FFFFF7*(1+win->cpu->CPSR.bits.T), win->curr_ligne+1);
                                       break;
                                  case SB_LINEUP :
                                       win->curr_ligne = (u32)max(0, (s32)win->curr_ligne-1);
                                       break;
                                  case SB_PAGEDOWN :
                                       win->curr_ligne = min(0x3FFFFF7*(1+win->cpu->CPSR.bits.T), win->curr_ligne+nbligne);
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
                                                
                   case WM_ERASEBKGND:
		                return 1;
                   default:
                           break;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
}


// DES VIEWER 
BOOL CALLBACK des_view_proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     CDesView * win = (CDesView *)GetWindowLong(hwnd, DWL_USER);

     switch (message)
     {
            case WM_INITDIALOG :
                 SetDlgItemInt(hwnd, IDC_SETPNUM, 1, FALSE);
                 SendMessage(GetDlgItem(hwnd, IDC_AUTO_DES), BM_SETCHECK, TRUE, 0);
                 return 1;
            case WM_CLOSE :
                 win->remove2RefreshList();
                 delete win;
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_PAINT:
                 DesViewDialog_OnPaint(hwnd, win, wParam, lParam);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             win->remove2RefreshList();
                             delete win;
                             EndDialog(hwnd, 0);
                             return 0;
                        case IDC_AUTO_DES :
                             win->mode = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX));
                             return 1;
                        case IDC_ARM :
                             win->mode = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX)); 
                             return 1;
                        case IDC_THUMB :
                             win->mode = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX)); 
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
                        case IDC_STEP :
                             {
     BITMAPV4HEADER bmi;

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
                                  int ndstep = GetDlgItemInt(hwnd, IDC_SETPNUM, NULL, FALSE);
                                  NDS_exec(ndstep, TRUE);
                                  if(!win->autoup) win->refresh();
                                  CWindow::refreshALL();
                                  SetDIBitsToDevice(hdc, 0, 0, 256, 192*2, 0, 0, 0, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
                             }
                             return 1;
                        case IDC_GO :
                             {
                             char tmp[8];
                             int lg = GetDlgItemText(hwnd, IDC_GOTODES, tmp, 8);
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
                             win->curr_ligne = adr>>2;
                             InvalidateRect(hwnd, NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX)); 
                             }
                             return 1;
                        return 1;
                 }
                 return 0;
     }
     return 0;    
}

CDesView::CDesView(HINSTANCE hInst, HWND parent, char * titre, armcpu_t * CPU) :
          CWindow(hInst, parent, titre, IDD_DESASSEMBLEUR_VIEWER, des_view_proc), cpu(CPU), mode(0)
{
     SetWindowLong(GetDlgItem(hwnd, IDC_DES_BOX), 0, (LONG)this);
}

void CDesView::refresh()
{
     if(((cpu->instruct_adr&0x0FFFFFFF)>>(2-cpu->CPSR.bits.T))>=8)
          if(((cpu->instruct_adr&0x0FFFFFFF)>>(2-cpu->CPSR.bits.T))<=0x3FFFFF7*(1+cpu->CPSR.bits.T))
               curr_ligne = ((cpu->instruct_adr&0x0FFFFFFF)>>(2-cpu->CPSR.bits.T))-8;
          else
               curr_ligne = 0x3FFFFEF*(1+cpu->CPSR.bits.T);
     else
          curr_ligne = 0;
     SetScrollRange(GetDlgItem(hwnd, IDC_DES_BOX), SB_VERT, 0, 0x3FFFFF7*(1+cpu->CPSR.bits.T), TRUE);
     SetScrollPos(GetDlgItem(hwnd, IDC_DES_BOX), SB_VERT, curr_ligne, TRUE);
     InvalidateRect(hwnd, NULL, FALSE);
}
*/
//////////////////////////////////////////////////////////////////////////////

LRESULT DisViewBox_OnPaint(disview_struct *win, WPARAM wParam, LPARAM lParam)
{
        HWND         hwnd = GetDlgItem(win->hwnd, IDC_DES_BOX);
        HDC          hdc;
        PAINTSTRUCT  ps;
        SIZE fontsize;
        TCHAR text[100];
        TCHAR txt[100];
        
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
        
        u32  nbligne = ht/fontsize.cy;
        
        SetTextColor(mem_dc, RGB(0,0,0));
        
        if((win->mode==1) || ((win->mode==0) && (win->cpu->CPSR.bits.T == 0)))
        {
             u32 i;
             u32 adr = win->curr_ligne*4;
        
             for(i = 0; i < nbligne; ++i)
             {
                  u32 ins = MMU_readWord(win->cpu->proc_ID, adr);
                  des_arm_instructions_set[INDEX(ins)](adr, ins, txt);
                  sprintf(text, "%04X:%04X  %08X  %s", (int)(adr>>16), (int)(adr&0xFFFF), (int)ins, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  rect.top+=fontsize.cy;
                  adr += 4;
             }
        
             if(((win->cpu->instruct_adr&0x0FFFFFFF) >= win->curr_ligne<<2)&&((win->cpu->instruct_adr&0x0FFFFFFF) <= (win->curr_ligne+nbligne<<2)))
             {
                  HBRUSH brjaune = CreateSolidBrush(RGB(255, 255, 0));
                  SetBkColor(mem_dc, RGB(255, 255, 0));
                  rect.top = (((win->cpu->instruct_adr&0x0FFFFFFF)>>2) - win->curr_ligne)*fontsize.cy;
                  rect.bottom = rect.top + fontsize.cy;
                  FillRect(mem_dc, &rect, brjaune);
                  des_arm_instructions_set[INDEX(win->cpu->instruction)](win->cpu->instruct_adr, win->cpu->instruction, txt);
                  sprintf(text, "%04X:%04X  %08X  %s", (int)((win->cpu->instruct_adr&0x0FFFFFFF)>>16), (int)(win->cpu->instruct_adr&0xFFFF), (int)win->cpu->instruction, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  DeleteObject(brjaune);
             }
        } 
        else
        {
             u32 i;
             u32 adr = win->curr_ligne*2;
        
             for(i = 0; i < nbligne; ++i)
             {
                  u32 ins = MMU_readHWord(win->cpu->proc_ID, adr);
                  des_thumb_instructions_set[ins>>6](adr, ins, txt);
                  sprintf(text, "%04X:%04X  %04X  %s", (int)(adr>>16), (int)(adr&0xFFFF), (int)ins, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  rect.top+=fontsize.cy;
                  adr += 2;
             }
        
             if(((win->cpu->instruct_adr&0x0FFFFFFF) >= win->curr_ligne<<1)&&((win->cpu->instruct_adr&0x0FFFFFFF) <= (win->curr_ligne+nbligne<<1)))
             {
                  HBRUSH brjaune = CreateSolidBrush(RGB(255, 255, 0));
                  SetBkColor(mem_dc, RGB(255, 255, 0));
                  
                  rect.top = (((win->cpu->instruct_adr&0x0FFFFFFF)>>1) - win->curr_ligne)*fontsize.cy;
                  rect.bottom = rect.top + fontsize.cy;
                  FillRect(mem_dc, &rect, brjaune);
                  des_thumb_instructions_set[((win->cpu->instruction)&0xFFFF)>>6](win->cpu->instruct_adr, win->cpu->instruction&0xFFFF, txt);
                  sprintf(text, "%04X:%04X  %04X  %s", (int)((win->cpu->instruct_adr&0x0FFFFFFF)>>16), (int)(win->cpu->instruct_adr&0xFFFF), (int)(win->cpu->instruction&0xFFFF), txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  DeleteObject(brjaune);
             }
        }
        
        BitBlt(hdc, 0, 0, lg, ht, mem_dc, 0, 0, SRCCOPY);
        
        DeleteDC(mem_dc);
        DeleteObject(mem_bmp);
        
        EndPaint(hwnd, &ps);
        return 1;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT DisViewDialog_OnPaint(HWND hwnd, disview_struct *win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        TCHAR        text[80];
        u32 i;
        
        hdc = BeginPaint(hwnd, &ps);
        
        for(i = 0; i < 16; ++i)
        {
             sprintf(text, "%08X", (int)win->cpu->R[i]);
             SetWindowText(GetDlgItem(hwnd, IDC_R0+i), text);
        }
        
        #define OFF 16
        
        SetBkMode(hdc, TRANSPARENT);
        if(win->cpu->CPSR.bits.N)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 452+OFF, 238, "N", 1);
        
        if(win->cpu->CPSR.bits.Z)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 464+OFF, 238, "Z", 1);
        
        if(win->cpu->CPSR.bits.C)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 475+OFF, 238, "C", 1);
        
        if(win->cpu->CPSR.bits.V)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 486+OFF, 238, "V", 1);
        
        if(win->cpu->CPSR.bits.Q)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 497+OFF, 238, "Q", 1);
        
        if(!win->cpu->CPSR.bits.I)
             SetTextColor(hdc, RGB(255,0,0));
        else
             SetTextColor(hdc, RGB(70, 70, 70));
        TextOut(hdc, 508+OFF, 238, "I", 1);
        
        sprintf(text, "%02X", (int)win->cpu->CPSR.bits.mode);
        SetWindowText(GetDlgItem(hwnd, IDC_MODE), text);

        sprintf(text, "%08X", MMU.timer[0][0]);//win->cpu->SPSR);
        SetWindowText(GetDlgItem(hwnd, IDC_TMP), text);
        
        EndPaint(hwnd, &ps);
        return 1;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT CALLBACK DisViewBoxWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
        disview_struct *win = (disview_struct *)GetWindowLong(hwnd, 0);
        
        switch(msg)
        {
                   case WM_NCCREATE:
                        SetScrollRange(hwnd, SB_VERT, 0, 0x3FFFFF7, TRUE);
                        SetScrollPos(hwnd, SB_VERT, 10, TRUE);
                        return 1;
                        
                   case WM_NCDESTROY:
                        free(win);
                        return 1;
                   
                   case WM_PAINT:
                        DisViewBox_OnPaint(win, wParam, lParam);
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
                                       win->curr_ligne = min(0x3FFFFF7*(1+win->cpu->CPSR.bits.T), win->curr_ligne+1);
                                       break;
                                  case SB_LINEUP :
                                       win->curr_ligne = (u32)max(0, (s32)win->curr_ligne-1);
                                       break;
                                  case SB_PAGEDOWN :
                                       win->curr_ligne = min(0x3FFFFF7*(1+win->cpu->CPSR.bits.T), win->curr_ligne+nbligne);
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
                                                
                   case WM_ERASEBKGND:
		                return 1;
                   default:
                           break;
        }
        
        return DefWindowProc(hwnd, msg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////

// DES VIEWER 
BOOL CALLBACK DisView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     disview_struct *win = (disview_struct *)GetWindowLong(hwnd, DWL_USER);

     switch (message)
     {
            case WM_INITDIALOG :
                 SetDlgItemInt(hwnd, IDC_SETPNUM, 1, FALSE);
                 SendMessage(GetDlgItem(hwnd, IDC_AUTO_DES), BM_SETCHECK, TRUE, 0);
                 return 1;
            case WM_CLOSE :
                 CWindow_RemoveFromRefreshList(win);
                 DisView_Deinit(win);
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_PAINT:
                 DisViewDialog_OnPaint(hwnd, win, wParam, lParam);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             CWindow_RemoveFromRefreshList(win);
                             DisView_Deinit(win);
                             EndDialog(hwnd, 0);
                             return 0;
                        case IDC_AUTO_DES :
                             win->mode = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX));
                             return 1;
                        case IDC_ARM :
                             win->mode = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX)); 
                             return 1;
                        case IDC_THUMB :
                             win->mode = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX)); 
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
                        case IDC_STEP :
                             {
     BITMAPV4HEADER bmi;

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
                                  int ndstep = GetDlgItemInt(hwnd, IDC_SETPNUM, NULL, FALSE);
                                  NDS_exec(ndstep, TRUE);
                                  if(!win->autoup) win->Refresh(win);
                                  CWindow_RefreshALL();
                                  SetDIBitsToDevice(hdc, 0, 0, 256, 192*2, 0, 0, 0, 192*2, GPU_screen, (BITMAPINFO*)&bmi, DIB_RGB_COLORS);
                             }
                             return 1;
                        case IDC_GO :
                             {
                             u16 i;
                             char tmp[8];
                             int lg = GetDlgItemText(hwnd, IDC_GOTODES, tmp, 8);
                             u32 adr = 0;
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
                             win->curr_ligne = adr>>2;
                             InvalidateRect(hwnd, NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_DES_BOX)); 
                             }
                             return 1;
                        return 1;
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

disview_struct *DisView_Init(HINSTANCE hInst, HWND parent, char *title, armcpu_t *CPU)
{
   disview_struct *DisView=NULL;

   if ((DisView = (disview_struct *)malloc(sizeof(disview_struct))) == NULL)
      return DisView;

   if (CWindow_Init2(DisView, hInst, parent, title, IDD_DESASSEMBLEUR_VIEWER, DisView_Proc) != 0)
   {
      free(DisView);
      return NULL;
   }

   DisView->cpu = CPU;
   DisView->mode = 0;
   DisView->Refresh = (void (*)(void *))&DisView_Refresh;

   SetWindowLong(GetDlgItem(DisView->hwnd, IDC_DES_BOX), 0, (LONG)DisView);

   return DisView;
}

//////////////////////////////////////////////////////////////////////////////

void DisView_Deinit(disview_struct *DisView)
{
   if (DisView)
      free(DisView);
}

//////////////////////////////////////////////////////////////////////////////

void DisView_Refresh(disview_struct *DisView)
{
     if(((DisView->cpu->instruct_adr&0x0FFFFFFF)>>(2-DisView->cpu->CPSR.bits.T))>=8)
          if(((DisView->cpu->instruct_adr&0x0FFFFFFF)>>(2-DisView->cpu->CPSR.bits.T))<=0x3FFFFF7*(1+DisView->cpu->CPSR.bits.T))
               DisView->curr_ligne = ((DisView->cpu->instruct_adr&0x0FFFFFFF)>>(2-DisView->cpu->CPSR.bits.T))-8;
          else
               DisView->curr_ligne = 0x3FFFFEF*(1+DisView->cpu->CPSR.bits.T);
     else
          DisView->curr_ligne = 0;
     SetScrollRange(GetDlgItem(DisView->hwnd, IDC_DES_BOX), SB_VERT, 0, 0x3FFFFF7*(1+DisView->cpu->CPSR.bits.T), TRUE);
     SetScrollPos(GetDlgItem(DisView->hwnd, IDC_DES_BOX), SB_VERT, DisView->curr_ligne, TRUE);
     InvalidateRect(DisView->hwnd, NULL, FALSE);
}

//////////////////////////////////////////////////////////////////////////////
