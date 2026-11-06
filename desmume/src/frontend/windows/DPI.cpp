/*
	Copyright (C) 2006-2026 DeSmuME team

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

#include "DPI.h"

#include <windows.h>
#include <commctrl.h>

// The per-monitor entry points were added over several OS releases, and the
// project still builds against whatever SDK happens to be installed, so none of
// them are linked directly. Resolving them at runtime keeps the binary usable on
// older systems and independent of _WIN32_WINNT.

typedef UINT (WINAPI *PFN_GetDpiForWindow)(HWND hwnd);
typedef UINT (WINAPI *PFN_GetDpiForSystem)(void);
typedef HRESULT (WINAPI *PFN_GetDpiForMonitor)(HMONITOR monitor, int dpiType, UINT *dpiX, UINT *dpiY);
typedef BOOL (WINAPI *PFN_SetProcessDpiAwarenessContext)(HANDLE context);
typedef HRESULT (WINAPI *PFN_SetProcessDpiAwareness)(int awareness);
typedef int (WINAPI *PFN_GetSystemMetricsForDpi)(int index, UINT dpi);
typedef BOOL (WINAPI *PFN_AdjustWindowRectExForDpi)(LPRECT rect, DWORD style, BOOL menu, DWORD exStyle, UINT dpi);
typedef BOOL (WINAPI *PFN_SystemParametersInfoForDpi)(UINT action, UINT param, PVOID data, UINT winIni, UINT dpi);

#define DPI_AWARENESS_CONTEXT_PMV2_VALUE  ((HANDLE)(LONG_PTR)-4)
#define DPI_PROCESS_PER_MONITOR_AWARE     2
#define DPI_MDT_EFFECTIVE_DPI             0

static PFN_GetDpiForWindow              pGetDpiForWindow = NULL;
static PFN_GetDpiForSystem              pGetDpiForSystem = NULL;
static PFN_GetDpiForMonitor             pGetDpiForMonitor = NULL;
static PFN_SetProcessDpiAwarenessContext pSetProcessDpiAwarenessContext = NULL;
static PFN_SetProcessDpiAwareness       pSetProcessDpiAwareness = NULL;
static PFN_GetSystemMetricsForDpi       pGetSystemMetricsForDpi = NULL;
static PFN_AdjustWindowRectExForDpi     pAdjustWindowRectExForDpi = NULL;
static PFN_SystemParametersInfoForDpi   pSystemParametersInfoForDpi = NULL;

static void DPI_ResolveEntryPoints()
{
	static bool resolved = false;
	if (resolved)
	{
		return;
	}
	resolved = true;

	HMODULE user32 = GetModuleHandleA("user32.dll");
	if (user32 == NULL)
	{
		user32 = LoadLibraryA("user32.dll");
	}

	if (user32 != NULL)
	{
		pGetDpiForWindow = (PFN_GetDpiForWindow)GetProcAddress(user32, "GetDpiForWindow");
		pGetDpiForSystem = (PFN_GetDpiForSystem)GetProcAddress(user32, "GetDpiForSystem");
		pSetProcessDpiAwarenessContext = (PFN_SetProcessDpiAwarenessContext)GetProcAddress(user32, "SetProcessDpiAwarenessContext");
		pGetSystemMetricsForDpi = (PFN_GetSystemMetricsForDpi)GetProcAddress(user32, "GetSystemMetricsForDpi");
		pAdjustWindowRectExForDpi = (PFN_AdjustWindowRectExForDpi)GetProcAddress(user32, "AdjustWindowRectExForDpi");
		pSystemParametersInfoForDpi = (PFN_SystemParametersInfoForDpi)GetProcAddress(user32, "SystemParametersInfoForDpi");
	}

	// shcore.dll only exists from Windows 8.1 on; a failure here simply means
	// the older fallbacks are used.
	HMODULE shcore = LoadLibraryA("shcore.dll");
	if (shcore != NULL)
	{
		pGetDpiForMonitor = (PFN_GetDpiForMonitor)GetProcAddress(shcore, "GetDpiForMonitor");
		pSetProcessDpiAwareness = (PFN_SetProcessDpiAwareness)GetProcAddress(shcore, "SetProcessDpiAwareness");
	}
}

void DPI_EnablePerMonitorAwareness()
{
	DPI_ResolveEntryPoints();

	// Once the manifest (or an earlier call) has set the awareness, every call
	// below fails without changing anything, so trying them in order is safe.
	if ( (pSetProcessDpiAwarenessContext != NULL) &&
	     pSetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PMV2_VALUE) )
	{
		return;
	}

	if ( (pSetProcessDpiAwareness != NULL) &&
	     SUCCEEDED(pSetProcessDpiAwareness(DPI_PROCESS_PER_MONITOR_AWARE)) )
	{
		return;
	}

	SetProcessDPIAware();
}

int DPI_GetSystem()
{
	DPI_ResolveEntryPoints();

	if (pGetDpiForSystem != NULL)
	{
		const UINT dpi = pGetDpiForSystem();
		if (dpi != 0)
		{
			return (int)dpi;
		}
	}

	HDC dc = GetDC(NULL);
	if (dc == NULL)
	{
		return 96;
	}

	const int dpi = GetDeviceCaps(dc, LOGPIXELSX);
	ReleaseDC(NULL, dc);

	return (dpi > 0) ? dpi : 96;
}

static int DPI_GetForMonitor(HMONITOR monitor)
{
	DPI_ResolveEntryPoints();

	if ( (monitor != NULL) && (pGetDpiForMonitor != NULL) )
	{
		UINT dpiX = 0;
		UINT dpiY = 0;
		if ( SUCCEEDED(pGetDpiForMonitor(monitor, DPI_MDT_EFFECTIVE_DPI, &dpiX, &dpiY)) && (dpiX != 0) )
		{
			return (int)dpiX;
		}
	}

	return DPI_GetSystem();
}

int DPI_GetForWindow(HWND hwnd)
{
	if (hwnd == NULL)
	{
		return DPI_GetSystem();
	}

	DPI_ResolveEntryPoints();

	if (pGetDpiForWindow != NULL)
	{
		const UINT dpi = pGetDpiForWindow(hwnd);
		if (dpi != 0)
		{
			return (int)dpi;
		}
	}

	return DPI_GetForMonitor(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST));
}

int DPI_GetForPoint(int x, int y)
{
	const POINT pt = { x, y };
	return DPI_GetForMonitor(MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST));
}

int DPI_Scale(int value, int dpi)
{
	if ( (dpi <= 0) || (dpi == 96) )
	{
		return value;
	}

	return MulDiv(value, dpi, 96);
}

int DPI_Unscale(int value, int dpi)
{
	if ( (dpi <= 0) || (dpi == 96) )
	{
		return value;
	}

	return MulDiv(value, 96, dpi);
}

int DPI_ScaleForWindow(int value, HWND hwnd)
{
	return DPI_Scale(value, DPI_GetForWindow(hwnd));
}

int DPI_UnscaleForWindow(int value, HWND hwnd)
{
	return DPI_Unscale(value, DPI_GetForWindow(hwnd));
}

DPI_ClientScale DPI_GetClientScale(HWND hwnd, int designWidth, int designHeight)
{
	DPI_ClientScale scale = { 1.0f, 1.0f, 1.0f };

	RECT client;
	if ( (hwnd == NULL) || (designWidth <= 0) || (designHeight <= 0) || !GetClientRect(hwnd, &client) )
	{
		return scale;
	}

	if (client.right > 0)
	{
		scale.x = (float)client.right / (float)designWidth;
	}

	if (client.bottom > 0)
	{
		scale.y = (float)client.bottom / (float)designHeight;
	}

	scale.uniform = (scale.x < scale.y) ? scale.x : scale.y;
	return scale;
}

int DPI_ListColumnWidth(HWND list, const char *title, int width96)
{
	const int dpi = DPI_GetForWindow(list);
	int width = DPI_Scale(width96, dpi);

	// An empty title (the check box column) never has to fit anything.
	if ( (title == NULL) || (title[0] == '\0') )
	{
		return width;
	}

	HDC dc = GetDC(list);
	if (dc == NULL)
	{
		return width;
	}

	// Measure with the font the list really uses.  With the modern UI font
	// switched on this is not the template font, and the two do not have the
	// same width for the same string.
	HFONT font = (HFONT)SendMessageA(list, WM_GETFONT, 0, 0);
	HGDIOBJ old = (font != NULL) ? SelectObject(dc, font) : NULL;

	SIZE text = { 0, 0 };
	if ( GetTextExtentPoint32A(dc, title, lstrlenA(title), &text) )
	{
		// Header cells pad the text on both sides and the sort indicator needs
		// its room too, even though none of these lists sorts on click.
		const int needed = (int)text.cx + DPI_Scale(14, dpi);
		if (width < needed)
		{
			width = needed;
		}
	}

	if (old != NULL)
	{
		SelectObject(dc, old);
	}
	ReleaseDC(list, dc);

	return width;
}

static BOOL CALLBACK DPI_RescaleListColumnsProc(HWND child, LPARAM param)
{
	const int *dpi = (const int *)param;

	char className[32] = { 0 };
	if ( (GetClassNameA(child, className, (int)sizeof(className)) == 0) ||
	     (lstrcmpiA(className, "SysListView32") != 0) )
	{
		return TRUE;
	}

	const HWND header = (HWND)SendMessageW(child, LVM_GETHEADER, 0, 0);
	if (header == NULL)
	{
		return TRUE;
	}

	const int count = (int)SendMessageW(header, HDM_GETITEMCOUNT, 0, 0);
	for (int i = 0; i < count; i++)
	{
		const int width = (int)SendMessageW(child, LVM_GETCOLUMNWIDTH, (WPARAM)i, 0);
		if (width > 0)
		{
			SendMessageW(child, LVM_SETCOLUMNWIDTH, (WPARAM)i,
			             MAKELPARAM(MulDiv(width, dpi[1], dpi[0]), 0));
		}
	}

	return TRUE;
}

void DPI_RescaleListColumns(HWND dialog, int fromDpi, int toDpi)
{
	if ( (dialog == NULL) || (fromDpi <= 0) || (toDpi <= 0) || (fromDpi == toDpi) )
	{
		return;
	}

	const int dpi[2] = { fromDpi, toDpi };
	EnumChildWindows(dialog, DPI_RescaleListColumnsProc, (LPARAM)&dpi);
}

int DPI_GetSystemMetricsForWindow(int index, HWND hwnd)
{
	DPI_ResolveEntryPoints();

	if (pGetSystemMetricsForDpi != NULL)
	{
		return pGetSystemMetricsForDpi(index, (UINT)DPI_GetForWindow(hwnd));
	}

	return GetSystemMetrics(index);
}

BOOL DPI_AdjustWindowRectEx(RECT *rect, DWORD style, BOOL menu, DWORD exStyle, HWND hwnd)
{
	DPI_ResolveEntryPoints();

	if (pAdjustWindowRectExForDpi != NULL)
	{
		return pAdjustWindowRectExForDpi(rect, style, menu, exStyle, (UINT)DPI_GetForWindow(hwnd));
	}

	return AdjustWindowRectEx(rect, style, menu, exStyle);
}

BOOL DPI_SystemParametersInfoForDpi(UINT action, UINT param, void *data, UINT winIni, UINT dpi)
{
	DPI_ResolveEntryPoints();

	if (pSystemParametersInfoForDpi == NULL)
	{
		return FALSE;
	}

	return pSystemParametersInfoForDpi(action, param, data, winIni, dpi);
}
