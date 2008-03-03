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

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <mmsystem.h>
#include <COMMDLG.H>
#include <string.h>

#include "CWindow.h"
#include "AboutBox.h"
#include "resource.h"


BOOL CALLBACK AboutBox_Proc (HWND dialog, UINT message,WPARAM wparam,LPARAM lparam)
{
	switch(message)
	{
		case WM_INITDIALOG: 
		{
			SetDlgItemText(dialog, IDC_AUTHORS_LIST, "Original author\n---------------\nyopyop\n\nCurrent team\n------------\nAllustar\namponzi\nape\ndelfare\nGuillaume Duhamel\nNormmatt\nRomain Vallet\nshash\nTheo Berkau\nthoduv\nTim Seidel (Mighty Max)\nDamien Nozay (damdoum)\nPascal Giard (evilynux)\nBen Jaques (masscat)\nJeff Bland\n\nContributors\n------------\nAnthony Molinaro\nsnkmad");
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