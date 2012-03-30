/*
	Copyright (C) 2007 Acid Burn
	Copyright (C) 2008-2011 DeSmuME team

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

#include "lightView.h"
#include "commctrl.h"
#include "colorctrl.h"
#include "gfx3d.h"
#include "resource.h"
#include "debug.h"
#include "main.h"

// Convert B5G5R5 color format into R8G8B8 color format
unsigned int ColorConv_B5R5R5ToR8G8B8(const unsigned int color)
{
	return	(((color&31)<<16)<<3)		|	// red
			((((color>>5)&31)<<8)<<3)	|	// green
			(((color>>10)&31)<<3);			// blue
}

typedef struct
{
	u32	autoup_secs;
	bool autoup;
} lightsview_struct;

lightsview_struct	*LightsView = NULL;

void LightView_OnPaintLight(HWND hwnd, int index)
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
	//ColorCtrl*		colorCtrl;
	char			buffer[128];

	// Get necessary information from gfx3d module
	gfx3d_glGetLightColor(index, &color);
	gfx3d_glGetLightDirection(index, &direction);

	// Print Light Direction
	sprintf(buffer, "%.8x", direction);
	SetWindowText(GetDlgItem(hwnd, idcDir[index]), buffer);

	// Print Light Color
	sprintf(buffer, "%.4x", color);
	SetWindowText(GetDlgItem(hwnd, idcColorEdit[index]), buffer);

	// Set Light Color in ColorDisplay component
	ColorCtrl_SetColor(GetDlgItem(hwnd, idcColorCtrl[index]), ColorConv_B5R5R5ToR8G8B8(color));
}

//////////////////////////////////////////////////////////////////////////////

BOOL LightView_OnPaint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
    HDC          hdc;
    PAINTSTRUCT  ps;

    hdc = BeginPaint(hwnd, &ps);
    
	LightView_OnPaintLight(hwnd, 0);
	LightView_OnPaintLight(hwnd, 1);
	LightView_OnPaintLight(hwnd, 2);
	LightView_OnPaintLight(hwnd, 3);

    EndPaint(hwnd, &ps);

    return TRUE;
}

BOOL CALLBACK ViewLightsProc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
				LightsView = new lightsview_struct;
				memset(LightsView, 0, sizeof(lightsview_struct));
				LightsView->autoup_secs = 1;
				SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETRANGE, 0, MAKELONG(99, 1));
				SendMessage(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN),
									UDM_SETPOS32, 0, LightsView->autoup_secs);
			 break;

		case WM_CLOSE:
				if(LightsView->autoup)
				{
					KillTimer(hwnd, IDT_VIEW_LIGHTS);
					LightsView->autoup = false;
				}
				delete LightsView;
				LightsView = NULL;
			//INFO("Close lights viewer dialog\n");
			PostQuitMessage(0);
			break;

		case WM_PAINT:
			 LightView_OnPaint(hwnd, wParam, lParam);
			 break;

		case WM_TIMER:
				SendMessage(hwnd, WM_COMMAND, IDC_REFRESH, 0);
				return 1;

		case WM_COMMAND:
			 switch (LOWORD (wParam))
			 {
					case IDOK:
						SendMessage(hwnd, WM_CLOSE, 0, 0);
						 return 1;
					case IDC_AUTO_UPDATE :
						 if(LightsView->autoup)
                         {
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), false);
							 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), false);
							 KillTimer(hwnd, IDT_VIEW_LIGHTS);
                              LightsView->autoup = FALSE;
                              return 1;
                         }
						 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SECS), true);
						 EnableWindow(GetDlgItem(hwnd, IDC_AUTO_UPDATE_SPIN), true);
                         LightsView->autoup = TRUE;
						 SetTimer(hwnd, IDT_VIEW_LIGHTS, LightsView->autoup_secs*20, (TIMERPROC) NULL);
						 return 1;
					case IDC_AUTO_UPDATE_SECS:
						{
							int t = GetDlgItemInt(hwnd, IDC_AUTO_UPDATE_SECS, FALSE, TRUE);
							if (!LightsView) 
							{
								SendMessage(hwnd, WM_INITDIALOG, 0, 0);
							}
							if (t != LightsView->autoup_secs)
							{
								LightsView->autoup_secs = t;
								if (LightsView->autoup)
									SetTimer(hwnd, IDT_VIEW_LIGHTS, 
											LightsView->autoup_secs*20, (TIMERPROC) NULL);
							}
						}
                         return 1;
					case IDC_REFRESH:
						InvalidateRect(hwnd, NULL, FALSE);
						return 1;
			 }
			 return 0;
	}
	return FALSE;
}
