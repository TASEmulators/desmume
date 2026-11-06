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

#ifndef DPI_H
#define DPI_H

#include <windows.h>

// Per-monitor DPI helpers.
//
// The frontend keeps all of its layout math in physical pixels: once the
// process is per-monitor DPI aware, the client area, the screen rects and the
// mouse coordinates are all in physical pixels, so the existing code keeps
// working unchanged. What needs adjusting is the point where a *user facing*
// size (a window preset, a saved window size, a toolbar icon) is turned into
// pixels, and the places that ask Windows for metrics or frame sizes.
//
// Everything here degrades to the system DPI when the per-monitor entry points
// are missing, so the helpers can be called unconditionally.

// Opts the process into PerMonitorV2 awareness. The manifest already asks for
// it; this is the fallback for builds that do not embed the manifest.
void DPI_EnablePerMonitorAwareness();

// 96 means "no scaling". DPI_GetForWindow() reads the monitor the window is
// currently on, DPI_GetForPoint() the monitor containing a screen point.
int DPI_GetSystem();
int DPI_GetForWindow(HWND hwnd);
int DPI_GetForPoint(int x, int y);

// Converts between 96-DPI logical units and physical pixels.
int DPI_Scale(int value, int dpi);
int DPI_Unscale(int value, int dpi);
int DPI_ScaleForWindow(int value, HWND hwnd);
int DPI_UnscaleForWindow(int value, HWND hwnd);

// Scale of a dialog's client area against the 96 DPI size its hand painted
// content was drawn for.  The two axes can differ: dialog units come from the
// template font's average character width and line height, which round
// independently at fractional DPIs (an 8 pt template is 12 x 25 px per unit at
// 192 DPI, so the dialog grows 2.0x horizontally but 1.923x vertically).
// `uniform` is the smaller of the two, for content that has to stay square.
struct DPI_ClientScale
{
	float x;
	float y;
	float uniform;
};

DPI_ClientScale DPI_GetClientScale(HWND hwnd, int designWidth, int designHeight);

// DPI-aware replacements for GetSystemMetrics() and AdjustWindowRectEx().
// Both consult the window's monitor instead of the system DPI.
int DPI_GetSystemMetricsForWindow(int index, HWND hwnd);
BOOL DPI_AdjustWindowRectEx(RECT *rect, DWORD style, BOOL menu, DWORD exStyle, HWND hwnd);

// SystemParametersInfo() for a specific DPI, for the metrics tables that come in
// per-DPI flavours (NONCLIENTMETRICS is the one the frontend needs).  Returns
// FALSE when the entry point is missing, in which case the caller uses the plain
// call and scales the result itself.
BOOL DPI_SystemParametersInfoForDpi(UINT action, UINT param, void *data, UINT winIni, UINT dpi);

#endif // DPI_H
