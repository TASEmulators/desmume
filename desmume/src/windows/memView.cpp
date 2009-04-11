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
	MemView_DataStruct(u8 CPU) : cpu(CPU), address(0x02000000), viewMode(0)
	{
	}

	HWND hDlg;
	u8 cpu;
	u32 address;
	u8 viewMode;
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

BOOL MemView_DlgOpen(HWND hParentWnd, u8 CPU)
{
	HWND hDlg;
	char title[32];

	MemView_Data[CPU] = new MemView_DataStruct(CPU);
	if(MemView_Data[CPU] == NULL)
		return 0;

	hDlg = CreateDialogParam(hAppInst, MAKEINTRESOURCE(IDD_MEM_VIEW), hParentWnd, MemView_DlgProc, (LPARAM)MemView_Data[CPU]);
	if(hDlg == NULL)
		return 0;

	MemView_Data[CPU]->hDlg = hDlg;

	sprintf(title, "ARM%s memory", ((CPU == ARMCPU_ARM7) ? "7" : "9"));
	SetWindowText(hDlg, title);

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

	GetTextExtentPoint32(mem_hdc, " ", 1, &fontsize);
	fontwidth = fontsize.cx;
	fontheight = fontsize.cy;
		
	FillRect(mem_hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	if(data != NULL)
	{
		u32 addr = data->address;
		u8 memory[0x100];
		char text[80];
		int startx, cury;
		int line;

		startx = 0;
		cury = 0;

		sprintf(text, "        ");
		GetTextExtentPoint32(mem_hdc, text, strlen(text), &fontsize);
		startx = (fontsize.cx + 5);
		cury = (fontsize.cy + 3);

		MoveToEx(mem_hdc, (fontsize.cx + 2), 0, NULL);
		LineTo(mem_hdc, (fontsize.cx + 2), h);

		MoveToEx(mem_hdc, 0, (fontsize.cy + 1), NULL);
		LineTo(mem_hdc, w, (fontsize.cy + 1));

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

			switch(data->viewMode)
			{
			case 0:
				{
					sprintf(text, "  ");
					for(i = 0; i < 16; i++)
					{
						sprintf(text, "%s%02X ", text, T1ReadByte(memory, ((line << 4) + i)));
					}
					sprintf(text, "%s  ", text);
				}
				break;

			case 1:
				{
					sprintf(text, "      ");
					for(i = 0; i < 16; i += 2)
					{
						sprintf(text, "%s%04X ", text, T1ReadWord(memory, ((line << 4) + i)));
					}
					sprintf(text, "%s      ", text);
				}
				break;

			case 2:
				{
					sprintf(text, "        ");
					for(i = 0; i < 16; i += 4)
					{
						sprintf(text, "%s%08X ", text, T1ReadLong(memory, ((line << 4) + i)));
					}
					sprintf(text, "%s        ", text);
				}
				break;
			}

			for(i = 0; i < 16; i++)
			{
				u8 val = T1ReadByte(memory, ((line << 4) + i));

				if((val >= 32) && (val <= 127))
					sprintf(text, "%s%c", text, (char)val);
				else
					sprintf(text, "%s.", text);
			}

			TextOut(mem_hdc, startx, cury, text, strlen(text));

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

			SetScrollPos(hCtl, SB_VERT, ((data->address >> 4) & 0x000FFFFF), TRUE);
			InvalidateRect(hCtl, NULL, FALSE); UpdateWindow(hCtl);
		}
		return 1;
	}

	return DefWindowProc(hCtl, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
