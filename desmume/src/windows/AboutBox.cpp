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
	"Original author\1",
	"yopyop",
	"",
	"Current Team\1",
	"Guillaume Duhamel",
	"Normmatt",
	"Riccardo Magliocchetti",
	"CrazyMax",
	"zeromus",
	"rogerman",
	"Luigi__",
	"",
	"Contributors\1",
	"Bernat Muñoz (shash)",
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
				SetBkMode(hdcMem, TRANSPARENT);
				SetTextAlign(hdcMem, TA_CENTER);
				u32 x = gRc.right / 2;
				FillRect(hdcMem, &gRc, (HBRUSH)COLOR_WINDOW);
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
			char buf[256] = {0};
			memset(&buf[0], 0, sizeof(buf));
			sprintf(buf, "DeSmuME%s", EMU_DESMUME_VERSION_STRING());
			SetDlgItemText(dialog, IDC_TXT_VERSION, buf);
			sprintf(buf, "compiled %s - %s %s", __DATE__, __TIME__, EMU_DESMUME_COMPILER_DETAIL());
			SetDlgItemText(dialog, IDC_TXT_COMPILED, buf);
			
			gList = GetDlgItem(dialog, IDC_AUTHORS_LIST);
			SetWindowLongPtr(gList, GWLP_WNDPROC, (LONG_PTR)ListProc);
			GetClientRect(gList, &gRc);
			gPosY = gRc.bottom;

			SetTimer(dialog, ABOUT_TIMER_ID, 20, (TIMERPROC) NULL);
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
