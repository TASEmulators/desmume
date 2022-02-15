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

#ifndef ENABLE_SSE2
	#error This code requires SSE2 support.
	#warning This error might occur if this file is compiled directly. Do not compile this file directly, as it is already included in GPU_Operations.cpp.
#else

#include "GPU_Operations_SSE2.h"
#include <emmintrin.h>


static const ColorOperation_SSE2 colorop_vec;
static const PixelOperation_SSE2 pixelop_vec;

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
{
	if (INTEGERSCALEHINT == 0)
	{
		memcpy(dst, src, dstWidth * ELEMENTSIZE);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), _mm_store_si128((__m128i *)dst + (X), _mm_load_si128((__m128i *)src + (X))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		__m128i srcVec;
		__m128i srcPixOut[2];
		
		switch (ELEMENTSIZE)
		{
			case 1:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcVec = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi8(srcVec, srcVec); \
							  srcPixOut[1] = _mm_unpackhi_epi8(srcVec, srcVec); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcVec = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi8(srcVec, srcVec); \
							  srcPixOut[1] = _mm_unpackhi_epi8(srcVec, srcVec); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + 1, srcPixOut[1]); \
							  );
				}
				break;
			}
				
			case 2:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcVec = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi16(srcVec, srcVec); \
							  srcPixOut[1] = _mm_unpackhi_epi16(srcVec, srcVec); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcVec = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi16(srcVec, srcVec); \
							  srcPixOut[1] = _mm_unpackhi_epi16(srcVec, srcVec); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + 1, srcPixOut[1]); \
							  );
				}
				break;
			}
				
			case 4:
			{
				// If we're also doing vertical expansion, then the total number of instructions for a fully
				// unrolled loop is 448 instructions. Therefore, let's not unroll the loop in this case in
				// order to avoid overusing the CPU's instruction cache.
				for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
				{
					srcVec = _mm_load_si128((__m128i *)src + srcX);
					srcPixOut[0] = _mm_shuffle_epi32(srcVec, 0x50);
					srcPixOut[1] = _mm_shuffle_epi32(srcVec, 0xFA);
					
					_mm_store_si128((__m128i *)dst + dstX + 0, srcPixOut[0]);
					_mm_store_si128((__m128i *)dst + dstX + 1, srcPixOut[1]);
					
					if (SCALEVERTICAL)
					{
						_mm_store_si128((__m128i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE) * INTEGERSCALEHINT) * 1) + 0, srcPixOut[0]);
						_mm_store_si128((__m128i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE) * INTEGERSCALEHINT) * 1) + 1, srcPixOut[1]);
					}
				}
				break;
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		__m128i srcPixOut[3];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m128i srcVec = _mm_load_si128((__m128i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(srcVec, _mm_set_epi8( 5, 4, 4, 4, 3, 3, 3, 2, 2, 2, 1, 1, 1, 0, 0, 0));
				srcPixOut[1] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(10,10, 9, 9, 9, 8, 8, 8, 7, 7, 7, 6, 6, 6, 5, 5));
				srcPixOut[2] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(15,15,15,14,14,14,13,13,13,12,12,12,11,11,11,10));
#else
				__m128i src8As32[4];
				src8As32[0] = _mm_unpacklo_epi8(srcVec, srcVec);
				src8As32[1] = _mm_unpackhi_epi8(srcVec, srcVec);
				src8As32[2] = _mm_unpacklo_epi8(src8As32[1], src8As32[1]);
				src8As32[3] = _mm_unpackhi_epi8(src8As32[1], src8As32[1]);
				src8As32[1] = _mm_unpackhi_epi8(src8As32[0], src8As32[0]);
				src8As32[0] = _mm_unpacklo_epi8(src8As32[0], src8As32[0]);
				
				src8As32[0] = _mm_and_si128(src8As32[0], _mm_set_epi32(0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF));
				src8As32[1] = _mm_and_si128(src8As32[1], _mm_set_epi32(0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF));
				src8As32[2] = _mm_and_si128(src8As32[2], _mm_set_epi32(0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF));
				src8As32[3] = _mm_and_si128(src8As32[3], _mm_set_epi32(0x000000FF, 0x000000FF, 0x000000FF, 0x000000FF));
				
				__m128i srcWorking[4];
				
				srcWorking[0] = _mm_shuffle_epi32(src8As32[0], 0x40);
				srcWorking[1] = _mm_shuffle_epi32(src8As32[0], 0xA5);
				srcWorking[2] = _mm_shuffle_epi32(src8As32[0], 0xFE);
				srcWorking[3] = _mm_shuffle_epi32(src8As32[1], 0x40);
				srcPixOut[0] = _mm_packus_epi16( _mm_packus_epi16(srcWorking[0], srcWorking[1]), _mm_packus_epi16(srcWorking[2], srcWorking[3]) );
				
				srcWorking[0] = _mm_shuffle_epi32(src8As32[1], 0xA5);
				srcWorking[1] = _mm_shuffle_epi32(src8As32[1], 0xFE);
				srcWorking[2] = _mm_shuffle_epi32(src8As32[2], 0x40);
				srcWorking[3] = _mm_shuffle_epi32(src8As32[2], 0xA5);
				srcPixOut[1] = _mm_packus_epi16( _mm_packus_epi16(srcWorking[0], srcWorking[1]), _mm_packus_epi16(srcWorking[2], srcWorking[3]) );
				
				srcWorking[0] = _mm_shuffle_epi32(src8As32[2], 0xFE);
				srcWorking[1] = _mm_shuffle_epi32(src8As32[3], 0x40);
				srcWorking[2] = _mm_shuffle_epi32(src8As32[3], 0xA5);
				srcWorking[3] = _mm_shuffle_epi32(src8As32[3], 0xFE);
				srcPixOut[2] = _mm_packus_epi16( _mm_packus_epi16(srcWorking[0], srcWorking[1]), _mm_packus_epi16(srcWorking[2], srcWorking[3]) );
#endif
			}
			else if (ELEMENTSIZE == 2)
			{
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(srcVec, _mm_set_epi8( 5, 4, 5, 4, 3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(11,10, 9, 8, 9, 8, 9, 8, 7, 6, 7, 6, 7, 6, 5, 4));
				srcPixOut[2] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(15,14,15,14,15,14,13,12,13,12,13,12,11,10,11,10));
#else
				const __m128i src16lo = _mm_shuffle_epi32(srcVec, 0x44);
				const __m128i src16hi = _mm_shuffle_epi32(srcVec, 0xEE);
				
				srcPixOut[0] = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16lo, 0x40), 0xA5);
				srcPixOut[1] = _mm_shufflehi_epi16(_mm_shufflelo_epi16(srcVec,  0xFE), 0x40);
				srcPixOut[2] = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16hi, 0xA5), 0xFE);
#endif
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm_shuffle_epi32(srcVec, 0x40);
				srcPixOut[1] = _mm_shuffle_epi32(srcVec, 0xA5);
				srcPixOut[2] = _mm_shuffle_epi32(srcVec, 0xFE);
			}
			
			for (size_t lx = 0; lx < (size_t)INTEGERSCALEHINT; lx++)
			{
				_mm_store_si128((__m128i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < (size_t)INTEGERSCALEHINT; lx++)
					{
						_mm_store_si128((__m128i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		__m128i srcPixOut[4];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE); srcX++, dstX+=INTEGERSCALEHINT)
		{
			const __m128i srcVec = _mm_load_si128((__m128i *)src + srcX);
			
			if (ELEMENTSIZE == 1)
			{
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(srcVec, _mm_set_epi8( 3, 3, 3, 3, 2, 2, 2, 2, 1, 1, 1, 1, 0, 0, 0, 0));
				srcPixOut[1] = _mm_shuffle_epi8(srcVec, _mm_set_epi8( 7, 7, 7, 7, 6, 6, 6, 6, 5, 5, 5, 5, 4, 4, 4, 4));
				srcPixOut[2] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(11,11,11,11,10,10,10,10, 9, 9, 9, 9, 8, 8, 8, 8));
				srcPixOut[3] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(15,15,15,15,14,14,14,14,13,13,13,13,12,12,12,12));
#else
				const __m128i src8_lo  = _mm_unpacklo_epi8(srcVec, srcVec);
				const __m128i src8_hi  = _mm_unpackhi_epi8(srcVec, srcVec);
				
				srcPixOut[0] = _mm_unpacklo_epi8(src8_lo, src8_lo);
				srcPixOut[1] = _mm_unpackhi_epi8(src8_lo, src8_lo);
				srcPixOut[2] = _mm_unpacklo_epi8(src8_hi, src8_hi);
				srcPixOut[3] = _mm_unpackhi_epi8(src8_hi, src8_hi);
#endif
			}
			else if (ELEMENTSIZE == 2)
			{
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(srcVec, _mm_set_epi8( 3, 2, 3, 2, 3, 2, 3, 2, 1, 0, 1, 0, 1, 0, 1, 0));
				srcPixOut[1] = _mm_shuffle_epi8(srcVec, _mm_set_epi8( 7, 6, 7, 6, 7, 6, 7, 6, 5, 4, 5, 4, 5, 4, 5, 4));
				srcPixOut[2] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(11,10,11,10,11,10,11,10, 9, 8, 9, 8, 9, 8, 9, 8));
				srcPixOut[3] = _mm_shuffle_epi8(srcVec, _mm_set_epi8(15,14,15,14,15,14,15,14,13,12,13,12,13,12,13,12));
#else
				const __m128i src16_lo = _mm_unpacklo_epi16(srcVec, srcVec);
				const __m128i src16_hi = _mm_unpackhi_epi16(srcVec, srcVec);
				
				srcPixOut[0] = _mm_unpacklo_epi16(src16_lo, src16_lo);
				srcPixOut[1] = _mm_unpackhi_epi16(src16_lo, src16_lo);
				srcPixOut[2] = _mm_unpacklo_epi16(src16_hi, src16_hi);
				srcPixOut[3] = _mm_unpackhi_epi16(src16_hi, src16_hi);
#endif
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPixOut[0] = _mm_shuffle_epi32(srcVec, 0x00);
				srcPixOut[1] = _mm_shuffle_epi32(srcVec, 0x55);
				srcPixOut[2] = _mm_shuffle_epi32(srcVec, 0xAA);
				srcPixOut[3] = _mm_shuffle_epi32(srcVec, 0xFF);
			}
			
			for (size_t lx = 0; lx < (size_t)INTEGERSCALEHINT; lx++)
			{
				_mm_store_si128((__m128i *)dst + dstX + lx, srcPixOut[lx]);
			}
			
			if (SCALEVERTICAL)
			{
				for (size_t ly = 1; ly < (size_t)INTEGERSCALEHINT; ly++)
				{
					for (size_t lx = 0; lx < (size_t)INTEGERSCALEHINT; lx++)
					{
						_mm_store_si128((__m128i *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE) * INTEGERSCALEHINT) * ly) + lx, srcPixOut[lx]);
					}
				}
			}
		}
	}
#ifdef ENABLE_SSSE3
	else if (INTEGERSCALEHINT > 1)
	{
		const size_t scale = dstWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE); srcX++, dstX+=scale)
		{
			const __m128i srcVec = _mm_load_si128((__m128i *)src + srcX);
			v128u8 ssse3idx;
			
			for (size_t lx = 0; lx < scale; lx++)
			{
				if (ELEMENTSIZE == 1)
				{
					ssse3idx = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u8_16e + (lx * sizeof(v128u8))));
				}
				else if (ELEMENTSIZE == 2)
				{
					ssse3idx = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u16_8e + (lx * sizeof(v128u8))));
				}
				else if (ELEMENTSIZE == 4)
				{
					ssse3idx = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u32_4e + (lx * sizeof(v128u8))));
				}
				
				_mm_store_si128( (__m128i *)dst + dstX + lx, _mm_shuffle_epi8(srcVec, ssse3idx) );
			}
		}
		
		if (SCALEVERTICAL)
		{
			CopyLinesForVerticalCount<ELEMENTSIZE>(dst, dstWidth, dstLineCount);
		}
	}
#endif
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
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), _mm_store_si128((__m128i *)dst + (X), _mm_load_si128((__m128i *)src + (X))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		__m128i srcPix[2];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = _mm_load_si128((__m128i *)src + (dstX * 2) + 0);
			srcPix[1] = _mm_load_si128((__m128i *)src + (dstX * 2) + 1);
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set1_epi32(0x00FF00FF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set1_epi32(0x00FF00FF));
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_packus_epi16(srcPix[0], srcPix[1]));
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set1_epi32(0x0000FFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set1_epi32(0x0000FFFF));
				
#if defined(ENABLE_SSE4_1)
				_mm_store_si128((__m128i *)dst + dstX, _mm_packus_epi32(srcPix[0], srcPix[1]));
#elif defined(ENABLE_SSSE3)
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8(15,14,11,10, 7, 6, 3, 2,13,12, 9, 8, 5, 4, 1, 0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8(13,12, 9, 8, 5, 4, 1, 0,15,14,11,10, 7, 6, 3, 2));
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_or_si128(srcPix[0], srcPix[1]));
#else
				srcPix[0] = _mm_shufflelo_epi16(srcPix[0], 0xD8);
				srcPix[0] = _mm_shufflehi_epi16(srcPix[0], 0xD8);
				srcPix[0] = _mm_shuffle_epi32(srcPix[0], 0xD8);
				
				srcPix[1] = _mm_shufflelo_epi16(srcPix[1], 0xD8);
				srcPix[1] = _mm_shufflehi_epi16(srcPix[1], 0xD8);
				srcPix[1] = _mm_shuffle_epi32(srcPix[1], 0x8D);
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_or_si128(srcPix[0], srcPix[1]));
#endif
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0, 0xFFFFFFFF, 0, 0xFFFFFFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0, 0xFFFFFFFF, 0, 0xFFFFFFFF));
				
				srcPix[0] = _mm_shuffle_epi32(srcPix[0], 0xD8);
				srcPix[1] = _mm_shuffle_epi32(srcPix[1], 0x8D);
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_or_si128(srcPix[0], srcPix[1]));
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
#ifdef ENABLE_SSSE3
		static const u8 X = 0x80;
#endif
		__m128i srcPix[3];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = _mm_load_si128((__m128i *)src + (dstX * 3) + 0);
			srcPix[1] = _mm_load_si128((__m128i *)src + (dstX * 3) + 1);
			srcPix[2] = _mm_load_si128((__m128i *)src + (dstX * 3) + 2);
			
			if (ELEMENTSIZE == 1)
			{
#ifdef ENABLE_SSSE3
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8( X, X, X, X, X, X, X, X, X, X,15,12, 9, 6, 3, 0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8( X, X, X, X, X,14,11, 8, 5, 2, X, X, X, X, X, X));
				srcPix[2] = _mm_shuffle_epi8(srcPix[2], _mm_set_epi8(13,10, 7, 4, 1, X, X, X, X, X, X, X, X, X, X, X));
				
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[2]);
#else
				__m128i srcWorking[3];
				
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0xFF0000FF, 0x0000FF00, 0x00FF0000, 0xFF0000FF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0x00FF0000, 0xFF0000FF, 0x0000FF00, 0x00FF0000));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x0000FF00, 0x00FF0000, 0xFF0000FF, 0x0000FF00));
				
				srcWorking[0] = _mm_unpacklo_epi8(srcPix[0], _mm_setzero_si128());
				srcWorking[1] = _mm_unpackhi_epi8(srcPix[0], _mm_setzero_si128());
				srcWorking[2] = _mm_unpacklo_epi8(srcPix[1], _mm_setzero_si128());
				srcPix[0] = _mm_or_si128(srcWorking[0], srcWorking[1]);
				srcPix[0] = _mm_or_si128(srcPix[0], srcWorking[2]);
				
				srcWorking[0] = _mm_unpackhi_epi8(srcPix[1], _mm_setzero_si128());
				srcWorking[1] = _mm_unpacklo_epi8(srcPix[2], _mm_setzero_si128());
				srcWorking[2] = _mm_unpackhi_epi8(srcPix[2], _mm_setzero_si128());
				srcPix[1] = _mm_or_si128(srcWorking[0], srcWorking[1]);
				srcPix[1] = _mm_or_si128(srcPix[1], srcWorking[2]);
				
				srcPix[0] = _mm_shufflelo_epi16(srcPix[0], 0x6C);
				srcPix[0] = _mm_shufflehi_epi16(srcPix[0], 0x6C);
				srcPix[1] = _mm_shufflelo_epi16(srcPix[1], 0x6C);
				srcPix[1] = _mm_shufflehi_epi16(srcPix[1], 0x6C);
				
				srcPix[0] = _mm_packus_epi16(srcPix[0], srcPix[1]);
				srcPix[1] = _mm_shuffle_epi32(srcPix[0], 0xB1);
				
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF, 0xFF00FFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0x00FF0000, 0x00FF0000, 0x00FF0000, 0x00FF0000));
				
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
#endif
				_mm_store_si128((__m128i *)dst + dstX, srcPix[0]);
			}
			else if (ELEMENTSIZE == 2)
			{
#ifdef ENABLE_SSSE3
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8( X, X, X, X, X, X, X, X, X, X,13,12, 7, 6, 1, 0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8( X, X, X, X,15,14, 9, 8, 3, 2, X, X, X, X, X, X));
				srcPix[2] = _mm_shuffle_epi8(srcPix[2], _mm_set_epi8(11,10, 5, 4, X, X, X, X, X, X, X, X, X, X, X, X));
#else
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0x0000FFFF, 0x00000000, 0xFFFF0000, 0x0000FFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0xFFFF0000, 0x0000FFFF, 0x00000000, 0xFFFF0000));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x00000000, 0xFFFF0000, 0x0000FFFF, 0x00000000));
				
				srcPix[0] = _mm_shufflelo_epi16(srcPix[0], 0x9C);
				srcPix[1] = _mm_shufflehi_epi16(srcPix[1], 0x9C);
				srcPix[2] = _mm_shuffle_epi32(srcPix[2], 0x9C);
				
				srcPix[0] = _mm_shuffle_epi32(srcPix[0], 0x9C);
				srcPix[1] = _mm_shuffle_epi32(srcPix[1], 0xE1);
				srcPix[2] = _mm_shufflehi_epi16(srcPix[2], 0xC9);
#endif
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[2]);
				
				_mm_store_si128((__m128i *)dst + dstX, srcPix[0]);
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0xFFFFFFFF, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0x00000000, 0xFFFFFFFF, 0x00000000, 0x00000000));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x00000000, 0x00000000, 0xFFFFFFFF, 0x00000000));
				
				srcPix[0] = _mm_shuffle_epi32(srcPix[0], 0x9C);
				srcPix[2] = _mm_shuffle_epi32(srcPix[2], 0x78);
				
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[2]);
				
				_mm_store_si128((__m128i *)dst + dstX, srcPix[0]);
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		__m128i srcPix[4];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = _mm_load_si128((__m128i *)src + (dstX * 4) + 0);
			srcPix[1] = _mm_load_si128((__m128i *)src + (dstX * 4) + 1);
			srcPix[2] = _mm_load_si128((__m128i *)src + (dstX * 4) + 2);
			srcPix[3] = _mm_load_si128((__m128i *)src + (dstX * 4) + 3);
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set1_epi32(0x000000FF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set1_epi32(0x000000FF));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set1_epi32(0x000000FF));
				srcPix[3] = _mm_and_si128(srcPix[3], _mm_set1_epi32(0x000000FF));
				
				srcPix[0] = _mm_packus_epi16(srcPix[0], srcPix[1]);
				srcPix[1] = _mm_packus_epi16(srcPix[2], srcPix[3]);
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_packus_epi16(srcPix[0], srcPix[1]));
			}
			else if (ELEMENTSIZE == 2)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				srcPix[3] = _mm_and_si128(srcPix[3], _mm_set_epi32(0x00000000, 0x0000FFFF, 0x00000000, 0x0000FFFF));
				
#if defined(ENABLE_SSE4_1)
				srcPix[0] = _mm_packus_epi32(srcPix[0], srcPix[1]);
				srcPix[1] = _mm_packus_epi32(srcPix[2], srcPix[3]);
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_packus_epi32(srcPix[0], srcPix[1]));
#elif defined(ENABLE_SSSE3)
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8(15,14,13,12,11,10, 7, 6, 5, 4, 3, 2, 9, 8, 1, 0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8(13,12,13,12,11,10, 7, 6, 9, 8, 1, 0, 5, 4, 3, 2));
				srcPix[2] = _mm_shuffle_epi8(srcPix[2], _mm_set_epi8(13,12,13,12, 9, 8, 1, 0,11,10, 7, 6, 5, 4, 3, 2));
				srcPix[3] = _mm_shuffle_epi8(srcPix[3], _mm_set_epi8( 9, 8, 1, 0,15,14,13,12,11,10, 7, 6, 5, 4, 3, 2));
				
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
				srcPix[1] = _mm_or_si128(srcPix[2], srcPix[3]);
				
				_mm_store_si128((__m128i *)dst + dstX, _mm_or_si128(srcPix[0], srcPix[1]));
#else
				srcPix[0] = _mm_shuffle_epi32(srcPix[0], 0xD8);
				srcPix[1] = _mm_shuffle_epi32(srcPix[1], 0xD8);
				srcPix[2] = _mm_shuffle_epi32(srcPix[2], 0xD8);
				srcPix[3] = _mm_shuffle_epi32(srcPix[3], 0xD8);
				
				srcPix[0] = _mm_unpacklo_epi32(srcPix[0], srcPix[1]);
				srcPix[1] = _mm_unpacklo_epi32(srcPix[2], srcPix[3]);
				
				srcPix[0] = _mm_shuffle_epi32(srcPix[0], 0xD8);
				srcPix[1] = _mm_shuffle_epi32(srcPix[1], 0x8D);
				
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
				srcPix[0] = _mm_shufflelo_epi16(srcPix[0], 0xD8);
				srcPix[0] = _mm_shufflehi_epi16(srcPix[0], 0xD8);
				
				_mm_store_si128((__m128i *)dst + dstX, srcPix[0]);
#endif
			}
			else if (ELEMENTSIZE == 4)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				srcPix[3] = _mm_and_si128(srcPix[3], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF));
				
				srcPix[0] = _mm_unpacklo_epi32(srcPix[0], srcPix[1]);
				srcPix[1] = _mm_unpacklo_epi32(srcPix[2], srcPix[3]);
#ifdef HOST_64
				srcPix[0] = _mm_unpacklo_epi64(srcPix[0], srcPix[1]);
#else
				srcPix[1] = _mm_shuffle_epi32(srcPix[1], 0x4E);
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
#endif
				_mm_store_si128((__m128i *)dst + dstX, srcPix[0]);
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

FORCEINLINE v128u16 ColorOperation_SSE2::blend(const v128u16 &colA, const v128u16 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const
{
	v128u16 ra;
	v128u16 ga;
	v128u16 ba;
	v128u16 colorBitMask = _mm_set1_epi16(0x001F);
	
#ifdef ENABLE_SSSE3
	ra = _mm_or_si128( _mm_and_si128(               colA,      colorBitMask), _mm_and_si128(_mm_slli_epi16(colB, 8), _mm_set1_epi16(0x1F00)) );
	ga = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(colA,  5), colorBitMask), _mm_and_si128(_mm_slli_epi16(colB, 3), _mm_set1_epi16(0x1F00)) );
	ba = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(colA, 10), colorBitMask), _mm_and_si128(_mm_srli_epi16(colB, 2), _mm_set1_epi16(0x1F00)) );
	
	const v128u16 blendAB = _mm_or_si128(blendEVA, _mm_slli_epi16(blendEVB, 8));
	ra = _mm_maddubs_epi16(ra, blendAB);
	ga = _mm_maddubs_epi16(ga, blendAB);
	ba = _mm_maddubs_epi16(ba, blendAB);
#else
	ra = _mm_and_si128(               colA,      colorBitMask);
	ga = _mm_and_si128(_mm_srli_epi16(colA,  5), colorBitMask);
	ba = _mm_and_si128(_mm_srli_epi16(colA, 10), colorBitMask);
	
	v128u16 rb = _mm_and_si128(               colB,      colorBitMask);
	v128u16 gb = _mm_and_si128(_mm_srli_epi16(colB,  5), colorBitMask);
	v128u16 bb = _mm_and_si128(_mm_srli_epi16(colB, 10), colorBitMask);
	
	ra = _mm_add_epi16( _mm_mullo_epi16(ra, blendEVA), _mm_mullo_epi16(rb, blendEVB) );
	ga = _mm_add_epi16( _mm_mullo_epi16(ga, blendEVA), _mm_mullo_epi16(gb, blendEVB) );
	ba = _mm_add_epi16( _mm_mullo_epi16(ba, blendEVA), _mm_mullo_epi16(bb, blendEVB) );
#endif
	
	ra = _mm_srli_epi16(ra, 4);
	ga = _mm_srli_epi16(ga, 4);
	ba = _mm_srli_epi16(ba, 4);
	
	ra = _mm_min_epi16(ra, colorBitMask);
	ga = _mm_min_epi16(ga, colorBitMask);
	ba = _mm_min_epi16(ba, colorBitMask);
	
	return _mm_or_si128(ra, _mm_or_si128( _mm_slli_epi16(ga, 5), _mm_slli_epi16(ba, 10)) );
}

// Note that if USECONSTANTBLENDVALUESHINT is true, then this method will assume that blendEVA contains identical values
// for each 16-bit vector element, and also that blendEVB contains identical values for each 16-bit vector element. If
// this assumption is broken, then the resulting color will be undefined.
template <NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT>
FORCEINLINE v128u32 ColorOperation_SSE2::blend(const v128u32 &colA, const v128u32 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const
{
	v128u16 outColorLo;
	v128u16 outColorHi;
	v128u32 outColor;
	
#ifdef ENABLE_SSSE3
	const v128u16 blendAB = _mm_or_si128(blendEVA, _mm_slli_epi16(blendEVB, 8));
	
	outColorLo = _mm_unpacklo_epi8(colA, colB);
	outColorHi = _mm_unpackhi_epi8(colA, colB);
	
	if (USECONSTANTBLENDVALUESHINT)
	{
		outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
		outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
	}
	else
	{
		const v128u16 blendABLo = _mm_unpacklo_epi16(blendAB, blendAB);
		const v128u16 blendABHi = _mm_unpackhi_epi16(blendAB, blendAB);
		outColorLo = _mm_maddubs_epi16(outColorLo, blendABLo);
		outColorHi = _mm_maddubs_epi16(outColorHi, blendABHi);
	}
#else
	const v128u16 colALo = _mm_unpacklo_epi8(colA, _mm_setzero_si128());
	const v128u16 colAHi = _mm_unpackhi_epi8(colA, _mm_setzero_si128());
	const v128u16 colBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
	const v128u16 colBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
	
	if (USECONSTANTBLENDVALUESHINT)
	{
		outColorLo = _mm_add_epi16( _mm_mullo_epi16(colALo, blendEVA), _mm_mullo_epi16(colBLo, blendEVB) );
		outColorHi = _mm_add_epi16( _mm_mullo_epi16(colAHi, blendEVA), _mm_mullo_epi16(colBHi, blendEVB) );
	}
	else
	{
		const v128u16 blendALo = _mm_unpacklo_epi16(blendEVA, blendEVA);
		const v128u16 blendAHi = _mm_unpackhi_epi16(blendEVA, blendEVA);
		const v128u16 blendBLo = _mm_unpacklo_epi16(blendEVB, blendEVB);
		const v128u16 blendBHi = _mm_unpackhi_epi16(blendEVB, blendEVB);
		
		outColorLo = _mm_add_epi16( _mm_mullo_epi16(colALo, blendALo), _mm_mullo_epi16(colBLo, blendBLo) );
		outColorHi = _mm_add_epi16( _mm_mullo_epi16(colAHi, blendAHi), _mm_mullo_epi16(colBHi, blendBHi) );
	}
#endif
	
	outColorLo = _mm_srli_epi16(outColorLo, 4);
	outColorHi = _mm_srli_epi16(outColorHi, 4);
	outColor = _mm_packus_epi16(outColorLo, outColorHi);
	
	// When the color format is 888, the packuswb instruction will naturally clamp
	// the color component values to 255. However, when the color format is 666, the
	// color component values must be clamped to 63. In this case, we must call pminub
	// to do the clamp.
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		outColor = _mm_min_epu8(outColor, _mm_set1_epi8(63));
	}
	
	outColor = _mm_and_si128(outColor, _mm_set1_epi32(0x00FFFFFF));
	
	return outColor;
}

FORCEINLINE v128u16 ColorOperation_SSE2::blend3D(const v128u32 &colA_Lo, const v128u32 &colA_Hi, const v128u16 &colB) const
{
	// If the color format of B is 555, then the colA_Hi parameter is required.
	// The color format of A is assumed to be RGB666.
	v128u32 ra_lo = _mm_and_si128(                colA_Lo,      _mm_set1_epi32(0x000000FF) );
	v128u32 ga_lo = _mm_and_si128( _mm_srli_epi32(colA_Lo,  8), _mm_set1_epi32(0x000000FF) );
	v128u32 ba_lo = _mm_and_si128( _mm_srli_epi32(colA_Lo, 16), _mm_set1_epi32(0x000000FF) );
	v128u32 aa_lo =                _mm_srli_epi32(colA_Lo, 24);
	
	v128u32 ra_hi = _mm_and_si128(                colA_Hi,      _mm_set1_epi32(0x000000FF) );
	v128u32 ga_hi = _mm_and_si128( _mm_srli_epi32(colA_Hi,  8), _mm_set1_epi32(0x000000FF) );
	v128u32 ba_hi = _mm_and_si128( _mm_srli_epi32(colA_Hi, 16), _mm_set1_epi32(0x000000FF) );
	v128u32 aa_hi =                _mm_srli_epi32(colA_Hi, 24);
	
	v128u16 ra = _mm_packs_epi32(ra_lo, ra_hi);
	v128u16 ga = _mm_packs_epi32(ga_lo, ga_hi);
	v128u16 ba = _mm_packs_epi32(ba_lo, ba_hi);
	v128u16 aa = _mm_packs_epi32(aa_lo, aa_hi);
	
#ifdef ENABLE_SSSE3
	ra = _mm_or_si128( ra, _mm_and_si128(_mm_slli_epi16(colB, 9), _mm_set1_epi16(0x3E00)) );
	ga = _mm_or_si128( ga, _mm_and_si128(_mm_slli_epi16(colB, 4), _mm_set1_epi16(0x3E00)) );
	ba = _mm_or_si128( ba, _mm_and_si128(_mm_srli_epi16(colB, 1), _mm_set1_epi16(0x3E00)) );
	
	aa = _mm_adds_epu8(aa, _mm_set1_epi16(1));
	aa = _mm_or_si128( aa, _mm_slli_epi16(_mm_subs_epu16(_mm_set1_epi8(32), aa), 8) );
	
	ra = _mm_maddubs_epi16(ra, aa);
	ga = _mm_maddubs_epi16(ga, aa);
	ba = _mm_maddubs_epi16(ba, aa);
#else
	aa = _mm_adds_epu16(aa, _mm_set1_epi16(1));
	v128u16 rb = _mm_and_si128( _mm_slli_epi16(colB, 1), _mm_set1_epi16(0x003E) );
	v128u16 gb = _mm_and_si128( _mm_srli_epi16(colB, 4), _mm_set1_epi16(0x003E) );
	v128u16 bb = _mm_and_si128( _mm_srli_epi16(colB, 9), _mm_set1_epi16(0x003E) );
	v128u16 ab = _mm_subs_epu16( _mm_set1_epi16(32), aa );
	
	ra = _mm_add_epi16( _mm_mullo_epi16(ra, aa), _mm_mullo_epi16(rb, ab) );
	ga = _mm_add_epi16( _mm_mullo_epi16(ga, aa), _mm_mullo_epi16(gb, ab) );
	ba = _mm_add_epi16( _mm_mullo_epi16(ba, aa), _mm_mullo_epi16(bb, ab) );
#endif
	
	ra = _mm_srli_epi16(ra, 6);
	ga = _mm_srli_epi16(ga, 6);
	ba = _mm_srli_epi16(ba, 6);
	
	return _mm_or_si128( _mm_or_si128(ra, _mm_slli_epi16(ga, 5)), _mm_slli_epi16(ba, 10) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_SSE2::blend3D(const v128u32 &colA, const v128u32 &colB) const
{
	// If the color format of B is 666 or 888, then the colA_Hi parameter is ignored.
	// The color format of A is assumed to match the color format of B.
	v128u16 rgbALo;
	v128u16 rgbAHi;
	
#ifdef ENABLE_SSSE3
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		// Does not work for RGBA8888 color format. The reason is because this
		// algorithm depends on the pmaddubsw instruction, which multiplies
		// two unsigned 8-bit integers into an intermediate signed 16-bit
		// integer. This means that we can overrun the signed 16-bit value
		// range, which would be limited to [-32767 - 32767]. For example, a
		// color component of value 255 multiplied by an alpha value of 255
		// would equal 65025, which is greater than the upper range of a signed
		// 16-bit value.
		rgbALo = _mm_unpacklo_epi8(colA, colB);
		rgbAHi = _mm_unpackhi_epi8(colA, colB);
		
		v128u32 alpha = _mm_and_si128( _mm_srli_epi32(colA, 24), _mm_set1_epi32(0x0000001F) );
		alpha = _mm_or_si128( alpha, _mm_or_si128(_mm_slli_epi32(alpha, 8), _mm_slli_epi32(alpha, 16)) );
		alpha = _mm_adds_epu8(alpha, _mm_set1_epi8(1));
		
		v128u32 invAlpha = _mm_subs_epu8(_mm_set1_epi8(32), alpha);
		v128u16 alphaLo = _mm_unpacklo_epi8(alpha, invAlpha);
		v128u16 alphaHi = _mm_unpackhi_epi8(alpha, invAlpha);
		
		rgbALo = _mm_maddubs_epi16(rgbALo, alphaLo);
		rgbAHi = _mm_maddubs_epi16(rgbAHi, alphaHi);
	}
	else
#endif
	{
		rgbALo = _mm_unpacklo_epi8(colA, _mm_setzero_si128());
		rgbAHi = _mm_unpackhi_epi8(colA, _mm_setzero_si128());
		v128u16 rgbBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
		v128u16 rgbBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
		
		v128u32 alpha = _mm_and_si128( _mm_srli_epi32(colA, 24), _mm_set1_epi32(0x000000FF) );
		alpha = _mm_or_si128( alpha, _mm_or_si128(_mm_slli_epi32(alpha, 8), _mm_slli_epi32(alpha, 16)) );
		
		v128u16 alphaLo = _mm_unpacklo_epi8(alpha, _mm_setzero_si128());
		v128u16 alphaHi = _mm_unpackhi_epi8(alpha, _mm_setzero_si128());
		alphaLo = _mm_add_epi16(alphaLo, _mm_set1_epi16(1));
		alphaHi = _mm_add_epi16(alphaHi, _mm_set1_epi16(1));
		
		if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
		{
			rgbALo = _mm_add_epi16( _mm_mullo_epi16(rgbALo, alphaLo), _mm_mullo_epi16(rgbBLo, _mm_sub_epi16(_mm_set1_epi16(32), alphaLo)) );
			rgbAHi = _mm_add_epi16( _mm_mullo_epi16(rgbAHi, alphaHi), _mm_mullo_epi16(rgbBHi, _mm_sub_epi16(_mm_set1_epi16(32), alphaHi)) );
		}
		else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
		{
			rgbALo = _mm_add_epi16( _mm_mullo_epi16(rgbALo, alphaLo), _mm_mullo_epi16(rgbBLo, _mm_sub_epi16(_mm_set1_epi16(256), alphaLo)) );
			rgbAHi = _mm_add_epi16( _mm_mullo_epi16(rgbAHi, alphaHi), _mm_mullo_epi16(rgbBHi, _mm_sub_epi16(_mm_set1_epi16(256), alphaHi)) );
		}
	}
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		rgbALo = _mm_srli_epi16(rgbALo, 5);
		rgbAHi = _mm_srli_epi16(rgbAHi, 5);
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		rgbALo = _mm_srli_epi16(rgbALo, 8);
		rgbAHi = _mm_srli_epi16(rgbAHi, 8);
	}
	
	return _mm_and_si128( _mm_packus_epi16(rgbALo, rgbAHi), _mm_set1_epi32(0x00FFFFFF) );
}

FORCEINLINE v128u16 ColorOperation_SSE2::increase(const v128u16 &col, const v128u16 &blendEVY) const
{
	v128u16 r = _mm_and_si128(                col,      _mm_set1_epi16(0x001F) );
	v128u16 g = _mm_and_si128( _mm_srli_epi16(col,  5), _mm_set1_epi16(0x001F) );
	v128u16 b = _mm_and_si128( _mm_srli_epi16(col, 10), _mm_set1_epi16(0x001F) );
	
	r = _mm_add_epi16( r, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), r), blendEVY), 4) );
	g = _mm_add_epi16( g, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), g), blendEVY), 4) );
	b = _mm_add_epi16( b, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), b), blendEVY), 4) );
	
	return _mm_or_si128(r, _mm_or_si128( _mm_slli_epi16(g, 5), _mm_slli_epi16(b, 10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_SSE2::increase(const v128u32 &col, const v128u16 &blendEVY) const
{
	v128u16 rgbLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
	v128u16 rgbHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
	
	rgbLo = _mm_add_epi16( rgbLo, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbLo), blendEVY), 4) );
	rgbHi = _mm_add_epi16( rgbHi, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbHi), blendEVY), 4) );
	
	return _mm_and_si128( _mm_packus_epi16(rgbLo, rgbHi), _mm_set1_epi32(0x00FFFFFF) );
}

FORCEINLINE v128u16 ColorOperation_SSE2::decrease(const v128u16 &col, const v128u16 &blendEVY) const
{
	v128u16 r = _mm_and_si128(                col,      _mm_set1_epi16(0x001F) );
	v128u16 g = _mm_and_si128( _mm_srli_epi16(col,  5), _mm_set1_epi16(0x001F) );
	v128u16 b = _mm_and_si128( _mm_srli_epi16(col, 10), _mm_set1_epi16(0x001F) );
	
	r = _mm_sub_epi16( r, _mm_srli_epi16(_mm_mullo_epi16(r, blendEVY), 4) );
	g = _mm_sub_epi16( g, _mm_srli_epi16(_mm_mullo_epi16(g, blendEVY), 4) );
	b = _mm_sub_epi16( b, _mm_srli_epi16(_mm_mullo_epi16(b, blendEVY), 4) );
	
	return _mm_or_si128(r, _mm_or_si128( _mm_slli_epi16(g, 5), _mm_slli_epi16(b, 10)) );
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE v128u32 ColorOperation_SSE2::decrease(const v128u32 &col, const v128u16 &blendEVY) const
{
	v128u16 rgbLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
	v128u16 rgbHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
	
	rgbLo = _mm_sub_epi16( rgbLo, _mm_srli_epi16(_mm_mullo_epi16(rgbLo, blendEVY), 4) );
	rgbHi = _mm_sub_epi16( rgbHi, _mm_srli_epi16(_mm_mullo_epi16(rgbHi, blendEVY), 4) );
	
	return _mm_and_si128( _mm_packus_epi16(rgbLo, rgbHi), _mm_set1_epi32(0x00FFFFFF) );
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_SSE2::_copy16(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_or_si128(src0, alphaBits) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_or_si128(src1, alphaBits) );
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555To6665Opaque_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To6665Opaque_SSE2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555To8888Opaque_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To8888Opaque_SSE2<false>(src1, src32[2], src32[3]);
		}
		
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, src32[0] );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, src32[1] );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, src32[2] );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, src32[3] );
	}
	
	if (!ISDEBUGRENDER)
	{
		_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, srcLayerID );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_SSE2::_copy32(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_SSE2<false>(src0, src1),
			ColorspaceConvert6665To5551_SSE2<false>(src2, src3)
			//_mm_set1_epi16(0x801F),
			//_mm_set1_epi16(0x801F)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_or_si128(src16[0], alphaBits) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_or_si128(src16[1], alphaBits) );
	}
	else
	{
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_or_si128(src0, alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_or_si128(src1, alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_or_si128(src2, alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_or_si128(src3, alphaBits) );
	}
	
	if (!ISDEBUGRENDER)
	{
		_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, srcLayerID );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_SSE2::_copyMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	const v128u16 passMask16[2]	= {
		_mm_unpacklo_epi8(passMask8, passMask8),
		_mm_unpackhi_epi8(passMask8, passMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(src0, alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(src1, alphaBits), passMask16[1]) );
	}
	else
	{
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555To6665Opaque_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To6665Opaque_SSE2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555To8888Opaque_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555To8888Opaque_SSE2<false>(src1, src32[2], src32[3]);
		}
		
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		const v128u32 passMask32[4]	= {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], src32[0], passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], src32[1], passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], src32[2], passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], src32[3], passMask32[3]) );
	}
	
	if (!ISDEBUGRENDER)
	{
		const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
		_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void PixelOperation_SSE2::_copyMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	const v128u16 passMask16[2]	= {
		_mm_unpacklo_epi8(passMask8, passMask8),
		_mm_unpackhi_epi8(passMask8, passMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_SSE2<false>(src0, src1),
			ColorspaceConvert6665To5551_SSE2<false>(src2, src3)
		};
		
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(src16[0], alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(src16[1], alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u32 passMask32[4]	= {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v128u32 dst[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst[0], _mm_or_si128(src0, alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst[1], _mm_or_si128(src1, alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst[2], _mm_or_si128(src2, alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst[3], _mm_or_si128(src3, alphaBits), passMask32[3]) );
	}
	
	if (!ISDEBUGRENDER)
	{
		const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
		_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	}
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessUp16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_or_si128(colorop_vec.increase(src0, evy16), alphaBits) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_or_si128(colorop_vec.increase(src1, evy16), alphaBits) );
	}
	else
	{
		v128u32 dst[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_SSE2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo666X_SSE2<false>(src1, dst[2], dst[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_SSE2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo888X_SSE2<false>(src1, dst[2], dst[3]);
		}
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm_set1_epi32(0x1F000000) : _mm_set1_epi32(0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(dst[0], evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(dst[1], evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(dst[2], evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(dst[3], evy16), alphaBits) );
	}
	
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessUp32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_SSE2<false>(src0, src1),
			ColorspaceConvert6665To5551_SSE2<false>(src2, src3)
		};
		
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_or_si128(colorop_vec.increase(src16[0], evy16), alphaBits) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_or_si128(colorop_vec.increase(src16[1], evy16), alphaBits) );
	}
	else
	{
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits) );
	}
	
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessUpMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	const v128u16 passMask16[2]	= {
		_mm_unpacklo_epi8(passMask8, passMask8),
		_mm_unpackhi_epi8(passMask8, passMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(colorop_vec.increase(src0, evy16), alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(colorop_vec.increase(src1, evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u32 passMask32[4]	= {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo666X_SSE2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo888X_SSE2<false>(src1, src32[2], src32[3]);
		}
		
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src32[0], evy16), alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src32[1], evy16), alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src32[2], evy16), alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src32[3], evy16), alphaBits), passMask32[3]) );
	}
	
	const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessUpMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	const v128u16 passMask16[2]	= {
		_mm_unpacklo_epi8(passMask8, passMask8),
		_mm_unpackhi_epi8(passMask8, passMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_SSE2<false>(src0, src1),
			ColorspaceConvert6665To5551_SSE2<false>(src2, src3)
		};
		
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(colorop_vec.increase(src16[0], evy16), alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(colorop_vec.increase(src16[1], evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u32 passMask32[4]	= {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src0, evy16), alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src1, evy16), alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src2, evy16), alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], _mm_or_si128(colorop_vec.increase<OUTPUTFORMAT>(src3, evy16), alphaBits), passMask32[3]) );
	}
	
	const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessDown16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_or_si128(colorop_vec.decrease(src0, evy16), alphaBits) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_or_si128(colorop_vec.decrease(src1, evy16), alphaBits) );
	}
	else
	{
		v128u32 dst[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_SSE2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo666X_SSE2<false>(src1, dst[2], dst[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_SSE2<false>(src0, dst[0], dst[1]);
			ColorspaceConvert555XTo888X_SSE2<false>(src1, dst[2], dst[3]);
		}
		
		const v128u32 alphaBits = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm_set1_epi32(0x1F000000) : _mm_set1_epi32(0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(dst[0], evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(dst[1], evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(dst[2], evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(dst[3], evy16), alphaBits) );
	}
	
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessDown32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_SSE2<false>(src0, src1),
			ColorspaceConvert6665To5551_SSE2<false>(src2, src3)
		};
		
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_or_si128(colorop_vec.decrease(src16[0], evy16), alphaBits) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_or_si128(colorop_vec.decrease(src16[1], evy16), alphaBits) );
	}
	else
	{
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits) );
	}
	
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, srcLayerID );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessDownMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const
{
	const v128u16 passMask16[2]	= {
		_mm_unpacklo_epi8(passMask8, passMask8),
		_mm_unpackhi_epi8(passMask8, passMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(colorop_vec.decrease(src0, evy16), alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(colorop_vec.decrease(src1, evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u32 passMask32[4]	= {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		v128u32 src32[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555XTo666X_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo666X_SSE2<false>(src1, src32[2], src32[3]);
		}
		else
		{
			ColorspaceConvert555XTo888X_SSE2<false>(src0, src32[0], src32[1]);
			ColorspaceConvert555XTo888X_SSE2<false>(src1, src32[2], src32[3]);
		}
		
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src32[0], evy16), alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src32[1], evy16), alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src32[2], evy16), alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src32[3], evy16), alphaBits), passMask32[3]) );
	}
	
	const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void PixelOperation_SSE2::_brightnessDownMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const
{
	const v128u16 passMask16[2]	= {
		_mm_unpacklo_epi8(passMask8, passMask8),
		_mm_unpackhi_epi8(passMask8, passMask8)
	};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 src16[2] = {
			ColorspaceConvert6665To5551_SSE2<false>(src0, src1),
			ColorspaceConvert6665To5551_SSE2<false>(src2, src3)
		};
		
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(colorop_vec.decrease(src16[0], evy16), alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(colorop_vec.decrease(src16[1], evy16), alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u32 passMask32[4]	= {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src0, evy16), alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src1, evy16), alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src2, evy16), alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], _mm_or_si128(colorop_vec.decrease<OUTPUTFORMAT>(src3, evy16), alphaBits), passMask32[3]) );
	}
	
	const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_SSE2::_unknownEffectMask16(GPUEngineCompositorInfo &compInfo,
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
	const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	
	v128u8 dstTargetBlendEnableMask;
	
#ifdef ENABLE_SSSE3
	dstTargetBlendEnableMask = _mm_shuffle_epi8(dstBlendEnableMaskLUT, dstLayerID);
#else
	dstTargetBlendEnableMask =                                         _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG0]));
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG1)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG1])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG2)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG2])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG3)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG3])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_OBJ)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_OBJ])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_Backdrop)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_Backdrop])) );
#endif
	
	dstTargetBlendEnableMask = _mm_andnot_si128( _mm_cmpeq_epi8(dstLayerID, srcLayerID), dstTargetBlendEnableMask );
	v128u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : _mm_setzero_si128();
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	__m128i eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? _mm_set1_epi8(compInfo.renderState.blendEVA) : _mm_set1_epi16(compInfo.renderState.blendEVA);
	__m128i evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? _mm_set1_epi8(compInfo.renderState.blendEVB) : _mm_set1_epi16(compInfo.renderState.blendEVB);
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v128u8 isObjTranslucentMask = _mm_and_si128( dstTargetBlendEnableMask, _mm_or_si128(_mm_cmpeq_epi8(spriteMode, _mm_set1_epi8(OBJMode_Transparent)), _mm_cmpeq_epi8(spriteMode, _mm_set1_epi8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v128u8 spriteAlphaMask = _mm_andnot_si128(_mm_cmpeq_epi8(spriteAlpha, _mm_set1_epi8(0xFF)), isObjTranslucentMask);
		eva_vec128 = _mm_blendv_epi8(eva_vec128, spriteAlpha, spriteAlphaMask);
		evb_vec128 = _mm_blendv_epi8(evb_vec128, _mm_sub_epi8(_mm_set1_epi8(16), spriteAlpha), spriteAlphaMask);
	}
	
	// Select the color effect based on the BLDCNT target flags.
	const v128u8 colorEffect_vec128 = _mm_blendv_epi8(_mm_set1_epi8(ColorEffect_Disable), _mm_set1_epi8(compInfo.renderState.colorEffect), enableColorEffectMask);
	
	// ----------
	
	__m128i tmpSrc[4];
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc[0] = src0;
		tmpSrc[1] = src1;
		tmpSrc[2] = _mm_setzero_si128();
		tmpSrc[3] = _mm_setzero_si128();
	}
	else if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
	{
		ColorspaceConvert555XTo666X_SSE2<false>(src0, tmpSrc[0], tmpSrc[1]);
		ColorspaceConvert555XTo666X_SSE2<false>(src1, tmpSrc[2], tmpSrc[3]);
	}
	else
	{
		ColorspaceConvert555XTo888X_SSE2<false>(src0, tmpSrc[0], tmpSrc[1]);
		ColorspaceConvert555XTo888X_SSE2<false>(src1, tmpSrc[2], tmpSrc[3]);
	}
	
	switch (compInfo.renderState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const v128u8 brightnessMask8 = _mm_andnot_si128( forceDstTargetBlendMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const int brightnessUpMaskValue = _mm_movemask_epi8(brightnessMask8);
			
			if (brightnessUpMaskValue != 0x00000000)
			{
				const v128u16 brightnessMask16[2] = {
					_mm_unpacklo_epi8(brightnessMask8, brightnessMask8),
					_mm_unpackhi_epi8(brightnessMask8, brightnessMask8)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.increase(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.increase(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						_mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
						_mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1])
					};
					
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v128u8 brightnessMask8 = _mm_andnot_si128( forceDstTargetBlendMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const int brightnessDownMaskValue = _mm_movemask_epi8(brightnessMask8);
			
			if (brightnessDownMaskValue != 0x00000000)
			{
				const v128u16 brightnessMask16[2] = {
					_mm_unpacklo_epi8(brightnessMask8, brightnessMask8),
					_mm_unpackhi_epi8(brightnessMask8, brightnessMask8)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.decrease(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.decrease(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						_mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
						_mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1])
					};
					
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v128u8 blendMask8 = _mm_or_si128( forceDstTargetBlendMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstTargetBlendEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const int blendMaskValue = _mm_movemask_epi8(blendMask8);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		if (blendMaskValue != 0x00000000)
		{
			const v128u16 blendMask16[2] = {
				_mm_unpacklo_epi8(blendMask8, blendMask8),
				_mm_unpackhi_epi8(blendMask8, blendMask8)
			};
			
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
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], eva_vec128, evb_vec128);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const v128u16 tempEVA[2] = {
						_mm_unpacklo_epi8(eva_vec128, _mm_setzero_si128()),
						_mm_unpackhi_epi8(eva_vec128, _mm_setzero_si128())
					};
					const v128u16 tempEVB[2] = {
						_mm_unpacklo_epi8(evb_vec128, _mm_setzero_si128()),
						_mm_unpackhi_epi8(evb_vec128, _mm_setzero_si128())
					};
					
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], tempEVA[0], tempEVB[0]);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], tempEVA[1], tempEVB[1]);
					break;
				}
			}
			
			tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
			tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		}
		
		// Store the final colors.
		const v128u16 passMask16[2] = {
			_mm_unpacklo_epi8(passMask8, passMask8),
			_mm_unpackhi_epi8(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(tmpSrc[0], alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(tmpSrc[1], alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u16 blendMask16[2] = {
			_mm_unpacklo_epi8(blendMask8, blendMask8),
			_mm_unpackhi_epi8(blendMask8, blendMask8)
		};
		
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
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
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[0], dst32[0], eva_vec128, evb_vec128);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[1], dst32[1], eva_vec128, evb_vec128);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[2], dst32[2], eva_vec128, evb_vec128);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[3], dst32[3], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					v128u16 tempBlendLo = _mm_unpacklo_epi8(eva_vec128, eva_vec128);
					v128u16 tempBlendHi = _mm_unpackhi_epi8(eva_vec128, eva_vec128);
					
					const v128u16 tempEVA[4] = {
						_mm_unpacklo_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpacklo_epi8(tempBlendHi, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendHi, _mm_setzero_si128())
					};
					
					tempBlendLo = _mm_unpacklo_epi8(evb_vec128, evb_vec128);
					tempBlendHi = _mm_unpackhi_epi8(evb_vec128, evb_vec128);
					
					const v128u16 tempEVB[4] = {
						_mm_unpacklo_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpacklo_epi8(tempBlendHi, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendHi, _mm_setzero_si128())
					};
					
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[0], dst32[0], tempEVA[0], tempEVB[0]);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[1], dst32[1], tempEVA[1], tempEVB[1]);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[2], dst32[2], tempEVA[2], tempEVB[2]);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[3], dst32[3], tempEVA[3], tempEVB[3]);
					break;
				}
			}
			
			const v128u32 blendMask32[4] = {
				_mm_unpacklo_epi16(blendMask16[0], blendMask16[0]),
				_mm_unpackhi_epi16(blendMask16[0], blendMask16[0]),
				_mm_unpacklo_epi16(blendMask16[1], blendMask16[1]),
				_mm_unpackhi_epi16(blendMask16[1], blendMask16[1])
			};
			
			tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
			tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
			tmpSrc[2] = _mm_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
			tmpSrc[3] = _mm_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		}
		
		// Store the final colors.
		const v128u16 passMask16[2] = {
			_mm_unpacklo_epi8(passMask8, passMask8),
			_mm_unpackhi_epi8(passMask8, passMask8)
		};
		
		const v128u32 passMask32[4] = {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], _mm_or_si128(tmpSrc[0], alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], _mm_or_si128(tmpSrc[1], alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], _mm_or_si128(tmpSrc[2], alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], _mm_or_si128(tmpSrc[3], alphaBits), passMask32[3]) );
	}
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void PixelOperation_SSE2::_unknownEffectMask32(GPUEngineCompositorInfo &compInfo,
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
	const v128u8 dstLayerID = _mm_load_si128((v128u8 *)compInfo.target.lineLayerID);
	_mm_store_si128( (v128u8 *)compInfo.target.lineLayerID, _mm_blendv_epi8(dstLayerID, srcLayerID, passMask8) );
	
	v128u8 dstTargetBlendEnableMask;
	
#ifdef ENABLE_SSSE3
	dstTargetBlendEnableMask = _mm_shuffle_epi8(dstBlendEnableMaskLUT, dstLayerID);
#else
	dstTargetBlendEnableMask =                                         _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG0]));
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG1)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG1])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG2)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG2])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG3)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_BG3])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_OBJ)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_OBJ])) );
	dstTargetBlendEnableMask = _mm_or_si128( dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_Backdrop)), _mm_set1_epi8(compInfo.renderState.dstBlendEnable[GPULayerID_Backdrop])) );
#endif
	
	dstTargetBlendEnableMask = _mm_andnot_si128( _mm_cmpeq_epi8(dstLayerID, srcLayerID), dstTargetBlendEnableMask );
	v128u8 forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : _mm_setzero_si128();
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	__m128i eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? _mm_set1_epi8(compInfo.renderState.blendEVA) : _mm_set1_epi16(compInfo.renderState.blendEVA);
	__m128i evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? _mm_set1_epi8(compInfo.renderState.blendEVB) : _mm_set1_epi16(compInfo.renderState.blendEVB);
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const v128u8 isObjTranslucentMask = _mm_and_si128( dstTargetBlendEnableMask, _mm_or_si128(_mm_cmpeq_epi8(spriteMode, _mm_set1_epi8(OBJMode_Transparent)), _mm_cmpeq_epi8(spriteMode, _mm_set1_epi8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const v128u8 spriteAlphaMask = _mm_andnot_si128(_mm_cmpeq_epi8(spriteAlpha, _mm_set1_epi8(0xFF)), isObjTranslucentMask);
		eva_vec128 = _mm_blendv_epi8(eva_vec128, spriteAlpha, spriteAlphaMask);
		evb_vec128 = _mm_blendv_epi8(evb_vec128, _mm_sub_epi8(_mm_set1_epi8(16), spriteAlpha), spriteAlphaMask);
	}
	
	// Select the color effect based on the BLDCNT target flags.
	const v128u8 colorEffect_vec128 = _mm_blendv_epi8(_mm_set1_epi8(ColorEffect_Disable), _mm_set1_epi8(compInfo.renderState.colorEffect), enableColorEffectMask);
	
	// ----------
	
	__m128i tmpSrc[4];
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		tmpSrc[0] = ColorspaceConvert6665To5551_SSE2<false>(src0, src1);
		tmpSrc[1] = ColorspaceConvert6665To5551_SSE2<false>(src2, src3);
		tmpSrc[2] = _mm_setzero_si128();
		tmpSrc[3] = _mm_setzero_si128();
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
			const v128u8 brightnessMask8 = _mm_andnot_si128( forceDstTargetBlendMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const int brightnessUpMaskValue = _mm_movemask_epi8(brightnessMask8);
			
			if (brightnessUpMaskValue != 0x00000000)
			{
				const v128u16 brightnessMask16[2] = {
					_mm_unpacklo_epi8(brightnessMask8, brightnessMask8),
					_mm_unpackhi_epi8(brightnessMask8, brightnessMask8)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.increase(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.increase(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						_mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
						_mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1])
					};
					
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], colorop_vec.increase<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const v128u8 brightnessMask8 = _mm_andnot_si128( forceDstTargetBlendMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const int brightnessDownMaskValue = _mm_movemask_epi8(brightnessMask8);
			
			if (brightnessDownMaskValue != 0x00000000)
			{
				const v128u16 brightnessMask16[2] = {
					_mm_unpacklo_epi8(brightnessMask8, brightnessMask8),
					_mm_unpackhi_epi8(brightnessMask8, brightnessMask8)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.decrease(tmpSrc[0], evy16), brightnessMask16[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.decrease(tmpSrc[1], evy16), brightnessMask16[1] );
				}
				else
				{
					const v128u32 brightnessMask32[4] = {
						_mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
						_mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
						_mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1])
					};
					
					tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[0], evy16), brightnessMask32[0] );
					tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[1], evy16), brightnessMask32[1] );
					tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[2], evy16), brightnessMask32[2] );
					tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], colorop_vec.decrease<OUTPUTFORMAT>(tmpSrc[3], evy16), brightnessMask32[3] );
				}
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const v128u8 blendMask8 = _mm_or_si128( forceDstTargetBlendMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstTargetBlendEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const int blendMaskValue = _mm_movemask_epi8(blendMask8);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const v128u16 dst16[2] = {
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 0),
			_mm_load_si128((v128u16 *)compInfo.target.lineColor16 + 1)
		};
		
		if (blendMaskValue != 0x00000000)
		{
			const v128u16 blendMask16[2] = {
				_mm_unpacklo_epi8(blendMask8, blendMask8),
				_mm_unpackhi_epi8(blendMask8, blendMask8)
			};
			
			v128u16 blendSrc16[2];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc16[0] = colorop_vec.blend3D(src0, src1, dst16[0]);
					blendSrc16[1] = colorop_vec.blend3D(src2, src3, dst16[1]);
					break;
					
				case GPULayerType_BG:
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], eva_vec128, evb_vec128);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					const v128u16 tempEVA[2] = {
						_mm_unpacklo_epi8(eva_vec128, _mm_setzero_si128()),
						_mm_unpackhi_epi8(eva_vec128, _mm_setzero_si128())
					};
					const v128u16 tempEVB[2] = {
						_mm_unpacklo_epi8(evb_vec128, _mm_setzero_si128()),
						_mm_unpackhi_epi8(evb_vec128, _mm_setzero_si128())
					};
					
					blendSrc16[0] = colorop_vec.blend(tmpSrc[0], dst16[0], tempEVA[0], tempEVB[0]);
					blendSrc16[1] = colorop_vec.blend(tmpSrc[1], dst16[1], tempEVA[1], tempEVB[1]);
					break;
				}
			}
			
			tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
			tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		}
		
		// Store the final colors.
		const v128u16 passMask16[2] = {
			_mm_unpacklo_epi8(passMask8, passMask8),
			_mm_unpackhi_epi8(passMask8, passMask8)
		};
		
		const v128u16 alphaBits = _mm_set1_epi16(0x8000);
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 0, _mm_blendv_epi8(dst16[0], _mm_or_si128(tmpSrc[0], alphaBits), passMask16[0]) );
		_mm_store_si128( (v128u16 *)compInfo.target.lineColor16 + 1, _mm_blendv_epi8(dst16[1], _mm_or_si128(tmpSrc[1], alphaBits), passMask16[1]) );
	}
	else
	{
		const v128u32 dst32[4] = {
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 0),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 1),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 2),
			_mm_load_si128((v128u32 *)compInfo.target.lineColor32 + 3),
		};
		
		if (blendMaskValue != 0x00000000)
		{
			const v128u16 blendMask16[2] = {
				_mm_unpacklo_epi8(blendMask8, blendMask8),
				_mm_unpackhi_epi8(blendMask8, blendMask8)
			};
			
			v128u32 blendSrc32[4];
			
			switch (LAYERTYPE)
			{
				case GPULayerType_3D:
					blendSrc32[0] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[0], dst32[0]);
					blendSrc32[1] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[1], dst32[1]);
					blendSrc32[2] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[2], dst32[2]);
					blendSrc32[3] = colorop_vec.blend3D<OUTPUTFORMAT>(tmpSrc[3], dst32[3]);
					break;
					
				case GPULayerType_BG:
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[0], dst32[0], eva_vec128, evb_vec128);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[1], dst32[1], eva_vec128, evb_vec128);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[2], dst32[2], eva_vec128, evb_vec128);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, true>(tmpSrc[3], dst32[3], eva_vec128, evb_vec128);
					break;
					
				case GPULayerType_OBJ:
				{
					// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
					//
					// Note that we are sending only 4 colors for each colorop_vec.blend() call, and so we are only
					// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
					// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
					v128u16 tempBlendLo = _mm_unpacklo_epi8(eva_vec128, eva_vec128);
					v128u16 tempBlendHi = _mm_unpackhi_epi8(eva_vec128, eva_vec128);
					
					const v128u16 tempEVA[4] = {
						_mm_unpacklo_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpacklo_epi8(tempBlendHi, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendHi, _mm_setzero_si128())
					};
					
					tempBlendLo = _mm_unpacklo_epi8(evb_vec128, evb_vec128);
					tempBlendHi = _mm_unpackhi_epi8(evb_vec128, evb_vec128);
					
					const v128u16 tempEVB[4] = {
						_mm_unpacklo_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendLo, _mm_setzero_si128()),
						_mm_unpacklo_epi8(tempBlendHi, _mm_setzero_si128()),
						_mm_unpackhi_epi8(tempBlendHi, _mm_setzero_si128())
					};
					
					blendSrc32[0] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[0], dst32[0], tempEVA[0], tempEVB[0]);
					blendSrc32[1] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[1], dst32[1], tempEVA[1], tempEVB[1]);
					blendSrc32[2] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[2], dst32[2], tempEVA[2], tempEVB[2]);
					blendSrc32[3] = colorop_vec.blend<OUTPUTFORMAT, false>(tmpSrc[3], dst32[3], tempEVA[3], tempEVB[3]);
					break;
				}
			}
			
			const v128u32 blendMask32[4] = {
				_mm_unpacklo_epi16(blendMask16[0], blendMask16[0]),
				_mm_unpackhi_epi16(blendMask16[0], blendMask16[0]),
				_mm_unpacklo_epi16(blendMask16[1], blendMask16[1]),
				_mm_unpackhi_epi16(blendMask16[1], blendMask16[1])
			};
			
			tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
			tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
			tmpSrc[2] = _mm_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
			tmpSrc[3] = _mm_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		}
		
		// Store the final colors.
		const v128u16 passMask16[2] = {
			_mm_unpacklo_epi8(passMask8, passMask8),
			_mm_unpackhi_epi8(passMask8, passMask8)
		};
		
		const v128u32 passMask32[4] = {
			_mm_unpacklo_epi16(passMask16[0], passMask16[0]),
			_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
			_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
			_mm_unpackhi_epi16(passMask16[1], passMask16[1])
		};
		
		const v128u32 alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 0, _mm_blendv_epi8(dst32[0], _mm_or_si128(tmpSrc[0], alphaBits), passMask32[0]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 1, _mm_blendv_epi8(dst32[1], _mm_or_si128(tmpSrc[1], alphaBits), passMask32[1]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 2, _mm_blendv_epi8(dst32[2], _mm_or_si128(tmpSrc[2], alphaBits), passMask32[2]) );
		_mm_store_si128( (v128u32 *)compInfo.target.lineColor32 + 3, _mm_blendv_epi8(dst32[3], _mm_or_si128(tmpSrc[3], alphaBits), passMask32[3]) );
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void PixelOperation_SSE2::Composite16(GPUEngineCompositorInfo &compInfo,
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
				const v128u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? _mm_load_si128((v128u8 *)enableColorEffectPtr) : _mm_set1_epi8(0xFF);
				const v128u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? _mm_load_si128((v128u8 *)sprAlphaPtr) : _mm_setzero_si128();
				const v128u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? _mm_load_si128((v128u8 *)sprModePtr) : _mm_setzero_si128();
				
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
FORCEINLINE void PixelOperation_SSE2::Composite32(GPUEngineCompositorInfo &compInfo,
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
				const v128u8 enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? _mm_load_si128((v128u8 *)enableColorEffectPtr): _mm_set1_epi8(0xFF);
				const v128u8 spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? _mm_load_si128((v128u8 *)sprAlphaPtr) : _mm_setzero_si128();
				const v128u8 spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? _mm_load_si128((v128u8 *)sprModePtr) : _mm_setzero_si128();
				
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
			_mm_load_si128((v128u16 *)(this->_deferredColorNative + x) + 0),
			_mm_load_si128((v128u16 *)(this->_deferredColorNative + x) + 1)
		};
		
		if (ISFIRSTLINE)
		{
			const v128u8 indexVec = _mm_load_si128((v128u8 *)(this->_deferredIndexNative + x));
			const v128u8 idxMask8 = _mm_cmpeq_epi8(indexVec, _mm_setzero_si128());
			const v128u16 idxMask16[2] = {
				_mm_unpacklo_epi8(idxMask8, idxMask8),
				_mm_unpackhi_epi8(idxMask8, idxMask8)
			};
			
			const v128u16 mosaicColor16[2] = {
				_mm_blendv_epi8(_mm_and_si128(dstColor16[0], _mm_set1_epi16(0x7FFF)), _mm_set1_epi16(0xFFFF), idxMask16[0]),
				_mm_blendv_epi8(_mm_and_si128(dstColor16[1], _mm_set1_epi16(0x7FFF)), _mm_set1_epi16(0xFFFF), idxMask16[1])
			};
			
			const v128u16 mosaicSetColorMask8 = _mm_cmpeq_epi16( _mm_loadu_si128((v128u8 *)(compInfo.renderState.mosaicWidthBG->begin + x)), _mm_setzero_si128() );
			const v128u16 mosaicSetColorMask16[2] = {
				_mm_unpacklo_epi8(mosaicSetColorMask8, mosaicSetColorMask8),
				_mm_unpackhi_epi8(mosaicSetColorMask8, mosaicSetColorMask8)
			};
			
			_mm_storeu_si128( (v128u16 *)(mosaicColorBG + x) + 0, _mm_blendv_epi8(mosaicColor16[0], _mm_loadu_si128((v128u16 *)(mosaicColorBG + x) + 0), mosaicSetColorMask16[0]) );
			_mm_storeu_si128( (v128u16 *)(mosaicColorBG + x) + 1, _mm_blendv_epi8(mosaicColor16[1], _mm_loadu_si128((v128u16 *)(mosaicColorBG + x) + 1), mosaicSetColorMask16[1]) );
		}
		
		const v128u16 outColor16[2] = {
			_mm_setr_epi16(mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+0]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+1]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+2]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+3]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+4]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+5]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+6]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+7]]),
		
			_mm_setr_epi16(mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+8]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+9]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+10]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+11]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+12]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+13]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+14]],
			               mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc32[x+15]])
		};
		
		const v128u16 writeColorMask16[2] = {
			_mm_cmpeq_epi16(outColor16[0], _mm_set1_epi16(0xFFFF)),
			_mm_cmpeq_epi16(outColor16[1], _mm_set1_epi16(0xFFFF))
		};
		
		_mm_store_si128( (v128u16 *)(this->_deferredColorNative + x) + 0, _mm_blendv_epi8(outColor16[0], dstColor16[0], writeColorMask16[0]) );
		_mm_store_si128( (v128u16 *)(this->_deferredColorNative + x) + 1, _mm_blendv_epi8(outColor16[1], dstColor16[1], writeColorMask16[1]) );
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeNativeLineOBJ_LoopOp(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const FragmentColor *__restrict srcColorNative32)
{
	static const size_t step = sizeof(v128u8);
	
	const bool isUsingSrc32 = (srcColorNative32 != NULL);
	const v128u16 evy16 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = _mm_set1_epi8(compInfo.renderState.srcEffectEnable[GPULayerID_OBJ]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm_load_si128((v128u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm_setzero_si128();
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=step, srcColorNative16+=step, srcColorNative32+=step, compInfo.target.xNative+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
	{
		v128u8 passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm_load_si128((v128u8 *)(this->_didPassWindowTestNative[GPULayerID_OBJ] + i));
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
			
			didAllPixelsPass = (passMaskValue == 0xFFFF);
		}
		else
		{
			passMask8 = _mm_set1_epi8(0xFF);
			passMaskValue = 0xFFFF;
			didAllPixelsPass = true;
		}
		
		if (isUsingSrc32)
		{
			const v128u32 src[4] = {
				_mm_load_si128((v128u32 *)srcColorNative32 + 0),
				_mm_load_si128((v128u32 *)srcColorNative32 + 1),
				_mm_load_si128((v128u32 *)srcColorNative32 + 2),
				_mm_load_si128((v128u32 *)srcColorNative32 + 3)
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
				_mm_load_si128((v128u16 *)srcColorNative16 + 0),
				_mm_load_si128((v128u16 *)srcColorNative16 + 1)
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
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v128u16 evy16 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = _mm_set1_epi8(compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm_load_si128((v128u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm_setzero_si128();
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
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
				passMask8 = _mm_load_si128((v128u8 *)(windowTestPtr + compInfo.target.xCustom));
			}
			
			if (LAYERTYPE == GPULayerType_BG)
			{
				// Do the index test. Pixels with an index value of 0 are rejected.
				const v128u8 idxPassMask8 = _mm_cmpeq_epi8(_mm_load_si128((v128u8 *)(srcIndexCustom + compInfo.target.xCustom)), _mm_setzero_si128());
				
				if (WILLPERFORMWINDOWTEST)
				{
					passMask8 = _mm_andnot_si128(idxPassMask8, passMask8);
				}
				else
				{
					passMask8 = _mm_xor_si128(idxPassMask8, _mm_set1_epi32(0xFFFFFFFF));
				}
			}
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
			
			didAllPixelsPass = (passMaskValue == 0xFFFF);
		}
		else
		{
			passMask8 = _mm_set1_epi8(0xFF);
			passMaskValue = 0xFFFF;
			didAllPixelsPass = true;
		}
		
		const v128u16 src[2] = {
			_mm_load_si128((v128u16 *)(srcColorCustom16 + compInfo.target.xCustom) + 0),
			_mm_load_si128((v128u16 *)(srcColorCustom16 + compInfo.target.xCustom) + 1)
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
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v128u16 evy16 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = _mm_set1_epi8(compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm_load_si128((v128u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm_setzero_si128();
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
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
			passMask8 = _mm_load_si128((v128u8 *)(windowTestPtr + compInfo.target.xCustom));
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = _mm_set1_epi8(0xFF);
			passMaskValue = 0xFFFF;
		}
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
			case NDSColorFormat_BGR666_Rev:
			{
				const v128u16 src16[2] = {
					_mm_load_si128((v128u16 *)((u16 *)vramColorPtr + i) + 0),
					_mm_load_si128((v128u16 *)((u16 *)vramColorPtr + i) + 1)
				};
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v128u8 tempPassMask = _mm_packus_epi16( _mm_srli_epi16(src16[0], 15), _mm_srli_epi16(src16[1], 15) );
					tempPassMask = _mm_cmpeq_epi8(tempPassMask, _mm_set1_epi8(1));
					
					passMask8 = _mm_and_si128(tempPassMask, passMask8);
					passMaskValue = _mm_movemask_epi8(passMask8);
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				const bool didAllPixelsPass = (passMaskValue == 0xFFFF);
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
					_mm_load_si128((v128u32 *)((FragmentColor *)vramColorPtr + i) + 0),
					_mm_load_si128((v128u32 *)((FragmentColor *)vramColorPtr + i) + 1),
					_mm_load_si128((v128u32 *)((FragmentColor *)vramColorPtr + i) + 2),
					_mm_load_si128((v128u32 *)((FragmentColor *)vramColorPtr + i) + 3)
				};
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					v128u8 tempPassMask = _mm_packus_epi16( _mm_packs_epi32(_mm_srli_epi32(src32[0], 24), _mm_srli_epi32(src32[1], 24)), _mm_packs_epi32(_mm_srli_epi32(src32[2], 24), _mm_srli_epi32(src32[3], 24)) );
					tempPassMask = _mm_cmpeq_epi8(tempPassMask, _mm_setzero_si128());
					
					passMask8 = _mm_andnot_si128(tempPassMask, passMask8);
					passMaskValue = _mm_movemask_epi8(passMask8);
				}
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (passMaskValue == 0)
				{
					continue;
				}
				
				// Write out the pixels.
				const bool didAllPixelsPass = (passMaskValue == 0xFFFF);
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
	
	static const size_t step = sizeof(v128u16);
	const v128u8 prioVec8 = _mm_set1_epi8(prio);
	
	const size_t ssePixCount = length - (length % step);
	for (; i < ssePixCount; i+=step, spriteX+=step, frameX+=step)
	{
		const v128u8 prioTabVec8 = _mm_loadu_si128((v128u8 *)(prioTab + frameX));
		const v128u16 color16Lo = _mm_loadu_si128((v128u16 *)(vramBuffer + spriteX) + 0);
		const v128u16 color16Hi = _mm_loadu_si128((v128u16 *)(vramBuffer + spriteX) + 1);
		
		const v128u8 alphaCompare = _mm_cmpeq_epi8( _mm_packus_epi16(_mm_srli_epi16(color16Lo, 15), _mm_srli_epi16(color16Hi, 15)), _mm_set1_epi8(0x01) );
		const v128u8 prioCompare = _mm_cmpgt_epi8(prioTabVec8, prioVec8);
		
		const v128u8 combinedCompare = _mm_and_si128(prioCompare, alphaCompare);
		const v128u16 combinedLoCompare = _mm_unpacklo_epi8(combinedCompare, combinedCompare);
		const v128u16 combinedHiCompare = _mm_unpackhi_epi8(combinedCompare, combinedCompare);
		
		// Just in case you're wondering why we're not using maskmovdqu, but instead using movdqu+pblendvb+movdqu, it's because
		// maskmovdqu won't keep the data in cache, and we really need the data in cache since we're about to render the sprite
		// to the framebuffer. In addition, the maskmovdqu instruction can be brutally slow on many non-Intel CPUs.
		
		_mm_storeu_si128( (v128u16 *)(dst + frameX) + 0, _mm_blendv_epi8(_mm_loadu_si128((v128u16 *)(dst + frameX) + 0), color16Lo, combinedLoCompare) );
		_mm_storeu_si128( (v128u16 *)(dst + frameX) + 1, _mm_blendv_epi8(_mm_loadu_si128((v128u16 *)(dst + frameX) + 1), color16Hi, combinedHiCompare) );
		_mm_storeu_si128( (v128u8 *)(prioTab + frameX),  _mm_blendv_epi8(prioTabVec8, prioVec8, combinedCompare) );
		
		if (!ISDEBUGRENDER)
		{
			_mm_storeu_si128( (v128u8 *)(dst_alpha + frameX),     _mm_blendv_epi8(_mm_loadu_si128((v128u8 *)(dst_alpha + frameX)), _mm_set1_epi8(spriteAlpha + 1), combinedCompare) );
			_mm_storeu_si128( (v128u8 *)(typeTab + frameX),       _mm_blendv_epi8(_mm_loadu_si128((v128u8 *)(typeTab + frameX)), _mm_set1_epi8(OBJMode_Bitmap), combinedCompare) );
			_mm_storeu_si128( (v128u8 *)(this->_sprNum + frameX), _mm_blendv_epi8(_mm_loadu_si128((v128u8 *)(this->_sprNum + frameX)), _mm_set1_epi8(spriteNum), combinedCompare) );
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
	
	__m128i didPassWindowTest;
	__m128i enableColorEffect;
	
	__m128i win0HandledMask;
	__m128i win1HandledMask;
	__m128i winOBJHandledMask;
	__m128i winOUTHandledMask;
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH/sizeof(v128u8); i++)
	{
		didPassWindowTest = _mm_setzero_si128();
		enableColorEffect = _mm_setzero_si128();
		
		win0HandledMask = _mm_setzero_si128();
		win1HandledMask = _mm_setzero_si128();
		winOBJHandledMask = _mm_setzero_si128();
		
		// Window 0 has the highest priority, so always check this first.
		if (win0Ptr != NULL)
		{
			const v128u8 win0Enable = _mm_set1_epi8(compInfo.renderState.WIN0_enable[layerID]);
			const v128u8 win0Effect = _mm_set1_epi8(compInfo.renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			win0HandledMask = _mm_cmpeq_epi8(_mm_load_si128(win0Ptr + i), _mm_set1_epi8(1));
			didPassWindowTest = _mm_and_si128(win0HandledMask, win0Enable);
			enableColorEffect = _mm_and_si128(win0HandledMask, win0Effect);
		}
		
		// Window 1 has medium priority, and is checked after Window 0.
		if (win1Ptr != NULL)
		{
			const v128u8 win1Enable = _mm_set1_epi8(compInfo.renderState.WIN1_enable[layerID]);
			const v128u8 win1Effect = _mm_set1_epi8(compInfo.renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			win1HandledMask = _mm_andnot_si128(win0HandledMask, _mm_cmpeq_epi8(_mm_load_si128(win1Ptr + i), _mm_set1_epi8(1)));
			didPassWindowTest = _mm_blendv_epi8(didPassWindowTest, win1Enable, win1HandledMask);
			enableColorEffect = _mm_blendv_epi8(enableColorEffect, win1Effect, win1HandledMask);
		}
		
		// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
		if (winObjPtr != NULL)
		{
			const v128u8 winObjEnable = _mm_set1_epi8(compInfo.renderState.WINOBJ_enable[layerID]);
			const v128u8 winObjEffect = _mm_set1_epi8(compInfo.renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG]);
			
			winOBJHandledMask = _mm_andnot_si128( _mm_or_si128(win0HandledMask, win1HandledMask), _mm_cmpeq_epi8(_mm_load_si128(winObjPtr + i), _mm_set1_epi8(1)) );
			didPassWindowTest = _mm_blendv_epi8(didPassWindowTest, winObjEnable, winOBJHandledMask);
			enableColorEffect = _mm_blendv_epi8(enableColorEffect, winObjEffect, winOBJHandledMask);
		}
		
		// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
		// This has the lowest priority, and is always checked last.
		const v128u8 winOutEnable = _mm_set1_epi8(compInfo.renderState.WINOUT_enable[layerID]);
		const v128u8 winOutEffect = _mm_set1_epi8(compInfo.renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG]);
		
		winOUTHandledMask = _mm_xor_si128( _mm_or_si128(win0HandledMask, _mm_or_si128(win1HandledMask, winOBJHandledMask)), _mm_set1_epi32(0xFFFFFFFF) );
		didPassWindowTest = _mm_blendv_epi8(didPassWindowTest, winOutEnable, winOUTHandledMask);
		enableColorEffect = _mm_blendv_epi8(enableColorEffect, winOutEffect, winOUTHandledMask);
		
		_mm_store_si128(didPassWindowTestNativePtr + i, didPassWindowTest);
		_mm_store_si128(enableColorEffectNativePtr + i, enableColorEffect);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
size_t GPUEngineA::_RenderLine_Layer3D_LoopOp(GPUEngineCompositorInfo &compInfo, const u8 *__restrict windowTestPtr, const u8 *__restrict colorEffectEnablePtr, const FragmentColor *__restrict srcLinePtr)
{
	static const size_t step = sizeof(v128u8);
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % step));
	const v128u16 evy16 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	const v128u8 srcLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	const v128u8 srcEffectEnableMask = _mm_set1_epi8(compInfo.renderState.srcEffectEnable[GPULayerID_BG0]);
	const v128u8 dstBlendEnableMaskLUT = (COMPOSITORMODE == GPUCompositorMode_Unknown) ? _mm_load_si128((v128u8 *)compInfo.renderState.dstBlendEnableVecLookup) : _mm_setzero_si128();
	
	size_t i = 0;
	for (; i < ssePixCount; i+=step, srcLinePtr+=step, compInfo.target.xCustom+=step, compInfo.target.lineColor16+=step, compInfo.target.lineColor32+=step, compInfo.target.lineLayerID+=step)
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
			passMask8 = _mm_load_si128((v128u8 *)(windowTestPtr + compInfo.target.xCustom));
			
			// If none of the pixels within the vector pass, then reject them all at once.
			passMaskValue = _mm_movemask_epi8(passMask8);
			if (passMaskValue == 0)
			{
				continue;
			}
		}
		else
		{
			passMask8 = _mm_set1_epi8(0xFF);
			passMaskValue = 0xFFFF;
		}
		
		const v128u32 src[4] = {
			_mm_load_si128((v128u32 *)srcLinePtr + 0),
			_mm_load_si128((v128u32 *)srcLinePtr + 1),
			_mm_load_si128((v128u32 *)srcLinePtr + 2),
			_mm_load_si128((v128u32 *)srcLinePtr + 3)
		};
		
		// Do the alpha test. Pixels with an alpha value of 0 are rejected.
		const v128u32 srcAlpha = _mm_packs_epi16( _mm_packs_epi32(_mm_srli_epi32(src[0], 24), _mm_srli_epi32(src[1], 24)),
												  _mm_packs_epi32(_mm_srli_epi32(src[2], 24), _mm_srli_epi32(src[3], 24)) );
		
		passMask8 = _mm_andnot_si128(_mm_cmpeq_epi8(srcAlpha, _mm_setzero_si128()), passMask8);
		
		// If none of the pixels within the vector pass, then reject them all at once.
		passMaskValue = _mm_movemask_epi8(passMask8);
		if (passMaskValue == 0)
		{
			continue;
		}
		
		// Write out the pixels.
		const bool didAllPixelsPass = (passMaskValue == 0xFFFF);
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
	const v128u16 blendEVA_vec = _mm_set1_epi16(blendEVA);
	const v128u16 blendEVB_vec = _mm_set1_epi16(blendEVB);
	
	__m128i srcA_vec;
	__m128i srcB_vec;
	__m128i dstColor;
	
#ifdef ENABLE_SSSE3
	const v128u8 blendAB = _mm_or_si128( blendEVA_vec, _mm_slli_epi16(blendEVB_vec, 8) );
#endif
	
	size_t i = 0;
	
	const size_t vecCount = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? length * sizeof(u32) / sizeof(__m128i) : length * sizeof(u16) / sizeof(__m128i);
	for (; i < vecCount; i++)
	{
		srcA_vec = _mm_load_si128((__m128i *)srcA + i);
		srcB_vec = _mm_load_si128((__m128i *)srcB + i);
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			// Get color masks based on if the alpha value is 0. Colors with an alpha value
			// equal to 0 are rejected.
			v128u32 srcA_alpha = _mm_and_si128(srcA_vec, _mm_set1_epi32(0xFF000000));
			v128u32 srcB_alpha = _mm_and_si128(srcB_vec, _mm_set1_epi32(0xFF000000));
			v128u32 srcA_masked = _mm_andnot_si128(_mm_cmpeq_epi32(srcA_alpha, _mm_setzero_si128()), srcA_vec);
			v128u32 srcB_masked = _mm_andnot_si128(_mm_cmpeq_epi32(srcB_alpha, _mm_setzero_si128()), srcB_vec);
			
			v128u16 outColorLo;
			v128u16 outColorHi;
			
			// Temporarily convert the color component values from 8-bit to 16-bit, and then
			// do the blend calculation.
#ifdef ENABLE_SSSE3
			outColorLo = _mm_unpacklo_epi8(srcA_masked, srcB_masked);
			outColorHi = _mm_unpackhi_epi8(srcA_masked, srcB_masked);
			
			outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
			outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
#else
			v128u16 srcA_maskedLo = _mm_unpacklo_epi8(srcA_masked, _mm_setzero_si128());
			v128u16 srcA_maskedHi = _mm_unpackhi_epi8(srcA_masked, _mm_setzero_si128());
			v128u16 srcB_maskedLo = _mm_unpacklo_epi8(srcB_masked, _mm_setzero_si128());
			v128u16 srcB_maskedHi = _mm_unpackhi_epi8(srcB_masked, _mm_setzero_si128());
			
			outColorLo = _mm_add_epi16( _mm_mullo_epi16(srcA_maskedLo, blendEVA_vec), _mm_mullo_epi16(srcB_maskedLo, blendEVB_vec) );
			outColorHi = _mm_add_epi16( _mm_mullo_epi16(srcA_maskedHi, blendEVA_vec), _mm_mullo_epi16(srcB_maskedHi, blendEVB_vec) );
#endif
			
			outColorLo = _mm_srli_epi16(outColorLo, 4);
			outColorHi = _mm_srli_epi16(outColorHi, 4);
			
			// Convert the color components back from 16-bit to 8-bit using a saturated pack.
			dstColor = _mm_packus_epi16(outColorLo, outColorHi);
			
			// Add the alpha components back in.
			dstColor = _mm_and_si128(dstColor, _mm_set1_epi32(0x00FFFFFF));
			dstColor = _mm_or_si128(dstColor, srcA_alpha);
			dstColor = _mm_or_si128(dstColor, srcB_alpha);
		}
		else
		{
			v128u16 srcA_alpha = _mm_and_si128(srcA_vec, _mm_set1_epi16(0x8000));
			v128u16 srcB_alpha = _mm_and_si128(srcB_vec, _mm_set1_epi16(0x8000));
			v128u16 srcA_masked = _mm_andnot_si128( _mm_cmpeq_epi16(srcA_alpha, _mm_setzero_si128()), srcA_vec );
			v128u16 srcB_masked = _mm_andnot_si128( _mm_cmpeq_epi16(srcB_alpha, _mm_setzero_si128()), srcB_vec );
			v128u16 colorBitMask = _mm_set1_epi16(0x001F);
			
			v128u16 ra;
			v128u16 ga;
			v128u16 ba;
			
#ifdef ENABLE_SSSE3
			ra = _mm_or_si128( _mm_and_si128(               srcA_masked,      colorBitMask), _mm_and_si128(_mm_slli_epi16(srcB_masked, 8), _mm_set1_epi16(0x1F00)) );
			ga = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(srcA_masked,  5), colorBitMask), _mm_and_si128(_mm_slli_epi16(srcB_masked, 3), _mm_set1_epi16(0x1F00)) );
			ba = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(srcA_masked, 10), colorBitMask), _mm_and_si128(_mm_srli_epi16(srcB_masked, 2), _mm_set1_epi16(0x1F00)) );
			
			ra = _mm_maddubs_epi16(ra, blendAB);
			ga = _mm_maddubs_epi16(ga, blendAB);
			ba = _mm_maddubs_epi16(ba, blendAB);
#else
			ra = _mm_and_si128(               srcA_masked,      colorBitMask);
			ga = _mm_and_si128(_mm_srli_epi16(srcA_masked,  5), colorBitMask);
			ba = _mm_and_si128(_mm_srli_epi16(srcA_masked, 10), colorBitMask);
			
			v128u16 rb = _mm_and_si128(               srcB_masked,      colorBitMask);
			v128u16 gb = _mm_and_si128(_mm_srli_epi16(srcB_masked,  5), colorBitMask);
			v128u16 bb = _mm_and_si128(_mm_srli_epi16(srcB_masked, 10), colorBitMask);
			
			ra = _mm_add_epi16( _mm_mullo_epi16(ra, blendEVA_vec), _mm_mullo_epi16(rb, blendEVB_vec) );
			ga = _mm_add_epi16( _mm_mullo_epi16(ga, blendEVA_vec), _mm_mullo_epi16(gb, blendEVB_vec) );
			ba = _mm_add_epi16( _mm_mullo_epi16(ba, blendEVA_vec), _mm_mullo_epi16(bb, blendEVB_vec) );
#endif
			
			ra = _mm_srli_epi16(ra, 4);
			ga = _mm_srli_epi16(ga, 4);
			ba = _mm_srli_epi16(ba, 4);
			
			ra = _mm_min_epi16(ra, colorBitMask);
			ga = _mm_min_epi16(ga, colorBitMask);
			ba = _mm_min_epi16(ba, colorBitMask);
			
			dstColor = _mm_or_si128( _mm_or_si128(_mm_or_si128(ra, _mm_slli_epi16(ga,  5)), _mm_slli_epi16(ba, 10)), _mm_or_si128(srcA_alpha, srcB_alpha) );
		}
		
		_mm_store_si128((__m128i *)dst + i, dstColor);
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
			v128u16 dstColor = _mm_load_si128((v128u16 *)dst + i);
			dstColor = colorop_vec.increase(dstColor, _mm_set1_epi16(intensityClamped));
			dstColor = _mm_or_si128(dstColor, _mm_set1_epi16(0x8000));
			_mm_store_si128((v128u16 *)dst + i, dstColor);
		}
		else
		{
			v128u32 dstColor = _mm_load_si128((v128u32 *)dst + i);
			dstColor = colorop_vec.increase<OUTPUTFORMAT>(dstColor, _mm_set1_epi16(intensityClamped));
			dstColor = _mm_or_si128(dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm_set1_epi32(0x1F000000) : _mm_set1_epi32(0xFF000000));
			_mm_store_si128((v128u32 *)dst + i, dstColor);
		}
	}
	
	return (i * sizeof(__m128i));
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
			v128u16 dstColor = _mm_load_si128((v128u16 *)dst + i);
			dstColor = colorop_vec.decrease(dstColor, _mm_set1_epi16(intensityClamped));
			dstColor = _mm_or_si128(dstColor, _mm_set1_epi16(0x8000));
			_mm_store_si128((v128u16 *)dst + i, dstColor);
		}
		else
		{
			v128u32 dstColor = _mm_load_si128((v128u32 *)dst + i);
			dstColor = colorop_vec.decrease<OUTPUTFORMAT>(dstColor, _mm_set1_epi16(intensityClamped));
			dstColor = _mm_or_si128(dstColor, (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? _mm_set1_epi32(0x1F000000) : _mm_set1_epi32(0xFF000000));
			_mm_store_si128((v128u32 *)dst + i, dstColor);
		}
	}
	
	return (i * sizeof(__m128i));
}

#endif // ENABLE_SSE2
