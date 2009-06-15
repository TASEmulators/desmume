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


#include "../MMU.h"
#include "debug.h"
#include "resource.h"
#include "common.h"
#include <algorithm>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include "memView.h"

using namespace std;

//////////////////////////////////////////////////////////////////////////////

typedef struct MemView_DataStruct
{
	MemView_DataStruct(u8 CPU) : cpu(CPU), address(0x02000000), viewMode(0), sel(FALSE), selPart(0), selAddress(0x00000000), selNewVal(0x000000000)
	{
	}

	HWND hDlg;

	u8 cpu;
	u32 address;
	u8 viewMode;

	BOOL sel;
	u8 selPart;
	u32 selAddress;
	u32 selNewVal;
} MemView_DataStruct;

MemView_DataStruct * MemView_Data[2] = {NULL, NULL};

//////////////////////////////////////////////////////////////////////////////

BOOL MemView_Init()
{
	WNDCLASSEX wc;

	wc.cbSize         = sizeof(wc);
	wc.lpszClassName  = "MemView_ViewBox";
	wc.hInstance      = hAppInst;
	wc.lpfnWndProc    = MemView_ViewBoxProc;
	wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon          = 0;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = 0;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = sizeof(MemView_DataStruct);
	wc.hIconSm        = 0;

	RegisterClassEx(&wc);

	return 1;
}

void MemView_DeInit()
{
	UnregisterClass("MemView_ViewBox", hAppInst);
}

//////////////////////////////////////////////////////////////////////////////

BOOL MemView_DlgOpen(HWND hParentWnd, char *Title, u8 CPU)
{
	HWND hDlg;

	MemView_Data[CPU] = new MemView_DataStruct(CPU);
	if(MemView_Data[CPU] == NULL)
		return 0;

	hDlg = CreateDialogParam(hAppInst, MAKEINTRESOURCE(IDD_MEM_VIEW), hParentWnd, MemView_DlgProc, (LPARAM)MemView_Data[CPU]);
	if(hDlg == NULL)
	{
		delete MemView_Data[CPU];
		MemView_Data[CPU] = NULL;
		return 0;
	}

	MemView_Data[CPU]->hDlg = hDlg;

	SetWindowText(hDlg, Title);

	ShowWindow(hDlg, SW_SHOW);
	UpdateWindow(hDlg);

	return 1;
}

void MemView_DlgClose(u8 CPU)
{
	if(MemView_Data[CPU] != NULL)
	{
		DestroyWindow(MemView_Data[CPU]->hDlg);
		delete MemView_Data[CPU];
		MemView_Data[CPU] = NULL;
	}
}

BOOL MemView_IsOpened(u8 CPU)
{
	return (MemView_Data[CPU] != NULL);
}

void MemView_Refresh(u8 CPU)
{
	InvalidateRect(MemView_Data[CPU]->hDlg, NULL, FALSE);
	UpdateWindow(MemView_Data[CPU]->hDlg);
}

//////////////////////////////////////////////////////////////////////////////

LRESULT MemView_DlgPaint(HWND hDlg, MemView_DataStruct *data, WPARAM wParam, LPARAM lParam)
{
	HDC				hdc;
	PAINTSTRUCT		ps;

	hdc = BeginPaint(hDlg, &ps);

	EndPaint(hDlg, &ps);

	return 0;
}

BOOL CALLBACK MemView_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MemView_DataStruct *data = (MemView_DataStruct*)GetWindowLong(hDlg, DWL_USER);
	if((data == NULL) && (uMsg != WM_INITDIALOG))
		return 0;

	switch(uMsg)
	{
	case WM_INITDIALOG:
		{
			if(data == NULL)
			{
				data = (MemView_DataStruct*)lParam;
				SetWindowLong(hDlg, DWL_USER, (LONG)data);
			}

			CheckRadioButton(hDlg, IDC_8_BIT, IDC_32_BIT, IDC_8_BIT);

			SendMessage(GetDlgItem(hDlg, IDC_ADDRESS), EM_SETLIMITTEXT, 8, 0);
			SetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), "02000000");

			SetWindowLong(GetDlgItem(hDlg, IDC_MEMVIEWBOX), DWL_USER, (LONG)data);

			InvalidateRect(hDlg, NULL, FALSE); UpdateWindow(hDlg);
		}
		return 1;

	case WM_CLOSE:
	case WM_DESTROY:
		MemView_DlgClose(data->cpu);
		return 1;

	case WM_PAINT:
		MemView_DlgPaint(hDlg, data, wParam, lParam);
		return 1;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			MemView_DlgClose(data->cpu);
			return 1;

		case IDC_8_BIT:
		case IDC_16_BIT:
		case IDC_32_BIT:
			data->sel = FALSE;
			data->selAddress = 0x00000000;
			data->selPart = 0;
			data->selNewVal = 0x00000000;
			CheckRadioButton(hDlg, IDC_8_BIT, IDC_32_BIT, LOWORD(wParam));
			data->viewMode = (LOWORD(wParam) - IDC_8_BIT);
			InvalidateRect(hDlg, NULL, FALSE); UpdateWindow(hDlg);
			return 1;

		case IDC_GO:
			{
				char addrstr[9];
				int len;
				int i;
				int shift;
				BOOL error = FALSE;
				u32 address = 0x00000000;

				len = GetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), addrstr, 9);

				for(i = 0; i < len; i++)
				{
					char ch = addrstr[i];

					if((ch >= '0') && (ch <= '9'))
						continue;

					if((ch >= 'A') && (ch <= 'F'))
						continue;

					if((ch >= 'a') && (ch <= 'f'))
						continue;

					if(ch == '\0')
						break;

					error = TRUE;
					break;
				}

				if(error)
				{
					MessageBox(hDlg, "Error:\nInvalid address specified.\nThe address must be an hexadecimal value.", "DeSmuME", (MB_OK | MB_ICONERROR));
					SetWindowText(GetDlgItem(hDlg, IDC_ADDRESS), "");
					return 1;
				}

				for(i = (len-1), shift = 0; i >= 0; i--, shift += 4)
				{
					char ch = addrstr[i];

					if((ch >= '0') && (ch <= '9'))
						address |= ((ch - '0') << shift);
					else if((ch >= 'A') && (ch <= 'F'))
						address |= ((ch - 'A' + 0xA) << shift);
					else if((ch >= 'a') && (ch <= 'f'))
						address |= ((ch - 'a' + 0xA) << shift);
				}

				data->sel = FALSE;
				data->selAddress = 0x00000000;
				data->selPart = 0;
				data->selNewVal = 0x00000000;

				data->address = min((u32)0xFFFFFF00, (address & 0xFFFFFFF0));
				SetScrollPos(GetDlgItem(hDlg, IDC_MEMVIEWBOX), SB_VERT, ((data->address >> 4) & 0x000FFFFF), TRUE);
				InvalidateRect(hDlg, NULL, FALSE); UpdateWindow(hDlg);
			}
			return 1;

		case IDC_TEXTDUMP:
			{
				char fileName[256] = "";
				OPENFILENAME ofn;

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Text file (*.txt)\0*.txt\0Any file (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = 256;
				ofn.lpstrDefExt = "txt";
				ofn.Flags = OFN_NOCHANGEDIR;

				if(GetSaveFileName(&ofn))
				{
					FILE *f;
					u8 memory[0x100];
					int line;

					MMU_DumpMemBlock(data->cpu, data->address, 0x100, memory);

					f = fopen(fileName, "a");

					for(line = 0; line < 16; line++)
					{
						int i;
						
						fprintf(f, "%08X\t\t", (data->address + (line << 4)));

						switch(data->viewMode)
						{
						case 0:
							{
								for(i = 0; i < 16; i++)
								{
									fprintf(f, "%02X ", T1ReadByte(memory, ((line << 4) + i)));
								}
								fprintf(f, "\t");
							}
							break;

						case 1:
							{
								for(i = 0; i < 16; i += 2)
								{
									fprintf(f, "%04X ", T1ReadWord(memory, ((line << 4) + i)));
								}
								fprintf(f, "\t\t");
							}
							break;

						case 2:
							{
								for(i = 0; i < 16; i += 4)
								{
									fprintf(f, "%08X ", T1ReadLong(memory, ((line << 4) + i)));
								}
								fprintf(f, "\t\t\t");
							}
							break;
						}

						for(i = 0; i < 16; i++)
						{
							u8 val = T1ReadByte(memory, ((line << 4) + i));
									
							if((val >= 32) && (val <= 127))
								fprintf(f, "%c", (char)val);
							else
								fprintf(f, ".");
						}
						fprintf(f, "\n");
					}

					fclose(f);
				}
			}
			return 1;

		case IDC_RAWDUMP:
			{
				char fileName[256] = "";
				OPENFILENAME ofn;

				ZeroMemory(&ofn, sizeof(ofn));
				ofn.lStructSize = sizeof(ofn);
				ofn.hwndOwner = hDlg;
				ofn.lpstrFilter = "Binary file (*.bin)\0*.bin\0Any file (*.*)\0*.*\0\0";
				ofn.nFilterIndex = 1;
				ofn.lpstrFile = fileName;
				ofn.nMaxFile = 256;
				ofn.lpstrDefExt = "bin";
				ofn.Flags = OFN_NOCHANGEDIR;

				if(GetSaveFileName(&ofn))
				{
					FILE *f;
					u8 memory[0x100];
					int line;

					MMU_DumpMemBlock(data->cpu, data->address, 0x100, memory);

					f = fopen(fileName, "ab");

					fwrite(memory, 0x100, 1, f);

					fclose(f);
				}
			}
			return 1;
		}
		return 0;
	}

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT MemView_ViewBoxPaint(HWND hCtl, MemView_DataStruct *data, WPARAM wParam, LPARAM lParam)
{
	HDC				hdc;
	PAINTSTRUCT		ps;
	RECT			rc;
	int				w, h;
	SIZE			fontsize;
	int				fontwidth, fontheight;
	HDC				mem_hdc;
	HBITMAP			mem_bmp;

	GetClientRect(hCtl, &rc);
	w = (rc.right - rc.left);
	h = (rc.bottom - rc.top);

	hdc = BeginPaint(hCtl, &ps);

	mem_hdc = CreateCompatibleDC(hdc);
	mem_bmp = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(mem_hdc, mem_bmp);

	SelectObject(mem_hdc, GetStockObject(SYSTEM_FIXED_FONT));

	SetBkMode(mem_hdc, OPAQUE);
	SetBkColor(mem_hdc, RGB(255, 255, 255));
	SetTextColor(mem_hdc, RGB(0, 0, 0));

	GetTextExtentPoint32(mem_hdc, " ", 1, &fontsize);
	fontwidth = fontsize.cx;
	fontheight = fontsize.cy;

	FillRect(mem_hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	if(data != NULL)
	{
		u32 addr = data->address;
		u8 memory[0x100];
		char text[80];
		int startx;
		int curx, cury;
		int line;

		startx = 0;
		curx = 0;
		cury = 0;

		startx = ((fontwidth * 8) + 5);
		cury = (fontheight + 3);

		MoveToEx(mem_hdc, ((fontwidth * 8) + 2), 0, NULL);
		LineTo(mem_hdc, ((fontwidth * 8) + 2), h);

		MoveToEx(mem_hdc, 0, (fontheight + 1), NULL);
		LineTo(mem_hdc, w, (fontheight + 1));

		switch(data->viewMode)
		{
		case 0:
			{
				sprintf(text, "   0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F   0123456789ABCDEF");
				TextOut(mem_hdc, startx, 0, text, strlen(text));
			}
			break;

		case 1:
			{
				sprintf(text, "       0 1  2 3  4 5  6 7  8 9  A B  C D  E F       0123456789ABCDEF");
				TextOut(mem_hdc, startx, 0, text, strlen(text));
			}
			break;

		case 2:
			{
				sprintf(text, "         0 1 2 3  4 5 6 7  8 9 A B  C D E F         0123456789ABCDEF");
				TextOut(mem_hdc, startx, 0, text, strlen(text));
			}
			break;
		}
		
		MMU_DumpMemBlock(data->cpu, data->address, 0x100, memory);

		for(line = 0; line < 16; line++, addr += 0x10)
		{
			int i;

			sprintf(text, "%08X", addr);
			TextOut(mem_hdc, 0, cury, text, strlen(text));

			curx = startx;

			switch(data->viewMode)
			{
			case 0:
				{
					curx += (fontwidth * 2);
					for(i = 0; i < 16; i++)
					{
						u8 val = T1ReadByte(memory, ((line << 4) + i));
						if(data->sel && (data->selAddress == (addr + i)))
						{
							SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
							SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							
							switch(data->selPart)
							{
							case 0: sprintf(text, "%02X", val); break;
							case 1: sprintf(text, "%01X.", data->selNewVal); break;
							}
						}
						else
						{
							SetBkColor(mem_hdc, RGB(255, 255, 255));
							SetTextColor(mem_hdc, RGB(0, 0, 0));
							
							sprintf(text, "%02X", val);
						}

						TextOut(mem_hdc, curx, cury, text, strlen(text));
						curx += (fontwidth * (2+1));
					}
					curx += (fontwidth * 2);
				}
				break;

			case 1:
				{
					curx += (fontwidth * 6);
					for(i = 0; i < 16; i += 2)
					{
						u16 val = T1ReadWord(memory, ((line << 4) + i));
						if(data->sel && (data->selAddress == (addr + i)))
						{
							SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
							SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							
							switch(data->selPart)
							{
							case 0: sprintf(text, "%04X", val); break;
							case 1: sprintf(text, "%01X...", data->selNewVal); break;
							case 2: sprintf(text, "%02X..", data->selNewVal); break;
							case 3: sprintf(text, "%03X.", data->selNewVal); break;
							}
						}
						else
						{
							SetBkColor(mem_hdc, RGB(255, 255, 255));
							SetTextColor(mem_hdc, RGB(0, 0, 0));
							
							sprintf(text, "%04X", val);
						}

						TextOut(mem_hdc, curx, cury, text, strlen(text));
						curx += (fontwidth * (4+1));
					}
					curx += (fontwidth * 6);
				}
				break;

			case 2:
				{
					curx += (fontwidth * 8);
					for(i = 0; i < 16; i += 4)
					{
						u32 val = T1ReadLong(memory, ((line << 4) + i));
						if(data->sel && (data->selAddress == (addr + i)))
						{
							SetBkColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHT));
							SetTextColor(mem_hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							
							switch(data->selPart)
							{
							case 0: sprintf(text, "%08X", val); break;
							case 1: sprintf(text, "%01X.......", data->selNewVal); break;
							case 2: sprintf(text, "%02X......", data->selNewVal); break;
							case 3: sprintf(text, "%03X.....", data->selNewVal); break;
							case 4: sprintf(text, "%04X....", data->selNewVal); break;
							case 5: sprintf(text, "%05X...", data->selNewVal); break;
							case 6: sprintf(text, "%06X..", data->selNewVal); break;
							case 7: sprintf(text, "%07X.", data->selNewVal); break;
							}
						}
						else
						{
							SetBkColor(mem_hdc, RGB(255, 255, 255));
							SetTextColor(mem_hdc, RGB(0, 0, 0));
							
							sprintf(text, "%08X", val);
						}	

						TextOut(mem_hdc, curx, cury, text, strlen(text));
						curx += (fontwidth * (8+1));
					}
					curx += (fontwidth * 8);
				}
				break;
			}

			SetBkColor(mem_hdc, RGB(255, 255, 255));
			SetTextColor(mem_hdc, RGB(0, 0, 0));

			for(i = 0; i < 16; i++)
			{
				u8 val = T1ReadByte(memory, ((line << 4) + i));

				if((val >= 32) && (val <= 127))
					text[i] = (char)val;
				else
					text[i] = '.';
			}
			text[16] = '\0';
			TextOut(mem_hdc, curx, cury, text, strlen(text));

			cury += fontheight;
		}
	}

	BitBlt(hdc, 0, 0, w, h, mem_hdc, 0, 0, SRCCOPY);

	DeleteDC(mem_hdc);
	DeleteObject(mem_bmp);

	EndPaint(hCtl, &ps);

	return 0;
}

LRESULT CALLBACK MemView_ViewBoxProc(HWND hCtl, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	MemView_DataStruct *data = (MemView_DataStruct*)GetWindowLong(hCtl, DWL_USER);

	switch(uMsg)
	{
	case WM_NCCREATE:
		SetScrollRange(hCtl, SB_VERT, 0x00000000, 0x000FFFF0, TRUE);
		SetScrollPos(hCtl, SB_VERT, 0x00000000, TRUE);
		return 1;

	case WM_NCDESTROY:
		return 1;
	
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		MemView_ViewBoxPaint(hCtl, data, wParam, lParam);
		return 1;

	case WM_LBUTTONDOWN:
		{
			HDC hdc;
			HFONT font;
			SIZE fontsize;
			int x, y;

			data->sel = FALSE;
			data->selAddress = 0x00000000;
			data->selPart = 0;
			data->selNewVal = 0x00000000;

			hdc = GetDC(hCtl);
			font = (HFONT)SelectObject(hdc, GetStockObject(SYSTEM_FIXED_FONT));
			GetTextExtentPoint32(hdc, " ", 1, &fontsize);
			
			x = LOWORD(lParam);
			y = HIWORD(lParam);

			if((x >= ((fontsize.cx * 8) + 5)) && (y >= (fontsize.cy + 3)))
			{
				int line, col;

				x -= ((fontsize.cx * 8) + 5);
				y -= (fontsize.cy + 3);
				
				line = (y / fontsize.cy);

				switch(data->viewMode)
				{
				case 0:
					{
						if((x < (fontsize.cx * 2)) || (x >= (fontsize.cx * (2 + ((2+1) * 16)))))
							break;

						col = ((x - (fontsize.cx * 2)) / (fontsize.cx * (2+1)));

						data->sel = TRUE;
						
					}
					break;

				case 1:
					{
						if((x < (fontsize.cx * 6)) || (x >= (fontsize.cx * (6 + ((4+1) * 8)))))
							break;

						col = ((x - (fontsize.cx * 6)) / (fontsize.cx * (4+1)) * 2);

						data->sel = TRUE;
						
					}
					break;

				case 2:
					{
						if((x < (fontsize.cx * 8)) || (x >= (fontsize.cx * (8 + ((8+1) * 4)))))
							break;

						col = ((x - (fontsize.cx * 8)) / (fontsize.cx * (8+1)) * 4);

						data->sel = TRUE;
						
					}
					break;
				}
				
				data->selAddress = (data->address + (line << 4) + col);
				data->selPart = 0;
				data->selNewVal = 0x00000000;
			}

			SelectObject(hdc, font);
			ReleaseDC(hCtl, hdc);

			SetFocus(hCtl);				/* Required to receive keyboard messages */
			InvalidateRect(hCtl, NULL, FALSE); UpdateWindow(hCtl);
		}
		return 1;

	case WM_CHAR:
		{
			char ch = (char)wParam;

			if(((ch >= '0') && (ch <= '9')) || ((ch >= 'A') && (ch <= 'F')) || ((ch >= 'a') && (ch <= 'f')))
			{
				u8 maxSelPart[3] = {2, 4, 8};

				data->selNewVal <<= 4;
				data->selPart++;

				if((ch >= '0') && (ch <= '9'))
					data->selNewVal |= (ch - '0');
				else if((ch >= 'A') && (ch <= 'F'))
					data->selNewVal |= (ch - 'A' + 0xA);
				else if((ch >= 'a') && (ch <= 'f'))
					data->selNewVal |= (ch - 'a' + 0xA);

				if(data->selPart >= maxSelPart[data->viewMode])
				{
					switch(data->viewMode)
					{
					case 0: MMU_write8(data->cpu, data->selAddress, (u8)data->selNewVal); data->selAddress++; break;
					case 1: MMU_write16(data->cpu, data->selAddress, (u16)data->selNewVal); data->selAddress += 2; break;
					case 2: MMU_write32(data->cpu, data->selAddress, data->selNewVal); data->selAddress += 4; break;
					}
					data->selPart = 0;
					data->selNewVal = 0x00000000;

					if(data->selAddress == 0x00000000)
					{
						data->sel = FALSE;
					}
					else if(data->selAddress >= (data->address + 0x100))
					{
						data->address = min((u32)0xFFFFFF00, (data->address + 0x10));
						SetScrollPos(hCtl, SB_VERT, ((data->address >> 4) & 0x000FFFFF), TRUE);
					}
				}
			}

			InvalidateRect(hCtl, NULL, FALSE); UpdateWindow(hCtl);
		}
		return 1;

	case WM_VSCROLL:
		{
			int firstpos = GetScrollPos(hCtl, SB_VERT);

			switch(LOWORD(wParam))
			{
			case SB_LINEUP:
				data->address = (u32)max(0x00000000, ((int)data->address - 0x10));
				break;

			case SB_LINEDOWN:
				data->address = min((u32)0xFFFFFF00, (data->address + 0x10));
				break;

			case SB_PAGEUP:
				data->address = (u32)max(0x00000000, ((int)data->address - 0x100));
				break;

			case SB_PAGEDOWN:
				data->address = min((u32)0xFFFFFF00, (data->address + 0x100));
				break;

			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				{
					SCROLLINFO si;
					
					ZeroMemory(&si, sizeof(si));
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS;

					GetScrollInfo(hCtl, SB_VERT, &si);

					data->address = min((u32)0xFFFFFF00, (data->address + ((si.nTrackPos - firstpos) * 16)));
				}
				break;
			}

			if((data->selAddress < data->address) || (data->selAddress >= (data->address + 0x100)))
			{
				data->sel = FALSE;
				data->selAddress = 0x00000000;
				data->selPart = 0;
				data->selNewVal = 0x00000000;
			}

			SetScrollPos(hCtl, SB_VERT, ((data->address >> 4) & 0x000FFFFF), TRUE);
			InvalidateRect(hCtl, NULL, FALSE); UpdateWindow(hCtl);
		}
		return 1;
	}

	return DefWindowProc(hCtl, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
