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

#include "CWindow.h"
#include "IORegView.h"
#include <commctrl.h>
#include "debug.h"
#include "resource.h"
#include "../MMU.h"
#include "../armcpu.h"

/*--------------------------------------------------------------------------*/

enum EIORegType
{
	ListEnd = 0,
	AllRegs,
	CatBegin,
	MMIOReg,
	CP15Reg
};

typedef struct _IORegBitfield
{
	char name[32];
	int shift;
	int nbits;
} IORegBitfield;

typedef struct _IOReg
{
	EIORegType type;
	char name[32];
	u32 address;
	int size;			// for AllRegs: total number of regs; for CatBegin: number of regs in category
	int numBitfields;
	IORegBitfield bitfields[32];
} IOReg;

IOReg IORegs9[] = {
	{AllRegs, "All registers", 0, 9, 0, {{0}}},

	{CatBegin, "General video registers", 0, 1, 0, {{0}}},
	{MMIOReg, "DISPSTAT", 0x04000004, 2, 8,    {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Enable VBlank IRQ",3,1},
												{"Enable HBlank IRQ",4,1},{"Enable VCount match IRQ",5,1},{"VCount MSb",7,1},{"VCount LSb's",8,8}}},

	{CatBegin, "Video engine A registers", 0, 1, 0, {{0}}},
	{MMIOReg, "DISPCNT_A", 0x04000000, 4, 23,  {{"BG mode",0,3},{"BG0 -> 3D",3,1},{"Tile OBJ mapping",4,1},{"Bitmap OBJ 2D size",5,1},
												{"Bitmap OBJ mapping",6,1},{"Forced blank",7,1},{"Display BG0",8,1},{"Display BG1",9,1},
												{"Display BG2",10,1},{"Display BG3",11,1},{"Display OBJ",12,1},{"Display window 0",13,1},
												{"Display window 1",14,1},{"Display OBJ window",15,1},{"Display mode",16,2},{"VRAM block",18,2},
												{"Tile OBJ 1D boundary",20,2},{"Bitmap OBJ 1D boundary",22,1},{"Process OBJs during HBlank",23,1},{"Character base",24,3},
												{"Screen base",27,3},{"Enable BG ext. palettes",30,1},{"Enable OBJ ext. palettes",31,1}}},

	{CatBegin, "Video engine B registers", 0, 1, 0, {{0}}},
	{MMIOReg, "DISPCNT_B", 0x04001000, 4, 18,  {{"BG mode",0,3},{"Tile OBJ mapping",4,1},{"Bitmap OBJ 2D size",5,1},{"Bitmap OBJ mapping",6,1},
												{"Forced blank",7,1},{"Display BG0",8,1},{"Display BG1",9,1},{"Display BG2",10,1},
												{"Display BG3",11,1},{"Display OBJ",12,1},{"Display window 0",13,1},{"Display window 1",14,1},
												{"Display OBJ window",15,1},{"Display mode",16,2},{"Tile OBJ 1D boundary",20,2},{"Process OBJs during HBlank",23,1},
												{"Enable BG ext. palettes",30,1},{"Enable OBJ ext. palettes",31,1}}},
	
	{CatBegin, "3D video engine registers", 0, 1, 0, {{0}}},
	{MMIOReg, "GXSTAT", 0x04000600, 4, 12, {{"Box/Pos/Vec-test busy",0,1},{"Box-test result",1,1},{"Pos&Vec mtx stack level",8,5},{"Proj mtx stack level",13,1},
											{"Mtx stack busy",14,1},{"Mtx stack over/under-flow",15,1},{"GX FIFO level",16,9},{"GX FIFO full",24,1},
											{"GX FIFO less than half full",25,1},{"GX FIFO empty",26,1},{"GX busy",27,1},{"GX FIFO IRQ condition",30,2}}},

	{CatBegin, "IPC registers", 0, 2, 0, {{0}}},
	{MMIOReg, "IPCSYNC", 0x04000180, 2, 3, {{"Data input from remote",0,4},{"Data output to remote",8,4},{"Enable IRQ from remote",14,1}}},
	{MMIOReg, "IPCFIFOCNT", 0x04000184, 2, 8,  {{"Send FIFO empty",0,1},{"Send FIFO full",1,1},{"Enable send FIFO empty IRQ",2,1},{"Recv FIFO empty",8,1},
												{"Recv FIFO full",9,1},{"Enable recv FIFO not empty IRQ",10,1},{"Recv empty/Send full",14,1},{"Enable send/recv FIFO",15,1}}},

	{CatBegin, "IRQ control registers", 0, 3, 0, {{0}}},
	{MMIOReg, "IME", 0x04000208, 2, 1, {{"Enable IRQs",0,1}}},
	{MMIOReg, "IE", 0x04000210, 4, 21, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
										{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"DMA 0",8,1},
										{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},{"Keypad",12,1},
										{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},{"IPC recv FIFO not empty",18,1},
										{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"GX FIFO",21,1}}},
	{MMIOReg, "IF", 0x04000214, 4, 21, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
										{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"DMA 0",8,1},
										{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},{"Keypad",12,1},
										{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},{"IPC recv FIFO not empty",18,1},
										{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"GX FIFO",21,1}}},

	{ListEnd, 0, 0, 0, 0, {{0}}}
};

IOReg IORegs7[] = {
	{AllRegs, "All registers", 0, 5, 0, {{0}}},

	{CatBegin, "IPC registers", 0, 2, 0, {{0}}},
	{MMIOReg, "IPCSYNC", 0x04000180, 2, 3, {{"Data input from remote",0,4},{"Data output to remote",8,4},{"Enable IRQ from remote",14,1}}},
	{MMIOReg, "IPCFIFOCNT", 0x04000184, 2, 8,  {{"Send FIFO empty",0,1},{"Send FIFO full",1,1},{"Enable send FIFO empty IRQ",2,1},{"Recv FIFO empty",8,1},
												{"Recv FIFO full",9,1},{"Enable recv FIFO not empty IRQ",10,1},{"Recv empty/Send full",14,1},{"Enable send/recv FIFO",15,1}}},

	{CatBegin, "IRQ control registers", 0, 3, 0, {{0}}},
	{MMIOReg, "IME", 0x04000208, 2, 1, {{"Enable IRQs",0,1}}},
	{MMIOReg, "IE", 0x04000210, 4, 22, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
										{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"RTC",7,1},
										{"DMA 0",8,1},{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},
										{"Keypad",12,1},{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},
										{"IPC recv FIFO not empty",18,1},{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"Lid opened",22,1},
										{"SPI bus",23,1},{"Wifi",24,1}}},
	{MMIOReg, "IF", 0x04000214, 4, 22, {{"VBlank",0,1},{"HBlank",1,1},{"VCount match",2,1},{"Timer 0",3,1},
										{"Timer 1",4,1},{"Timer 2",5,1},{"Timer 3",6,1},{"RTC",7,1},
										{"DMA 0",8,1},{"DMA 1",9,1},{"DMA 2",10,1},{"DMA 3",11,1},
										{"Keypad",12,1},{"Game Pak",13,1},{"IPC sync",16,1},{"IPC send FIFO empty",17,1},
										{"IPC recv FIFO not empty",18,1},{"Gamecard transfer",19,1},{"Gamecard IREQ_MC",20,1},{"Lid opened",22,1},
										{"SPI bus",23,1},{"Wifi",24,1}}},

	{ListEnd, 0, 0, 0, 0, {{0}}}
};

IOReg* IORegs[2] = {IORegs9, IORegs7};

static const int kXMargin = 5;
static const int kYMargin = 1;

/*--------------------------------------------------------------------------*/

CIORegView::CIORegView()
	: CToolWindow("DeSmuME_IORegView", IORegView_Proc, "I/O registers", 400, 400)
	, CPU(ARMCPU_ARM9)
	, Reg(0)
	, yoff(0)
{
}

CIORegView::~CIORegView()
{
	DestroyWindow(hWnd);
	UnregWndClass("DeSmuME_IORegView");
}

/*--------------------------------------------------------------------------*/

void CIORegView::ChangeCPU(int cpu)
{
	CPU = cpu;

	SendMessage(hRegCombo, CB_RESETCONTENT, 0, 0);
	for (int i = 0; ; i++)
	{
		IOReg reg = IORegs[CPU][i];
		char str[128];

		if (reg.type == ListEnd)
			break;
		else
		switch (reg.type)
		{
		case AllRegs:
			sprintf(str, "* %s", reg.name);
			break;
		case CatBegin:
			sprintf(str, "** %s", reg.name);
			break;
		case MMIOReg:
			sprintf(str, "*** 0x%08X - %s", reg.address, reg.name);
			break;
		}

		SendMessage(hRegCombo, CB_ADDSTRING, 0, (LPARAM)str);
	}
	ChangeReg(0);
}

void CIORegView::ChangeReg(int reg)
{
	Reg = reg;
	IOReg _reg = IORegs[CPU][Reg];

	if ((_reg.type == AllRegs) || (_reg.type == CatBegin))
		numlines = 2 + _reg.size;
	else
		numlines = 3 + _reg.numBitfields;

	if (maxlines < numlines)
	{
		BOOL oldenable = IsWindowEnabled(hScrollbar);
		if (!oldenable)
		{
			RECT rc; GetClientRect(hWnd, &rc);

			EnableWindow(hScrollbar, TRUE);
			SendMessage(hScrollbar, SBM_SETRANGE, 0, (numlines * lineheight) - (rc.bottom - rebarHeight));
			SendMessage(hScrollbar, SBM_SETPOS, 0, 0);
		}
	}
	else
		EnableWindow(hScrollbar, FALSE);

	SendMessage(hRegCombo, CB_SETCURSEL, Reg, 0);
}

/*--------------------------------------------------------------------------*/

void IORegView_Paint(CIORegView* wnd, HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	HDC 			hDC; 
	PAINTSTRUCT 	ps;
	HDC 			hMemDC;
	HBITMAP			hMemBitmap;
	RECT			rc;
	HPEN 			pen;
	int				x, y, w, h;
	int				nameColWidth;
	int				curx, cury;
	SIZE			fontsize;
	IOReg			reg;
	char 			txt[80];

	GetClientRect(hWnd, &rc);
	x = 0;
	y = wnd->rebarHeight;
	w = rc.right - wnd->vsbWidth;
	h = rc.bottom - wnd->rebarHeight;
	curx = kXMargin; cury = wnd->yoff + kYMargin;

	hDC = BeginPaint(hWnd, &ps);

	hMemDC = CreateCompatibleDC(hDC);
	hMemBitmap = CreateCompatibleBitmap(hDC, w, h);
	SelectObject(hMemDC, hMemBitmap);

	pen = CreatePen(PS_SOLID, 1, RGB(210, 230, 255));
	SelectObject(hMemDC, pen);

	SelectObject(hMemDC, wnd->hFont);
	GetTextExtentPoint32(hMemDC, " ", 1, &fontsize);

	FillRect(hMemDC, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	reg = IORegs[wnd->CPU][wnd->Reg];
	if ((reg.type == AllRegs) || (reg.type == CatBegin))
	{
		nameColWidth = w - (kXMargin + (fontsize.cx*8) + kXMargin + 1 + kXMargin + kXMargin + 1 + kXMargin + (fontsize.cx*8) + kXMargin);

		DrawText(hMemDC, reg.name, curx, cury, w, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		cury += fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury ++;

		curx = kXMargin;
		DrawText(hMemDC, "Address", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_LEFT);
		curx += (fontsize.cx*8) + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Name", curx, cury+kYMargin, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		curx += nameColWidth + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Value", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_RIGHT);

		cury += kYMargin + fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury += 1 + kYMargin;

		for (int i = wnd->Reg+1; ; i++)
		{
			IOReg curReg = IORegs[wnd->CPU][i];
			curx = kXMargin;

			if (curReg.type == CatBegin)
			{
				if (reg.type == AllRegs) continue;
				else break;
			}
			else if (curReg.type == ListEnd)
				break;
			else if (curReg.type == MMIOReg)
			{
				u32 val;

				sprintf(txt, "%08X", curReg.address);
				DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_LEFT);
				curx += (fontsize.cx*8) + kXMargin + 1 + kXMargin;

				DrawText(hMemDC, curReg.name, curx, cury, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
				curx += nameColWidth + kXMargin + 1 + kXMargin;

				switch (curReg.size)
				{
				case 1:
					val = (u32)MMU_read8(wnd->CPU, curReg.address);
					sprintf(txt, "%02X", val);
					break;
				case 2:
					val = (u32)MMU_read16(wnd->CPU, curReg.address);
					sprintf(txt, "%04X", val);
					break;
				case 4:
					val = MMU_read32(wnd->CPU, curReg.address);
					sprintf(txt, "%08X", val);
					break;
				}
				DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_RIGHT);
			}

			cury += fontsize.cy + kYMargin;
			if (cury >= h) break;
			MoveToEx(hMemDC, 0, cury, NULL);
			LineTo(hMemDC, w, cury);
			cury += 1 + kYMargin;
		}
	}
	else
	{
		u32 val;
		nameColWidth = w - (kXMargin + (fontsize.cx*8) + kXMargin + 1 + kXMargin + kXMargin + 1 + kXMargin + (fontsize.cx*8) + kXMargin);

		sprintf(txt, "%08X - %s", reg.address, reg.name);
		DrawText(hMemDC, txt, curx, cury, w, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		cury += fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury += 1 + kYMargin;

		switch (reg.size)
		{
		case 1:
			val = (u32)MMU_read8(wnd->CPU, reg.address);
			sprintf(txt, "Value:       %02X", val);
			break;
		case 2:
			val = (u32)MMU_read16(wnd->CPU, reg.address);
			sprintf(txt, "Value:     %04X", val);
			break;
		case 4:
			val = MMU_read32(wnd->CPU, reg.address);
			sprintf(txt, "Value: %08X", val);
			break;
		}
		DrawText(hMemDC, txt, curx, cury, w, fontsize.cy, DT_LEFT);
		cury += fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury ++;

		curx = kXMargin;
		DrawText(hMemDC, "Bits", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_LEFT);
		curx += (fontsize.cx*8) + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Description", curx, cury+kYMargin, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
		curx += nameColWidth + kXMargin;
		MoveToEx(hMemDC, curx, cury, NULL);
		LineTo(hMemDC, curx, h);
		curx += 1 + kXMargin;
		DrawText(hMemDC, "Value", curx, cury+kYMargin, fontsize.cx*8, fontsize.cy, DT_RIGHT);

		cury += kYMargin + fontsize.cy + kYMargin;
		MoveToEx(hMemDC, 0, cury, NULL);
		LineTo(hMemDC, w, cury);
		cury += 1 + kYMargin;

		for (int i = 0; i < reg.numBitfields; i++)
		{
			IORegBitfield bitfield = reg.bitfields[i];
			curx = kXMargin;

			if (bitfield.nbits > 1)
				sprintf(txt, "Bit%i-%i", bitfield.shift, bitfield.shift + bitfield.nbits - 1);
			else
				sprintf(txt, "Bit%i", bitfield.shift);
			DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_LEFT);
			curx += (fontsize.cx*8) + kXMargin + 1 + kXMargin;

			DrawText(hMemDC, bitfield.name, curx, cury, nameColWidth, fontsize.cy, DT_LEFT | DT_END_ELLIPSIS);
			curx += nameColWidth + kXMargin + 1 + kXMargin;

			char bfpattern[8];
			sprintf(bfpattern, "%%0%iX", ((bitfield.nbits+3)&~3) >> 2);
			sprintf(txt, bfpattern, (val >> bitfield.shift) & ((1<<bitfield.nbits)-1));
			DrawText(hMemDC, txt, curx, cury, fontsize.cx*8, fontsize.cy, DT_RIGHT);

			cury += fontsize.cy + kYMargin;
			if (cury >= h) break;
			MoveToEx(hMemDC, 0, cury, NULL);
			LineTo(hMemDC, w, cury);
			cury += 1 + kYMargin;
		}
	}

	BitBlt(hDC, x, y, w, h, hMemDC, 0, 0, SRCCOPY);

	DeleteDC(hMemDC);
	DeleteObject(hMemBitmap);

	DeleteObject(pen);

	EndPaint(hWnd, &ps);
}

LRESULT CALLBACK IORegView_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	CIORegView* wnd = (CIORegView*)GetWindowLongPtr(hWnd, DWLP_USER);
	if ((wnd == NULL) && (uMsg != WM_CREATE))
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_CREATE:
		{
			RECT rc;
			SIZE fontsize;

			// Retrieve the CIORegView instance passed upon window creation
			// and match it to the window
			wnd = (CIORegView*)((CREATESTRUCT*)lParam)->lpCreateParams;
			SetWindowLongPtr(hWnd, DWLP_USER, (LONG)wnd);

			// Create the fixed-pitch font
			wnd->hFont = CreateFont(16, 0, 0, 0, FW_MEDIUM, FALSE, FALSE, FALSE, DEFAULT_CHARSET, 
				OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, GetFontQuality(), FIXED_PITCH, "Courier New");

			// Create the vertical scrollbar
			// The sizing and positioning of the scrollbar is done in WM_SIZE messages
			wnd->vsbWidth = GetSystemMetrics(SM_CXVSCROLL);
			wnd->hScrollbar = CreateWindow("Scrollbar", "",
				WS_CHILD | WS_VISIBLE | WS_DISABLED | SBS_VERT,
				0, 0, 0, 0, hWnd, NULL, hAppInst, NULL);

			// Create the rebar that will hold all the controls
			wnd->hRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, NULL, 
				WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | CCS_NODIVIDER | RBS_VARHEIGHT | RBS_BANDBORDERS, 
				0, 0, 0, 0, hWnd, NULL, hAppInst, NULL);

			// Create the CPU combobox and fill it
			wnd->hCPUCombo = CreateWindow("ComboBox", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 
				0, 0, 0, 50, wnd->hRebar, (HMENU)IDC_CPU, hAppInst, NULL);

			SendMessage(wnd->hCPUCombo, WM_SETFONT, (WPARAM)wnd->hFont, TRUE);
			SendMessage(wnd->hCPUCombo, CB_ADDSTRING, 0, (LPARAM)"ARM9");
			SendMessage(wnd->hCPUCombo, CB_ADDSTRING, 0, (LPARAM)"ARM7");
			SendMessage(wnd->hCPUCombo, CB_SETCURSEL, 0, 0);

			// Create the reg combobox and fill it
			wnd->hRegCombo = CreateWindow("ComboBox", "",
				WS_CHILD | WS_VISIBLE | WS_VSCROLL | CBS_DROPDOWNLIST, 
				0, 0, 0, 400, wnd->hRebar, (HMENU)IDC_IOREG, hAppInst, NULL);

			SendMessage(wnd->hRegCombo, WM_SETFONT, (WPARAM)wnd->hFont, TRUE);
			SendMessage(wnd->hRegCombo, CB_SETDROPPEDWIDTH, 300, 0);
			wnd->ChangeCPU(ARMCPU_ARM9);
			SendMessage(wnd->hRegCombo, CB_SETCURSEL, 0, 0);

			// Add all those nice controls to the rebar
			REBARBANDINFO rbBand = { 80 };

			rbBand.fMask = RBBIM_STYLE | RBBIM_TEXT | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_SIZE;
			rbBand.fStyle = RBBS_CHILDEDGE | RBBS_NOGRIPPER;

			GetWindowRect(wnd->hCPUCombo, &rc);
			rbBand.lpText = "CPU: ";
			rbBand.hwndChild = wnd->hCPUCombo;
			rbBand.cxMinChild = 0;
			rbBand.cyMinChild = rc.bottom - rc.top;
			SendMessage(wnd->hRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

			GetWindowRect(wnd->hRegCombo, &rc);
			rbBand.lpText = "Registers: ";
			rbBand.hwndChild = wnd->hRegCombo;
			rbBand.cxMinChild = 0;
			rbBand.cyMinChild = rc.bottom - rc.top;
			rbBand.cx = 0;
			SendMessage(wnd->hRebar, RB_INSERTBAND, (WPARAM)-1, (LPARAM)&rbBand);

			GetWindowRect(wnd->hRebar, &rc);
			wnd->rebarHeight = rc.bottom - rc.top;

			GetFontSize(hWnd, wnd->hFont, &fontsize);
			wnd->lineheight = kYMargin + fontsize.cy + kYMargin + 1;
		}
		return 0;

	case WM_CLOSE:
		CloseToolWindow(wnd);
		return 0;

	case WM_SIZE:
		{
			RECT rc;
			int wndHeight, lineHeight;

			// Resize and reposition the controls
			SetWindowPos(wnd->hRebar, NULL, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, SWP_NOZORDER | SWP_NOMOVE);

			GetClientRect(hWnd, &rc);
			SetWindowPos(wnd->hScrollbar, NULL, rc.right-wnd->vsbWidth, wnd->rebarHeight, wnd->vsbWidth, rc.bottom-wnd->rebarHeight, SWP_NOZORDER);

			// Keep the first rebar band width to a reasonable value
			SendMessage(wnd->hRebar, RB_SETBANDWIDTH, 0, 60);

			GetClientRect(hWnd, &rc);
			wndHeight = rc.bottom - wnd->rebarHeight;
			wnd->maxlines = wndHeight / wnd->lineheight;

			if (wnd->maxlines < wnd->numlines)
			{
				BOOL oldenable = IsWindowEnabled(wnd->hScrollbar);
				if (!oldenable)
				{
					EnableWindow(wnd->hScrollbar, TRUE);
					SendMessage(wnd->hScrollbar, SBM_SETPOS, 0, 0);
				}
				SendMessage(wnd->hScrollbar, SBM_SETRANGE, 0, (wnd->numlines * wnd->lineheight) - wndHeight);
			}
			else
				EnableWindow(wnd->hScrollbar, FALSE);

			wnd->Refresh();
		}
		return 0;

	case WM_PAINT:
		IORegView_Paint(wnd, hWnd, wParam, lParam);
		return 0;

	case WM_VSCROLL:
		{
			int pos = SendMessage(wnd->hScrollbar, SBM_GETPOS, 0, 0);
			int minpos, maxpos;

			SendMessage(wnd->hScrollbar, SBM_GETRANGE, (WPARAM)&minpos, (LPARAM)&maxpos);

			switch(LOWORD(wParam))
			{
			case SB_LINEUP:
				pos = max(minpos, pos - 1);
				break;
			case SB_LINEDOWN:
				pos = min(maxpos, pos + 1);
				break;
			case SB_PAGEUP:
				pos = max(minpos, pos - wnd->lineheight);
				break;
			case SB_PAGEDOWN:
				pos = min(maxpos, pos + wnd->lineheight);
				break;
			case SB_THUMBTRACK:
			case SB_THUMBPOSITION:
				{
					SCROLLINFO si;
					
					ZeroMemory(&si, sizeof(si));
					si.cbSize = sizeof(si);
					si.fMask = SIF_TRACKPOS;

					SendMessage(wnd->hScrollbar, SBM_GETSCROLLINFO, 0, (LPARAM)&si);
					pos = si.nTrackPos;
				}
				break;
			}

			SendMessage(wnd->hScrollbar, SBM_SETPOS, pos, TRUE);
			wnd->yoff = -pos;
			wnd->Refresh();
		}
		return 0;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDC_CPU:
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
			case CBN_CLOSEUP:
				{
					int cpu = SendMessage(wnd->hCPUCombo, CB_GETCURSEL, 0, 0);
					if (cpu != wnd->CPU) 
					{
						wnd->ChangeCPU(cpu);
						wnd->Refresh();
					}
				}
				break;
			}
			break;

		case IDC_IOREG:
			switch (HIWORD(wParam))
			{
			case CBN_SELCHANGE:
			case CBN_CLOSEUP:
				{
					int reg = SendMessage(wnd->hRegCombo, CB_GETCURSEL, 0, 0);
					if (reg != wnd->Reg) 
					{
						wnd->ChangeReg(reg);
						wnd->Refresh();
					}
				}
				break;
			}
			break;
		}
		return 0;
	}

	return DefWindowProc(hWnd, uMsg, wParam, lParam);
}

/*--------------------------------------------------------------------------*/
