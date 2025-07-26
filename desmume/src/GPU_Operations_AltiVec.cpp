/*
	Copyright (C) 2025 DeSmuME team

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

/*
	Build Notes:
	- Compiling in GCC 4.2.1 has a bug where using v128s16 or v128u16 as parameters for
	  vec_or() will cause data alignment issues. When using vec_or(), you must typecast
	  any v128s16/v128u16 parameters to v128s8/v128u8 to make vec_or() work correctly.
    - Using a call to vec_ld() as a direct parameter to vec_perm() causes data alignment
      issues when compiled for 64-bit. Instead, call vec_perm() on a separate line after
      vec_ld() so as to not confuse the compiler.
 */

#ifndef ENABLE_ALTIVEC
	#error This code requires PowerPC AltiVec support.
	#warning This error might occur if this file is compiled directly. Do not compile this file directly, as it is already included in GPU_Operations.cpp.
#else

#include "GPU_Operations_AltiVec.h"
#include "./utils/colorspacehandler/colorspacehandler_AltiVec.h"

#define COPYLINE_LOAD_SRC() \
const v128u8 srcVec = vec_ld(0, srcPtr128 + srcX);

#define COPYLINE_WRITE() \
for (size_t lx = 0; lx < (size_t)INTEGERSCALEHINT; lx++)\
{\
	vec_st(srcPixOut[lx], 0, dstPtr128 + dstX + lx);\
}\
\
if (SCALEVERTICAL)\
{\
	for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)\
	{\
		for (size_t lx = 0; lx < (size_t)INTEGERSCALEHINT; lx++)\
		{\
			vec_st(srcPixOut[lx], 0, dstPtr128 + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx);\
		}\
	}\
}\


static const ColorOperation_AltiVec colorop_vec;
static const PixelOperation_AltiVec pixelop_vec;

template <bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void __CopyLineSingleUnlimited(void *__restrict dst, const void *__restrict src, size_t pixCount)
{
	if (NEEDENDIANSWAP && (ELEMENTSIZE > 1))
	{
		const size_t vecLength = ((pixCount * ELEMENTSIZE) & ~(size_t)0x0F) / ELEMENTSIZE;
		size_t i = 0;
		
		if (ELEMENTSIZE == 2)
		{
			for (; i < vecLength; i+=(sizeof(v128u8)/ELEMENTSIZE))
			{
				v128u16 pixSwap16 = vec_ld(0, (u16 *)src + i);
				pixSwap16 = vec_perm( pixSwap16, vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E}) );
				vec_st(pixSwap16, 0, (u16 *)dst + i);
			}
			
#pragma LOOPVECTORIZE_DISABLE
			for (; i < pixCount; i++)
			{
				((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
			}
		}
		else if (ELEMENTSIZE == 4)
		{
			for (; i < vecLength; i+=(sizeof(v128u8)/ELEMENTSIZE))
			{
				v128u32 pixSwap32 = vec_ld(0, (u32 *)src + i);
				pixSwap32 = vec_perm( pixSwap32, vec_splat_u8(0), ((v128u8){0x03,0x02,0x01,0x00,  0x07,0x06,0x05,0x04,  0x0B,0x0A,0x09,0x08,  0x0F,0x0E,0x0D,0x0C}) );
				vec_st(pixSwap32, 0, (u32 *)dst + i);
			}
			
#pragma LOOPVECTORIZE_DISABLE
			for (; i < pixCount; i++)
			{
				((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
			}
		}
	}
	else
	{
		memcpy(dst, src, pixCount * ELEMENTSIZE);
	}
}

template <bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void __CopyLineSingle(v128u8 *__restrict dstPtr128, const v128u8 *__restrict srcPtr128)
{
	if (NEEDENDIANSWAP && (ELEMENTSIZE > 1))
	{
		if (ELEMENTSIZE == 2)
		{
			MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), vec_st(vec_perm(vec_ld(sizeof(v128u8) * (X), srcPtr128), vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E})), sizeof(v128u8) * (X), dstPtr128) );
		}
		else if (ELEMENTSIZE == 4)
		{
			MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), vec_st(vec_perm(vec_ld(sizeof(v128u8) * (X), srcPtr128), vec_splat_u8(0), ((v128u8){0x03,0x02,0x01,0x00,  0x07,0x06,0x05,0x04,  0x0B,0x0A,0x09,0x08,  0x0F,0x0E,0x0D,0x0C})), sizeof(v128u8) * (X), dstPtr128) );
		}
	}
	else
	{
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), vec_st(vec_ld(sizeof(v128u8) * (X), srcPtr128), sizeof(v128u8) * (X), dstPtr128) );
	}
}

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
{
	v128u8 *__restrict srcPtr128 = (v128u8 *__restrict)src;
	v128u8 *__restrict dstPtr128 = (v128u8 *__restrict)dst;
	
	if (INTEGERSCALEHINT == 0)
	{
		__CopyLineSingleUnlimited<NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, dstWidth);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		__CopyLineSingle<NEEDENDIANSWAP, ELEMENTSIZE>(dstPtr128, srcPtr128);
	}
	else if (INTEGERSCALEHINT == 2)
	{
		v128u8 srcPixOut[2];
		
#if defined(NDEBUG) && defined(PUBLIC_RELEASE)
		v128u8 srcVec;
		
		switch (ELEMENTSIZE)
		{
			case 1:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), srcPtr128); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x01,0x01,  0x02,0x02,0x03,0x03,  0x04,0x04,0x05,0x05,  0x06,0x06,0x07,0x07}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x09,0x09,  0x0A,0x0A,0x0B,0x0B,  0x0C,0x0C,0x0D,0x0D,  0x0E,0x0E,0x0F,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 1), dstPtr128 ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 1), dstPtr128 ); \
					           );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), srcPtr128); \
					           srcPixOut[0] = vec_perm(srcVec, srcVec, dupPattern[0]); \
					           srcPixOut[1] = vec_perm(srcVec, srcVec, dupPattern[1]); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 1), dstPtr128 ); \
					           );
				}
				break;
			}
				
			case 2:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), srcPtr128); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x00,0x01,  0x02,0x03,0x02,0x03,  0x04,0x05,0x04,0x05,  0x06,0x07,0x06,0x07}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x08,0x09,  0x0A,0x0B,0x0A,0x0B,  0x0C,0x0D,0x0C,0x0D,  0x0E,0x0F,0x0E,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 1), dstPtr128 ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 1), dstPtr128 ); \
					           );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), srcPtr128); \
					           srcPixOut[0] = vec_perm(srcVec, srcVec, dupPattern[0]); \
					           srcPixOut[1] = vec_perm(srcVec, srcVec, dupPattern[1]); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 1), dstPtr128 ); \
					           );
				}
				break;
			}
				
			case 4:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), srcPtr128); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 1), dstPtr128 ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 1), dstPtr128 ); \
					           );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), srcPtr128); \
					           srcPixOut[0] = vec_perm(srcVec, srcVec, dupPattern[0]); \
					           srcPixOut[1] = vec_perm(srcVec, srcVec, dupPattern[1]); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 0), dstPtr128 ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 1), dstPtr128 ); \
					           );
				}
				break;
			}
		}
#else
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x01,0x01,  0x02,0x02,0x03,0x03,  0x04,0x04,0x05,0x05,  0x06,0x06,0x07,0x07}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x09,0x09,  0x0A,0x0A,0x0B,0x0B,  0x0C,0x0C,0x0D,0x0D,  0x0E,0x0E,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x00,0x01,  0x02,0x03,0x02,0x03,  0x04,0x05,0x04,0x05,  0x06,0x07,0x06,0x07}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x08,0x09,  0x0A,0x0B,0x0A,0x0B,  0x0C,0x0D,0x0C,0x0D,  0x0E,0x0F,0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
#endif
	}
	else if (INTEGERSCALEHINT == 3)
	{
		v128u8 srcPixOut[3];
		
#if defined(NDEBUG) && defined(PUBLIC_RELEASE)
		v128u8 srcVec;
		
		switch (ELEMENTSIZE)
		{
			case 1:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), (v128u8 *)src); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,  0x01,0x01,0x01,  0x02,0x02,0x02,  0x03,0x03,0x03,  0x04,0x04,0x04,  0x05}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x05,0x05,  0x06,0x06,0x06,  0x07,0x07,0x07,  0x08,0x08,0x08,  0x09,0x09,0x09,  0x0A,0x0A}) ); \
					           srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,  0x0B,0x0B,0x0B,  0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 2), (v128u8 *)dst ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 2), (v128u8 *)dst ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 2), (v128u8 *)dst ); \
					           );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), (v128u8 *)src); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,  0x01,0x01,0x01,  0x02,0x02,0x02,  0x03,0x03,0x03,  0x04,0x04,0x04,  0x05}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x05,0x05,  0x06,0x06,0x06,  0x07,0x07,0x07,  0x08,0x08,0x08,  0x09,0x09,0x09,  0x0A,0x0A}) ); \
					           srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,  0x0B,0x0B,0x0B,  0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 2), (v128u8 *)dst ); \
					           );
				}
				break;
			}
				
			case 2:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), (v128u8 *)src); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x04,0x05,  0x04,0x05}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x0A,0x0B}) ); \
					           srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 2), (v128u8 *)dst ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 2), (v128u8 *)dst ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 2), (v128u8 *)dst ); \
					           );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), (v128u8 *)src); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x04,0x05,  0x04,0x05}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x0A,0x0B}) ); \
					           srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 2), (v128u8 *)dst ); \
					           );
				}
				break;
			}
				
			case 4:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), (v128u8 *)src); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) ); \
					           srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 0) + 2), (v128u8 *)dst ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 1) + 2), (v128u8 *)dst ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(v128u8) / ELEMENTSIZE)) * 2) + 2), (v128u8 *)dst ); \
					           );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
					           srcVec = vec_ld(sizeof(v128u8) * (X), (v128u8 *)src); \
					           srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07}) ); \
					           srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) ); \
					           srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) ); \
					           vec_st( srcPixOut[0], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 0), (v128u8 *)dst ); \
					           vec_st( srcPixOut[1], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 1), (v128u8 *)dst ); \
					           vec_st( srcPixOut[2], sizeof(v128u8) * (((X) * INTEGERSCALEHINT) + 2), (v128u8 *)dst ); \
					           );
				}
				break;
			}
		}
#else
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,  0x01,0x01,0x01,  0x02,0x02,0x02,  0x03,0x03,0x03,  0x04,0x04,0x04,  0x05}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x05,0x05,  0x06,0x06,0x06,  0x07,0x07,0x07,  0x08,0x08,0x08,  0x09,0x09,0x09,  0x0A,0x0A}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,  0x0B,0x0B,0x0B,  0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x04,0x05,  0x04,0x05}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x0A,0x0B}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
#endif
	}
	else if (INTEGERSCALEHINT == 4)
	{
		v128u8 srcPixOut[4];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT == 5)
	{
		v128u8 srcPixOut[5];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,0x02,  0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x03,0x03,0x03,0x03,  0x04,0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,0x05,  0x06,0x06}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x06,0x06,  0x07,0x07,0x07,0x07,0x07,  0x08,0x08,0x08,0x08,0x08,  0x09,0x09,0x09}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x09,0x09,  0x0A,0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B,0x0B,  0x0C,0x0C,0x0C,0x0C}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,  0x0D,0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,  0x02,0x03,0x02,0x03,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,0x02,0x03,  0x04,0x05,0x04,0x05,0x04,0x05,0x04,0x05,0x04,0x05,  0x06,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x07,0x06,0x07,0x06,0x07,0x06,0x07,  0x08,0x09,0x08,0x09,0x08,0x09,0x08,0x09}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,  0x0C,0x0D,0x0C,0x0D}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0C,0x0D,0x0C,0x0D,  0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT == 6)
	{
		v128u8 srcPixOut[6];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x02,  0x03,0x03,0x03,0x03,0x03,0x03,  0x04,0x04,0x04,0x04,0x04,0x04,  0x05,0x05}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07,0x07,0x07}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,  0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,  0x02,0x03,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,0x02,0x03,0x02,0x03,0x02,0x03,  0x04,0x05,0x04,0x05,0x04,0x05,0x04,0x05}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x04,0x05,  0x06,0x07,0x06,0x07,0x06,0x07,0x06,0x07,0x06,0x07,0x06,0x07}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x08,0x09,0x08,0x09,0x08,0x09,0x08,0x09,0x08,0x09,  0x0A,0x0B,0x0A,0x0B}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,  0x0C,0x0D,0x0C,0x0D,0x0C,0x0D,0x0C,0x0D}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0C,0x0D,  0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT == 7)
	{
		v128u8 srcPixOut[7];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,0x01,0x01,0x01,  0x02,0x02}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03,0x03,0x03,0x03,  0x04,0x04,0x04,0x04}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x04,0x04,  0x05,0x05,0x05,0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,0x06,0x06}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x06,  0x07,0x07,0x07,0x07,0x07,0x07,0x07,  0x08,0x08,0x08,0x08,0x08,0x08,0x08,  0x09}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x09,0x09,0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0B,0x0B,0x0B,0x0B,  0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,0x0D}) );
				srcPixOut[6] = vec_perm( srcVec, srcVec, ((v128u8){0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,0x00,0x01,  0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,0x02,0x03,0x02,0x03,0x02,0x03,0x02,0x03,0x02,0x03,  0x04,0x05,0x04,0x05}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x04,0x05,0x04,0x05,0x04,0x05,0x04,0x05,  0x06,0x07,0x06,0x07,0x06,0x07}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x07,0x06,0x07,0x06,0x07,0x06,0x07,  0x08,0x09,0x08,0x09,0x08,0x09,0x08,0x09}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x08,0x09,0x08,0x09,  0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,0x0A,0x0B,0x0A,0x0B}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,0x0A,0x0B,  0x0C,0x0D,0x0C,0x0D,0x0C,0x0D,0x0C,0x0D,0x0C,0x0D,0x0C,0x0D}) );
				srcPixOut[6] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F,0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[6] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT == 8)
	{
		v128u8 srcPixOut[8];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B}) );
				srcPixOut[6] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}) );
				srcPixOut[7] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B}) );
				srcPixOut[6] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D}) );
				srcPixOut[7] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[2] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[3] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[4] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[5] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[6] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[7] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT == 12)
	{
		v128u8 srcPixOut[12];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[ 0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01}) );
				srcPixOut[ 1] = vec_perm( srcVec, srcVec, ((v128u8){0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02}) );
				srcPixOut[ 2] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03}) );
				srcPixOut[ 3] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05}) );
				srcPixOut[ 4] = vec_perm( srcVec, srcVec, ((v128u8){0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06}) );
				srcPixOut[ 5] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}) );
				srcPixOut[ 6] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09}) );
				srcPixOut[ 7] = vec_perm( srcVec, srcVec, ((v128u8){0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A}) );
				srcPixOut[ 8] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B}) );
				srcPixOut[ 9] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D}) );
				srcPixOut[10] = vec_perm( srcVec, srcVec, ((v128u8){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E}) );
				srcPixOut[11] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[ 0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01}) );
				srcPixOut[ 1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03}) );
				srcPixOut[ 2] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03}) );
				srcPixOut[ 3] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05}) );
				srcPixOut[ 4] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07}) );
				srcPixOut[ 5] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07}) );
				srcPixOut[ 6] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09}) );
				srcPixOut[ 7] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B}) );
				srcPixOut[ 8] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B}) );
				srcPixOut[ 9] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D}) );
				srcPixOut[10] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
				srcPixOut[11] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[ 0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 2] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 3] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 4] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 5] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 6] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[ 7] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[ 8] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[ 9] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[10] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[11] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT == 16)
	{
		v128u8 srcPixOut[16];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			COPYLINE_LOAD_SRC()
			
			if (ELEMENTSIZE == 1)
			{
				srcPixOut[ 0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}) );
				srcPixOut[ 1] = vec_perm( srcVec, srcVec, ((v128u8){0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01,0x01}) );
				srcPixOut[ 2] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02,0x02}) );
				srcPixOut[ 3] = vec_perm( srcVec, srcVec, ((v128u8){0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03,0x03}) );
				srcPixOut[ 4] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x04}) );
				srcPixOut[ 5] = vec_perm( srcVec, srcVec, ((v128u8){0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05,0x05}) );
				srcPixOut[ 6] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06,0x06}) );
				srcPixOut[ 7] = vec_perm( srcVec, srcVec, ((v128u8){0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07,0x07}) );
				srcPixOut[ 8] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08,0x08}) );
				srcPixOut[ 9] = vec_perm( srcVec, srcVec, ((v128u8){0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09,0x09}) );
				srcPixOut[10] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A,0x0A}) );
				srcPixOut[11] = vec_perm( srcVec, srcVec, ((v128u8){0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B,0x0B}) );
				srcPixOut[12] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C,0x0C}) );
				srcPixOut[13] = vec_perm( srcVec, srcVec, ((v128u8){0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D,0x0D}) );
				srcPixOut[14] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E,0x0E}) );
				srcPixOut[15] = vec_perm( srcVec, srcVec, ((v128u8){0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F,0x0F}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPixOut[ 0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01}) );
				srcPixOut[ 1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01,  0x00,0x01}) );
				srcPixOut[ 2] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03}) );
				srcPixOut[ 3] = vec_perm( srcVec, srcVec, ((v128u8){0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03,  0x02,0x03}) );
				srcPixOut[ 4] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05}) );
				srcPixOut[ 5] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05,  0x04,0x05}) );
				srcPixOut[ 6] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07}) );
				srcPixOut[ 7] = vec_perm( srcVec, srcVec, ((v128u8){0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07,  0x06,0x07}) );
				srcPixOut[ 8] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09}) );
				srcPixOut[ 9] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09,  0x08,0x09}) );
				srcPixOut[10] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B}) );
				srcPixOut[11] = vec_perm( srcVec, srcVec, ((v128u8){0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B,  0x0A,0x0B}) );
				srcPixOut[12] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D}) );
				srcPixOut[13] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D,  0x0C,0x0D}) );
				srcPixOut[14] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
				srcPixOut[15] = vec_perm( srcVec, srcVec, ((v128u8){0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F,  0x0E,0x0F}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[ 0] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 1] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 2] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 3] = vec_perm( srcVec, srcVec, ((v128u8){0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03,  0x00,0x01,0x02,0x03}) );
				srcPixOut[ 4] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 5] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 6] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 7] = vec_perm( srcVec, srcVec, ((v128u8){0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07,  0x04,0x05,0x06,0x07}) );
				srcPixOut[ 8] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[ 9] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[10] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[11] = vec_perm( srcVec, srcVec, ((v128u8){0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B,  0x08,0x09,0x0A,0x0B}) );
				srcPixOut[12] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[13] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[14] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
				srcPixOut[15] = vec_perm( srcVec, srcVec, ((v128u8){0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F,  0x0C,0x0D,0x0E,0x0F}) );
			}
			
			COPYLINE_WRITE()
		}
	}
	else if (INTEGERSCALEHINT > 1)
	{
		const size_t scale = dstWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=scale)
		{
			COPYLINE_LOAD_SRC()
			v128u8 ssse3idx;
			
			for (size_t lx = 0; lx < scale; lx++)
			{
				if (ELEMENTSIZE == 1)
				{
					ssse3idx = vec_ld( 0, _gpuDstToSrcSSSE3_u8_16e + (lx * sizeof(v128u8)) );
				}
				else if (ELEMENTSIZE == 2)
				{
					ssse3idx = vec_ld( 0, _gpuDstToSrcSSSE3_u16_8e + (lx * sizeof(v128u8)) );
				}
				else if (ELEMENTSIZE == 4)
				{
					ssse3idx = vec_ld( 0, _gpuDstToSrcSSSE3_u32_4e + (lx * sizeof(v128u8)) );
				}
				
				vec_st( vec_perm(srcVec, srcVec, ssse3idx), 0, (v128u8 *)dst + dstX + lx );
			}
		}
		
		if (SCALEVERTICAL)
		{
			CopyLinesForVerticalCount<ELEMENTSIZE>(dst, dstWidth, dstLineCount);
		}
	}
	else
	{
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				if (ELEMENTSIZE == 1)
				{
					( (u8 *)dst)[_gpuDstPitchIndex[x] + p] = ((u8 *)src)[x];
				}
				else if (ELEMENTSIZE == 2)
				{
					((u16 *)dst)[_gpuDstPitchIndex[x] + p] = ((u16 *)src)[x];
				}
				else if (ELEMENTSIZE == 4)
				{
					((u32 *)dst)[_gpuDstPitchIndex[x] + p] = ((u32 *)src)[x];
				}
			}
		}
		
		if (SCALEVERTICAL)
		{
			CopyLinesForVerticalCount<ELEMENTSIZE>(dst, dstWidth, dstLineCount);
		}
	}
}

template <s32 INTEGERSCALEHINT, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineReduce(void *__restrict dst, const void *__restrict src, size_t srcWidth)
{
	v128u8 *__restrict srcPtr128 = (v128u8 *__restrict)src;
	v128u8 *__restrict dstPtr128 = (v128u8 *__restrict)dst;
	
	if (INTEGERSCALEHINT == 0)
	{
		__CopyLineSingleUnlimited<NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, srcWidth);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		__CopyLineSingle<NEEDENDIANSWAP, ELEMENTSIZE>(dstPtr128, srcPtr128);
	}
	else if (INTEGERSCALEHINT == 2)
	{
		v128u8 srcPix[2];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = vec_ld(  0, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			srcPix[1] = vec_ld( 16, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			
			if (ELEMENTSIZE == 1)
			{
				vec_st( vec_pack((v128u16)srcPix[0], (v128u16)srcPix[1]), 0, dstPtr128 + dstX );
			}
			else if (ELEMENTSIZE == 2)
			{
				vec_st( vec_pack((v128u32)srcPix[0], (v128u32)srcPix[1]), 0, dstPtr128 + dstX );
			}
			else if (ELEMENTSIZE == 4)
			{
				vec_st( vec_perm(srcPix[0], srcPix[1], ((v128u8){0x00,0x01,0x02,0x03,  0x08,0x09,0x0A,0x0B,  0x10,0x11,0x12,0x13,  0x18,0x19,0x1A,0x1B})), 0, dstPtr128 + dstX );
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		v128u8 srcPix[3];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = vec_ld(  0, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			srcPix[1] = vec_ld( 16, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			srcPix[2] = vec_ld( 32, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = vec_perm( srcPix[0], srcPix[1], ((v128u8){0x00,0x03,0x06,0x09,  0x0C,0x0F,0x12,0x15,  0x18,0x1B,0x1E,0x1F,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[2], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x11,  0x14,0x17,0x1A,0x1D}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = vec_perm( srcPix[0], srcPix[1], ((v128u8){0x00,0x01,0x06,0x07,  0x0C,0x0D,0x12,0x13,  0x18,0x19,0x1E,0x1F,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[2], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x14,0x15,0x1A,0x1B}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = vec_perm( srcPix[0], srcPix[1], ((v128u8){0x00,0x01,0x02,0x03,  0x0C,0x0D,0x0E,0x0F,  0x18,0x19,0x1A,0x1B,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[2], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x14,0x15,0x16,0x17}) );
			}
			
			vec_st(srcPix[0], 0, dstPtr128 + dstX);
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		v128u8 srcPix[4];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = vec_ld(  0, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			srcPix[1] = vec_ld( 16, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			srcPix[2] = vec_ld( 32, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			srcPix[3] = vec_ld( 48, srcPtr128 + (dstX * INTEGERSCALEHINT) );
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = vec_perm( srcPix[0], srcPix[1], ((v128u8){0x00,0x04,0x08,0x0C,  0x10,0x14,0x18,0x1C,  0x1F,0x1F,0x1F,0x1F,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[2], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x10,0x14,0x18,0x1C,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[3], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x10,0x14,0x18,0x1C}) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = vec_perm( srcPix[0], srcPix[1], ((v128u8){0x00,0x01,0x08,0x09,  0x10,0x11,0x18,0x19,  0x1F,0x1F,0x1F,0x1F,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[2], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x10,0x11,0x18,0x19,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[3], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x10,0x11,0x18,0x19}) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = vec_perm( srcPix[0], srcPix[1], ((v128u8){0x00,0x01,0x02,0x03,  0x11,0x12,0x13,0x14,  0x1F,0x1F,0x1F,0x1F,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[2], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x11,0x12,0x13,0x14,  0x1F,0x1F,0x1F,0x1F}) );
				srcPix[0] = vec_perm( srcPix[0], srcPix[3], ((v128u8){0x00,0x01,0x02,0x03,  0x04,0x05,0x06,0x07,  0x08,0x09,0x0A,0x0B,  0x11,0x12,0x13,0x14}) );
			}
			
			vec_st(srcPix[0], 0, dstPtr128 + dstX);
		}
	}
	else if (INTEGERSCALEHINT > 1)
	{
		const size_t scale = srcWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			if (ELEMENTSIZE == 1)
			{
				((u8 *)dst)[x] = ((u8 *)src)[x * scale];
			}
			else if (ELEMENTSIZE == 2)
			{
				((u16 *)dst)[x] = ((u16 *)src)[x * scale];
			}
			else if (ELEMENTSIZE == 4)
			{
				((u32 *)dst)[x] = ((u32 *)src)[x * scale];
			}
		}
	}
	else
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			if (ELEMENTSIZE == 1)
			{
				( (u8 *)dst)[i] = ( (u8 *)src)[_gpuDstPitchIndex[i]];
			}
			else if (ELEMENTSIZE == 2)
			{
				((u16 *)dst)[i] = ((u16 *)src)[_gpuDstPitchIndex[i]];
			}
			else if (ELEMENTSIZE == 4)
			{
				((u32 *)dst)[i] = ((u32 *)src)[_gpuDstPitchIndex[i]];
			}
		}
	}
}

FORCEINLINE v128u16 ColorOperation_AltiVec::blend(const v128u16 &colA, const v128u16 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const
{
	const v128u16 colorBitMask = ((v128u16){0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F});
	
	v128u16 ra = vec_and(        colA,                     colorBitMask );
	v128u16 ga = vec_and( vec_sr(colA, vec_splat_u16( 5)), colorBitMask );
	v128u16 ba = vec_and( vec_sr(colA, vec_splat_u16(10)), colorBitMask );
	
	v128u16 rb = vec_and(        colB,                     colorBitMask );
	v128u16 gb = vec_and( vec_sr(colB, vec_splat_u16( 5)), colorBitMask );
	v128u16 bb = vec_and( vec_sr(colB, vec_splat_u16(10)), colorBitMask );
	
	ra = vec_add( vec_mulo((v128u8)ra, (v128u8)blendEVA), vec_mulo((v128u8)rb, (v128u8)blendEVB) );
	ga = vec_add( vec_mulo((v128u8)ga, (v128u8)blendEVA), vec_mulo((v128u8)gb, (v128u8)blendEVB) );
	ba = vec_add( vec_mulo((v128u8)ba, (v128u8)blendEVA), vec_mulo((v128u8)bb, (v128u8)blendEVB) );
	
	ra = vec_sr( ra, vec_splat_u16(4) );
	ga = vec_sr( ga, vec_splat_u16(4) );
	ba = vec_sr( ba, vec_splat_u16(4) );
	
	ra = vec_min(ra, colorBitMask);
	ga = vec_min(ga, colorBitMask);
	ba = vec_min(ba, colorBitMask);
	
	return vec_or( (v128u8)ra, (v128u8)vec_or( (v128u8)vec_sl(ga, vec_splat_u16(5)), (v128u8)vec_sl(ba, vec_splat_u16(10)) ) );
}

// Note that if USECONSTANTBLENDVALUESHINT is true, then this method will assume that blendEVA contains identical values
// for each 16-bit vector element, and also that blendEVB contains identical values for each 16-bit vector element. If
// this assumption is broken, then the resulting color will be undefined.
template <NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT>
FORCEINLINE v128u32 ColorOperation_AltiVec::blend(const v128u32 &colA, const v128u32 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const
{
	v128u16 outColorLo;
	v128u16 outColorHi;
	v128u32 outColor;
	
	const v128u16 colALo = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)colA );
	const v128u16 colAHi = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)colA );
	const v128u16 colBLo = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)colB );
	const v128u16 colBHi = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)colB );
	
	if (USECONSTANTBLENDVALUESHINT)
	{
		outColorLo = vec_add( vec_mulo((v128u8)colALo, (v128u8)blendEVA), vec_mulo((v128u8)colBLo, (v128u8)blendEVB) );
		outColorHi = vec_add( vec_mulo((v128u8)colAHi, (v128u8)blendEVA), vec_mulo((v128u8)colBHi, (v128u8)blendEVB) );
	}
	else
	{
		const v128u16 blendALo = (v128u16)vec_mergeh(blendEVA, blendEVA);
		const v128u16 blendAHi = (v128u16)vec_mergel(blendEVA, blendEVA);
		const v128u16 blendBLo = (v128u16)vec_mergeh(blendEVB, blendEVB);
		const v128u16 blendBHi = (v128u16)vec_mergel(blendEVB, blendEVB);
		
		outColorLo = vec_add( vec_mulo((v128u8)colALo, (v128u8)blendALo), vec_mulo((v128u8)colBLo, (v128u8)blendBLo) );
		outColorHi = vec_add( vec_mulo((v128u8)colAHi, (v128u8)blendAHi), vec_mulo((v128u8)colBHi, (v128u8)blendBHi) );
	}
	
	outColorLo = vec_sr( outColorLo, vec_splat_u16(4) );
	outColorHi = vec_sr( outColorHi, vec_splat_u16(4) );
	outColor = (v128u32)vec_packsu(outColorHi, outColorLo);
	
	// When the color format is 888, the vpkuhus instruction will naturally clamp
	// the color component values to 255. However, when the color format is 666, the
	// color component values must be clamped to 63. In this case, we must call vminub
	// to do the clamp.
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{ 
		outColor = vec_min( (v128u8)outColor, ((v128u8){63,63,63,63,63,63,63,63,63,63,63,63,63,63,63,63}) );
	}
	
	const v128u32 colorMask32Swapped = ((v128u32){0xFFFFFF00,0xFFFFFF00,0xFFFFFF00,0xFFFFFF00});
	outColor = vec_and(outColor, colorMask32Swapped);
	
	return outColor;
}

FORCEINLINE v128u16 ColorOperation_AltiVec::blend3D(const v128u32 &colA_Lo, const v128u32 &colA_Hi, const v128u16 &colB) const
{
	// If the color format of B is 555, then the colA_Hi parameter is required.
	// The color format of A is assumed to be RGB666.
	
	const v128u32 colorBitMask32 = ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
	const v128u16 colorBitMask16 = ((v128u16){0x003E,0x003E,0x003E,0x003E,0x003E,0x003E,0x003E,0x003E});
	
	v128u32 aa_lo = vec_and(        colA_Lo,                            colorBitMask32 );
	v128u32 ba_lo = vec_and( vec_sr(colA_Lo,         vec_splat_u32(8)), colorBitMask32 );
	v128u32 ga_lo = vec_and( vec_sr(colA_Lo, ((v128u32){16,16,16,16})), colorBitMask32 );
	v128u32 ra_lo =          vec_sr(colA_Lo, ((v128u32){24,24,24,24}));
	
	v128u32 aa_hi = vec_and(        colA_Hi,                            colorBitMask32 );
	v128u32 ba_hi = vec_and( vec_sr(colA_Hi,         vec_splat_u32(8)), colorBitMask32 );
	v128u32 ga_hi = vec_and( vec_sr(colA_Hi, ((v128u32){16,16,16,16})), colorBitMask32 );
	v128u32 ra_hi =          vec_sr(colA_Hi, ((v128u32){24,24,24,24}));
	
	v128u16 ra = vec_pack(ra_lo, ra_hi);
	v128u16 ga = vec_pack(ga_lo, ga_hi);
	v128u16 ba = vec_pack(ba_lo, ba_hi);
	v128u16 aa = vec_pack(aa_lo, aa_hi);
	
	aa = vec_adds(aa, vec_splat_u16(1));
	v128u16 rb = vec_and( vec_sl(colB, vec_splat_u16(1)), colorBitMask16 );
	v128u16 gb = vec_and( vec_sr(colB, vec_splat_u16(4)), colorBitMask16 );
	v128u16 bb = vec_and( vec_sr(colB, vec_splat_u16(9)), colorBitMask16 );
	v128u16 ab = vec_subs( ((v128u16){32,32,32,32,32,32,32,32}), aa );
	
	ra = vec_add( vec_mulo((v128u8)ra, (v128u8)aa), vec_mulo((v128u8)rb, (v128u8)ab) );
	ga = vec_add( vec_mulo((v128u8)ga, (v128u8)aa), vec_mulo((v128u8)gb, (v128u8)ab) );
	ba = vec_add( vec_mulo((v128u8)ba, (v128u8)aa), vec_mulo((v128u8)bb, (v128u8)ab) );
	
	ra = vec_sr( ra, vec_splat_u16(6) );
	ga = vec_sr( ga, vec_splat_u16(6) );
	ba = vec_sr( ba, vec_splat_u16(6) );
	
	return vec_or( (v128u8)vec_or((v128u8)ra, (v128u8)vec_sl(ga, vec_splat_u16(5))), (v128u8)vec_sl(ba, vec_splat_u16(10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_AltiVec::blend3D(const v128u32 &colA, const v128u32 &colB) const
{
	// If the color format of B is 666 or 888, then the colA_Hi parameter is ignored.
	// The color format of A is assumed to match the color format of B.
	
	v128u16 rgbALo = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)colA );
	v128u16 rgbAHi = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)colA );
	v128u16 rgbBLo = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)colB );
	v128u16 rgbBHi = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)colB );
	
	v128u8  alpha   = vec_perm((v128u8)colA,  vec_splat_u8(0), ((v128u8){0x03,0x03,0x03,0x13,  0x07,0x07,0x07,0x17,  0x0B,0x0B,0x0B,0x1B,  0x0F,0x0F,0x0F,0x1F}) );
	v128u16 alphaLo = (v128u16)vec_mergel( vec_splat_u8(0), alpha );
	v128u16 alphaHi = (v128u16)vec_mergeh( vec_splat_u8(0), alpha );
	alphaLo = vec_add(alphaLo, vec_splat_u16(1));
	alphaHi = vec_add(alphaHi, vec_splat_u16(1));
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		const v128u16 s = ((v128u16){32,32,32,32,32,32,32,32});
		rgbALo = vec_add( vec_mulo((v128u8)rgbALo, (v128u8)alphaLo), vec_mulo((v128u8)rgbBLo, (v128u8)vec_sub(s, alphaLo)) );
		rgbAHi = vec_add( vec_mulo((v128u8)rgbAHi, (v128u8)alphaHi), vec_mulo((v128u8)rgbBHi, (v128u8)vec_sub(s, alphaHi)) );
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		const v128u16 s = ((v128u16){256,256,256,256,256,256,256,256});
		rgbALo = vec_mladd( rgbALo, alphaLo, vec_mladd(rgbBLo, vec_sub(s, alphaLo), vec_splat_u16(0)) );
		rgbAHi = vec_mladd( rgbAHi, alphaHi, vec_mladd(rgbBHi, vec_sub(s, alphaHi), vec_splat_u16(0)) );
	}
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		rgbALo = vec_sr( rgbALo, vec_splat_u16(5) );
		rgbAHi = vec_sr( rgbAHi, vec_splat_u16(5) );
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		rgbALo = vec_sr( rgbALo, vec_splat_u16(8) );
		rgbAHi = vec_sr( rgbAHi, vec_splat_u16(8) );
	}
	
	const v128u32 colorMask32Swapped = ((v128u32){0xFFFFFF00,0xFFFFFF00,0xFFFFFF00,0xFFFFFF00});
	return vec_and( (v128u32)vec_packsu((v128u16)rgbAHi, (v128u16)rgbALo), colorMask32Swapped );
}

FORCEINLINE v128u16 ColorOperation_AltiVec::increase(const v128u16 &col, const v128u16 &blendEVY) const
{
	const v128u16 colorBitMask16 = ((v128u16){0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F});
	v128u16 r = vec_and(        col,                     colorBitMask16 );
	v128u16 g = vec_and( vec_sr(col, vec_splat_u16( 5)), colorBitMask16 );
	v128u16 b = vec_and( vec_sr(col, vec_splat_u16(10)), colorBitMask16 );
	
	const v128u16 s = ((v128u16){31,31,31,31,31,31,31,31});
	r = vec_add( r, vec_sr(vec_mulo((v128u8)vec_sub(s, r), (v128u8)blendEVY), vec_splat_u16(4)) );
	g = vec_add( g, vec_sr(vec_mulo((v128u8)vec_sub(s, g), (v128u8)blendEVY), vec_splat_u16(4)) );
	b = vec_add( b, vec_sr(vec_mulo((v128u8)vec_sub(s, b), (v128u8)blendEVY), vec_splat_u16(4)) );
	
	return vec_or( (v128u8)r, (v128u8)vec_or( (v128u8)vec_sl(g, vec_splat_u16(5)), (v128u8)vec_sl(b, vec_splat_u16(10))) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_AltiVec::increase(const v128u32 &col, const v128u16 &blendEVY) const
{
	v128u16 rgbLoSwapped = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)col );
	v128u16 rgbHiSwapped = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)col );
	
	const v128u16 s = (COLORFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u16){63,63,63,63,63,63,63,63}) : ((v128u16){255,255,255,255,255,255,255,255});
	rgbLoSwapped = vec_add( rgbLoSwapped, vec_sr(vec_mulo((v128u8)vec_sub(s, rgbLoSwapped), (v128u8)blendEVY), vec_splat_u16(4)) );
	rgbHiSwapped = vec_add( rgbHiSwapped, vec_sr(vec_mulo((v128u8)vec_sub(s, rgbHiSwapped), (v128u8)blendEVY), vec_splat_u16(4)) );
	
	const v128u32 colorMask32Swapped = ((v128u32){0xFFFFFF00,0xFFFFFF00,0xFFFFFF00,0xFFFFFF00});
	return vec_and( (v128u32)vec_packsu(rgbHiSwapped, rgbLoSwapped), colorMask32Swapped );
}

FORCEINLINE v128u16 ColorOperation_AltiVec::decrease(const v128u16 &col, const v128u16 &blendEVY) const
{
	const v128u16 colorBitMask16 = ((v128u16){0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F});
	v128u16 r = vec_and(        col,                     colorBitMask16 );
	v128u16 g = vec_and( vec_sr(col, vec_splat_u16( 5)), colorBitMask16 );
	v128u16 b = vec_and( vec_sr(col, vec_splat_u16(10)), colorBitMask16 );
	
	r = vec_sub( r, vec_sr(vec_mulo((v128u8)r, (v128u8)blendEVY), vec_splat_u16(4)) );
	g = vec_sub( g, vec_sr(vec_mulo((v128u8)g, (v128u8)blendEVY), vec_splat_u16(4)) );
	b = vec_sub( b, vec_sr(vec_mulo((v128u8)b, (v128u8)blendEVY), vec_splat_u16(4)) );
	
	return vec_or( (v128u8)r, (v128u8)vec_or( (v128u8)vec_sl(g, vec_splat_u16(5)), (v128u8)vec_sl(b, vec_splat_u16(10))) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_AltiVec::decrease(const v128u32 &col, const v128u16 &blendEVY) const
{
	v128u16 rgbLoSwapped = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)col );
	v128u16 rgbHiSwapped = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)col );
	
	rgbLoSwapped = vec_sub( rgbLoSwapped, vec_sr(vec_mulo((v128u8)rgbLoSwapped, (v128u8)blendEVY), vec_splat_u16(4)) );
	rgbHiSwapped = vec_sub( rgbHiSwapped, vec_sr(vec_mulo((v128u8)rgbHiSwapped, (v128u8)blendEVY), vec_splat_u16(4)) );
	
	const v128u32 colorMask32Swapped = ((v128u32){0xFFFFFF00,0xFFFFFF00,0xFFFFFF00,0xFFFFFF00});
	return vec_and( (v128u32)vec_pack(rgbHiSwapped, rgbLoSwapped), colorMask32Swapped );
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AltiVec::_copy16(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2] = {
			vec_or((v128u8)src0, (v128u8)alphaBits),
			vec_or((v128u8)src1, (v128u8)alphaBits)
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo6665Opaque_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo6665Opaque_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555xTo8888Opaque_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo8888Opaque_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		
		vec_st(src32[1],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(src32[0], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(src32[3], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(src32[2], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	if (!ISDEBUGRENDER)
	{
		vec_st(srcLayerID, 0, compInfo.target.lineLayerID);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AltiVec::_copy32(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2] = {
			vec_or( (v128u8)ColorspaceConvert6665To5551_AltiVec<false>(src0, src1), (v128u8)alphaBits ),
			vec_or( (v128u8)ColorspaceConvert6665To5551_AltiVec<false>(src2, src3), (v128u8)alphaBits )
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4] = {
			vec_or(src0, alphaBits),
			vec_or(src1, alphaBits),
			vec_or(src2, alphaBits),
			vec_or(src3, alphaBits)
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	if (!ISDEBUGRENDER)
	{
		vec_st(srcLayerID, 0, compInfo.target.lineLayerID);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AltiVec::_copyMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		const v128u16 passMask16[2]	= {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_sel(dst16[0], vec_or((v128u8)src0, (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)src1, (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo6665Opaque_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo6665Opaque_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555xTo8888Opaque_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo8888Opaque_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], src32[1], passMask32[0]),
			vec_sel(dst32[1], src32[0], passMask32[1]),
			vec_sel(dst32[2], src32[3], passMask32[2]),
			vec_sel(dst32[3], src32[2], passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	if (!ISDEBUGRENDER)
	{
		const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
		vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
	} 
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AltiVec::_copyMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_AltiVec<false>(src0, src1),
			ColorspaceConvert6665To5551_AltiVec<false>(src2, src3)
		};
		
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		const v128u16 passMask16[2]	= {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_sel(dst16[0], vec_or((v128u8)src16[0], (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)src16[1], (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], vec_or(src0, alphaBits), passMask32[0]),
			vec_sel(dst32[1], vec_or(src1, alphaBits), passMask32[1]),
			vec_sel(dst32[2], vec_or(src2, alphaBits), passMask32[2]),
			vec_sel(dst32[3], vec_or(src3, alphaBits), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	if (!ISDEBUGRENDER)
	{
		const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
		vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
	}
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessUp16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_or( (v128u8)colorop_vec.increase(src0, evy16), (v128u8)alphaBits ),
			vec_or( (v128u8)colorop_vec.increase(src1, evy16), (v128u8)alphaBits )
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[1], evy16), alphaBits),
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[0], evy16), alphaBits),
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[3], evy16), alphaBits),
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[2], evy16), alphaBits)
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	vec_st(srcLayerID, 0, compInfo.target.lineLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessUp32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_AltiVec<false>(src0, src1),
			ColorspaceConvert6665To5551_AltiVec<false>(src2, src3)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_or( (v128u8)colorop_vec.increase(src16[0], evy16), (v128u8)alphaBits ),
			vec_or( (v128u8)colorop_vec.increase(src16[1], evy16), (v128u8)alphaBits )
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits),
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits),
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits),
			vec_or(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits)
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	vec_st(srcLayerID, 0, compInfo.target.lineLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessUpMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		const v128u16 passMask16[2]	= {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_sel(dst16[0], vec_or((v128u8)colorop_vec.increase(src0, evy16), (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)colorop_vec.increase(src1, evy16), (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[1], evy16), alphaBits), passMask32[0]),
			vec_sel(dst32[1], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[0], evy16), alphaBits), passMask32[1]),
			vec_sel(dst32[2], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[3], evy16), alphaBits), passMask32[2]),
			vec_sel(dst32[3], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src32[2], evy16), alphaBits), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
	vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessUpMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_AltiVec<false>(src0, src1),
			ColorspaceConvert6665To5551_AltiVec<false>(src2, src3)
		};
		
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		const v128u16 passMask16[2]	= {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_sel(dst16[0], vec_or((v128u8)colorop_vec.increase(src16[0], evy16), (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)colorop_vec.increase(src16[1], evy16), (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits), passMask32[0]),
			vec_sel(dst32[1], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits), passMask32[1]),
			vec_sel(dst32[2], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits), passMask32[2]),
			vec_sel(dst32[3], vec_or(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
	vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessDown16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]	= {
			vec_or( (v128u8)colorop_vec.decrease(src0, evy16), (v128u8)alphaBits ),
			vec_or( (v128u8)colorop_vec.decrease(src1, evy16), (v128u8)alphaBits )
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src0, src32[1], src32[0]);
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src1, src32[3], src32[2]);
		}
		else
		{
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src0, src32[1], src32[0]);
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src1, src32[3], src32[2]);
		}
		
		const v128u32 alphaBitsSwapped = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[0], evy16), alphaBitsSwapped),
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[1], evy16), alphaBitsSwapped),
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[2], evy16), alphaBitsSwapped),
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[3], evy16), alphaBitsSwapped)
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	vec_st(srcLayerID, 0, compInfo.target.lineLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessDown32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_AltiVec<false>(src0, src1),
			ColorspaceConvert6665To5551_AltiVec<false>(src2, src3)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]  = {
			vec_or( (v128u8)colorop_vec.decrease(src16[0], evy16), (v128u8)alphaBits ),
			vec_or( (v128u8)colorop_vec.decrease(src16[1], evy16), (v128u8)alphaBits )
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]  = {
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits),
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits),
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits),
			vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits)
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	vec_st(srcLayerID, 0, compInfo.target.lineLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessDownMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		const v128u16 passMask16[2]	= {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]  = {
			vec_sel(dst16[0], vec_or((v128u8)colorop_vec.decrease(src0, evy16), (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)colorop_vec.decrease(src1, evy16), (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src0, src32[0], src32[1]);
			ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src1, src32[2], src32[3]);
		}
		
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[1], evy16), alphaBits), passMask32[0]),
			vec_sel(dst32[1], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[0], evy16), alphaBits), passMask32[1]),
			vec_sel(dst32[2], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[3], evy16), alphaBits), passMask32[2]),
			vec_sel(dst32[3], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src32[2], evy16), alphaBits), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
	vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AltiVec::_brightnessDownMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_AltiVec<false>(src0, src1),
			ColorspaceConvert6665To5551_AltiVec<false>(src2, src3)
		};
		
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		const v128u16 passMask16[2]	= {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]  = {
			vec_sel(dst16[0], vec_or((v128u8)colorop_vec.decrease(src16[0], evy16), (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)colorop_vec.decrease(src16[1], evy16), (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]  = {
			vec_sel(dst32[0], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits), passMask32[0]),
			vec_sel(dst32[1], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits), passMask32[1]),
			vec_sel(dst32[2], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits), passMask32[2]),
			vec_sel(dst32[3], vec_or(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
	
	const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
	vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_AltiVec::_unknownEffectMask16(GPUEngineCompositorInfo &compInfo,
														   const v128u8 &passMask8,
														   const v128u16 &evy16,
														   const v128u8 &srcLayerID,
														   const v128u16 &src1, const v128u16 &src0,
														   const v128u8 &srcEffectEnableMask,
														   const v128u8 &dstBlendEnableMaskLUT,
														   const v128u8 &enableColorEffectMask,
														   const v128u8 &spriteAlpha,
														   const v128u8 &spriteMode) const
{
	const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
	vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
	
	v128u8 dstTargetBlendEnableMask = vec_perm(dstBlendEnableMaskLUT, dstBlendEnableMaskLUT, dstLayerID);
	dstTargetBlendEnableMask = vec_andc( dstTargetBlendEnableMask, vec_cmpeq(dstLayerID, srcLayerID) );
	
	v128u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : (v128u8)vec_splat_u8(0);
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	CACHE_ALIGN const u8  a8  = compInfo.renderState.blendEVA;
	CACHE_ALIGN const u8  b8  = compInfo.renderState.blendEVB;
	CACHE_ALIGN const u16 a16 = compInfo.renderState.blendEVA;
	CACHE_ALIGN const u16 b16 = compInfo.renderState.blendEVB;
	v128u8 eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? (v128u8)vec_splat( vec_lde(0, &a8), 0 ) : (v128u8)vec_splat( vec_lde(0, &a16), 0 );
	v128u8 evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? (v128u8)vec_splat( vec_lde(0, &b8), 0 ) : (v128u8)vec_splat( vec_lde(0, &b16), 0 );
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v128u8 isObjTranslucentMask = vec_and( dstTargetBlendEnableMask, vec_or(vec_cmpeq(spriteMode, vec_splat_u8(OBJMode_Transparent)), vec_cmpeq(spriteMode, vec_splat_u8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v128u8 spriteAlphaMask = vec_andc( isObjTranslucentMask, vec_cmpeq(spriteAlpha, vec_splat_u8((s8)0xFF)) );
		eva_vec128 = vec_sel(eva_vec128, spriteAlpha, spriteAlphaMask);
		evb_vec128 = vec_sel(evb_vec128, vec_sub(((v128u8){16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}), spriteAlpha), spriteAlphaMask);
	}
	
	// Select the color effect based on the BLDCNT target flags.
	CACHE_ALIGN const u8 ce = compInfo.renderState.colorEffect;
	const v128u8 ce_vec = vec_splat( vec_lde(0, &ce), 0 );
	const v128u8 colorEffect_vec128 = vec_sel(vec_splat_u8(ColorEffect_Disable), ce_vec, enableColorEffectMask);
	
	// ----------
	
	v128u16 tmpSrc16[2];
	v128u32 tmpSrc32[4];
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc16[0] = src0;
		tmpSrc16[1] = src1;
	}
	else if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
	{
		ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src0, tmpSrc32[1], tmpSrc32[0]);
		ColorspaceConvert555xTo666x_AltiVec<false, BESwapDst>(src1, tmpSrc32[3], tmpSrc32[2]);
	}
	else
	{
		ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src0, tmpSrc32[1], tmpSrc32[0]);
		ColorspaceConvert555xTo888x_AltiVec<false, BESwapDst>(src1, tmpSrc32[3], tmpSrc32[2]);
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v128u8 brightnessMask8 = vec_andc( vec_and(srcEffectEnableMask, vec_cmpeq(colorEffect_vec128, vec_splat_u8(ColorEffect_IncreaseBrightness))), forceDstTargetBlendMask );
			const int brightnessUpMaskValue = vec_any_ne(brightnessMask8, vec_splat_u8(0));
			
			if (brightnessUpMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const v128u16 brightnessMask16[2] = {
						(v128u16)vec_mergeh(brightnessMask8, brightnessMask8),
						(v128u16)vec_mergel(brightnessMask8, brightnessMask8)
					};
					
					tmpSrc16[0] = vec_sel( tmpSrc16[0], colorop_vec.increase(tmpSrc16[0], evy16), brightnessMask16[0] );
					tmpSrc16[1] = vec_sel( tmpSrc16[1], colorop_vec.increase(tmpSrc16[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
					};
					
					tmpSrc32[0] = vec_sel( tmpSrc32[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[0], evy16), brightnessMask32[0] );
					tmpSrc32[1] = vec_sel( tmpSrc32[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[1], evy16), brightnessMask32[1] );
					tmpSrc32[2] = vec_sel( tmpSrc32[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[2], evy16), brightnessMask32[2] );
					tmpSrc32[3] = vec_sel( tmpSrc32[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v128u8 brightnessMask8 = vec_andc( vec_and(srcEffectEnableMask, vec_cmpeq(colorEffect_vec128, vec_splat_u8(ColorEffect_DecreaseBrightness))), forceDstTargetBlendMask );
			const int brightnessDownMaskValue = vec_any_ne(brightnessMask8, vec_splat_u8(0));
			
			if (brightnessDownMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const v128u16 brightnessMask16[2] = {
						(v128u16)vec_mergeh(brightnessMask8, brightnessMask8),
						(v128u16)vec_mergel(brightnessMask8, brightnessMask8)
					};
					
					tmpSrc16[0] = vec_sel( tmpSrc16[0], colorop_vec.decrease(tmpSrc16[0], evy16), brightnessMask16[0] );
					tmpSrc16[1] = vec_sel( tmpSrc16[1], colorop_vec.decrease(tmpSrc16[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
					};
					
					tmpSrc32[0] = vec_sel( tmpSrc32[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[0], evy16), brightnessMask32[0] );
					tmpSrc32[1] = vec_sel( tmpSrc32[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[1], evy16), brightnessMask32[1] );
					tmpSrc32[2] = vec_sel( tmpSrc32[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[2], evy16), brightnessMask32[2] );
					tmpSrc32[3] = vec_sel( tmpSrc32[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v128u8 blendMask8 = vec_or( forceDstTargetBlendMask, vec_and(vec_and(srcEffectEnableMask, dstTargetBlendEnableMask), vec_cmpeq(colorEffect_vec128, vec_splat_u8(ColorEffect_Blend))) );
	const int blendMaskValue = vec_any_ne(blendMask8, vec_splat_u8(0));
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		if (blendMaskValue != 0)
		{
			v128u16 blendSrc16[2];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					//blendSrc16[0] = colorop_vec.blend3D(src0, src1, dst16[0]);
					//blendSrc16[1] = colorop_vec.blend3D(src2, src3, dst16[1]);
					printf("GPU: 3D layers cannot be in RGBA5551 format. To composite a 3D layer, use the _unknownEffectMask32() method instead.\n");
					assert(false);
					break;
					
				case GPULayerType_BG:
					blendSrc16[0] = colorop_vec.blend(tmpSrc16[0], dst16[0], eva_vec128, evb_vec128);
					blendSrc16[1] = colorop_vec.blend(tmpSrc16[1], dst16[1], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const v128u16 tempEVA[2] = {
						(v128u16)vec_mergel( vec_splat_u8(0), eva_vec128 ),
						(v128u16)vec_mergeh( vec_splat_u8(0), eva_vec128 )
					};
					const v128u16 tempEVB[2] = {
						(v128u16)vec_mergel( vec_splat_u8(0), evb_vec128 ),
						(v128u16)vec_mergeh( vec_splat_u8(0), evb_vec128 )
					};
					
					blendSrc16[0] = colorop_vec.blend(tmpSrc16[0], dst16[0], tempEVA[0], tempEVB[0]);
					blendSrc16[1] = colorop_vec.blend(tmpSrc16[1], dst16[1], tempEVA[1], tempEVB[1]);
					break;
				}
			}
			
			const v128u16 blendMask16[2] = {
				(v128u16)vec_mergeh(blendMask8, blendMask8),
				(v128u16)vec_mergel(blendMask8, blendMask8)
			};
			
			tmpSrc16[0] = vec_sel(tmpSrc16[0], blendSrc16[0], blendMask16[0]);
			tmpSrc16[1] = vec_sel(tmpSrc16[1], blendSrc16[1], blendMask16[1]);
		}
		
		// Store the final colors.
		const v128u16 passMask16[2] = {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]  = {
			vec_sel(dst16[0], vec_or((v128u8)tmpSrc16[0], (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)tmpSrc16[1], (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		if (blendMaskValue != 0x00000000)
		{
			v128u32 blendSrc32[4];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					//blendSrc32[0] = colorop_vec.blend3D<OUTPUTFORMAT>(src0, dst32[0]);
					//blendSrc32[1] = colorop_vec.blend3D<OUTPUTFORMAT>(src1, dst32[1]);
					//blendSrc32[2] = colorop_vec.blend3D<OUTPUTFORMAT>(src2, dst32[2]);
					//blendSrc32[3] = colorop_vec.blend3D<OUTPUTFORMAT>(src3, dst32[3]);
					printf("GPU: 3D layers cannot be in RGBA5551 format. To composite a 3D layer, use the _unknownEffectMask32() method instead.\n");
					assert(false);
					break;
					
				case GPULayerType_BG:
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[0], dst32[0], eva_vec128, evb_vec128);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[1], dst32[1], eva_vec128, evb_vec128);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[2], dst32[2], eva_vec128, evb_vec128);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[3], dst32[3], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					v128u16 tempBlendLo = (v128u16)vec_mergeh(eva_vec128, eva_vec128);
					v128u16 tempBlendHi = (v128u16)vec_mergel(eva_vec128, eva_vec128);
					
					const v128u16 tempEVA[4] = {
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendHi ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendHi )
					};
					
					tempBlendLo = (v128u16)vec_mergeh(evb_vec128, evb_vec128);
					tempBlendHi = (v128u16)vec_mergel(evb_vec128, evb_vec128);
					
					const v128u16 tempEVB[4] = {
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendHi ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendHi )
					};
					
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[0], dst32[0], tempEVA[0], tempEVB[0]);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[1], dst32[1], tempEVA[1], tempEVB[1]);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[2], dst32[2], tempEVA[2], tempEVB[2]);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[3], dst32[3], tempEVA[3], tempEVB[3]);
					break;
				}
			}
			
			const v128u32 blendMask32[4] = {
				vec_perm( blendMask8, blendMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
				vec_perm( blendMask8, blendMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
				vec_perm( blendMask8, blendMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
				vec_perm( blendMask8, blendMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
			};
			
			tmpSrc32[0] = vec_sel(tmpSrc32[0], blendSrc32[0], blendMask32[0]);
			tmpSrc32[1] = vec_sel(tmpSrc32[1], blendSrc32[1], blendMask32[1]);
			tmpSrc32[2] = vec_sel(tmpSrc32[2], blendSrc32[2], blendMask32[2]);
			tmpSrc32[3] = vec_sel(tmpSrc32[3], blendSrc32[3], blendMask32[3]);
		}
		
		// Store the final colors.
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBitsSwapped = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], vec_or(tmpSrc32[0], alphaBitsSwapped), passMask32[0]),
			vec_sel(dst32[1], vec_or(tmpSrc32[1], alphaBitsSwapped), passMask32[1]),
			vec_sel(dst32[2], vec_or(tmpSrc32[2], alphaBitsSwapped), passMask32[2]),
			vec_sel(dst32[3], vec_or(tmpSrc32[3], alphaBitsSwapped), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_AltiVec::_unknownEffectMask32(GPUEngineCompositorInfo &compInfo,
														   const v128u8 &passMask8,
														   const v128u16 &evy16,
														   const v128u8 &srcLayerID,
														   const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0,
														   const v128u8 &srcEffectEnableMask,
														   const v128u8 &dstBlendEnableMaskLUT,
														   const v128u8 &enableColorEffectMask,
														   const v128u8 &spriteAlpha,
														   const v128u8 &spriteMode) const
{
	const v128u8 dstLayerID = vec_ld(0, compInfo.target.lineLayerID);
	vec_st( vec_sel(dstLayerID, srcLayerID, passMask8), 0, compInfo.target.lineLayerID );
	
	v128u8 dstTargetBlendEnableMask = vec_perm(dstBlendEnableMaskLUT, dstBlendEnableMaskLUT, dstLayerID);
	dstTargetBlendEnableMask = vec_andc( dstTargetBlendEnableMask, vec_cmpeq(dstLayerID, srcLayerID) );
	
	v128u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : (v128u8)vec_splat_u8(0);
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	CACHE_ALIGN const u8  a8  = compInfo.renderState.blendEVA;
	CACHE_ALIGN const u8  b8  = compInfo.renderState.blendEVB;
	CACHE_ALIGN const u16 a16 = compInfo.renderState.blendEVA;
	CACHE_ALIGN const u16 b16 = compInfo.renderState.blendEVB;
	v128u8 eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? (v128u8)vec_splat( vec_lde(0, &a8), 0 ) : (v128u8)vec_splat( vec_lde(0, &a16), 0 );
	v128u8 evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? (v128u8)vec_splat( vec_lde(0, &b8), 0 ) : (v128u8)vec_splat( vec_lde(0, &b16), 0 );
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v128u8 isObjTranslucentMask = vec_and( dstTargetBlendEnableMask, vec_or(vec_cmpeq(spriteMode, vec_splat_u8(OBJMode_Transparent)), vec_cmpeq(spriteMode, vec_splat_u8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v128u8 spriteAlphaMask = vec_andc( isObjTranslucentMask, vec_cmpeq(spriteAlpha, vec_splat_u8((s8)0xFF)) );
		eva_vec128 = vec_sel(eva_vec128, spriteAlpha, spriteAlphaMask);
		evb_vec128 = vec_sel(evb_vec128, vec_sub(((v128u8){16,16,16,16,16,16,16,16,16,16,16,16,16,16,16,16}), spriteAlpha), spriteAlphaMask);
	}
	
	// Select the color effect based on the BLDCNT target flags.
	CACHE_ALIGN const u8 ce = compInfo.renderState.colorEffect;
	const v128u8 ce_vec = vec_splat( vec_lde(0, &ce), 0 );
	const v128u8 colorEffect_vec128 = vec_sel( vec_splat_u8(ColorEffect_Disable), ce_vec, enableColorEffectMask );
	
	// ----------
	
	v128u16 tmpSrc16[2];
	v128u32 tmpSrc32[4];
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc16[0] = ColorspaceConvert6665To5551_AltiVec<false>(src0, src1);
		tmpSrc16[1] = ColorspaceConvert6665To5551_AltiVec<false>(src2, src3);
	}
	else
	{
		tmpSrc32[0] = src0;
		tmpSrc32[1] = src1;
		tmpSrc32[2] = src2;
		tmpSrc32[3] = src3;
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v128u8 brightnessMask8 = vec_andc( vec_and(srcEffectEnableMask, vec_cmpeq(colorEffect_vec128, vec_splat_u8(ColorEffect_IncreaseBrightness))), forceDstTargetBlendMask );
			const int brightnessUpMaskValue = vec_any_ne(brightnessMask8, vec_splat_u8(0));
			
			if (brightnessUpMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const v128u16 brightnessMask16[2] = {
						(v128u16)vec_mergeh(brightnessMask8, brightnessMask8),
						(v128u16)vec_mergel(brightnessMask8, brightnessMask8)
					};
					
					tmpSrc16[0] = vec_sel( tmpSrc16[0], colorop_vec.increase(tmpSrc16[0], evy16), brightnessMask16[0] );
					tmpSrc16[1] = vec_sel( tmpSrc16[1], colorop_vec.increase(tmpSrc16[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
					};
					
					tmpSrc32[0] = vec_sel( tmpSrc32[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[0], evy16), brightnessMask32[0] );
					tmpSrc32[1] = vec_sel( tmpSrc32[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[1], evy16), brightnessMask32[1] );
					tmpSrc32[2] = vec_sel( tmpSrc32[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[2], evy16), brightnessMask32[2] );
					tmpSrc32[3] = vec_sel( tmpSrc32[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v128u8 brightnessMask8 = vec_andc( vec_and(srcEffectEnableMask, vec_cmpeq(colorEffect_vec128, vec_splat_u8(ColorEffect_DecreaseBrightness))), forceDstTargetBlendMask );
			const int brightnessDownMaskValue = vec_any_ne(brightnessMask8, vec_splat_u8(0));
			
			if (brightnessDownMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const v128u16 brightnessMask16[2] = {
						(v128u16)vec_mergeh(brightnessMask8, brightnessMask8),
						(v128u16)vec_mergel(brightnessMask8, brightnessMask8)
					};
					
					tmpSrc16[0] = vec_sel( tmpSrc16[0], colorop_vec.decrease(tmpSrc16[0], evy16), brightnessMask16[0] );
					tmpSrc16[1] = vec_sel( tmpSrc16[1], colorop_vec.decrease(tmpSrc16[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
						vec_perm( brightnessMask8, brightnessMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
					};
					
					tmpSrc32[0] = vec_sel( tmpSrc32[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[0], evy16), brightnessMask32[0] );
					tmpSrc32[1] = vec_sel( tmpSrc32[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[1], evy16), brightnessMask32[1] );
					tmpSrc32[2] = vec_sel( tmpSrc32[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[2], evy16), brightnessMask32[2] );
					tmpSrc32[3] = vec_sel( tmpSrc32[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v128u8 blendMask8 = vec_or( forceDstTargetBlendMask, vec_and(vec_and(srcEffectEnableMask, dstTargetBlendEnableMask), vec_cmpeq(colorEffect_vec128, vec_splat_u8(ColorEffect_Blend))) );
	const int blendMaskValue = vec_any_ne(blendMask8, vec_splat_u8(0));
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			vec_ld( 0, (u16 *)compInfo.target.lineColor16),
			vec_ld(16, (u16 *)compInfo.target.lineColor16)
		};
		
		if (blendMaskValue != 0)
		{
			v128u16 blendSrc16[2];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc16[0] = colorop_vec.blend3D(src0, src1, dst16[0]);
					blendSrc16[1] = colorop_vec.blend3D(src2, src3, dst16[1]);
					break;
					
				case GPULayerType_BG:
					blendSrc16[0] = colorop_vec.blend(tmpSrc16[0], dst16[0], eva_vec128, evb_vec128);
					blendSrc16[1] = colorop_vec.blend(tmpSrc16[1], dst16[1], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const v128u16 tempEVA[2] = {
						(v128u16)vec_mergel( vec_splat_u8(0), eva_vec128 ),
						(v128u16)vec_mergeh( vec_splat_u8(0), eva_vec128 )
					};
					const v128u16 tempEVB[2] = {
						(v128u16)vec_mergel( vec_splat_u8(0), evb_vec128 ),
						(v128u16)vec_mergeh( vec_splat_u8(0), evb_vec128 )
					};
					
					blendSrc16[0] = colorop_vec.blend(tmpSrc16[0], dst16[0], tempEVA[0], tempEVB[0]);
					blendSrc16[1] = colorop_vec.blend(tmpSrc16[1], dst16[1], tempEVA[1], tempEVB[1]);
					break;
				}
			}
			
			const v128u16 blendMask16[2] = {
				(v128u16)vec_mergeh(blendMask8, blendMask8),
				(v128u16)vec_mergel(blendMask8, blendMask8)
			};
			
			tmpSrc16[0] = vec_sel(tmpSrc16[0], blendSrc16[0], blendMask16[0]);
			tmpSrc16[1] = vec_sel(tmpSrc16[1], blendSrc16[1], blendMask16[1]);
		}
		
		// Store the final colors.
		const v128u16 passMask16[2] = {
			(v128u16)vec_mergeh(passMask8, passMask8),
			(v128u16)vec_mergel(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000});
		const v128u16 out16[2]  = {
			vec_sel(dst16[0], vec_or((v128u8)tmpSrc16[0], (v128u8)alphaBits), passMask16[0]),
			vec_sel(dst16[1], vec_or((v128u8)tmpSrc16[1], (v128u8)alphaBits), passMask16[1])
		};
		
		vec_st(out16[0],  0, (u16 *)compInfo.target.lineColor16);
		vec_st(out16[1], 16, (u16 *)compInfo.target.lineColor16);
	}
	else
	{
		const v128u32 dst32[4] = {
			vec_ld( 0, (u32 *)compInfo.target.lineColor32),
			vec_ld(16, (u32 *)compInfo.target.lineColor32),
			vec_ld(32, (u32 *)compInfo.target.lineColor32),
			vec_ld(48, (u32 *)compInfo.target.lineColor32)
		};
		
		if (blendMaskValue != 0x00000000)
		{
			v128u32 blendSrc32[4];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc32[0] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32[0], dst32[0]);
					blendSrc32[1] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32[1], dst32[1]);
					blendSrc32[2] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32[2], dst32[2]);
					blendSrc32[3] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32[3], dst32[3]);
					break;
					
				case GPULayerType_BG:
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[0], dst32[0], eva_vec128, evb_vec128);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[1], dst32[1], eva_vec128, evb_vec128);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[2], dst32[2], eva_vec128, evb_vec128);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32[3], dst32[3], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					v128u16 tempBlendLo = (v128u16)vec_mergeh(eva_vec128, eva_vec128);
					v128u16 tempBlendHi = (v128u16)vec_mergel(eva_vec128, eva_vec128);
					
					const v128u16 tempEVA[4] = {
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendHi ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendHi )
					};
					
					tempBlendLo = (v128u16)vec_mergeh(evb_vec128, evb_vec128);
					tempBlendHi = (v128u16)vec_mergel(evb_vec128, evb_vec128);
					
					const v128u16 tempEVB[4] = {
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendLo ),
						(v128u16)vec_mergel( vec_splat_u8(0), (v128u8)tempBlendHi ),
						(v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)tempBlendHi )
					};
					
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[0], dst32[0], tempEVA[0], tempEVB[0]);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[1], dst32[1], tempEVA[1], tempEVB[1]);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[2], dst32[2], tempEVA[2], tempEVB[2]);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32[3], dst32[3], tempEVA[3], tempEVB[3]);
					break;
				}
			}
			
			const v128u32 blendMask32[4] = {
				vec_perm( blendMask8, blendMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
				vec_perm( blendMask8, blendMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
				vec_perm( blendMask8, blendMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
				vec_perm( blendMask8, blendMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
			};
			
			tmpSrc32[0] = vec_sel(tmpSrc32[0], blendSrc32[0], blendMask32[0]);
			tmpSrc32[1] = vec_sel(tmpSrc32[1], blendSrc32[1], blendMask32[1]);
			tmpSrc32[2] = vec_sel(tmpSrc32[2], blendSrc32[2], blendMask32[2]);
			tmpSrc32[3] = vec_sel(tmpSrc32[3], blendSrc32[3], blendMask32[3]);
		}
		
		// Store the final colors.
		const v128u32 passMask32[4] = {
			vec_perm( passMask8, passMask8, ((v128u8){0x00,0x00,0x00,0x00,  0x01,0x01,0x01,0x01,  0x02,0x02,0x02,0x02,  0x03,0x03,0x03,0x03}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x04,0x04,0x04,0x04,  0x05,0x05,0x05,0x05,  0x06,0x06,0x06,0x06,  0x07,0x07,0x07,0x07}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x08,0x08,0x08,0x08,  0x09,0x09,0x09,0x09,  0x0A,0x0A,0x0A,0x0A,  0x0B,0x0B,0x0B,0x0B}) ),
			vec_perm( passMask8, passMask8, ((v128u8){0x0C,0x0C,0x0C,0x0C,  0x0D,0x0D,0x0D,0x0D,  0x0E,0x0E,0x0E,0x0E,  0x0F,0x0F,0x0F,0x0F}) )
		};
		
		const v128u32 alphaBitsSwapped = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u32 out32[4]	= {
			vec_sel(dst32[0], vec_or(tmpSrc32[0], alphaBitsSwapped), passMask32[0]),
			vec_sel(dst32[1], vec_or(tmpSrc32[1], alphaBitsSwapped), passMask32[1]),
			vec_sel(dst32[2], vec_or(tmpSrc32[2], alphaBitsSwapped), passMask32[2]),
			vec_sel(dst32[3], vec_or(tmpSrc32[3], alphaBitsSwapped), passMask32[3])
		};
		
		vec_st(out32[0],  0, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[1], 16, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[2], 32, (u32 *)compInfo.target.lineColor32);
		vec_st(out32[3], 48, (u32 *)compInfo.target.lineColor32);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void PixelOperation_AltiVec::Composite16(GPUEngineCompositorInfo &compInfo,
												  const bool didAllPixelsPass,
												  const v128u8 &passMask8,
												  const v128u16 &evy16,
												  const v128u8 &srcLayerID,
												  const v128u16 &src1, const v128u16 &src0,
												  const v128u8 &srcEffectEnableMask,
												  const v128u8 &dstBlendEnableMaskLUT,
												  const u8 *__restrict enableColorEffectPtr,
												  const u8 *__restrict sprAlphaPtr,
												  const u8 *__restrict sprModePtr) const
{
	if ((COMPOSITORMODE != GPUCompositorMode_Unknown) && didAllPixelsPass)
	{
		switch (COMPOSITORMODE)
		{
			case GPUCompositorMode_Debug:
				this->_copy16<OUTPUTFORMAT, true>(compInfo, srcLayerID, src1, src0);
				break;
				
			case GPUCompositorMode_Copy:
				this->_copy16<OUTPUTFORMAT, false>(compInfo, srcLayerID, src1, src0);
				break;
				
			case GPUCompositorMode_BrightUp:
				this->_brightnessUp16<OUTPUTFORMAT>(compInfo, evy16, srcLayerID, src1, src0);
				break;
				
			case GPUCompositorMode_BrightDown:
				this->_brightnessDown16<OUTPUTFORMAT>(compInfo, evy16, srcLayerID, src1, src0);
				break;
				
			default:
				break;
		}
	}
	else
	{
		switch (COMPOSITORMODE)
		{
			case GPUCompositorMode_Debug:
				this->_copyMask16<OUTPUTFORMAT, true>(compInfo, passMask8, srcLayerID, src1, src0);
				break;
				
			case GPUCompositorMode_Copy:
				this->_copyMask16<OUTPUTFORMAT, false>(compInfo, passMask8, srcLayerID, src1, src0);
				break;
				
			case GPUCompositorMode_BrightUp:
				this->_brightnessUpMask16<OUTPUTFORMAT>(compInfo, passMask8, evy16, srcLayerID, src1, src0);
				break;
				
			case GPUCompositorMode_BrightDown:
				this->_brightnessDownMask16<OUTPUTFORMAT>(compInfo, passMask8, evy16, srcLayerID, src1, src0);
				break;
				
			default:
			{
				const v128u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? vec_ld(0, enableColorEffectPtr) : vec_splat_u8((s8)0xFF);
				const v128u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ)   ? vec_ld(0, sprAlphaPtr)          : vec_splat_u8(0);
				const v128u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ)    ? vec_ld(0, sprModePtr)           : vec_splat_u8(0);
				
				this->_unknownEffectMask16<OUTPUTFORMAT, LAYERTYPE>(compInfo,
																	passMask8,
																	evy16,
																	srcLayerID,
																	src1, src0,
																	srcEffectEnableMask,
																	dstBlendEnableMaskLUT,
																	enableColorEffectMask,
																	spriteAlpha,
																	spriteMode);
				break;
			}
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void PixelOperation_AltiVec::Composite32(GPUEngineCompositorInfo &compInfo,
												  const bool didAllPixelsPass,
												  const v128u8 &passMask8,
												  const v128u16 &evy16,
												  const v128u8 &srcLayerID,
												  const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0,
												  const v128u8 &srcEffectEnableMask,
												  const v128u8 &dstBlendEnableMaskLUT,
												  const u8 *__restrict enableColorEffectPtr,
												  const u8 *__restrict sprAlphaPtr,
												  const u8 *__restrict sprModePtr) const
{
	if ((COMPOSITORMODE != GPUCompositorMode_Unknown) && didAllPixelsPass)
	{
		switch (COMPOSITORMODE)
		{
			case GPUCompositorMode_Debug:
				this->_copy32<OUTPUTFORMAT, true>(compInfo, srcLayerID, src3, src2, src1, src0);
				break;
				
			case GPUCompositorMode_Copy:
				this->_copy32<OUTPUTFORMAT, false>(compInfo, srcLayerID, src3, src2, src1, src0);
				break;
				
			case GPUCompositorMode_BrightUp:
				this->_brightnessUp32<OUTPUTFORMAT>(compInfo, evy16, srcLayerID, src3, src2, src1, src0);
				break;
				
			case GPUCompositorMode_BrightDown:
				this->_brightnessDown32<OUTPUTFORMAT>(compInfo, evy16, srcLayerID, src3, src2, src1, src0);
				break;
				
			default:
				break;
		}
	}
	else
	{
		switch (COMPOSITORMODE)
		{
			case GPUCompositorMode_Debug:
				this->_copyMask32<OUTPUTFORMAT, true>(compInfo, passMask8, srcLayerID, src3, src2, src1, src0);
				break;
				
			case GPUCompositorMode_Copy:
				this->_copyMask32<OUTPUTFORMAT, false>(compInfo, passMask8, srcLayerID, src3, src2, src1, src0);
				break;
				
			case GPUCompositorMode_BrightUp:
				this->_brightnessUpMask32<OUTPUTFORMAT>(compInfo, passMask8, evy16, srcLayerID, src3, src2, src1, src0);
				break;
				
			case GPUCompositorMode_BrightDown:
				this->_brightnessDownMask32<OUTPUTFORMAT>(compInfo, passMask8, evy16, srcLayerID, src3, src2, src1, src0);
				break;
				
			default:
			{
				const v128u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? vec_ld(0, enableColorEffectPtr) : vec_splat_u8((s8)0xFF);
				const v128u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ)   ? vec_ld(0, sprAlphaPtr)          : vec_splat_u8(0);
				const v128u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ)    ? vec_ld(0, sprModePtr)           : vec_splat_u8(0);
				
				this->_unknownEffectMask32<OUTPUTFORMAT, LAYERTYPE>(compInfo,
																	passMask8,
																	evy16,
																	srcLayerID,
																	src3, src2, src1, src0,
																	srcEffectEnableMask,
																	dstBlendEnableMaskLUT,
																	enableColorEffectMask,
																	spriteAlpha,
																	spriteMode);
				break;
			}
		}
	}
}

template <bool ISFIRSTLINE>
void GPUEngineBase::_MosaicLine(GPUEngineCompositorInfo &compInfo)
{
	const u16 *mosaicColorBG = this->_mosaicColors.bg[compInfo.renderState.selectedLayerID];
	
	for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=sizeof(v128u16))
	{
		const v128u16 dstColor16[2] = {
			vec_ld( 0, this->_deferredColorNative + x),
			vec_ld(16, this->_deferredColorNative + x)
		};
		
		if (ISFIRSTLINE)
		{
			const v128u8 indexVec = vec_ld(0, this->_deferredIndexNative + x);
			const v128u8 idxMask8 = vec_cmpeq( indexVec, vec_splat_u8(0) );
			const v128u16 idxMask16[2] = {
				(v128u16)vec_mergeh(idxMask8, idxMask8),
				(v128u16)vec_mergel(idxMask8, idxMask8)
			};
			
			const v128u16 mosaicColor16[2] = {
				vec_sel( vec_and(dstColor16[0], ((v128u16){0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF})), vec_splat_u16((s16)0xFFFF), idxMask16[0] ),
				vec_sel( vec_and(dstColor16[1], ((v128u16){0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF})), vec_splat_u16((s16)0xFFFF), idxMask16[1] )
			};
			
			const v128u8 mosaicSetColorMask8 = vec_cmpeq( vec_ld(0, compInfo.renderState.mosaicWidthBG->begin + x), vec_splat_u16(0) );
			const v128u16 mosaicSetColorMask16[2] = {
				(v128u16)vec_mergeh(mosaicSetColorMask8, mosaicSetColorMask8),
				(v128u16)vec_mergel(mosaicSetColorMask8, mosaicSetColorMask8)
			};
			
			const u16 *__restrict mcbg = mosaicColorBG + x;
			vec_st( vec_sel(mosaicColor16[0], vec_ld( 0, mcbg), mosaicSetColorMask16[0]),  0, mcbg );
			vec_st( vec_sel(mosaicColor16[1], vec_ld(16, mcbg), mosaicSetColorMask16[1]), 16, mcbg );
		}
		
		// TODO: Need a better algorithm that doesn't rely on scatter-gather.
		CACHE_ALIGN const u16 src16[16] = {
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 0]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 1]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 2]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 3]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 4]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 5]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 6]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 7]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 8]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+ 9]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+10]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+11]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+12]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+13]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+14]],
			mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+15]]
		};
		
		const v128u16 src16_vec[2] = {
			vec_ld( 0, src16),
			vec_ld(16, src16)
		};
		
		const v128u16 writeMask16[2] = {
			vec_cmpeq(src16_vec[0], vec_splat_u16((s16)0xFFFF)),
			vec_cmpeq(src16_vec[1], vec_splat_u16((s16)0xFFFF))
		};
		
		const v128u16 out16[2] = {
			vec_sel(src16_vec[0], dstColor16[0], writeMask16[0]),
			vec_sel(src16_vec[1], dstColor16[1], writeMask16[1])
		};
		
		vec_st(out16[0],  0, this->_deferredColorNative + x);
		vec_st(out16[1], 16, this->_deferredColorNative + x);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeNativeLineOBJ_LoopOp(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const Color4u8 *__restrict srcColorNative32)
{
	static const size_t step = sizeof(v128u8);
	
	const bool isUsingSrc32 = (srcColorNative32 != NULL);
	CACHE_ALIGN const u16 evy = compInfo.renderState.blendEVY;
	const v128u16 evy16 = vec_splat( vec_lde(0, &evy), 0 );
	CACHE_ALIGN const u8 lid = compInfo.renderState.selectedLayerID;
	const v128u8 srcLayerID = vec_splat( vec_lde(0, &lid), 0 );
	CACHE_ALIGN const u8 en = compInfo.renderState.srcEffectEnable[GPULayerID_OBJ];
	const v128u8 srcEffectEnableMask = vec_splat( vec_lde(0, &en), 0 );
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vec_ld(0, compInfo.renderState.dstBlendEnableVecLookup) : vec_splat_u8(0);
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=step, srcColorNative16+=step, srcColorNative32+=step, compInfo.target.xNative+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		v128u8 passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = vec_ld(0, this->_didPassWindowTestNative[GPULayerID_OBJ] + i);
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
			if (passMaskValue == 0)
			{
				continue;
			}
			
			passMaskValue = vec_all_ne( passMask8, vec_splat_u8(0) );
			didAllPixelsPass = (passMaskValue == 1);
		}
		else
		{
			passMask8 = vec_splat_u8((s8)0xFF);
			passMaskValue = 1;
			didAllPixelsPass = true;
		}
		
		if (isUsingSrc32)
		{
			const v128u32 src[4] = {
				vec_ld( 0, (u32 *)srcColorNative32),
				vec_ld(16, (u32 *)srcColorNative32),
				vec_ld(32, (u32 *)srcColorNative32),
				vec_ld(48, (u32 *)srcColorNative32)
			};
			
			pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo,
			                                                                                               didAllPixelsPass,
			                                                                                               passMask8, evy16,
			                                                                                               srcLayerID,
			                                                                                               src[3], src[2], src[1], src[0],
			                                                                                               srcEffectEnableMask,
			                                                                                               dstBlendEnableMaskLUT,
			                                                                                               this->_enableColorEffectNative[GPULayerID_OBJ] + i,
			                                                                                               this->_sprAlpha[compInfo.line.indexNative] + i,
			                                                                                               this->_sprType[compInfo.line.indexNative] + i);
		}
		else
		{
			const v128u16 src[2] = {
				vec_ld( 0, srcColorNative16),
				vec_ld(16, srcColorNative16)
			};
			
			pixelop_vec.Composite16<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo,
			                                                                                               didAllPixelsPass,
			                                                                                               passMask8, evy16,
			                                                                                               srcLayerID,
			                                                                                               src[1], src[0],
			                                                                                               srcEffectEnableMask,
			                                                                                               dstBlendEnableMaskLUT,
			                                                                                               this->_enableColorEffectNative[GPULayerID_OBJ] + i,
			                                                                                               this->_sprAlpha[compInfo.line.indexNative] + i,
			                                                                                               this->_sprType[compInfo.line.indexNative] + i);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
size_t GPUEngineBase::_CompositeLineDeferred_LoopOp(GPUEngineCompositorInfo &compInfo, const u8 *__restrict windowTestPtr, const u8 *__restrict colorEffectEnablePtr, const u16 *__restrict srcColorCustom16, const u8 *__restrict srcIndexCustom)
{
	static const size_t step = sizeof(v128u8);
	
	const size_t vecPixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	CACHE_ALIGN const u16 evy = compInfo.renderState.blendEVY;
	const v128u16 evy16 = vec_splat( vec_lde(0, &evy), 0 );
	CACHE_ALIGN const u8 lid = compInfo.renderState.selectedLayerID;
	const v128u8 srcLayerID = vec_splat( vec_lde(0, &lid), 0 );
	CACHE_ALIGN const u8 en = compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID];
	const v128u8 srcEffectEnableMask = vec_splat( vec_lde(0, &en), 0 );
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vec_ld(0, compInfo.renderState.dstBlendEnableVecLookup) : vec_splat_u8(0);
	
	size_t i = 0;
	for (; i < vecPixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		v128u8 passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST || (LAYERTYPE == GPULayerType_BG))
		{
			if (WILLPERFORMWINDOWTEST)
			{
				// Do the window test.
				passMask8 = vec_ld(0, windowTestPtr + compInfo.target.xCustom);
			}
			
			if (LAYERTYPE == GPULayerType_BG)
			{
				// Do the index test. Pixels with an index value of 0 are rejected.
				const v128u8 idxPassMask8 = vec_cmpeq( vec_ld(0, srcIndexCustom + compInfo.target.xCustom), vec_splat_u8(0) );
				
				if (WILLPERFORMWINDOWTEST)
				{
					passMask8 = vec_andc(passMask8, idxPassMask8);
				}
				else
				{
					passMask8 = vec_xor(idxPassMask8, vec_splat_u8((s8)0xFF));
				}
			}
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
			if (passMaskValue == 0)
			{
				continue;
			}
			
			passMaskValue = vec_all_ne( passMask8, vec_splat_u8(0) );
			didAllPixelsPass = (passMaskValue == 1);
		}
		else
		{
			passMask8 = vec_splat_u8((s8)0xFF);
			passMaskValue = 1;
			didAllPixelsPass = true;
		}
		
		const v128u16 src[2] = {
			vec_ld( 0, srcColorCustom16 + compInfo.target.xCustom),
			vec_ld(16, srcColorCustom16 + compInfo.target.xCustom)
		};
		
		pixelop_vec.Composite16<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
		                                                                                        didAllPixelsPass,
		                                                                                        passMask8, evy16,
		                                                                                        srcLayerID,
		                                                                                        src[1], src[0],
		                                                                                        srcEffectEnableMask,
		                                                                                        dstBlendEnableMaskLUT,
		                                                                                        colorEffectEnablePtr + compInfo.target.xCustom,
		                                                                                        this->_sprAlphaCustom + compInfo.target.xCustom,
		                                                                                        this->_sprTypeCustom + compInfo.target.xCustom);
	}
	
	return i;
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
size_t GPUEngineBase::_CompositeVRAMLineDeferred_LoopOp(GPUEngineCompositorInfo &compInfo, const u8 *__restrict windowTestPtr, const u8 *__restrict colorEffectEnablePtr, const void *__restrict vramColorPtr)
{
	static const size_t step = sizeof(v128u8);
	
	const size_t vecPixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	CACHE_ALIGN const u16 evy = compInfo.renderState.blendEVY;
	const v128u16 evy16 = vec_splat( vec_lde(0, &evy), 0 );
	CACHE_ALIGN const u8 lid = compInfo.renderState.selectedLayerID;
	const v128u8 srcLayerID = vec_splat( vec_lde(0, &lid), 0 );
	CACHE_ALIGN const u8 en = compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID];
	const v128u8 srcEffectEnableMask = vec_splat( vec_lde(0, &en), 0 );
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vec_ld(0, compInfo.renderState.dstBlendEnableVecLookup) : vec_splat_u8(0);
	
	size_t i = 0;
	for (; i < vecPixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		v128u8 passMask8;
		int passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = vec_ld( 0, (u8 *)((v128u8 *)(windowTestPtr + compInfo.target.xCustom)) );
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = vec_splat_u8((s8)0xFF);
			passMaskValue = 1;
		}
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
			case NDSColorFormat_BGR666_Rev:
			{
				v128u16 src16[2] = {
					(v128u16)vec_ld( 0, (u16 *)vramColorPtr + i),
					(v128u16)vec_ld(16, (u16 *)vramColorPtr + i)
				};
				
				// Do note that calling vec_ld() within vec_perm() can cause data alignment issues when compiled for 64-bit.
				// Resist the urge to do this, and ensure that vec_ld() and vec_perm() are called on separate lines for safety.
				src16[0] = (v128u16)vec_perm( (v128u8)src16[0], vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E}) );
				src16[1] = (v128u16)vec_perm( (v128u8)src16[1], vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E}) );
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v128u8 tempPassMask = vec_pack( vec_sr(src16[0], vec_splat_u16(15)), vec_sr(src16[1], vec_splat_u16(15)) );
					tempPassMask = vec_cmpeq(tempPassMask, vec_splat_u8(1));
					
					passMask8 = vec_and(tempPassMask, passMask8);
					passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
					
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				passMaskValue = vec_all_ne( passMask8, vec_splat_u8(0) );
				const bool didAllPixelsPass = (passMaskValue == 1);
				pixelop_vec.Composite16<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
				                                                                                        didAllPixelsPass,
				                                                                                        passMask8, evy16,
				                                                                                        srcLayerID,
				                                                                                        src16[1], src16[0],
				                                                                                        srcEffectEnableMask,
				                                                                                        dstBlendEnableMaskLUT,
				                                                                                        colorEffectEnablePtr + compInfo.target.xCustom,
				                                                                                        this->_sprAlphaCustom + compInfo.target.xCustom,
				                                                                                        this->_sprTypeCustom + compInfo.target.xCustom);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				const v128u32 src32[4] = {
					vec_ld( 0, (u32 *)vramColorPtr + i),
					vec_ld(16, (u32 *)vramColorPtr + i),
					vec_ld(32, (u32 *)vramColorPtr + i),
					vec_ld(48, (u32 *)vramColorPtr + i)
				};
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					// Do the alpha test. Pixels with an alpha value of 0 are rejected.
					const v128u32 alphaBits = ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
					const v128u8 srcAlpha = (v128u8)vec_pack( (v128u16)vec_pack( (v128u32)vec_and(src32[0], alphaBits), (v128u32)vec_and(src32[1], alphaBits) ),
					                                          (v128u16)vec_pack( (v128u32)vec_and(src32[2], alphaBits), (v128u32)vec_and(src32[3], alphaBits) )
					);
					
					passMask8 = vec_andc( passMask8, vec_cmpeq(srcAlpha, vec_splat_u8(0)) );
					passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				passMaskValue = vec_all_ne( passMask8, vec_splat_u8(0) );
				const bool didAllPixelsPass = (passMaskValue == 1);
				pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
				                                                                                        didAllPixelsPass,
				                                                                                        passMask8, evy16,
				                                                                                        srcLayerID,
				                                                                                        src32[3], src32[2], src32[1], src32[0],
				                                                                                        srcEffectEnableMask,
				                                                                                        dstBlendEnableMaskLUT,
				                                                                                        colorEffectEnablePtr + compInfo.target.xCustom,
				                                                                                        this->_sprAlphaCustom + compInfo.target.xCustom,
				                                                                                        this->_sprTypeCustom + compInfo.target.xCustom);
				break;
			}
		}
	}
	
	return i;
}

template <bool ISDEBUGRENDER>
size_t GPUEngineBase::_RenderSpriteBMP_LoopOp(const size_t length, const u8 spriteAlpha, const u8 prio, const u8 spriteNum, const u16 *__restrict vramBuffer,
											  size_t &frameX, size_t &spriteX,
											  u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	static const size_t step = sizeof(v128u16);
	if (length < step)
	{
		return 0;
	}
	
	u16 *__restrict srcAligned = (u16 *)vramBuffer + spriteX;
	if ( (uintptr_t)srcAligned != ( (uintptr_t)srcAligned & ~(uintptr_t)0x0F) )
	{
		return 0;
	}
	
	u16 *__restrict dstAligned = dst + frameX;
	if ( (uintptr_t)dstAligned != ( (uintptr_t)dstAligned & ~(uintptr_t)0x0F) )
	{
		return 0;
	}
	
	u8  *__restrict prioTabAligned = prioTab + frameX;
	if ( (uintptr_t)prioTabAligned != ( (uintptr_t)prioTabAligned & ~(uintptr_t)0x0F) )
	{
		return 0;
	}
	
	if (!ISDEBUGRENDER)
	{
		u8 *__restrict dst_alpha_Aligned = dst_alpha + frameX;
		u8 *__restrict typeTabAligned    = typeTab + frameX;
		u8 *__restrict sprNumAligned     = this->_sprNum + frameX;
		
		if ( (uintptr_t)dst_alpha_Aligned != ( (uintptr_t)dst_alpha_Aligned & ~(uintptr_t)0x0F) ||
		     (uintptr_t)typeTabAligned    != ( (uintptr_t)typeTabAligned    & ~(uintptr_t)0x0F) ||
		     (uintptr_t)sprNumAligned     != ( (uintptr_t)sprNumAligned     & ~(uintptr_t)0x0F) )
		{
			return 0;
		}
	}
	
	CACHE_ALIGN const u8 sa = spriteAlpha + 1;
	const v128u8 spriteAlphaPlusOneVec = (v128u8)vec_splat( vec_lde(0, &sa), 0 );
	CACHE_ALIGN const u8 num = spriteNum;
	const v128u8 spriteNumVec = (v128u8)vec_splat( vec_lde(0, &num), 0 );
	CACHE_ALIGN const u8 pr = prio;
	const v128u8 prioVec8 = (v128u8)vec_splat( vec_lde(0, &pr), 0 );
	
	const size_t vecPixCount = length & ~(uintptr_t)0x0F;
	size_t i = 0;
	
	for (; i < vecPixCount; i+=step, spriteX+=step, frameX+=step)
	{
		prioTabAligned = prioTab + frameX;
		const v128u8 prioTabVec8 = vec_ld(0, prioTabAligned);
		
		srcAligned = (u16 *)vramBuffer + spriteX;
		v128u16 color16Lo = vec_ld( 0, srcAligned);
		v128u16 color16Hi = vec_ld(16, srcAligned);
		color16Lo = (v128u16)vec_perm( (v128u8)color16Lo, vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E}) );
		color16Hi = (v128u16)vec_perm( (v128u8)color16Hi, vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E}) );
		
		const v128u8 alphaCompare = vec_cmpeq( vec_pack( vec_sr(color16Lo, vec_splat_u16(15)), vec_sr(color16Hi, vec_splat_u16(15)) ), vec_splat_u8(0x01) );
		const v128u8 prioCompare  = vec_cmpgt(prioTabVec8, prioVec8);
		
		const v128u8  combinedCompare   = vec_and(prioCompare, alphaCompare);
		const v128u16 combinedLoCompare = (v128u16)vec_mergeh(combinedCompare, combinedCompare);
		const v128u16 combinedHiCompare = (v128u16)vec_mergel(combinedCompare, combinedCompare);
		
		dstAligned = dst + frameX;
		const v128u16 dst16[2] = {
			vec_ld( 0, dstAligned),
			vec_ld(16, dstAligned)
		};
		
		const v128u16 out16[2] = {
			vec_sel(dst16[0], color16Lo, combinedLoCompare),
			vec_sel(dst16[1], color16Hi, combinedHiCompare)
		};
		
		vec_st(out16[0],  0, dstAligned);
		vec_st(out16[1], 16, dstAligned);
		vec_st( vec_sel(prioTabVec8, prioVec8, combinedCompare), 0, prioTabAligned );
		
		if (!ISDEBUGRENDER)
		{
			u8 *__restrict dst_alpha_Aligned = dst_alpha + frameX;
			v128u8 dstAlphaOut = vec_ld(0, dst_alpha_Aligned);
			dstAlphaOut = vec_sel(dstAlphaOut, spriteAlphaPlusOneVec, combinedCompare);
			vec_st(dstAlphaOut, 0, dst_alpha_Aligned);
			
			u8 *__restrict typeTabAligned = typeTab + frameX;
			v128u8 typeTabOut = vec_ld(0, typeTabAligned);
			typeTabOut = vec_sel(typeTabOut, vec_splat_u8(OBJMode_Bitmap), combinedCompare);
			vec_st(typeTabOut, 0, typeTabAligned);
			
			u8 *__restrict sprNumAligned = this->_sprNum + frameX;
			v128u8 sprNumOut = vec_ld(0, sprNumAligned);
			sprNumOut = vec_sel(sprNumOut, spriteNumVec, combinedCompare);
			vec_st(sprNumOut, 0, sprNumAligned);
		}
	}
	
	return i;
}

void GPUEngineBase::_PerformWindowTestingNative(GPUEngineCompositorInfo &compInfo, const size_t layerID, const u8 *__restrict win0, const u8 *__restrict win1, const u8 *__restrict winObj, u8 *__restrict didPassWindowTestNative, u8 *__restrict enableColorEffectNative)
{
	const v128u8 *__restrict win0Ptr = (const v128u8 *__restrict)win0;
	const v128u8 *__restrict win1Ptr = (const v128u8 *__restrict)win1;
	const v128u8 *__restrict winObjPtr = (const v128u8 *__restrict)winObj;
	
	v128u8 *__restrict didPassWindowTestNativePtr = (v128u8 *__restrict)didPassWindowTestNative;
	v128u8 *__restrict enableColorEffectNativePtr = (v128u8 *__restrict)enableColorEffectNative;
	
	v128u8 didPassWindowTest;
	v128u8 enableColorEffect;
	
	v128u8 win0HandledMask;
	v128u8 win1HandledMask;
	v128u8 winOBJHandledMask;
	v128u8 winOUTHandledMask;
	
	CACHE_ALIGN u8 en;
	CACHE_ALIGN u8 fx;
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH/sizeof(v128u8); i++)
	{
		didPassWindowTest = vec_splat_u8(0);
		enableColorEffect = vec_splat_u8(0);
		
		win0HandledMask = vec_splat_u8(0);
		win1HandledMask = vec_splat_u8(0);
		winOBJHandledMask = vec_splat_u8(0);
		
		// Window 0 has the highest priority, so always check this first.
		if (win0Ptr != NULL)
		{
			en = compInfo.renderState.WIN0_enable[layerID];
			fx = compInfo.renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG];
			const v128u8 en_vec = vec_splat( vec_lde(0, &en), 0 );
			const v128u8 fx_vec = vec_splat( vec_lde(0, &fx), 0 );
			
			win0HandledMask = vec_cmpeq( vec_ld(0, win0Ptr + i), vec_splat_u8(1) );
			didPassWindowTest = vec_and(win0HandledMask, en_vec);
			enableColorEffect = vec_and(win0HandledMask, fx_vec);
		}
		
		// Window 1 has medium priority, and is checked after Window 0.
		if (win1Ptr != NULL)
		{
			en = compInfo.renderState.WIN1_enable[layerID];
			fx = compInfo.renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG];
			const v128u8 en_vec = vec_splat( vec_lde(0, &en), 0 );
			const v128u8 fx_vec = vec_splat( vec_lde(0, &fx), 0 );
			
			win1HandledMask = vec_andc( vec_cmpeq(vec_ld(0, win1Ptr + i), vec_splat_u8(1)), win0HandledMask );
			didPassWindowTest = vec_sel(didPassWindowTest, en_vec, win1HandledMask);
			enableColorEffect = vec_sel(enableColorEffect, fx_vec, win1HandledMask);
		}
		
		// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
		if (winObjPtr != NULL)
		{
			en = compInfo.renderState.WINOBJ_enable[layerID];
			fx = compInfo.renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG];
			const v128u8 en_vec = vec_splat( vec_lde(0, &en), 0 );
			const v128u8 fx_vec = vec_splat( vec_lde(0, &fx), 0 );
			
			winOBJHandledMask = vec_andc( vec_cmpeq(vec_ld(0, winObjPtr + i), vec_splat_u8(1)), vec_or(win0HandledMask, win1HandledMask) );
			didPassWindowTest = vec_sel(didPassWindowTest, en_vec, winOBJHandledMask);
			enableColorEffect = vec_sel(enableColorEffect, fx_vec, winOBJHandledMask);
		}
		
		// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
		// This has the lowest priority, and is always checked last.
		en = compInfo.renderState.WINOUT_enable[layerID];
		fx = compInfo.renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG];
		const v128u8 en_vec = vec_splat( vec_lde(0, &en), 0 );
		const v128u8 fx_vec = vec_splat( vec_lde(0, &fx), 0 );
		
		winOUTHandledMask = vec_xor( vec_or(win0HandledMask, vec_or(win1HandledMask, winOBJHandledMask)), vec_splat_u32((s32)0xFFFFFFFF) );
		didPassWindowTest = vec_sel(didPassWindowTest, en_vec, winOUTHandledMask);
		enableColorEffect = vec_sel(enableColorEffect, fx_vec, winOUTHandledMask);
		
		vec_st(didPassWindowTest, 0, didPassWindowTestNativePtr + i);
		vec_st(enableColorEffect, 0, enableColorEffectNativePtr + i);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
size_t GPUEngineA::_RenderLine_Layer3D_LoopOp(GPUEngineCompositorInfo &compInfo, const u8 *__restrict windowTestPtr, const u8 *__restrict colorEffectEnablePtr, const Color4u8 *__restrict srcLinePtr)
{
	static const size_t step = sizeof(v128u8);
	
	const size_t vecPixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	CACHE_ALIGN const u16 evy = compInfo.renderState.blendEVY;
	const v128u16 evy16 = vec_splat( vec_lde(0, &evy), 0 );
	CACHE_ALIGN const u8 lid = compInfo.renderState.selectedLayerID;
	const v128u8 srcLayerID = vec_splat( vec_lde(0, &lid), 0 );
	CACHE_ALIGN const u8 en = compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID];
	const v128u8 srcEffectEnableMask = vec_splat( vec_lde(0, &en), 0 );
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vec_ld(0, compInfo.renderState.dstBlendEnableVecLookup) : vec_splat_u8(0);
	
	size_t i = 0;
	for (; i < vecPixCount; i+=step, srcLinePtr+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		// Determine which pixels pass by doing the window test and the alpha test.
		v128u8 passMask8;
		int passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = vec_ld( 0, (v128u8 *)(windowTestPtr + compInfo.target.xCustom) );
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = vec_splat_u8((s8)0xFF);
			passMaskValue = 1;
		}
		
		const v128u32 src32[4] = {
			vec_ld( 0, srcLinePtr),
			vec_ld(16, srcLinePtr),
			vec_ld(32, srcLinePtr),
			vec_ld(48, srcLinePtr)
		};
		
		// Do the alpha test. Pixels with an alpha value of 0 are rejected.
		const v128u32 alphaBits = ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF});
		const v128u8 srcAlpha = vec_pack( vec_pack( vec_and(src32[0], alphaBits), vec_and(src32[1], alphaBits) ),
		                                  vec_pack( vec_and(src32[2], alphaBits), vec_and(src32[3], alphaBits) )
		);
		
		passMask8 = vec_andc( passMask8, vec_cmpeq(srcAlpha, vec_splat_u8(0)) );
		
		// If none of the pixels within the vector pass, then reject them all at once.
		passMaskValue = vec_any_ne( passMask8, vec_splat_u8(0) );
		if (passMaskValue == 0)
		{
			continue;
		}
		
		// Write out the pixels.
		passMaskValue = vec_all_ne( passMask8, vec_splat_u8(0) );
		const bool didAllPixelsPass = (passMaskValue == 1);
		pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D, WILLPERFORMWINDOWTEST>(compInfo,
		                                                                                              didAllPixelsPass,
		                                                                                              passMask8, evy16,
		                                                                                              srcLayerID,
		                                                                                              src32[3], src32[2], src32[1], src32[0],
		                                                                                              srcEffectEnableMask,
		                                                                                              dstBlendEnableMaskLUT,
		                                                                                              colorEffectEnablePtr + compInfo.target.xCustom,
		                                                                                              NULL,
		                                                                                              NULL);
	}
	
	return i;
}

template <NDSColorFormat OUTPUTFORMAT>
size_t GPUEngineA::_RenderLine_DispCapture_Blend_VecLoop(const void *srcA, const void *srcB, void *dst, const u8 blendEVA, const u8 blendEVB, const size_t length)
{
	CACHE_ALIGN const u16 eva = blendEVA;
	CACHE_ALIGN const u16 evb = blendEVB;
	const v128u16 blendEVA_vec = vec_splat( vec_lde(0, &eva), 0 );
	const v128u16 blendEVB_vec = vec_splat( vec_lde(0, &evb), 0 );
	
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? length * sizeof(u32) / sizeof(v128u8) : length * sizeof(u16) / sizeof(v128u8);
	for (; i < vecCount; i++)
	{
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			v128u32 srcA_vec = vec_ld(0, (v128u32 *)srcA + i);
			v128u32 srcB_vec = vec_ld(0, (v128u32 *)srcB + i);
			
			// Get color masks based on if the alpha value is 0. Colors with an alpha value
			// equal to 0 are rejected.
			v128u32 srcA_alpha = vec_and( srcA_vec, ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF}) );
			v128u32 srcB_alpha = vec_and( srcB_vec, ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF}) );
			v128u32 srcA_masked = vec_andc( srcA_vec, vec_cmpeq(srcA_alpha, vec_splat_u32(0)) );
			v128u32 srcB_masked = vec_andc( srcB_vec, vec_cmpeq(srcB_alpha, vec_splat_u32(0)) );
			
			v128u16 outColorLo;
			v128u16 outColorHi;
			v128u32 dstColor32;
			
			// Temporarily convert the color component values from 8-bit to 16-bit, and then
			// do the blend calculation.
			v128u16 srcA_maskedLo = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)srcA_masked );
			v128u16 srcA_maskedHi = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)srcA_masked );
			v128u16 srcB_maskedLo = (v128u16)vec_mergel( vec_splat_u8(0), (v128u8)srcB_masked );
			v128u16 srcB_maskedHi = (v128u16)vec_mergeh( vec_splat_u8(0), (v128u8)srcB_masked );
			
			outColorLo = vec_add( vec_mulo((v128u8)srcA_maskedLo, (v128u8)blendEVA_vec), vec_mulo((v128u8)srcB_maskedLo, (v128u8)blendEVB_vec) );
			outColorHi = vec_add( vec_mulo((v128u8)srcA_maskedHi, (v128u8)blendEVA_vec), vec_mulo((v128u8)srcB_maskedHi, (v128u8)blendEVB_vec) );
			
			outColorLo = vec_sr( outColorLo, vec_splat_u16(4) );
			outColorHi = vec_sr( outColorHi, vec_splat_u16(4) );
			
			// Convert the color components back from 16-bit to 8-bit using a saturated pack.
			dstColor32 = (v128u32)vec_packsu(outColorLo, outColorHi);
			
			// Add the alpha components back in.
			dstColor32 = vec_and( dstColor32, ((v128u32){0xFFFFFF00,0xFFFFFF00,0xFFFFFF00,0xFFFFFF00}) );
			dstColor32 = vec_or(dstColor32, srcA_alpha);
			dstColor32 = vec_or(dstColor32, srcB_alpha);
			vec_st(dstColor32, 0, (v128u32 *)dst + i);
		}
		else
		{
			v128u16 srcA_vec = vec_ld(0, (v128u16 *)srcA + i);
			v128u16 srcB_vec = vec_ld(0, (v128u16 *)srcB + i);
			
			v128u16 srcA_alpha = vec_and( srcA_vec, ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}) );
			v128u16 srcB_alpha = vec_and( srcB_vec, ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}) );
			v128u16 srcA_masked = vec_andc( srcA_vec, vec_cmpeq(srcA_alpha, vec_splat_u16(0)) );
			v128u16 srcB_masked = vec_andc( srcB_vec, vec_cmpeq(srcB_alpha, vec_splat_u16(0)) );
			v128u16 colorBitMask = ((v128u16){0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F});
			
			v128u16 ra = vec_and(       srcA_masked,                     colorBitMask);
			v128u16 ga = vec_and(vec_sr(srcA_masked, vec_splat_u16( 5)), colorBitMask);
			v128u16 ba = vec_and(vec_sr(srcA_masked, vec_splat_u16(10)), colorBitMask);
			
			v128u16 rb = vec_and(       srcB_masked,                     colorBitMask);
			v128u16 gb = vec_and(vec_sr(srcB_masked, vec_splat_u16( 5)), colorBitMask);
			v128u16 bb = vec_and(vec_sr(srcB_masked, vec_splat_u16(10)), colorBitMask);
			
			ra = vec_add( vec_mulo((v128u8)ra, (v128u8)blendEVA_vec), vec_mulo((v128u8)rb, (v128u8)blendEVB_vec) );
			ga = vec_add( vec_mulo((v128u8)ga, (v128u8)blendEVA_vec), vec_mulo((v128u8)gb, (v128u8)blendEVB_vec) );
			ba = vec_add( vec_mulo((v128u8)ba, (v128u8)blendEVA_vec), vec_mulo((v128u8)bb, (v128u8)blendEVB_vec) );
			
			ra = vec_sr( ra, vec_splat_u16(4) );
			ga = vec_sr( ga, vec_splat_u16(4) );
			ba = vec_sr( ba, vec_splat_u16(4) );
			
			ra = vec_min(ra, colorBitMask);
			ga = vec_min(ga, colorBitMask);
			ba = vec_min(ba, colorBitMask);
			
			v128u16 dstColor16 = vec_or( (v128u8)vec_or((v128u8)vec_or((v128u8)ra, (v128u8)vec_sl(ga, vec_splat_u16(5))), (v128u8)vec_sl(ba, vec_splat_u16(10))), (v128u8)vec_or(srcA_alpha, srcB_alpha) );
			vec_st( vec_perm(dstColor16, vec_splat_u8(0), ((v128u8){0x01,0x00,  0x03,0x02,  0x05,0x04,  0x07,0x06,  0x09,0x08,  0x0B,0x0A,  0x0D,0x0C,  0x0F,0x0E})), 0, (v128u16 *)dst + i );
		}
	}
	
	return (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? i * sizeof(v128u32) / sizeof(u32) : i * sizeof(v128u16) / sizeof(u16);
}

template <NDSColorFormat OUTPUTFORMAT>
size_t NDSDisplay::_ApplyMasterBrightnessUp_LoopOp(void *__restrict dst, const size_t pixCount, const u8 intensityClamped)
{
	CACHE_ALIGN const u16 ic = intensityClamped;
	const v128u16 intensityVec16 = vec_splat( vec_lde(0, &ic), 0 );
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? pixCount * sizeof(u32) / sizeof(v128u32) : pixCount * sizeof(u16) / sizeof(v128u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			v128u16 dstColor = vec_ld(0, (v128u16 *)dst + i);
			dstColor = colorop_vec.increase(dstColor, intensityVec16);
			dstColor = vec_or( (v128u8)dstColor, (v128u8)((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}) );
			vec_st(dstColor, 0, (v128u16 *)dst + i);
		}
		else
		{
			v128u32 dstColor = vec_ld(0, (v128u32 *)dst + i);
			dstColor = colorop_vec.increase<OUTPUTFORMAT>(dstColor, intensityVec16);
			dstColor = vec_or( dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF}) );
			vec_st(dstColor, 0, (v128u32 *)dst + i);
		}
	}
	
	return (i * sizeof(v128u8));
}

template <NDSColorFormat OUTPUTFORMAT>
size_t NDSDisplay::_ApplyMasterBrightnessDown_LoopOp(void *__restrict dst, const size_t pixCount, const u8 intensityClamped)
{
	CACHE_ALIGN const u16 ic = intensityClamped;
	const v128u16 intensityVec16 = vec_splat( vec_lde(0, &ic), 0 );
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? pixCount * sizeof(u32) / sizeof(v128u32) : pixCount * sizeof(u16) / sizeof(v128u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			v128u16 dstColor = vec_ld(0, (v128u16 *)dst + i);
			dstColor = colorop_vec.decrease(dstColor, intensityVec16);
			dstColor = vec_or( (v128u8)dstColor, (v128u8)((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}) );
			vec_st(dstColor, 0, (v128u16 *)dst + i);
		}
		else
		{
			v128u32 dstColor = vec_ld(0, (v128u32 *)dst + i);
			dstColor = colorop_vec.decrease<OUTPUTFORMAT>(dstColor, intensityVec16);
			dstColor = vec_or( dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F}) : ((v128u32){0x000000FF,0x000000FF,0x000000FF,0x000000FF}) );
			vec_st(dstColor, 0, (v128u32 *)dst + i);
		}
	}
	
	return (i * sizeof(v128u8));
}

#endif // ENABLE_ALTIVEC
