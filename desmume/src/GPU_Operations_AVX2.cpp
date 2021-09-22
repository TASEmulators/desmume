/*
	Copyright (C) 2021 DeSmuME team

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

#ifndef ENABLE_AVX2
	#error This code requires AVX2 support.
	#warning This error might occur if this file is compiled directly. Do not compile this file directly, as it is already included in GPU_Operations.cpp.
#else

#include "GPU_Operations_AVX2.h"


static const ColorOperation_AVX2 colorop_vec;
static const PixelOperation_AVX2 pixelop_vec;

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
{
	if (INTEGERSCALEHINT == 0)
	{
		memcpy(dst, src, dstWidth * ELEMENTSIZE);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256s8) / ELEMENTSIZE), _mm256_store_si256((v256s8 *)dst + (X), _mm256_load_si256((v256s8 *)src + (X))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		__m256i srcPix;
		__m256i srcPixOut[2];
		
		switch (ELEMENTSIZE)
		{
			case 1:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256u8) / ELEMENTSIZE), \
							  srcPix = _mm256_load_si256((v256u8 *)((v256u8 *)src + (X))); \
							  srcPix = _mm256_permute4x64_epi64(srcPix, 0xD8); \
							  srcPixOut[0] = _mm256_unpacklo_epi8(srcPix, srcPix); \
							  srcPixOut[1] = _mm256_unpackhi_epi8(srcPix, srcPix); \
							  _mm256_store_si256((v256u8 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u8) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u8 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u8) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm256_store_si256((v256u8 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u8) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u8 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u8) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256u8) / ELEMENTSIZE), \
							  srcPix = _mm256_load_si256((v256u8 *)((v256u8 *)src + (X))); \
							  srcPix = _mm256_permute4x64_epi64(srcPix, 0xD8); \
							  srcPixOut[0] = _mm256_unpacklo_epi8(srcPix, srcPix); \
							  srcPixOut[1] = _mm256_unpackhi_epi8(srcPix, srcPix); \
							  _mm256_store_si256((v256u8 *)dst + ((X) * 2) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u8 *)dst + ((X) * 2) + 1, srcPixOut[1]); \
							  );
				}
				break;
			}
				
			case 2:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256u16) / ELEMENTSIZE), \
							  srcPix = _mm256_load_si256((v256u16 *)((v256u16 *)src + (X))); \
							  srcPix = _mm256_permute4x64_epi64(srcPix, 0xD8); \
							  srcPixOut[0] = _mm256_unpacklo_epi16(srcPix, srcPix); \
							  srcPixOut[1] = _mm256_unpackhi_epi16(srcPix, srcPix); \
							  _mm256_store_si256((v256u16 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u16) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u16 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u16) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm256_store_si256((v256u16 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u16) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u16 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u16) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256u16) / ELEMENTSIZE), \
							  srcPix = _mm256_load_si256((v256u16 *)((v256u16 *)src + (X))); \
							  srcPix = _mm256_permute4x64_epi64(srcPix, 0xD8); \
							  srcPixOut[0] = _mm256_unpacklo_epi16(srcPix, srcPix); \
							  srcPixOut[1] = _mm256_unpackhi_epi16(srcPix, srcPix); \
							  _mm256_store_si256((v256u16 *)dst + ((X) * 2) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u16 *)dst + ((X) * 2) + 1, srcPixOut[1]); \
							  );
				}
				break;
			}
				
			case 4:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256u32) / ELEMENTSIZE), \
							  srcPix = _mm256_load_si256((v256u32 *)((v256u32 *)src + (X))); \
							  srcPixOut[0] = _mm256_permutevar8x32_epi32(srcPix, _mm256_set_epi32(3, 3, 2, 2, 1, 1, 0, 0)); \
							  srcPixOut[1] = _mm256_permutevar8x32_epi32(srcPix, _mm256_set_epi32(7, 7, 6, 6, 5, 5, 4, 4)); \
							  _mm256_store_si256((v256u32 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u32) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u32 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u32) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm256_store_si256((v256u32 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u32) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u32 *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(v256u32) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256u32) / ELEMENTSIZE), \
							  srcPix = _mm256_load_si256((v256u32 *)((v256u32 *)src + (X))); \
							  srcPixOut[0] = _mm256_permutevar8x32_epi32(srcPix, _mm256_set_epi32(3, 3, 2, 2, 1, 1, 0, 0)); \
							  srcPixOut[1] = _mm256_permutevar8x32_epi32(srcPix, _mm256_set_epi32(7, 7, 6, 6, 5, 5, 4, 4)); \
							  _mm256_store_si256((v256u32 *)dst + ((X) * 2) + 0, srcPixOut[0]); \
							  _mm256_store_si256((v256u32 *)dst + ((X) * 2) + 1, srcPixOut[1]); \
							  );
				}
				break;
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		__m256i srcPixOut[3];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m256i srcVec  = _mm256_load_si256((__m256i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
				const v256u8 src8lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u8 src8hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(10,10, 9, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6, 5, 5,   5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(srcVec, _mm256_set_epi8(21,20,20,20,19,19,19,18,18,18,17,17,17,16,16,16,  15,15,15,14,14,14,13,13,13,12,12,12,11,11,11,10));
				srcPixOut[2] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,30,30,30,29,29,29,28,28,28,27,27,27,26,  26,26,25,25,25,24,24,24,23,23,23,22,22,22,21,21));
			}
			else if (ELEMENTSIZE == 2)
			{
				const v256u16 src16lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u16 src16hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(11,10, 9, 8, 9, 8, 9, 8, 7, 6, 7, 6, 7, 6, 5, 4,   5, 4, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(srcVec,  _mm256_set_epi8(21,20,21,20,19,18,19,18,19,18,17,16,17,16,17,16,  15,14,15,14,15,14,13,12,13,12,13,12,11,10,11,10));
				srcPixOut[2] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(31,30,31,30,31,30,29,28,29,28,29,28,27,26,27,26,  27,26,25,24,25,24,25,24,23,22,23,22,23,22,21,20));
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(2, 2, 1, 1, 1, 0, 0, 0));
				srcPixOut[1] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(5, 4, 4, 4, 3, 3, 3, 2));
				srcPixOut[2] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(7, 7, 7, 6, 6, 6, 5, 5));
			}
			
			for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
			{
				_mm256_store_si256((__m256i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
					{
						_mm256_store_si256((__m256i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		__m256i srcPixOut[4];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m256i srcVec  = _mm256_load_si256((__m256i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
				const v256u8 src8lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u8 src8hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4,   3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(15,15,15,15,14,14,14,14,13,13,13,13,12,12,12,12,  11,11,11,11,10,10,10,10, 9, 9, 9, 9, 8, 8, 8, 8));
				srcPixOut[2] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(23,23,23,23,22,22,22,22,21,21,21,21,20,20,20,20,  19,19,19,19,18,18,18,18,17,17,17,17,16,16,16,16));
				srcPixOut[3] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,31,30,30,30,30,29,29,29,29,28,28,28,28,  27,27,27,27,26,26,26,26,25,25,25,25,24,24,24,24));
			}
			else if (ELEMENTSIZE == 2)
			{
				const v256u16 src16lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u16 src16hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 5, 4, 5, 4, 5, 4,   3, 2, 3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(15,14,15,14,15,14,15,14,13,12,13,12,13,12,13,12,  11,10,11,10,11,10,11,10, 9, 8, 9, 8, 9, 8, 9, 8));
				srcPixOut[2] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(23,22,23,22,23,22,23,22,21,20,21,20,21,20,21,20,  19,18,19,18,19,18,19,18,17,16,17,16,17,16,17,16));
				srcPixOut[3] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(31,30,31,30,31,30,31,30,29,28,29,28,29,28,29,28,  27,26,27,26,27,26,27,26,25,24,25,24,25,24,25,24));
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(1, 1, 1, 1, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(3, 3, 3, 3, 2, 2, 2, 2));
				srcPixOut[2] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(5, 5, 5, 5, 4, 4, 4, 4));
				srcPixOut[3] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(7, 7, 7, 7, 6, 6, 6, 6));
			}
			
			for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
			{
				_mm256_store_si256((__m256i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
					{
						_mm256_store_si256((__m256i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 5)
	{
		__m256i srcPixOut[5];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m256i srcVec  = _mm256_load_si256((__m256i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
				const v256u8 src8lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u8 src8hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 6, 6, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 3, 3, 3, 3,   3, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(12,12,12,12,11,11,11,11,11,10,10,10,10,10, 9, 9,   9, 9, 9, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 6, 6, 6));
				srcPixOut[2] = _mm256_shuffle_epi8(srcVec, _mm256_set_epi8(19,18,18,18,18,18,17,17,17,17,17,16,16,16,16,16,  15,15,15,15,15,14,14,14,14,14,13,13,13,13,13,12));
				srcPixOut[3] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,31,30,30,30,30,29,29,29,29,28,28,28,28,  27,27,27,27,26,26,26,26,25,25,25,25,24,24,24,24));
				srcPixOut[4] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,31,30,30,30,30,29,29,29,29,28,28,28,28,  27,27,27,27,26,26,26,26,25,25,25,25,24,24,24,24));
			}
			else if (ELEMENTSIZE == 2)
			{
				const v256u16 src16lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u16 src16hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 7, 6, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 3, 2, 3, 2,   3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(13,12,13,12,11,10,11,10,11,10,11,10,11,10, 9, 8,   9, 8, 9, 8, 9, 8, 9, 8, 7, 6, 7, 6, 7, 6, 7, 6));
				srcPixOut[2] = _mm256_shuffle_epi8(srcVec,  _mm256_set_epi8(19,18,19,18,19,18,17,16,17,16,17,16,17,16,17,16,  15,14,15,14,15,14,15,14,15,14,13,12,13,12,13,12));
				srcPixOut[3] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(25,24,25,24,25,24,25,24,23,22,23,22,23,22,23,22,  23,22,21,20,21,20,21,20,21,20,21,20,19,18,19,18));
				srcPixOut[4] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(31,30,31,30,31,30,31,30,31,30,29,28,29,28,29,28,  29,28,29,28,27,26,27,26,27,26,27,26,27,26,25,24));
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(1, 1, 1, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(3, 2, 2, 2, 2, 2, 1, 1));
				srcPixOut[2] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(4, 4, 4, 4, 3, 3, 3, 3));
				srcPixOut[3] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(6, 6, 5, 5, 5, 5, 5, 4));
				srcPixOut[4] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(7, 7, 7, 7, 7, 6, 6, 6));
			}
			
			for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
			{
				_mm256_store_si256((__m256i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
					{
						_mm256_store_si256((__m256i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 6)
	{
		__m256i srcPixOut[6];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m256i srcVec  = _mm256_load_si256((__m256i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
				const v256u8 src8lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u8 src8hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 5, 5, 4, 4, 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 2, 2,   2, 2, 2, 2, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(10,10,10,10, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8,   7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 5, 5, 5, 5));
				srcPixOut[2] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(15,15,15,15,15,15,14,14,14,14,14,14,13,13,13,13,  13,13,12,12,12,12,12,12,11,11,11,11,11,11,10,10));
				srcPixOut[3] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(21,21,20,20,20,20,20,20,19,19,19,19,19,19,18,18,  18,18,18,18,17,17,17,17,17,17,16,16,16,16,16,16));
				srcPixOut[4] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(26,26,26,26,25,25,25,25,25,25,24,24,24,24,24,24,  23,23,23,23,23,23,22,22,22,22,22,22,21,21,21,21));
				srcPixOut[5] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,31,31,31,30,30,30,30,30,30,29,29,29,29,  29,29,28,28,28,28,28,28,27,27,27,27,27,27,26,26));
			}
			else if (ELEMENTSIZE == 2)
			{
				const v256u16 src16lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u16 src16hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 5, 4, 5, 4, 5, 4, 5, 4, 3, 2, 3, 2, 3, 2, 3, 2,   3, 2, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(11,10,11,10, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8,   7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 5, 4));
				srcPixOut[2] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(15,14,15,14,15,14,15,14,15,14,15,14,13,12,13,12,  13,12,13,12,13,12,13,12,11,10,11,10,11,10,11,10));
				srcPixOut[3] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(21,20,21,20,21,20,21,20,19,18,19,18,19,18,19,18,  19,18,19,18,17,16,17,16,17,16,17,16,17,16,17,16));
				srcPixOut[4] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(27,26,27,26,25,24,25,24,25,24,25,24,25,24,25,24,  23,22,23,22,23,22,23,22,23,22,23,22,21,20,21,20));
				srcPixOut[5] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(31,30,31,30,31,30,31,30,31,30,31,30,29,28,29,28,  29,28,29,28,29,28,29,28,27,26,27,26,27,26,27,26));
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(1, 1, 0, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(2, 2, 2, 2, 1, 1, 1, 1));
				srcPixOut[2] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(3, 3, 3, 3, 3, 3, 2, 2));
				srcPixOut[3] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(5, 5, 4, 4, 4, 4, 4, 4));
				srcPixOut[4] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(6, 6, 6, 6, 5, 5, 5, 5));
				srcPixOut[5] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(7, 7, 7, 7, 7, 7, 6, 6));
			}
			
			for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
			{
				_mm256_store_si256((__m256i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
					{
						_mm256_store_si256((__m256i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 7)
	{
		__m256i srcPixOut[7];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m256i srcVec  = _mm256_load_si256((__m256i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
				const v256u8 src8lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u8 src8hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 4, 4, 4, 4, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2,   2, 2, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 9, 8, 8, 8, 8, 8, 8, 8, 7, 7, 7, 7, 7, 7, 7, 6,   6, 6, 6, 6, 6, 6, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4));
				srcPixOut[2] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(13,13,13,13,13,12,12,12,12,12,12,12,11,11,11,11,  11,11,11,10,10,10,10,10,10,10, 9, 9, 9, 9, 9, 9));
				srcPixOut[3] = _mm256_shuffle_epi8(srcVec, _mm256_set_epi8(18,18,17,17,17,17,17,17,17,16,16,16,16,16,16,16,  15,15,15,15,15,15,15,14,14,14,14,14,14,14,13,13));
				srcPixOut[4] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(22,22,22,22,22,22,21,21,21,21,21,21,21,20,20,20,  20,20,20,20,19,19,19,19,19,19,19,18,18,18,18,18));
				srcPixOut[5] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(27,27,27,26,26,26,26,26,26,26,25,25,25,25,25,25,  25,24,24,24,24,24,24,24,23,23,23,23,23,23,23,22));
				srcPixOut[6] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,31,31,31,31,30,30,30,30,30,30,30,29,29,  29,29,29,29,29,28,28,28,28,28,28,28,27,27,27,27));
			}
			else if (ELEMENTSIZE == 2)
			{
				const v256u16 src16lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u16 src16hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 5, 4, 5, 4, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2,   3, 2, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 9, 8, 9, 8, 9, 8, 9, 8, 7, 6, 7, 6, 7, 6, 7, 6,   7, 6, 7, 6, 7, 6, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4));
				srcPixOut[2] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(13,12,13,12,13,12,13,12,13,12,13,12,11,10,11,10,  11,10,11,10,11,10,11,10,11,10, 9, 8, 9, 8, 9, 8));
				srcPixOut[3] = _mm256_shuffle_epi8(srcVec,  _mm256_set_epi8(19,18,17,16,17,16,17,16,17,16,17,16,17,16,17,16,  15,14,15,14,15,14,15,14,15,14,15,14,15,14,13,12));
				srcPixOut[4] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(23,22,23,22,23,22,21,20,21,20,21,20,21,20,21,20,  21,20,21,20,19,18,19,18,19,18,19,18,19,18,19,18));
				srcPixOut[5] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(27,26,27,26,27,26,27,26,27,26,25,24,25,24,25,24,  25,24,25,24,25,24,25,24,23,22,23,22,23,22,23,22));
				srcPixOut[6] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(31,30,31,30,31,30,31,30,31,30,31,30,31,30,29,28,  29,28,29,28,29,28,29,28,29,28,29,28,27,26,27,26));
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(1, 0, 0, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(2, 2, 1, 1, 1, 1, 1, 1));
				srcPixOut[2] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(3, 3, 3, 2, 2, 2, 2, 2));
				srcPixOut[3] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(4, 4, 4, 4, 3, 3, 3, 3));
				srcPixOut[4] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(5, 5, 5, 5, 5, 4, 4, 4));
				srcPixOut[5] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(6, 6, 6, 6, 6, 6, 5, 5));
				srcPixOut[6] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set_epi32(7, 7, 7, 7, 7, 7, 7, 6));
			}
			
			for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
			{
				_mm256_store_si256((__m256i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
					{
						_mm256_store_si256((__m256i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 8)
	{
		__m256i srcPixOut[8];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m256i srcVec  = _mm256_load_si256((__m256i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
				const v256u8 src8lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u8 src8hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 3, 3, 3, 3, 3, 3, 3, 3, 2, 2, 2, 2, 2, 2, 2, 2,   1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8( 7, 7, 7, 7, 7, 7, 7, 7, 6, 6, 6, 6, 6, 6, 6, 6,   5, 5, 5, 5, 5, 5, 5, 5, 4, 4, 4, 4, 4, 4, 4, 4));
				srcPixOut[2] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(11,11,11,11,11,11,11,11,10,10,10,10,10,10,10,10,   9, 9, 9, 9, 9, 9, 9, 9, 8, 8, 8, 8, 8, 8, 8, 8));
				srcPixOut[3] = _mm256_shuffle_epi8(src8lo, _mm256_set_epi8(15,15,15,15,15,15,15,15,14,14,14,14,14,14,14,14,  13,13,13,13,13,13,13,13,12,12,12,12,12,12,12,12));
				srcPixOut[4] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(19,19,19,19,19,19,19,19,18,18,18,18,18,18,18,18,  17,17,17,17,17,17,17,17,16,16,16,16,16,16,16,16));
				srcPixOut[5] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(23,23,23,23,23,23,23,23,22,22,22,22,22,22,22,22,  21,21,21,21,21,21,21,21,20,20,20,20,20,20,20,20));
				srcPixOut[6] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(27,27,27,27,27,27,27,27,26,26,26,26,26,26,26,26,  25,25,25,25,25,25,25,25,24,24,24,24,24,24,24,24));
				srcPixOut[7] = _mm256_shuffle_epi8(src8hi, _mm256_set_epi8(31,31,31,31,31,31,31,31,30,30,30,30,30,30,30,30,  29,29,29,29,29,29,29,29,28,28,28,28,28,28,28,28));
			}
			else if (ELEMENTSIZE == 2)
			{
				const v256u16 src16lo = _mm256_permute4x64_epi64(srcVec, 0x44);
				const v256u16 src16hi = _mm256_permute4x64_epi64(srcVec, 0xEE);
				
				srcPixOut[0] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2, 3, 2,   1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8( 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6, 7, 6,   5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4, 5, 4));
				srcPixOut[2] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(11,10,11,10,11,10,11,10,11,10,11,10,11,10,11,10,   9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8, 9, 8));
				srcPixOut[3] = _mm256_shuffle_epi8(src16lo, _mm256_set_epi8(15,14,15,14,15,14,15,14,15,14,15,14,15,14,15,14,  13,12,13,12,13,12,13,12,13,12,13,12,13,12,13,12));
				srcPixOut[4] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(19,18,19,18,19,18,19,18,19,18,19,18,19,18,19,18,  17,16,17,16,17,16,17,16,17,16,17,16,17,16,17,16));
				srcPixOut[5] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(23,22,23,22,23,22,23,22,23,22,23,22,23,22,23,22,  21,20,21,20,21,20,21,20,21,20,21,20,21,20,21,20));
				srcPixOut[6] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(27,26,27,26,27,26,27,26,27,26,27,26,27,26,27,26,  25,24,25,24,25,24,25,24,25,24,25,24,25,24,25,24));
				srcPixOut[7] = _mm256_shuffle_epi8(src16hi, _mm256_set_epi8(31,30,31,30,31,30,31,30,31,30,31,30,31,30,31,30,  29,28,29,28,29,28,29,28,29,28,29,28,29,28,29,28));
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(0));
				srcPixOut[1] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(1));
				srcPixOut[2] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(2));
				srcPixOut[3] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(3));
				srcPixOut[4] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(4));
				srcPixOut[5] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(5));
				srcPixOut[6] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(6));
				srcPixOut[7] = _mm256_permutevar8x32_epi32(srcVec, _mm256_set1_epi32(7));
			}
			
			for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
			{
				_mm256_store_si256((__m256i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < INTEGERSCALEHINT; lx++)
					{
						_mm256_store_si256((__m256i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT > 1)
	{
		const size_t scale = dstWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		const size_t scaleLo = scale / 2;
		const size_t scaleMid = scaleLo + (scale & 0x0001);
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX++, dstX+=scale)
		{
			const __m256i srcVec = _mm256_load_si256((__m256i *)src + srcX);
			const __m256i srcVecLo = _mm256_permute4x64_epi64(srcVec, 0x44);
			const __m256i srcVecHi = _mm256_permute4x64_epi64(srcVec, 0xEE);
			v256u8 ssse3idx;
			
			size_t lx = 0;
			
			for (; lx < scaleLo; lx++)
			{
				if (ELEMENTSIZE == 1)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u8_16e + (lx * sizeof(v256u8))));
				}
				else if (ELEMENTSIZE == 2)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u16_8e + (lx * sizeof(v256u8))));
				}
				else if (ELEMENTSIZE == 4)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u32_4e + (lx * sizeof(v256u8))));
				}
				
				_mm256_store_si256( (__m256i *)dst + dstX + lx, _mm256_shuffle_epi8(srcVecLo, ssse3idx) );
			}
			
			if (scaleMid > scaleLo)
			{
				if (ELEMENTSIZE == 1)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u8_16e + (lx * sizeof(v256u8))));
				}
				else if (ELEMENTSIZE == 2)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u16_8e + (lx * sizeof(v256u8))));
				}
				else if (ELEMENTSIZE == 4)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u32_4e + (lx * sizeof(v256u8))));
				}
				
				_mm256_store_si256( (__m256i *)dst + dstX + lx, _mm256_shuffle_epi8(srcVec, ssse3idx) );
				lx++;
			}
			
			for (; lx < scale; lx++)
			{
				if (ELEMENTSIZE == 1)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u8_16e + (lx * sizeof(v256u8))));
				}
				else if (ELEMENTSIZE == 2)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u16_8e + (lx * sizeof(v256u8))));
				}
				else if (ELEMENTSIZE == 4)
				{
					ssse3idx = _mm256_load_si256((v256u8 *)(_gpuDstToSrcSSSE3_u32_4e + (lx * sizeof(v256u8))));
				}
				
				_mm256_store_si256( (__m256i *)dst + dstX + lx, _mm256_shuffle_epi8(srcVecHi, ssse3idx) );
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
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(v256s8) / ELEMENTSIZE), _mm256_store_si256((v256s8 *)dst + (X), _mm256_load_si256((v256s8 *)src + (X))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		__m256i srcPix[2];
		__m256i dstPix;
		
		for (size_t srcX = 0, dstX = 0; dstX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX+=INTEGERSCALEHINT, dstX++)
		{
			srcPix[0] = _mm256_load_si256((__m256i *)src + srcX + 0);
			srcPix[1] = _mm256_load_si256((__m256i *)src + srcX + 1);
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = _mm256_and_si256(srcPix[0], _mm256_set1_epi32(0x00FF00FF));
				srcPix[1] = _mm256_and_si256(srcPix[1], _mm256_set1_epi32(0x00FF00FF));
				dstPix = _mm256_permute4x64_epi64(_mm256_packus_epi16(srcPix[0], srcPix[1]), 0xD8);
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = _mm256_and_si256(srcPix[0], _mm256_set1_epi32(0x0000FFFF));
				srcPix[1] = _mm256_and_si256(srcPix[1], _mm256_set1_epi32(0x0000FFFF));
				dstPix = _mm256_permute4x64_epi64(_mm256_packus_epi32(srcPix[0], srcPix[1]), 0xD8);
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = _mm256_permutevar8x32_epi32(srcPix[0], _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
				srcPix[1] = _mm256_permutevar8x32_epi32(srcPix[1], _mm256_set_epi32(7, 5, 3, 1, 6, 4, 2, 0));
				dstPix = _mm256_permute2x128_si256(srcPix[0], srcPix[1], 0x20);
				/*
				// Pixel minification algorithm that takes the average value of each color component of the 2x2 pixel group.
				__m256i workingPix[4];
				__m256i finalPix[2];
				
				srcPix[0] = _mm256_load_si256((__m256i *)src + srcX + 0);
				srcPix[1] = _mm256_load_si256((__m256i *)src + srcX + 1);
				srcPix[2] = _mm256_load_si256((__m256i *)src + srcX + (GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(__m256i) / ELEMENTSIZE)) + 0);
				srcPix[3] = _mm256_load_si256((__m256i *)src + srcX + (GPU_FRAMEBUFFER_NATIVE_WIDTH * INTEGERSCALEHINT / (sizeof(__m256i) / ELEMENTSIZE)) + 1);
				
				srcPix[0] = _mm256_permutevar8x32_epi32(srcPix[0], _mm256_set_epi32(7, 5, 6, 4, 3, 1, 2, 0));
				srcPix[1] = _mm256_permutevar8x32_epi32(srcPix[1], _mm256_set_epi32(7, 5, 6, 4, 3, 1, 2, 0));
				srcPix[2] = _mm256_permutevar8x32_epi32(srcPix[2], _mm256_set_epi32(7, 5, 6, 4, 3, 1, 2, 0));
				srcPix[3] = _mm256_permutevar8x32_epi32(srcPix[3], _mm256_set_epi32(7, 5, 6, 4, 3, 1, 2, 0));
				
				workingPix[0] = _mm256_unpacklo_epi8(srcPix[0], _mm256_setzero_si256());
				workingPix[1] = _mm256_unpackhi_epi8(srcPix[0], _mm256_setzero_si256());
				workingPix[2] = _mm256_unpacklo_epi8(srcPix[2], _mm256_setzero_si256());
				workingPix[3] = _mm256_unpackhi_epi8(srcPix[2], _mm256_setzero_si256());
				
				finalPix[0] = _mm256_adds_epi16(workingPix[0], workingPix[1]);
				finalPix[0] = _mm256_adds_epi16(finalPix[0], workingPix[2]);
				finalPix[0] = _mm256_adds_epi16(finalPix[0], workingPix[3]);
				finalPix[0] = _mm256_srli_epi16(finalPix[0], 2);
				
				workingPix[0] = _mm256_unpacklo_epi8(srcPix[1], _mm256_setzero_si256());
				workingPix[1] = _mm256_unpackhi_epi8(srcPix[1], _mm256_setzero_si256());
				workingPix[2] = _mm256_unpacklo_epi8(srcPix[3], _mm256_setzero_si256());
				workingPix[3] = _mm256_unpackhi_epi8(srcPix[3], _mm256_setzero_si256());
				
				finalPix[1] = _mm256_adds_epi16(workingPix[0], workingPix[1]);
				finalPix[1] = _mm256_adds_epi16(finalPix[1], workingPix[2]);
				finalPix[1] = _mm256_adds_epi16(finalPix[1], workingPix[3]);
				finalPix[1] = _mm256_srli_epi16(finalPix[1], 2);
				
				dstPix = _mm256_permute4x64_epi64(_mm256_packus_epi16(finalPix[0], finalPix[1]), 0xD8);
				*/
			}
			
			_mm256_store_si256((__m256i *)dst + dstX, dstPix);
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		static const u8 X = 0x80;
		__m256i srcPix[3];
		
		for (size_t srcX = 0, dstX = 0; dstX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX+=INTEGERSCALEHINT, dstX++)
		{
			srcPix[0] = _mm256_load_si256((__m256i *)src + srcX + 0);
			srcPix[1] = _mm256_load_si256((__m256i *)src + srcX + 1);
			srcPix[2] = _mm256_load_si256((__m256i *)src + srcX + 2);
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = _mm256_shuffle_epi8(srcPix[0], _mm256_set_epi8(30,27,24,21,18, X, X, X, X, X, X, X, X, X, X, X,   X, X, X, X, X, X, X, X, X, X,15,12, 9, 6, 3, 0));
				srcPix[0] = _mm256_permute4x64_epi64(srcPix[0], 0x9C);
				srcPix[0] = _mm256_shuffle_epi8(srcPix[0], _mm256_set_epi8( X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,   X, X, X, X, X,15,14,13,12,11, 5, 4, 3, 2, 1, 0));
				
				srcPix[1] = _mm256_shuffle_epi8(srcPix[1], _mm256_set_epi8( X, X, X, X, X, X, X, X, X, X,31,28,25,22,19,16,  13,10, 7, 4, 1, X, X, X, X, X, X, X, X, X, X, X));
				
				srcPix[2] = _mm256_shuffle_epi8(srcPix[2], _mm256_set_epi8(29,26,23,20,17, X, X, X, X, X, X, X, X, X, X, X,   X, X, X, X, X, X, X, X, X, X, X,14,11, 8, 5, 2));
				srcPix[2] = _mm256_permute4x64_epi64(srcPix[2], 0xC9);
				srcPix[2] = _mm256_shuffle_epi8(srcPix[2], _mm256_set_epi8(31,30,29,28,27,20,19,18,17,16, X, X, X, X, X, X,   X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X));
				
				srcPix[0] = _mm256_or_si256(srcPix[0], srcPix[1]);
				srcPix[0] = _mm256_or_si256(srcPix[0], srcPix[2]);
				
				_mm256_store_si256((__m256i *)dst + dstX, srcPix[0]);
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = _mm256_shuffle_epi8(srcPix[0], _mm256_set_epi8(31,30,25,24,19,18, X, X, X, X, X, X, X, X, X, X,   X, X, X, X, X, X, X, X, X, X,13,12, 7, 6, 1, 0));
				srcPix[0] = _mm256_permute4x64_epi64(srcPix[0], 0x9C);
				srcPix[0] = _mm256_shuffle_epi8(srcPix[0], _mm256_set_epi8( X, X, X, X, X, X, X, X, X, X, X, X, X, X, X, X,   X, X, X, X,15,14,13,12,11,10, 5, 4, 3, 2, 1, 0));
				
				srcPix[1] = _mm256_shuffle_epi8(srcPix[1], _mm256_set_epi8( X, X, X, X, X, X, X, X, X, X,29,28,23,22,17,16,  11,10, 5, 4, X, X, X, X, X, X, X, X, X, X, X, X));
				
				srcPix[2] = _mm256_shuffle_epi8(srcPix[2], _mm256_set_epi8(27,26,21,20, X, X, X, X, X, X, X, X, X, X, X, X,   X, X, X, X,15,14, 9, 8, 3, 2, X, X, X, X, X, X));
				srcPix[2] = _mm256_permutevar8x32_epi32(srcPix[2], _mm256_set_epi32( 7, 2, 1, 0, 0, 0, 0, 0));
				
				srcPix[0] = _mm256_or_si256(srcPix[0], srcPix[1]);
				srcPix[0] = _mm256_or_si256(srcPix[0], srcPix[2]);
				
				_mm256_store_si256((__m256i *)dst + dstX, srcPix[0]);
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = _mm256_and_si256(srcPix[0], _mm256_set_epi32(0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[1] = _mm256_and_si256(srcPix[1], _mm256_set_epi32(0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000));
				srcPix[2] = _mm256_and_si256(srcPix[2], _mm256_set_epi32(0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000));
				
				srcPix[0] = _mm256_permutevar8x32_epi32(srcPix[0], _mm256_set_epi32( 7, 7, 7, 7, 7, 6, 3, 0));
				srcPix[1] = _mm256_permutevar8x32_epi32(srcPix[1], _mm256_set_epi32( 0, 0, 7, 4, 1, 0, 0, 0));
				srcPix[2] = _mm256_permutevar8x32_epi32(srcPix[2], _mm256_set_epi32( 5, 2, 0, 0, 0, 0, 0, 0));
				
				srcPix[0] = _mm256_or_si256(srcPix[0], srcPix[1]);
				srcPix[0] = _mm256_or_si256(srcPix[0], srcPix[2]);
				
				_mm256_store_si256((__m256i *)dst + dstX, srcPix[0]);
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		__m256i srcPix[4];
		__m256i dstPix;
		
		for (size_t srcX = 0, dstX = 0; dstX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m256i) / ELEMENTSIZE); srcX+=INTEGERSCALEHINT, dstX++)
		{
			srcPix[0] = _mm256_load_si256((__m256i *)src + srcX + 0);
			srcPix[1] = _mm256_load_si256((__m256i *)src + srcX + 1);
			srcPix[2] = _mm256_load_si256((__m256i *)src + srcX + 2);
			srcPix[3] = _mm256_load_si256((__m256i *)src + srcX + 3);
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = _mm256_and_si256(srcPix[0], _mm256_set1_epi32(0x000000FF));
				srcPix[1] = _mm256_and_si256(srcPix[1], _mm256_set1_epi32(0x000000FF));
				srcPix[2] = _mm256_and_si256(srcPix[2], _mm256_set1_epi32(0x000000FF));
				srcPix[3] = _mm256_and_si256(srcPix[3], _mm256_set1_epi32(0x000000FF));
				
				srcPix[0] = _mm256_permute4x64_epi64(_mm256_packus_epi16(srcPix[0], srcPix[1]), 0xD8);
				srcPix[1] = _mm256_permute4x64_epi64(_mm256_packus_epi16(srcPix[2], srcPix[3]), 0xD8);
				
				dstPix = _mm256_permute4x64_epi64(_mm256_packus_epi16(srcPix[0], srcPix[1]), 0xD8);
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = _mm256_and_si256(srcPix[0], _mm256_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				srcPix[1] = _mm256_and_si256(srcPix[1], _mm256_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				srcPix[2] = _mm256_and_si256(srcPix[2], _mm256_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				srcPix[3] = _mm256_and_si256(srcPix[3], _mm256_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				
				srcPix[0] = _mm256_permute4x64_epi64(_mm256_packus_epi32(srcPix[0], srcPix[1]), 0xD8);
				srcPix[1] = _mm256_permute4x64_epi64(_mm256_packus_epi32(srcPix[2], srcPix[3]), 0xD8);
				
				dstPix = _mm256_permute4x64_epi64(_mm256_packus_epi32(srcPix[0], srcPix[1]), 0xD8);
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = _mm256_and_si256(srcPix[0], _mm256_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[1] = _mm256_and_si256(srcPix[1], _mm256_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[2] = _mm256_and_si256(srcPix[2], _mm256_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[3] = _mm256_and_si256(srcPix[3], _mm256_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				
				// Here is a special case where we don't need to precede our vpunpckldq instructions with vpermq.
				// Data swizzling is unnecessary here since our desired data is already aligned to their 128-bit
				// lanes as-is.
				srcPix[0] = _mm256_unpacklo_epi32(srcPix[0], srcPix[1]);
				srcPix[1] = _mm256_unpacklo_epi32(srcPix[2], srcPix[3]);
				
				dstPix = _mm256_unpacklo_epi64(srcPix[0], srcPix[1]);
				dstPix = _mm256_permutevar8x32_epi32(dstPix, _mm256_set_epi32(7, 3, 6, 2, 5, 1, 4, 0));
			}
			
			_mm256_store_si256((__m256i *)dst + dstX, dstPix);
		}
	}
	else if ( (INTEGERSCALEHINT >= 5) && (INTEGERSCALEHINT <= 32) )
	{
		if (ELEMENTSIZE == 1)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				((u8 *)dst)[x] = ((u8 *)src)[x * INTEGERSCALEHINT];
			}
		}
		else if (ELEMENTSIZE == 2)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				((u16 *)dst)[x] = ((u16 *)src)[x * INTEGERSCALEHINT];
			}
		}
		else if (ELEMENTSIZE == 4)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=(sizeof(__m256i)/ELEMENTSIZE))
			{
				const v256u32 idx = _mm256_madd_epi16(_mm256_set1_epi16(INTEGERSCALEHINT), _mm256_set_epi16(x, 7, x, 6, x, 5, x, 4, x, 3, x, 2, x, 1, x, 0));
				_mm256_store_si256( (v256u32 *)((u32 *)dst + x), _mm256_i32gather_epi32((int const *)src, idx, sizeof(u32)) );
			}
		}
	}
	else if (INTEGERSCALEHINT > 1)
	{
		const size_t scale = srcWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		if (ELEMENTSIZE == 1)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				((u8 *)dst)[x] = ((u8 *)src)[x * scale];
			}
		}
		else if (ELEMENTSIZE == 2)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				((u16 *)dst)[x] = ((u16 *)src)[x * scale];
			}
		}
		else if (ELEMENTSIZE == 4)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=(sizeof(__m256i)/ELEMENTSIZE))
			{
				const v256u32 idx = _mm256_madd_epi16(_mm256_set1_epi16(scale), _mm256_set_epi16(x, 7, x, 6, x, 5, x, 4, x, 3, x, 2, x, 1, x, 0));
				_mm256_store_si256( (v256u32 *)((u32 *)dst + x), _mm256_i32gather_epi32((int const *)src, idx, sizeof(u32)) );
			}
		}
	}
	else
	{
		if (ELEMENTSIZE == 1)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				( (u8 *)dst)[x] = ( (u8 *)src)[_gpuDstPitchIndex[x]];
			}
		}
		else if (ELEMENTSIZE == 2)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				((u16 *)dst)[x] = ((u16 *)src)[_gpuDstPitchIndex[x]];
			}
		}
		else if (ELEMENTSIZE == 4)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=(sizeof(__m256i)/ELEMENTSIZE))
			{
				const v256u32 idx = _mm256_load_si256((v256u32 *)(_gpuDstPitchIndex + x));
				_mm256_store_si256( (v256u32 *)((u32 *)dst + x), _mm256_i32gather_epi32((int const *)src, idx, sizeof(u32)) );
			}
		}
	}
}

FORCEINLINE v256u16 ColorOperation_AVX2::blend(const v256u16 &colA, const v256u16 &colB, const v256u16 &blendEVA, const v256u16 &blendEVB) const
{
	v256u16 ra;
	v256u16 ga;
	v256u16 ba;
	v256u16 colorBitMask = _mm256_set1_epi16(0x001F);
	
	ra = _mm256_or_si256( _mm256_and_si256(                  colA,      colorBitMask), _mm256_and_si256(_mm256_slli_epi16(colB, 8), _mm256_set1_epi16(0x1F00)) );
	ga = _mm256_or_si256( _mm256_and_si256(_mm256_srli_epi16(colA,  5), colorBitMask), _mm256_and_si256(_mm256_slli_epi16(colB, 3), _mm256_set1_epi16(0x1F00)) );
	ba = _mm256_or_si256( _mm256_and_si256(_mm256_srli_epi16(colA, 10), colorBitMask), _mm256_and_si256(_mm256_srli_epi16(colB, 2), _mm256_set1_epi16(0x1F00)) );
	
	const v256u16 blendAB = _mm256_or_si256(blendEVA, _mm256_slli_epi16(blendEVB, 8));
	ra = _mm256_maddubs_epi16(ra, blendAB);
	ga = _mm256_maddubs_epi16(ga, blendAB);
	ba = _mm256_maddubs_epi16(ba, blendAB);
	
	ra = _mm256_srli_epi16(ra, 4);
	ga = _mm256_srli_epi16(ga, 4);
	ba = _mm256_srli_epi16(ba, 4);
	
	ra = _mm256_min_epi16(ra, colorBitMask);
	ga = _mm256_min_epi16(ga, colorBitMask);
	ba = _mm256_min_epi16(ba, colorBitMask);
	
	return _mm256_or_si256(ra, _mm256_or_si256( _mm256_slli_epi16(ga, 5), _mm256_slli_epi16(ba, 10)) );
}

// Note that if USECONSTANTBLENDVALUESHINT is true, then this method will assume that blendEVA contains identical values
// for each 16-bit vector element, and also that blendEVB contains identical values for each 16-bit vector element. If
// this assumption is broken, then the resulting color will be undefined.
template <NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT>
FORCEINLINE v256u32 ColorOperation_AVX2::blend(const v256u32 &colA, const v256u32 &colB, const v256u16 &blendEVA, const v256u16 &blendEVB) const
{
	v256u16 outColorLo;
	v256u16 outColorHi;
	v256u32 outColor;
	
	v256u16 blendAB = _mm256_or_si256(blendEVA, _mm256_slli_epi16(blendEVB, 8));
	
	if (USECONSTANTBLENDVALUESHINT)
	{
		const v256u16 tempColorA = _mm256_permute4x64_epi64(colA, 0xD8);
		const v256u16 tempColorB = _mm256_permute4x64_epi64(colB, 0xD8);
		
		outColorLo = _mm256_unpacklo_epi8(tempColorA, tempColorB);
		outColorHi = _mm256_unpackhi_epi8(tempColorA, tempColorB);
		
		outColorLo = _mm256_maddubs_epi16(outColorLo, blendAB);
		outColorHi = _mm256_maddubs_epi16(outColorHi, blendAB);
		
		outColorLo = _mm256_srli_epi16(outColorLo, 4);
		outColorHi = _mm256_srli_epi16(outColorHi, 4);
		outColor = _mm256_packus_epi16(outColorLo, outColorHi);
		outColor = _mm256_permute4x64_epi64(outColor, 0xD8);
	}
	else
	{
		const v256u16 tempColorA = _mm256_permute4x64_epi64(colA, 0xD8);
		const v256u16 tempColorB = _mm256_permute4x64_epi64(colB, 0xD8);
		
		outColorLo = _mm256_unpacklo_epi8(tempColorA, tempColorB);
		outColorHi = _mm256_unpackhi_epi8(tempColorA, tempColorB);
		
		blendAB = _mm256_permute4x64_epi64(blendAB, 0xD8);
		const v256u16 blendABLo = _mm256_unpacklo_epi16(blendAB, blendAB);
		const v256u16 blendABHi = _mm256_unpackhi_epi16(blendAB, blendAB);
		outColorLo = _mm256_maddubs_epi16(outColorLo, blendABLo);
		outColorHi = _mm256_maddubs_epi16(outColorHi, blendABHi);
		
		outColorLo = _mm256_srli_epi16(outColorLo, 4);
		outColorHi = _mm256_srli_epi16(outColorHi, 4);
		
		outColor = _mm256_packus_epi16(outColorLo, outColorHi);
		outColor = _mm256_permute4x64_epi64(outColor, 0xD8);
	}
	
	// When the color format is 888, the vpackuswb instruction will naturally clamp
	// the color component values to 255. However, when the color format is 666, the
	// color component values must be clamped to 63. In this case, we must call pminub
	// to do the clamp.
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		outColor = _mm256_min_epu8(outColor, _mm256_set1_epi8(63));
	}
	
	outColor = _mm256_and_si256(outColor, _mm256_set1_epi32(0x00FFFFFF));
	
	return outColor;
}

FORCEINLINE v256u16 ColorOperation_AVX2::blend3D(const v256u32 &colA_Lo, const v256u32 &colA_Hi, const v256u16 &colB) const
{
	// If the color format of B is 555, then the colA_Hi parameter is required.
	// The color format of A is assumed to be RGB666.
	v256u32 ra_lo = _mm256_and_si256(                   colA_Lo,      _mm256_set1_epi32(0x000000FF) );
	v256u32 ga_lo = _mm256_and_si256( _mm256_srli_epi32(colA_Lo,  8), _mm256_set1_epi32(0x000000FF) );
	v256u32 ba_lo = _mm256_and_si256( _mm256_srli_epi32(colA_Lo, 16), _mm256_set1_epi32(0x000000FF) );
	v256u32 aa_lo =                   _mm256_srli_epi32(colA_Lo, 24);
	
	v256u32 ra_hi = _mm256_and_si256(                   colA_Hi,      _mm256_set1_epi32(0x000000FF) );
	v256u32 ga_hi = _mm256_and_si256( _mm256_srli_epi32(colA_Hi,  8), _mm256_set1_epi32(0x000000FF) );
	v256u32 ba_hi = _mm256_and_si256( _mm256_srli_epi32(colA_Hi, 16), _mm256_set1_epi32(0x000000FF) );
	v256u32 aa_hi =                   _mm256_srli_epi32(colA_Hi, 24);
	
	v256u16 ra = _mm256_packus_epi32(ra_lo, ra_hi);
	v256u16 ga = _mm256_packus_epi32(ga_lo, ga_hi);
	v256u16 ba = _mm256_packus_epi32(ba_lo, ba_hi);
	v256u16 aa = _mm256_packus_epi32(aa_lo, aa_hi);
	
	ra = _mm256_permute4x64_epi64(ra, 0xD8);
	ga = _mm256_permute4x64_epi64(ga, 0xD8);
	ba = _mm256_permute4x64_epi64(ba, 0xD8);
	aa = _mm256_permute4x64_epi64(aa, 0xD8);
	
	ra = _mm256_or_si256( ra, _mm256_and_si256(_mm256_slli_epi16(colB, 9), _mm256_set1_epi16(0x3E00)) );
	ga = _mm256_or_si256( ga, _mm256_and_si256(_mm256_slli_epi16(colB, 4), _mm256_set1_epi16(0x3E00)) );
	ba = _mm256_or_si256( ba, _mm256_and_si256(_mm256_srli_epi16(colB, 1), _mm256_set1_epi16(0x3E00)) );
	
	aa = _mm256_adds_epu8(aa, _mm256_set1_epi16(1));
	aa = _mm256_or_si256( aa, _mm256_slli_epi16(_mm256_subs_epu16(_mm256_set1_epi8(32), aa), 8) );
	
	ra = _mm256_maddubs_epi16(ra, aa);
	ga = _mm256_maddubs_epi16(ga, aa);
	ba = _mm256_maddubs_epi16(ba, aa);
	
	ra = _mm256_srli_epi16(ra, 6);
	ga = _mm256_srli_epi16(ga, 6);
	ba = _mm256_srli_epi16(ba, 6);
	
	return _mm256_or_si256( _mm256_or_si256(ra, _mm256_slli_epi16(ga, 5)), _mm256_slli_epi16(ba, 10) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v256u32 ColorOperation_AVX2::blend3D(const v256u32 &colA, const v256u32 &colB) const
{
	// If the color format of B is 666 or 888, then the colA_Hi parameter is ignored.
	// The color format of A is assumed to match the color format of B.
	v256u32 alpha;
	v256u16 alphaLo;
	v256u16 alphaHi;
	
	v256u16 tempColor[2] = {
		_mm256_permute4x64_epi64(colA, 0xD8),
		_mm256_permute4x64_epi64(colB, 0xD8)
	};
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		// Does not work for RGBA8888 color format. The reason is because this
		// algorithm depends on the vpmaddubsw instruction, which multiplies
		// two unsigned 8-bit integers into an intermediate signed 16-bit
		// integer. This means that we can overrun the signed 16-bit value
		// range, which would be limited to [-32767 - 32767]. For example, a
		// color component of value 255 multiplied by an alpha value of 255
		// would equal 65025, which is greater than the upper range of a signed
		// 16-bit value.
		v256u16 tempColorLo = _mm256_unpacklo_epi8(tempColor[0], tempColor[1]);
		v256u16 tempColorHi = _mm256_unpackhi_epi8(tempColor[0], tempColor[1]);
		
		alpha = _mm256_and_si256( _mm256_srli_epi32(colA, 24), _mm256_set1_epi32(0x0000001F) );
		alpha = _mm256_or_si256( alpha, _mm256_or_si256(_mm256_slli_epi32(alpha, 8), _mm256_slli_epi32(alpha, 16)) );
		alpha = _mm256_adds_epu8(alpha, _mm256_set1_epi8(1));
		
		v256u32 invAlpha = _mm256_subs_epu8(_mm256_set1_epi8(32), alpha);
		invAlpha = _mm256_permute4x64_epi64(invAlpha, 0xD8);
		
		alpha = _mm256_permute4x64_epi64(alpha, 0xD8);
		alphaLo = _mm256_unpacklo_epi8(alpha, invAlpha);
		alphaHi = _mm256_unpackhi_epi8(alpha, invAlpha);
		
		tempColorLo = _mm256_maddubs_epi16(tempColorLo, alphaLo);
		tempColorHi = _mm256_maddubs_epi16(tempColorHi, alphaHi);
		
		tempColor[0] = _mm256_srli_epi16(tempColorLo, 5);
		tempColor[1] = _mm256_srli_epi16(tempColorHi, 5);
	}
	else
	{
		v256u16 rgbALo = _mm256_unpacklo_epi8(tempColor[0], _mm256_setzero_si256());
		v256u16 rgbAHi = _mm256_unpackhi_epi8(tempColor[0], _mm256_setzero_si256());
		v256u16 rgbBLo = _mm256_unpacklo_epi8(tempColor[1], _mm256_setzero_si256());
		v256u16 rgbBHi = _mm256_unpackhi_epi8(tempColor[1], _mm256_setzero_si256());
		
		alpha = _mm256_and_si256( _mm256_srli_epi32(colA, 24), _mm256_set1_epi32(0x000000FF) );
		alpha = _mm256_or_si256( alpha, _mm256_or_si256(_mm256_slli_epi32(alpha, 8), _mm256_slli_epi32(alpha, 16)) );
		alpha = _mm256_permute4x64_epi64(alpha, 0xD8);
		
		alphaLo = _mm256_unpacklo_epi8(alpha, _mm256_setzero_si256());
		alphaHi = _mm256_unpackhi_epi8(alpha, _mm256_setzero_si256());
		alphaLo = _mm256_add_epi16(alphaLo, _mm256_set1_epi16(1));
		alphaHi = _mm256_add_epi16(alphaHi, _mm256_set1_epi16(1));
		
		rgbALo = _mm256_add_epi16( _mm256_mullo_epi16(rgbALo, alphaLo), _mm256_mullo_epi16(rgbBLo, _mm256_sub_epi16(_mm256_set1_epi16(256), alphaLo)) );
		rgbAHi = _mm256_add_epi16( _mm256_mullo_epi16(rgbAHi, alphaHi), _mm256_mullo_epi16(rgbBHi, _mm256_sub_epi16(_mm256_set1_epi16(256), alphaHi)) );
		
		tempColor[0] = _mm256_srli_epi16(rgbALo, 8);
		tempColor[1] = _mm256_srli_epi16(rgbAHi, 8);
	}
	
	tempColor[0] = _mm256_packus_epi16(tempColor[0], tempColor[1]);
	tempColor[0] = _mm256_permute4x64_epi64(tempColor[0], 0xD8);
	
	return _mm256_and_si256(tempColor[0], _mm256_set1_epi32(0x00FFFFFF));
}

FORCEINLINE v256u16 ColorOperation_AVX2::increase(const v256u16 &col, const v256u16 &blendEVY) const
{
	v256u16 r = _mm256_and_si256(                   col,      _mm256_set1_epi16(0x001F) );
	v256u16 g = _mm256_and_si256( _mm256_srli_epi16(col,  5), _mm256_set1_epi16(0x001F) );
	v256u16 b = _mm256_and_si256( _mm256_srli_epi16(col, 10), _mm256_set1_epi16(0x001F) );
	
	r = _mm256_add_epi16( r, _mm256_srli_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(_mm256_set1_epi16(31), r), blendEVY), 4) );
	g = _mm256_add_epi16( g, _mm256_srli_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(_mm256_set1_epi16(31), g), blendEVY), 4) );
	b = _mm256_add_epi16( b, _mm256_srli_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(_mm256_set1_epi16(31), b), blendEVY), 4) );
	
	return _mm256_or_si256(r, _mm256_or_si256( _mm256_slli_epi16(g, 5), _mm256_slli_epi16(b, 10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v256u32 ColorOperation_AVX2::increase(const v256u32 &col, const v256u16 &blendEVY) const
{
	const v256u32 tempCol = _mm256_permute4x64_epi64(col, 0xD8);
	v256u16 rgbLo = _mm256_unpacklo_epi8(tempCol, _mm256_setzero_si256());
	v256u16 rgbHi = _mm256_unpackhi_epi8(tempCol, _mm256_setzero_si256());
	
	rgbLo = _mm256_add_epi16( rgbLo, _mm256_srli_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(_mm256_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbLo), blendEVY), 4) );
	rgbHi = _mm256_add_epi16( rgbHi, _mm256_srli_epi16(_mm256_mullo_epi16(_mm256_sub_epi16(_mm256_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbHi), blendEVY), 4) );
	
	return _mm256_and_si256( _mm256_permute4x64_epi64(_mm256_packus_epi16(rgbLo, rgbHi), 0xD8), _mm256_set1_epi32(0x00FFFFFF) );
}

FORCEINLINE v256u16 ColorOperation_AVX2::decrease(const v256u16 &col, const v256u16 &blendEVY) const
{
	v256u16 r = _mm256_and_si256(                   col,      _mm256_set1_epi16(0x001F) );
	v256u16 g = _mm256_and_si256( _mm256_srli_epi16(col,  5), _mm256_set1_epi16(0x001F) );
	v256u16 b = _mm256_and_si256( _mm256_srli_epi16(col, 10), _mm256_set1_epi16(0x001F) );
	
	r = _mm256_sub_epi16( r, _mm256_srli_epi16(_mm256_mullo_epi16(r, blendEVY), 4) );
	g = _mm256_sub_epi16( g, _mm256_srli_epi16(_mm256_mullo_epi16(g, blendEVY), 4) );
	b = _mm256_sub_epi16( b, _mm256_srli_epi16(_mm256_mullo_epi16(b, blendEVY), 4) );
	
	return _mm256_or_si256(r, _mm256_or_si256( _mm256_slli_epi16(g, 5), _mm256_slli_epi16(b, 10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v256u32 ColorOperation_AVX2::decrease(const v256u32 &col, const v256u16 &blendEVY) const
{
	const v256u32 tempCol = _mm256_permute4x64_epi64(col, 0xD8);
	v256u16 rgbLo = _mm256_unpacklo_epi8(tempCol, _mm256_setzero_si256());
	v256u16 rgbHi = _mm256_unpackhi_epi8(tempCol, _mm256_setzero_si256());
	
	rgbLo = _mm256_sub_epi16( rgbLo, _mm256_srli_epi16(_mm256_mullo_epi16(rgbLo, blendEVY), 4) );
	rgbHi = _mm256_sub_epi16( rgbHi, _mm256_srli_epi16(_mm256_mullo_epi16(rgbHi, blendEVY), 4) );
	
	return _mm256_and_si256( _mm256_permute4x64_epi64(_mm256_packus_epi16(rgbLo, rgbHi), 0xD8), _mm256_set1_epi32(0x00FFFFFF) );
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AVX2::_copy16(GPUEngineCompositorInfo &compInfo, const v256u8 &srcLayerID, const v256u16 &src1, const v256u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_or_si256(src0, alphaBits) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_or_si256(src1, alphaBits) );
	}
	else
	{
		v256u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555To6665Opaque_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To6665Opaque_AVX2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555To8888Opaque_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To8888Opaque_AVX2<false>(src1, src32[2], src32[3]);
		}
		
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 0, src32[0] );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 1, src32[1] );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 2, src32[2] );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 3, src32[3] );
	}
	
	if (!ISDEBUGRENDER)
	{
		_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, srcLayerID );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AVX2::_copy32(GPUEngineCompositorInfo &compInfo, const v256u8 &srcLayerID, const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 src16[2] = {
			ColorspaceConvert6665To5551_AVX2<false>(src0, src1),
			ColorspaceConvert6665To5551_AVX2<false>(src2, src3)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_or_si256(src16[0], alphaBits) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_or_si256(src16[1], alphaBits) );
	}
	else
	{
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 0, _mm256_or_si256(src0, alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 1, _mm256_or_si256(src1, alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 2, _mm256_or_si256(src2, alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 3, _mm256_or_si256(src3, alphaBits) );
	}
	
	if (!ISDEBUGRENDER)
	{
		_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, srcLayerID );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AVX2::_copyMask16(GPUEngineCompositorInfo &compInfo, const v256u8 &passMask8, const v256u8 &srcLayerID, const v256u16 &src1, const v256u16 &src0) const
{
	const v256u8 tempPassMask8 = _mm256_permute4x64_epi64(passMask8, 0xD8);
	
	v256u16 passMask16[2] = {
		_mm256_unpacklo_epi8(tempPassMask8, tempPassMask8),
		_mm256_unpackhi_epi8(tempPassMask8, tempPassMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(src0, alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(src1, alphaBits), passMask16[1]) );
	}
	else
	{
		v256u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555To6665Opaque_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To6665Opaque_AVX2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555To8888Opaque_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To8888Opaque_AVX2<false>(src1, src32[2], src32[3]);
		}
		
		passMask16[0] = _mm256_permute4x64_epi64(passMask16[0], 0xD8);
		passMask16[1] = _mm256_permute4x64_epi64(passMask16[1], 0xD8);
		
		const v256u32 passMask32[4]	= {
			_mm256_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm256_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm256_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm256_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], src32[0] );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], src32[1] );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], src32[2] );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], src32[3] );
	}
	
	if (!ISDEBUGRENDER)
	{
		const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
		_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_AVX2::_copyMask32(GPUEngineCompositorInfo &compInfo, const v256u8 &passMask8, const v256u8 &srcLayerID, const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0) const
{
	const v256u8 tempPassMask8 = _mm256_permute4x64_epi64(passMask8, 0xD8);
	
	v256u16 passMask16[2] = {
		_mm256_unpacklo_epi8(tempPassMask8, tempPassMask8),
		_mm256_unpackhi_epi8(tempPassMask8, tempPassMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 src16[2] = {
			ColorspaceConvert6665To5551_AVX2<false>(src0, src1),
			ColorspaceConvert6665To5551_AVX2<false>(src2, src3)
		};
		
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(src16[0], alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(src16[1], alphaBits), passMask16[1]) );
	}
	else
	{
		passMask16[0] = _mm256_permute4x64_epi64(passMask16[0], 0xD8);
		passMask16[1] = _mm256_permute4x64_epi64(passMask16[1], 0xD8);
		
		const v256u32 passMask32[4]	= {
			_mm256_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm256_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm256_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm256_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(src0, alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(src1, alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(src2, alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(src3, alphaBits) );
	}
	
	if (!ISDEBUGRENDER)
	{
		const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
		_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	}
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessUp16(GPUEngineCompositorInfo &compInfo, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u16 &src1, const v256u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_or_si256(colorop_vec.increase(src0, evy16), alphaBits) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_or_si256(colorop_vec.increase(src1, evy16), alphaBits) );
	}
	else
	{
		v256u32 dst[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_AVX2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo666X_AVX2<false>(src1, dst[2], dst[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_AVX2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo888X_AVX2<false>(src1, dst[2], dst[3]);
		}
		
		const v256u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm256_set1_epi32(0x1F000000) : _mm256_set1_epi32(0xFF000000);
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 0, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(dst[0], evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 1, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(dst[1], evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 2, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(dst[2], evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 3, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(dst[3], evy16), alphaBits) );
	}
	
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessUp32(GPUEngineCompositorInfo &compInfo, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		
		const v256u16 src16[2] = {
			ColorspaceConvert6665To5551_AVX2<false>(src0, src1),
			ColorspaceConvert6665To5551_AVX2<false>(src2, src3)
		};
		
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_or_si256(colorop_vec.increase(src16[0], evy16), alphaBits) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_or_si256(colorop_vec.increase(src16[1], evy16), alphaBits) );
	}
	else
	{
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 0, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 1, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 2, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 3, _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits) );
	}
	
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessUpMask16(GPUEngineCompositorInfo &compInfo, const v256u8 &passMask8, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u16 &src1, const v256u16 &src0) const
{
	const v256u8 tempPassMask8 = _mm256_permute4x64_epi64(passMask8, 0xD8);
	
	v256u16 passMask16[2] = {
		_mm256_unpacklo_epi8(tempPassMask8, tempPassMask8),
		_mm256_unpackhi_epi8(tempPassMask8, tempPassMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(colorop_vec.increase(src0, evy16), alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(colorop_vec.increase(src1, evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		v256u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo666X_AVX2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo888X_AVX2<false>(src1, src32[2], src32[3]);
		}
		
		passMask16[0] = _mm256_permute4x64_epi64(passMask16[0], 0xD8);
		passMask16[1] = _mm256_permute4x64_epi64(passMask16[1], 0xD8);
		
		const v256u32 passMask32[4]	= {
			_mm256_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm256_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm256_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm256_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src32[0], evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src32[1], evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src32[2], evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src32[3], evy16), alphaBits) );
	}
	
	const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessUpMask32(GPUEngineCompositorInfo &compInfo, const v256u8 &passMask8, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0) const
{
	const v256u8 tempPassMask8 = _mm256_permute4x64_epi64(passMask8, 0xD8);
	
	v256u16 passMask16[2] = {
		_mm256_unpacklo_epi8(tempPassMask8, tempPassMask8),
		_mm256_unpackhi_epi8(tempPassMask8, tempPassMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 src16[2] = {
			ColorspaceConvert6665To5551_AVX2<false>(src0, src1),
			ColorspaceConvert6665To5551_AVX2<false>(src2, src3)
		};
		
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(colorop_vec.increase(src16[0], evy16), alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(colorop_vec.increase(src16[1], evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		passMask16[0] = _mm256_permute4x64_epi64(passMask16[0], 0xD8);
		passMask16[1] = _mm256_permute4x64_epi64(passMask16[1], 0xD8);
		
		const v256u32 passMask32[4]	= {
			_mm256_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm256_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm256_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm256_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits) );
	}
	
	const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessDown16(GPUEngineCompositorInfo &compInfo, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u16 &src1, const v256u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_or_si256(colorop_vec.decrease(src0, evy16), alphaBits) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_or_si256(colorop_vec.decrease(src1, evy16), alphaBits) );
	}
	else
	{
		v256u32 dst[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_AVX2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo666X_AVX2<false>(src1, dst[2], dst[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_AVX2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo888X_AVX2<false>(src1, dst[2], dst[3]);
		}
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 0, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(dst[0], evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 1, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(dst[1], evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 2, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(dst[2], evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 3, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(dst[3], evy16), alphaBits) );
	}
	
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessDown32(GPUEngineCompositorInfo &compInfo, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		
		const v256u16 src16[2] = {
			ColorspaceConvert6665To5551_AVX2<false>(src0, src1),
			ColorspaceConvert6665To5551_AVX2<false>(src2, src3)
		};
		
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_or_si256(colorop_vec.decrease(src16[0], evy16), alphaBits) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_or_si256(colorop_vec.decrease(src16[1], evy16), alphaBits) );
	}
	else
	{
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 0, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 1, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 2, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits) );
		_mm256_store_si256( (v256u32 *)compInfo.target.lineColor32 + 3, _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits) );
	}
	
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessDownMask16(GPUEngineCompositorInfo &compInfo, const v256u8 &passMask8, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u16 &src1, const v256u16 &src0) const
{
	const v256u8 tempPassMask8 = _mm256_permute4x64_epi64(passMask8, 0xD8);
	
	v256u16 passMask16[2] = {
		_mm256_unpacklo_epi8(tempPassMask8, tempPassMask8),
		_mm256_unpackhi_epi8(tempPassMask8, tempPassMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(colorop_vec.decrease(src0, evy16), alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(colorop_vec.decrease(src1, evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		v256u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo666X_AVX2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_AVX2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo888X_AVX2<false>(src1, src32[2], src32[3]);
		}
		
		passMask16[0] = _mm256_permute4x64_epi64(passMask16[0], 0xD8);
		passMask16[1] = _mm256_permute4x64_epi64(passMask16[1], 0xD8);
		
		const v256u32 passMask32[4]	= {
			_mm256_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm256_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm256_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm256_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src32[0], evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src32[1], evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src32[2], evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src32[3], evy16), alphaBits) );
	}
	
	const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_AVX2::_brightnessDownMask32(GPUEngineCompositorInfo &compInfo, const v256u8 &passMask8, const v256u16 &evy16, const v256u8 &srcLayerID, const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0) const
{
	const v256u8 tempPassMask8 = _mm256_permute4x64_epi64(passMask8, 0xD8);
	
	v256u16 passMask16[2] = {
		_mm256_unpacklo_epi8(tempPassMask8, tempPassMask8),
		_mm256_unpackhi_epi8(tempPassMask8, tempPassMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 src16[2] = {
			ColorspaceConvert6665To5551_AVX2<false>(src0, src1),
			ColorspaceConvert6665To5551_AVX2<false>(src2, src3)
		};
		
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(colorop_vec.decrease(src16[0], evy16), alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(colorop_vec.decrease(src16[1], evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		passMask16[0] = _mm256_permute4x64_epi64(passMask16[0], 0xD8);
		passMask16[1] = _mm256_permute4x64_epi64(passMask16[1], 0xD8);
		
		const v256u32 passMask32[4]	= {
			_mm256_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm256_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm256_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm256_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits) );
	}
	
	const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_AVX2::_unknownEffectMask16(GPUEngineCompositorInfo &compInfo,
														   const v256u8 &passMask8,
														   const v256u16 &evy16,
														   const v256u8 &srcLayerID,
														   const v256u16 &src1, const v256u16 &src0,
														   const v256u8 &srcEffectEnableMask,
														   const v256u8 &dstBlendEnableMaskLUT,
														   const v256u8 &enableColorEffectMask,
														   const v256u8 &spriteAlpha,
														   const v256u8 &spriteMode) const
{
	const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	
	v256u8 dstTargetBlendEnableMask = _mm256_shuffle_epi8(dstBlendEnableMaskLUT, dstLayerID);
	dstTargetBlendEnableMask = _mm256_andnot_si256( _mm256_cmpeq_epi8(dstLayerID, srcLayerID), dstTargetBlendEnableMask );
	
	// Select the color effect based on the BLDCNT target flags.
	const v256u8 colorEffectMask = _mm256_blendv_epi8(_mm256_set1_epi8(ColorEffect_Disable), _mm256_set1_epi8(compInfo.renderState.colorEffect), enableColorEffectMask);
	v256u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : _mm256_setzero_si256();
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	__m256i eva_vec256 = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_set1_epi8(compInfo.renderState.blendEVA) : _mm256_set1_epi16(compInfo.renderState.blendEVA);
	__m256i evb_vec256 = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_set1_epi8(compInfo.renderState.blendEVB) : _mm256_set1_epi16(compInfo.renderState.blendEVB);
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v256u8 isObjTranslucentMask = _mm256_and_si256( dstTargetBlendEnableMask, _mm256_or_si256(_mm256_cmpeq_epi8(spriteMode, _mm256_set1_epi8(OBJMode_Transparent)), _mm256_cmpeq_epi8(spriteMode, _mm256_set1_epi8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v256u8 spriteAlphaMask = _mm256_andnot_si256(_mm256_cmpeq_epi8(spriteAlpha, _mm256_set1_epi8(0xFF)), isObjTranslucentMask);
		eva_vec256 = _mm256_blendv_epi8(eva_vec256, spriteAlpha, spriteAlphaMask);
		evb_vec256 = _mm256_blendv_epi8(evb_vec256, _mm256_sub_epi8(_mm256_set1_epi8(16), spriteAlpha), spriteAlphaMask);
	}
	
	// ----------
	
	__m256i tmpSrc[4];
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc[0] = src0;
		tmpSrc[1] = src1;
		tmpSrc[2] = _mm256_setzero_si256();
		tmpSrc[3] = _mm256_setzero_si256();
	}
	else if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
	{
		ColorspaceConvert555XTo666X_AVX2<false>(src0, tmpSrc[0], tmpSrc[1]);
		ColorspaceConvert555XTo666X_AVX2<false>(src1, tmpSrc[2], tmpSrc[3]);
	}
	else
	{
		ColorspaceConvert555XTo888X_AVX2<false>(src0, tmpSrc[0], tmpSrc[1]);
		ColorspaceConvert555XTo888X_AVX2<false>(src1, tmpSrc[2], tmpSrc[3]);
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v256u8 brightnessMask8 = _mm256_andnot_si256( forceDstTargetBlendMask, _mm256_and_si256(srcEffectEnableMask, _mm256_cmpeq_epi8(colorEffectMask, _mm256_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const int brightnessUpMaskValue = _mm256_movemask_epi8(brightnessMask8);
			
			if (brightnessUpMaskValue != 0x00000000)
			{
				const v256u8 brightnessMask8pre = _mm256_permute4x64_epi64(brightnessMask8, 0xD8);
				const v256u16 brightnessMask16[2] = {
					_mm256_unpacklo_epi8(brightnessMask8pre, brightnessMask8pre),
					_mm256_unpackhi_epi8(brightnessMask8pre, brightnessMask8pre)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.increase(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.increase(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v256u16 brightnessMask16pre[2] = {
						_mm256_permute4x64_epi64(brightnessMask16[0], 0xD8),
						_mm256_permute4x64_epi64(brightnessMask16[1], 0xD8)
					};
					const v256u32 brightnessMask32[4] = {
						_mm256_unpacklo_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpackhi_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpacklo_epi16(brightnessMask16pre[1], brightnessMask16pre[1]),
						_mm256_unpackhi_epi16(brightnessMask16pre[1], brightnessMask16pre[1])
					};
					
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm256_blendv_epi8( tmpSrc[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm256_blendv_epi8( tmpSrc[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v256u8 brightnessMask8 = _mm256_andnot_si256( forceDstTargetBlendMask, _mm256_and_si256(srcEffectEnableMask, _mm256_cmpeq_epi8(colorEffectMask, _mm256_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const int brightnessDownMaskValue = _mm256_movemask_epi8(brightnessMask8);
			
			if (brightnessDownMaskValue != 0x00000000)
			{
				const v256u8 brightnessMask8pre = _mm256_permute4x64_epi64(brightnessMask8, 0xD8);
				const v256u16 brightnessMask16[2] = {
					_mm256_unpacklo_epi8(brightnessMask8pre, brightnessMask8pre),
					_mm256_unpackhi_epi8(brightnessMask8pre, brightnessMask8pre)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.decrease(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.decrease(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v256u16 brightnessMask16pre[2] = {
						_mm256_permute4x64_epi64(brightnessMask16[0], 0xD8),
						_mm256_permute4x64_epi64(brightnessMask16[1], 0xD8)
					};
					const v256u32 brightnessMask32[4] = {
						_mm256_unpacklo_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpackhi_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpacklo_epi16(brightnessMask16pre[1], brightnessMask16pre[1]),
						_mm256_unpackhi_epi16(brightnessMask16pre[1], brightnessMask16pre[1])
					};
					
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm256_blendv_epi8( tmpSrc[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm256_blendv_epi8( tmpSrc[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v256u8 blendMask8 = _mm256_or_si256( forceDstTargetBlendMask, _mm256_and_si256(_mm256_and_si256(srcEffectEnableMask, dstTargetBlendEnableMask), _mm256_cmpeq_epi8(colorEffectMask, _mm256_set1_epi8(ColorEffect_Blend))) );
	const int blendMaskValue = _mm256_movemask_epi8(blendMask8);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		if (blendMaskValue != 0x00000000)
		{
			const v256u8 blendMask8pre = _mm256_permute4x64_epi64(blendMask8, 0xD8);
			const v256u16 blendMask16[2] = {
				_mm256_unpacklo_epi8(blendMask8pre, blendMask8pre),
				_mm256_unpackhi_epi8(blendMask8pre, blendMask8pre)
			};
			
			v256u16 blendSrc16[2];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					//blendSrc16[0] = colorop_vec.blend3D(src0, src1, dst16[0]);
					//blendSrc16[1] = colorop_vec.blend3D(src2, src3, dst16[1]);
					printf("GPU: 3D layers cannot be in RGBA5551 format. To composite a 3D layer, use the _unknownEffectMask32() method instead.\n");
					assert(false);
					break;
					
				case GPULayerType_BG:
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], eva_vec256, evb_vec256);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], eva_vec256, evb_vec256);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const v256u8 eva_pre = _mm256_permute4x64_epi64(eva_vec256, 0xD8);
					const v256u16 tempEVA[2] = {
						_mm256_unpacklo_epi8(eva_pre, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(eva_pre, _mm256_setzero_si256())
					};
					
					const v256u8 evb_pre = _mm256_permute4x64_epi64(evb_vec256, 0xD8);
					const v256u16 tempEVB[2] = {
						_mm256_unpacklo_epi8(evb_pre, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(evb_pre, _mm256_setzero_si256())
					};
					
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], tempEVA[0], tempEVB[0]);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], tempEVA[1], tempEVB[1]);
					break;
				}
			}
			
			tmpSrc[0] = _mm256_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
			tmpSrc[1] = _mm256_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		}
		
		// Store the final colors.
		const v256u8 passMask8pre = _mm256_permute4x64_epi64(passMask8, 0xD8);
		const v256u16 passMask16[2] = {
			_mm256_unpacklo_epi8(passMask8pre, passMask8pre),
			_mm256_unpackhi_epi8(passMask8pre, passMask8pre)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(tmpSrc[0], alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(tmpSrc[1], alphaBits), passMask16[1]) );
	}
	else
	{
		if (blendMaskValue != 0x00000000)
		{
			const v256u8 blendMask8pre = _mm256_permute4x64_epi64(blendMask8, 0xD8);
			const v256u16 blendMask16[2] = {
				_mm256_unpacklo_epi8(blendMask8pre, blendMask8pre),
				_mm256_unpackhi_epi8(blendMask8pre, blendMask8pre)
			};
			
			const v256u32 dst32[4] = {
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 0),
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 1),
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 2),
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 3)
			};
			
			v256u32 blendSrc32[4];
			
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
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[0], dst32[0], eva_vec256, evb_vec256);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[1], dst32[1], eva_vec256, evb_vec256);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[2], dst32[2], eva_vec256, evb_vec256);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[3], dst32[3], eva_vec256, evb_vec256);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					const v256u8 eva_pre = _mm256_permute4x64_epi64(eva_vec256, 0xD8);
					v256u16 tempBlendLo = _mm256_permute4x64_epi64( _mm256_unpacklo_epi8(eva_pre, eva_pre), 0xD8);
					v256u16 tempBlendHi = _mm256_permute4x64_epi64( _mm256_unpackhi_epi8(eva_pre, eva_pre), 0xD8);
					
					const v256u16 tempEVA[4] = {
						_mm256_unpacklo_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpacklo_epi8(tempBlendHi, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendHi, _mm256_setzero_si256())
					};
					
					const v256u8 evb_pre = _mm256_permute4x64_epi64(evb_vec256, 0xD8);
					tempBlendLo = _mm256_permute4x64_epi64( _mm256_unpacklo_epi8(evb_pre, evb_pre), 0xD8);
					tempBlendHi = _mm256_permute4x64_epi64( _mm256_unpackhi_epi8(evb_pre, evb_pre), 0xD8);
					
					const v256u16 tempEVB[4] = {
						_mm256_unpacklo_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpacklo_epi8(tempBlendHi, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendHi, _mm256_setzero_si256())
					};
					
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[0], dst32[0], tempEVA[0], tempEVB[0]);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[1], dst32[1], tempEVA[1], tempEVB[1]);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[2], dst32[2], tempEVA[2], tempEVB[2]);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[3], dst32[3], tempEVA[3], tempEVB[3]);
					break;
				}
			}
			
			const v256u16 blendMask16pre[2] = {
				_mm256_permute4x64_epi64(blendMask16[0], 0xD8),
				_mm256_permute4x64_epi64(blendMask16[1], 0xD8)
			};
			const v256u32 blendMask32[4] = {
				_mm256_unpacklo_epi16(blendMask16pre[0], blendMask16pre[0]),
				_mm256_unpackhi_epi16(blendMask16pre[0], blendMask16pre[0]),
				_mm256_unpacklo_epi16(blendMask16pre[1], blendMask16pre[1]),
				_mm256_unpackhi_epi16(blendMask16pre[1], blendMask16pre[1])
			};
			
			tmpSrc[0] = _mm256_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
			tmpSrc[1] = _mm256_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
			tmpSrc[2] = _mm256_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
			tmpSrc[3] = _mm256_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		}
		
		// Store the final colors.
		const v256u8 passMask8pre = _mm256_permute4x64_epi64(passMask8, 0xD8);
		const v256u16 passMask16pre[2] = {
			_mm256_permute4x64_epi64( _mm256_unpacklo_epi8(passMask8pre, passMask8pre), 0xD8 ),
			_mm256_permute4x64_epi64( _mm256_unpackhi_epi8(passMask8pre, passMask8pre), 0xD8 )
		};
		
		const v256u32 passMask32[4] = {
			_mm256_unpacklo_epi16(passMask16pre[0], passMask16pre[0]),
			_mm256_unpackhi_epi16(passMask16pre[0], passMask16pre[0]),
			_mm256_unpacklo_epi16(passMask16pre[1], passMask16pre[1]),
			_mm256_unpackhi_epi16(passMask16pre[1], passMask16pre[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(tmpSrc[0], alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(tmpSrc[1], alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(tmpSrc[2], alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(tmpSrc[3], alphaBits) );
	}
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_AVX2::_unknownEffectMask32(GPUEngineCompositorInfo &compInfo,
														   const v256u8 &passMask8,
														   const v256u16 &evy16,
														   const v256u8 &srcLayerID,
														   const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0,
														   const v256u8 &srcEffectEnableMask,
														   const v256u8 &dstBlendEnableMaskLUT,
														   const v256u8 &enableColorEffectMask,
														   const v256u8 &spriteAlpha,
														   const v256u8 &spriteMode) const
{
	const v256u8 dstLayerID = _mm256_load_si256((v256u8 *)compInfo.target.lineLayerID);
	_mm256_store_si256( (v256u8 *)compInfo.target.lineLayerID, _mm256_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	
	v256u8 dstTargetBlendEnableMask = _mm256_shuffle_epi8(dstBlendEnableMaskLUT, dstLayerID);
	dstTargetBlendEnableMask = _mm256_andnot_si256( _mm256_cmpeq_epi8(dstLayerID, srcLayerID), dstTargetBlendEnableMask );
	
	// Select the color effect based on the BLDCNT target flags.
	const v256u8 colorEffectMask = _mm256_blendv_epi8(_mm256_set1_epi8(ColorEffect_Disable), _mm256_set1_epi8(compInfo.renderState.colorEffect), enableColorEffectMask);
	v256u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : _mm256_setzero_si256();
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	__m256i eva_vec256 = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_set1_epi8(compInfo.renderState.blendEVA) : _mm256_set1_epi16(compInfo.renderState.blendEVA);
	__m256i evb_vec256 = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_set1_epi8(compInfo.renderState.blendEVB) : _mm256_set1_epi16(compInfo.renderState.blendEVB);
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v256u8 isObjTranslucentMask = _mm256_and_si256( dstTargetBlendEnableMask, _mm256_or_si256(_mm256_cmpeq_epi8(spriteMode, _mm256_set1_epi8(OBJMode_Transparent)), _mm256_cmpeq_epi8(spriteMode, _mm256_set1_epi8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v256u8 spriteAlphaMask = _mm256_andnot_si256(_mm256_cmpeq_epi8(spriteAlpha, _mm256_set1_epi8(0xFF)), isObjTranslucentMask);
		eva_vec256 = _mm256_blendv_epi8(eva_vec256, spriteAlpha, spriteAlphaMask);
		evb_vec256 = _mm256_blendv_epi8(evb_vec256, _mm256_sub_epi8(_mm256_set1_epi8(16), spriteAlpha), spriteAlphaMask);
	}
	
	// ----------
	
	__m256i tmpSrc[4];
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc[0] = ColorspaceConvert6665To5551_AVX2<false>(src0, src1);
		tmpSrc[1] = ColorspaceConvert6665To5551_AVX2<false>(src2, src3);
		tmpSrc[2] = _mm256_setzero_si256();
		tmpSrc[3] = _mm256_setzero_si256();
	}
	else
	{
		tmpSrc[0] = src0;
		tmpSrc[1] = src1;
		tmpSrc[2] = src2;
		tmpSrc[3] = src3;
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v256u8 brightnessMask8 = _mm256_andnot_si256( forceDstTargetBlendMask, _mm256_and_si256(srcEffectEnableMask, _mm256_cmpeq_epi8(colorEffectMask, _mm256_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const int brightnessUpMaskValue = _mm256_movemask_epi8(brightnessMask8);
			
			if (brightnessUpMaskValue != 0x00000000)
			{
				const v256u8 brightnessMask8pre = _mm256_permute4x64_epi64(brightnessMask8, 0xD8);
				const v256u16 brightnessMask16[2] = {
					_mm256_unpacklo_epi8(brightnessMask8pre, brightnessMask8pre),
					_mm256_unpackhi_epi8(brightnessMask8pre, brightnessMask8pre)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.increase(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.increase(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v256u16 brightnessMask16pre[2] = {
						_mm256_permute4x64_epi64(brightnessMask16[0], 0xD8),
						_mm256_permute4x64_epi64(brightnessMask16[1], 0xD8)
					};
					const v256u32 brightnessMask32[4] = {
						_mm256_unpacklo_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpackhi_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpacklo_epi16(brightnessMask16pre[1], brightnessMask16pre[1]),
						_mm256_unpackhi_epi16(brightnessMask16pre[1], brightnessMask16pre[1])
					};
					
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm256_blendv_epi8( tmpSrc[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm256_blendv_epi8( tmpSrc[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v256u8 brightnessMask8 = _mm256_andnot_si256( forceDstTargetBlendMask, _mm256_and_si256(srcEffectEnableMask, _mm256_cmpeq_epi8(colorEffectMask, _mm256_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const int brightnessDownMaskValue = _mm256_movemask_epi8(brightnessMask8);
			
			if (brightnessDownMaskValue != 0x00000000)
			{
				const v256u8 brightnessMask8pre = _mm256_permute4x64_epi64(brightnessMask8, 0xD8);
				const v256u16 brightnessMask16[2] = {
					_mm256_unpacklo_epi8(brightnessMask8pre, brightnessMask8pre),
					_mm256_unpackhi_epi8(brightnessMask8pre, brightnessMask8pre)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.decrease(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.decrease(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v256u16 brightnessMask16pre[2] = {
						_mm256_permute4x64_epi64(brightnessMask16[0], 0xD8),
						_mm256_permute4x64_epi64(brightnessMask16[1], 0xD8)
					};
					const v256u32 brightnessMask32[4] = {
						_mm256_unpacklo_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpackhi_epi16(brightnessMask16pre[0], brightnessMask16pre[0]),
						_mm256_unpacklo_epi16(brightnessMask16pre[1], brightnessMask16pre[1]),
						_mm256_unpackhi_epi16(brightnessMask16pre[1], brightnessMask16pre[1])
					};
					
					tmpSrc[0] = _mm256_blendv_epi8( tmpSrc[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm256_blendv_epi8( tmpSrc[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm256_blendv_epi8( tmpSrc[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm256_blendv_epi8( tmpSrc[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v256u8 blendMask8 = _mm256_or_si256( forceDstTargetBlendMask, _mm256_and_si256(_mm256_and_si256(srcEffectEnableMask, dstTargetBlendEnableMask), _mm256_cmpeq_epi8(colorEffectMask, _mm256_set1_epi8(ColorEffect_Blend))) );
	const int blendMaskValue = _mm256_movemask_epi8(blendMask8);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v256u16 dst16[2] = {
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 0),
			_mm256_load_si256((v256u16 *)compInfo.target.lineColor16 + 1)
		};
		
		if (blendMaskValue != 0x00000000)
		{
			const v256u8 blendMask8pre = _mm256_permute4x64_epi64(blendMask8, 0xD8);
			const v256u16 blendMask16[2] = {
				_mm256_unpacklo_epi8(blendMask8pre, blendMask8pre),
				_mm256_unpackhi_epi8(blendMask8pre, blendMask8pre)
			};
			
			v256u16 blendSrc16[2];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc16[0] = colorop_vec.blend3D(src0, src1, dst16[0]);
					blendSrc16[1] = colorop_vec.blend3D(src2, src3, dst16[1]);
					break;
					
				case GPULayerType_BG:
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], eva_vec256, evb_vec256);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], eva_vec256, evb_vec256);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const v256u8 eva_pre = _mm256_permute4x64_epi64(eva_vec256, 0xD8);
					const v256u16 tempEVA[2] = {
						_mm256_unpacklo_epi8(eva_pre, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(eva_pre, _mm256_setzero_si256())
					};
					
					const v256u8 evb_pre = _mm256_permute4x64_epi64(evb_vec256, 0xD8);
					const v256u16 tempEVB[2] = {
						_mm256_unpacklo_epi8(evb_pre, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(evb_pre, _mm256_setzero_si256())
					};
					
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], tempEVA[0], tempEVB[0]);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], tempEVA[1], tempEVB[1]);
					break;
				}
			}
			
			tmpSrc[0] = _mm256_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
			tmpSrc[1] = _mm256_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		}
		
		// Store the final colors.
		const v256u8 passMask8pre = _mm256_permute4x64_epi64(passMask8, 0xD8);
		const v256u16 passMask16[2] = {
			_mm256_unpacklo_epi8(passMask8pre, passMask8pre),
			_mm256_unpackhi_epi8(passMask8pre, passMask8pre)
		};
		
		const v256u16 alphaBits = _mm256_set1_epi16(0x8000);
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 0, _mm256_blendv_epi8(dst16[0], _mm256_or_si256(tmpSrc[0], alphaBits), passMask16[0]) );
		_mm256_store_si256( (v256u16 *)compInfo.target.lineColor16 + 1, _mm256_blendv_epi8(dst16[1], _mm256_or_si256(tmpSrc[1], alphaBits), passMask16[1]) );
	}
	else
	{
		if (blendMaskValue != 0x00000000)
		{
			const v256u8 blendMask8pre = _mm256_permute4x64_epi64(blendMask8, 0xD8);
			const v256u16 blendMask16[2] = {
				_mm256_unpacklo_epi8(blendMask8pre, blendMask8pre),
				_mm256_unpackhi_epi8(blendMask8pre, blendMask8pre)
			};
			
			const v256u32 dst32[4] = {
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 0),
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 1),
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 2),
				_mm256_load_si256((v256u32 *)compInfo.target.lineColor32 + 3),
			};
			
			v256u32 blendSrc32[4];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc32[0] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[0], dst32[0]);
					blendSrc32[1] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[1], dst32[1]);
					blendSrc32[2] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[2], dst32[2]);
					blendSrc32[3] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[3], dst32[3]);
					break;
					
				case GPULayerType_BG:
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[0], dst32[0], eva_vec256, evb_vec256);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[1], dst32[1], eva_vec256, evb_vec256);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[2], dst32[2], eva_vec256, evb_vec256);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[3], dst32[3], eva_vec256, evb_vec256);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					const v256u8 eva_pre = _mm256_permute4x64_epi64(eva_vec256, 0xD8);
					v256u16 tempBlendLo = _mm256_permute4x64_epi64( _mm256_unpacklo_epi8(eva_pre, eva_pre), 0xD8 );
					v256u16 tempBlendHi = _mm256_permute4x64_epi64( _mm256_unpackhi_epi8(eva_pre, eva_pre), 0xD8 );
					
					const v256u16 tempEVA[4] = {
						_mm256_unpacklo_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpacklo_epi8(tempBlendHi, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendHi, _mm256_setzero_si256())
					};
					
					const v256u8 evb_pre = _mm256_permute4x64_epi64(evb_vec256, 0xD8);
					tempBlendLo = _mm256_permute4x64_epi64( _mm256_unpacklo_epi8(evb_pre, evb_pre), 0xD8 );
					tempBlendHi = _mm256_permute4x64_epi64( _mm256_unpackhi_epi8(evb_pre, evb_pre), 0xD8 );
					
					const v256u16 tempEVB[4] = {
						_mm256_unpacklo_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendLo, _mm256_setzero_si256()),
						_mm256_unpacklo_epi8(tempBlendHi, _mm256_setzero_si256()),
						_mm256_unpackhi_epi8(tempBlendHi, _mm256_setzero_si256())
					};
					
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[0], dst32[0], tempEVA[0], tempEVB[0]);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[1], dst32[1], tempEVA[1], tempEVB[1]);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[2], dst32[2], tempEVA[2], tempEVB[2]);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[3], dst32[3], tempEVA[3], tempEVB[3]);
					break;
				}
			}
			
			const v256u16 blendMask16pre[2] = {
				_mm256_permute4x64_epi64(blendMask16[0], 0xD8),
				_mm256_permute4x64_epi64(blendMask16[1], 0xD8)
			};
			const v256u32 blendMask32[4] = {
				_mm256_unpacklo_epi16(blendMask16pre[0], blendMask16pre[0]),
				_mm256_unpackhi_epi16(blendMask16pre[0], blendMask16pre[0]),
				_mm256_unpacklo_epi16(blendMask16pre[1], blendMask16pre[1]),
				_mm256_unpackhi_epi16(blendMask16pre[1], blendMask16pre[1])
			};
			
			tmpSrc[0] = _mm256_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
			tmpSrc[1] = _mm256_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
			tmpSrc[2] = _mm256_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
			tmpSrc[3] = _mm256_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		}
		
		// Store the final colors.
		const v256u8 passMask8pre = _mm256_permute4x64_epi64(passMask8, 0xD8);
		const v256u16 passMask16pre[2] = {
			_mm256_permute4x64_epi64( _mm256_unpacklo_epi8(passMask8pre, passMask8pre), 0xD8 ),
			_mm256_permute4x64_epi64( _mm256_unpackhi_epi8(passMask8pre, passMask8pre), 0xD8 )
		};
		
		const v256u32 passMask32[4] = {
			_mm256_unpacklo_epi16(passMask16pre[0], passMask16pre[0]),
			_mm256_unpackhi_epi16(passMask16pre[0], passMask16pre[0]),
			_mm256_unpacklo_epi16(passMask16pre[1], passMask16pre[1]),
			_mm256_unpackhi_epi16(passMask16pre[1], passMask16pre[1])
		};
		
		const v256u32 alphaBits = _mm256_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 0), passMask32[0], _mm256_or_si256(tmpSrc[0], alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 1), passMask32[1], _mm256_or_si256(tmpSrc[1], alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 2), passMask32[2], _mm256_or_si256(tmpSrc[2], alphaBits) );
		_mm256_maskstore_epi32( (int *)compInfo.target.lineColor32 + ((sizeof(v256u32)/sizeof(int)) * 3), passMask32[3], _mm256_or_si256(tmpSrc[3], alphaBits) );
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void PixelOperation_AVX2::Composite16(GPUEngineCompositorInfo &compInfo,
												  const bool didAllPixelsPass,
												  const v256u8 &passMask8,
												  const v256u16 &evy16,
												  const v256u8 &srcLayerID,
												  const v256u16 &src1, const v256u16 &src0,
												  const v256u8 &srcEffectEnableMask,
												  const v256u8 &dstBlendEnableMaskLUT,
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
				const v256u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? _mm256_load_si256((v256u8 *)enableColorEffectPtr) : _mm256_set1_epi8(0xFF);
				const v256u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_load_si256((v256u8 *)sprAlphaPtr) : _mm256_setzero_si256();
				const v256u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_load_si256((v256u8 *)sprModePtr) : _mm256_setzero_si256();
				
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
FORCEINLINE void PixelOperation_AVX2::Composite32(GPUEngineCompositorInfo &compInfo,
												  const bool didAllPixelsPass,
												  const v256u8 &passMask8,
												  const v256u16 &evy16,
												  const v256u8 &srcLayerID,
												  const v256u32 &src3, const v256u32 &src2, const v256u32 &src1, const v256u32 &src0,
												  const v256u8 &srcEffectEnableMask,
												  const v256u8 &dstBlendEnableMaskLUT,
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
				const v256u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? _mm256_load_si256((v256u8 *)enableColorEffectPtr) : _mm256_set1_epi8(0xFF);
				const v256u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_load_si256((v256u8 *)sprAlphaPtr) : _mm256_setzero_si256();
				const v256u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? _mm256_load_si256((v256u8 *)sprModePtr) : _mm256_setzero_si256();
				
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
	
	for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=sizeof(v256u16))
	{
		const v256u16 dstColor16[2] = {
			_mm256_load_si256((v256u16 *)(this->_deferredColorNative + x) + 0),
			_mm256_load_si256((v256u16 *)(this->_deferredColorNative + x) + 1)
		};
		
		if (ISFIRSTLINE)
		{
			const v256u8 indexVec = _mm256_load_si256((v256u8 *)(this->_deferredIndexNative + x));
			const v256u8 idxMask8 = _mm256_permute4x64_epi64( _mm256_cmpeq_epi8(indexVec, _mm256_setzero_si256()), 0xD8 );
			const v256u16 idxMask16[2] = {
				_mm256_unpacklo_epi8(idxMask8, idxMask8),
				_mm256_unpackhi_epi8(idxMask8, idxMask8)
			};
			
			const v256u16 mosaicColor16[2] = {
				_mm256_blendv_epi8(_mm256_and_si256(dstColor16[0], _mm256_set1_epi16(0x7FFF)), _mm256_set1_epi16(0xFFFF), idxMask16[0]),
				_mm256_blendv_epi8(_mm256_and_si256(dstColor16[1], _mm256_set1_epi16(0x7FFF)), _mm256_set1_epi16(0xFFFF), idxMask16[1])
			};
			
			const v256u16 mosaicSetColorMask8 = _mm256_permute4x64_epi64( _mm256_cmpeq_epi16(_mm256_loadu_si256((v256u8 *)(compInfo.renderState.mosaicWidthBG->begin + x)), _mm256_setzero_si256()), 0xD8 );
			const v256u16 mosaicSetColorMask16[2] = {
				_mm256_unpacklo_epi8(mosaicSetColorMask8, mosaicSetColorMask8),
				_mm256_unpackhi_epi8(mosaicSetColorMask8, mosaicSetColorMask8)
			};
			
			__m256i mosaicColorOut[2];
			mosaicColorOut[0] = _mm256_blendv_epi8(mosaicColor16[0], _mm256_loadu_si256((v256u16 *)(mosaicColorBG + x) + 0), mosaicSetColorMask16[0]);
			mosaicColorOut[1] = _mm256_blendv_epi8(mosaicColor16[1], _mm256_loadu_si256((v256u16 *)(mosaicColorBG + x) + 1), mosaicSetColorMask16[1]);
			
			_mm256_storeu_si256((v256u16 *)(mosaicColorBG + x) + 0, mosaicColorOut[0]);
			_mm256_storeu_si256((v256u16 *)(mosaicColorBG + x) + 1, mosaicColorOut[1]);
		}
		
		const v256u32 outColor32idx[4] = {
			_mm256_loadu_si256((v256u32 *)(compInfo.renderState.mosaicWidthBG->trunc32 + x) + 0),
			_mm256_loadu_si256((v256u32 *)(compInfo.renderState.mosaicWidthBG->trunc32 + x) + 1),
			_mm256_loadu_si256((v256u32 *)(compInfo.renderState.mosaicWidthBG->trunc32 + x) + 2),
			_mm256_loadu_si256((v256u32 *)(compInfo.renderState.mosaicWidthBG->trunc32 + x) + 3)
		};
		
		const v256u16 outColor32[4] = {
			_mm256_and_si256( _mm256_i32gather_epi32((int const *)mosaicColorBG, outColor32idx[0], sizeof(u16)), _mm256_set1_epi32(0x0000FFFF) ),
			_mm256_and_si256( _mm256_i32gather_epi32((int const *)mosaicColorBG, outColor32idx[1], sizeof(u16)), _mm256_set1_epi32(0x0000FFFF) ),
			_mm256_and_si256( _mm256_i32gather_epi32((int const *)mosaicColorBG, outColor32idx[2], sizeof(u16)), _mm256_set1_epi32(0x0000FFFF) ),
			_mm256_and_si256( _mm256_i32gather_epi32((int const *)mosaicColorBG, outColor32idx[3], sizeof(u16)), _mm256_set1_epi32(0x0000FFFF) )
		};
		
		const v256u16 outColor16[2] = {
			_mm256_permute4x64_epi64( _mm256_packus_epi32(outColor32[0], outColor32[1]), 0xD8 ),
			_mm256_permute4x64_epi64( _mm256_packus_epi32(outColor32[2], outColor32[3]), 0xD8 )
		};
		
		const v256u16 writeColorMask16[2] = {
			_mm256_cmpeq_epi16(outColor16[0], _mm256_set1_epi16(0xFFFF)),
			_mm256_cmpeq_epi16(outColor16[1], _mm256_set1_epi16(0xFFFF))
		};
		
		_mm256_store_si256( (v256u16 *)(this->_deferredColorNative + x) + 0, _mm256_blendv_epi8(outColor16[0], dstColor16[0], writeColorMask16[0]) );
		_mm256_store_si256( (v256u16 *)(this->_deferredColorNative + x) + 1, _mm256_blendv_epi8(outColor16[1], dstColor16[1], writeColorMask16[1]) );
		
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeNativeLineOBJ_LoopOp(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const FragmentColor *__restrict srcColorNative32)
{
	static const size_t step = sizeof(v256u8);
	
	const bool isUsingSrc32 = (srcColorNative32 != NULL);
	const v256u16 evy16 = _mm256_set1_epi16(compInfo.renderState.blendEVY);
	const v256u8 srcLayerID = _mm256_set1_epi8(compInfo.renderState.selectedLayerID);
	const v256u8 srcEffectEnableMask = _mm256_set1_epi8(compInfo.renderState.srcEffectEnable[GPULayerID_OBJ]);
	const v256u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm256_load_si256((v256u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm256_setzero_si256();
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=step, srcColorNative16+=step, srcColorNative32+=step, compInfo.target.xNative+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		v256u8 passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm256_load_si256((v256u8 *)(this->_didPassWindowTestNative[GPULayerID_OBJ] + i));
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm256_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
			
			didAllPixelsPass = (passMaskValue == 0xFFFFFFFF);
		}
		else
		{
			passMask8 = _mm256_set1_epi8(0xFF);
			passMaskValue = 0xFFFFFFFF;
			didAllPixelsPass = true;
		}
		
		if (isUsingSrc32)
		{
			const v256u32 src[4] = {
				_mm256_load_si256((v256u32 *)srcColorNative32 + 0),
				_mm256_load_si256((v256u32 *)srcColorNative32 + 1),
				_mm256_load_si256((v256u32 *)srcColorNative32 + 2),
				_mm256_load_si256((v256u32 *)srcColorNative32 + 3)
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
			const v256u16 src[2] = {
				_mm256_load_si256((v256u16 *)srcColorNative16 + 0),
				_mm256_load_si256((v256u16 *)srcColorNative16 + 1)
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
	static const size_t step = sizeof(v256u8);
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v256u16 evy16 = _mm256_set1_epi16(compInfo.renderState.blendEVY);
	const v256u8 srcLayerID = _mm256_set1_epi8(compInfo.renderState.selectedLayerID);
	const v256u8 srcEffectEnableMask = _mm256_set1_epi8(compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID]);
	const v256u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm256_load_si256((v256u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm256_setzero_si256();
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		v256u8 passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST || (LAYERTYPE == GPULayerType_BG))
		{
			if (WILLPERFORMWINDOWTEST)
			{
				// Do the window test.
				passMask8 = _mm256_load_si256((v256u8 *)(windowTestPtr + compInfo.target.xCustom));
			}
			
			if (LAYERTYPE == GPULayerType_BG)
			{
				// Do the index test. Pixels with an index value of 0 are rejected.
				const v256u8 idxPassMask8 = _mm256_cmpeq_epi8(_mm256_load_si256((v256u8 *)(srcIndexCustom + compInfo.target.xCustom)), _mm256_setzero_si256());
				
				if (WILLPERFORMWINDOWTEST)
				{
					passMask8 = _mm256_andnot_si256(idxPassMask8, passMask8);
				}
				else
				{
					passMask8 = _mm256_xor_si256(idxPassMask8, _mm256_set1_epi32(0xFFFFFFFF));
				}
			}
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm256_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
			
			didAllPixelsPass = (passMaskValue == 0xFFFFFFFF);
		}
		else
		{
			passMask8 = _mm256_set1_epi8(0xFF);
			passMaskValue = 0xFFFFFFFF;
			didAllPixelsPass = true;
		}
		
		const v256u16 src[2] = {
			_mm256_load_si256((v256u16 *)(srcColorCustom16 + compInfo.target.xCustom) + 0),
			_mm256_load_si256((v256u16 *)(srcColorCustom16 + compInfo.target.xCustom) + 1)
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
	static const size_t step = sizeof(v256u8);
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v256u16 evy16 = _mm256_set1_epi16(compInfo.renderState.blendEVY);
	const v256u8 srcLayerID = _mm256_set1_epi8(compInfo.renderState.selectedLayerID);
	const v256u8 srcEffectEnableMask = _mm256_set1_epi8(compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID]);
	const v256u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm256_load_si256((v256u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm256_setzero_si256();
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		v256u8 passMask8;
		int passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm256_load_si256((v256u8 *)(windowTestPtr + compInfo.target.xCustom));
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm256_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = _mm256_set1_epi8(0xFF);
			passMaskValue = 0xFFFFFFFF;
		}
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
			case NDSColorFormat_BGR666_Rev:
			{
				const v256u16 src16[2] = {
					_mm256_load_si256((v256u16 *)((u16 *)vramColorPtr + i) + 0),
					_mm256_load_si256((v256u16 *)((u16 *)vramColorPtr + i) + 1)
				};
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v256u8 tempPassMask = _mm256_packus_epi16( _mm256_srli_epi16(src16[0], 15), _mm256_srli_epi16(src16[1], 15) );
					tempPassMask = _mm256_permute4x64_epi64(tempPassMask, 0xD8);
					tempPassMask = _mm256_cmpeq_epi8(tempPassMask, _mm256_set1_epi8(1));
					
					passMask8 = _mm256_and_si256(tempPassMask, passMask8);
					passMaskValue = _mm256_movemask_epi8(passMask8);
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				const bool didAllPixelsPass = (passMaskValue == 0xFFFFFFFF);
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
				const v256u32 src32[4] = {
					_mm256_load_si256((v256u32 *)((FragmentColor *)vramColorPtr + i) + 0),
					_mm256_load_si256((v256u32 *)((FragmentColor *)vramColorPtr + i) + 1),
					_mm256_load_si256((v256u32 *)((FragmentColor *)vramColorPtr + i) + 2),
					_mm256_load_si256((v256u32 *)((FragmentColor *)vramColorPtr + i) + 3)
				};
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v256u8 tempPassMask = _mm256_packus_epi16( _mm256_permute4x64_epi64( _mm256_packus_epi32(_mm256_srli_epi32(src32[0], 24), _mm256_srli_epi32(src32[1], 24)), 0xD8 ),
															   _mm256_permute4x64_epi64( _mm256_packus_epi32(_mm256_srli_epi32(src32[2], 24), _mm256_srli_epi32(src32[3], 24)), 0xD8 ) );
					tempPassMask = _mm256_permute4x64_epi64(tempPassMask, 0xD8);
					tempPassMask = _mm256_cmpeq_epi8(tempPassMask, _mm256_setzero_si256());
					
					passMask8 = _mm256_andnot_si256(tempPassMask, passMask8);
					passMaskValue = _mm256_movemask_epi8(passMask8);
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				const bool didAllPixelsPass = (passMaskValue == 0xFFFFFFFF);
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
	size_t i = 0;
	
	static const size_t step = sizeof(v256u16);
	const v256u8 prioVec8 = _mm256_set1_epi8(prio);
	
	const size_t ssePixCount = length - (length % step);
	for (; i < ssePixCount; i+=step, spriteX+=step, frameX+=step)
	{
		const v256u8 prioTabVec8 = _mm256_loadu_si256((v256u8 *)(prioTab + frameX));
		const v256u16 color16Lo = _mm256_loadu_si256((v256u16 *)(vramBuffer + spriteX) + 0);
		const v256u16 color16Hi = _mm256_loadu_si256((v256u16 *)(vramBuffer + spriteX) + 1);
		
		const v256u8 alphaCompare = _mm256_cmpeq_epi8( _mm256_permute4x64_epi64(_mm256_packus_epi16(_mm256_srli_epi16(color16Lo, 15), _mm256_srli_epi16(color16Hi, 15)), 0xD8), _mm256_set1_epi8(0x01) );
		const v256u8 prioCompare = _mm256_cmpgt_epi8(prioTabVec8, prioVec8);
		
		const v256u8 combinedCompare = _mm256_and_si256(prioCompare, alphaCompare);
		const v256u8 combinedComparePre = _mm256_permute4x64_epi64(combinedCompare, 0xD8);
		const v256u16 combinedLoCompare = _mm256_unpacklo_epi8(combinedComparePre, combinedComparePre);
		const v256u16 combinedHiCompare = _mm256_unpackhi_epi8(combinedComparePre, combinedComparePre);
		
		_mm256_storeu_si256( (v256u16 *)(dst + frameX) + 0, _mm256_blendv_epi8(_mm256_loadu_si256((v256u16 *)(dst + frameX) + 0), color16Lo, combinedLoCompare) );
		_mm256_storeu_si256( (v256u16 *)(dst + frameX) + 1, _mm256_blendv_epi8(_mm256_loadu_si256((v256u16 *)(dst + frameX) + 1), color16Hi, combinedHiCompare) );
		_mm256_storeu_si256( (v256u8 *)(prioTab + frameX),  _mm256_blendv_epi8(prioTabVec8, prioVec8, combinedCompare) );
		
		if (!ISDEBUGRENDER)
		{
			_mm256_storeu_si256( (v256u8 *)(dst_alpha + frameX),     _mm256_blendv_epi8(_mm256_loadu_si256((v256u8 *)(dst_alpha + frameX)), _mm256_set1_epi8(spriteAlpha + 1), combinedCompare) );
			_mm256_storeu_si256( (v256u8 *)(typeTab + frameX),       _mm256_blendv_epi8(_mm256_loadu_si256((v256u8 *)(typeTab + frameX)), _mm256_set1_epi8(OBJMode_Bitmap), combinedCompare) );
			_mm256_storeu_si256( (v256u8 *)(this->_sprNum + frameX), _mm256_blendv_epi8(_mm256_loadu_si256((v256u8 *)(this->_sprNum + frameX)), _mm256_set1_epi8(spriteNum), combinedCompare) );
		}
	}
	
	return i;
}

void GPUEngineBase::_PerformWindowTestingNative(GPUEngineCompositorInfo &compInfo, const size_t layerID, const u8 *__restrict win0, const u8 *__restrict win1, const u8 *__restrict winObj, u8 *__restrict didPassWindowTestNative, u8 *__restrict enableColorEffectNative)
{
	const v256u8 *__restrict win0Ptr = (const v256u8 *__restrict)win0;
	const v256u8 *__restrict win1Ptr = (const v256u8 *__restrict)win1;
	const v256u8 *__restrict winObjPtr = (const v256u8 *__restrict)winObj;
	
	v256u8 *__restrict didPassWindowTestNativePtr = (v256u8 *__restrict)didPassWindowTestNative;
	v256u8 *__restrict enableColorEffectNativePtr = (v256u8 *__restrict)enableColorEffectNative;
	
	v256u8 didPassWindowTest;
	v256u8 enableColorEffect;
	
	v256u8 win0HandledMask;
	v256u8 win1HandledMask;
	v256u8 winOBJHandledMask;
	v256u8 winOUTHandledMask;
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH/sizeof(v256u8); i++)
	{
		didPassWindowTest = _mm256_setzero_si256();
		enableColorEffect = _mm256_setzero_si256();
		
		win0HandledMask = _mm256_setzero_si256();
		win1HandledMask = _mm256_setzero_si256();
		winOBJHandledMask = _mm256_setzero_si256();
		
		// Window 0 has the highest priority, so always check this first.
		if (win0Ptr != NULL)
		{
			const v256u8 win0Enable = _mm256_set1_epi8(compInfo.renderState.WIN0_enable[layerID]);
			const v256u8 win0Effect = _mm256_set1_epi8(compInfo.renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			win0HandledMask = _mm256_cmpeq_epi8(_mm256_load_si256(win0Ptr + i), _mm256_set1_epi8(1));
			didPassWindowTest = _mm256_and_si256(win0HandledMask, win0Enable);
			enableColorEffect = _mm256_and_si256(win0HandledMask, win0Effect);
		}
		
		// Window 1 has medium priority, and is checked after Window 0.
		if (win1Ptr != NULL)
		{
			const v256u8 win1Enable = _mm256_set1_epi8(compInfo.renderState.WIN1_enable[layerID]);
			const v256u8 win1Effect = _mm256_set1_epi8(compInfo.renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			win1HandledMask = _mm256_andnot_si256(win0HandledMask, _mm256_cmpeq_epi8(_mm256_load_si256(win1Ptr + i), _mm256_set1_epi8(1)));
			didPassWindowTest = _mm256_blendv_epi8(didPassWindowTest, win1Enable, win1HandledMask);
			enableColorEffect = _mm256_blendv_epi8(enableColorEffect, win1Effect, win1HandledMask);
		}
		
		// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
		if (winObjPtr != NULL)
		{
			const v256u8 winObjEnable = _mm256_set1_epi8(compInfo.renderState.WINOBJ_enable[layerID]);
			const v256u8 winObjEffect = _mm256_set1_epi8(compInfo.renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			winOBJHandledMask = _mm256_andnot_si256( _mm256_or_si256(win0HandledMask, win1HandledMask), _mm256_cmpeq_epi8(_mm256_load_si256(winObjPtr + i), _mm256_set1_epi8(1)) );
			didPassWindowTest = _mm256_blendv_epi8(didPassWindowTest, winObjEnable, winOBJHandledMask);
			enableColorEffect = _mm256_blendv_epi8(enableColorEffect, winObjEffect, winOBJHandledMask);
		}
		
		// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
		// This has the lowest priority, and is always checked last.
		const v256u8 winOutEnable = _mm256_set1_epi8(compInfo.renderState.WINOUT_enable[layerID]);
		const v256u8 winOutEffect = _mm256_set1_epi8(compInfo.renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG]);
		
		winOUTHandledMask = _mm256_xor_si256( _mm256_or_si256(win0HandledMask, _mm256_or_si256(win1HandledMask, winOBJHandledMask)), _mm256_set1_epi32(0xFFFFFFFF) );
		didPassWindowTest = _mm256_blendv_epi8(didPassWindowTest, winOutEnable, winOUTHandledMask);
		enableColorEffect = _mm256_blendv_epi8(enableColorEffect, winOutEffect, winOUTHandledMask);
		
		_mm256_store_si256(didPassWindowTestNativePtr + i, didPassWindowTest);
		_mm256_store_si256(enableColorEffectNativePtr + i, enableColorEffect);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
size_t GPUEngineA::_RenderLine_Layer3D_LoopOp(GPUEngineCompositorInfo &compInfo, const u8 *__restrict windowTestPtr, const u8 *__restrict colorEffectEnablePtr, const FragmentColor *__restrict srcLinePtr)
{
	static const size_t step = sizeof(v256u32);
	
	const size_t vecPixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v256u16 evy16 = _mm256_set1_epi16(compInfo.renderState.blendEVY);
	const v256u8 srcLayerID = _mm256_set1_epi8(compInfo.renderState.selectedLayerID);
	const v256u8 srcEffectEnableMask = _mm256_set1_epi8(compInfo.renderState.srcEffectEnable[GPULayerID_BG0]);
	const v256u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm256_load_si256((v256u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm256_setzero_si256();
	
	size_t i = 0;
	for (; i < vecPixCount; i+=step, srcLinePtr+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		// Determine which pixels pass by doing the window test and the alpha test.
		v256u8 passMask8;
		int passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm256_load_si256((v256u8 *)(windowTestPtr + compInfo.target.xCustom));
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm256_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = _mm256_set1_epi8(0xFF);
			passMaskValue = 0xFFFFFFFF;
		}
		
		const v256u32 src[4] = {
			_mm256_load_si256((v256u32 *)srcLinePtr + 0),
			_mm256_load_si256((v256u32 *)srcLinePtr + 1),
			_mm256_load_si256((v256u32 *)srcLinePtr + 2),
			_mm256_load_si256((v256u32 *)srcLinePtr + 3)
		};
		
		// Do the alpha test. Pixels with an alpha value of 0 are rejected.
		const v256u32 srcAlpha = _mm256_permute4x64_epi64( _mm256_packus_epi16( _mm256_permute4x64_epi64( _mm256_packus_epi32(_mm256_srli_epi32(src[0], 24), _mm256_srli_epi32(src[1], 24)), 0xD8 ),
																			    _mm256_permute4x64_epi64( _mm256_packus_epi32(_mm256_srli_epi32(src[2], 24), _mm256_srli_epi32(src[3], 24)), 0xD8 ) ), 0xD8 );
		
		passMask8 = _mm256_andnot_si256(_mm256_cmpeq_epi8(srcAlpha, _mm256_setzero_si256()), passMask8);
		
		// If none of the pixels within the vector pass, then reject them all at once.
		passMaskValue = _mm256_movemask_epi8(passMask8);
		if (passMaskValue == 0)
		{
			continue;
		}
		
		// Write out the pixels.
		const bool didAllPixelsPass = (passMaskValue == 0xFFFFFFFF);
		pixelop_vec.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D, WILLPERFORMWINDOWTEST>(compInfo,
		                                                                                              didAllPixelsPass,
		                                                                                              passMask8, evy16,
		                                                                                              srcLayerID,
		                                                                                              src[3], src[2], src[1], src[0],
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
	const v256u16 blendEVA_vec = _mm256_set1_epi16(blendEVA);
	const v256u16 blendEVB_vec = _mm256_set1_epi16(blendEVB);
	const v256u8 blendAB = _mm256_or_si256( blendEVA_vec, _mm256_slli_epi16(blendEVB_vec, 8) );
	
	__m256i srcA_vec;
	__m256i srcB_vec;
	__m256i dstColor;
	
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? length * sizeof(u32) / sizeof(v256u32) : length * sizeof(u16) / sizeof(v256u16);
	for (; i < vecCount; i++)
	{
		srcA_vec = _mm256_load_si256((__m256i *)srcA + i);
		srcB_vec = _mm256_load_si256((__m256i *)srcB + i);
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			// Get color masks based on if the alpha value is 0. Colors with an alpha value
			// equal to 0 are rejected.
			v256u32 srcA_alpha = _mm256_and_si256(srcA_vec, _mm256_set1_epi32(0xFF000000));
			v256u32 srcB_alpha = _mm256_and_si256(srcB_vec, _mm256_set1_epi32(0xFF000000));
			v256u32 srcA_masked = _mm256_andnot_si256(_mm256_cmpeq_epi32(srcA_alpha, _mm256_setzero_si256()), srcA_vec);
			v256u32 srcB_masked = _mm256_andnot_si256(_mm256_cmpeq_epi32(srcB_alpha, _mm256_setzero_si256()), srcB_vec);
			
			v256u16 outColorLo;
			v256u16 outColorHi;
			
			const v256u32 srcA_maskedPre = _mm256_permute4x64_epi64(srcA_masked, 0xD8);
			const v256u32 srcB_maskedPre = _mm256_permute4x64_epi64(srcB_masked, 0xD8);
			
			// Temporarily convert the color component values from 8-bit to 16-bit, and then
			// do the blend calculation.
			outColorLo = _mm256_unpacklo_epi8(srcA_maskedPre, srcB_maskedPre);
			outColorHi = _mm256_unpackhi_epi8(srcA_maskedPre, srcB_maskedPre);
			
			outColorLo = _mm256_maddubs_epi16(outColorLo, blendAB);
			outColorHi = _mm256_maddubs_epi16(outColorHi, blendAB);
			
			outColorLo = _mm256_srli_epi16(outColorLo, 4);
			outColorHi = _mm256_srli_epi16(outColorHi, 4);
			
			// Convert the color components back from 16-bit to 8-bit using a saturated pack.
			dstColor = _mm256_packus_epi16(outColorLo, outColorHi);
			dstColor = _mm256_permute4x64_epi64(dstColor, 0xD8);
			
			// Add the alpha components back in.
			dstColor = _mm256_and_si256(dstColor, _mm256_set1_epi32(0x00FFFFFF));
			dstColor = _mm256_or_si256(dstColor, srcA_alpha);
			dstColor = _mm256_or_si256(dstColor, srcB_alpha);
		}
		else
		{
			v256u16 srcA_alpha = _mm256_and_si256(srcA_vec, _mm256_set1_epi16(0x8000));
			v256u16 srcB_alpha = _mm256_and_si256(srcB_vec, _mm256_set1_epi16(0x8000));
			v256u16 srcA_masked = _mm256_andnot_si256( _mm256_cmpeq_epi16(srcA_alpha, _mm256_setzero_si256()), srcA_vec );
			v256u16 srcB_masked = _mm256_andnot_si256( _mm256_cmpeq_epi16(srcB_alpha, _mm256_setzero_si256()), srcB_vec );
			v256u16 colorBitMask = _mm256_set1_epi16(0x001F);
			
			v256u16 ra;
			v256u16 ga;
			v256u16 ba;
			
			ra = _mm256_or_si256( _mm256_and_si256(                  srcA_masked,      colorBitMask), _mm256_and_si256(_mm256_slli_epi16(srcB_masked, 8), _mm256_set1_epi16(0x1F00)) );
			ga = _mm256_or_si256( _mm256_and_si256(_mm256_srli_epi16(srcA_masked,  5), colorBitMask), _mm256_and_si256(_mm256_slli_epi16(srcB_masked, 3), _mm256_set1_epi16(0x1F00)) );
			ba = _mm256_or_si256( _mm256_and_si256(_mm256_srli_epi16(srcA_masked, 10), colorBitMask), _mm256_and_si256(_mm256_srli_epi16(srcB_masked, 2), _mm256_set1_epi16(0x1F00)) );
			
			ra = _mm256_maddubs_epi16(ra, blendAB);
			ga = _mm256_maddubs_epi16(ga, blendAB);
			ba = _mm256_maddubs_epi16(ba, blendAB);
			
			ra = _mm256_srli_epi16(ra, 4);
			ga = _mm256_srli_epi16(ga, 4);
			ba = _mm256_srli_epi16(ba, 4);
			
			ra = _mm256_min_epi16(ra, colorBitMask);
			ga = _mm256_min_epi16(ga, colorBitMask);
			ba = _mm256_min_epi16(ba, colorBitMask);
			
			dstColor = _mm256_or_si256( _mm256_or_si256(_mm256_or_si256(ra, _mm256_slli_epi16(ga,  5)), _mm256_slli_epi16(ba, 10)), _mm256_or_si256(srcA_alpha, srcB_alpha) );
		}
		
		_mm256_store_si256((__m256i *)dst + i, dstColor);
	}
	
	return (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? i * sizeof(v256u32) / sizeof(u32) : i * sizeof(v256u16) / sizeof(u16);
}

template <NDSColorFormat OUTPUTFORMAT>
size_t NDSDisplay::_ApplyMasterBrightnessUp_LoopOp(void *__restrict dst, const size_t pixCount, const u8 intensityClamped)
{
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? pixCount * sizeof(u32) / sizeof(v256u32) : pixCount * sizeof(u16) / sizeof(v256u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			v256u16 dstColor = _mm256_load_si256((v256u16 *)dst + i);
			dstColor = colorop_vec.increase(dstColor, _mm256_set1_epi16(intensityClamped));
			dstColor = _mm256_or_si256(dstColor, _mm256_set1_epi16(0x8000));
			_mm256_store_si256((v256u16 *)dst + i, dstColor);
		}
		else
		{
			v256u32 dstColor = _mm256_load_si256((v256u32 *)dst + i);
			dstColor = colorop_vec.increase<OUTPUTFORMAT>(dstColor, _mm256_set1_epi16(intensityClamped));
			dstColor = _mm256_or_si256(dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm256_set1_epi32(0x1F000000) : _mm256_set1_epi32(0xFF000000));
			_mm256_store_si256((v256u32 *)dst + i, dstColor);
		}
	}
	
	return (i * sizeof(__m256i));
}

template <NDSColorFormat OUTPUTFORMAT>
size_t NDSDisplay::_ApplyMasterBrightnessDown_LoopOp(void *__restrict dst, const size_t pixCount, const u8 intensityClamped)
{
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? pixCount * sizeof(u32) / sizeof(v256u32) : pixCount * sizeof(u16) / sizeof(v256u16);
	for (; i < vecCount; i++)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			v256u16 dstColor = _mm256_load_si256((v256u16 *)dst + i);
			dstColor = colorop_vec.decrease(dstColor, _mm256_set1_epi16(intensityClamped));
			dstColor = _mm256_or_si256(dstColor, _mm256_set1_epi16(0x8000));
			_mm256_store_si256((v256u16 *)dst + i, dstColor);
		}
		else
		{
			v256u32 dstColor = _mm256_load_si256((v256u32 *)dst + i);
			dstColor = colorop_vec.decrease<OUTPUTFORMAT>(dstColor, _mm256_set1_epi16(intensityClamped));
			dstColor = _mm256_or_si256(dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm256_set1_epi32(0x1F000000) : _mm256_set1_epi32(0xFF000000));
			_mm256_store_si256((v256u32 *)dst + i, dstColor);
		}
	}
	
	return (i * sizeof(__m256i));
}

#endif // ENABLE_AVX2
