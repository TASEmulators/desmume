/*  AboutBox.cpp

    Copyright (C) 2008-2009 shash
	Copyright (C) 2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include "../common.h"
#include "version.h"

#include "AboutBox.h"
#include "resource.h"

#define ABOUT_TIMER_ID 110222
#define PER_PAGE_TEAM 23
#define SIZE_SCROLL_BUFFER PER_PAGE_TEAM + TEAM
const char	*team[] = { 
	"Current Team",
	"------------",
	"Guillaume Duhamel",
	"Normmatt",
	"Bernat Muñoz (shash)",
	"Riccardo Magliocchetti",
	"Max Tabachenko (CrazyMax)",
	"zeromus",
	"Luigi__",
	"adelikat",
	"matusz",
	"pa__",
	"gocha",
	"nitsuja",
	"",
	"Contributors",
	"------------",
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
	"",
	"Honorary Nagmasters",
	"(Thanks to our super testers for this release!)",
	"------------",
	"nash679",
	"pokefan999",
	"dottorleo",
	"",
	"average time from bug checkin to bugreport:",
	"23 seconds",
};

const int TEAM = ARRAY_SIZE(team);

u8	scroll_start;
u8	scroll_buffer[SIZE_SCROLL_BUFFER][255];

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

			for (int i = 0; i < SIZE_SCROLL_BUFFER; i++)
				strcpy((char *)scroll_buffer[i], "\n");
			for (int i = 0; i < TEAM; i++)
			{
				strcpy((char *)scroll_buffer[i + PER_PAGE_TEAM], team[i]);
				strcat((char *)scroll_buffer[i + PER_PAGE_TEAM], "\n");
			}
			SetTimer(dialog, ABOUT_TIMER_ID, 400, (TIMERPROC) NULL);
			scroll_start = 1;
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
			char buf[4096];
			memset(buf, 0, sizeof(buf));
			for (int i = 0; i < PER_PAGE_TEAM; i++)
				if(i+scroll_start < SIZE_SCROLL_BUFFER)
					strcat(buf, (char *)scroll_buffer[i + scroll_start]);
			scroll_start++;
			if (scroll_start >= SIZE_SCROLL_BUFFER)
				scroll_start = 0;
			SetDlgItemText(dialog, IDC_AUTHORS_LIST, buf);
			break;
		}
	}
	return 0;
}
