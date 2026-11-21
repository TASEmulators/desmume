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

#ifndef UIFONT_H
#define UIFONT_H

#include <windows.h>

// The face Windows itself uses, instead of the 8 point bitmap fonts the dialog
// templates in resources.rc were drawn with in 2005.
//
// The face comes from NONCLIENTMETRICS.lfMessageFont, so it follows the system
// language: "Segoe UI" on an English install, "Microsoft YaHei UI" on a Chinese
// one, and whatever the user selected in the display settings.  It is asked for
// with SystemParametersInfoForDpi(), so it also follows the scaling of the
// monitor the window opens on.
//
// The templates lay their controls out in dialog units derived from that 8
// point line, so the face is used at the largest size that still fits the line
// height the template was drawn for; one pixel of descender is tolerated.
// Dialog units, and with them every control position, stay untouched.
//
// Dialogs are found with a thread local WH_CALLWNDPROC hook: the dialog manager
// sends WM_INITDIALOG to the dialog procedure, the hook sees the message before
// the procedure runs and gives the dialog and its children the new font.  The
// tool windows open on a thread of their own, so every thread that creates a
// dialog calls init() first.  Each of those dialogs is remembered, which is how
// switching the option reaches the windows that are already open.
namespace uiFont
{
	// Installs the hook for the calling thread.  Idempotent, safe to call on
	// every thread, and must run before the first dialog of that thread.
	void init();

	// Config > Use Modern UI Font.  Turning it off brings the bitmap faces back
	// for the disassembly view; sizes and positions are untouched either way.
	// Dialogs that are already open are switched on the spot.
	void setModern(bool enabled);
	bool modern();

	// The system message font, at the DPI of the given window.  Never NULL after
	// init().
	HFONT message(HWND window);

	// Fixed pitch TrueType face for machine generated text (disassembly, memory,
	// IO registers), sized to the font the given window already uses so the text
	// lines up with the labels around it.  Falls back to the stock bitmap face
	// while modern fonts are off.  The returned font is cached; do not delete it.
	HFONT mono(HWND window);

	// The best installed fixed pitch face, for callers that build their own font
	// with CreateFont() at a size of their choosing.  Never NULL.
	const char *monoFace();

	// Sends WM_SETFONT to the dialog and all of its children.  The hook already
	// does this for templated dialogs; call it directly for dialogs that are
	// created without a template.  The dialog is remembered, so a later
	// setModern() reaches it too.
	void applyToDialog(HWND dialog);

	// The DPI the dialog was last laid out at.
	UINT layoutDpi(HWND dialog);

	// The DPI of the monitor the window is on right now.  This is the live value,
	// so it already reports the new monitor while WM_DPICHANGED is still being
	// dispatched; layoutDpi() only catches up afterwards.
	UINT dpiOf(HWND window);

	// Scales every child by the ratio of the two DPIs.  Used by dialogs that
	// handle WM_DPICHANGED themselves, and by the hook for the rest.
	void scaleDialog(HWND dialog, UINT fromDpi, UINT toDpi);

	// Rebuilds the fonts for the new DPI, rescales nothing.
	void onDpiChanged(HWND dialog, UINT oldDpi, UINT newDpi);

	// Tells the font hook that this dialog's WM_DPICHANGED is handled by the
	// dialog itself, so the hook must not scale it a second time.
	void markResizable(HWND dialog);
}

#endif // UIFONT_H
