/*
	Copyright (C) 2008-2017 DeSmuME team

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

#include <stdio.h>
#include <zlib.h>
#include "types.h"
#include "ImageOut.h"
#include "formats/rpng.h"
#include "formats/rbmp.h"
#include "GPU.h"

static u8* Convert15To24(const u16* src, int width, int height)
{
	u8 *tmp_buffer;
	u8 *tmp_inc;
	tmp_inc = tmp_buffer = (u8 *)malloc(width * height * 3);

	for (int i = 0; i < width*height; i++)
	{
		u32 dst = ColorspaceConvert555To8888Opaque<true>(*src++);
		*tmp_inc++ = dst & 0xFF;
		*tmp_inc++ = (dst >> 8) & 0xFF;
		*tmp_inc++ = (dst >> 16) & 0xFF;
	}

	return tmp_buffer;
}

int NDS_WritePNG_15bpp(int width, int height, const u16 *data, const char *filename)
{
	u8* tmp = Convert15To24(data,width,height);
	bool ok = rpng_save_image_bgr24(filename,tmp,width,height,width*3);
	free(tmp);
	return ok?1:0;
}

int NDS_WriteBMP_15bpp(int width, int height, const u16 *data, const char *filename)
{
	u8* tmp = Convert15To24(data,width,height);
	bool ok = rbmp_save_image(filename,tmp,width,height,width*3,RBMP_SOURCE_TYPE_BGR24); 
	free(tmp);
	return ok?1:0;
}

int NDS_WritePNG_32bppBuffer(int width, int height, const void* buf, const char *filename)
{
	bool ok = rpng_save_image_argb(filename, (const uint32_t*)buf, width, height, width * 4);
	return ok ? 1 : 0;
}

int NDS_WriteBMP_32bppBuffer(int width, int height, const void* buf, const char *filename)
{
	bool ok = rbmp_save_image(filename,buf,width,height,width*4,(rbmp_source_type)(RBMP_SOURCE_TYPE_ARGB8888 | RBMP_SOURCE_TYPE_YFLIPPED));
	return ok?1:0;
}