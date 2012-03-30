/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2009 DeSmuME team

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

#include "../common.h"

extern BOOL CALLBACK ViewDisasm_ARM7Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK ViewDisasm_ARM7BoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

extern BOOL CALLBACK ViewDisasm_ARM9Proc (HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);
extern LRESULT CALLBACK ViewDisasm_ARM9BoxProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

template<int proc> void FORCEINLINE DisassemblerTools_Refresh();

#endif
