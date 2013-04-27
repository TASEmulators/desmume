/*
	Copyright (C) 2008 shash
	Copyright (C) 2008-2013 DeSmuME team

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


#include "../common.h"
#include "version.h"

#include "AboutBox.h"
#include "resource.h"

#define ABOUT_TIMER_ID 110222
const char	*team[] = { 
	"Current Team\1",
	"Guillaume Duhamel",
	"Normmatt",
	"Riccardo Magliocchetti",
	"Max Tabachenko (CrazyMax)",
	"zeromus",
	"rogerman",
	"Luigi__",
	"",
	"Contributors\1",
	"Allustar",
	"amponzi",
	"Anthony Molinaro",
	"ape",
	"Damien Nozay (damdoum)",
	"delfare",
	"Romain Vallet",
	"snkmad",
	"Theo Berkau",
	"thoduv",
	"Tim Seidel (Mighty Max)",
	"Pascal Giard (evilynux)",
	"Ben Jaques (masscat)",
	"Jeff Bland",
	"Bernat Muñoz (shash)",
	"matusz",
	"nitsuja",
	"gocha",
	"pa__",
	"adelikat",
	"hi-coder",
	"WinterMute",
	"pengvado",
	"dormito",
	"",
	"Honorary Nagmasters\1",
	"(Thanks to our super testers!)\1",
	"nash679",
	"pokefan999",
	"dottorleo",
};

static HWND		gList = NULL;
static RECT		gRc = {0};
static s32		gPosY = 0;
const u32		size = ARRAY_SIZE(team);

BOOL CALLBACK ListProc(HWND Dlg, UINT msg,WPARAM wparam,LPARAM lparam)
{

	switch (msg)
	{
		case WM_PAINT:
			{
				PAINTSTRUCT ps = {0};
				
				HDC hDC = BeginPaint(Dlg, &ps);
				HDC hdcMem = CreateCompatibleDC(hDC);
				HBITMAP hbmMem = CreateCompatibleBitmap(hDC, gRc.right, gRc.bottom);
				HANDLE hOld   = SelectObject(hdcMem, hbmMem);
				SetTextAlign(hdcMem, TA_CENTER);
				u32 x = gRc.right / 2;
				FillRect(hdcMem, &gRc, (HBRUSH)GetStockObject(WHITE_BRUSH));
				SetTextColor(hdcMem, RGB(255, 0, 0));
				for (u32 i = 0; i < size; i++)
				{
					s32 pos = gPosY+(i*20);
					if (pos > gRc.bottom) break;
					if (team[i][strlen(team[i])-1] == 1)
					{
						SetTextColor(hdcMem, RGB(255, 0, 0));
						ExtTextOut(hdcMem, x, pos, ETO_CLIPPED, &gRc, team[i], strlen(team[i])-1, NULL);
					}
					else
					{
						SetTextColor(hdcMem, RGB(0, 0, 0));
						ExtTextOut(hdcMem, x, pos, ETO_CLIPPED, &gRc, team[i], strlen(team[i]), NULL);
					}
					if ((i == size-1) && (pos < (s32)(gRc.top - 20))) gPosY = gRc.bottom;
				}
				FrameRect(hdcMem, &gRc, (HBRUSH)GetStockObject(BLACK_BRUSH));

				BitBlt(hDC, 0, 0, gRc.right, gRc.bottom, hdcMem, 0, 0, SRCCOPY);
				SelectObject(hdcMem, hOld);
				DeleteObject(hbmMem);
				DeleteDC(hdcMem);
				EndPaint(Dlg, &ps);
			}
			return TRUE;
	}
	return FALSE;
}

BOOL CALLBACK AboutBox_Proc (HWND dialog, UINT message,WPARAM wparam,LPARAM lparam)
{
	switch(message)
	{
		case WM_INITDIALOG: 
		{
			// Support Unicode text display
			wchar_t wstr[256];
			wchar_t wstr1[256];
			wchar_t wstr2[256];

			GetDlgItemTextW(dialog, IDC_TXT_VERSION, wstr,256);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, EMU_DESMUME_VERSION_STRING(), -1, wstr1, 255);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, EMU_DESMUME_COMPILER_DETAIL(), -1, wstr2, 255);
			wcscat(wstr, wcscat(wstr1, wstr2));
			SetDlgItemTextW(dialog, IDC_TXT_VERSION, wstr);

			GetDlgItemTextW(dialog, IDC_TXT_COMPILED, wstr,256);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, __DATE__, -1, wstr1, 255);
			MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, __TIME__, -1, wstr2, 255);
			wcscat(wstr, wcscat(wcscat(wstr1, L" "), wstr2));
			SetDlgItemTextW(dialog, IDC_TXT_COMPILED, wstr);
			
			gList = GetDlgItem(dialog, IDC_AUTHORS_LIST);
			SetWindowLongPtr(gList, GWLP_WNDPROC, (LONG_PTR)ListProc);
			GetClientRect(gList, &gRc);
			gPosY = gRc.bottom;

			SetTimer(dialog, ABOUT_TIMER_ID, 10, (TIMERPROC) NULL);
			break;
		}
	
		case WM_COMMAND:
		{
			if((HIWORD(wparam) == BN_CLICKED)&&(((int)LOWORD(wparam)) == IDC_FERMER))
			{
				KillTimer(dialog, ABOUT_TIMER_ID);
				EndDialog(dialog,0);
				return 1;
			}
			break;
		}

		case WM_TIMER:
		{
			gPosY--;
			InvalidateRect(gList, &gRc, false);
			break;
		}
	}
	return 0;
}
