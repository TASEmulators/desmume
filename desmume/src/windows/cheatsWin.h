/*
	Copyright (C) 2009-2011 DeSmuME team

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

extern void CheatsListDialog(HWND hwnd);
extern void CheatsSearchDialog(HWND hwnd);
extern void CheatsSearchReset();
extern void CheatAddVerify(HWND dialog,char* addre, char* valu,u8 size);
extern void CheatsAddDialog(HWND parentHwnd, u32 address, u32 value, u8 size, const char* description=0);
