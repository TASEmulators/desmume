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

#include <algorithm>
#include "memView.h"
#include <commctrl.h>
#include "../MMU.h"
#include "debug.h"
#include "resource.h"

typedef struct
{
	u32	autoup_secs;
	bool autoup;

	s8 cpu;
	u32 curr_ligne;
	u8 representation;
} memview_struct;


memview_struct		*MemView7 = NULL;
memview_struct		*MemView9 = NULL;

LRESULT MemViewBox_OnPaint(HWND hwnd, memview_struct * win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        SIZE fontsize;
        TCHAR text[80];
        int i;
        RECT rect;
        int lg;
        int ht;
        HDC mem_dc;
        HBITMAP mem_bmp;
        int nbligne;
        RECT r;
        u32 adr;

        GetClientRect(hwnd, &rect);
        lg = rect.right - rect.left;
        ht = rect.bottom - rect.top;
        
        hdc = BeginPaint(hwnd, &ps);
        
        mem_dc = CreateCompatibleDC(hdc);
        mem_bmp = CreateCompatibleBitmap(hdc, lg, ht);
        SelectObject(mem_dc, mem_bmp);
        
        FillRect(mem_dc, &rect, (HBRUSH)GetStockObject(WHITE_BRUSH));
        
        SelectObject(mem_dc, GetStockObject(SYSTEM_FIXED_FONT));
        
        GetTextExtentPoint32(mem_dc, "0", 1, &fontsize);
        
        nbligne = ht/fontsize.cy;
        
        SetTextColor(mem_dc, RGB(0,0,0));
                
        r.top = 3;
        r.left = 3;
        r.bottom = r.top+fontsize.cy;
        r.right = rect.right-3;
        
        adr = win->curr_ligne*0x10;
		//printlog("curr_ligne=%i\n", win->curr_ligne);
        
        for(i=0; i<nbligne; ++i)
        {
                int j;
                sprintf(text, "%04X:%04X", (int)(adr>>16), (int)(adr&0xFFFF));
                DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                r.left += 11*fontsize.cx;

                if(win->representation == 0)
                     for(j=0; j<16; ++j)
                     {
                          sprintf(text, "%02X", MMU_read8(win->cpu, adr+j));
                          DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                          r.left+=3*fontsize.cx;
                     }
                else
                     if(win->representation == 1)
                          for(j=0; j<16; j+=2)
                          {
                               sprintf(text, "%04X", MMU_read16(win->cpu, adr+j));
                               DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                               r.left+=5*fontsize.cx;
                          }
                     else
                          for(j=0; j<16; j+=4)
                          {
                               sprintf(text, "%08X", (int)MMU_read32(win->cpu, adr+j));
                               DrawText(mem_dc, text, -1, &r, DT_TOP | DT_LEFT | DT_NOPREFIX);
                               r.left+=9*fontsize.cx;
                          }                     
                
                r.left+=fontsize.cx;
                 
                for(j=0; j<16; ++j)
                {
                        u8 c = MMU_read8(win->cpu, adr+j);
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

//=================================================== ARM7
LRESULT CALLBACK ViewMem_ARM7BoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
    {
               case WM_NCCREATE:
                    SetScrollRange(hwnd, SB_VERT, 0, 0x0FFFFFFF, TRUE);
                    SetScrollPos(hwnd, SB_VERT, 10, TRUE);
                    return 1;
                    
               case WM_NCDESTROY:
                    return 1;
               
               case WM_PAINT:
                    MemViewBox_OnPaint(hwnd, MemView7, wParam, lParam);
                    return 1;
               
               case WM_ERASEBKGND:
	                return 1;
               case WM_VSCROLL :
                    {
                         RECT rect;
                         SIZE fontsize;
                         HDC dc;
                         HFONT old;
                         int nbligne;

						 GetClientRect(hwnd, &rect);
                         dc = GetDC(hwnd);
                         old = (HFONT)SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
                         GetTextExtentPoint32(dc, "0", 1, &fontsize);
    
                         nbligne = (rect.bottom - rect.top)/fontsize.cy;

                         switch LOWORD(wParam)
                         {
                              case SB_LINEDOWN :
                                   MemView7->curr_ligne = std::min((u32)0x0FFFFFFFF, (u32)MemView7->curr_ligne+1);
                                   break;
                              case SB_LINEUP :
                                   MemView7->curr_ligne = (u32)std::max((u32)0l, (u32)MemView7->curr_ligne-1);
                                   break;
                              case SB_PAGEDOWN :
                                   MemView7->curr_ligne = std::min((u32)0x0FFFFFFFF, (u32)MemView7->curr_ligne+nbligne);
                                   break;
                              case SB_PAGEUP :
                                   MemView7->curr_ligne = (u32)std::max((u32)0l, (u32)MemView7->curr_ligne-nbligne);
                                   break;
                          }
                          
                          SelectObject(dc, old);
                          SetScrollPos(hwnd, SB_VERT, MemView7->curr_ligne, TRUE);
                          InvalidateRect(hwnd, NULL, FALSE);
                          UpdateWindow(hwnd);
                    }
                    return 1;
    }

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool CALLBACK ViewMem_ARM7Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	switch (message)
	{
		case WM_INITDIALOG :
			{
				SetWindowText(hwnd, "ARM7 memory viewer");
				SendMessage(GetDlgItem(hwnd, IDC_8_BIT), BM_SETCHECK, TRUE, 0);
				MemView7 = new memview_struct;
				memset(MemView7, 0, sizeof(memview_struct));
				MemView7->cpu = 1;
				MemView7->autoup_secs = 5;
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, MemView7->autoup_secs);
				return 0;
			}
		case WM_CLOSE:
			{
				if(MemView7->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_MEM7);
					MemView7->autoup = false;
				}

				if (MemView7!=NULL) 
				{
					delete MemView7;
					MemView7 = NULL;
				}
				//printlog("Close ARM7 memory dialog\n");
				PostQuitMessage(0);
				return 0;
			}
		case WM_TIMER:
			SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
			return 1;

		case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_8_BIT :
                             MemView7->representation = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX));
                             return 1;
                        case IDC_16_BIT :
                             MemView7->representation = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
                        case IDC_32_BIT :
                             MemView7->representation = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
						case IDC_AUTO_UPDATE :
							 if(MemView7->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_MEM7);
                                  MemView7->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             MemView7->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_MEM7, MemView7->autoup_secs*1000, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (t != MemView7->autoup_secs)
								{
									MemView7->autoup_secs = t;
									if (MemView7->autoup)
										SetTimer(hwnd, IDT_VIEW_MEM7, 
												MemView7->autoup_secs*1000, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
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
                             MemView7->curr_ligne = (adr>>4);
                             InvalidateRect(hwnd, NULL, FALSE);
                             UpdateWindow(hwnd);
                             }
                             return 1;
                        case IDC_FERMER :
                             SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 1;
                 }
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

//=================================================== ARM9
LRESULT CALLBACK ViewMem_ARM9BoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
    {
               case WM_NCCREATE:
                    SetScrollRange(hwnd, SB_VERT, 0, 0x0FFFFFFF, TRUE);
                    SetScrollPos(hwnd, SB_VERT, 10, TRUE);
                    return 1;
                    
               case WM_NCDESTROY:
                    return 1;
               
               case WM_PAINT:
                    MemViewBox_OnPaint(hwnd, MemView9, wParam, lParam);
                    return 1;
               
               case WM_ERASEBKGND:
	                return 1;
               case WM_VSCROLL :
                    {
                         RECT rect;
                         SIZE fontsize;
                         HDC dc;
                         HFONT old;
                         int nbligne;

						 GetClientRect(hwnd, &rect);
                         dc = GetDC(hwnd);
                         old = (HFONT)SelectObject(dc, GetStockObject(SYSTEM_FIXED_FONT));
                         GetTextExtentPoint32(dc, "0", 1, &fontsize);
    
                         nbligne = (rect.bottom - rect.top)/fontsize.cy;

                         switch LOWORD(wParam)
                         {
                              case SB_LINEDOWN :
                                   MemView9->curr_ligne = std::min((u32)0x0FFFFFFFF, (u32)MemView9->curr_ligne+1);
                                   break;
                              case SB_LINEUP :
                                   MemView9->curr_ligne = (u32)std::max((u32)0l, (u32)MemView9->curr_ligne-1);
                                   break;
                              case SB_PAGEDOWN :
                                   MemView9->curr_ligne = std::min((u32)0x0FFFFFFFF, (u32)MemView9->curr_ligne+nbligne);
                                   break;
                              case SB_PAGEUP :
                                   MemView9->curr_ligne = (u32)std::max((u32)0l, (u32)MemView9->curr_ligne-nbligne);
                                   break;
                          }
                          
                          SelectObject(dc, old);
                          SetScrollPos(hwnd, SB_VERT, MemView9->curr_ligne, TRUE);
                          InvalidateRect(hwnd, NULL, FALSE);
                          UpdateWindow(hwnd);
                    }
                    return 1;
    }

	return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool CALLBACK ViewMem_ARM9Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	
	switch (message)
	{
		case WM_INITDIALOG :
			{
				SetWindowText(hwnd, "ARM9 memory viewer");
				SendMessage(GetDlgItem(hwnd, IDC_8_BIT), BM_SETCHECK, TRUE, 0);
				MemView9 = new memview_struct;
				memset(MemView9, 0, sizeof(memview_struct));
				MemView9->cpu = 0;
				MemView9->autoup_secs = 5;
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, MemView9->autoup_secs);
				return 0;
			}
		case WM_CLOSE:
			{
				if(MemView9->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_MEM9);
					MemView9->autoup = false;
				}

				if (MemView9!=NULL) 
				{
					delete MemView9;
					MemView9 = NULL;
				}
				//printlog("Close ARM9 memory dialog\n");
				PostQuitMessage(0);
				return 0;
			}
		case WM_TIMER:
			SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
			return 1;

		case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_8_BIT :
                             MemView9->representation = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX));
                             return 1;
                        case IDC_16_BIT :
                             MemView9->representation = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
                        case IDC_32_BIT :
                             MemView9->representation = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_MEM_BOX), NULL, FALSE);
                             UpdateWindow(GetDlgItem(hwnd, IDC_MEM_BOX)); 
                             return 1;
						case IDC_AUTO_UPDATE :
							 if(MemView9->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_MEM9);
                                  MemView9->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             MemView9->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_MEM9, MemView9->autoup_secs*1000, (TIMERPROC) NULL);
							 return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (t != MemView9->autoup_secs)
								{
									MemView9->autoup_secs = t;
									if (MemView9->autoup)
										SetTimer(hwnd, IDT_VIEW_MEM9, 
												MemView9->autoup_secs*1000, (TIMERPROC) NULL);
								}
							}
                             return 1;
						case IDC_REFRESH:
							InvalidateRect(hwnd, NULL, FALSE);
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
                             MemView9->curr_ligne = (adr>>4);
                             InvalidateRect(hwnd, NULL, FALSE);
                             UpdateWindow(hwnd);
                             }
                             return 1;
                        case IDC_FERMER :
                             SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 1;
                 }
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}
