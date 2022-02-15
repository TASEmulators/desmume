/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2016 DeSmuME team

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

#include "ginfo.h"

#include <stdio.h>
#include <commctrl.h>

#include "MMU.h"
#include "NDSSystem.h"

#include "resource.h"
#include "FirmConfig.h"
#include "main.h"
#include "Database.h"

//////////////////////////////////////////////////////////////////////////////

BOOL GInfo_Init()
{
	WNDCLASSEX wc;

	wc.cbSize         = sizeof(wc);
	wc.lpszClassName  = "GInfo_IconBox";
	wc.hInstance      = hAppInst;
	wc.lpfnWndProc    = GInfo_IconBoxProc;
	wc.hCursor        = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon          = 0;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = 0;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = 0;
	wc.hIconSm        = 0;

	RegisterClassEx(&wc);

	return 1;
}

void GInfo_DeInit()
{
	UnregisterClass("GInfo_IconBox", hAppInst);
}

//////////////////////////////////////////////////////////////////////////////

BOOL GInfo_DlgOpen(HWND hParentWnd)
{
	HWND hDlg;

	hDlg = CreateDialogW(hAppInst, MAKEINTRESOURCEW(IDD_GAME_INFO), hParentWnd, GInfo_DlgProc);
	if(hDlg == NULL)
		return 0;

	ShowWindow(hDlg, SW_SHOW);
	UpdateWindow(hDlg);

	return 1;
}

//////////////////////////////////////////////////////////////////////////////

LRESULT GInfo_Paint(HWND hDlg, WPARAM wParam, LPARAM lParam)
{
	HDC				hdc;
	PAINTSTRUCT		ps;
	wchar_t			text[80];
	char			atext[80];
	u32				icontitleOffset;
	wchar_t			*utf16text;
	u32				val;

       
	hdc = BeginPaint(hDlg, &ps);

	const RomBanner& banner = gameInfo.getRomBanner();

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[CommonSettings.fwConfig.language]);	// TODO: langID from real/fake fw
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLE), text);

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[0]);
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLEJP), text);

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[1]);
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLEEN), text);

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[2]);
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLEFR), text);

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[3]);
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLEGE), text);

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[4]);
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLEIT), text);

	swprintf(text, 80, L"%s", (wchar_t*)banner.titles[5]);
	SetWindowTextW(GetDlgItem(hDlg, IDC_GI_TITLESP), text);
	
	//TODO - pull this from the header, not straight out of the rom (yuck!)

	memcpy(atext, (u8*)&gameInfo.header, 12);
	atext[12] = '\0';
	SetWindowText(GetDlgItem(hDlg, IDC_GI_GAMETITLE), atext);

	SetDlgItemText(hDlg, IDC_GI_GAMECODE, gameInfo.ROMserial);

	memcpy(atext, ((u8*)&gameInfo.header+0x10), 2);
	atext[2] = '\0';
	SetWindowText(GetDlgItem(hDlg, IDC_GI_MAKERCODE), atext);
	SetWindowText(GetDlgItem(hDlg, IDC_SDEVELOPER), Database::MakerNameForMakerCode(T1ReadWord((u8*)&gameInfo.header, 0x10),true));

	val = T1ReadByte((u8*)&gameInfo.header, 0x14);
	sprintf(atext, "%i kilobytes", (0x80 << val));
	SetWindowText(GetDlgItem(hDlg, IDC_GI_CHIPSIZE), atext);


	val = T1ReadLong((u8*)&gameInfo.header, 0x20);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM9ROM), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x24);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM9ENTRY), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x28);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM9START), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x2C);
	sprintf(atext, "%i bytes", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM9SIZE), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x30);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM7ROM), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x34);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM7ENTRY), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x38);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM7START), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x3C);
	sprintf(atext, "%i bytes", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ARM7SIZE), atext);


	val = T1ReadLong((u8*)&gameInfo.header, 0x40);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_FNTOFS), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x44);
	sprintf(atext, "%i bytes", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_FNTSIZE), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x48);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_FATOFS), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x4C);
	sprintf(atext, "%i bytes", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_FATSIZE), atext);

	icontitleOffset = T1ReadLong((u8*)&gameInfo.header, 0x68);
	sprintf(atext, "0x%08X", icontitleOffset);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_ICONTITLEOFS), atext);

	val = T1ReadLong((u8*)&gameInfo.header, 0x80);
	sprintf(atext, "0x%08X", val);
	SetWindowText(GetDlgItem(hDlg, IDC_GI_USEDROMSIZE), atext);

	EndPaint(hDlg, &ps);

	return 0;
}

INT_PTR CALLBACK GInfo_DlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_INITDIALOG:
		return 1;

	case WM_CLOSE:
		EndDialog(hDlg, 0);
		return 1;

	case WM_PAINT:
		GInfo_Paint(hDlg, wParam, lParam);
		return 1;

	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDCANCEL:
			EndDialog(hDlg, 0);
			return 1;
		}
		return 0;
	}

	return 0;    
}

//////////////////////////////////////////////////////////////////////////////

LRESULT GInfo_IconBoxPaint(HWND hCtl, WPARAM wParam, LPARAM lParam)
{
	HDC				hdc;
	PAINTSTRUCT		ps;
	RECT			rc;
	int				w, h;
	SIZE			fontsize;
	HDC				mem_hdc;
	HBITMAP			mem_bmp;
	BITMAPV4HEADER	bmph;
	u32				icontitleOffset;
	u16				icon[32 * 32];
	int				x, y;

	GetClientRect(hCtl, &rc);
	w = (rc.right - rc.left);
	h = (rc.bottom - rc.top);

	hdc = BeginPaint(hCtl, &ps);

	mem_hdc = CreateCompatibleDC(hdc);
	mem_bmp = CreateCompatibleBitmap(hdc, w, h);
	SelectObject(mem_hdc, mem_bmp);
		
	FillRect(mem_hdc, &rc, (HBRUSH)GetStockObject(WHITE_BRUSH));

	ZeroMemory(&bmph, sizeof(bmph));
	bmph.bV4Size			 = sizeof(bmph);
	bmph.bV4Planes			 = 1;
	bmph.bV4BitCount		 = 16;
	bmph.bV4V4Compression	 = BI_BITFIELDS;
	bmph.bV4RedMask			 = 0x001F;
	bmph.bV4GreenMask		 = 0x03E0;
	bmph.bV4BlueMask		 = 0x7C00;
	bmph.bV4Width			 = 32;
	bmph.bV4Height			 = -32;

	const RomBanner& banner = gameInfo.getRomBanner();

	if(gameInfo.hasRomBanner())
	{
		for(y = 0; y < 32; y++)
		{
			for(x = 0; x < 32; x++)
			{
				int tilenum = (((y / 8) * 4) + (x / 8));
				int tilex = (x % 8);
				int tiley = (y % 8);
				int mapoffset = ((tilenum * 64) + (tiley * 8) + tilex);

				u8 val = banner.bitmap[(mapoffset>>1)];

				if(mapoffset & 1)
					val = ((val >> 4) & 0xF);
				else
					val = (val & 0xF);

				icon[(y * 32) + x] = banner.palette[val];
			}
		}

		SetDIBitsToDevice(mem_hdc, ((w/2) - 16), ((h/2) - 16), 32, 32, 0, 0, 0, 32, icon, (BITMAPINFO*)&bmph, DIB_RGB_COLORS);
	}
	else
	{
		char *noicon = "No icon";
		GetTextExtentPoint32(mem_hdc, noicon, strlen(noicon), &fontsize);
		TextOut(mem_hdc, ((w/2) - (fontsize.cx/2)), ((h/2) - (fontsize.cy/2)), noicon, strlen(noicon));
	}
	
	BitBlt(hdc, 0, 0, w, h, mem_hdc, 0, 0, SRCCOPY);

	DeleteDC(mem_hdc);
	DeleteObject(mem_bmp);

	EndPaint(hCtl, &ps);

	return 0;
}

LRESULT CALLBACK GInfo_IconBoxProc(HWND hCtl, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch(uMsg)
	{
	case WM_NCCREATE:
		return 1;

	case WM_NCDESTROY:
		return 1;
	
	case WM_ERASEBKGND:
		return 1;

	case WM_PAINT:
		GInfo_IconBoxPaint(hCtl, wParam, lParam);
		return 1;
	}

	return DefWindowProc(hCtl, uMsg, wParam, lParam);
}

//////////////////////////////////////////////////////////////////////////////
