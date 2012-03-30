/*
	Copyright (C) 2009 DeSmuME team

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

#ifndef __GBASLOT_CONFIG_H_
#define __GBASLOT_CONFIG_H_
#include "../common.h"
#include <string>

extern std::string win32_CFlash_cfgDirectory, win32_CFlash_cfgFileName;
extern UINT win32_CFlash_cfgMode;

extern void GBAslotDialog(HWND hwnd);

#endif
