/*
	Copyright (C) 2006 yopyop
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

#ifndef DISVIEW_H
#define DISVIEW_H

#include <windows.h>
#include <stdio.h>
#include <types.h>

// One dialog now covers both CPUs; the ARM9/ARM7 selection is a combo box.
// Everything below it (decoding, address navigation, breakpoints, registers)
// lives in ../shared/disassembly_core.cpp, which the Cocoa and GTK3 frontends
// use as well, so this file is UI only.
extern BOOL CALLBACK ViewDisasmProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK ViewDisasmBoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Posted by the core's breakpoint handler. wParam is the CPU
// (NDS_DEBUG_CPU_ARM9/ARM7), lParam the instruction address. Posting from the
// CPU loop keeps the emulation thread from touching the dialog directly.
#define WM_DISVIEW_DEBUG_BREAK (WM_APP + 0x51)

// Safe to call from the emulation thread and when the window is closed.
void DisView_NotifyDebugBreak(int cpu, u32 address);

// The instruction list is a custom child window, not a dialog, so the wheel
// messages the main window receives have to be routed to it by hand. Returns
// NULL when the window is closed.
HWND DisView_GetListHwnd(void);

#endif
