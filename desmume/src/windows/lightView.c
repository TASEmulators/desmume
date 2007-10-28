/*  Copyright (C) 2007 Acid Burn

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
#include <stdio.h>
#include "lightView.h"
#include "resource.h"
#include "matrix.h"
#include "render3d.h"
#include "armcpu.h"
#include "colorctrl.h"
#include "colorconv.h"


//////////////////////////////////////////////////////////////////////////////

BOOL LightView_OnInitDialog(HWND hwnd)
{
	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

BOOL LightView_OnClose(lightview_struct* win)
{
	win->window.autoup = FALSE;
	CWindow_RemoveFromRefreshList(win);
	LightView_Deinit(win);
	EndDialog(win->window.hwnd, 0);

	return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

void LightView_OnPaintLight(lightview_struct* win, int index)
{
	const int idcDir[4] =
	{
		IDC_LIGHT_VIEWER_LIGHT0VECTOR_EDIT, IDC_LIGHT_VIEWER_LIGHT1VECTOR_EDIT,
		IDC_LIGHT_VIEWER_LIGHT2VECTOR_EDIT, IDC_LIGHT_VIEWER_LIGHT3VECTOR_EDIT
	};

	const int idcColorEdit[4] =
	{
		IDC_LIGHT_VIEWER_LIGHT0COLOR_EDIT, IDC_LIGHT_VIEWER_LIGHT1COLOR_EDIT,
		IDC_LIGHT_VIEWER_LIGHT2COLOR_EDIT, IDC_LIGHT_VIEWER_LIGHT3COLOR_EDIT
	};

	const int idcColorCtrl[4] =
	{
		IDC_LIGHT_VIEWER_LIGHT0COLOR_COLORCTRL, IDC_LIGHT_VIEWER_LIGHT1COLOR_COLORCTRL,
		IDC_LIGHT_VIEWER_LIGHT2COLOR_COLORCTRL, IDC_LIGHT_VIEWER_LIGHT3COLOR_COLORCTRL
	};

	unsigned int	color;
	unsigned int	direction;
	ColorCtrl*		colorCtrl;
	char			buffer[128];

	// Get necessary information from render module
	gpu3D->NDS_glGetLightColor(index, &color);
	gpu3D->NDS_glGetLightDirection(index, &direction);

	// Print Light Direction
	sprintf(buffer, "%.8x", direction);
	SetWindowText(GetDlgItem(win->window.hwnd, idcDir[index]), buffer);

	// Print Light Color
	sprintf(buffer, "%.4x", color);
	SetWindowText(GetDlgItem(win->window.hwnd, idcColorEdit[index]), buffer);

	// Set Light Color in ColorDisplay component
	ColorCtrl_SetColor(GetDlgItem(win->window.hwnd, idcColorCtrl[index]), ColorConv_B5R5R5ToR8G8B8(color));
}

//////////////////////////////////////////////////////////////////////////////

BOOL LightView_OnPaint(lightview_struct* win, HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    hdc = BeginPaint(hwnd, &ps);
    
	LightView_OnPaintLight(win, 0);
	LightView_OnPaintLight(win, 1);
	LightView_OnPaintLight(win, 2);
	LightView_OnPaintLight(win, 3);

    EndPaint(hwnd, &ps);

    return TRUE;
}

//////////////////////////////////////////////////////////////////////////////

BOOL CALLBACK LightView_Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
     lightview_struct *win = (lightview_struct *)GetWindowLong(hwnd, DWL_USER);

     switch (message)
     {
            case WM_INITDIALOG:
                 LightView_OnInitDialog(hwnd);
				 break;

            case WM_CLOSE:
                 LightView_OnClose(win);
				 break;

            case WM_PAINT:
                 LightView_OnPaint(win, hwnd, wParam, lParam);
				 break;

            case WM_COMMAND:
                 switch (LOWORD (wParam))
                 {
                        case IDOK:
                             CWindow_RemoveFromRefreshList(win);
                             LightView_Deinit(win);
                             EndDialog(hwnd, 0);
                             return 1;
                 }
                 return 0;

			case WM_SYSCOMMAND:
                 switch (LOWORD (wParam))
                 {
                        case IDC_AUTO_UPDATE:
                             CWindow_ToggleAutoUpdate(win);
                             return 1;
                 }
                 return 0;
     }
     return 0;    
}

//////////////////////////////////////////////////////////////////////////////

lightview_struct *LightView_Init(HINSTANCE hInst, HWND parent)
{
   lightview_struct *LightView=NULL;

   if ((LightView = (lightview_struct *)malloc(sizeof(lightview_struct))) == NULL)
      return NULL;

   if (CWindow_Init2(LightView, hInst, parent, "Light Viewer", IDD_LIGHT_VIEWER, LightView_Proc) != 0)
   {
      free(LightView);
      return NULL;
   }

   return LightView;
}

//////////////////////////////////////////////////////////////////////////////

void LightView_Deinit(lightview_struct *LightView)
{
   if (LightView)
      free(LightView);
}

//////////////////////////////////////////////////////////////////////////////
