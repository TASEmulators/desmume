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

#ifndef ENABLE_NEON_A64
	#error This code requires ARM64 NEON support.
	#warning This error might occur if this file is compiled directly. Do not compile this file directly, as it is already included in GPU_Operations.cpp.
#else

#include "GPU_Operations_NEON.h"
#include "./utils/colorspacehandler/colorspacehandler_NEON.h"


static const ColorOperation_NEON colorop_vec;
static const PixelOperation_NEON pixelop_vec;

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
{
	if (INTEGERSCALEHINT == 0)
	{
		memcpy(dst, src, dstWidth * ELEMENTSIZE);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(uint8x16_t) / ELEMENTSIZE), vst1q_u8((u8 *)((uint8x16_t *)dst + (X)), vld1q_u8((u8 *)((uint8x16_t *)src + (X)))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		if (ELEMENTSIZE == 1)
		{
			v128u8 srcPix;
			uint8x16x2_t dstPix;
			
			if (SCALEVERTICAL)
			{
				MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
				           srcPix = vld1q_u8((u8 *)((v128u8 *)src + (X))); \
				           dstPix.val[0] = vzip1q_u8(srcPix, srcPix); \
				           dstPix.val[1] = vzip2q_u8(srcPix, srcPix); \
				           vst1q_u8_x2((u8 *)((v128u8 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v128u8) / ELEMENTSIZE)) * 0)), dstPix); \
				           vst1q_u8_x2((u8 *)((v128u8 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v128u8) / ELEMENTSIZE)) * 1)), dstPix); \
				           );
			}
			else
			{
				MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), \
				           srcPix = vld1q_u8((u8 *)((v128u8 *)src + (X))); \
				           dstPix.val[0] = vzip1q_u8(srcPix, srcPix); \
				           dstPix.val[1] = vzip2q_u8(srcPix, srcPix); \
				           vst1q_u8_x2((u8 *)((v128u8 *)dst + ((X) * 2)), dstPix); \
				           );
			}
		}
		else if (ELEMENTSIZE == 2)
		{
			v128u16 srcPix;
			uint16x8x2_t dstPix;
			
			if (SCALEVERTICAL)
			{
				MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u16) / ELEMENTSIZE), \
				           srcPix = vld1q_u16((u16 *)((v128u16 *)src + (X))); \
				           dstPix.val[0] = vzip1q_u16(srcPix, srcPix); \
				           dstPix.val[1] = vzip2q_u16(srcPix, srcPix); \
				           vst1q_u16_x2((u16 *)((v128u16 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v128u16) / ELEMENTSIZE)) * 0)), dstPix); \
				           vst1q_u16_x2((u16 *)((v128u16 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v128u16) / ELEMENTSIZE)) * 1)), dstPix); \
				           );
			}
			else
			{
				MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u16) / ELEMENTSIZE), \
				           srcPix = vld1q_u16((u16 *)((v128u16 *)src + (X))); \
				           dstPix.val[0] = vzip1q_u16(srcPix, srcPix); \
				           dstPix.val[1] = vzip2q_u16(srcPix, srcPix); \
				           vst1q_u16_x2((u16 *)((v128u16 *)dst + ((X) * 2)), dstPix); \
				           );
			}
		}
		else if (ELEMENTSIZE == 4)
		{
			v128u32 srcPix;
			uint32x4x2_t dstPix;
			
			if (SCALEVERTICAL)
			{
				MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u32) / ELEMENTSIZE), \
				           srcPix = vld1q_u32((u32 *)((v128u32 *)src + (X))); \
				           dstPix.val[0] = vzip1q_u32(srcPix, srcPix); \
				           dstPix.val[1] = vzip2q_u32(srcPix, srcPix); \
				           vst1q_u32_x2((u32 *)((v128u32 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v128u32) / ELEMENTSIZE)) * 0)), dstPix); \
				           vst1q_u32_x2((u32 *)((v128u32 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v128u32) / ELEMENTSIZE)) * 1)), dstPix); \
				           );
			}
			else
			{
				MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u32) / ELEMENTSIZE), \
				           srcPix = vld1q_u32((u32 *)((v128u32 *)src + (X))); \
				           dstPix.val[0] = vzip1q_u32(srcPix, srcPix); \
				           dstPix.val[1] = vzip2q_u32(srcPix, srcPix); \
				           vst1q_u32_x2((u32 *)((v128u32 *)dst + ((X) * 2)), dstPix); \
				           );
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		uint8x16x3_t dstPix;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 0, 0, 1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4, 5}));
				dstPix.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 5, 5, 6, 6, 6, 7, 7, 7, 8, 8, 8, 9, 9, 9,10,10}));
				dstPix.val[2] = vqtbl1q_u8(srcPix, ((v128u8){10,11,11,11,12,12,12,13,13,13,14,14,14,15,15,15}));
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3, 4, 5, 4, 5}));
				dstPix.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 6, 7, 6, 7, 6, 7, 8, 9, 8, 9, 8, 9,10,11}));
				dstPix.val[2] = vqtbl1q_u8(srcPix, ((v128u8){10,11,10,11,12,13,12,13,12,13,14,15,14,15,14,15}));
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7}));
				dstPix.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 6, 7, 4, 5, 6, 7, 8, 9,10,11, 8, 9,10,11}));
				dstPix.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9,10,11,12,13,14,15,12,13,14,15,12,13,14,15}));
			}
			
			vst1q_u8_x3( (u8 *)((v128u8 *)dst + dstX), dstPix );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x3( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly)), dstPix );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		uint8x16x4_t dstPix00;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15}));
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3, 2, 3}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 4, 5, 4, 5, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9, 8, 9, 8, 9, 8, 9,10,11,10,11,10,11,10,11}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){12,13,12,13,12,13,12,13,14,15,14,15,14,15,14,15}));
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix00.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix00.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix00.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
			}
			
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX), dstPix00 );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly)), dstPix00 );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 5)
	{
		uint8x16x4_t dstPix00;
		uint8x16_t dstPix04;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 2, 3}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 3, 3, 3, 3, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 6, 6}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 6, 6, 6, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 9, 9, 9}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 9, 9,10,10,10,10,10,11,11,11,11,11,12,12,12,12}));
				dstPix04        = vqtbl1q_u8(srcPix, ((v128u8){12,13,13,13,13,13,14,14,14,14,14,15,15,15,15,15}));
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3, 2, 3}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 2, 3, 2, 3, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 6, 7}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 6, 7, 6, 7, 6, 7, 6, 7, 8, 9, 8, 9, 8, 9, 8, 9}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9,10,11,10,11,10,11,10,11,10,11,12,13,12,13}));
				dstPix04        = vqtbl1q_u8(srcPix, ((v128u8){12,13,12,13,12,13,14,15,14,15,14,15,14,15,14,15}));
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix00.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 2, 3, 4, 5, 6, 7, 4, 5, 6, 7, 4, 5, 6, 7}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 6, 7, 4, 5, 6, 7, 8, 9,10,11, 8, 9,10,11}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9,10,11, 8, 9,10,11, 8, 9,10,11,12,13,14,15}));
				dstPix04        = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
			}
			
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 0), dstPix00 );
			vst1q_u8(    (u8 *)((v128u8 *)dst + dstX + 4), dstPix04 );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 0), dstPix00 );
					vst1q_u8(    (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 4), dstPix04 );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 6)
	{
		uint8x16x4_t dstPix00;
		uint8x16x2_t dstPix04;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 2, 2, 2, 2}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 2, 2, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4, 4, 4, 5, 5}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 5, 5, 5, 5, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9,10,10,10,10}));
				dstPix04.val[0] = vqtbl1q_u8(srcPix, ((v128u8){10,10,11,11,11,11,11,11,12,12,12,12,12,12,13,13}));
				dstPix04.val[1] = vqtbl1q_u8(srcPix, ((v128u8){13,13,13,13,14,14,14,14,14,14,15,15,15,15,15,15}));
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3, 2, 3}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 2, 3, 2, 3, 2, 3, 2, 3, 4, 5, 4, 5, 4, 5, 4, 5}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 4, 5, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9,10,11,10,11}));
				dstPix04.val[0] = vqtbl1q_u8(srcPix, ((v128u8){10,11,10,11,10,11,10,11,12,13,12,13,12,13,12,13}));
				dstPix04.val[1] = vqtbl1q_u8(srcPix, ((v128u8){12,13,12,13,14,15,14,15,14,15,14,15,14,15,14,15}));
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix00.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7, 4, 5, 6, 7}));
				dstPix00.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix00.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix04.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9,10,11, 8, 9,10,11,12,13,14,15,12,13,14,15}));
				dstPix04.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
			}
			
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 0), dstPix00 );
			vst1q_u8_x2( (u8 *)((v128u8 *)dst + dstX + 4), dstPix04 );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 0), dstPix00 );
					vst1q_u8_x2( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 4), dstPix04 );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 7)
	{
		uint8x16x4_t dstPix00;
		uint8x16x3_t dstPix04;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 2, 2}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 4, 4, 4, 4}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 6, 6, 6, 6, 6, 6}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 6, 7, 7, 7, 7, 7, 7, 7, 8, 8, 8, 8, 8, 8, 8, 9}));
				dstPix04.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 9, 9, 9, 9, 9, 9,10,10,10,10,10,10,10,11,11,11}));
				dstPix04.val[1] = vqtbl1q_u8(srcPix, ((v128u8){11,11,11,11,12,12,12,12,12,12,12,13,13,13,13,13}));
				dstPix04.val[2] = vqtbl1q_u8(srcPix, ((v128u8){13,13,14,14,14,14,14,14,14,15,15,15,15,15,15,15}));
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 2, 3}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 4, 5, 4, 5}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 6, 7, 6, 7, 6, 7}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 6, 7, 6, 7, 6, 7, 6, 7, 8, 9, 8, 9, 8, 9, 8, 9}));
				dstPix04.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9, 8, 9, 8, 9,10,11,10,11,10,11,10,11,10,11}));
				dstPix04.val[1] = vqtbl1q_u8(srcPix, ((v128u8){10,11,10,11,12,13,12,13,12,13,12,13,12,13,12,13}));
				dstPix04.val[2] = vqtbl1q_u8(srcPix, ((v128u8){12,13,14,15,14,15,14,15,14,15,14,15,14,15,14,15}));
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix00.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 1, 2, 3, 0, 1, 2, 3, 0, 1, 2, 3, 4, 5, 6, 7}));
				dstPix00.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 5, 6, 7, 4, 5, 6, 7, 8, 9,10,11, 8, 9,10,11}));
				dstPix04.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix04.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 9,10,11,12,13,14,15,12,13,14,15,12,13,14,15}));
				dstPix04.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
			}
			
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 0), dstPix00 );
			vst1q_u8_x3( (u8 *)((v128u8 *)dst + dstX + 4), dstPix04 );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 0), dstPix00 );
					vst1q_u8_x3( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 4), dstPix04 );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 8)
	{
		uint8x16x4_t dstPix00;
		uint8x16x4_t dstPix04;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix00.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1}));
				dstPix00.val[1] = vqtbl1q_u8(srcPix, ((v128u8){ 2, 2, 2, 2, 2, 2, 2, 2, 3, 3, 3, 3, 3, 3, 3, 3}));
				dstPix00.val[2] = vqtbl1q_u8(srcPix, ((v128u8){ 4, 4, 4, 4, 4, 4, 4, 4, 5, 5, 5, 5, 5, 5, 5, 5}));
				dstPix00.val[3] = vqtbl1q_u8(srcPix, ((v128u8){ 6, 6, 6, 6, 6, 6, 6, 6, 7, 7, 7, 7, 7, 7, 7, 7}));
				dstPix04.val[0] = vqtbl1q_u8(srcPix, ((v128u8){ 8, 8, 8, 8, 8, 8, 8, 8, 9, 9, 9, 9, 9, 9, 9, 9}));
				dstPix04.val[1] = vqtbl1q_u8(srcPix, ((v128u8){10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11}));
				dstPix04.val[2] = vqtbl1q_u8(srcPix, ((v128u8){12,12,12,12,12,12,12,12,13,13,13,13,13,13,13,13}));
				dstPix04.val[3] = vqtbl1q_u8(srcPix, ((v128u8){14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15}));
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix00.val[0] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 0) );
				dstPix00.val[1] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 1) );
				dstPix00.val[2] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 2) );
				dstPix00.val[3] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 3) );
				dstPix04.val[0] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 4) );
				dstPix04.val[1] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 5) );
				dstPix04.val[2] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 6) );
				dstPix04.val[3] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 7) );
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix00.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix00.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix04.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix04.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix04.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
				dstPix04.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
			}
			
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 0), dstPix00 );
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 4), dstPix04 );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 0), dstPix00 );
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 4), dstPix04 );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 16)
	{
		uint8x16x4_t dstPix00;
		uint8x16x4_t dstPix04;
		uint8x16x4_t dstPix08;
		uint8x16x4_t dstPix12;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const v128u8 srcPix = vld1q_u8( (u8 *)((v128u8 *)src + srcX) );
			
			if (ELEMENTSIZE == 1)
			{
				dstPix00.val[0] = vdupq_laneq_u8(srcPix, 0);
				dstPix00.val[1] = vdupq_laneq_u8(srcPix, 1);
				dstPix00.val[2] = vdupq_laneq_u8(srcPix, 2);
				dstPix00.val[3] = vdupq_laneq_u8(srcPix, 3);
				dstPix04.val[0] = vdupq_laneq_u8(srcPix, 4);
				dstPix04.val[1] = vdupq_laneq_u8(srcPix, 5);
				dstPix04.val[2] = vdupq_laneq_u8(srcPix, 6);
				dstPix04.val[3] = vdupq_laneq_u8(srcPix, 7);
				dstPix08.val[0] = vdupq_laneq_u8(srcPix, 8);
				dstPix08.val[1] = vdupq_laneq_u8(srcPix, 9);
				dstPix08.val[2] = vdupq_laneq_u8(srcPix,10);
				dstPix08.val[3] = vdupq_laneq_u8(srcPix,11);
				dstPix12.val[0] = vdupq_laneq_u8(srcPix,12);
				dstPix12.val[1] = vdupq_laneq_u8(srcPix,13);
				dstPix12.val[2] = vdupq_laneq_u8(srcPix,14);
				dstPix12.val[3] = vdupq_laneq_u8(srcPix,15);
			}
			else if (ELEMENTSIZE == 2)
			{
				dstPix00.val[0] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 0) );
				dstPix00.val[1] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 0) );
				dstPix00.val[2] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 1) );
				dstPix00.val[3] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 1) );
				dstPix04.val[0] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 2) );
				dstPix04.val[1] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 2) );
				dstPix04.val[2] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 3) );
				dstPix04.val[3] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 3) );
				dstPix08.val[0] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 4) );
				dstPix08.val[1] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 4) );
				dstPix08.val[2] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 5) );
				dstPix08.val[3] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 5) );
				dstPix12.val[0] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 6) );
				dstPix12.val[1] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 6) );
				dstPix12.val[2] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 7) );
				dstPix12.val[3] = vreinterpretq_u8_u16( vdupq_laneq_u16(vreinterpretq_u16_u8(srcPix), 7) );
			}
			else if (ELEMENTSIZE == 4)
			{
				dstPix00.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix00.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 0) );
				dstPix04.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix04.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix04.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix04.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 1) );
				dstPix08.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix08.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix08.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix08.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 2) );
				dstPix12.val[0] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
				dstPix12.val[1] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
				dstPix12.val[2] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
				dstPix12.val[3] = vreinterpretq_u8_u32( vdupq_laneq_u32(vreinterpretq_u32_u8(srcPix), 3) );
			}
			
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 0), dstPix00 );
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 4), dstPix04 );
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + 8), dstPix08 );
			vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX +12), dstPix12 );
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 0), dstPix00 );
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 4), dstPix04 );
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + 8), dstPix08 );
					vst1q_u8_x4( (u8 *)((v128u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) +12), dstPix12 );
				}
			}
		}
	}
	else if (INTEGERSCALEHINT > 1)
	{
		const size_t scale = dstWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE); srcX++, dstX+=scale)
		{
			const v128u8 srcVec = vld1q_u8((u8 *)((v128u8 *)src + srcX));
			v128u8 ssse3idx;
			
			for (size_t lx = 0; lx < scale; lx++)
			{
				if (ELEMENTSIZE == 1)
				{
					ssse3idx = vld1q_u8((u8 *)((v128u8 *)(_gpuDstToSrcSSSE3_u8_16e + (lx * sizeof(v128u8)))));
				}
				else if (ELEMENTSIZE == 2)
				{
					ssse3idx = vld1q_u8((u8 *)((v128u8 *)(_gpuDstToSrcSSSE3_u16_8e + (lx * sizeof(v128u8)))));
				}
				else if (ELEMENTSIZE == 4)
				{
					ssse3idx = vld1q_u8((u8 *)((v128u8 *)(_gpuDstToSrcSSSE3_u32_4e + (lx * sizeof(v128u8)))));
				}
				
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX + lx), vqtbl1q_u8(srcVec, ssse3idx) );
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
	if (INTEGERSCALEHINT == 0)
	{
		memcpy(dst, src, srcWidth * ELEMENTSIZE);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE), vst1q_u8((u8 *)((v128u8 *)dst + (X)), vld1q_u8((u8 *)((v128u8 *)src + (X)))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE)); dstX++)
		{
			if (ELEMENTSIZE == 1)
			{
				const uint8x16x2_t srcPix = vld1q_u8_x2( (u8 *)((v128u8 *)src + (dstX * 2)) );
				vst1q_u8((u8 *)((v128u8 *)dst + dstX), vuzp1q_u8(srcPix.val[0], srcPix.val[1]));
			}
			else if (ELEMENTSIZE == 2)
			{
				const uint16x8x2_t srcPix = vld1q_u16_x2( (u16 *)((v128u16 *)src + (dstX * 2)) );
				vst1q_u16((u16 *)((v128u16 *)dst + dstX), vuzp1q_u16(srcPix.val[0], srcPix.val[1]));
			}
			else if (ELEMENTSIZE == 4)
			{
				const uint32x4x2_t srcPix = vld1q_u32_x2( (u32 *)((v128u32 *)src + (dstX * 2)) );
				vst1q_u32((u32 *)((v128u32 *)dst + dstX), vuzp1q_u32(srcPix.val[0], srcPix.val[1]));
				/*
				// Pixel minification algorithm that takes the average value of each color component of the 2x2 pixel group.
				      uint8x16x4_t srcPix0 = vld4q_u8( (u8 *)((v128u8 *)src + (dstX * 4) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * 2) * 0)) );
				const uint8x16x4_t srcPix1 = vld4q_u8( (u8 *)((v128u8 *)src + (dstX * 4) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * 2) * 1)) );
				srcPix0.val[0] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[0]) );
				srcPix0.val[1] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[1]) );
				srcPix0.val[2] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[2]) );
				srcPix0.val[3] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[3]) );
				
				srcPix0.val[0] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[0]), srcPix1.val[0] );
				srcPix0.val[1] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[1]), srcPix1.val[1] );
				srcPix0.val[2] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[2]), srcPix1.val[2] );
				srcPix0.val[3] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[3]), srcPix1.val[3] );
				
				srcPix0.val[0] = vshrq_n_u16( vreinterpretq_u16_u8(srcPix0.val[0]), 2 );
				srcPix0.val[1] = vshrq_n_u16( vreinterpretq_u16_u8(srcPix0.val[1]), 2 );
				srcPix0.val[2] = vshrq_n_u16( vreinterpretq_u16_u8(srcPix0.val[2]), 2 );
				srcPix0.val[3] = vshrq_n_u16( vreinterpretq_u16_u8(srcPix0.val[3]), 2 );
				
				const uint8x16x2_t dstPix = {
					vqtbl4q_u8( srcPix0, ((v128u8){ 0,16,32,48, 2,18,34,50, 4,20,36,52, 6,22,38,54}) ),
					vqtbl4q_u8( srcPix0, ((v128u8){ 8,24,40,56,10,26,42,58,12,28,44,60,14,30,46,62}) )
				};
				vst1q_u8_x2( (u8 *)((v128u8 *)dst + (dstX * 2)), dstPix);
				*/
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		uint8x16x3_t srcPix;
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE)); dstX++)
		{
			srcPix = vld1q_u8_x3( (u8 *)((v128u8 *)src + (dstX * 3)) );
			
			if (ELEMENTSIZE == 1)
			{
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), vqtbl3q_u8( srcPix, ((v128u8){ 0, 3, 6, 9,12,15,18,21,24,27,30,33,36,39,42,45}) ) );
			}
			else if (ELEMENTSIZE == 2)
			{
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), vqtbl3q_u8( srcPix, ((v128u8){ 0, 1, 6, 7,12,13,18,19,24,25,30,31,36,37,42,43}) ) );
			}
			else if (ELEMENTSIZE == 4)
			{
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), vqtbl3q_u8( srcPix, ((v128u8){ 0, 1, 2, 3,12,13,14,15,24,25,26,27,36,37,38,39}) ) );
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		uint8x16x4_t srcPix0;
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE)); dstX++)
		{
			if (ELEMENTSIZE == 1)
			{
				srcPix0 = vld1q_u8_x4( (u8 *)((v128u8 *)src + (dstX * 4)) );
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), vqtbl4q_u8( srcPix0, ((v128u8){ 0, 4, 8,12,16,20,24,28,32,36,40,44,48,52,56,60}) ) );
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix0 = vld1q_u8_x4( (u8 *)((v128u8 *)src + (dstX * 4)) );
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), vqtbl4q_u8( srcPix0, ((v128u8){ 0, 1, 8, 9,16,17,24,25,32,33,40,41,48,49,56,57}) ) );
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix0 = vld1q_u8_x4( (u8 *)((v128u8 *)src + (dstX * 4)) );
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), vqtbl4q_u8( srcPix0, ((v128u8){ 0, 1, 2, 3,16,17,18,19,32,33,34,35,48,49,50,51}) ) );
				/*
				// Pixel minification algorithm that takes the average value of each color component of the 4x4 pixel group.
				                   srcPix0 = vld4q_u8( (u8 *)((v128u8 *)src + (dstX * 4) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * 4) * 0)) );
				const uint8x16x4_t srcPix1 = vld4q_u8( (u8 *)((v128u8 *)src + (dstX * 4) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * 4) * 1)) );
				const uint8x16x4_t srcPix2 = vld4q_u8( (u8 *)((v128u8 *)src + (dstX * 4) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * 4) * 2)) );
				const uint8x16x4_t srcPix3 = vld4q_u8( (u8 *)((v128u8 *)src + (dstX * 4) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v128u8) / ELEMENTSIZE) * 4) * 3)) );
				
				srcPix0.val[0] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[0]) );
				srcPix0.val[1] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[1]) );
				srcPix0.val[2] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[2]) );
				srcPix0.val[3] = vreinterpretq_u8_u16( vpaddlq_u8(srcPix0.val[3]) );
				
				srcPix0.val[0] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[0]), srcPix1.val[0] );
				srcPix0.val[1] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[1]), srcPix1.val[1] );
				srcPix0.val[2] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[2]), srcPix1.val[2] );
				srcPix0.val[3] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[3]), srcPix1.val[3] );
				
				srcPix0.val[0] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[0]), srcPix2.val[0] );
				srcPix0.val[1] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[1]), srcPix2.val[1] );
				srcPix0.val[2] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[2]), srcPix2.val[2] );
				srcPix0.val[3] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[3]), srcPix2.val[3] );
				
				srcPix0.val[0] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[0]), srcPix3.val[0] );
				srcPix0.val[1] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[1]), srcPix3.val[1] );
				srcPix0.val[2] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[2]), srcPix3.val[2] );
				srcPix0.val[3] = vpadalq_u8( vreinterpretq_u16_u8(srcPix0.val[3]), srcPix3.val[3] );
				
				srcPix0.val[0] = vreinterpretq_u8_u32( vpaddlq_u16(vreinterpretq_u16_u8(srcPix0.val[0])) );
				srcPix0.val[1] = vreinterpretq_u8_u32( vpaddlq_u16(vreinterpretq_u16_u8(srcPix0.val[1])) );
				srcPix0.val[2] = vreinterpretq_u8_u32( vpaddlq_u16(vreinterpretq_u16_u8(srcPix0.val[2])) );
				srcPix0.val[3] = vreinterpretq_u8_u32( vpaddlq_u16(vreinterpretq_u16_u8(srcPix0.val[3])) );
				
				srcPix0.val[0] = vshrq_n_u32( vreinterpretq_u32_u8(srcPix0.val[0]), 4 );
				srcPix0.val[1] = vshrq_n_u32( vreinterpretq_u32_u8(srcPix0.val[1]), 4 );
				srcPix0.val[2] = vshrq_n_u32( vreinterpretq_u32_u8(srcPix0.val[2]), 4 );
				srcPix0.val[3] = vshrq_n_u32( vreinterpretq_u32_u8(srcPix0.val[3]), 4 );
				
				const v128u8 dstPix = vqtbl4q_u8( srcPix0, ((v128u8){ 0,16,32,48, 4,20,36,52, 8,24,40,56,12,28,44,60}) );
				vst1q_u8( (u8 *)((v128u8 *)dst + dstX), dstPix);
				*/
			}
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

FORCEINLINE v128u16 ColorOperation_NEON::blend(const v128u16 &colA, const v128u16 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const
{
	v128u16 ra;
	v128u16 ga;
	v128u16 ba;
	static const v128u16 colorBitMask = vdupq_n_u16(0x001F);
	
	ra = vandq_u16(            colA,      colorBitMask);
	ga = vandq_u16(vshrq_n_u16(colA,  5), colorBitMask);
	ba = vandq_u16(vshrq_n_u16(colA, 10), colorBitMask);
	
	v128u16 rb = vandq_u16(            colB,      colorBitMask);
	v128u16 gb = vandq_u16(vshrq_n_u16(colB,  5), colorBitMask);
	v128u16 bb = vandq_u16(vshrq_n_u16(colB, 10), colorBitMask);
	
	ra = vmlaq_u16( vmulq_u16(ra, blendEVA), rb, blendEVB );
	ga = vmlaq_u16( vmulq_u16(ga, blendEVA), gb, blendEVB );
	ba = vmlaq_u16( vmulq_u16(ba, blendEVA), bb, blendEVB );
	
	ra = vshrq_n_u16(ra, 4);
	ga = vshrq_n_u16(ga, 4);
	ba = vshrq_n_u16(ba, 4);
	
	ra = vminq_u16(ra, colorBitMask);
	ga = vminq_u16(ga, colorBitMask);
	ba = vminq_u16(ba, colorBitMask);
	
	return vorrq_u16(ra, vorrq_u16( vshlq_n_u16(ga, 5), vshlq_n_u16(ba, 10)) );
}

// Note that if USECONSTANTBLENDVALUESHINT is true, then this method will assume that blendEVA contains identical values
// for each 16-bit vector element, and also that blendEVB contains identical values for each 16-bit vector element. If
// this assumption is broken, then the resulting color will be undefined.
template <NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT>
FORCEINLINE v128u32 ColorOperation_NEON::blend(const v128u32 &colA, const v128u32 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const
{
	v128u16 outColorLo;
	v128u16 outColorHi;
	v128u32 outColor;
	
	if (USECONSTANTBLENDVALUESHINT)
	{
		const v128u8 blendA = vdupq_laneq_u8(vreinterpretq_u8_u16(blendEVA), 0);
		const v128u8 blendB = vdupq_laneq_u8(vreinterpretq_u8_u16(blendEVB), 0);
		outColorLo = vmlal_u8( vmull_u8(vget_low_u8( vreinterpretq_u8_u32(colA)), vget_low_u8(blendA)), vget_low_u8( vreinterpretq_u8_u32(colB)), vget_low_u8(blendB) );
		outColorHi = vmlal_high_u8( vmull_high_u8(vreinterpretq_u8_u32(colA), blendA), vreinterpretq_u8_u32(colB), blendB );
	}
	else
	{
		const v128u8 blendA = vqtbl1q_u8(vreinterpretq_u8_u16(blendEVA), ((v128u8){ 0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8,12,12,12,12}));
		const v128u8 blendB = vqtbl1q_u8(vreinterpretq_u8_u16(blendEVB), ((v128u8){ 0, 0, 0, 0, 4, 4, 4, 4, 8, 8, 8, 8,12,12,12,12}));
		outColorLo = vmlal_u8( vmull_u8(vget_low_u8( vreinterpretq_u8_u32(colA)), vget_low_u8(blendA)), vget_low_u8( vreinterpretq_u8_u32(colB)), vget_low_u8(blendB) );
		outColorHi = vmlal_high_u8( vmull_high_u8(vreinterpretq_u8_u32(colA), blendA), vreinterpretq_u8_u32(colB), blendB );
	}
	
	outColorLo = vshrq_n_u16(outColorLo, 4);
	outColorHi = vshrq_n_u16(outColorHi, 4);
	
	if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		outColorLo = vminq_u16(outColorLo, vdupq_n_u16(255));
		outColorHi = vminq_u16(outColorHi, vdupq_n_u16(255));
	}
	
	outColor = vreinterpretq_u32_u8( vuzp1q_u8( vreinterpretq_u8_u16(outColorLo), vreinterpretq_u8_u16(outColorHi) ) );
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		outColor = vreinterpretq_u32_u8( vminq_u8(vreinterpretq_u8_u32(outColor), vdupq_n_u8(63)) );
	}
	
	outColor = vandq_u32(outColor, vdupq_n_u32(0x00FFFFFF));
	
	return outColor;
}

FORCEINLINE v128u16 ColorOperation_NEON::blend3D(const v128u32 &colA_Lo, const v128u32 &colA_Hi, const v128u16 &colB) const
{
	static const u8 X = 0x80;
	const uint8x16x2_t col = { vreinterpretq_u8_u32(colA_Lo), vreinterpretq_u8_u32(colA_Hi) };
	
	// If the color format of B is 555, then the colA_Hi parameter is required.
	// The color format of A is assumed to be RGB666.
	v128u16 ra = vreinterpretq_u16_u8( vqtbl2q_u8( col, ((v128u8){ 0, X, 4, X, 8, X,12, X,16, X,20, X,24, X,28, X}) ) );
	v128u16 ga = vreinterpretq_u16_u8( vqtbl2q_u8( col, ((v128u8){ 1, X, 5, X, 9, X,13, X,17, X,21, X,25, X,29, X}) ) );
	v128u16 ba = vreinterpretq_u16_u8( vqtbl2q_u8( col, ((v128u8){ 2, X, 6, X,10, X,14, X,18, X,22, X,26, X,30, X}) ) );
	v128u16 aa = vreinterpretq_u16_u8( vqtbl2q_u8( col, ((v128u8){ 3, X, 7, X,11, X,15, X,19, X,23, X,27, X,31, X}) ) );
	aa = vqaddq_u16(aa, vdupq_n_u16(1));
	
	const v128u16 rb = vandq_u16( vshlq_n_u16(colB, 1), vdupq_n_u16(0x003E) );
	const v128u16 gb = vandq_u16( vshrq_n_u16(colB, 4), vdupq_n_u16(0x003E) );
	const v128u16 bb = vandq_u16( vshrq_n_u16(colB, 9), vdupq_n_u16(0x003E) );
	const v128u16 ab = vqsubq_u16( vdupq_n_u16(32), aa );
	
	ra = vmlaq_u16( vmulq_u16(ra, aa), rb, ab );
	ga = vmlaq_u16( vmulq_u16(ga, aa), gb, ab );
	ba = vmlaq_u16( vmulq_u16(ba, aa), bb, ab );
	
	ra = vshrq_n_u16(ra, 6);
	ga = vshrq_n_u16(ga, 6);
	ba = vshrq_n_u16(ba, 6);
	
	return vorrq_u16( vorrq_u16(ra, vshlq_n_u16(ga, 5)), vshlq_n_u16(ba, 10) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_NEON::blend3D(const v128u32 &colA, const v128u32 &colB) const
{
	static const u8 X = 0x80;
	
	// If the color format of B is 666 or 888, then the colA_Hi parameter is ignored.
	// The color format of A is assumed to match the color format of B.
	v128u16 rgbALo;
	v128u16 rgbAHi;
	
	v128u8 alpha = vqtbl1q_u8( vreinterpretq_u8_u32(colA), ((v128u8){ 3, 3, 3, X, 7, 7, 7, X,11,11,11, X,15,15,15, X}) );
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		alpha = vaddq_u8(alpha, vdupq_n_u8(1));
		const v128u8 invAlpha = vsubq_u8(vdupq_n_u8(32), alpha);
		rgbALo = vmlal_u8( vmull_u8( vget_low_u8(vreinterpretq_u8_u32(colA)), vget_low_u8(alpha) ), vget_low_u8(vreinterpretq_u8_u32(colB)), vget_low_u8(invAlpha) );
		rgbAHi = vmlal_high_u8( vmull_high_u8(vreinterpretq_u8_u32(colA), alpha), vreinterpretq_u8_u32(colB), invAlpha );
	}
	else
	{
		const v128u8 zeroVec = vdupq_n_u8(0);
		rgbALo = vreinterpretq_u16_u8( vzip1q_u8(vreinterpretq_u8_u32(colA), zeroVec) );
		rgbAHi = vreinterpretq_u16_u8( vzip2q_u8(vreinterpretq_u8_u32(colA), zeroVec) );
		v128u16 rgbBLo = vreinterpretq_u16_u8( vzip1q_u8(vreinterpretq_u8_u32(colB), zeroVec) );
		v128u16 rgbBHi = vreinterpretq_u16_u8( vzip2q_u8(vreinterpretq_u8_u32(colB), zeroVec) );
		
		v128u16 alphaLo = vreinterpretq_u16_u8( vzip1q_u8(alpha, zeroVec) );
		v128u16 alphaHi = vreinterpretq_u16_u8( vzip2q_u8(alpha, zeroVec) );
		alphaLo = vaddq_u16(alphaLo, vdupq_n_u16(1));
		alphaHi = vaddq_u16(alphaHi, vdupq_n_u16(1));
		const v128u16 invAlphaLo = vsubq_u16(vdupq_n_u16(256), alphaLo);
		const v128u16 invAlphaHi = vsubq_u16(vdupq_n_u16(256), alphaHi);
		
		rgbALo = vmlaq_u16( vmulq_u16(rgbALo, alphaLo), rgbBLo, invAlphaLo );
		rgbAHi = vmlaq_u16( vmulq_u16(rgbAHi, alphaHi), rgbBHi, invAlphaHi );
	}
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		rgbALo = vshrq_n_u16(rgbALo, 5);
		rgbAHi = vshrq_n_u16(rgbAHi, 5);
	}
	else
	{
		rgbALo = vshrq_n_u16(rgbALo, 8);
		rgbAHi = vshrq_n_u16(rgbAHi, 8);
	}
	
	return vandq_u32( vreinterpretq_u32_u8( vuzp1q_u8(vreinterpretq_u8_u16(rgbALo), vreinterpretq_u8_u16(rgbAHi)) ), vdupq_n_u32(0x00FFFFFF) );
}

FORCEINLINE v128u16 ColorOperation_NEON::increase(const v128u16 &col, const v128u16 &blendEVY) const
{
	v128u16 r = vandq_u16(             col,      vdupq_n_u16(0x001F) );
	v128u16 g = vandq_u16( vshrq_n_u16(col,  5), vdupq_n_u16(0x001F) );
	v128u16 b = vandq_u16( vshrq_n_u16(col, 10), vdupq_n_u16(0x001F) );
	
	r = vsraq_n_u16( r, vmulq_u16(vsubq_u16(vdupq_n_u16(31), r), blendEVY), 4 );
	g = vsraq_n_u16( g, vmulq_u16(vsubq_u16(vdupq_n_u16(31), g), blendEVY), 4 );
	b = vsraq_n_u16( b, vmulq_u16(vsubq_u16(vdupq_n_u16(31), b), blendEVY), 4 );
	
	return vorrq_u16(r, vorrq_u16( vshlq_n_u16(g, 5), vshlq_n_u16(b, 10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_NEON::increase(const v128u32 &col, const v128u16 &blendEVY) const
{
	static const v128u8 zeroVec = vdupq_n_u8(0);
	v128u16 rgbLo = vreinterpretq_u16_u8( vzip1q_u8(vreinterpretq_u8_u32(col), zeroVec) );
	v128u16 rgbHi = vreinterpretq_u16_u8( vzip2q_u8(vreinterpretq_u8_u32(col), zeroVec) );
	
	rgbLo = vsraq_n_u16( rgbLo, vmulq_u16(vsubq_u16(vdupq_n_u16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbLo), blendEVY), 4 );
	rgbHi = vsraq_n_u16( rgbHi, vmulq_u16(vsubq_u16(vdupq_n_u16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbHi), blendEVY), 4 );
	
	return vandq_u32( vreinterpretq_u32_u8( vuzp1q_u8(vreinterpretq_u8_u16(rgbLo), vreinterpretq_u8_u16(rgbHi)) ), vdupq_n_u32(0x00FFFFFF) );
}

FORCEINLINE v128u16 ColorOperation_NEON::decrease(const v128u16 &col, const v128u16 &blendEVY) const
{
	v128u16 r = vandq_u16(             col,      vdupq_n_u16(0x001F) );
	v128u16 g = vandq_u16( vshrq_n_u16(col,  5), vdupq_n_u16(0x001F) );
	v128u16 b = vandq_u16( vshrq_n_u16(col, 10), vdupq_n_u16(0x001F) );
	
	r = vsubq_u16( r, vshrq_n_u16(vmulq_u16(r, blendEVY), 4) );
	g = vsubq_u16( g, vshrq_n_u16(vmulq_u16(g, blendEVY), 4) );
	b = vsubq_u16( b, vshrq_n_u16(vmulq_u16(b, blendEVY), 4) );
	
	return vorrq_u16(r, vorrq_u16( vshlq_n_u16(g, 5), vshlq_n_u16(b, 10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_NEON::decrease(const v128u32 &col, const v128u16 &blendEVY) const
{
	static const v128u8 zeroVec = vdupq_n_u8(0);
	v128u16 rgbLo = vreinterpretq_u16_u8( vzip1q_u8(vreinterpretq_u8_u32(col), zeroVec) );
	v128u16 rgbHi = vreinterpretq_u16_u8( vzip2q_u8(vreinterpretq_u8_u32(col), zeroVec) );
	
	rgbLo = vsubq_u16( rgbLo, vshrq_n_u16(vmulq_u16(rgbLo, blendEVY), 4) );
	rgbHi = vsubq_u16( rgbHi, vshrq_n_u16(vmulq_u16(rgbHi, blendEVY), 4) );
	
	return vandq_u32( vreinterpretq_u32_u8( vuzp1q_u8(vreinterpretq_u8_u16(rgbLo), vreinterpretq_u8_u16(rgbHi)) ), vdupq_n_u32(0x00FFFFFF) );
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_NEON::_copy16(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t dst16 = {
			vorrq_u16(src0, alphaBits),
			vorrq_u16(src1, alphaBits)
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		uint32x4x4_t dst32;
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo6665Opaque_NEON<false>(src0, dst32.val[0], dst32.val[1]);
			ColorspaceConvert555xTo6665Opaque_NEON<false>(src1, dst32.val[2], dst32.val[3]);
		}
		else
		{
			ColorspaceConvert555xTo8888Opaque_NEON<false>(src0, dst32.val[0], dst32.val[1]);
			ColorspaceConvert555xTo8888Opaque_NEON<false>(src1, dst32.val[2], dst32.val[3]);
		}
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	if (!ISDEBUGRENDER)
	{
		vst1q_u8(compInfo.target.lineLayerID, srcLayerID);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_NEON::_copy32(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t dst16 = {
			vorrq_u16( ColorspaceConvert6665To5551_NEON<false>(src0, src1), alphaBits),
			vorrq_u16( ColorspaceConvert6665To5551_NEON<false>(src2, src3), alphaBits)
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const v128u32 alphaBits = vdupq_n_u32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		const uint32x4x4_t dst32 = {
			vorrq_u32(src0, alphaBits),
			vorrq_u32(src1, alphaBits),
			vorrq_u32(src2, alphaBits),
			vorrq_u32(src3, alphaBits)
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	if (!ISDEBUGRENDER)
	{
		vst1q_u8(compInfo.target.lineLayerID, srcLayerID);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_NEON::_copyMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	const uint16x8x2_t passMask16 = {
		vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
		vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t oldColor16 = vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(src0, alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(src1, alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		uint32x4x4_t src32;
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo6665Opaque_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo6665Opaque_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		else
		{
			ColorspaceConvert555xTo8888Opaque_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo8888Opaque_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[1], passMask16.val[1]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[1], passMask16.val[1]) )
		};
		
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], src32.val[0], oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], src32.val[1], oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], src32.val[2], oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], src32.val[3], oldColor32.val[3])
		};
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	if (!ISDEBUGRENDER)
	{
		const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
		vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_NEON::_copyMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	const uint16x8x2_t passMask16 = {
		vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
		vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const uint16x8x2_t src16 = {
			ColorspaceConvert6665To5551_NEON<false>(src0, src1),
			ColorspaceConvert6665To5551_NEON<false>(src2, src3)
		};
		
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t oldColor16 = vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(src16.val[0], alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(src16.val[1], alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[1], passMask16.val[1]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[1], passMask16.val[1]) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(src0, alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(src1, alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(src2, alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(src3, alphaBits), oldColor32.val[3])
		};
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	if (!ISDEBUGRENDER)
	{
		const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
		vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
	}
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessUp16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t dst16 = {
			vorrq_u16(colorop_vec.increase(src0, evy16), alphaBits),
			vorrq_u16(colorop_vec.increase(src1, evy16), alphaBits)
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		uint32x4x4_t src32;
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo666x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo888x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		
		const uint32x4x4_t dst32 = {
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src32.val[0], evy16), alphaBits ),
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src32.val[1], evy16), alphaBits ),
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src32.val[2], evy16), alphaBits ),
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src32.val[3], evy16), alphaBits )
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	vst1q_u8(compInfo.target.lineLayerID, srcLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessUp32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t src16 = {
			ColorspaceConvert6665To5551_NEON<false>(src0, src1),
			ColorspaceConvert6665To5551_NEON<false>(src2, src3)
		};
		const uint16x8x2_t dst16 = {
			vorrq_u16( colorop_vec.increase(src16.val[0], evy16), alphaBits ),
			vorrq_u16( colorop_vec.increase(src16.val[1], evy16), alphaBits )
		};
		
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		
		const uint32x4x4_t dst32 = {
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits ),
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits ),
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits ),
			vorrq_u32( colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits )
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	vst1q_u8(compInfo.target.lineLayerID, srcLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessUpMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	const uint16x8x2_t passMask16 = {
		vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
		vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t oldColor16 =  vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(colorop_vec.increase(src0, evy16), alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(colorop_vec.increase(src1, evy16), alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		uint32x4x4_t src32;
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo666x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo888x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[1], passMask16.val[1]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[1], passMask16.val[1]) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src32.val[0], evy16), alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src32.val[1], evy16), alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src32.val[2], evy16), alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src32.val[3], evy16), alphaBits), oldColor32.val[3])
		};
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
	vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessUpMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	const uint16x8x2_t passMask16 = {
		vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
		vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const uint16x8x2_t src16 = {
			ColorspaceConvert6665To5551_NEON<false>(src0, src1),
			ColorspaceConvert6665To5551_NEON<false>(src2, src3)
		};
		
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t oldColor16 =  vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(colorop_vec.increase(src16.val[0], evy16), alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(colorop_vec.increase(src16.val[1], evy16), alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[1], passMask16.val[1]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[1], passMask16.val[1]) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits), oldColor32.val[3])
		};
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
	vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessDown16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t dst16 = {
			vorrq_u16(colorop_vec.decrease(src0, evy16), alphaBits),
			vorrq_u16(colorop_vec.decrease(src1, evy16), alphaBits)
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		uint32x4x4_t src32;
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo666x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo888x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		
		const uint32x4x4_t dst32 = {
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src32.val[0], evy16), alphaBits ),
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src32.val[1], evy16), alphaBits ),
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src32.val[2], evy16), alphaBits ),
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src32.val[3], evy16), alphaBits )
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	vst1q_u8(compInfo.target.lineLayerID, srcLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessDown32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t src16 = {
			ColorspaceConvert6665To5551_NEON<false>(src0, src1),
			ColorspaceConvert6665To5551_NEON<false>(src2, src3)
		};
		const uint16x8x2_t dst16 = {
			vorrq_u16( colorop_vec.decrease(src16.val[0], evy16), alphaBits ),
			vorrq_u16( colorop_vec.decrease(src16.val[1], evy16), alphaBits )
		};
		
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		
		const uint32x4x4_t dst32 = {
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits ),
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits ),
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits ),
			vorrq_u32( colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits )
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	vst1q_u8(compInfo.target.lineLayerID, srcLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessDownMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	const uint16x8x2_t passMask16 = {
		vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
		vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t oldColor16 =  vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(colorop_vec.decrease(src0, evy16), alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(colorop_vec.decrease(src1, evy16), alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		uint32x4x4_t src32;
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555xTo666x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo666x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		else
		{
			ColorspaceConvert555xTo888x_NEON<false>(src0, src32.val[0], src32.val[1]);
			ColorspaceConvert555xTo888x_NEON<false>(src1, src32.val[2], src32.val[3]);
		}
		
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[1], passMask16.val[1]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[1], passMask16.val[1]) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src32.val[0], evy16), alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src32.val[1], evy16), alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src32.val[2], evy16), alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src32.val[3], evy16), alphaBits), oldColor32.val[3])
		};
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
	vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );

}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_NEON::_brightnessDownMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	const uint16x8x2_t passMask16 = {
		vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
		vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const uint16x8x2_t src16 = {
			ColorspaceConvert6665To5551_NEON<false>(src0, src1),
			ColorspaceConvert6665To5551_NEON<false>(src2, src3)
		};
		
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t oldColor16 =  vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(colorop_vec.decrease(src16.val[0], evy16), alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(colorop_vec.decrease(src16.val[1], evy16), alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[0], passMask16.val[0]) ),
			vreinterpretq_u32_u16( vzip1q_u16(passMask16.val[1], passMask16.val[1]) ),
			vreinterpretq_u32_u16( vzip2q_u16(passMask16.val[1], passMask16.val[1]) )
		};
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000);
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits), oldColor32.val[3])
		};
		
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
	
	const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
	vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_NEON::_unknownEffectMask16(GPUEngineCompositorInfo &compInfo,
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
	static uint8x16_t zeroVec = ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
	vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
	
	v128u8 dstTargetBlendEnableMask = vqtbl1q_u8(dstBlendEnableMaskLUT, dstLayerID);
	dstTargetBlendEnableMask = vandq_u8( vmvnq_u8(vceqq_u8(dstLayerID, srcLayerID)), dstTargetBlendEnableMask );
	
	v128u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : zeroVec;
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	v128u8 eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? vdupq_n_u8(compInfo.renderState.blendEVA) : vreinterpretq_u8_u16( vdupq_n_u16(compInfo.renderState.blendEVA) );
	v128u8 evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? vdupq_n_u8(compInfo.renderState.blendEVB) : vreinterpretq_u8_u16( vdupq_n_u16(compInfo.renderState.blendEVB) );
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v128u8 isObjTranslucentMask = vandq_u8( dstTargetBlendEnableMask, vorrq_u8(vceqq_u8(spriteMode, vdupq_n_u8(OBJMode_Transparent)), vceqq_u8(spriteMode, vdupq_n_u8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v128u8 spriteAlphaMask = vandq_u8( vmvnq_u8(vceqq_u8(spriteAlpha, vdupq_n_u8(0xFF))), isObjTranslucentMask );
		eva_vec128 = vbslq_u8(spriteAlphaMask, spriteAlpha, eva_vec128);
		evb_vec128 = vbslq_u8(spriteAlphaMask, vsubq_u8(vdupq_n_u8(16), spriteAlpha), evb_vec128);
	}
	
	// Select the color effect based on the BLDCNT target flags.
	const v128u8 colorEffect_vec128 = vbslq_u8(enableColorEffectMask, vdupq_n_u8(compInfo.renderState.colorEffect), vdupq_n_u8(ColorEffect_Disable));
	
	// ----------
	
	uint16x8x2_t tmpSrc16;
	uint32x4x4_t tmpSrc32;
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc16.val[0] = src0;
		tmpSrc16.val[1] = src1;
	}
	else if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
	{
		ColorspaceConvert555xTo666x_NEON<false>(src0, tmpSrc32.val[0], tmpSrc32.val[1]);
		ColorspaceConvert555xTo666x_NEON<false>(src1, tmpSrc32.val[2], tmpSrc32.val[3]);
	}
	else
	{
		ColorspaceConvert555xTo888x_NEON<false>(src0, tmpSrc32.val[0], tmpSrc32.val[1]);
		ColorspaceConvert555xTo888x_NEON<false>(src1, tmpSrc32.val[2], tmpSrc32.val[3]);
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v128u8 brightnessMask8 = vandq_u8( vmvnq_u8(forceDstTargetBlendMask), vandq_u8(srcEffectEnableMask, vceqq_u8(colorEffect_vec128, vdupq_n_u8(ColorEffect_IncreaseBrightness))) );
			const u8 brightnessUpMaskValue = vmaxvq_u8(brightnessMask8);
			
			if (brightnessUpMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const uint16x8x2_t brightnessMask16 = {
						vreinterpretq_u16_u8( vzip1q_u8(brightnessMask8, brightnessMask8) ),
						vreinterpretq_u16_u8( vzip2q_u8(brightnessMask8, brightnessMask8) )
					};
					
					tmpSrc16.val[0] = vbslq_u16( brightnessMask16.val[0], colorop_vec.increase(tmpSrc16.val[0], evy16), tmpSrc16.val[0] );
					tmpSrc16.val[1] = vbslq_u16( brightnessMask16.val[1], colorop_vec.increase(tmpSrc16.val[1], evy16), tmpSrc16.val[1] );
				}
				else
				{
					const uint32x4x4_t brightnessMask32 = {
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15})) )
					};
					
					tmpSrc32.val[0] = vbslq_u32( brightnessMask32.val[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[0], evy16), tmpSrc32.val[0] );
					tmpSrc32.val[1] = vbslq_u32( brightnessMask32.val[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[1], evy16), tmpSrc32.val[1] );
					tmpSrc32.val[2] = vbslq_u32( brightnessMask32.val[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[2], evy16), tmpSrc32.val[2] );
					tmpSrc32.val[3] = vbslq_u32( brightnessMask32.val[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[3], evy16), tmpSrc32.val[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v128u8 brightnessMask8 = vandq_u8( vmvnq_u8(forceDstTargetBlendMask), vandq_u8(srcEffectEnableMask, vceqq_u8(colorEffect_vec128, vdupq_n_u8(ColorEffect_DecreaseBrightness))) );
			const u8 brightnessDownMaskValue = vmaxvq_u8(brightnessMask8);
			
			if (brightnessDownMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const uint16x8x2_t brightnessMask16 = {
						vreinterpretq_u16_u8( vzip1q_u8(brightnessMask8, brightnessMask8) ),
						vreinterpretq_u16_u8( vzip2q_u8(brightnessMask8, brightnessMask8) )
					};
					
					tmpSrc16.val[0] = vbslq_u16( brightnessMask16.val[0], colorop_vec.decrease(tmpSrc16.val[0], evy16), tmpSrc16.val[0] );
					tmpSrc16.val[1] = vbslq_u16( brightnessMask16.val[1], colorop_vec.decrease(tmpSrc16.val[1], evy16), tmpSrc16.val[1] );
				}
				else
				{
					const uint32x4x4_t brightnessMask32 = {
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15})) )
					};
					
					tmpSrc32.val[0] = vbslq_u32( brightnessMask32.val[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[0], evy16), tmpSrc32.val[0] );
					tmpSrc32.val[1] = vbslq_u32( brightnessMask32.val[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[1], evy16), tmpSrc32.val[1] );
					tmpSrc32.val[2] = vbslq_u32( brightnessMask32.val[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[2], evy16), tmpSrc32.val[2] );
					tmpSrc32.val[3] = vbslq_u32( brightnessMask32.val[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[3], evy16), tmpSrc32.val[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v128u8 blendMask8 = vorrq_u8( forceDstTargetBlendMask, vandq_u8(vandq_u8(srcEffectEnableMask, dstTargetBlendEnableMask), vceqq_u8(colorEffect_vec128, vdupq_n_u8(ColorEffect_Blend))) );
	const u8 blendMaskValue = vmaxvq_u8(blendMask8);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const uint16x8x2_t oldColor16 = vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		
		if (blendMaskValue != 0)
		{
			const uint16x8x2_t blendMask16 = {
				vreinterpretq_u16_u8( vzip1q_u8(blendMask8, blendMask8) ),
				vreinterpretq_u16_u8( vzip2q_u8(blendMask8, blendMask8) )
			};
			
			uint16x8x2_t blendSrc16;
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					//blendSrc16[0] = colorop_vec.blend3D(src0, src1, dst16[0]);
					//blendSrc16[1] = colorop_vec.blend3D(src2, src3, dst16[1]);
					printf("GPU: 3D layers cannot be in RGBA5551 format. To composite a 3D layer, use the _unknownEffectMask32() method instead.\n");
					assert(false);
					break;
					
				case GPULayerType_BG:
					blendSrc16.val[0] = colorop_vec.blend(tmpSrc16.val[0], oldColor16.val[0], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc16.val[1] = colorop_vec.blend(tmpSrc16.val[1], oldColor16.val[1], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const uint16x8x2_t tempEVA = {
						vreinterpretq_u16_u8( vzip1q_u8(eva_vec128, zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(eva_vec128, zeroVec) )
					};
					const uint16x8x2_t tempEVB = {
						vreinterpretq_u16_u8( vzip1q_u8(evb_vec128, zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(evb_vec128, zeroVec) )
					};
					
					blendSrc16.val[0] = colorop_vec.blend(tmpSrc16.val[0], oldColor16.val[0], tempEVA.val[0], tempEVB.val[0]);
					blendSrc16.val[1] = colorop_vec.blend(tmpSrc16.val[1], oldColor16.val[1], tempEVA.val[1], tempEVB.val[1]);
					break;
				}
			}
			
			tmpSrc16.val[0] = vbslq_u16(blendMask16.val[0], blendSrc16.val[0], tmpSrc16.val[0]);
			tmpSrc16.val[1] = vbslq_u16(blendMask16.val[1], blendSrc16.val[1], tmpSrc16.val[1]);
		}
		
		// Store the final colors.
		const uint16x8x2_t passMask16 = {
			vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
			vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
		};
		
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(tmpSrc16.val[0], alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(tmpSrc16.val[1], alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		
		if (blendMaskValue != 0)
		{
			uint32x4x4_t blendSrc32;
			
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
					blendSrc32.val[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[0], oldColor32.val[0], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc32.val[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[1], oldColor32.val[1], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc32.val[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[2], oldColor32.val[2], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc32.val[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[3], oldColor32.val[3], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					uint8x16x2_t tempBlend8 = {
						vzip1q_u8(eva_vec128, eva_vec128),
						vzip2q_u8(eva_vec128, eva_vec128)
					};
					
					const uint16x8x4_t tempEVA = {
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[1], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[1], zeroVec) )
					};
					
					tempBlend8.val[0] = vzip1q_u8(evb_vec128, evb_vec128);
					tempBlend8.val[1] = vzip2q_u8(evb_vec128, evb_vec128);
					
					const uint16x8x4_t tempEVB = {
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[1], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[1], zeroVec) )
					};
					
					blendSrc32.val[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[0], oldColor32.val[0], tempEVA.val[0], tempEVB.val[0]);
					blendSrc32.val[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[1], oldColor32.val[1], tempEVA.val[1], tempEVB.val[1]);
					blendSrc32.val[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[2], oldColor32.val[2], tempEVA.val[2], tempEVB.val[2]);
					blendSrc32.val[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[3], oldColor32.val[3], tempEVA.val[3], tempEVB.val[3]);
					break;
				}
			}
			
			const uint16x8x2_t blendMask16 = {
				vreinterpretq_u16_u8( vzip1q_u8(blendMask8, blendMask8) ),
				vreinterpretq_u16_u8( vzip2q_u8(blendMask8, blendMask8) )
			};
			
			const uint32x4x4_t blendMask32 = {
				vreinterpretq_u32_u16( vzip1q_u16(blendMask16.val[0], blendMask16.val[0]) ),
				vreinterpretq_u32_u16( vzip2q_u16(blendMask16.val[0], blendMask16.val[0]) ),
				vreinterpretq_u32_u16( vzip1q_u16(blendMask16.val[1], blendMask16.val[1]) ),
				vreinterpretq_u32_u16( vzip2q_u16(blendMask16.val[1], blendMask16.val[1]) )
			};
			
			tmpSrc32.val[0] = vbslq_u32(blendMask32.val[0], blendSrc32.val[0], tmpSrc32.val[0]);
			tmpSrc32.val[1] = vbslq_u32(blendMask32.val[1], blendSrc32.val[1], tmpSrc32.val[1]);
			tmpSrc32.val[2] = vbslq_u32(blendMask32.val[2], blendSrc32.val[2], tmpSrc32.val[2]);
			tmpSrc32.val[3] = vbslq_u32(blendMask32.val[3], blendSrc32.val[3], tmpSrc32.val[3]);
		}
		
		// Store the final colors.
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3})) ),
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7})) ),
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11})) ),
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15})) )
		};
		
		const v128u32 alphaBits = vdupq_n_u32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(tmpSrc32.val[0], alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(tmpSrc32.val[1], alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(tmpSrc32.val[2], alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(tmpSrc32.val[3], alphaBits), oldColor32.val[3])
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_NEON::_unknownEffectMask32(GPUEngineCompositorInfo &compInfo,
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
	static uint8x16_t zeroVec = ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	const v128u8 dstLayerID = vld1q_u8(compInfo.target.lineLayerID);
	vst1q_u8( compInfo.target.lineLayerID, vbslq_u8(passMask8, srcLayerID, dstLayerID) );
	
	v128u8 dstTargetBlendEnableMask = vqtbl1q_u8(dstBlendEnableMaskLUT, dstLayerID);
	dstTargetBlendEnableMask = vandq_u8( vmvnq_u8(vceqq_u8(dstLayerID, srcLayerID)), dstTargetBlendEnableMask );
	
	v128u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : zeroVec;
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	v128u8 eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? vdupq_n_u8(compInfo.renderState.blendEVA) : vreinterpretq_u8_u16( vdupq_n_u16(compInfo.renderState.blendEVA) );
	v128u8 evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? vdupq_n_u8(compInfo.renderState.blendEVB) : vreinterpretq_u8_u16( vdupq_n_u16(compInfo.renderState.blendEVB) );
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v128u8 isObjTranslucentMask = vandq_u8( dstTargetBlendEnableMask, vorrq_u8(vceqq_u8(spriteMode, vdupq_n_u8(OBJMode_Transparent)), vceqq_u8(spriteMode, vdupq_n_u8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v128u8 spriteAlphaMask = vandq_u8( vmvnq_u8(vceqq_u8(spriteAlpha, vdupq_n_u8(0xFF))), isObjTranslucentMask );
		eva_vec128 = vbslq_u8(spriteAlphaMask, spriteAlpha, eva_vec128);
		evb_vec128 = vbslq_u8(spriteAlphaMask, vsubq_u8(vdupq_n_u8(16), spriteAlpha), evb_vec128);
	}
	
	// Select the color effect based on the BLDCNT target flags.
	const v128u8 colorEffect_vec128 = vbslq_u8(enableColorEffectMask, vdupq_n_u8(compInfo.renderState.colorEffect), vdupq_n_u8(ColorEffect_Disable));
	
	// ----------
	
	uint16x8x2_t tmpSrc16;
	uint32x4x4_t tmpSrc32;
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc16.val[0] = ColorspaceConvert6665To5551_NEON<false>(src0, src1);
		tmpSrc16.val[1] = ColorspaceConvert6665To5551_NEON<false>(src2, src3);
	}
	else
	{
		tmpSrc32.val[0] = src0;
		tmpSrc32.val[1] = src1;
		tmpSrc32.val[2] = src2;
		tmpSrc32.val[3] = src3;
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v128u8 brightnessMask8 = vandq_u8( vmvnq_u8(forceDstTargetBlendMask), vandq_u8(srcEffectEnableMask, vceqq_u8(colorEffect_vec128, vdupq_n_u8(ColorEffect_IncreaseBrightness))) );
			const u8 brightnessUpMaskValue = vmaxvq_u8(brightnessMask8);
			
			if (brightnessUpMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const uint16x8x2_t brightnessMask16 = {
						vreinterpretq_u16_u8( vzip1q_u8(brightnessMask8, brightnessMask8) ),
						vreinterpretq_u16_u8( vzip2q_u8(brightnessMask8, brightnessMask8) )
					};
					
					tmpSrc16.val[0] = vbslq_u16( brightnessMask16.val[0], colorop_vec.increase(tmpSrc16.val[0], evy16), tmpSrc16.val[0] );
					tmpSrc16.val[1] = vbslq_u16( brightnessMask16.val[1], colorop_vec.increase(tmpSrc16.val[1], evy16), tmpSrc16.val[1] );
				}
				else
				{
					const uint32x4x4_t brightnessMask32 = {
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15})) )
					};
					
					tmpSrc32.val[0] = vbslq_u32( brightnessMask32.val[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[0], evy16), tmpSrc32.val[0] );
					tmpSrc32.val[1] = vbslq_u32( brightnessMask32.val[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[1], evy16), tmpSrc32.val[1] );
					tmpSrc32.val[2] = vbslq_u32( brightnessMask32.val[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[2], evy16), tmpSrc32.val[2] );
					tmpSrc32.val[3] = vbslq_u32( brightnessMask32.val[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc32.val[3], evy16), tmpSrc32.val[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v128u8 brightnessMask8 = vandq_u8( vmvnq_u8(forceDstTargetBlendMask), vandq_u8(srcEffectEnableMask, vceqq_u8(colorEffect_vec128, vdupq_n_u8(ColorEffect_DecreaseBrightness))) );
			const u8 brightnessDownMaskValue = vmaxvq_u8(brightnessMask8);
			
			if (brightnessDownMaskValue != 0)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					const uint16x8x2_t brightnessMask16 = {
						vreinterpretq_u16_u8( vzip1q_u8(brightnessMask8, brightnessMask8) ),
						vreinterpretq_u16_u8( vzip2q_u8(brightnessMask8, brightnessMask8) )
					};
					
					tmpSrc16.val[0] = vbslq_u16( brightnessMask16.val[0], colorop_vec.decrease(tmpSrc16.val[0], evy16), tmpSrc16.val[0] );
					tmpSrc16.val[1] = vbslq_u16( brightnessMask16.val[1], colorop_vec.decrease(tmpSrc16.val[1], evy16), tmpSrc16.val[1] );
				}
				else
				{
					const uint32x4x4_t brightnessMask32 = {
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11})) ),
						vreinterpretq_u32_u8( vqtbl1q_u8(brightnessMask8, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15})) )
					};
					
					tmpSrc32.val[0] = vbslq_u32( brightnessMask32.val[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[0], evy16), tmpSrc32.val[0] );
					tmpSrc32.val[1] = vbslq_u32( brightnessMask32.val[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[1], evy16), tmpSrc32.val[1] );
					tmpSrc32.val[2] = vbslq_u32( brightnessMask32.val[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[2], evy16), tmpSrc32.val[2] );
					tmpSrc32.val[3] = vbslq_u32( brightnessMask32.val[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc32.val[3], evy16), tmpSrc32.val[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v128u8 blendMask8 = vorrq_u8( forceDstTargetBlendMask, vandq_u8(vandq_u8(srcEffectEnableMask, dstTargetBlendEnableMask), vceqq_u8(colorEffect_vec128, vdupq_n_u8(ColorEffect_Blend))) );
	const u8 blendMaskValue = vmaxvq_u8(blendMask8);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const uint16x8x2_t oldColor16 = vld1q_u16_x2((u16 *)compInfo.target.lineColor16);
		
		if (blendMaskValue != 0)
		{
			const uint16x8x2_t blendMask16 = {
				vreinterpretq_u16_u8( vzip1q_u8(blendMask8, blendMask8) ),
				vreinterpretq_u16_u8( vzip2q_u8(blendMask8, blendMask8) )
			};
			
			uint16x8x2_t blendSrc16;
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc16.val[0] = colorop_vec.blend3D(src0, src1, oldColor16.val[0]);
					blendSrc16.val[1] = colorop_vec.blend3D(src2, src3, oldColor16.val[1]);
					break;
					
				case GPULayerType_BG:
					blendSrc16.val[0] = colorop_vec.blend(tmpSrc16.val[0], oldColor16.val[0], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc16.val[1] = colorop_vec.blend(tmpSrc16.val[1], oldColor16.val[1], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const uint16x8x2_t tempEVA = {
						vreinterpretq_u16_u8( vzip1q_u8(eva_vec128, zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(eva_vec128, zeroVec) )
					};
					const uint16x8x2_t tempEVB = {
						vreinterpretq_u16_u8( vzip1q_u8(evb_vec128, zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(evb_vec128, zeroVec) )
					};
					
					blendSrc16.val[0] = colorop_vec.blend(tmpSrc16.val[0], oldColor16.val[0], tempEVA.val[0], tempEVB.val[0]);
					blendSrc16.val[1] = colorop_vec.blend(tmpSrc16.val[1], oldColor16.val[1], tempEVA.val[1], tempEVB.val[1]);
					break;
				}
			}
			
			tmpSrc16.val[0] = vbslq_u16(blendMask16.val[0], blendSrc16.val[0], tmpSrc16.val[0]);
			tmpSrc16.val[1] = vbslq_u16(blendMask16.val[1], blendSrc16.val[1], tmpSrc16.val[1]);
		}
		
		// Store the final colors.
		const uint16x8x2_t passMask16 = {
			vreinterpretq_u16_u8( vzip1q_u8(passMask8, passMask8) ),
			vreinterpretq_u16_u8( vzip2q_u8(passMask8, passMask8) )
		};
		
		const v128u16 alphaBits = vdupq_n_u16(0x8000);
		const uint16x8x2_t dst16 = {
			vbslq_u16(passMask16.val[0], vorrq_u16(tmpSrc16.val[0], alphaBits), oldColor16.val[0]),
			vbslq_u16(passMask16.val[1], vorrq_u16(tmpSrc16.val[1], alphaBits), oldColor16.val[1])
		};
		vst1q_u16_x2((u16 *)compInfo.target.lineColor16, dst16);
	}
	else
	{
		const uint32x4x4_t oldColor32 = vld1q_u32_x4((u32 *)compInfo.target.lineColor32);
		
		if (blendMaskValue != 0)
		{
			uint32x4x4_t blendSrc32;
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc32.val[0] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32.val[0], oldColor32.val[0]);
					blendSrc32.val[1] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32.val[1], oldColor32.val[1]);
					blendSrc32.val[2] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32.val[2], oldColor32.val[2]);
					blendSrc32.val[3] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc32.val[3], oldColor32.val[3]);
					break;
					
				case GPULayerType_BG:
					blendSrc32.val[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[0], oldColor32.val[0], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc32.val[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[1], oldColor32.val[1], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc32.val[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[2], oldColor32.val[2], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					blendSrc32.val[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc32.val[3], oldColor32.val[3], vreinterpretq_u16_u8(eva_vec128), vreinterpretq_u16_u8(evb_vec128));
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					uint8x16x2_t tempBlend8 = {
						vzip1q_u8(eva_vec128, eva_vec128),
						vzip2q_u8(eva_vec128, eva_vec128)
					};
					
					const uint16x8x4_t tempEVA = {
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[1], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[1], zeroVec) )
					};
					
					tempBlend8.val[0] = vzip1q_u8(evb_vec128, evb_vec128);
					tempBlend8.val[1] = vzip2q_u8(evb_vec128, evb_vec128);
					
					const uint16x8x4_t tempEVB = {
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[0], zeroVec) ),
						vreinterpretq_u16_u8( vzip1q_u8(tempBlend8.val[1], zeroVec) ),
						vreinterpretq_u16_u8( vzip2q_u8(tempBlend8.val[1], zeroVec) )
					};
					
					blendSrc32.val[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[0], oldColor32.val[0], tempEVA.val[0], tempEVB.val[0]);
					blendSrc32.val[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[1], oldColor32.val[1], tempEVA.val[1], tempEVB.val[1]);
					blendSrc32.val[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[2], oldColor32.val[2], tempEVA.val[2], tempEVB.val[2]);
					blendSrc32.val[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc32.val[3], oldColor32.val[3], tempEVA.val[3], tempEVB.val[3]);
					break;
				}
			}
			
			const uint16x8x2_t blendMask16 = {
				vreinterpretq_u16_u8( vzip1q_u8(blendMask8, blendMask8) ),
				vreinterpretq_u16_u8( vzip2q_u8(blendMask8, blendMask8) )
			};
			
			const uint32x4x4_t blendMask32 = {
				vreinterpretq_u32_u16( vzip1q_u16(blendMask16.val[0], blendMask16.val[0]) ),
				vreinterpretq_u32_u16( vzip2q_u16(blendMask16.val[0], blendMask16.val[0]) ),
				vreinterpretq_u32_u16( vzip1q_u16(blendMask16.val[1], blendMask16.val[1]) ),
				vreinterpretq_u32_u16( vzip2q_u16(blendMask16.val[1], blendMask16.val[1]) )
			};
			
			tmpSrc32.val[0] = vbslq_u32(blendMask32.val[0], blendSrc32.val[0], tmpSrc32.val[0]);
			tmpSrc32.val[1] = vbslq_u32(blendMask32.val[1], blendSrc32.val[1], tmpSrc32.val[1]);
			tmpSrc32.val[2] = vbslq_u32(blendMask32.val[2], blendSrc32.val[2], tmpSrc32.val[2]);
			tmpSrc32.val[3] = vbslq_u32(blendMask32.val[3], blendSrc32.val[3], tmpSrc32.val[3]);
		}
		
		// Store the final colors.
		const uint32x4x4_t passMask32 = {
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){ 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3})) ),
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){ 4, 4, 4, 4, 5, 5, 5, 5, 6, 6, 6, 6, 7, 7, 7, 7})) ),
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){ 8, 8, 8, 8, 9, 9, 9, 9,10,10,10,10,11,11,11,11})) ),
			vreinterpretq_u32_u8( vqtbl1q_u8(passMask8, ((v128u8){12,12,12,12,13,13,13,13,14,14,14,14,15,15,15,15})) )
		};
		
		const v128u32 alphaBits = vdupq_n_u32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		const uint32x4x4_t dst32 = {
			vbslq_u32(passMask32.val[0], vorrq_u32(tmpSrc32.val[0], alphaBits), oldColor32.val[0]),
			vbslq_u32(passMask32.val[1], vorrq_u32(tmpSrc32.val[1], alphaBits), oldColor32.val[1]),
			vbslq_u32(passMask32.val[2], vorrq_u32(tmpSrc32.val[2], alphaBits), oldColor32.val[2]),
			vbslq_u32(passMask32.val[3], vorrq_u32(tmpSrc32.val[3], alphaBits), oldColor32.val[3])
		};
		vst1q_u32_x4((u32 *)compInfo.target.lineColor32, dst32);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void PixelOperation_NEON::Composite16(GPUEngineCompositorInfo &compInfo,
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
				const v128u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? vld1q_u8(enableColorEffectPtr) : vdupq_n_u8(0xFF);
				const v128u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? vld1q_u8(sprAlphaPtr) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
				const v128u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? vld1q_u8(sprModePtr) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
				
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
FORCEINLINE void PixelOperation_NEON::Composite32(GPUEngineCompositorInfo &compInfo,
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
				const v128u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? vld1q_u8(enableColorEffectPtr): vdupq_n_u8(0xFF);
				const v128u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? vld1q_u8(sprAlphaPtr) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
				const v128u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? vld1q_u8(sprModePtr) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
				
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
		const uint16x8x2_t oldColor16 = vld1q_u16_x2(this->_deferredColorNative + x);
		
		if (ISFIRSTLINE)
		{
			const v128u8 indexVec = vld1q_u8(this->_deferredIndexNative + x);
			const v128u8 idxMask8 = vceqzq_u8(indexVec);
			const uint16x8x2_t idxMask16 = {
				vreinterpretq_u16_u8( vzip1q_u8(idxMask8, idxMask8) ),
				vreinterpretq_u16_u8( vzip2q_u8(idxMask8, idxMask8) )
			};
			
			const v128u16 mosaicColor16[2] = {
				vbslq_u16( idxMask16.val[0], vdupq_n_u16(0xFFFF), vandq_u16(oldColor16.val[0], vdupq_n_u16(0x7FFF)) ),
				vbslq_u16( idxMask16.val[1], vdupq_n_u16(0xFFFF), vandq_u16(oldColor16.val[1], vdupq_n_u16(0x7FFF)) )
			};
			
			const v128u8 mosaicSetColorMask8 = vceqzq_u8( vld1q_u8(compInfo.renderState.mosaicWidthBG->begin + x) );
			const v128u16 mosaicSetColorMask16[2] = {
				vreinterpretq_u16_u8( vzip1q_u8(mosaicSetColorMask8, mosaicSetColorMask8) ),
				vreinterpretq_u16_u8( vzip2q_u8(mosaicSetColorMask8, mosaicSetColorMask8) )
			};
			
			const uint16x8x2_t newColor16 = vld1q_u16_x2(mosaicColorBG + x);
			const uint16x8x2_t dst16 = {
				vbslq_u16( mosaicSetColorMask16[0], newColor16.val[0], mosaicColor16[0] ),
				vbslq_u16( mosaicSetColorMask16[1], newColor16.val[1], mosaicColor16[1] )
			};
			vst1q_u16_x2((u16 *)mosaicColorBG + x, dst16);
		}
		
		const v128u16 outColor16[2] = {
			((v128u16){
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+0]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+1]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+2]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+3]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+4]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+5]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+6]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+7]]
			}),
			
			((v128u16){
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+8]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+9]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+10]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+11]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+12]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+13]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+14]],
				mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+15]]
			})
		};
		
		const v128u16 writeColorMask16[2] = {
			vceqq_u16(outColor16[0], vdupq_n_u16(0xFFFF)),
			vceqq_u16(outColor16[1], vdupq_n_u16(0xFFFF))
		};
		
		const uint16x8x2_t dst16 = {
			vbslq_u16( writeColorMask16[0], oldColor16.val[0], outColor16[0] ),
			vbslq_u16( writeColorMask16[1], oldColor16.val[1], outColor16[1] )
		};
		vst1q_u16_x2(this->_deferredColorNative + x, dst16);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeNativeLineOBJ_LoopOp(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const Color4u8 *__restrict srcColorNative32)
{
	static const size_t step = sizeof(v128u8);
	
	const bool isUsingSrc32 = (srcColorNative32 != NULL);
	const v128u16 evy16 = vdupq_n_u16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = vdupq_n_u8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = vdupq_n_u8(compInfo.renderState.srcEffectEnable[GPULayerID_OBJ]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vld1q_u8(compInfo.renderState.dstBlendEnableVecLookup) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=step, srcColorNative16+=step, srcColorNative32+=step, compInfo.target.xNative+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		v128u8 passMask8;
		u16 passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = vld1q_u8(this->_didPassWindowTestNative[GPULayerID_OBJ] + i);
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vaddlvq_u8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
			
			didAllPixelsPass = (passMaskValue == 0xFF0);
		}
		else
		{
			passMask8 = vdupq_n_u8(0xFF);
			passMaskValue = 0xFF0;
			didAllPixelsPass = true;
		}
		
		if (isUsingSrc32)
		{
			const uint32x4x4_t src32 = vld1q_u32_x4((u32 *)srcColorNative32);
			
			pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo,
			                                                                                               didAllPixelsPass,
			                                                                                               passMask8, evy16,
			                                                                                               srcLayerID,
			                                                                                               src32.val[3], src32.val[2], src32.val[1], src32.val[0],
			                                                                                               srcEffectEnableMask,
			                                                                                               dstBlendEnableMaskLUT,
			                                                                                               this->_enableColorEffectNative[GPULayerID_OBJ] + i,
			                                                                                               this->_sprAlpha[compInfo.line.indexNative] + i,
			                                                                                               this->_sprType[compInfo.line.indexNative] + i);
		}
		else
		{
			const uint16x8x2_t src16 = vld1q_u16_x2(srcColorNative16);
			
			pixelop_vec.Composite16<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo,
			                                                                                               didAllPixelsPass,
			                                                                                               passMask8, evy16,
			                                                                                               srcLayerID,
			                                                                                               src16.val[1], src16.val[0],
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
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v128u16 evy16 = vdupq_n_u16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = vdupq_n_u8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = vdupq_n_u8(compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vld1q_u8(compInfo.renderState.dstBlendEnableVecLookup) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		v128u8 passMask8;
		u16 passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST || (LAYERTYPE == GPULayerType_BG))
		{
			if (WILLPERFORMWINDOWTEST)
			{
				// Do the window test.
				passMask8 = vld1q_u8(windowTestPtr + compInfo.target.xCustom);
			}
			
			if (LAYERTYPE == GPULayerType_BG)
			{
				// Do the index test. Pixels with an index value of 0 are rejected.
				const v128u8 idxPassMask8 = vmvnq_u8( vceqzq_u8( vld1q_u8(srcIndexCustom + compInfo.target.xCustom) ) );
				
				if (WILLPERFORMWINDOWTEST)
				{
					passMask8 = vandq_u8(idxPassMask8, passMask8 );
				}
				else
				{
					passMask8 = idxPassMask8;
				}
			}
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vaddlvq_u8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
			
			didAllPixelsPass = (passMaskValue == 0xFF0);
		}
		else
		{
			passMask8 = vdupq_n_u8(0xFF);
			passMaskValue = 0xFF0;
			didAllPixelsPass = true;
		}
		
		const uint16x8x2_t src16 = vld1q_u16_x2(srcColorCustom16 + compInfo.target.xCustom);
		
		pixelop_vec.Composite16<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
		                                                                                        didAllPixelsPass,
		                                                                                        passMask8, evy16,
		                                                                                        srcLayerID,
		                                                                                        src16.val[1], src16.val[0],
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
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v128u16 evy16 = vdupq_n_u16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = vdupq_n_u8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = vdupq_n_u8(compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vld1q_u8(compInfo.renderState.dstBlendEnableVecLookup) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		v128u8 passMask8;
		u16 passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = vld1q_u8(windowTestPtr + compInfo.target.xCustom);
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vaddlvq_u8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = vdupq_n_u8(0xFF);
			passMaskValue = 0xFF0;
		}
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
			case NDSColorFormat_BGR666_Rev:
			{
				const uint16x8x2_t src16 = vld1q_u16_x2((u16 *)vramColorPtr + i);
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v128u8 tempPassMask = vuzp1q_u8( vreinterpretq_u8_u16(vshrq_n_u16(src16.val[0], 15)), vreinterpretq_u8_u16(vshrq_n_u16(src16.val[1], 15)) );
					tempPassMask = vceqq_u8(tempPassMask, vdupq_n_u8(1));
					
					passMask8 = vandq_u8(tempPassMask, passMask8);
					passMaskValue = vaddlvq_u8(passMask8);
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				const bool didAllPixelsPass = (passMaskValue == 0xFF0);
				pixelop_vec.Composite16<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
				                                                                                        didAllPixelsPass,
				                                                                                        passMask8, evy16,
				                                                                                        srcLayerID,
				                                                                                        src16.val[1], src16.val[0],
				                                                                                        srcEffectEnableMask,
				                                                                                        dstBlendEnableMaskLUT,
				                                                                                        colorEffectEnablePtr + compInfo.target.xCustom,
				                                                                                        this->_sprAlphaCustom + compInfo.target.xCustom,
				                                                                                        this->_sprTypeCustom + compInfo.target.xCustom);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				const uint32x4x4_t src32 = vld1q_u32_x4((u32 *)vramColorPtr + i);
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v128u8 tempPassMask = vuzp1q_u8( vreinterpretq_u8_u16(vuzp1q_u16(vreinterpretq_u16_u32(vshrq_n_u32(src32.val[0], 24)), vreinterpretq_u16_u32(vshrq_n_u32(src32.val[1], 24)))), vreinterpretq_u8_u16(vuzp1q_u16(vreinterpretq_u16_u32(vshrq_n_u32(src32.val[2], 24)), vreinterpretq_u16_u32(vshrq_n_u32(src32.val[3], 24)))) );
					tempPassMask = vmvnq_u8( vceqzq_u8(tempPassMask) );
					
					passMask8 = vandq_u8(tempPassMask, passMask8);
					passMaskValue = vaddlvq_u8(passMask8);
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				const bool didAllPixelsPass = (passMaskValue == 0xFF0);
				pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
				                                                                                        didAllPixelsPass,
				                                                                                        passMask8, evy16,
				                                                                                        srcLayerID,
				                                                                                        src32.val[3], src32.val[2], src32.val[1], src32.val[0],
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
	size_t i = 0;
	
	static const size_t step = sizeof(v128u16);
	const v128u8 prioVec8 = vdupq_n_u8(prio);
	
	const size_t ssePixCount = length - (length % step);
	for (; i < ssePixCount; i+=step, spriteX+=step, frameX+=step)
	{
		const v128u8 prioTabVec8 = vld1q_u8(prioTab + frameX);
		const uint16x8x2_t color16 = vld1q_u16_x2(vramBuffer + spriteX);
		
		const v128u8 alphaCompare = vceqq_u8( vuzp1q_u8(vreinterpretq_u8_u16(vshrq_n_u16(color16.val[0], 15)), vreinterpretq_u8_u16(vshrq_n_u16(color16.val[1], 15))), vdupq_n_u8(0x01) );
		const v128u8 prioCompare = vcgtq_u8(prioTabVec8, prioVec8);
		
		const v128u8 combinedCompare8 = vandq_u8(prioCompare, alphaCompare);
		const uint16x8x2_t combinedCompare16 = {
			vreinterpretq_u16_u8( vzip1q_u8(combinedCompare8, combinedCompare8) ),
			vreinterpretq_u16_u8( vzip2q_u8(combinedCompare8, combinedCompare8) )
		};
		
		const uint16x8x2_t dst16 = vld1q_u16_x2(dst + frameX);
		const uint16x8x2_t out16 = {
			vbslq_u16(combinedCompare16.val[0], color16.val[0], dst16.val[0]),
			vbslq_u16(combinedCompare16.val[1], color16.val[1], dst16.val[1])
		};
		
		vst1q_u16_x2(dst + frameX, out16);
		vst1q_u8( prioTab + frameX,  vbslq_u8(combinedCompare8, prioVec8, prioTabVec8) );
		
		if (!ISDEBUGRENDER)
		{
			vst1q_u8( dst_alpha + frameX,     vbslq_u8(combinedCompare8, vdupq_n_u8(spriteAlpha + 1), vld1q_u8(dst_alpha + frameX)) );
			vst1q_u8( typeTab + frameX,       vbslq_u8(combinedCompare8, vdupq_n_u8(OBJMode_Bitmap),  vld1q_u8(typeTab + frameX)) );
			vst1q_u8( this->_sprNum + frameX, vbslq_u8(combinedCompare8, vdupq_n_u8(spriteNum),       vld1q_u8(this->_sprNum + frameX)) );
		}
	}
	
	return i;
}

void GPUEngineBase::_PerformWindowTestingNative(GPUEngineCompositorInfo &compInfo, const size_t layerID, const u8 *__restrict win0, const u8 *__restrict win1, const u8 *__restrict winObj, u8 *__restrict didPassWindowTestNative, u8 *__restrict enableColorEffectNative)
{
	static const v128u8 zeroVec = ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
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
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH/sizeof(v128u8); i++)
	{
		didPassWindowTest = zeroVec;
		enableColorEffect = zeroVec;
		
		win0HandledMask = zeroVec;
		win1HandledMask = zeroVec;
		winOBJHandledMask = zeroVec;
		
		// Window 0 has the highest priority, so always check this first.
		if (win0Ptr != NULL)
		{
			const v128u8 win0Enable = vdupq_n_u8(compInfo.renderState.WIN0_enable[layerID]);
			const v128u8 win0Effect = vdupq_n_u8(compInfo.renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			win0HandledMask = vceqq_u8(vld1q_u8((u8 *)(win0Ptr + i)), vdupq_n_u8(1));
			didPassWindowTest = vandq_u8(win0HandledMask, win0Enable);
			enableColorEffect = vandq_u8(win0HandledMask, win0Effect);
		}
		
		// Window 1 has medium priority, and is checked after Window 0.
		if (win1Ptr != NULL)
		{
			const v128u8 win1Enable = vdupq_n_u8(compInfo.renderState.WIN1_enable[layerID]);
			const v128u8 win1Effect = vdupq_n_u8(compInfo.renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			win1HandledMask = vandq_u8( vmvnq_u8(win0HandledMask), vceqq_u8(vld1q_u8((u8 *)(win1Ptr + i)), vdupq_n_u8(1)) );
			didPassWindowTest = vbslq_u8(win1HandledMask, win1Enable, didPassWindowTest);
			enableColorEffect = vbslq_u8(win1HandledMask, win1Effect, enableColorEffect);
		}
		
		// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
		if (winObjPtr != NULL)
		{
			const v128u8 winObjEnable = vdupq_n_u8(compInfo.renderState.WINOBJ_enable[layerID]);
			const v128u8 winObjEffect = vdupq_n_u8(compInfo.renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			winOBJHandledMask = vandq_u8( vmvnq_u8(vorrq_u8(win0HandledMask, win1HandledMask)), vceqq_u8(vld1q_u8((u8 *)(winObjPtr + i)), vdupq_n_u8(1)) );
			didPassWindowTest = vbslq_u8(winOBJHandledMask, winObjEnable, didPassWindowTest);
			enableColorEffect = vbslq_u8(winOBJHandledMask, winObjEffect, enableColorEffect);
		}
		
		// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
		// This has the lowest priority, and is always checked last.
		const v128u8 winOutEnable = vdupq_n_u8(compInfo.renderState.WINOUT_enable[layerID]);
		const v128u8 winOutEffect = vdupq_n_u8(compInfo.renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG]);
		
		winOUTHandledMask = vmvnq_u8( vorrq_u8(win0HandledMask, vorrq_u8(win1HandledMask, winOBJHandledMask)) );
		didPassWindowTest = vbslq_u8(winOUTHandledMask, winOutEnable, didPassWindowTest);
		enableColorEffect = vbslq_u8(winOUTHandledMask, winOutEffect, enableColorEffect);
		
		vst1q_u8((u8 *)(didPassWindowTestNativePtr + i), didPassWindowTest);
		vst1q_u8((u8 *)(enableColorEffectNativePtr + i), enableColorEffect);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
size_t GPUEngineA::_RenderLine_Layer3D_LoopOp(GPUEngineCompositorInfo &compInfo, const u8 *__restrict windowTestPtr, const u8 *__restrict colorEffectEnablePtr, const Color4u8 *__restrict srcLinePtr)
{
	static const size_t step = sizeof(v128u8);
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v128u16 evy16 = vdupq_n_u16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = vdupq_n_u8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = vdupq_n_u8(compInfo.renderState.srcEffectEnable[GPULayerID_BG0]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? vld1q_u8(compInfo.renderState.dstBlendEnableVecLookup) : ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, srcLinePtr+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		// Determine which pixels pass by doing the window test and the alpha test.
		v128u8 passMask8;
		u16 passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = vld1q_u8(windowTestPtr + compInfo.target.xCustom);
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = vaddlvq_u8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = vdupq_n_u8(0xFF);
			passMaskValue = 0xFF0;
		}
		
		const uint32x4x4_t src32 = vld1q_u32_x4((u32 *)srcLinePtr);
		
		// Do the alpha test. Pixels with an alpha value of 0 are rejected.
		const v128u8 srcAlpha = vuzp1q_u8( vreinterpretq_u8_u16( vuzp1q_u16( vreinterpretq_u16_u32(vshrq_n_u32(src32.val[0], 24)), vreinterpretq_u16_u32(vshrq_n_u32(src32.val[1], 24)) ) ),
		                                   vreinterpretq_u8_u16( vuzp1q_u16( vreinterpretq_u16_u32(vshrq_n_u32(src32.val[2], 24)), vreinterpretq_u16_u32(vshrq_n_u32(src32.val[3], 24)) ) ) );
		
		passMask8 = vandq_u8( vmvnq_u8(vceqzq_u8(srcAlpha)), passMask8 );
		
		// If none of the pixels within the vector pass, then reject them all at once.
		passMaskValue = vaddlvq_u8(passMask8);
		if (passMaskValue == 0)
		{
			continue;
		}
		
		// Write out the pixels.
		const bool didAllPixelsPass = (passMaskValue == 0xFF0);
		pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D, WILLPERFORMWINDOWTEST>(compInfo,
		                                                                                              didAllPixelsPass,
		                                                                                              passMask8, evy16,
		                                                                                              srcLayerID,
		                                                                                              src32.val[3], src32.val[2], src32.val[1], src32.val[0],
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
	static const v128u8 zeroVec = ((v128u8){0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0});
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? length * sizeof(u32) / sizeof(v128u32) : length * sizeof(u16) / sizeof(v128u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			const v128u32 srcA32 = vld1q_u32((u32 *)((v128u32 *)srcA + i));
			const v128u32 srcB32 = vld1q_u32((u32 *)((v128u32 *)srcB + i));
			
			// Get color masks based on if the alpha value is 0. Colors with an alpha value
			// equal to 0 are rejected.
			v128u32 srcA_alpha = vandq_u32(srcA32, vdupq_n_u32(0xFF000000));
			v128u32 srcB_alpha = vandq_u32(srcB32, vdupq_n_u32(0xFF000000));
			v128u32 srcA_masked = vandq_u32( vmvnq_u32(vceqzq_u32(srcA_alpha)), srcA32 );
			v128u32 srcB_masked = vandq_u32( vmvnq_u32(vceqzq_u32(srcB_alpha)), srcB32 );
			
			v128u16 dstLo;
			v128u16 dstHi;
			v128u32 out32;
			
			// Temporarily convert the color component values from 8-bit to 16-bit, and then
			// do the blend calculation.
			v128u16 srcA_maskedLo = vreinterpretq_u16_u8( vzip1q_u8(vreinterpretq_u8_u32(srcA_masked), zeroVec) );
			v128u16 srcA_maskedHi = vreinterpretq_u16_u8( vzip2q_u8(vreinterpretq_u8_u32(srcA_masked), zeroVec) );
			v128u16 srcB_maskedLo = vreinterpretq_u16_u8( vzip1q_u8(vreinterpretq_u8_u32(srcB_masked), zeroVec) );
			v128u16 srcB_maskedHi = vreinterpretq_u16_u8( vzip2q_u8(vreinterpretq_u8_u32(srcB_masked), zeroVec) );
			
			dstLo = vmlaq_n_u16( vmulq_n_u16(srcA_maskedLo, blendEVA), srcB_maskedLo, blendEVB );
			dstHi = vmlaq_n_u16( vmulq_n_u16(srcA_maskedHi, blendEVA), srcB_maskedHi, blendEVB );
			
			dstLo = vshrq_n_u16(dstLo, 4);
			dstHi = vshrq_n_u16(dstHi, 4);
			
			// Convert the color components back from 16-bit to 8-bit using a saturated pack.
			out32 = vreinterpretq_u32_u8( vuzp1q_u8(vreinterpretq_u8_u16(dstLo), vreinterpretq_u8_u16(dstHi)) );
			
			// Add the alpha components back in.
			out32 = vandq_u32(out32, vdupq_n_u32(0x00FFFFFF));
			out32 = vorrq_u32(out32, srcA_alpha);
			out32 = vorrq_u32(out32, srcB_alpha);
			vst1q_u32((u32 *)((v128u32 *)dst + i), out32);
		}
		else
		{
			const v128u16 srcA16 = vld1q_u16((u16 *)((v128u16 *)srcA + i));
			const v128u16 srcB16 = vld1q_u16((u16 *)((v128u16 *)srcB + i));
			v128u16 srcA_alpha = vandq_u16(srcA16, vdupq_n_u16(0x8000));
			v128u16 srcB_alpha = vandq_u16(srcB16, vdupq_n_u16(0x8000));
			v128u16 srcA_masked = vandq_u16( vmvnq_u16(vceqzq_u16(srcA_alpha)), srcA16 );
			v128u16 srcB_masked = vandq_u16( vmvnq_u16(vceqzq_u16(srcB_alpha)), srcB16 );
			v128u16 colorBitMask = vdupq_n_u16(0x001F);
			
			v128u16 ra;
			v128u16 ga;
			v128u16 ba;
			v128u16 dst16;
			
			ra = vandq_u16(            srcA_masked,      colorBitMask);
			ga = vandq_u16(vshrq_n_u16(srcA_masked,  5), colorBitMask);
			ba = vandq_u16(vshrq_n_u16(srcA_masked, 10), colorBitMask);
			
			v128u16 rb = vandq_u16(            srcB_masked,      colorBitMask);
			v128u16 gb = vandq_u16(vshrq_n_u16(srcB_masked,  5), colorBitMask);
			v128u16 bb = vandq_u16(vshrq_n_u16(srcB_masked, 10), colorBitMask);
			
			ra = vmlaq_n_u16( vmulq_n_u16(ra, blendEVA), rb, blendEVB );
			ga = vmlaq_n_u16( vmulq_n_u16(ga, blendEVA), gb, blendEVB );
			ba = vmlaq_n_u16( vmulq_n_u16(ba, blendEVA), bb, blendEVB );
			
			ra = vshrq_n_u16(ra, 4);
			ga = vshrq_n_u16(ga, 4);
			ba = vshrq_n_u16(ba, 4);
			
			ra = vminq_u16(ra, colorBitMask);
			ga = vminq_u16(ga, colorBitMask);
			ba = vminq_u16(ba, colorBitMask);
			
			dst16 = vorrq_u16( vorrq_u16(vorrq_u16(ra, vshlq_n_u16(ga,  5)), vshlq_n_u16(ba, 10)), vorrq_u16(srcA_alpha, srcB_alpha) );
			vst1q_u16((u16 *)((v128u16 *)dst + i), dst16);
		}
	}
	
	return (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? i * sizeof(v128u32) / sizeof(u32) : i * sizeof(v128u16) / sizeof(u16);
}

template <NDSColorFormat OUTPUTFORMAT>
size_t NDSDisplay::_ApplyMasterBrightnessUp_LoopOp(void *__restrict dst, const size_t pixCount, const u8 intensityClamped)
{
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? pixCount * sizeof(u32) / sizeof(v128u32) : pixCount * sizeof(u16) / sizeof(v128u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			v128u16 dstColor = vld1q_u16((u16 *)((v128u16 *)dst + i));
			dstColor = colorop_vec.increase(dstColor, vdupq_n_u16(intensityClamped));
			dstColor = vorrq_u16(dstColor, vdupq_n_u16(0x8000));
			vst1q_u16( (u16 *)((v128u16 *)dst + i), dstColor);
		}
		else
		{
			v128u32 dstColor = vld1q_u32((u32 *)((v128u32 *)dst + i));
			dstColor = colorop_vec.increase<OUTPUTFORMAT>(dstColor, vdupq_n_u16(intensityClamped));
			dstColor = vorrq_u32(dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000));
			vst1q_u32( (u32 *)((v128u32 *)dst + i), dstColor);
		}
	}
	
	return (i * sizeof(v128u8));
}

template <NDSColorFormat OUTPUTFORMAT>
size_t NDSDisplay::_ApplyMasterBrightnessDown_LoopOp(void *__restrict dst, const size_t pixCount, const u8 intensityClamped)
{
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? pixCount * sizeof(u32) / sizeof(v128u32) : pixCount * sizeof(u16) / sizeof(v128u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			v128u16 dstColor = vld1q_u16((u16 *)((v128u16 *)dst + i));
			dstColor = colorop_vec.decrease(dstColor, vdupq_n_u16(intensityClamped));
			dstColor = vorrq_u16(dstColor, vdupq_n_u16(0x8000));
			vst1q_u16( (u16 *)((v128u16 *)dst + i), dstColor);
		}
		else
		{
			v128u32 dstColor = vld1q_u32((u32 *)((v128u32 *)dst + i));
			dstColor = colorop_vec.decrease<OUTPUTFORMAT>(dstColor, vdupq_n_u16(intensityClamped));
			dstColor = vorrq_u32(dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? vdupq_n_u32(0x1F000000) : vdupq_n_u32(0xFF000000));
			vst1q_u32( (u32 *)((v128u32 *)dst + i), dstColor);
		}
	}
	
	return (i * sizeof(v128u8));
}

#endif // ENABLE_NEON_A64
