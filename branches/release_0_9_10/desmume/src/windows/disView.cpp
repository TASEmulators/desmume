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

#include <algorithm>
#include <tchar.h>
#include <stdio.h>
#include "../MMU.h"
#include "../Disassembler.h"
#include "../NDSSystem.h"
#include "../armcpu.h"
#include "disView.h"
#include <commctrl.h>
#include "resource.h"
#include "main.h"

typedef struct
{
	BOOL autogo;
	BOOL autoup;
	u32	autoup_secs;
	u32 curr_ligne;
	armcpu_t *cpu;
	u16 mode;
} disview_struct;

disview_struct		*DisView7 = NULL;
disview_struct		*DisView9 = NULL;

static	HWND DisViewWnd[2] = {NULL, NULL};

#define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))

LRESULT DisViewBox_OnPaint(HWND hwnd, disview_struct *win, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        SIZE fontsize;
        TCHAR text[100];
        TCHAR txt[100];
        RECT rect;
        int lg;
        int ht;
        HDC mem_dc;
        HBITMAP mem_bmp;
        u32  nbligne;

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
        
        if((win->mode==1) || ((win->mode==0) && (win->cpu->CPSR.bits.T == 0)))
        {
             u32 i;
             u32 adr;

             if (win->autoup||win->autogo)
				 win->curr_ligne = (win->cpu->instruct_adr >> 2);
             adr = win->curr_ligne*4;

             for(i = 0; i < nbligne; ++i)
             {
                  u32 ins = _MMU_read32(win->cpu->proc_ID, MMU_AT_DEBUG, adr);
                  des_arm_instructions_set[INDEX(ins)](adr, ins, txt);
                  sprintf(text, "%04X:%04X  %08X  %s", (int)(adr>>16), (int)(adr&0xFFFF), (int)ins, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  rect.top+=fontsize.cy;
                  adr += 4;
             }
             
             
        
             if(((win->cpu->instruct_adr&0x0FFFFFFF) >= (win->curr_ligne<<2))&&((win->cpu->instruct_adr&0x0FFFFFFF) <= (win->curr_ligne+(nbligne<<2))))
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
        {    /* thumb display */
             u32 i;
             u32 adr;
             if (win->autoup)
                  win->curr_ligne = (win->cpu->instruct_adr >> 1) - (win->curr_ligne % nbligne) ;
             
             adr = win->curr_ligne*2;
        
             for(i = 0; i < nbligne; ++i)
             {
                  u32 ins = _MMU_read16(win->cpu->proc_ID, MMU_AT_DEBUG, adr);
                  des_thumb_instructions_set[ins>>6](adr, ins, txt);
                  sprintf(text, "%04X:%04X  %04X  %s", (int)(adr>>16), (int)(adr&0xFFFF), (int)ins, txt);
                  DrawText(mem_dc, text, -1, &rect, DT_TOP | DT_LEFT | DT_NOPREFIX);
                  rect.top+=fontsize.cy;
                  adr += 2;
             }
        
             if(((win->cpu->instruct_adr&0x0FFFFFFF) >= (win->curr_ligne<<1))&&((win->cpu->instruct_adr&0x0FFFFFFF) <= (win->curr_ligne+(nbligne<<1))))
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

// =================================================== ARM7
LRESULT CALLBACK ViewDisasm_ARM7BoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_NCCREATE:
			SetScrollRange(hwnd, SB_VERT, 0, 0x3FFFFF7, TRUE);
			SetScrollPos(hwnd, SB_VERT, 10, TRUE);
			return 1;
		    
		case WM_NCDESTROY:
			//free(win);
			return 1;

		case WM_PAINT:
			DisViewBox_OnPaint(hwnd, DisView7, wParam, lParam);
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
						   DisView7->curr_ligne = std::min<s32>(0x3FFFFF7*(1+DisView7->cpu->CPSR.bits.T), DisView7->curr_ligne+1);
						   break;
					  case SB_LINEUP :
						   DisView7->curr_ligne = (u32)std::max<s32>(0, (s32)DisView7->curr_ligne-1);
						   break;
					  case SB_PAGEDOWN :
						   DisView7->curr_ligne = std::min<s32>(0x3FFFFF7*(1+DisView7->cpu->CPSR.bits.T), DisView7->curr_ligne+nbligne);
						   break;
					  case SB_PAGEUP :
						   DisView7->curr_ligne = (u32)std::max<s32>(0, DisView7->curr_ligne-nbligne);
						   break;
				  }
		          
				  SelectObject(dc, old);
				  SetScrollPos(hwnd, SB_VERT, DisView7->curr_ligne, TRUE);
				  InvalidateRect(hwnd, NULL, FALSE);
			}
			return 1;
		                            
		case WM_ERASEBKGND:
			return 1;
	}
	return FALSE;
}

BOOL CALLBACK ViewDisasm_ARM7Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//bail out early if the dialog isnt initialized
	if(!DisView7 && message != WM_INITDIALOG)
		return false;

	switch (message)
	{
			case WM_INITDIALOG :
				{
					SetWindowText(hwnd, "ARM7 Disassembler");
					SetDlgItemInt(hwnd, IDC_SETPNUM, 1, FALSE);
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_DES), BM_SETCHECK, TRUE, 0);
					DisView7 = new disview_struct;
					memset(DisView7, 0, sizeof(disview_struct));
					DisView7->cpu = &NDS_ARM7;
					DisView7->autoup_secs = 1;
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, DisView7->autoup_secs);
					DisViewWnd[1] = NULL;
					return 1;
				}
			case WM_CLOSE :
				{
					EndDialog(hwnd,0);
					if(DisView7->autoup)
					{
						KillTimer(hwnd, IDT_VIEW_DISASM7);
						DisView7->autoup = false;
					}
					delete DisView7;
					DisView7 = NULL;
					DisViewWnd[1] = NULL;
					//INFO("Close ARM7 disassembler\n");
					return 1;
				}
			case WM_PAINT:
					DisViewDialog_OnPaint(hwnd, DisView7, wParam, lParam);
					return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 0;
                        case IDC_AUTO_DES :
                             /* address to line correction */
                             if ((DisView7->cpu->CPSR.bits.T) && (DisView7->mode == 1)) {
                                  /* from arm to thumb, line * 2*/
                                   DisView7->curr_ligne <<= 1 ;
                             } else if (!(DisView7->cpu->CPSR.bits.T) && (DisView7->mode == 2)) {
                                  /* from thumb to arm, line / 2 */
                                   DisView7->curr_ligne >>= 1 ;
                             }
                             DisView7->mode = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             return 1;
                        case IDC_ARM :
                             /* address to line correction */
                             if ((DisView7->mode==2) || ((DisView7->mode==0) && (DisView7->cpu->CPSR.bits.T))) {
                                   DisView7->curr_ligne >>= 1 ;
                             } ;
                             DisView7->mode = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             return 1;
                        case IDC_THUMB :
                             /* address to line correction */
                             if ((DisView7->mode==1) || ((DisView7->mode==0) && !(DisView7->cpu->CPSR.bits.T))) {
                                   DisView7->curr_ligne <<= 1 ;
                             } ;
                             DisView7->mode = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             return 1;
                        case IDC_AUTO_UPDATE :
                             if(DisView7->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTOUPDATE_ASM), TRUE);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_DISASM7);
                                  DisView7->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTOUPDATE_ASM), FALSE);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             DisView7->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_DISASM7, DisView7->autoup_secs*20, (TIMERPROC) NULL);
                             return 1;
						case IDC_STEP:
                             {
								NDS_debug_step();
                             }
                             return 1;

						case IDC_CONTINUE:
                             {
								NDS_debug_continue();
                             }
                             return 1;
                        case IDC_GO :
                             {
                             u16 i;
                             char tmp[16];
                             int lg = GetDlgItemText(hwnd, IDC_GOTODES, tmp, 16);
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
                             /* address to line correction */
                             switch (DisView7->mode) {
                                    case 0:    /* auto */
                                         DisView7->curr_ligne = adr>>2;
                                         if (DisView7->cpu->CPSR.bits.T) {
                                             DisView7->curr_ligne = adr>>1;
                                         }
                                         break ;
                                    case 1:    /* arm */
                                         DisView7->curr_ligne = adr>>2;
                                         break ;
                                    case 2:    /* thumb */
                                         DisView7->curr_ligne = adr>>1;
                                         break ;
                             } ;
                             InvalidateRect(hwnd, NULL, FALSE);
                             }
                             return 1;
						case IDC_REFRESH:
							DisView7->autogo=true;
							InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
							DisView7->autogo=false;
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								u16 t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (!DisView7) 
								{
									SendMessage(hwnd, WM_INITDIALOG, 0, 0);
								}
								if (t != DisView7->autoup_secs)
								{
									DisView7->autoup_secs = t;
									if (DisView7->autoup)
										SetTimer(hwnd, IDT_VIEW_DISASM7, 
												DisView7->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
							return 1;
						case IDC_AUTOUPDATE_ASM:
							{
								if (DisViewWnd[1] == NULL)
								{
									EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE), FALSE);
									DisViewWnd[1] = GetDlgItem(hwnd, IDC_DES_BOX);
									return 1;
								}
								DisViewWnd[1] = NULL;
								EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE), TRUE);
							}
							return 1;
                        return 1;
                 }
                 return 0;
     }

	return FALSE;
}

// =================================================== ARM9
LRESULT CALLBACK ViewDisasm_ARM9BoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch(msg)
	{
		case WM_NCCREATE:
			SetScrollRange(hwnd, SB_VERT, 0, 0x3FFFFF7, TRUE);
			SetScrollPos(hwnd, SB_VERT, 10, TRUE);
			return 1;
		    
		case WM_NCDESTROY:
			//free(win);
			return 1;

		case WM_PAINT:
			DisViewBox_OnPaint(hwnd, DisView9, wParam, lParam);
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
						   DisView9->curr_ligne = std::min<s32>(0x3FFFFF7*(1+DisView9->cpu->CPSR.bits.T), DisView9->curr_ligne+1);
						   break;
					  case SB_LINEUP :
						   DisView9->curr_ligne = (u32)std::max<s32>(0, (s32)DisView9->curr_ligne-1);
						   break;
					  case SB_PAGEDOWN :
						   DisView9->curr_ligne = std::min<s32>(0x3FFFFF7*(1+DisView9->cpu->CPSR.bits.T), DisView9->curr_ligne+nbligne);
						   break;
					  case SB_PAGEUP :
						   DisView9->curr_ligne = (u32)std::max<s32>(0, DisView9->curr_ligne-nbligne);
						   break;
				  }
		          
				  SelectObject(dc, old);
				  SetScrollPos(hwnd, SB_VERT, DisView9->curr_ligne, TRUE);
				  InvalidateRect(hwnd, NULL, FALSE);
			}
			return 1;
		                            
		case WM_ERASEBKGND:
			return 1;
	}
	return FALSE;
}

BOOL CALLBACK ViewDisasm_ARM9Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	//bail out early if the dialog isnt initialized
	if(!DisView9 && message != WM_INITDIALOG)
		return false;

	switch (message)
     {
            case WM_INITDIALOG :
				{
					SetWindowText(hwnd, "ARM9 Disassembler");
					SetDlgItemInt(hwnd, IDC_SETPNUM, 1, FALSE);
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_DES), BM_SETCHECK, TRUE, 0);
					DisView9 = new disview_struct;
					memset(DisView9, 0, sizeof(disview_struct));
					DisView9->cpu = &NDS_ARM9;
					DisView9->autoup_secs = 1;
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
					SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, DisView9->autoup_secs);
					DisViewWnd[0] = NULL;
					return 1;
				}
						case WM_CLOSE :
				{
					EndDialog(hwnd,0);
					if(DisView9->autoup)
					{
						KillTimer(hwnd, IDT_VIEW_DISASM9);
						DisView9->autoup = false;
					}
					delete DisView9;
					DisView9 = NULL;
					DisViewWnd[0] = NULL;
					//INFO("Close ARM9 disassembler\n");
					return 1;
				}
            case WM_PAINT:
					DisViewDialog_OnPaint(hwnd, DisView9, wParam, lParam);
					return 1;
			case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             SendMessage(hwnd, WM_CLOSE, 0, 0);
                             return 0;
                        case IDC_AUTO_DES :
                             /* address to line correction */
                             if ((DisView9->cpu->CPSR.bits.T) && (DisView9->mode == 1)) {
                                  /* from arm to thumb, line * 2*/
                                   DisView9->curr_ligne <<= 1 ;
                             } else if (!(DisView9->cpu->CPSR.bits.T) && (DisView9->mode == 2)) {
                                  /* from thumb to arm, line / 2 */
                                   DisView9->curr_ligne >>= 2 ;
                             }
                             DisView9->mode = 0;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             return 1;
                        case IDC_ARM :
                             /* address to line correction */
                             if ((DisView9->mode==2) || ((DisView9->mode==0) && (DisView9->cpu->CPSR.bits.T))) {
                                   DisView9->curr_ligne >>= 2 ;
                             } ;
                             DisView9->mode = 1;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             return 1;
                        case IDC_THUMB :
                             /* address to line correction */
                             if ((DisView9->mode==1) || ((DisView9->mode==0) && !(DisView9->cpu->CPSR.bits.T))) {
                                   DisView9->curr_ligne <<= 1 ;
                             } ;
                             DisView9->mode = 2;
                             InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
                             return 1;
                        case IDC_AUTO_UPDATE :
                             if(DisView9->autoup)
                             {
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTOUPDATE_ASM), TRUE);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
								 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
								 KillTimer(hwnd, IDT_VIEW_DISASM9);
                                  DisView9->autoup = FALSE;
                                  return 1;
                             }
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTOUPDATE_ASM), FALSE);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                             DisView9->autoup = TRUE;
							 SetTimer(hwnd, IDT_VIEW_DISASM9, DisView9->autoup_secs*20, (TIMERPROC) NULL);
                             return 1;
						case IDC_STEP:
                             {
								NDS_debug_step();
                             }
                             return 1;
						case IDC_CONTINUE:
                             {
								NDS_debug_continue();
                             }
                             return 1;
                        case IDC_GO :
                             {
                             u16 i;
                             char tmp[16];
                             int lg = GetDlgItemText(hwnd, IDC_GOTODES, tmp, 16);
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
                             /* address to line correction */
                             switch (DisView9->mode) {
                                    case 0:    /* auto */
                                         DisView9->curr_ligne = adr>>2;
                                         if (DisView9->cpu->CPSR.bits.T) {
                                             DisView9->curr_ligne = adr>>1;
                                         }
                                         break ;
                                    case 1:    /* arm */
                                         DisView9->curr_ligne = adr>>2;
                                         break ;
                                    case 2:    /* thumb */
                                         DisView9->curr_ligne = adr>>1;
                                         break ;
                             } ;
                             InvalidateRect(hwnd, NULL, FALSE);
                             }
                             return 1;
						case IDC_REFRESH:
							DisView9->autogo=true;
							InvalidateRect(GetDlgItem(hwnd, IDC_DES_BOX), NULL, FALSE);
							DisView9->autogo=false;
							InvalidateRect(hwnd, NULL, FALSE);
							return 1;
						case IDC_AUTO_UPDATE_SECS:
							{
								u16 t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
								if (!DisView9) 
								{
									SendMessage(hwnd, WM_INITDIALOG, 0, 0);
								}
								if (t != DisView9->autoup_secs)
								{
									DisView9->autoup_secs = t;
									if (DisView9->autoup)
										SetTimer(hwnd, IDT_VIEW_DISASM9, 
												DisView9->autoup_secs*20, (TIMERPROC) NULL);
								}
							}
							return 1;
						case IDC_AUTOUPDATE_ASM:
							{
								if (DisViewWnd[0] == NULL)
								{
									EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE), FALSE);
									DisViewWnd[0] = GetDlgItem(hwnd, IDC_DES_BOX);
									return 1;
								}
								DisViewWnd[0] = NULL;
								EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE), TRUE);
							}
							return 1;
						return 1;
                 }
                 return 0;
     }

	return FALSE;
}

template<int proc>
FORCEINLINE void DisassemblerTools_Refresh()
{
	if (DisViewWnd[proc] == NULL) return;
	if (proc == 0)
	{
		DisView9->autogo=true;
		InvalidateRect(DisViewWnd[proc], NULL, FALSE);
		DisView9->autogo=false;
	}
	else
	{
		DisView7->autogo=true;
		InvalidateRect(DisViewWnd[proc], NULL, FALSE);
		DisView7->autogo=false;
	}
}

//these templates needed to be instantiated manually
template void DisassemblerTools_Refresh<0>();
template void DisassemblerTools_Refresh<1>();
