/*
	Copyright (C) 2015-2017 DeSmuME team

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

#ifndef _DESMUME_IMAGEOUT_H_
#define _DESMUME_IMAGEOUT_H_

#include "types.h"

u8* Convert15To24(const u16* src, int width, int height);
u8* Convert32To32SwapRB(const void* src, int width, int height);

int NDS_WritePNG_15bpp(int width, int height, const u16 *data, const char *fname);
int NDS_WriteBMP_15bpp(int width, int height, const u16 *data, const char *filename);
int NDS_WritePNG_32bppBuffer(int width, int height, const void* buf, const char *filename);
int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename);

#endif