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
