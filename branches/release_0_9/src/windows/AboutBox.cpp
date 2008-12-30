/*  
	Copyright (C) 2008 shash

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


#include "../common.h"

#include "AboutBox.h"
#include "resource.h"

#define TEAM	31
const char	*team[TEAM] = { "Original author",
							"---------------",
							"yopyop",
							"",
							"Current team",
							"------------",
							"Guillaume Duhamel",
							"Normmatt",
							"Bernat Muñoz (shash)",
							"thoduv",
							"Tim Seidel (Mighty Max)",
							"Pascal Giard (evilynux)",
							"Ben Jaques (masscat)",
							"Jeff Bland",
							"Andres Delikat",
							"Riccardo Magliocchetti",
							"Max Tabachenko (CrazyMax/mtabachenko)",
							"Zeromus",
							"Luigi__",
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
							"Theo Berkau"};


BOOL CALLBACK AboutBox_Proc (HWND dialog, UINT message,WPARAM wparam,LPARAM lparam)
{
	switch(message)
	{
		case WM_INITDIALOG: 
		{
			char buf[2048];
			memset(buf, 0, sizeof(buf));
			for (int i = 0; i < TEAM; i++)
			{
				strcat(buf,team[i]);
				strcat(buf,"\n");
			}

			SetDlgItemText(dialog, IDC_AUTHORS_LIST, buf);
			break;
		}
	
		case WM_COMMAND:
		{
			if((HIWORD(wparam) == BN_CLICKED)&&(((int)LOWORD(wparam)) == IDC_FERMER))
			{
				EndDialog(dialog,0);
				return 1;
			}
			break;
		}
	}
	return 0;
}
