/*
	Copyright (C) 2007 Acid Burn
	Copyright (C) 2008-2010 DeSmuME team

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
#include <stdio.h>
#include "colorctrl.h"

static char szClassName[] = "DeSmuME_ColorCtrl";

LRESULT CALLBACK ColorCtrl_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
LRESULT ColorCtrl_OnPaint(ColorCtrl *ccp, WPARAM wParam, LPARAM lParam);
LRESULT ColorCtrl_OnNCCreate(HWND hWnd, WPARAM wParam, LPARAM lParam);
LRESULT ColorCtrl_OnNCDestroy(ColorCtrl *ccp, WPARAM wParam, LPARAM lParam);

static ColorCtrl* ColorCtrl_Get(HWND hWnd)
{
	return (ColorCtrl*)GetWindowLong(hWnd, 0);
}

void ColorCtrl_Register()
{
	WNDCLASSEX wc;

	wc.cbSize         = sizeof(wc);
	wc.lpszClassName  = szClassName;
	wc.hInstance      = GetModuleHandle(0);
	wc.lpfnWndProc    = ColorCtrl_WndProc;
	wc.hCursor        = LoadCursor (NULL, IDC_ARROW);
	wc.hIcon          = 0;
	wc.lpszMenuName   = 0;
	wc.hbrBackground  = (HBRUSH)GetSysColorBrush(COLOR_BTNFACE);
	wc.style          = 0;
	wc.cbClsExtra     = 0;
	wc.cbWndExtra     = sizeof(ColorCtrl*);
	wc.hIconSm        = 0;

	RegisterClassEx(&wc);
}

HWND ColorCtrl_Create(HWND hParent)
{
    HWND hwndCtrl;

    hwndCtrl = CreateWindowEx(
                 WS_EX_CLIENTEDGE, // give it a standard border
                 szClassName,
                 NULL,
                 WS_VISIBLE | WS_CHILD,
                 0, 0, 16, 16,
                 hParent,
                 NULL, GetModuleHandle(0), NULL
               );

    return hwndCtrl;
}

LRESULT CALLBACK ColorCtrl_WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    ColorCtrl *pCtrl = ColorCtrl_Get(hwnd);

    switch(msg)
    {
    case WM_NCCREATE:
        return ColorCtrl_OnNCCreate(hwnd, wParam, lParam);

    case WM_NCDESTROY:
		ColorCtrl_OnNCDestroy(pCtrl, wParam, lParam);
		break;

	case WM_PAINT:
		return ColorCtrl_OnPaint(pCtrl, wParam, lParam);

	case WM_ERASEBKGND:
		return 1;

    default:
        break;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

LRESULT ColorCtrl_OnNCCreate(HWND hWnd, WPARAM wParam, LPARAM lParam)
{
	// Allocate a new CustCtrl structure for this window.
	ColorCtrl* pCtrl = (ColorCtrl*)malloc(sizeof(ColorCtrl));

	// Failed to allocate, stop window creation.
	if(pCtrl == NULL) 
		return FALSE;

	// Initialize the CustCtrl structure. 
	pCtrl->hWnd = hWnd;
	pCtrl->color = 0;

	// Attach custom structure to this window.
	SetWindowLong(hWnd, 0, (LONG)pCtrl);

	// Continue with window creation.
	return TRUE;
}

LRESULT ColorCtrl_OnNCDestroy(ColorCtrl *pCtrl, WPARAM wParam, LPARAM lParam)
{
	free(pCtrl);
	return TRUE;
}

LRESULT ColorCtrl_OnPaint(ColorCtrl *pCtrl, WPARAM wParam, LPARAM lParam)
{
    HDC			hdc;
    PAINTSTRUCT	ps;
    RECT		rect;
	HBRUSH		brush;

    // Get a device context for this window
    hdc = BeginPaint(pCtrl->hWnd, &ps);

    // Work out where to draw
    GetClientRect(pCtrl->hWnd, &rect);

	// Create brush and fill
	brush = CreateSolidBrush(pCtrl->color);
	FillRect(hdc, &rect, brush);

    // Release the device context
    EndPaint(pCtrl->hWnd, &ps);

	// free brush again
	DeleteObject(brush);

    return 0;
}

void ColorCtrl_SetColor(HWND hWnd, COLORREF color)
{
    ColorCtrl *pCtrl = ColorCtrl_Get(hWnd);

	pCtrl->color = color;
	InvalidateRect(hWnd, NULL, FALSE);
}
