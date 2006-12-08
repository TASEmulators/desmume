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

#include <windows.h>
#include "resource.h"
#include "../NDSSystem.h"
#include "../MMU.h"
#include <stdio.h>
#include "CWindow.h"

//////////////////////////////////////////////////////////////////////////////

LRESULT Ginfo_OnPaint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
        HDC          hdc;
        PAINTSTRUCT  ps;
        TCHAR        text[80];
        NDS_header * header = NDS_getROMHeader();
        
        hdc = BeginPaint(hwnd, &ps);
         
        //sprintf(text, "%08X", MMU::ARM9_MEM_MASK[0x80]);
        // This needs to be done because some games use all 12 bytes for text
        // without a 0x00 termination
        memcpy(text, header->gameTile, 12);
        text[12] = 0x00;
        SetWindowText(GetDlgItem(hwnd, IDC_NOM_JEU), text);
        SetWindowText(GetDlgItem(hwnd, IDC_CDE), header->gameCode);
        sprintf(text, "%d", header->makerCode);
        SetWindowText(GetDlgItem(hwnd, IDC_FAB), text);
        sprintf(text, "%d", header->cardSize);
        SetWindowText(GetDlgItem(hwnd, IDC_TAILLE), text);
        sprintf(text, "%d", (int)header->ARM9binSize);
        SetWindowText(GetDlgItem(hwnd, IDC_ARM9_T), text);
        sprintf(text, "%d", (int)header->ARM7binSize);
        SetWindowText(GetDlgItem(hwnd, IDC_ARM7_T), text);
        sprintf(text, "%d", (int)(header->ARM7binSize + header->ARM7src));
        SetWindowText(GetDlgItem(hwnd, IDC_DATA), text);
        
        EndPaint(hwnd, &ps);

        free(header);

        return 0;
}

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK GinfoView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     switch (message)
     {
            case WM_INITDIALOG :
                 return 1;
            case WM_CLOSE :
                 EndDialog(hwnd, 0);
                 return 1;
            case WM_PAINT:
                 Ginfo_OnPaint(hwnd, wParam, lParam);
                 return 1;
            case WM_COMMAND :
                 switch (LOWORD (wParam))
                 {
                        case IDC_FERMER :
                             EndDialog(hwnd, 0);
                             return 1;
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////
