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

#ifndef DESMUME_WIN32_DARKMODE_H_INCLUDED
#define DESMUME_WIN32_DARKMODE_H_INCLUDED

#include <windows.h>

// Follow the Windows "app mode" (Settings > Personalization > Colors) for the
// main window chrome: the title bar, the menu bar, the toolbar and the client
// background.  Dialogs and tool windows keep their original light colours for
// now.
//
// The mode is read once at startup, so a system theme change takes effect on
// the next launch.  Every entry point the feature needs is resolved at run
// time; on older Windows, or when they are missing, the frontend simply keeps
// its classic look.
namespace darkMode
{
	// Call once from WinMain, before the first window is created.
	void init();

	bool enabled();

	// The menu bar uses barColor(), the toolbar (and a future status bar) uses
	// panelColor(), and the idle emulated screen / window background uses
	// displayColor().  The toolbar bitmaps have their key colour replaced with
	// panelColor() so no light halo shows around the artwork.
	COLORREF barColor();
	COLORREF panelColor();
	COLORREF panelTextColor();
	COLORREF displayColor();

	// Opaque BGRA8888 value of displayColor(), for the display pipeline's
	// 32bpp framebuffer.  The display converters produce opaque pixels, so
	// the alpha byte has to be set here as well.
	DWORD displayPixel32();

	// A brush with displayColor(), owned by this module and kept for the
	// lifetime of the process: the window class keeps using it for every erase.
	HBRUSH backgroundBrush();

	// Apply the dark title bar, the dark menu bar and the dark class background
	// to a top level window.  Call from that window's WM_CREATE.
	void applyFrame(HWND hwnd);

	// Give a freshly created toolbar its dark colours.  CToolBar recreates its
	// HWND on a DPI change, so this is needed for every incarnation.
	void prepareToolBar(HWND hwnd);

	// Register a bitmap right after CToolBar added it with TB_ADDBITMAP.
	// The index is the one TB_ADDBITMAP returned.  Dark mode keeps a dimmed
	// copy: the themed renderer paints disabled buttons with the light system
	// 3D face colour, so disabled buttons are drawn here instead.
	void addToolBarBitmap(HWND hwnd, int index, HBITMAP bitmap);

	// Handler for the toolbar's NM_CUSTOMDRAW.  Pass the WM_NOTIFY lParam and
	// return the result from the parent window procedure.
	LRESULT toolBarCustomDraw(LPARAM lParam);

	// The menu bar font is built for the window's DPI; call this on a DPI
	// change so the next paint picks up the new size.
	void resetFont();
}

#endif // DESMUME_WIN32_DARKMODE_H_INCLUDED
