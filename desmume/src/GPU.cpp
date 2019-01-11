/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2008-2019 DeSmuME team

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

#include "GPU.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <iostream>

#include "common.h"
#include "MMU.h"
#include "FIFO.h"
#include "debug.h"
#include "render3D.h"
#include "registers.h"
#include "gfx3d.h"
#include "debug.h"
#include "NDSSystem.h"
#include "matrix.h"
#include "emufile.h"
#include "utils/task.h"


#ifdef FASTBUILD
	#undef FORCEINLINE
	#define FORCEINLINE
	//compilation speed hack (cuts time exactly in half by cutting out permutations)
	#define DISABLE_MOSAIC
	#define DISABLE_COMPOSITOR_FAST_PATHS
#endif

//instantiate static instance
u16 GPUEngineBase::_brightnessUpTable555[17][0x8000];
FragmentColor GPUEngineBase::_brightnessUpTable666[17][0x8000];
FragmentColor GPUEngineBase::_brightnessUpTable888[17][0x8000];
u16 GPUEngineBase::_brightnessDownTable555[17][0x8000];
FragmentColor GPUEngineBase::_brightnessDownTable666[17][0x8000];
FragmentColor GPUEngineBase::_brightnessDownTable888[17][0x8000];
u8 GPUEngineBase::_blendTable555[17][17][32][32];
GPUEngineBase::MosaicLookup GPUEngineBase::_mosaicLookup;

GPUSubsystem *GPU = NULL;

static size_t _gpuLargestDstLineCount = 1;
static size_t _gpuVRAMBlockOffset = GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH;

static u16 *_gpuDstToSrcIndex = NULL; // Key: Destination pixel index / Value: Source pixel index
static u8 *_gpuDstToSrcSSSE3_u8_8e = NULL;
static u8 *_gpuDstToSrcSSSE3_u8_16e = NULL;
static u8 *_gpuDstToSrcSSSE3_u16_8e = NULL;
static u8 *_gpuDstToSrcSSSE3_u32_4e = NULL;

static CACHE_ALIGN size_t _gpuDstPitchCount[GPU_FRAMEBUFFER_NATIVE_WIDTH];	// Key: Source pixel index in x-dimension / Value: Number of x-dimension destination pixels for the source pixel
static CACHE_ALIGN size_t _gpuDstPitchIndex[GPU_FRAMEBUFFER_NATIVE_WIDTH];	// Key: Source pixel index in x-dimension / Value: First destination pixel that maps to the source pixel

const CACHE_ALIGN SpriteSize GPUEngineBase::_sprSizeTab[4][4] = {
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};

const CACHE_ALIGN BGType GPUEngineBase::_mode2type[8][4] = {
      {BGType_Text, BGType_Text, BGType_Text, BGType_Text},
      {BGType_Text, BGType_Text, BGType_Text, BGType_Affine},
      {BGType_Text, BGType_Text, BGType_Affine, BGType_Affine},
      {BGType_Text, BGType_Text, BGType_Text, BGType_AffineExt},
      {BGType_Text, BGType_Text, BGType_Affine, BGType_AffineExt},
      {BGType_Text, BGType_Text, BGType_AffineExt, BGType_AffineExt},
      {BGType_Invalid, BGType_Invalid, BGType_Large8bpp, BGType_Invalid},
      {BGType_Invalid, BGType_Invalid, BGType_Invalid, BGType_Invalid}
};

//dont ever think of changing these to bits because you could avoid the multiplies in the main tile blitter.
//it doesnt really help any
const CACHE_ALIGN BGLayerSize GPUEngineBase::_BGLayerSizeLUT[8][4] = {
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //Invalid
    {{256,256}, {512,256}, {256,512}, {512,512}}, //text
    {{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine
    {{512,1024}, {1024,512}, {0,0}, {0,0}}, //large 8bpp
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //affine ext (to be elaborated with another value)
	{{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine ext 256x16
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext 256x1
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext direct
};

template <size_t ELEMENTSIZE>
static FORCEINLINE void CopyLinesForVerticalCount(void *__restrict dstLineHead, size_t lineWidth, size_t lineCount)
{
	u8 *__restrict dst = (u8 *)dstLineHead + (lineWidth * ELEMENTSIZE);
	
	for (size_t line = 1; line < lineCount; line++)
	{
		memcpy(dst, dstLineHead, lineWidth * ELEMENTSIZE);
		dst += (lineWidth * ELEMENTSIZE);
	}
}

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand_C(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
{
	if (INTEGERSCALEHINT == 0)
	{
#if defined(MSB_FIRST)
		if (NEEDENDIANSWAP && (ELEMENTSIZE != 1))
		{
			for (size_t i = 0; i < dstWidth; i++)
			{
				if (ELEMENTSIZE == 2)
				{
					((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
				}
				else if (ELEMENTSIZE == 4)
				{
					((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
				}
			}
		}
		else
#endif
		{
			memcpy(dst, src, dstWidth * ELEMENTSIZE);
		}
	}
	else if (INTEGERSCALEHINT == 1)
	{
#if defined(MSB_FIRST)
		if (NEEDENDIANSWAP && (ELEMENTSIZE != 1))
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			{
				if (ELEMENTSIZE == 2)
				{
					((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
				}
				else if (ELEMENTSIZE == 4)
				{
					((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
				}
			}
		}
		else
#endif
		{
			memcpy(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE);
		}
	}
	else if ( (INTEGERSCALEHINT >= 2) && (INTEGERSCALEHINT <= 16) )
	{
		const size_t S = INTEGERSCALEHINT;
		
		if (SCALEVERTICAL)
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				for (size_t q = 0; q < S; q++)
				{
					for (size_t p = 0; p < S; p++)
					{
						if (ELEMENTSIZE == 1)
						{
							( (u8 *)dst)[(q * (GPU_FRAMEBUFFER_NATIVE_WIDTH * S)) + ((x * S) + p)] = ( (u8 *)src)[x];
						}
						else if (ELEMENTSIZE == 2)
						{
							((u16 *)dst)[(q * (GPU_FRAMEBUFFER_NATIVE_WIDTH * S)) + ((x * S) + p)] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[x] ) : ((u16 *)src)[x];
						}
						else if (ELEMENTSIZE == 4)
						{
							((u32 *)dst)[(q * (GPU_FRAMEBUFFER_NATIVE_WIDTH * S)) + ((x * S) + p)] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[x] ) : ((u32 *)src)[x];
						}
					}
				}
			}
		}
		else
		{
			for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
			{
				for (size_t p = 0; p < S; p++)
				{
					if (ELEMENTSIZE == 1)
					{
						( (u8 *)dst)[(x * S) + p] = ( (u8 *)src)[x];
					}
					else if (ELEMENTSIZE == 2)
					{
						((u16 *)dst)[(x * S) + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[x] ) : ((u16 *)src)[x];
					}
					else if (ELEMENTSIZE == 4)
					{
						((u32 *)dst)[(x * S) + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[x] ) : ((u32 *)src)[x];
					}
				}
			}
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
					((u16 *)dst)[_gpuDstPitchIndex[x] + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[x] ) : ((u16 *)src)[x];
				}
				else if (ELEMENTSIZE == 4)
				{
					((u32 *)dst)[_gpuDstPitchIndex[x] + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[x] ) : ((u32 *)src)[x];
				}
			}
		}
		
		if (SCALEVERTICAL)
		{
			CopyLinesForVerticalCount<ELEMENTSIZE>(dst, dstWidth, dstLineCount);
		}
	}
}

#ifdef ENABLE_SSE2
template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand_SSE2(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
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
		__m128i srcPix;
		__m128i srcPixOut[2];
		
		switch (ELEMENTSIZE)
		{
			case 1:
			{
				if (SCALEVERTICAL)
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcPix = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi8(srcPix, srcPix); \
							  srcPixOut[1] = _mm_unpackhi_epi8(srcPix, srcPix); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcPix = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi8(srcPix, srcPix); \
							  srcPixOut[1] = _mm_unpackhi_epi8(srcPix, srcPix); \
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
							  srcPix = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi16(srcPix, srcPix); \
							  srcPixOut[1] = _mm_unpackhi_epi16(srcPix, srcPix); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 0) + 1, srcPixOut[1]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]); \
							  _mm_store_si128((__m128i *)dst + ((X) * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]); \
							  );
				}
				else
				{
					MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), \
							  srcPix = _mm_load_si128((__m128i *)((__m128i *)src + (X))); \
							  srcPixOut[0] = _mm_unpacklo_epi16(srcPix, srcPix); \
							  srcPixOut[1] = _mm_unpackhi_epi16(srcPix, srcPix); \
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
				for (size_t i = 0; i < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE)); i++)
				{
					srcPix = _mm_load_si128((__m128i *)((__m128i *)src + i));
					srcPixOut[0] = _mm_unpacklo_epi32(srcPix, srcPix);
					srcPixOut[1] = _mm_unpackhi_epi32(srcPix, srcPix);
					
					_mm_store_si128((__m128i *)dst + (i * 2) + 0, srcPixOut[0]);
					_mm_store_si128((__m128i *)dst + (i * 2) + 1, srcPixOut[1]);
					
					if (SCALEVERTICAL)
					{
						_mm_store_si128((__m128i *)dst + (i * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 0, srcPixOut[0]);
						_mm_store_si128((__m128i *)dst + (i * 2) + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 2 / (sizeof(__m128i) / ELEMENTSIZE)) * 1) + 1, srcPixOut[1]);
					}
				}
				break;
			}
		}
	}
	else if (INTEGERSCALEHINT == 3)
	{
		__m128i srcPixOut[3];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; )
		{
			if (ELEMENTSIZE == 1)
			{
				const __m128i src8 = _mm_load_si128((__m128i *)((u8 *)src + srcX));
				
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(src8, _mm_set_epi8( 5,  4,  4,  4,  3,  3,  3,  2,  2,  2,  1,  1,  1,  0,  0,  0));
				srcPixOut[1] = _mm_shuffle_epi8(src8, _mm_set_epi8(10, 10,  9,  9,  9,  8,  8,  8,  7,  7,  7,  6,  6,  6,  5,  5));
				srcPixOut[2] = _mm_shuffle_epi8(src8, _mm_set_epi8(15, 15, 15, 14, 14, 14, 13, 13, 13, 12, 12, 12, 11, 11, 11, 10));
#else
				__m128i src8As32[4];
				src8As32[0] = _mm_unpacklo_epi8(src8, src8);
				src8As32[1] = _mm_unpackhi_epi8(src8, src8);
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
				_mm_store_si128((__m128i *)((u8 *)dst + dstX +  0), srcPixOut[0]);
				_mm_store_si128((__m128i *)((u8 *)dst + dstX + 16), srcPixOut[1]);
				_mm_store_si128((__m128i *)((u8 *)dst + dstX + 32), srcPixOut[2]);
				
				if (SCALEVERTICAL)
				{
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) + 16), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) + 32), srcPixOut[2]);
					
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) + 16), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) + 32), srcPixOut[2]);
				}
				
				srcX += 16;
				dstX += 48;
			}
			else if (ELEMENTSIZE == 2)
			{
				const __m128i src16 = _mm_load_si128((__m128i *)((u16 *)src + srcX));
				
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(src16, _mm_set_epi8( 5,  4,  5,  4,  3,  2,  3,  2,  3,  2,  1,  0,  1,  0,  1,  0));
				srcPixOut[1] = _mm_shuffle_epi8(src16, _mm_set_epi8(11, 10,  9,  8,  9,  8,  9,  8,  7,  6,  7,  6,  7,  6,  5,  4));
				srcPixOut[2] = _mm_shuffle_epi8(src16, _mm_set_epi8(15, 14, 15, 14, 15, 14, 13, 12, 13, 12, 13, 12, 11, 10, 11, 10));
#else
				const __m128i src16lo = _mm_shuffle_epi32(src16, 0x44);
				const __m128i src16hi = _mm_shuffle_epi32(src16, 0xEE);
				
				srcPixOut[0] = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16lo, 0x40), 0xA5);
				srcPixOut[1] = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16,   0xFE), 0x40);
				srcPixOut[2] = _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16hi, 0xA5), 0xFE);
#endif
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  0), srcPixOut[0]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  8), srcPixOut[1]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX + 16), srcPixOut[2]);
				
				if (SCALEVERTICAL)
				{
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) +  8), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) + 16), srcPixOut[2]);
					
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) +  8), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) + 16), srcPixOut[2]);
				}
				
				srcX += 8;
				dstX += 24;
			}
			else if (ELEMENTSIZE == 4)
			{
				const __m128i src32 = _mm_load_si128((__m128i *)((u32 *)src + srcX));
				
				srcPixOut[0] = _mm_shuffle_epi32(src32, 0x40);
				srcPixOut[1] = _mm_shuffle_epi32(src32, 0xA5);
				srcPixOut[2] = _mm_shuffle_epi32(src32, 0xFE);
				
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  0), srcPixOut[0]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  4), srcPixOut[1]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  8), srcPixOut[2]);
				
				if (SCALEVERTICAL)
				{
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) +  4), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 1) +  8), srcPixOut[2]);
					
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) +  4), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 3) * 2) +  8), srcPixOut[2]);
				}
				
				srcX += 4;
				dstX += 12;
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		__m128i srcPixOut[4];
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; )
		{
			if (ELEMENTSIZE == 1)
			{
				const __m128i src8  = _mm_load_si128((__m128i *)( (u8 *)src + srcX));
				
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(src8, _mm_set_epi8( 3,  3,  3,  3,  2,  2,  2,  2,  1,  1,  1,  1,  0,  0,  0,  0));
				srcPixOut[1] = _mm_shuffle_epi8(src8, _mm_set_epi8( 7,  7,  7,  7,  6,  6,  6,  6,  5,  5,  5,  5,  4,  4,  4,  4));
				srcPixOut[2] = _mm_shuffle_epi8(src8, _mm_set_epi8(11, 11, 11, 11, 10, 10, 10, 10,  9,  9,  9,  9,  8,  8,  8,  8));
				srcPixOut[3] = _mm_shuffle_epi8(src8, _mm_set_epi8(15, 15, 15, 15, 14, 14, 14, 14, 13, 13, 13, 13, 12, 12, 12, 12));
#else
				const __m128i src8_lo  = _mm_unpacklo_epi8(src8, src8);
				const __m128i src8_hi  = _mm_unpackhi_epi8(src8, src8);
				
				srcPixOut[0] = _mm_unpacklo_epi8(src8_lo, src8_lo);
				srcPixOut[1] = _mm_unpackhi_epi8(src8_lo, src8_lo);
				srcPixOut[2] = _mm_unpacklo_epi8(src8_hi, src8_hi);
				srcPixOut[3] = _mm_unpackhi_epi8(src8_hi, src8_hi);
#endif
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX +  0), srcPixOut[0]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 16), srcPixOut[1]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 32), srcPixOut[2]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 48), srcPixOut[3]);
				
				if (SCALEVERTICAL)
				{
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) + 16), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) + 32), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) + 48), srcPixOut[3]);
					
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) + 16), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) + 32), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) + 48), srcPixOut[3]);
					
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) + 16), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) + 32), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u8 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) + 48), srcPixOut[3]);
				}
				
				srcX += 16;
				dstX += 64;
			}
			else if (ELEMENTSIZE == 2)
			{
				const __m128i src16 = _mm_load_si128((__m128i *)((u16 *)src + srcX));
				
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(src16, _mm_set_epi8( 3,  2,  3,  2,  3,  2,  3,  2,  1,  0,  1,  0,  1,  0,  1,  0));
				srcPixOut[1] = _mm_shuffle_epi8(src16, _mm_set_epi8( 7,  6,  7,  6,  7,  6,  7,  6,  5,  4,  5,  4,  5,  4,  5,  4));
				srcPixOut[2] = _mm_shuffle_epi8(src16, _mm_set_epi8(11, 10, 11, 10, 11, 10, 11, 10,  9,  8,  9,  8,  9,  8,  9,  8));
				srcPixOut[3] = _mm_shuffle_epi8(src16, _mm_set_epi8(15, 14, 15, 14, 15, 14, 15, 14, 13, 12, 13, 12, 13, 12, 13, 12));
#else
				const __m128i src16_lo = _mm_unpacklo_epi16(src16, src16);
				const __m128i src16_hi = _mm_unpackhi_epi16(src16, src16);
				
				srcPixOut[0] = _mm_unpacklo_epi16(src16_lo, src16_lo);
				srcPixOut[1] = _mm_unpackhi_epi16(src16_lo, src16_lo);
				srcPixOut[2] = _mm_unpacklo_epi16(src16_hi, src16_hi);
				srcPixOut[3] = _mm_unpackhi_epi16(src16_hi, src16_hi);
#endif
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  0), srcPixOut[0]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  8), srcPixOut[1]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX + 16), srcPixOut[2]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX + 24), srcPixOut[3]);
				
				if (SCALEVERTICAL)
				{
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) +  8), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) + 16), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) + 24), srcPixOut[3]);
					
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) +  8), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) + 16), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) + 24), srcPixOut[3]);
					
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) +  8), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) + 16), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u16 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) + 24), srcPixOut[3]);
				}
				
				srcX += 8;
				dstX += 32;
			}
			else if (ELEMENTSIZE == 4)
			{
				const __m128i src32 = _mm_load_si128((__m128i *)((u32 *)src + srcX));
				
#ifdef ENABLE_SSSE3
				srcPixOut[0] = _mm_shuffle_epi8(src32, _mm_set_epi8( 3,  2,  1,  0,  3,  2,  1,  0,  3,  2,  1,  0,  3,  2,  1,  0));
				srcPixOut[1] = _mm_shuffle_epi8(src32, _mm_set_epi8( 7,  6,  5,  4,  7,  6,  5,  4,  7,  6,  5,  4,  7,  6,  5,  4));
				srcPixOut[2] = _mm_shuffle_epi8(src32, _mm_set_epi8(11, 10,  9,  8, 11, 10,  9,  8, 11, 10,  9,  8, 11, 10,  9,  8));
				srcPixOut[3] = _mm_shuffle_epi8(src32, _mm_set_epi8(15, 14, 13, 12, 15, 14, 13, 12, 15, 14, 13, 12, 15, 14, 13, 12));
#else
				const __m128i src32_lo = _mm_unpacklo_epi32(src32, src32);
				const __m128i src32_hi = _mm_unpackhi_epi32(src32, src32);
				
				srcPixOut[0] = _mm_unpacklo_epi32(src32_lo, src32_lo);
				srcPixOut[1] = _mm_unpackhi_epi32(src32_lo, src32_lo);
				srcPixOut[2] = _mm_unpacklo_epi32(src32_hi, src32_hi);
				srcPixOut[3] = _mm_unpackhi_epi32(src32_hi, src32_hi);
#endif
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  0), srcPixOut[0]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  4), srcPixOut[1]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  8), srcPixOut[2]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX + 12), srcPixOut[3]);
				
				if (SCALEVERTICAL)
				{
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) +  4), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) +  8), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 1) + 12), srcPixOut[3]);
					
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) +  4), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) +  8), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 2) + 12), srcPixOut[3]);
					
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) +  0), srcPixOut[0]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) +  4), srcPixOut[1]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) +  8), srcPixOut[2]);
					_mm_store_si128((__m128i *)( (u32 *)dst + dstX + ((GPU_FRAMEBUFFER_NATIVE_WIDTH * 4) * 3) + 12), srcPixOut[3]);
				}
				
				srcX += 4;
				dstX += 16;
			}
		}
	}
#ifdef ENABLE_SSSE3
	else if (INTEGERSCALEHINT >= 0)
	{
		const size_t scale = dstWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; )
		{
			if (ELEMENTSIZE == 1)
			{
				const __m128i src8 = _mm_load_si128((__m128i *)((u8 *)src + srcX));
				
				for (size_t s = 0; s < scale; s++)
				{
					const __m128i ssse3idx_u8 = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u8_16e + (s * 16)));
					_mm_store_si128( (__m128i *)( (u8 *)dst + dstX + (s * 16)), _mm_shuffle_epi8( src8, ssse3idx_u8 ) );
				}
				
				srcX += 16;
				dstX += (16 * scale);
			}
			else if (ELEMENTSIZE == 2)
			{
				const __m128i src16 = _mm_load_si128((__m128i *)((u16 *)src + srcX));
				
				for (size_t s = 0; s < scale; s++)
				{
					const __m128i ssse3idx_u16 = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u16_8e + (s * 16)));
					_mm_store_si128( (__m128i *)((u16 *)dst + dstX + (s *  8)), _mm_shuffle_epi8(src16, ssse3idx_u16) );
				}
				
				srcX += 8;
				dstX += (8 * scale);
			}
			else if (ELEMENTSIZE == 4)
			{
				const __m128i src32 = _mm_load_si128((__m128i *)((u32 *)src + srcX));
				
				for (size_t s = 0; s < scale; s++)
				{
					const __m128i ssse3idx_u32 = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u32_4e + (s * 16)));
					_mm_store_si128( (__m128i *)((u32 *)dst + dstX + (s *  4)), _mm_shuffle_epi8(src32, ssse3idx_u32) );
				}
				
				srcX += 4;
				dstX += (4 * scale);
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
#endif

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand(void *__restrict dst, const void *__restrict src, size_t dstWidth, size_t dstLineCount)
{
	// Use INTEGERSCALEHINT to provide a hint to CopyLineExpand() for the fastest execution path.
	// INTEGERSCALEHINT represents the scaling value of the framebuffer width, and is always
	// assumed to be a positive integer.
	//
	// Use cases:
	// - Passing a value of 0 causes CopyLineExpand() to perform a simple copy, using dstWidth
	//   to copy dstWidth elements.
	// - Passing a value of 1 causes CopyLineExpand() to perform a simple copy, ignoring dstWidth
	//   and always copying GPU_FRAMEBUFFER_NATIVE_WIDTH elements.
	// - Passing any negative value causes CopyLineExpand() to assume that the framebuffer width
	//   is NOT scaled by an integer value, and will therefore take the safest (but slowest)
	//   execution path.
	// - Passing any positive value greater than 1 causes CopyLineExpand() to expand the line
	//   using the integer scaling value.
	
#ifdef ENABLE_SSE2
	CopyLineExpand_SSE2<INTEGERSCALEHINT, SCALEVERTICAL, ELEMENTSIZE>(dst, src, dstWidth, dstLineCount);
#else
	CopyLineExpand_C<INTEGERSCALEHINT, SCALEVERTICAL, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, dstWidth, dstLineCount);
#endif
}

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineExpandHinted(const void *__restrict srcBuffer, const size_t srcLineIndex,
						  void *__restrict dstBuffer, const size_t dstLineIndex, const size_t dstLineWidth, const size_t dstLineCount)
{
	switch (INTEGERSCALEHINT)
	{
		case 0:
		{
			const u8 *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (dstLineIndex * dstLineWidth * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (dstLineIndex * dstLineWidth * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			CopyLineExpand<INTEGERSCALEHINT, true, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, dstLineWidth * dstLineCount, 1);
			break;
		}
			
		case 1:
		{
			const u8 *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (srcLineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (srcLineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			CopyLineExpand<INTEGERSCALEHINT, true, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH, 1);
			break;
		}
			
		default:
		{
			const u8 *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (srcLineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (dstLineIndex * dstLineWidth * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			// TODO: Determine INTEGERSCALEHINT earlier in the pipeline, preferably when the framebuffer is first initialized.
			//
			// The implementation below is a stopgap measure for getting the faster code paths to run.
			// However, this setup is not ideal, since the code size will greatly increase in order to
			// include all possible code paths, possibly causing cache misses on lesser CPUs.
			switch (dstLineWidth)
			{
				case (GPU_FRAMEBUFFER_NATIVE_WIDTH * 2):
					CopyLineExpand<2, SCALEVERTICAL, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 2, 2);
					break;
					
				case (GPU_FRAMEBUFFER_NATIVE_WIDTH * 3):
					CopyLineExpand<3, SCALEVERTICAL, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 3, 3);
					break;
					
				case (GPU_FRAMEBUFFER_NATIVE_WIDTH * 4):
					CopyLineExpand<4, SCALEVERTICAL, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 4, 4);
					break;
					
				default:
				{
					if ((dstLineWidth % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0)
					{
						CopyLineExpand<0xFFFF, SCALEVERTICAL, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, dstLineWidth, dstLineCount);
					}
					else
					{
						CopyLineExpand<-1, SCALEVERTICAL, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, dstLineWidth, dstLineCount);
					}
					break;
				}
			}
			break;
		}
	}
}

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineExpandHinted(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer)
{
	CopyLineExpandHinted<INTEGERSCALEHINT, SCALEVERTICAL, USELINEINDEX, NEEDENDIANSWAP, ELEMENTSIZE>(srcBuffer, lineInfo.indexNative,
																									 dstBuffer, lineInfo.indexCustom, lineInfo.widthCustom, lineInfo.renderCount);
}

template <s32 INTEGERSCALEHINT, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineReduce_C(void *__restrict dst, const void *__restrict src, size_t srcWidth)
{
	if (INTEGERSCALEHINT == 0)
	{
#if defined(MSB_FIRST)
		if (NEEDENDIANSWAP && (ELEMENTSIZE != 1))
		{
			for (size_t i = 0; i < srcWidth; i++)
			{
				if (ELEMENTSIZE == 2)
				{
					((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
				}
				else if (ELEMENTSIZE == 4)
				{
					((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
				}
			}
		}
		else
#endif
		{
			memcpy(dst, src, srcWidth * ELEMENTSIZE);
		}
	}
	else if (INTEGERSCALEHINT == 1)
	{
#if defined(MSB_FIRST)
		if (NEEDENDIANSWAP && (ELEMENTSIZE != 1))
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			{
				if (ELEMENTSIZE == 2)
				{
					((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
				}
				else if (ELEMENTSIZE == 4)
				{
					((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
				}
			}
		}
		else
#endif
		{
			memcpy(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE);
		}
	}
	else if ( (INTEGERSCALEHINT >= 2) && (INTEGERSCALEHINT <= 16) )
	{
		const size_t S = INTEGERSCALEHINT;
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			if (ELEMENTSIZE == 1)
			{
				((u8 *)dst)[x] = ((u8 *)src)[x * S];
			}
			else if (ELEMENTSIZE == 2)
			{
				((u16 *)dst)[x] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[x * S] ) : ((u16 *)src)[x * S];
			}
			else if (ELEMENTSIZE == 4)
			{
				((u32 *)dst)[x] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[x * S] ) : ((u32 *)src)[x * S];
			}
		}
	}
	else
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			if (ELEMENTSIZE == 1)
			{
				( (u8 *)dst)[i] = ((u8 *)src)[_gpuDstPitchIndex[i]];
			}
			else if (ELEMENTSIZE == 2)
			{
				((u16 *)dst)[i] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[_gpuDstPitchIndex[i]] ) : ((u16 *)src)[_gpuDstPitchIndex[i]];
			}
			else if (ELEMENTSIZE == 4)
			{
				((u32 *)dst)[i] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[_gpuDstPitchIndex[i]] ) : ((u32 *)src)[_gpuDstPitchIndex[i]];
			}
		}
	}
}

#ifdef ENABLE_SSE2
template <s32 INTEGERSCALEHINT, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineReduce_SSE2(void *__restrict dst, const void *__restrict src, size_t srcWidth)
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
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8(15, 14, 11, 10,  7,  6,  3,  2, 13, 12,  9,  8,  5,  4,  1,  0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8(13, 12,  9,  8,  5,  4,  1,  0, 15, 14, 11, 10,  7,  6,  3,  2));
				
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
		__m128i srcPix[3];
		
		for (size_t dstX = 0; dstX < (GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE)); dstX++)
		{
			srcPix[0] = _mm_load_si128((__m128i *)src + (dstX * 3) + 0);
			srcPix[1] = _mm_load_si128((__m128i *)src + (dstX * 3) + 1);
			srcPix[2] = _mm_load_si128((__m128i *)src + (dstX * 3) + 2);
			
			if (ELEMENTSIZE == 1)
			{
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0xFF0000FF, 0x0000FF00, 0x00FF0000, 0xFF0000FF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0x00FF0000, 0xFF0000FF, 0x0000FF00, 0x00FF0000));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x0000FF00, 0x00FF0000, 0xFF0000FF, 0x0000FF00));
				
#ifdef ENABLE_SSSE3
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8(14, 13, 11, 10,  8,  7,  5,  4,  2,  1, 15, 12,  9,  6,  3,  0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8(15, 13, 12, 10,  9, 14, 11,  8,  5,  2,  7,  6,  4,  3,  1,  0));
				srcPix[2] = _mm_shuffle_epi8(srcPix[2], _mm_set_epi8(13, 10,  7,  4,  1, 15, 14, 12, 11,  9,  8,  6,  5,  3,  2,  0));
				
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[1]);
				srcPix[0] = _mm_or_si128(srcPix[0], srcPix[2]);
#else
				__m128i srcWorking[3];
				
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
				srcPix[0] = _mm_and_si128(srcPix[0], _mm_set_epi32(0x0000FFFF, 0x00000000, 0xFFFF0000, 0x0000FFFF));
				srcPix[1] = _mm_and_si128(srcPix[1], _mm_set_epi32(0xFFFF0000, 0x0000FFFF, 0x00000000, 0xFFFF0000));
				srcPix[2] = _mm_and_si128(srcPix[2], _mm_set_epi32(0x00000000, 0xFFFF0000, 0x0000FFFF, 0x00000000));
				
#ifdef ENABLE_SSSE3
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8(15, 14, 11, 10,  9,  8,  5,  4,  3,  2, 13, 12,  7,  6,  1,  0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8(13, 12, 11, 10, 15, 14,  9,  8,  3,  2,  7,  6,  5,  4,  1,  0));
				srcPix[2] = _mm_shuffle_epi8(srcPix[2], _mm_set_epi8(11, 10,  5,  4, 15, 14, 13, 12,  9,  8,  7,  6,  3,  2,  1,  0));
#else
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
				srcPix[0] = _mm_shuffle_epi8(srcPix[0], _mm_set_epi8(15, 14, 13, 12, 11, 10,  7,  6,  5,  4,  3,  2,  9,  8,  1,  0));
				srcPix[1] = _mm_shuffle_epi8(srcPix[1], _mm_set_epi8(13, 12, 13, 12, 11, 10,  7,  6,  9,  8,  1,  0,  5,  4,  3,  2));
				srcPix[2] = _mm_shuffle_epi8(srcPix[2], _mm_set_epi8(13, 12, 13, 12,  9,  8,  1,  0, 11, 10,  7,  6,  5,  4,  3,  2));
				srcPix[3] = _mm_shuffle_epi8(srcPix[3], _mm_set_epi8( 9,  8,  1,  0, 15, 14, 13, 12, 11, 10,  7,  6,  5,  4,  3,  2));
				
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
	else if ( (INTEGERSCALEHINT >= 5) && (INTEGERSCALEHINT <= 16) )
	{
		const size_t S = INTEGERSCALEHINT;
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			if (ELEMENTSIZE == 1)
			{
				((u8 *)dst)[x] = ((u8 *)src)[x * S];
			}
			else if (ELEMENTSIZE == 2)
			{
				((u16 *)dst)[x] = ((u16 *)src)[x * S];
			}
			else if (ELEMENTSIZE == 4)
			{
				((u32 *)dst)[x] = ((u32 *)src)[x * S];
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
#endif

template <s32 INTEGERSCALEHINT, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineReduce(void *__restrict dst, const void *__restrict src, size_t srcWidth)
{
	// Use INTEGERSCALEHINT to provide a hint to CopyLineReduce() for the fastest execution path.
	// INTEGERSCALEHINT represents the scaling value of the source framebuffer width, and is always
	// assumed to be a positive integer.
	//
	// Use cases:
	// - Passing a value of 0 causes CopyLineReduce() to perform a simple copy, using srcWidth
	//   to copy srcWidth elements.
	// - Passing a value of 1 causes CopyLineReduce() to perform a simple copy, ignoring srcWidth
	//   and always copying GPU_FRAMEBUFFER_NATIVE_WIDTH elements.
	// - Passing any negative value causes CopyLineReduce() to assume that the framebuffer width
	//   is NOT scaled by an integer value, and will therefore take the safest (but slowest)
	//   execution path.
	// - Passing any positive value greater than 1 causes CopyLineReduce() to expand the line
	//   using the integer scaling value.
	
#ifdef ENABLE_SSE2
	CopyLineReduce_SSE2<INTEGERSCALEHINT, ELEMENTSIZE>(dst, src, srcWidth);
#else
	CopyLineReduce_C<INTEGERSCALEHINT, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, srcWidth);
#endif
}

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineReduceHinted(const void *__restrict srcBuffer, const size_t srcLineIndex, const size_t srcLineWidth,
						  void *__restrict dstBuffer, const size_t dstLineIndex)
{
	switch (INTEGERSCALEHINT)
	{
		case 0:
		{
			const u8 *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (srcLineIndex * srcLineWidth * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (srcLineIndex * srcLineWidth * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			CopyLineReduce<INTEGERSCALEHINT, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, srcLineWidth);
			break;
		}
			
		case 1:
		{
			const u8 *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (dstLineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (dstLineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			CopyLineReduce<INTEGERSCALEHINT, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH);
			break;
		}
			
		default:
		{
			const u8 *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (srcLineIndex * srcLineWidth * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (dstLineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			// TODO: Determine INTEGERSCALEHINT earlier in the pipeline, preferably when the framebuffer is first initialized.
			//
			// The implementation below is a stopgap measure for getting the faster code paths to run.
			// However, this setup is not ideal, since the code size will greatly increase in order to
			// include all possible code paths, possibly causing cache misses on lesser CPUs.
			switch (srcLineWidth)
			{
				case (GPU_FRAMEBUFFER_NATIVE_WIDTH * 2):
					CopyLineReduce<2, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 2);
					break;
					
				case (GPU_FRAMEBUFFER_NATIVE_WIDTH * 3):
					CopyLineReduce<3, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 3);
					break;
					
				case (GPU_FRAMEBUFFER_NATIVE_WIDTH * 4):
					CopyLineReduce<4, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
					break;
					
				default:
				{
					if ((srcLineWidth % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0)
					{
						CopyLineReduce<0xFFFF, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, srcLineWidth);
					}
					else
					{
						CopyLineReduce<-1, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, srcLineWidth);
					}
					break;
				}
			}
			break;
		}
	}
}

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineReduceHinted(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer)
{
	CopyLineReduceHinted<INTEGERSCALEHINT, USELINEINDEX, NEEDENDIANSWAP, ELEMENTSIZE>(srcBuffer, lineInfo.indexCustom, lineInfo.widthCustom,
																					  dstBuffer, lineInfo.indexNative);
}

/*****************************************************************************/
//			BACKGROUND RENDERING -ROTOSCALE-
/*****************************************************************************/

FORCEINLINE void rot_tiled_8bit_entry(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	const u16 tileindex = *(u8*)MMU_gpu_map(map + ((auxX>>3) + (auxY>>3) * (lg>>3)));
	const u16 x = auxX & 0x0007;
	const u16 y = auxY & 0x0007;
	
	outIndex = *(u8*)MMU_gpu_map(tile + ((tileindex<<6)+(y<<3)+x));
	outColor = LE_TO_LOCAL_16(pal[outIndex]);
}

template<bool EXTPAL>
FORCEINLINE void rot_tiled_16bit_entry(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	TILEENTRY tileentry;
	tileentry.val = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map(map + (((auxX>>3) + (auxY>>3) * (lg>>3))<<1)) );
	
	const u16 x = ((tileentry.bits.HFlip) ? 7 - (auxX) : (auxX)) & 0x0007;
	const u16 y = ((tileentry.bits.VFlip) ? 7 - (auxY) : (auxY)) & 0x0007;
	
	outIndex = *(u8*)MMU_gpu_map(tile + ((tileentry.bits.TileNum<<6)+(y<<3)+x));
	outColor = LE_TO_LOCAL_16(pal[(outIndex + (EXTPAL ? (tileentry.bits.Palette<<8) : 0))]);
}

FORCEINLINE void rot_256_map(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	outIndex = *(u8*)MMU_gpu_map(map + ((auxX + auxY * lg)));
	outColor = LE_TO_LOCAL_16(pal[outIndex]);
}

FORCEINLINE void rot_BMP_map(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	outColor = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map(map + ((auxX + auxY * lg) << 1)) );
	outIndex = ((outColor & 0x8000) == 0) ? 0 : 1;
}

void gpu_savestate(EMUFILE &os)
{
	GPU->SaveState(os);
}

bool gpu_loadstate(EMUFILE &is, int size)
{
	return GPU->LoadState(is, size);
}

/*****************************************************************************/
//			INITIALIZATION
/*****************************************************************************/
void GPUEngineBase::_InitLUTs()
{
	static bool didInit = false;
	
	if (didInit)
	{
		return;
	}
	
	/*
	NOTE: gbatek (in the reference above) seems to expect 6bit values 
	per component, but as desmume works with 5bit per component, 
	we use 31 as top, instead of 63. Testing it on a few games, 
	using 63 seems to give severe color wraping, and 31 works
	nicely, so for now we'll just that, until proven wrong.

	i have seen pics of pokemon ranger getting white with 31, with 63 it is nice.
	it could be pb of alpha or blending or...

	MightyMax> created a test NDS to check how the brightness values work,
	and 31 seems to be correct. FactorEx is a override for max brighten/darken
	See: http://mightymax.org/gfx_test_brightness.nds
	The Pokemon Problem could be a problem with 8/32 bit writes not recognized yet,
	i'll add that so you can check back.
	*/
	
	for (u16 i = 0; i <= 16; i++)
	{
		for (u16 j = 0x0000; j < 0x8000; j++)
		{
			COLOR cur;

			cur.val = j;
			cur.bits.red = (cur.bits.red + ((31 - cur.bits.red) * i / 16));
			cur.bits.green = (cur.bits.green + ((31 - cur.bits.green) * i / 16));
			cur.bits.blue = (cur.bits.blue + ((31 - cur.bits.blue) * i / 16));
			cur.bits.alpha = 0;
			GPUEngineBase::_brightnessUpTable555[i][j] = cur.val;
			GPUEngineBase::_brightnessUpTable666[i][j].color = COLOR555TO666(cur.val);
			GPUEngineBase::_brightnessUpTable888[i][j].color = COLOR555TO888(cur.val);
			
			cur.val = j;
			cur.bits.red = (cur.bits.red - (cur.bits.red * i / 16));
			cur.bits.green = (cur.bits.green - (cur.bits.green * i / 16));
			cur.bits.blue = (cur.bits.blue - (cur.bits.blue * i / 16));
			cur.bits.alpha = 0;
			GPUEngineBase::_brightnessDownTable555[i][j] = cur.val;
			GPUEngineBase::_brightnessDownTable666[i][j].color = COLOR555TO666(cur.val);
			GPUEngineBase::_brightnessDownTable888[i][j].color = COLOR555TO888(cur.val);
		}
	}
	
	for(int c0=0;c0<=31;c0++) 
		for(int c1=0;c1<=31;c1++) 
			for(int eva=0;eva<=16;eva++)
				for(int evb=0;evb<=16;evb++)
				{
					int blend = ((c0 * eva) + (c1 * evb) ) / 16;
					int final = std::min<int>(31,blend);
					GPUEngineBase::_blendTable555[eva][evb][c0][c1] = final;
				}
	
	didInit = true;
}

GPUEngineBase::GPUEngineBase()
{
	_IORegisterMap = NULL;
	_paletteOBJ = NULL;
	
	_BGLayer[GPULayerID_BG0].layerID = GPULayerID_BG0;
	_BGLayer[GPULayerID_BG1].layerID = GPULayerID_BG1;
	_BGLayer[GPULayerID_BG2].layerID = GPULayerID_BG2;
	_BGLayer[GPULayerID_BG3].layerID = GPULayerID_BG3;
	
	_BGLayer[GPULayerID_BG0].extPaletteSlot = GPULayerID_BG0;
	_BGLayer[GPULayerID_BG1].extPaletteSlot = GPULayerID_BG1;
	_BGLayer[GPULayerID_BG2].extPaletteSlot = GPULayerID_BG2;
	_BGLayer[GPULayerID_BG3].extPaletteSlot = GPULayerID_BG3;
	
	_BGLayer[GPULayerID_BG0].extPalette = NULL;
	_BGLayer[GPULayerID_BG1].extPalette = NULL;
	_BGLayer[GPULayerID_BG2].extPalette = NULL;
	_BGLayer[GPULayerID_BG3].extPalette = NULL;
	
	_InitLUTs();
	_internalRenderLineTargetCustom = NULL;
	_renderLineLayerIDCustom = NULL;
	_deferredIndexCustom = NULL;
	_deferredColorCustom = NULL;
	
	_enableEngine = true;
	_enableBGLayer[GPULayerID_BG0] = true;
	_enableBGLayer[GPULayerID_BG1] = true;
	_enableBGLayer[GPULayerID_BG2] = true;
	_enableBGLayer[GPULayerID_BG3] = true;
	_enableBGLayer[GPULayerID_OBJ] = true;
	
	_needExpandSprColorCustom = false;
	_sprColorCustom = NULL;
	_sprAlphaCustom = NULL;
	_sprTypeCustom = NULL;
	
	if (CommonSettings.num_cores > 1)
	{
		_asyncClearTask = new Task;
		_asyncClearTask->start(false);
	}
	else
	{
		_asyncClearTask = NULL;
	}
	
	_asyncClearTransitionedLineFromBackdropCount = 0;
	_asyncClearLineCustom = 0;
	_asyncClearInterrupt = 0;
	_asyncClearBackdropColor16 = 0;
	_asyncClearBackdropColor32.color = 0;
	_asyncClearIsRunning = false;
	_asyncClearUseInternalCustomBuffer = false;
	
	_didPassWindowTestCustomMasterPtr = NULL;
	_didPassWindowTestCustom[GPULayerID_BG0] = NULL;
	_didPassWindowTestCustom[GPULayerID_BG1] = NULL;
	_didPassWindowTestCustom[GPULayerID_BG2] = NULL;
	_didPassWindowTestCustom[GPULayerID_BG3] = NULL;
	_didPassWindowTestCustom[GPULayerID_OBJ] = NULL;
	
	_enableColorEffectCustomMasterPtr = NULL;
	_enableColorEffectCustom[GPULayerID_BG0] = NULL;
	_enableColorEffectCustom[GPULayerID_BG1] = NULL;
	_enableColorEffectCustom[GPULayerID_BG2] = NULL;
	_enableColorEffectCustom[GPULayerID_BG3] = NULL;
	_enableColorEffectCustom[GPULayerID_OBJ] = NULL;
}

GPUEngineBase::~GPUEngineBase()
{
	if (this->_asyncClearTask != NULL)
	{
		this->RenderLineClearAsyncFinish();
		delete this->_asyncClearTask;
		this->_asyncClearTask = NULL;
	}
	
	free_aligned(this->_internalRenderLineTargetCustom);
	this->_internalRenderLineTargetCustom = NULL;
	free_aligned(this->_renderLineLayerIDCustom);
	this->_renderLineLayerIDCustom = NULL;
	free_aligned(this->_deferredIndexCustom);
	this->_deferredIndexCustom = NULL;
	free_aligned(this->_deferredColorCustom);
	this->_deferredColorCustom = NULL;
	
	free_aligned(this->_sprColorCustom);
	this->_sprColorCustom = NULL;
	free_aligned(this->_sprAlphaCustom);
	this->_sprAlphaCustom = NULL;
	free_aligned(this->_sprTypeCustom);
	this->_sprTypeCustom = NULL;
	
	free_aligned(this->_didPassWindowTestCustomMasterPtr);
	this->_didPassWindowTestCustomMasterPtr = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG0] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG1] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG2] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG3] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_OBJ] = NULL;
	
	this->_enableColorEffectCustomMasterPtr = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG0] = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG1] = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG2] = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG3] = NULL;
	this->_enableColorEffectCustom[GPULayerID_OBJ] = NULL;
}

void GPUEngineBase::_Reset_Base()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	this->_needExpandSprColorCustom = false;
	
	this->SetupBuffers();
	
	memset(this->_sprColor, 0, sizeof(this->_sprColor));
	memset(this->_sprNum, 0, sizeof(this->_sprNum));
	
	memset(this->_didPassWindowTestNative, 1, 5 * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	memset(this->_enableColorEffectNative, 1, 5 * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	memset(this->_didPassWindowTestCustomMasterPtr, 1, 10 * dispInfo.customWidth * sizeof(u8));
	
	memset(this->_h_win[0], 0, sizeof(this->_h_win[0]));
	memset(this->_h_win[1], 0, sizeof(this->_h_win[1]));
	memset(&this->_mosaicColors, 0, sizeof(MosaicColor));
	memset(this->_itemsForPriority, 0, sizeof(this->_itemsForPriority));
	
	memset(this->_internalRenderLineTargetNative, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(FragmentColor));
	
	if (this->_internalRenderLineTargetCustom != NULL)
	{
		memset(this->_internalRenderLineTargetCustom, 0, dispInfo.customWidth * dispInfo.customHeight * dispInfo.pixelBytes);
	}
	
	this->_isBGLayerShown[GPULayerID_BG0] = false;
	this->_isBGLayerShown[GPULayerID_BG1] = false;
	this->_isBGLayerShown[GPULayerID_BG2] = false;
	this->_isBGLayerShown[GPULayerID_BG3] = false;
	this->_isBGLayerShown[GPULayerID_OBJ] = false;
	this->_isAnyBGLayerShown = false;
	
	this->_BGLayer[GPULayerID_BG0].BGnCNT = this->_IORegisterMap->BG0CNT;
	this->_BGLayer[GPULayerID_BG1].BGnCNT = this->_IORegisterMap->BG1CNT;
	this->_BGLayer[GPULayerID_BG2].BGnCNT = this->_IORegisterMap->BG2CNT;
	this->_BGLayer[GPULayerID_BG3].BGnCNT = this->_IORegisterMap->BG3CNT;
	
	this->_BGLayer[GPULayerID_BG0].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	this->_BGLayer[GPULayerID_BG1].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	this->_BGLayer[GPULayerID_BG2].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	this->_BGLayer[GPULayerID_BG3].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	
	this->_BGLayer[GPULayerID_BG0].baseType = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG1].baseType = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG2].baseType = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG3].baseType = BGType_Invalid;
	
	this->_BGLayer[GPULayerID_BG0].type = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG1].type = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG2].type = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG3].type = BGType_Invalid;
	
	this->_BGLayer[GPULayerID_BG0].priority = 0;
	this->_BGLayer[GPULayerID_BG1].priority = 0;
	this->_BGLayer[GPULayerID_BG2].priority = 0;
	this->_BGLayer[GPULayerID_BG3].priority = 0;
	
	this->_BGLayer[GPULayerID_BG0].isVisible = false;
	this->_BGLayer[GPULayerID_BG1].isVisible = false;
	this->_BGLayer[GPULayerID_BG2].isVisible = false;
	this->_BGLayer[GPULayerID_BG3].isVisible = false;
	
	this->_BGLayer[GPULayerID_BG0].isMosaic = false;
	this->_BGLayer[GPULayerID_BG1].isMosaic = false;
	this->_BGLayer[GPULayerID_BG2].isMosaic = false;
	this->_BGLayer[GPULayerID_BG3].isMosaic = false;
	
	this->_BGLayer[GPULayerID_BG0].isDisplayWrapped = false;
	this->_BGLayer[GPULayerID_BG1].isDisplayWrapped = false;
	this->_BGLayer[GPULayerID_BG2].isDisplayWrapped = false;
	this->_BGLayer[GPULayerID_BG3].isDisplayWrapped = false;
	
	this->_BGLayer[GPULayerID_BG0].extPaletteSlot = GPULayerID_BG0;
	this->_BGLayer[GPULayerID_BG1].extPaletteSlot = GPULayerID_BG1;
	this->_BGLayer[GPULayerID_BG0].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG0];
	this->_BGLayer[GPULayerID_BG1].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG1];
	this->_BGLayer[GPULayerID_BG2].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG2];
	this->_BGLayer[GPULayerID_BG3].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG3];
	
	this->_needUpdateWINH[0] = true;
	this->_needUpdateWINH[1] = true;
	
	this->vramBlockOBJAddress = 0;
	
	this->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->isLineRenderNative[l] = true;
		this->isLineOutputNative[l] = true;
	}
	
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.displayOutputMode = GPUDisplayMode_Off;
	renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	renderState.selectedLayerID = GPULayerID_BG0;
	renderState.selectedBGLayer = &this->_BGLayer[GPULayerID_BG0];
	renderState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	renderState.workingBackdropColor16 = renderState.backdropColor16;
	renderState.workingBackdropColor32.color = (dispInfo.colorFormat == NDSColorFormat_BGR666_Rev) ? COLOR555TO666(LOCAL_TO_LE_16(renderState.workingBackdropColor16)) : COLOR555TO888(LOCAL_TO_LE_16(renderState.workingBackdropColor16));
	renderState.colorEffect = (ColorEffect)this->_IORegisterMap->BLDCNT.ColorEffect;
	renderState.blendEVA = 0;
	renderState.blendEVB = 0;
	renderState.blendEVY = 0;
	renderState.masterBrightnessMode = GPUMasterBrightMode_Disable;
	renderState.masterBrightnessIntensity = 0;
	renderState.masterBrightnessIsFullIntensity = false;
	renderState.masterBrightnessIsMaxOrMin = true;
	renderState.blendTable555 = (TBlendTable *)&GPUEngineBase::_blendTable555[renderState.blendEVA][renderState.blendEVB][0][0];
	renderState.brightnessUpTable555 = &GPUEngineBase::_brightnessUpTable555[renderState.blendEVY][0];
	renderState.brightnessUpTable666 = &GPUEngineBase::_brightnessUpTable666[renderState.blendEVY][0];
	renderState.brightnessUpTable888 = &GPUEngineBase::_brightnessUpTable888[renderState.blendEVY][0];
	renderState.brightnessDownTable555 = &GPUEngineBase::_brightnessDownTable555[renderState.blendEVY][0];
	renderState.brightnessDownTable666 = &GPUEngineBase::_brightnessDownTable666[renderState.blendEVY][0];
	renderState.brightnessDownTable888 = &GPUEngineBase::_brightnessDownTable888[renderState.blendEVY][0];
	
	renderState.srcEffectEnable[GPULayerID_BG0] = false;
	renderState.srcEffectEnable[GPULayerID_BG1] = false;
	renderState.srcEffectEnable[GPULayerID_BG2] = false;
	renderState.srcEffectEnable[GPULayerID_BG3] = false;
	renderState.srcEffectEnable[GPULayerID_OBJ] = false;
	renderState.srcEffectEnable[GPULayerID_Backdrop] = false;
	
	renderState.dstBlendEnable[GPULayerID_BG0] = false;
	renderState.dstBlendEnable[GPULayerID_BG1] = false;
	renderState.dstBlendEnable[GPULayerID_BG2] = false;
	renderState.dstBlendEnable[GPULayerID_BG3] = false;
	renderState.dstBlendEnable[GPULayerID_OBJ] = false;
	renderState.dstBlendEnable[GPULayerID_Backdrop] = false;
	renderState.dstAnyBlendEnable = false;
	
#ifdef ENABLE_SSE2
	renderState.srcEffectEnable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	renderState.srcEffectEnable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	renderState.srcEffectEnable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	renderState.srcEffectEnable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	renderState.srcEffectEnable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	renderState.srcEffectEnable_SSE2[GPULayerID_Backdrop] = _mm_setzero_si128();
#ifdef ENABLE_SSSE3
	renderState.dstBlendEnable_SSSE3 = _mm_setzero_si128();
#else
	renderState.dstBlendEnable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	renderState.dstBlendEnable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	renderState.dstBlendEnable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	renderState.dstBlendEnable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	renderState.dstBlendEnable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	renderState.dstBlendEnable_SSE2[GPULayerID_Backdrop] = _mm_setzero_si128();
#endif
#endif
	
	renderState.WIN0_enable[GPULayerID_BG0] = 0;
	renderState.WIN0_enable[GPULayerID_BG1] = 0;
	renderState.WIN0_enable[GPULayerID_BG2] = 0;
	renderState.WIN0_enable[GPULayerID_BG3] = 0;
	renderState.WIN0_enable[GPULayerID_OBJ] = 0;
	renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
	renderState.WIN1_enable[GPULayerID_BG0] = 0;
	renderState.WIN1_enable[GPULayerID_BG1] = 0;
	renderState.WIN1_enable[GPULayerID_BG2] = 0;
	renderState.WIN1_enable[GPULayerID_BG3] = 0;
	renderState.WIN1_enable[GPULayerID_OBJ] = 0;
	renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
	renderState.WINOUT_enable[GPULayerID_BG0] = 0;
	renderState.WINOUT_enable[GPULayerID_BG1] = 0;
	renderState.WINOUT_enable[GPULayerID_BG2] = 0;
	renderState.WINOUT_enable[GPULayerID_BG3] = 0;
	renderState.WINOUT_enable[GPULayerID_OBJ] = 0;
	renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
	renderState.WINOBJ_enable[GPULayerID_BG0] = 0;
	renderState.WINOBJ_enable[GPULayerID_BG1] = 0;
	renderState.WINOBJ_enable[GPULayerID_BG2] = 0;
	renderState.WINOBJ_enable[GPULayerID_BG3] = 0;
	renderState.WINOBJ_enable[GPULayerID_OBJ] = 0;
	renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
#if defined(ENABLE_SSE2)
	renderState.WIN0_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	renderState.WIN0_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	renderState.WIN0_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	renderState.WIN0_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	renderState.WIN0_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	renderState.WIN0_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
	
	renderState.WIN1_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	renderState.WIN1_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	renderState.WIN1_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	renderState.WIN1_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	renderState.WIN1_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	renderState.WIN1_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
	
	renderState.WINOUT_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	renderState.WINOUT_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	renderState.WINOUT_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	renderState.WINOUT_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	renderState.WINOUT_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	renderState.WINOUT_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
	
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	renderState.WINOBJ_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	renderState.WINOBJ_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
#endif
	
	renderState.WIN0_ENABLED = false;
	renderState.WIN1_ENABLED = false;
	renderState.WINOBJ_ENABLED = false;
	renderState.isAnyWindowEnabled = false;
	
	renderState.mosaicWidthBG = this->_mosaicLookup.table[0];
	renderState.mosaicHeightBG = this->_mosaicLookup.table[0];
	renderState.mosaicWidthOBJ = this->_mosaicLookup.table[0];
	renderState.mosaicHeightOBJ = this->_mosaicLookup.table[0];
	renderState.isBGMosaicSet = false;
	renderState.isOBJMosaicSet = false;
	
	renderState.spriteRenderMode = SpriteRenderMode_Sprite1D;
	renderState.spriteBoundary = 0;
	renderState.spriteBMPBoundary = 0;
	
	this->savedBG2X.value = 0;
	this->savedBG2Y.value = 0;
	this->savedBG3X.value = 0;
	this->savedBG3Y.value = 0;
	
	this->_asyncClearTransitionedLineFromBackdropCount = 0;
	this->_asyncClearLineCustom = 0;
	this->_asyncClearInterrupt = 0;
	this->_asyncClearIsRunning = false;
	this->_asyncClearUseInternalCustomBuffer = false;
	
	this->_renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_renderedBuffer = this->_nativeBuffer;
	
	for (size_t line = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		this->_currentCompositorInfo[line].renderState = renderState;
	}
}

void GPUEngineBase::Reset()
{
	this->_Reset_Base();
}

void GPUEngineBase::_ResortBGLayers()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	int i, prio;
	itemsForPriority_t *item;
	
	// we don't need to check for windows here...
	// if we tick boxes, invisible layers become invisible & vice versa
#define OP ^ !
	// if we untick boxes, layers become invisible
	//#define OP &&
	this->_isBGLayerShown[GPULayerID_BG0] = this->_enableBGLayer[GPULayerID_BG0] OP(this->_BGLayer[GPULayerID_BG0].isVisible);
	this->_isBGLayerShown[GPULayerID_BG1] = this->_enableBGLayer[GPULayerID_BG1] OP(this->_BGLayer[GPULayerID_BG1].isVisible);
	this->_isBGLayerShown[GPULayerID_BG2] = this->_enableBGLayer[GPULayerID_BG2] OP(this->_BGLayer[GPULayerID_BG2].isVisible);
	this->_isBGLayerShown[GPULayerID_BG3] = this->_enableBGLayer[GPULayerID_BG3] OP(this->_BGLayer[GPULayerID_BG3].isVisible);
	this->_isBGLayerShown[GPULayerID_OBJ] = this->_enableBGLayer[GPULayerID_OBJ] OP(DISPCNT.OBJ_Enable);
	
	this->_isAnyBGLayerShown = this->_isBGLayerShown[GPULayerID_BG0] || this->_isBGLayerShown[GPULayerID_BG1] || this->_isBGLayerShown[GPULayerID_BG2] || this->_isBGLayerShown[GPULayerID_BG3];
	
	// KISS ! lower priority first, if same then lower num
	for (i = 0; i < NB_PRIORITIES; i++)
	{
		item = &(this->_itemsForPriority[i]);
		item->nbBGs = 0;
		item->nbPixelsX = 0;
	}
	
	for (i = NB_BG; i > 0; )
	{
		i--;
		if (!this->_isBGLayerShown[i]) continue;
		prio = this->_BGLayer[i].priority;
		item = &(this->_itemsForPriority[prio]);
		item->BGs[item->nbBGs]=i;
		item->nbBGs++;
	}
	
#if 0
	//debug
	for (i = 0; i < NB_PRIORITIES; i++)
	{
		item = &(this->_itemsForPriority[i]);
		printf("%d : ", i);
		for (j=0; j<NB_PRIORITIES; j++)
		{
			if (j < item->nbBGs)
				printf("BG%d ", item->BGs[j]);
			else
				printf("... ", item->BGs[j]);
		}
	}
	printf("\n");
#endif
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectBlend(const u16 colA, const u16 colB, const u16 blendEVA, const u16 blendEVB)
{
	u16 ra =  colA        & 0x001F;
	u16 ga = (colA >>  5) & 0x001F;
	u16 ba = (colA >> 10) & 0x001F;
	u16 rb =  colB        & 0x001F;
	u16 gb = (colB >>  5) & 0x001F;
	u16 bb = (colB >> 10) & 0x001F;
	
	ra = ( (ra * blendEVA) + (rb * blendEVB) ) / 16;
	ga = ( (ga * blendEVA) + (gb * blendEVB) ) / 16;
	ba = ( (ba * blendEVA) + (bb * blendEVB) ) / 16;
	
	ra = (ra > 31) ? 31 : ra;
	ga = (ga > 31) ? 31 : ga;
	ba = (ba > 31) ? 31 : ba;
	
	return ra | (ga << 5) | (ba << 10);
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectBlend(const FragmentColor colA, const FragmentColor colB, const u16 blendEVA, const u16 blendEVB)
{
	FragmentColor outColor;
	
	u16 r16 = ( (colA.r * blendEVA) + (colB.r * blendEVB) ) / 16;
	u16 g16 = ( (colA.g * blendEVA) + (colB.g * blendEVB) ) / 16;
	u16 b16 = ( (colA.b * blendEVA) + (colB.b * blendEVB) ) / 16;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		outColor.r = (r16 > 63) ? 63 : r16;
		outColor.g = (g16 > 63) ? 63 : g16;
		outColor.b = (b16 > 63) ? 63 : b16;
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		outColor.r = (r16 > 255) ? 255 : r16;
		outColor.g = (g16 > 255) ? 255 : g16;
		outColor.b = (b16 > 255) ? 255 : b16;
	}
	
	outColor.a = 0;
	return outColor;
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectBlend(const u16 colA, const u16 colB, const TBlendTable *blendTable)
{
	const u8 r = (*blendTable)[ colA        & 0x1F][ colB        & 0x1F];
	const u8 g = (*blendTable)[(colA >>  5) & 0x1F][(colB >>  5) & 0x1F];
	const u8 b = (*blendTable)[(colA >> 10) & 0x1F][(colB >> 10) & 0x1F];

	return r | (g << 5) | (b << 10);
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectBlend3D(const FragmentColor colA, const u16 colB)
{
	const u16 alpha = colA.a + 1;
	COLOR c2;
	COLOR cfinal;
	
	c2.val = colB;
	
	cfinal.bits.red   = ((colA.r * alpha) + ((c2.bits.red   << 1) * (32 - alpha))) >> 6;
	cfinal.bits.green = ((colA.g * alpha) + ((c2.bits.green << 1) * (32 - alpha))) >> 6;
	cfinal.bits.blue  = ((colA.b * alpha) + ((c2.bits.blue  << 1) * (32 - alpha))) >> 6;
	cfinal.bits.alpha = 0;
	
	return cfinal.val;
}

template <NDSColorFormat COLORFORMATB>
FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectBlend3D(const FragmentColor colA, const FragmentColor colB)
{
	FragmentColor blendedColor;
	const u16 alpha = colA.a + 1;
	
	if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
	{
		blendedColor.r = ((colA.r * alpha) + (colB.r * (32 - alpha))) >> 5;
		blendedColor.g = ((colA.g * alpha) + (colB.g * (32 - alpha))) >> 5;
		blendedColor.b = ((colA.b * alpha) + (colB.b * (32 - alpha))) >> 5;
	}
	else if (COLORFORMATB == NDSColorFormat_BGR888_Rev)
	{
		blendedColor.r = ((colA.r * alpha) + (colB.r * (256 - alpha))) >> 8;
		blendedColor.g = ((colA.g * alpha) + (colB.g * (256 - alpha))) >> 8;
		blendedColor.b = ((colA.b * alpha) + (colB.b * (256 - alpha))) >> 8;
	}
	
	blendedColor.a = 0;
	return blendedColor;
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectIncreaseBrightness(const u16 col, const u16 blendEVY)
{
	u16 r =  col        & 0x001F;
	u16 g = (col >>  5) & 0x001F;
	u16 b = (col >> 10) & 0x001F;
	
	r = (r + ((31 - r) * blendEVY / 16));
	g = (g + ((31 - g) * blendEVY / 16));
	b = (b + ((31 - b) * blendEVY / 16));
	
	return r | (g << 5) | (b << 10);
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectIncreaseBrightness(const FragmentColor col, const u16 blendEVY)
{
	FragmentColor newColor;
	newColor.color = 0;
	
	u32 r = col.r;
	u32 g = col.g;
	u32 b = col.b;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		newColor.r = (r + ((63 - r) * blendEVY / 16));
		newColor.g = (g + ((63 - g) * blendEVY / 16));
		newColor.b = (b + ((63 - b) * blendEVY / 16));
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		newColor.r = (r + ((255 - r) * blendEVY / 16));
		newColor.g = (g + ((255 - g) * blendEVY / 16));
		newColor.b = (b + ((255 - b) * blendEVY / 16));
	}
	
	return newColor;
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectDecreaseBrightness(const u16 col, const u16 blendEVY)
{
	u16 r =  col        & 0x001F;
	u16 g = (col >>  5) & 0x001F;
	u16 b = (col >> 10) & 0x001F;
	
	r = (r - (r * blendEVY / 16));
	g = (g - (g * blendEVY / 16));
	b = (b - (b * blendEVY / 16));
	
	return r | (g << 5) | (b << 10);
}

FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectDecreaseBrightness(const FragmentColor col, const u16 blendEVY)
{
	FragmentColor newColor;
	newColor.color = 0;
	
	u32 r = col.r;
	u32 g = col.g;
	u32 b = col.b;
	
	newColor.r = (r - (r * blendEVY / 16));
	newColor.g = (g - (g * blendEVY / 16));
	newColor.b = (b - (b * blendEVY / 16));
	
	return newColor;
}

#ifdef ENABLE_SSE2

template <NDSColorFormat COLORFORMAT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectIncreaseBrightness(const __m128i &col, const __m128i &blendEVY)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i r_vec128 = _mm_and_si128(                col,      _mm_set1_epi16(0x001F) );
		__m128i g_vec128 = _mm_and_si128( _mm_srli_epi16(col,  5), _mm_set1_epi16(0x001F) );
		__m128i b_vec128 = _mm_and_si128( _mm_srli_epi16(col, 10), _mm_set1_epi16(0x001F) );
		
		r_vec128 = _mm_add_epi16( r_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), r_vec128), blendEVY), 4) );
		g_vec128 = _mm_add_epi16( g_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), g_vec128), blendEVY), 4) );
		b_vec128 = _mm_add_epi16( b_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), b_vec128), blendEVY), 4) );
		
		return _mm_or_si128(r_vec128, _mm_or_si128( _mm_slli_epi16(g_vec128, 5), _mm_slli_epi16(b_vec128, 10)) );
	}
	else
	{
		__m128i rgbLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
		__m128i rgbHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
		
		rgbLo = _mm_add_epi16( rgbLo, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbLo), blendEVY), 4) );
		rgbHi = _mm_add_epi16( rgbHi, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbHi), blendEVY), 4) );
		
		return _mm_and_si128( _mm_packus_epi16(rgbLo, rgbHi), _mm_set1_epi32(0x00FFFFFF) );
	}
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectDecreaseBrightness(const __m128i &col, const __m128i &blendEVY)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i r_vec128 = _mm_and_si128(                col,      _mm_set1_epi16(0x001F) );
		__m128i g_vec128 = _mm_and_si128( _mm_srli_epi16(col,  5), _mm_set1_epi16(0x001F) );
		__m128i b_vec128 = _mm_and_si128( _mm_srli_epi16(col, 10), _mm_set1_epi16(0x001F) );
		
		r_vec128 = _mm_sub_epi16( r_vec128, _mm_srli_epi16(_mm_mullo_epi16(r_vec128, blendEVY), 4) );
		g_vec128 = _mm_sub_epi16( g_vec128, _mm_srli_epi16(_mm_mullo_epi16(g_vec128, blendEVY), 4) );
		b_vec128 = _mm_sub_epi16( b_vec128, _mm_srli_epi16(_mm_mullo_epi16(b_vec128, blendEVY), 4) );
		
		return _mm_or_si128(r_vec128, _mm_or_si128( _mm_slli_epi16(g_vec128, 5), _mm_slli_epi16(b_vec128, 10)) );
	}
	else
	{
		__m128i rgbLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
		__m128i rgbHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
		
		rgbLo = _mm_sub_epi16( rgbLo, _mm_srli_epi16(_mm_mullo_epi16(rgbLo, blendEVY), 4) );
		rgbHi = _mm_sub_epi16( rgbHi, _mm_srli_epi16(_mm_mullo_epi16(rgbHi, blendEVY), 4) );
		
		return _mm_and_si128( _mm_packus_epi16(rgbLo, rgbHi), _mm_set1_epi32(0x00FFFFFF) );
	}
}

// Note that if USECONSTANTBLENDVALUESHINT is true, then this method will assume that blendEVA contains identical values
// for each 16-bit vector element, and also that blendEVB contains identical values for each 16-bit vector element. If
// this assumption is broken, then the resulting color will be undefined.
template <NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend(const __m128i &colA, const __m128i &colB, const __m128i &blendEVA, const __m128i &blendEVB)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i ra;
		__m128i ga;
		__m128i ba;
		__m128i colorBitMask = _mm_set1_epi16(0x001F);
		
#ifdef ENABLE_SSSE3
		ra = _mm_or_si128( _mm_and_si128(               colA,      colorBitMask), _mm_and_si128(_mm_slli_epi16(colB, 8), _mm_set1_epi16(0x1F00)) );
		ga = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(colA,  5), colorBitMask), _mm_and_si128(_mm_slli_epi16(colB, 3), _mm_set1_epi16(0x1F00)) );
		ba = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(colA, 10), colorBitMask), _mm_and_si128(_mm_srli_epi16(colB, 2), _mm_set1_epi16(0x1F00)) );
		
		const __m128i blendAB = _mm_or_si128(blendEVA, _mm_slli_epi16(blendEVB, 8));
		ra = _mm_maddubs_epi16(ra, blendAB);
		ga = _mm_maddubs_epi16(ga, blendAB);
		ba = _mm_maddubs_epi16(ba, blendAB);
#else
		ra = _mm_and_si128(               colA,      colorBitMask);
		ga = _mm_and_si128(_mm_srli_epi16(colA,  5), colorBitMask);
		ba = _mm_and_si128(_mm_srli_epi16(colA, 10), colorBitMask);
		
		__m128i rb = _mm_and_si128(               colB,      colorBitMask);
		__m128i gb = _mm_and_si128(_mm_srli_epi16(colB,  5), colorBitMask);
		__m128i bb = _mm_and_si128(_mm_srli_epi16(colB, 10), colorBitMask);
		
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
	else
	{
		__m128i outColorLo;
		__m128i outColorHi;
		__m128i outColor;
		
#ifdef ENABLE_SSSE3
		const __m128i blendAB = _mm_or_si128(blendEVA, _mm_slli_epi16(blendEVB, 8));
		
		outColorLo = _mm_unpacklo_epi8(colA, colB);
		outColorHi = _mm_unpackhi_epi8(colA, colB);
		
		if (USECONSTANTBLENDVALUESHINT)
		{
			outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
			outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
		}
		else
		{
			const __m128i blendABLo = _mm_unpacklo_epi16(blendAB, blendAB);
			const __m128i blendABHi = _mm_unpackhi_epi16(blendAB, blendAB);
			outColorLo = _mm_maddubs_epi16(outColorLo, blendABLo);
			outColorHi = _mm_maddubs_epi16(outColorHi, blendABHi);
		}
#else
		const __m128i colALo = _mm_unpacklo_epi8(colA, _mm_setzero_si128());
		const __m128i colAHi = _mm_unpackhi_epi8(colA, _mm_setzero_si128());
		const __m128i colBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
		const __m128i colBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
		
		if (USECONSTANTBLENDVALUESHINT)
		{
			outColorLo = _mm_add_epi16( _mm_mullo_epi16(colALo, blendEVA), _mm_mullo_epi16(colBLo, blendEVB) );
			outColorHi = _mm_add_epi16( _mm_mullo_epi16(colAHi, blendEVA), _mm_mullo_epi16(colBHi, blendEVB) );
		}
		else
		{
			const __m128i blendALo = _mm_unpacklo_epi16(blendEVA, blendEVA);
			const __m128i blendAHi = _mm_unpackhi_epi16(blendEVA, blendEVA);
			const __m128i blendBLo = _mm_unpacklo_epi16(blendEVB, blendEVB);
			const __m128i blendBHi = _mm_unpackhi_epi16(blendEVB, blendEVB);
			
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
}

template <NDSColorFormat COLORFORMATB>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend3D(const __m128i &colA_Lo, const __m128i &colA_Hi, const __m128i &colB)
{
	if (COLORFORMATB == NDSColorFormat_BGR555_Rev)
	{
		// If the color format of B is 555, then the colA_Hi parameter is required.
		// The color format of A is assumed to be RGB666.
		__m128i ra_lo = _mm_and_si128(                colA_Lo,      _mm_set1_epi32(0x000000FF) );
		__m128i ga_lo = _mm_and_si128( _mm_srli_epi32(colA_Lo,  8), _mm_set1_epi32(0x000000FF) );
		__m128i ba_lo = _mm_and_si128( _mm_srli_epi32(colA_Lo, 16), _mm_set1_epi32(0x000000FF) );
		__m128i aa_lo =                _mm_srli_epi32(colA_Lo, 24);
		
		__m128i ra_hi = _mm_and_si128(                colA_Hi,      _mm_set1_epi32(0x000000FF) );
		__m128i ga_hi = _mm_and_si128( _mm_srli_epi32(colA_Hi,  8), _mm_set1_epi32(0x000000FF) );
		__m128i ba_hi = _mm_and_si128( _mm_srli_epi32(colA_Hi, 16), _mm_set1_epi32(0x000000FF) );
		__m128i aa_hi =                _mm_srli_epi32(colA_Hi, 24);
		
		__m128i ra = _mm_packs_epi32(ra_lo, ra_hi);
		__m128i ga = _mm_packs_epi32(ga_lo, ga_hi);
		__m128i ba = _mm_packs_epi32(ba_lo, ba_hi);
		__m128i aa = _mm_packs_epi32(aa_lo, aa_hi);
		
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
		__m128i rb = _mm_and_si128( _mm_slli_epi16(colB, 1), _mm_set1_epi16(0x003E) );
		__m128i gb = _mm_and_si128( _mm_srli_epi16(colB, 4), _mm_set1_epi16(0x003E) );
		__m128i bb = _mm_and_si128( _mm_srli_epi16(colB, 9), _mm_set1_epi16(0x003E) );
		__m128i ab = _mm_subs_epu16( _mm_set1_epi16(32), aa );
		
		ra = _mm_add_epi16( _mm_mullo_epi16(ra, aa), _mm_mullo_epi16(rb, ab) );
		ga = _mm_add_epi16( _mm_mullo_epi16(ga, aa), _mm_mullo_epi16(gb, ab) );
		ba = _mm_add_epi16( _mm_mullo_epi16(ba, aa), _mm_mullo_epi16(bb, ab) );
#endif
		
		ra = _mm_srli_epi16(ra, 6);
		ga = _mm_srli_epi16(ga, 6);
		ba = _mm_srli_epi16(ba, 6);
		
		return _mm_or_si128( _mm_or_si128(ra, _mm_slli_epi16(ga, 5)), _mm_slli_epi16(ba, 10) );
	}
	else
	{
		// If the color format of B is 666 or 888, then the colA_Hi parameter is ignored.
		// The color format of A is assumed to match the color format of B.
		__m128i rgbALo;
		__m128i rgbAHi;
		
#ifdef ENABLE_SSSE3
		if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
		{
			// Does not work for RGBA8888 color format. The reason is because this
			// algorithm depends on the pmaddubsw instruction, which multiplies
			// two unsigned 8-bit integers into an intermediate signed 16-bit
			// integer. This means that we can overrun the signed 16-bit value
			// range, which would be limited to [-32767 - 32767]. For example, a
			// color component of value 255 multiplied by an alpha value of 255
			// would equal 65025, which is greater than the upper range of a signed
			// 16-bit value.
			rgbALo = _mm_unpacklo_epi8(colA_Lo, colB);
			rgbAHi = _mm_unpackhi_epi8(colA_Lo, colB);
			
			__m128i alpha = _mm_and_si128( _mm_srli_epi32(colA_Lo, 24), _mm_set1_epi32(0x0000001F) );
			alpha = _mm_or_si128( alpha, _mm_or_si128(_mm_slli_epi32(alpha, 8), _mm_slli_epi32(alpha, 16)) );
			alpha = _mm_adds_epu8(alpha, _mm_set1_epi8(1));
			
			__m128i invAlpha = _mm_subs_epu8(_mm_set1_epi8(32), alpha);
			__m128i alphaLo = _mm_unpacklo_epi8(alpha, invAlpha);
			__m128i alphaHi = _mm_unpackhi_epi8(alpha, invAlpha);
			
			rgbALo = _mm_maddubs_epi16(rgbALo, alphaLo);
			rgbAHi = _mm_maddubs_epi16(rgbAHi, alphaHi);
		}
		else
#endif
		{
			rgbALo = _mm_unpacklo_epi8(colA_Lo, _mm_setzero_si128());
			rgbAHi = _mm_unpackhi_epi8(colA_Lo, _mm_setzero_si128());
			__m128i rgbBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
			__m128i rgbBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
			
			__m128i alpha = _mm_and_si128( _mm_srli_epi32(colA_Lo, 24), _mm_set1_epi32(0x000000FF) );
			alpha = _mm_or_si128( alpha, _mm_or_si128(_mm_slli_epi32(alpha, 8), _mm_slli_epi32(alpha, 16)) );
			
			__m128i alphaLo = _mm_unpacklo_epi8(alpha, _mm_setzero_si128());
			__m128i alphaHi = _mm_unpackhi_epi8(alpha, _mm_setzero_si128());
			alphaLo = _mm_add_epi16(alphaLo, _mm_set1_epi16(1));
			alphaHi = _mm_add_epi16(alphaHi, _mm_set1_epi16(1));
			
			if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
			{
				rgbALo = _mm_add_epi16( _mm_mullo_epi16(rgbALo, alphaLo), _mm_mullo_epi16(rgbBLo, _mm_sub_epi16(_mm_set1_epi16(32), alphaLo)) );
				rgbAHi = _mm_add_epi16( _mm_mullo_epi16(rgbAHi, alphaHi), _mm_mullo_epi16(rgbBHi, _mm_sub_epi16(_mm_set1_epi16(32), alphaHi)) );
			}
			else if (COLORFORMATB == NDSColorFormat_BGR888_Rev)
			{
				rgbALo = _mm_add_epi16( _mm_mullo_epi16(rgbALo, alphaLo), _mm_mullo_epi16(rgbBLo, _mm_sub_epi16(_mm_set1_epi16(256), alphaLo)) );
				rgbAHi = _mm_add_epi16( _mm_mullo_epi16(rgbAHi, alphaHi), _mm_mullo_epi16(rgbBHi, _mm_sub_epi16(_mm_set1_epi16(256), alphaHi)) );
			}
		}
		
		if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
		{
			rgbALo = _mm_srli_epi16(rgbALo, 5);
			rgbAHi = _mm_srli_epi16(rgbAHi, 5);
		}
		else if (COLORFORMATB == NDSColorFormat_BGR888_Rev)
		{
			rgbALo = _mm_srli_epi16(rgbALo, 8);
			rgbAHi = _mm_srli_epi16(rgbAHi, 8);
		}
		
		return _mm_and_si128( _mm_packus_epi16(rgbALo, rgbAHi), _mm_set1_epi32(0x00FFFFFF) );
	}
}

#endif

void GPUEngineBase::ParseReg_MASTER_BRIGHT()
{
	const IOREG_MASTER_BRIGHT &MASTER_BRIGHT = this->_IORegisterMap->MASTER_BRIGHT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.masterBrightnessIntensity = (MASTER_BRIGHT.Intensity >= 16) ? 16 : MASTER_BRIGHT.Intensity;
	renderState.masterBrightnessMode = (GPUMasterBrightMode)MASTER_BRIGHT.Mode;
	renderState.masterBrightnessIsFullIntensity = ( (MASTER_BRIGHT.Intensity >= 16) && ((MASTER_BRIGHT.Mode == GPUMasterBrightMode_Up) || (MASTER_BRIGHT.Mode == GPUMasterBrightMode_Down)) );
	renderState.masterBrightnessIsMaxOrMin = ( (MASTER_BRIGHT.Intensity >= 16) || (MASTER_BRIGHT.Intensity == 0) );
}

//Sets up LCD control variables for Display Engines A and B for quick reading
void GPUEngineBase::ParseReg_DISPCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.displayOutputMode = (this->_engineID == GPUEngineID_Main) ? (GPUDisplayMode)DISPCNT.DisplayMode : (GPUDisplayMode)(DISPCNT.DisplayMode & 0x01);
	
	renderState.WIN0_ENABLED = (DISPCNT.Win0_Enable != 0);
	renderState.WIN1_ENABLED = (DISPCNT.Win1_Enable != 0);
	renderState.WINOBJ_ENABLED = (DISPCNT.WinOBJ_Enable != 0);
	renderState.isAnyWindowEnabled = (renderState.WIN0_ENABLED || renderState.WIN1_ENABLED || renderState.WINOBJ_ENABLED);
	
	if (DISPCNT.OBJ_Tile_mapping)
	{
		//1-d sprite mapping boundaries:
		//32k, 64k, 128k, 256k
		renderState.spriteBoundary = 5 + DISPCNT.OBJ_Tile_1D_Bound;
		
		//do not be deceived: even though a sprBoundary==8 (256KB region) is impossible to fully address
		//in GPU_SUB, it is still fully legal to address it with that granularity.
		//so don't do this: //if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3)) gpu->sprBoundary = 7;

		renderState.spriteRenderMode = SpriteRenderMode_Sprite1D;
	}
	else
	{
		//2d sprite mapping
		//boundary : 32k
		renderState.spriteBoundary = 5;
		renderState.spriteRenderMode = SpriteRenderMode_Sprite2D;
	}
     
	if (DISPCNT.OBJ_BMP_1D_Bound && (this->_engineID == GPUEngineID_Main))
		renderState.spriteBMPBoundary = 8;
	else
		renderState.spriteBMPBoundary = 7;
	
	this->ParseReg_BGnCNT(GPULayerID_BG3);
	this->ParseReg_BGnCNT(GPULayerID_BG2);
	this->ParseReg_BGnCNT(GPULayerID_BG1);
	this->ParseReg_BGnCNT(GPULayerID_BG0);
}

void GPUEngineBase::ParseReg_BGnCNT(const GPULayerID layerID)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_BGnCNT &BGnCNT = this->_IORegisterMap->BGnCNT[layerID];
	this->_BGLayer[layerID].BGnCNT = BGnCNT;
	
	switch (layerID)
	{
		case GPULayerID_BG0: this->_BGLayer[layerID].isVisible = (DISPCNT.BG0_Enable != 0); break;
		case GPULayerID_BG1: this->_BGLayer[layerID].isVisible = (DISPCNT.BG1_Enable != 0); break;
		case GPULayerID_BG2: this->_BGLayer[layerID].isVisible = (DISPCNT.BG2_Enable != 0); break;
		case GPULayerID_BG3: this->_BGLayer[layerID].isVisible = (DISPCNT.BG3_Enable != 0); break;
			
		default:
			break;
	}
	
	if (this->_engineID == GPUEngineID_Main)
	{
		this->_BGLayer[layerID].largeBMPAddress  = MMU_ABG;
		this->_BGLayer[layerID].BMPAddress       = MMU_ABG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_16KB);
		this->_BGLayer[layerID].tileMapAddress   = MMU_ABG + (DISPCNT.ScreenBase_Block * ADDRESS_STEP_64KB) + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_2KB);
		this->_BGLayer[layerID].tileEntryAddress = MMU_ABG + (DISPCNT.CharacBase_Block * ADDRESS_STEP_64KB) + (BGnCNT.CharacBase_Block * ADDRESS_STEP_16KB);
	}
	else
	{
		this->_BGLayer[layerID].largeBMPAddress  = MMU_BBG;
		this->_BGLayer[layerID].BMPAddress       = MMU_BBG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_16KB);
		this->_BGLayer[layerID].tileMapAddress   = MMU_BBG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_2KB);
		this->_BGLayer[layerID].tileEntryAddress = MMU_BBG + (BGnCNT.CharacBase_Block * ADDRESS_STEP_16KB);
	}
	
	//clarify affine ext modes
	BGType mode = GPUEngineBase::_mode2type[DISPCNT.BG_Mode][layerID];
	this->_BGLayer[layerID].baseType = mode;
	
	if (mode == BGType_AffineExt)
	{
		//see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol
		const u8 affineModeSelection = (BGnCNT.PaletteMode << 1) | (BGnCNT.CharacBase_Block & 1);
		switch (affineModeSelection)
		{
			case 0:
			case 1:
				mode = BGType_AffineExt_256x16;
				break;
			case 2:
				mode = BGType_AffineExt_256x1;
				break;
			case 3:
				mode = BGType_AffineExt_Direct;
				break;
		}
	}
	
	// Extended palette slots can be changed for BG0 and BG1, but BG2 and BG3 remain constant.
	// Display wrapping can be changed for BG2 and BG3, but BG0 and BG1 cannot wrap.
	if (layerID == GPULayerID_BG0 || layerID == GPULayerID_BG1)
	{
		this->_BGLayer[layerID].extPaletteSlot = (BGnCNT.PaletteSet_Wrap * 2) + layerID;
	}
	else
	{
		this->_BGLayer[layerID].isDisplayWrapped = (BGnCNT.PaletteSet_Wrap != 0);
	}
	
	this->_BGLayer[layerID].type = mode;
	this->_BGLayer[layerID].size = GPUEngineBase::_BGLayerSizeLUT[mode][BGnCNT.ScreenSize];
	this->_BGLayer[layerID].isMosaic = (BGnCNT.Mosaic != 0);
	this->_BGLayer[layerID].priority = BGnCNT.Priority;
	this->_BGLayer[layerID].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][this->_BGLayer[layerID].extPaletteSlot];
	
	this->_ResortBGLayers();
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnHOFS()
{
	const IOREG_BGnHOFS &BGnHOFS = this->_IORegisterMap->BGnOFS[LAYERID].BGnHOFS;
	this->_BGLayer[LAYERID].BGnHOFS = BGnHOFS;
	
#ifdef MSB_FIRST
	this->_BGLayer[LAYERID].xOffset = LOCAL_TO_LE_16(BGnHOFS.value) & 0x01FF;
#else
	this->_BGLayer[LAYERID].xOffset = BGnHOFS.Offset;
#endif
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnVOFS()
{
	const IOREG_BGnVOFS &BGnVOFS = this->_IORegisterMap->BGnOFS[LAYERID].BGnVOFS;
	this->_BGLayer[LAYERID].BGnVOFS = BGnVOFS;
	
#ifdef MSB_FIRST
	this->_BGLayer[LAYERID].yOffset = LOCAL_TO_LE_16(BGnVOFS.value) & 0x01FF;
#else
	this->_BGLayer[LAYERID].yOffset = BGnVOFS.Offset;
#endif
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnX()
{
	if (LAYERID == GPULayerID_BG2)
	{
		this->savedBG2X = this->_IORegisterMap->BG2X;
	}
	else if (LAYERID == GPULayerID_BG3)
	{
		this->savedBG3X = this->_IORegisterMap->BG3X;
	}
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnY()
{
	if (LAYERID == GPULayerID_BG2)
	{
		this->savedBG2Y = this->_IORegisterMap->BG2Y;
	}
	else if (LAYERID == GPULayerID_BG3)
	{
		this->savedBG3Y = this->_IORegisterMap->BG3Y;
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void* GPUEngine_RunClearAsynchronous(void *arg)
{
	GPUEngineBase *gpuEngine = (GPUEngineBase *)arg;
	gpuEngine->RenderLineClearAsync<OUTPUTFORMAT>();
	
	return NULL;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::RenderLineClearAsync()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const bool isCustomClearNeeded = dispInfo.isCustomSizeRequested;
	
	s32 asyncClearLineCustom = atomic_and_barrier32(&this->_asyncClearLineCustom, 0x000000FF);
	
	if (isCustomClearNeeded)
	{
		// Note that the following variables are not explicitly synchronized, and therefore are expected
		// to remain constant while this thread is running:
		//    _asyncClearUseInternalCustomBuffer
		//    _asyncClearBackdropColor16
		//    _asyncClearBackdropColor32
		//
		// These variables are automatically handled when calling RenderLineClearAsyncStart(),
		// and so there should be no need to modify these variables directly.
		u8 *targetBufferHead = (this->_asyncClearUseInternalCustomBuffer) ? (u8 *)this->_internalRenderLineTargetCustom : (u8 *)this->_customBuffer;
		
		while (asyncClearLineCustom < 192)
		{
			const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[asyncClearLineCustom].line;
			
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					memset_u16(targetBufferHead + (lineInfo.blockOffsetCustom * sizeof(u16)), this->_asyncClearBackdropColor16, lineInfo.pixelCount);
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					memset_u32(targetBufferHead + (lineInfo.blockOffsetCustom * sizeof(FragmentColor)), this->_asyncClearBackdropColor32.color, lineInfo.pixelCount);
					break;
			}
			
			asyncClearLineCustom++;
			atomic_inc_barrier32(&this->_asyncClearLineCustom);
			
			if ( atomic_test_and_clear_barrier32(&this->_asyncClearInterrupt, 0x07) )
			{
				return;
			}
		}
	}
	else
	{
		atomic_add_32(&this->_asyncClearLineCustom, 192 - asyncClearLineCustom);
		asyncClearLineCustom = 192;
	}
	
	atomic_test_and_clear_barrier32(&this->_asyncClearInterrupt, 0x07);
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::RenderLineClearAsyncStart(bool willClearInternalCustomBuffer,
											  s32 startLineIndex,
											  u16 clearColor16,
											  FragmentColor clearColor32)
{
	if (this->_asyncClearTask == NULL)
	{
		return;
	}
	
	this->RenderLineClearAsyncFinish();
	
	this->_asyncClearLineCustom = startLineIndex;
	this->_asyncClearBackdropColor16 = clearColor16;
	this->_asyncClearBackdropColor32 = clearColor32;
	this->_asyncClearUseInternalCustomBuffer = willClearInternalCustomBuffer;
	
	this->_asyncClearTask->execute(&GPUEngine_RunClearAsynchronous<OUTPUTFORMAT>, this);
	this->_asyncClearIsRunning = true;
}

void GPUEngineBase::RenderLineClearAsyncFinish()
{
	if (!this->_asyncClearIsRunning)
	{
		return;
	}
	
	atomic_test_and_set_barrier32(&this->_asyncClearInterrupt, 0x07);
	
	this->_asyncClearTask->finish();
	this->_asyncClearIsRunning = false;
	this->_asyncClearInterrupt = 0;
}

void GPUEngineBase::RenderLineClearAsyncWaitForCustomLine(const s32 l)
{
	while (l >= atomic_and_barrier32(&this->_asyncClearLineCustom, 0x000000FF))
	{
		// Do nothing -- just spin.
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_RenderLine_Clear(GPUEngineCompositorInfo &compInfo)
{
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(*compInfo.target.lineColor, compInfo.renderState.workingBackdropColor16);
			break;
			
		case NDSColorFormat_BGR666_Rev:
		case NDSColorFormat_BGR888_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(*compInfo.target.lineColor, compInfo.renderState.workingBackdropColor32.color);
			break;
	}
	
	// init pixels priorities
	assert(NB_PRIORITIES == 4);
	this->_itemsForPriority[0].nbPixelsX = 0;
	this->_itemsForPriority[1].nbPixelsX = 0;
	this->_itemsForPriority[2].nbPixelsX = 0;
	this->_itemsForPriority[3].nbPixelsX = 0;
}

void GPUEngineBase::SetupBuffers()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	memset(this->_renderLineLayerIDNative, GPULayerID_Backdrop, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	
	memset(this->_sprAlpha, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	memset(this->_sprType, OBJMode_Normal, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	memset(this->_sprPrio, 0x7F, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	memset(this->_sprWin, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	
	if (dispInfo.isCustomSizeRequested)
	{
		if (this->_renderLineLayerIDCustom != NULL)
		{
			memset(this->_renderLineLayerIDCustom, GPULayerID_Backdrop, dispInfo.customWidth * dispInfo.customHeight * sizeof(u8));
		}
	}
}

void GPUEngineBase::SetupRenderStates(void *nativeBuffer, void *customBuffer)
{
	this->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->isLineRenderNative[l] = true;
		this->isLineOutputNative[l] = true;
	}
	
	this->_nativeBuffer = nativeBuffer;
	this->_customBuffer = customBuffer;
	this->_renderedBuffer = this->_nativeBuffer;
	this->_renderedWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::UpdateRenderStates(const size_t l)
{
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	GPUEngineRenderState &currRenderState = this->_currentRenderState;
	
	// Get the current backdrop color.
	currRenderState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	if (currRenderState.srcEffectEnable[GPULayerID_Backdrop])
	{
		if (currRenderState.colorEffect == ColorEffect_IncreaseBrightness)
		{
			currRenderState.workingBackdropColor16 = currRenderState.brightnessUpTable555[currRenderState.backdropColor16];
		}
		else if (currRenderState.colorEffect == ColorEffect_DecreaseBrightness)
		{
			currRenderState.workingBackdropColor16 = currRenderState.brightnessDownTable555[currRenderState.backdropColor16];
		}
		else
		{
			currRenderState.workingBackdropColor16 = currRenderState.backdropColor16;
		}
	}
	else
	{
		currRenderState.workingBackdropColor16 = currRenderState.backdropColor16;
	}
	currRenderState.workingBackdropColor32.color = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? COLOR555TO666(LOCAL_TO_LE_16(currRenderState.workingBackdropColor16)) : COLOR555TO888(LOCAL_TO_LE_16(currRenderState.workingBackdropColor16));
	
	// Save the current render states to this line's compositor info.
	compInfo.renderState = currRenderState;
	
	// Handle the asynchronous custom line clearing thread.
	if (compInfo.line.indexNative == 0)
	{
		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		
		// If, in the last frame, we transitioned all 192 lines directly from the backdrop layer,
		// then we'll assume that this current frame will also do the same. Therefore, we will
		// attempt to asynchronously clear all of the custom lines with the backdrop color as an
		// optimization.
		const bool wasPreviousHDFrameFullyTransitionedFromBackdrop = (this->_asyncClearTransitionedLineFromBackdropCount >= 192);
		this->_asyncClearTransitionedLineFromBackdropCount = 0;
		
		if (dispInfo.isCustomSizeRequested && wasPreviousHDFrameFullyTransitionedFromBackdrop)
		{
			this->RenderLineClearAsyncStart<OUTPUTFORMAT>((compInfo.renderState.displayOutputMode != GPUDisplayMode_Normal),
														  compInfo.line.indexNative,
														  compInfo.renderState.workingBackdropColor16,
														  compInfo.renderState.workingBackdropColor32);
		}
	}
	else if (this->_asyncClearIsRunning)
	{
		// If the backdrop color or the display output mode changes mid-frame, then we need to cancel
		// the asynchronous clear and then clear this and any other remaining lines synchronously.
		const bool isUsingInternalCustomBuffer = (compInfo.renderState.displayOutputMode != GPUDisplayMode_Normal);
		
		if ( (compInfo.renderState.workingBackdropColor16 != this->_asyncClearBackdropColor16) ||
			 (isUsingInternalCustomBuffer != this->_asyncClearUseInternalCustomBuffer) )
		{
			this->RenderLineClearAsyncFinish();
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::RenderLine(const size_t l)
{
	// By default, do nothing.
	this->UpdatePropertiesWithoutRender(l);
}

void GPUEngineBase::UpdatePropertiesWithoutRender(const u16 l)
{
	// Update BG2/BG3 parameters for Affine and AffineExt modes
	if (  this->_isBGLayerShown[GPULayerID_BG2] &&
		((this->_BGLayer[GPULayerID_BG2].baseType == BGType_Affine) || (this->_BGLayer[GPULayerID_BG2].baseType == BGType_AffineExt)) )
	{
		IOREG_BG2Parameter &BG2Param = this->_IORegisterMap->BG2Param;
		
		BG2Param.BG2X.value += BG2Param.BG2PB.value;
		BG2Param.BG2Y.value += BG2Param.BG2PD.value;
	}
	
	if (  this->_isBGLayerShown[GPULayerID_BG3] &&
		((this->_BGLayer[GPULayerID_BG3].baseType == BGType_Affine) || (this->_BGLayer[GPULayerID_BG3].baseType == BGType_AffineExt)) )
	{
		IOREG_BG3Parameter &BG3Param = this->_IORegisterMap->BG3Param;
		
		BG3Param.BG3X.value += BG3Param.BG3PB.value;
		BG3Param.BG3Y.value += BG3Param.BG3PD.value;
	}
}

void GPUEngineBase::LastLineProcess()
{
	this->RefreshAffineStartRegs();
}

const GPU_IOREG& GPUEngineBase::GetIORegisterMap() const
{
	return *this->_IORegisterMap;
}

bool GPUEngineBase::IsMasterBrightFullIntensity() const
{
	return this->_currentRenderState.masterBrightnessIsFullIntensity;
}

bool GPUEngineBase::IsMasterBrightMaxOrMin() const
{
	return this->_currentRenderState.masterBrightnessIsMaxOrMin;
}

bool GPUEngineBase::IsMasterBrightFullIntensityAtLineZero() const
{
	return this->_currentCompositorInfo[0].renderState.masterBrightnessIsFullIntensity;
}

void GPUEngineBase::GetMasterBrightnessAtLineZero(GPUMasterBrightMode &outMode, u8 &outIntensity)
{
	outMode = this->_currentCompositorInfo[0].renderState.masterBrightnessMode;
	outIntensity = this->_currentCompositorInfo[0].renderState.masterBrightnessIntensity;
}

/*****************************************************************************/
//			ENABLING / DISABLING LAYERS
/*****************************************************************************/

bool GPUEngineBase::GetEnableState()
{
	return CommonSettings.showGpu.screens[this->_engineID];
}

bool GPUEngineBase::GetEnableStateApplied()
{
	return this->_enableEngine;
}

void GPUEngineBase::SetEnableState(bool theState)
{
	CommonSettings.showGpu.screens[this->_engineID] = theState;
}

bool GPUEngineBase::GetLayerEnableState(const size_t layerIndex)
{
	return CommonSettings.dispLayers[this->_engineID][layerIndex];
}

void GPUEngineBase::SetLayerEnableState(const size_t layerIndex, bool theState)
{
	CommonSettings.dispLayers[this->_engineID][layerIndex] = theState;
}

void GPUEngineBase::ApplySettings()
{
	this->_enableEngine = CommonSettings.showGpu.screens[this->_engineID];
	
	bool needResortBGLayers = ( (this->_enableBGLayer[GPULayerID_BG0] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG0]) ||
							    (this->_enableBGLayer[GPULayerID_BG1] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG1]) ||
							    (this->_enableBGLayer[GPULayerID_BG2] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG2]) ||
							    (this->_enableBGLayer[GPULayerID_BG3] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG3]) ||
							    (this->_enableBGLayer[GPULayerID_OBJ] != CommonSettings.dispLayers[this->_engineID][GPULayerID_OBJ]) );
	
	if (needResortBGLayers)
	{
		this->_enableBGLayer[GPULayerID_BG0] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG0];
		this->_enableBGLayer[GPULayerID_BG1] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG1];
		this->_enableBGLayer[GPULayerID_BG2] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG2];
		this->_enableBGLayer[GPULayerID_BG3] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG3];
		this->_enableBGLayer[GPULayerID_OBJ] = CommonSettings.dispLayers[this->_engineID][GPULayerID_OBJ];
		
		this->_ResortBGLayers();
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_TransitionLineNativeToCustom(GPUEngineCompositorInfo &compInfo)
{
	if (this->isLineRenderNative[compInfo.line.indexNative])
	{
		if (compInfo.renderState.previouslyRenderedLayerID == GPULayerID_Backdrop)
		{
			if (this->_asyncClearIsRunning)
			{
				this->RenderLineClearAsyncWaitForCustomLine(compInfo.line.indexNative);
			}
			else
			{
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memset_u16(compInfo.target.lineColorHeadCustom, compInfo.renderState.workingBackdropColor16, compInfo.line.pixelCount);
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						memset_u32(compInfo.target.lineColorHeadCustom, compInfo.renderState.workingBackdropColor32.color, compInfo.line.pixelCount);
						break;
				}
			}
			
			this->_asyncClearTransitionedLineFromBackdropCount++;
		}
		else
		{
			this->RenderLineClearAsyncFinish();
			
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					CopyLineExpandHinted<0xFFFF, true, false, false, 2>(compInfo.line, compInfo.target.lineColorHeadNative, compInfo.target.lineColorHeadCustom);
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					CopyLineExpandHinted<0xFFFF, true, false, false, 4>(compInfo.line, compInfo.target.lineColorHeadNative, compInfo.target.lineColorHeadCustom);
					break;
			}
			
			CopyLineExpandHinted<0xFFFF, true, false, false, 1>(compInfo.line, compInfo.target.lineLayerIDHeadNative, compInfo.target.lineLayerIDHeadCustom);
		}
		
		compInfo.target.lineColorHead = compInfo.target.lineColorHeadCustom;
		compInfo.target.lineLayerIDHead = compInfo.target.lineLayerIDHeadCustom;
		this->isLineRenderNative[compInfo.line.indexNative] = false;
		this->nativeLineRenderCount--;
	}
}

/*****************************************************************************/
//			PIXEL RENDERING
/*****************************************************************************/
template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void GPUEngineBase::_PixelCopy(GPUEngineCompositorInfo &compInfo, const u16 srcColor16)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			dstColor16 = srcColor16 | 0x8000;
			break;
			
		case NDSColorFormat_BGR666_Rev:
			dstColor32.color = ColorspaceConvert555To6665Opaque<false>(srcColor16);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			dstColor32.color = ColorspaceConvert555To8888Opaque<false>(srcColor16);
			break;
	}
	
	if (!ISDEBUGRENDER)
	{
		dstLayerID = compInfo.renderState.selectedLayerID;
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void GPUEngineBase::_PixelCopy(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			dstColor16 = ColorspaceConvert6665To5551<false>(srcColor32);
			dstColor16 = dstColor16 | 0x8000;
			break;
			
		case NDSColorFormat_BGR666_Rev:
			dstColor32 = srcColor32;
			dstColor32.a = 0x1F;
			break;
			
		case NDSColorFormat_BGR888_Rev:
			dstColor32 = srcColor32;
			dstColor32.a = 0xFF;
			break;
			
		default:
			return;
	}
	
	if (!ISDEBUGRENDER)
	{
		dstLayerID = compInfo.renderState.selectedLayerID;
	}
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessUp(GPUEngineCompositorInfo &compInfo, const u16 srcColor16)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			dstColor16 = compInfo.renderState.brightnessUpTable555[srcColor16 & 0x7FFF] | 0x8000;
			break;
			
		case NDSColorFormat_BGR666_Rev:
			dstColor32 = compInfo.renderState.brightnessUpTable666[srcColor16 & 0x7FFF];
			dstColor32.a = 0x1F;
			break;
			
		case NDSColorFormat_BGR888_Rev:
			dstColor32 = compInfo.renderState.brightnessUpTable888[srcColor16 & 0x7FFF];
			dstColor32.a = 0xFF;
			break;
	}
	
	dstLayerID = compInfo.renderState.selectedLayerID;
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessUp(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const u16 srcColor16 = ColorspaceConvert6665To5551<false>(srcColor32);
		dstColor16 = compInfo.renderState.brightnessUpTable555[srcColor16 & 0x7FFF];
		dstColor16 = dstColor16 | 0x8000;
	}
	else
	{
		dstColor32 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(srcColor32, compInfo.renderState.blendEVY);
		dstColor32.a = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
	}
	
	dstLayerID = compInfo.renderState.selectedLayerID;
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessDown(GPUEngineCompositorInfo &compInfo, const u16 srcColor16)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			dstColor16 = compInfo.renderState.brightnessDownTable555[srcColor16 & 0x7FFF] | 0x8000;
			break;
			
		case NDSColorFormat_BGR666_Rev:
			dstColor32 = compInfo.renderState.brightnessDownTable666[srcColor16 & 0x7FFF];
			dstColor32.a = 0x1F;
			break;
			
		case NDSColorFormat_BGR888_Rev:
			dstColor32 = compInfo.renderState.brightnessDownTable888[srcColor16 & 0x7FFF];
			dstColor32.a = 0xFF;
			break;
	}
	
	dstLayerID = compInfo.renderState.selectedLayerID;
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessDown(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const u16 srcColor16 = ColorspaceConvert6665To5551<false>(srcColor32);
		dstColor16 = compInfo.renderState.brightnessDownTable555[srcColor16 & 0x7FFF];
		dstColor16 = dstColor16 | 0x8000;
	}
	else
	{
		dstColor32 = this->_ColorEffectDecreaseBrightness(srcColor32, compInfo.renderState.blendEVY);
		dstColor32.a = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
	}
	
	dstLayerID = compInfo.renderState.selectedLayerID;
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void GPUEngineBase::_PixelUnknownEffect(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	TBlendTable *selectedBlendTable = compInfo.renderState.blendTable555;
	u8 blendEVA = compInfo.renderState.blendEVA;
	u8 blendEVB = compInfo.renderState.blendEVB;
	
	const bool dstTargetBlendEnable = (dstLayerID != compInfo.renderState.selectedLayerID) && compInfo.renderState.dstBlendEnable[dstLayerID];
	bool forceDstTargetBlend = false;
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		//translucent-capable OBJ are forcing the function to blend when the second target is satisfied
		const bool isObjTranslucentType = (spriteMode == OBJMode_Transparent) || (spriteMode == OBJMode_Bitmap);
		if (isObjTranslucentType && dstTargetBlendEnable)
		{
			// OBJ without fine-grained alpha are using EVA/EVB for blending. This is signified by receiving 0xFF in the alpha.
			// Test cases:
			// * The spriteblend demo
			// * Glory of Heracles - fairy on the title screen
			// * Phoenix Wright: Ace Attorney - character fade-in/fade-out
			if (spriteAlpha != 0xFF)
			{
				blendEVA = spriteAlpha;
				blendEVB = 16 - spriteAlpha;
				selectedBlendTable = &GPUEngineBase::_blendTable555[blendEVA][blendEVB];
			}
			
			forceDstTargetBlend = true;
		}
	}
	
	ColorEffect selectedEffect = ColorEffect_Disable;
	
	if (forceDstTargetBlend)
	{
		selectedEffect = ColorEffect_Blend;
	}
	else
	{
		// If we're not forcing blending, then select the color effect based on the BLDCNT target flags.
		if (enableColorEffect && compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID])
		{
			switch (compInfo.renderState.colorEffect)
			{
				// For the Blend effect, both first and second target flags must be checked.
				case ColorEffect_Blend:
				{
					if (dstTargetBlendEnable) selectedEffect = compInfo.renderState.colorEffect;
					break;
				}
					
				// For the Increase/Decrease Brightness effects, only the first target flag needs to be checked.
				// Test case: Bomberman Land Touch! dialog boxes will render too dark without this check.
				case ColorEffect_IncreaseBrightness:
				case ColorEffect_DecreaseBrightness:
					selectedEffect = compInfo.renderState.colorEffect;
					break;
					
				default:
					break;
			}
		}
	}
	
	// Render the pixel using the selected color effect.
	switch (selectedEffect)
	{
		case ColorEffect_Disable:
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = srcColor16;
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					dstColor32.color = ColorspaceConvert555To6665Opaque<false>(srcColor16);
					break;
					
				case NDSColorFormat_BGR888_Rev:
					dstColor32.color = ColorspaceConvert555To8888Opaque<false>(srcColor16);
					break;
			}
			break;
		}
			
		case ColorEffect_IncreaseBrightness:
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = compInfo.renderState.brightnessUpTable555[srcColor16 & 0x7FFF];
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					dstColor32 = compInfo.renderState.brightnessUpTable666[srcColor16 & 0x7FFF];
					dstColor32.a = 0x1F;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					dstColor32 = compInfo.renderState.brightnessUpTable888[srcColor16 & 0x7FFF];
					dstColor32.a = 0xFF;
					break;
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = compInfo.renderState.brightnessDownTable555[srcColor16 & 0x7FFF];
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					dstColor32 = compInfo.renderState.brightnessDownTable666[srcColor16 & 0x7FFF];
					dstColor32.a = 0x1F;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					dstColor32 = compInfo.renderState.brightnessDownTable888[srcColor16 & 0x7FFF];
					dstColor32.a = 0xFF;
					break;
			}
			break;
		}
			
		case ColorEffect_Blend:
		{
			FragmentColor srcColor32;
			
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = this->_ColorEffectBlend(srcColor16, dstColor16, selectedBlendTable);
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					srcColor32.color = ColorspaceConvert555To6665Opaque<false>(srcColor16);
					dstColor32 = this->_ColorEffectBlend<OUTPUTFORMAT>(srcColor32, dstColor32, blendEVA, blendEVB);
					dstColor32.a = 0x1F;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					srcColor32.color = ColorspaceConvert555To8888Opaque<false>(srcColor16);
					dstColor32 = this->_ColorEffectBlend<OUTPUTFORMAT>(srcColor32, dstColor32, blendEVA, blendEVB);
					dstColor32.a = 0xFF;
					break;
			}
			break;
		}
	}
	
	dstLayerID = compInfo.renderState.selectedLayerID;
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void GPUEngineBase::_PixelUnknownEffect(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode)
{
	u16 &dstColor16 = *compInfo.target.lineColor16;
	FragmentColor &dstColor32 = *compInfo.target.lineColor32;
	u8 &dstLayerID = *compInfo.target.lineLayerID;
	
	u8 blendEVA = compInfo.renderState.blendEVA;
	u8 blendEVB = compInfo.renderState.blendEVB;
	
	const bool dstTargetBlendEnable = (dstLayerID != compInfo.renderState.selectedLayerID) && compInfo.renderState.dstBlendEnable[dstLayerID];
	
	// 3D rendering has a special override: If the destination pixel is set to blend, then always blend.
	// Test case: When starting a stage in Super Princess Peach, the screen will be solid black unless
	// blending is forced here.
	//
	// This behavior must take priority over checking for the window color effect enable flag.
	// Test case: Dialogue boxes in Front Mission will be rendered with blending disabled unless
	// blend forcing takes priority.
	bool forceDstTargetBlend = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnable : false;
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		//translucent-capable OBJ are forcing the function to blend when the second target is satisfied
		const bool isObjTranslucentType = (spriteMode == OBJMode_Transparent) || (spriteMode == OBJMode_Bitmap);
		if (isObjTranslucentType && dstTargetBlendEnable)
		{
			// OBJ without fine-grained alpha are using EVA/EVB for blending. This is signified by receiving 0xFF in the alpha.
			// Test cases:
			// * The spriteblend demo
			// * Glory of Heracles - fairy on the title screen
			// * Phoenix Wright: Ace Attorney - character fade-in/fade-out
			if (spriteAlpha != 0xFF)
			{
				blendEVA = spriteAlpha;
				blendEVB = 16 - spriteAlpha;
			}
			
			forceDstTargetBlend = true;
		}
	}
	
	ColorEffect selectedEffect = ColorEffect_Disable;
	
	if (forceDstTargetBlend)
	{
		selectedEffect = ColorEffect_Blend;
	}
	else
	{
		// If we're not forcing blending, then select the color effect based on the BLDCNT target flags.
		if (enableColorEffect && compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID])
		{
			switch (compInfo.renderState.colorEffect)
			{
				// For the Blend effect, both first and second target flags must be checked.
				case ColorEffect_Blend:
				{
					if (dstTargetBlendEnable) selectedEffect = compInfo.renderState.colorEffect;
					break;
				}
					
				// For the Increase/Decrease Brightness effects, only the first target flag needs to be checked.
				// Test case: Bomberman Land Touch! dialog boxes will render too dark without this check.
				case ColorEffect_IncreaseBrightness:
				case ColorEffect_DecreaseBrightness:
					selectedEffect = compInfo.renderState.colorEffect;
					break;
					
				default:
					break;
			}
		}
	}
	
	// Render the pixel using the selected color effect.
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const u16 srcColor16 = ColorspaceConvert6665To5551<false>(srcColor32);
		
		switch (selectedEffect)
		{
			case ColorEffect_Disable:
				dstColor16 = srcColor16;
				break;
				
			case ColorEffect_IncreaseBrightness:
				dstColor16 = compInfo.renderState.brightnessUpTable555[srcColor16 & 0x7FFF];
				break;
				
			case ColorEffect_DecreaseBrightness:
				dstColor16 = compInfo.renderState.brightnessDownTable555[srcColor16 & 0x7FFF];
				break;
				
			case ColorEffect_Blend:
				dstColor16 = this->_ColorEffectBlend3D(srcColor32, dstColor16);
				break;
		}
		
		dstColor16 |= 0x8000;
	}
	else
	{
		switch (selectedEffect)
		{
			case ColorEffect_Disable:
				dstColor32 = srcColor32;
				break;
				
			case ColorEffect_IncreaseBrightness:
				dstColor32 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(srcColor32, compInfo.renderState.blendEVY);
				break;
				
			case ColorEffect_DecreaseBrightness:
				dstColor32 = this->_ColorEffectDecreaseBrightness(srcColor32, compInfo.renderState.blendEVY);
				break;
				
			case ColorEffect_Blend:
				dstColor32 = (LAYERTYPE == GPULayerType_3D) ? this->_ColorEffectBlend3D<OUTPUTFORMAT>(srcColor32, dstColor32) : this->_ColorEffectBlend<OUTPUTFORMAT>(srcColor32, dstColor32, blendEVA, blendEVB);
				break;
		}
		
		dstColor32.a = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
	}
	
	dstLayerID = compInfo.renderState.selectedLayerID;
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void GPUEngineBase::_PixelComposite(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const bool enableColorEffect, const u8 spriteAlpha, const u8 spriteMode)
{
	switch (COMPOSITORMODE)
	{
		case GPUCompositorMode_Debug:
			this->_PixelCopy<OUTPUTFORMAT, true>(compInfo, srcColor16);
			break;
			
		case GPUCompositorMode_Copy:
			this->_PixelCopy<OUTPUTFORMAT, false>(compInfo, srcColor16);
			break;
			
		case GPUCompositorMode_BrightUp:
			this->_PixelBrightnessUp<OUTPUTFORMAT>(compInfo, srcColor16);
			break;
			
		case GPUCompositorMode_BrightDown:
			this->_PixelBrightnessDown<OUTPUTFORMAT>(compInfo, srcColor16);
			break;
			
		default:
			this->_PixelUnknownEffect<OUTPUTFORMAT, LAYERTYPE>(compInfo, srcColor16, enableColorEffect, spriteAlpha, (OBJMode)spriteMode);
			break;
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void GPUEngineBase::_PixelComposite(GPUEngineCompositorInfo &compInfo, FragmentColor srcColor32, const bool enableColorEffect, const u8 spriteAlpha, const u8 spriteMode)
{
	switch (COMPOSITORMODE)
	{
		case GPUCompositorMode_Debug:
			this->_PixelCopy<OUTPUTFORMAT, true>(compInfo, srcColor32);
			break;
			
		case GPUCompositorMode_Copy:
			this->_PixelCopy<OUTPUTFORMAT, false>(compInfo, srcColor32);
			break;
			
		case GPUCompositorMode_BrightUp:
			this->_PixelBrightnessUp<OUTPUTFORMAT>(compInfo, srcColor32);
			break;
			
		case GPUCompositorMode_BrightDown:
			this->_PixelBrightnessDown<OUTPUTFORMAT>(compInfo, srcColor32);
			break;
			
		default:
			this->_PixelUnknownEffect<OUTPUTFORMAT, LAYERTYPE>(compInfo, srcColor32, enableColorEffect, spriteAlpha, (OBJMode)spriteMode);
			break;
	}
}

#ifdef ENABLE_SSE2

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void GPUEngineBase::_PixelCopy16_SSE2(GPUEngineCompositorInfo &compInfo,
												  const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
												  __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
												  __m128i &dstLayerID)
{
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i alphaBits = _mm_set1_epi16(0x8000);
		dst0 = _mm_or_si128(src0, alphaBits);
		dst1 = _mm_or_si128(src1, alphaBits);
	}
	else
	{
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		dst0 = _mm_or_si128(src0, alphaBits);
		dst1 = _mm_or_si128(src1, alphaBits);
		dst2 = _mm_or_si128(src2, alphaBits);
		dst3 = _mm_or_si128(src3, alphaBits);
	}
	
	if (!ISDEBUGRENDER)
	{
		dstLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER>
FORCEINLINE void GPUEngineBase::_PixelCopyWithMask16_SSE2(GPUEngineCompositorInfo &compInfo,
														  const __m128i &passMask8,
														  const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
														  __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
														  __m128i &dstLayerID)
{
	const __m128i passMask16[2]	= { _mm_unpacklo_epi8(passMask8, passMask8),
								    _mm_unpackhi_epi8(passMask8, passMask8) };
	
	// Do the masked copy.
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i alphaBits = _mm_set1_epi16(0x8000);
		dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(src0, alphaBits), passMask16[0]);
		dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(src1, alphaBits), passMask16[1]);
	}
	else
	{
		const __m128i passMask32[4]	= { _mm_unpacklo_epi16(passMask16[0], passMask16[0]),
									    _mm_unpackhi_epi16(passMask16[0], passMask16[0]),
									    _mm_unpacklo_epi16(passMask16[1], passMask16[1]),
									    _mm_unpackhi_epi16(passMask16[1], passMask16[1]) };
		
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(src0, alphaBits), passMask32[0]);
		dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(src1, alphaBits), passMask32[1]);
		dst2 = _mm_blendv_epi8(dst2, _mm_or_si128(src2, alphaBits), passMask32[2]);
		dst3 = _mm_blendv_epi8(dst3, _mm_or_si128(src3, alphaBits), passMask32[3]);
	}
	
	if (!ISDEBUGRENDER)
	{
		const __m128i srcLayerID_vec128 = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
		dstLayerID = _mm_blendv_epi8(dstLayerID, srcLayerID_vec128, passMask8);
	}
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessUp16_SSE2(GPUEngineCompositorInfo &compInfo,
														  const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
														  __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
														  __m128i &dstLayerID)
{
	const __m128i evy_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i alphaBits = _mm_set1_epi16(0x8000);
		dst0 = _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits);
		dst1 = _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits);
	}
	else
	{
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		dst0 = _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits);
		dst1 = _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits);
		dst2 = _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src2, evy_vec128), alphaBits);
		dst3 = _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src3, evy_vec128), alphaBits);
	}
	
	dstLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessUpWithMask16_SSE2(GPUEngineCompositorInfo &compInfo,
																  const __m128i &passMask8,
																  const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
																  __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
																  __m128i &dstLayerID)
{
	const __m128i passMask16[2]	= { _mm_unpacklo_epi8(passMask8, passMask8),
								    _mm_unpackhi_epi8(passMask8, passMask8) };
	
	const __m128i evy_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i alphaBits = _mm_set1_epi16(0x8000);
		dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits), passMask16[0]);
		dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits), passMask16[1]);
	}
	else
	{
		const __m128i passMask32[4]	= { _mm_unpacklo_epi16(passMask16[0], passMask16[0]),
									    _mm_unpackhi_epi16(passMask16[0], passMask16[0]),
									    _mm_unpacklo_epi16(passMask16[1], passMask16[1]),
									    _mm_unpackhi_epi16(passMask16[1], passMask16[1]) };
		
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits), passMask32[0]);
		dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits), passMask32[1]);
		dst2 = _mm_blendv_epi8(dst2, _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src2, evy_vec128), alphaBits), passMask32[2]);
		dst3 = _mm_blendv_epi8(dst3, _mm_or_si128(this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(src3, evy_vec128), alphaBits), passMask32[3]);
	}
	
	const __m128i srcLayerID_vec128 = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	dstLayerID = _mm_blendv_epi8(dstLayerID, srcLayerID_vec128, passMask8);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessDown16_SSE2(GPUEngineCompositorInfo &compInfo,
															const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
															__m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
															__m128i &dstLayerID)
{
	const __m128i evy_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i alphaBits = _mm_set1_epi16(0x8000);
		dst0 = _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits);
		dst1 = _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits);
	}
	else
	{
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		dst0 = _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits);
		dst1 = _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits);
		dst2 = _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src2, evy_vec128), alphaBits);
		dst3 = _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src3, evy_vec128), alphaBits);
	}
	
	dstLayerID = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
}

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_PixelBrightnessDownWithMask16_SSE2(GPUEngineCompositorInfo &compInfo,
																	const __m128i &passMask8,
																	const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
																	__m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
																	__m128i &dstLayerID)
{
	const __m128i passMask16[2]	= { _mm_unpacklo_epi8(passMask8, passMask8),
								    _mm_unpackhi_epi8(passMask8, passMask8) };
	
	const __m128i evy_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i alphaBits = _mm_set1_epi16(0x8000);
		dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits), passMask16[0]);
		dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits), passMask16[1]);
	}
	else
	{
		const __m128i passMask32[4]	= { _mm_unpacklo_epi16(passMask16[0], passMask16[0]),
									    _mm_unpackhi_epi16(passMask16[0], passMask16[0]),
									    _mm_unpacklo_epi16(passMask16[1], passMask16[1]),
									    _mm_unpackhi_epi16(passMask16[1], passMask16[1]) };
		
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src0, evy_vec128), alphaBits), passMask32[0]);
		dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src1, evy_vec128), alphaBits), passMask32[1]);
		dst2 = _mm_blendv_epi8(dst2, _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src2, evy_vec128), alphaBits), passMask32[2]);
		dst3 = _mm_blendv_epi8(dst3, _mm_or_si128(this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(src3, evy_vec128), alphaBits), passMask32[3]);
	}
	
	const __m128i srcLayerID_vec128 = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	dstLayerID = _mm_blendv_epi8(dstLayerID, srcLayerID_vec128, passMask8);
}

template <NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void GPUEngineBase::_PixelUnknownEffectWithMask16_SSE2(GPUEngineCompositorInfo &compInfo,
																   const __m128i &passMask8,
																   const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
																   const __m128i &srcEffectEnableMask,
																   const __m128i &enableColorEffectMask,
																   const __m128i &spriteAlpha,
																   const __m128i &spriteMode,
																   __m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
																   __m128i &dstLayerID)
{
	const __m128i srcLayerID_vec128 = _mm_set1_epi8(compInfo.renderState.selectedLayerID);
	const __m128i passMask16[2] = { _mm_unpacklo_epi8(passMask8, passMask8),
	                                _mm_unpackhi_epi8(passMask8, passMask8) };
	
	const __m128i passMask32[4] = { _mm_unpacklo_epi16(passMask16[0], passMask16[0]),
	                                _mm_unpackhi_epi16(passMask16[0], passMask16[0]),
	                                _mm_unpacklo_epi16(passMask16[1], passMask16[1]),
	                                _mm_unpackhi_epi16(passMask16[1], passMask16[1]) };
	
	__m128i dstTargetBlendEnableMask;
	
#ifdef ENABLE_SSSE3
	dstTargetBlendEnableMask = _mm_shuffle_epi8(compInfo.renderState.dstBlendEnable_SSSE3, dstLayerID);
	dstTargetBlendEnableMask = _mm_xor_si128( _mm_cmpeq_epi8(dstTargetBlendEnableMask, _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF) );
#else
	dstTargetBlendEnableMask =                                        _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0)), compInfo.renderState.dstBlendEnable_SSE2[GPULayerID_BG0]);
	dstTargetBlendEnableMask = _mm_or_si128(dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG1)), compInfo.renderState.dstBlendEnable_SSE2[GPULayerID_BG1]) );
	dstTargetBlendEnableMask = _mm_or_si128(dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG2)), compInfo.renderState.dstBlendEnable_SSE2[GPULayerID_BG2]) );
	dstTargetBlendEnableMask = _mm_or_si128(dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG3)), compInfo.renderState.dstBlendEnable_SSE2[GPULayerID_BG3]) );
	dstTargetBlendEnableMask = _mm_or_si128(dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_OBJ)), compInfo.renderState.dstBlendEnable_SSE2[GPULayerID_OBJ]) );
	dstTargetBlendEnableMask = _mm_or_si128(dstTargetBlendEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_Backdrop)), compInfo.renderState.dstBlendEnable_SSE2[GPULayerID_Backdrop]) );
#endif
	
	dstTargetBlendEnableMask = _mm_andnot_si128( _mm_cmpeq_epi8(dstLayerID, srcLayerID_vec128), dstTargetBlendEnableMask );
	
	// Select the color effect based on the BLDCNT target flags.
	const __m128i colorEffect_vec128 = _mm_blendv_epi8(_mm_set1_epi8(ColorEffect_Disable), _mm_set1_epi8(compInfo.renderState.colorEffect), enableColorEffectMask);
	const __m128i evy_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVY);
	__m128i forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : _mm_setzero_si128();
	
	// Do note that OBJ layers can modify EVA or EVB, meaning that these blend values may not be constant for OBJ layers.
	// Therefore, we're going to treat EVA and EVB as vectors of uint8 so that the OBJ layer can modify them, and then
	// convert EVA and EVB into vectors of uint16 right before we use them.
	__m128i eva_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? _mm_set1_epi8(compInfo.renderState.blendEVA) : _mm_set1_epi16(compInfo.renderState.blendEVA);
	__m128i evb_vec128 = (LAYERTYPE == GPULayerType_OBJ) ? _mm_set1_epi8(compInfo.renderState.blendEVB) : _mm_set1_epi16(compInfo.renderState.blendEVB);
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const __m128i isObjTranslucentMask = _mm_and_si128( dstTargetBlendEnableMask, _mm_or_si128(_mm_cmpeq_epi8(spriteMode, _mm_set1_epi8(OBJMode_Transparent)), _mm_cmpeq_epi8(spriteMode, _mm_set1_epi8(OBJMode_Bitmap))) );
		forceDstTargetBlendMask = isObjTranslucentMask;
		
		const __m128i spriteAlphaMask = _mm_andnot_si128(_mm_cmpeq_epi8(spriteAlpha, _mm_set1_epi8(0xFF)), isObjTranslucentMask);
		eva_vec128 = _mm_blendv_epi8(eva_vec128, spriteAlpha, spriteAlphaMask);
		evb_vec128 = _mm_blendv_epi8(evb_vec128, _mm_sub_epi8(_mm_set1_epi8(16), spriteAlpha), spriteAlphaMask);
	}
	
	__m128i tmpSrc[4];
	
	if ( (LAYERTYPE == GPULayerType_3D) && (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) )
	{
		// 3D layer blending requires that all src colors are preserved as 32-bit values.
		// Since dst2 and dst3 are currently unused for RGB555 output, we used these variables
		// to store the converted 16-bit src colors in a previous step.
		tmpSrc[0] = dst2;
		tmpSrc[1] = dst3;
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
			const __m128i brightnessMask8 = _mm_andnot_si128( forceDstTargetBlendMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const __m128i brightnessMask16[2] = {_mm_unpacklo_epi8(brightnessMask8, brightnessMask8), _mm_unpackhi_epi8(brightnessMask8, brightnessMask8)};
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask16[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask16[1] );
			}
			else
			{
				const __m128i brightnessMask32[4] = { _mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
				                                      _mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1]) };
				
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask32[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask32[1] );
				tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[2], evy_vec128), brightnessMask32[2] );
				tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[3], evy_vec128), brightnessMask32[3] );
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const __m128i brightnessMask8 = _mm_andnot_si128( forceDstTargetBlendMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const __m128i brightnessMask16[2] = {_mm_unpacklo_epi8(brightnessMask8, brightnessMask8), _mm_unpackhi_epi8(brightnessMask8, brightnessMask8)};
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask16[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask16[1] );
			}
			else
			{
				const __m128i brightnessMask32[4] = { _mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
				                                      _mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1]) };
				
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask32[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask32[1] );
				tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[2], evy_vec128), brightnessMask32[2] );
				tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[3], evy_vec128), brightnessMask32[3] );
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const __m128i blendMask8 = _mm_or_si128( forceDstTargetBlendMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstTargetBlendEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const __m128i blendMask16[2] = {_mm_unpacklo_epi8(blendMask8, blendMask8), _mm_unpackhi_epi8(blendMask8, blendMask8)};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i blendSrc16[2];
		
		switch (LAYERTYPE)
		{
			case GPULayerType_3D:
				blendSrc16[0] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src0, src1, dst0);
				blendSrc16[1] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src2, src3, dst1);
				break;
				
			case GPULayerType_BG:
				blendSrc16[0] = this->_ColorEffectBlend<OUTPUTFORMAT, true>(tmpSrc[0], dst0, eva_vec128, evb_vec128);
				blendSrc16[1] = this->_ColorEffectBlend<OUTPUTFORMAT, true>(tmpSrc[1], dst1, eva_vec128, evb_vec128);
				break;
				
			case GPULayerType_OBJ:
			{
				// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
				const __m128i tempEVA[2] = {
					_mm_unpacklo_epi8(eva_vec128, _mm_setzero_si128()),
					_mm_unpackhi_epi8(eva_vec128, _mm_setzero_si128())
				};
				const __m128i tempEVB[2] = {
					_mm_unpacklo_epi8(evb_vec128, _mm_setzero_si128()),
					_mm_unpackhi_epi8(evb_vec128, _mm_setzero_si128())
				};
				
				blendSrc16[0] = this->_ColorEffectBlend<OUTPUTFORMAT, false>(tmpSrc[0], dst0, tempEVA[0], tempEVB[0]);
				blendSrc16[1] = this->_ColorEffectBlend<OUTPUTFORMAT, false>(tmpSrc[1], dst1, tempEVA[1], tempEVB[1]);
				break;
			}
		}
		
		tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
		tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		
		// Combine the final colors.
		tmpSrc[0] = _mm_or_si128(tmpSrc[0], _mm_set1_epi16(0x8000));
		tmpSrc[1] = _mm_or_si128(tmpSrc[1], _mm_set1_epi16(0x8000));
		
		dst0 = _mm_blendv_epi8(dst0, tmpSrc[0], passMask16[0]);
		dst1 = _mm_blendv_epi8(dst1, tmpSrc[1], passMask16[1]);
	}
	else
	{
		__m128i blendSrc32[4];
		
		switch (LAYERTYPE)
		{
			case GPULayerType_3D:
				blendSrc32[0] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src0, src0, dst0);
				blendSrc32[1] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src1, src1, dst1);
				blendSrc32[2] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src2, src2, dst2);
				blendSrc32[3] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src3, src3, dst3);
				break;
				
			case GPULayerType_BG:
				blendSrc32[0] = this->_ColorEffectBlend<OUTPUTFORMAT, true>(tmpSrc[0], dst0, eva_vec128, evb_vec128);
				blendSrc32[1] = this->_ColorEffectBlend<OUTPUTFORMAT, true>(tmpSrc[1], dst1, eva_vec128, evb_vec128);
				blendSrc32[2] = this->_ColorEffectBlend<OUTPUTFORMAT, true>(tmpSrc[2], dst2, eva_vec128, evb_vec128);
				blendSrc32[3] = this->_ColorEffectBlend<OUTPUTFORMAT, true>(tmpSrc[3], dst3, eva_vec128, evb_vec128);
				break;
				
			case GPULayerType_OBJ:
			{
				// For OBJ layers, we need to convert EVA and EVB from vectors of uint8 into vectors of uint16.
				//
				// Note that we are sending only 4 colors for each _ColorEffectBlend() call, and so we are only
				// going to send the 4 correspending EVA/EVB vectors as well. In this case, each individual
				// EVA/EVB value is mirrored for each adjacent 16-bit boundary.
				__m128i tempBlendLo = _mm_unpacklo_epi8(eva_vec128, eva_vec128);
				__m128i tempBlendHi = _mm_unpackhi_epi8(eva_vec128, eva_vec128);
				
				const __m128i tempEVA[4] = {
					_mm_unpacklo_epi8(tempBlendLo, _mm_setzero_si128()),
					_mm_unpackhi_epi8(tempBlendLo, _mm_setzero_si128()),
					_mm_unpacklo_epi8(tempBlendHi, _mm_setzero_si128()),
					_mm_unpackhi_epi8(tempBlendHi, _mm_setzero_si128())
				};
				
				tempBlendLo = _mm_unpacklo_epi8(evb_vec128, evb_vec128);
				tempBlendHi = _mm_unpackhi_epi8(evb_vec128, evb_vec128);
				
				const __m128i tempEVB[4] = {
					_mm_unpacklo_epi8(tempBlendLo, _mm_setzero_si128()),
					_mm_unpackhi_epi8(tempBlendLo, _mm_setzero_si128()),
					_mm_unpacklo_epi8(tempBlendHi, _mm_setzero_si128()),
					_mm_unpackhi_epi8(tempBlendHi, _mm_setzero_si128())
				};
				
				blendSrc32[0] = this->_ColorEffectBlend<OUTPUTFORMAT, false>(tmpSrc[0], dst0, tempEVA[0], tempEVB[0]);
				blendSrc32[1] = this->_ColorEffectBlend<OUTPUTFORMAT, false>(tmpSrc[1], dst1, tempEVA[1], tempEVB[1]);
				blendSrc32[2] = this->_ColorEffectBlend<OUTPUTFORMAT, false>(tmpSrc[2], dst2, tempEVA[2], tempEVB[2]);
				blendSrc32[3] = this->_ColorEffectBlend<OUTPUTFORMAT, false>(tmpSrc[3], dst3, tempEVA[3], tempEVB[3]);
				break;
			}
		}
		
		const __m128i blendMask32[4] = { _mm_unpacklo_epi16(blendMask16[0], blendMask16[0]),
		                                 _mm_unpackhi_epi16(blendMask16[0], blendMask16[0]),
		                                 _mm_unpacklo_epi16(blendMask16[1], blendMask16[1]),
		                                 _mm_unpackhi_epi16(blendMask16[1], blendMask16[1]) };
		
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		
		tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
		tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
		tmpSrc[2] = _mm_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
		tmpSrc[3] = _mm_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		
		tmpSrc[0] = _mm_or_si128(tmpSrc[0], alphaBits);
		tmpSrc[1] = _mm_or_si128(tmpSrc[1], alphaBits);
		tmpSrc[2] = _mm_or_si128(tmpSrc[2], alphaBits);
		tmpSrc[3] = _mm_or_si128(tmpSrc[3], alphaBits);
		
		dst0 = _mm_blendv_epi8(dst0, tmpSrc[0], passMask32[0]);
		dst1 = _mm_blendv_epi8(dst1, tmpSrc[1], passMask32[1]);
		dst2 = _mm_blendv_epi8(dst2, tmpSrc[2], passMask32[2]);
		dst3 = _mm_blendv_epi8(dst3, tmpSrc[3], passMask32[3]);
	}
	
	dstLayerID = _mm_blendv_epi8(dstLayerID, srcLayerID_vec128, passMask8);
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_PixelComposite16_SSE2(GPUEngineCompositorInfo &compInfo,
													   const bool didAllPixelsPass,
													   const __m128i &passMask8,
													   const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
													   const __m128i &srcEffectEnableMask,
													   const u8 *__restrict enableColorEffectPtr,
													   const u8 *__restrict sprAlphaPtr,
													   const u8 *__restrict sprModePtr)
{
	const bool is555and3D = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) && (LAYERTYPE == GPULayerType_3D);
	__m128i dst[4];
	__m128i dstLayerID_vec128;
	
	if (is555and3D)
	{
		// 3D layer blending requires that all src colors are preserved as 32-bit values.
		// Since dst2 and dst3 are currently unused for RGB555 output, we using these variables
		// to store the converted 16-bit src colors.
		dst[2] = _mm_packs_epi32( _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src0, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src0, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src0, _mm_set1_epi32(0x003E0000)), 7)),
		                          _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src1, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src1, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src1, _mm_set1_epi32(0x003E0000)), 7)) );
		dst[3] = _mm_packs_epi32( _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src2, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src2, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src2, _mm_set1_epi32(0x003E0000)), 7)),
		                          _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src3, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src3, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src3, _mm_set1_epi32(0x003E0000)), 7)) );
	}
	
	if ((COMPOSITORMODE != GPUCompositorMode_Unknown) && didAllPixelsPass)
	{
		switch (COMPOSITORMODE)
		{
			case GPUCompositorMode_Debug:
				this->_PixelCopy16_SSE2<OUTPUTFORMAT, true>(compInfo,
															src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
															dst[3], dst[2], dst[1], dst[0],
															dstLayerID_vec128);
				break;
				
			case GPUCompositorMode_Copy:
				this->_PixelCopy16_SSE2<OUTPUTFORMAT, false>(compInfo,
															 src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
															 dst[3], dst[2], dst[1], dst[0],
															 dstLayerID_vec128);
				break;
				
			case GPUCompositorMode_BrightUp:
				this->_PixelBrightnessUp16_SSE2<OUTPUTFORMAT>(compInfo,
															  src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
															  dst[3], dst[2], dst[1], dst[0],
															  dstLayerID_vec128);
				break;
				
			case GPUCompositorMode_BrightDown:
				this->_PixelBrightnessDown16_SSE2<OUTPUTFORMAT>(compInfo,
																src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
																dst[3], dst[2], dst[1], dst[0],
																dstLayerID_vec128);
				break;
				
			default:
				break;
		}
	}
	else
	{
		// Read the destination pixels into registers if we're doing a masked pixel write.
		dst[0] = _mm_load_si128((__m128i *)*compInfo.target.lineColor + 0);
		dst[1] = _mm_load_si128((__m128i *)*compInfo.target.lineColor + 1);
		
		if (OUTPUTFORMAT != NDSColorFormat_BGR555_Rev)
		{
			dst[2] = _mm_load_si128((__m128i *)*compInfo.target.lineColor + 2);
			dst[3] = _mm_load_si128((__m128i *)*compInfo.target.lineColor + 3);
		}
		
		dstLayerID_vec128 = _mm_load_si128((__m128i *)compInfo.target.lineLayerID);
		
		switch (COMPOSITORMODE)
		{
			case GPUCompositorMode_Debug:
				this->_PixelCopyWithMask16_SSE2<OUTPUTFORMAT, true>(compInfo,
																	passMask8,
																	src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
																	dst[3], dst[2], dst[1], dst[0],
																	dstLayerID_vec128);
				break;
				
			case GPUCompositorMode_Copy:
				this->_PixelCopyWithMask16_SSE2<OUTPUTFORMAT, false>(compInfo,
																	 passMask8,
																	 src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
																	 dst[3], dst[2], dst[1], dst[0],
																	 dstLayerID_vec128);
				break;
				
			case GPUCompositorMode_BrightUp:
				this->_PixelBrightnessUpWithMask16_SSE2<OUTPUTFORMAT>(compInfo,
																	  passMask8,
																	  src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
																	  dst[3], dst[2], dst[1], dst[0],
																	  dstLayerID_vec128);
				break;
				
			case GPUCompositorMode_BrightDown:
				this->_PixelBrightnessDownWithMask16_SSE2<OUTPUTFORMAT>(compInfo,
																		passMask8,
																		src3, src2, (!is555and3D) ? src1 : dst[3], (!is555and3D) ? src0 : dst[2],
																		dst[3], dst[2], dst[1], dst[0],
																		dstLayerID_vec128);
				break;
				
			default:
			{
				const __m128i enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? _mm_cmpeq_epi8( _mm_load_si128((__m128i *)enableColorEffectPtr), _mm_set1_epi8(1) ) : _mm_set1_epi8(0xFF);
				const __m128i spriteAlpha = (LAYERTYPE == GPULayerType_OBJ) ? _mm_load_si128((__m128i *)sprAlphaPtr) : _mm_setzero_si128();
				const __m128i spriteMode = (LAYERTYPE == GPULayerType_OBJ) ? _mm_load_si128((__m128i *)sprModePtr) : _mm_setzero_si128();
				
				this->_PixelUnknownEffectWithMask16_SSE2<OUTPUTFORMAT, LAYERTYPE>(compInfo,
																				  passMask8,
																				  src3, src2, src1, src0,
																				  srcEffectEnableMask,
																				  enableColorEffectMask,
																				  spriteAlpha,
																				  spriteMode,
																				  dst[3], dst[2], dst[1], dst[0],
																				  dstLayerID_vec128);
				break;
			}
		}
	}
	
	_mm_store_si128((__m128i *)*compInfo.target.lineColor + 0, dst[0]);
	_mm_store_si128((__m128i *)*compInfo.target.lineColor + 1, dst[1]);
	
	if (OUTPUTFORMAT != NDSColorFormat_BGR555_Rev)
	{
		_mm_store_si128((__m128i *)*compInfo.target.lineColor + 2, dst[2]);
		_mm_store_si128((__m128i *)*compInfo.target.lineColor + 3, dst[3]);
	}
	
	_mm_store_si128((__m128i *)compInfo.target.lineLayerID, dstLayerID_vec128);
}

#endif

//this is fantastically inaccurate.
//we do the early return even though it reduces the resulting accuracy
//because we need the speed, and because it is inaccurate anyway
void GPUEngineBase::_MosaicSpriteLinePixel(GPUEngineCompositorInfo &compInfo, const size_t x, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const bool enableMosaic = (this->_oamList[this->_sprNum[x]].Mosaic != 0);
	if (!enableMosaic)
		return;
	
	const bool opaque = prioTab[x] <= 4;

	GPUEngineBase::MosaicColor::Obj objColor;
	objColor.color = LE_TO_LOCAL_16(dst[x]);
	objColor.alpha = dst_alpha[x];
	objColor.opaque = opaque;

	const size_t y = compInfo.line.indexNative;
	
	if (!compInfo.renderState.mosaicWidthOBJ[x].begin || !compInfo.renderState.mosaicHeightOBJ[y].begin)
	{
		objColor = this->_mosaicColors.obj[compInfo.renderState.mosaicWidthOBJ[x].trunc];
	}
	
	this->_mosaicColors.obj[x] = objColor;
	
	dst[x] = LE_TO_LOCAL_16(objColor.color);
	dst_alpha[x] = objColor.alpha;
	if (!objColor.opaque) prioTab[x] = 0x7F;
}

void GPUEngineBase::_MosaicSpriteLine(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (!compInfo.renderState.isOBJMosaicSet)
	{
		return;
	}
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		this->_MosaicSpriteLinePixel(compInfo, i, dst, dst_alpha, typeTab, prioTab);
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_Final(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	const u16 lineWidth = (COMPOSITORMODE == GPUCompositorMode_Debug) ? compInfo.renderState.selectedBGLayer->size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const s16 dx = (s16)LOCAL_TO_LE_16(param.BGnPA.value);
	const s16 dy = (s16)LOCAL_TO_LE_16(param.BGnPC.value);
	const s32 wh = compInfo.renderState.selectedBGLayer->size.width;
	const s32 ht = compInfo.renderState.selectedBGLayer->size.height;
	const s32 wmask = wh - 1;
	const s32 hmask = ht - 1;
	
	IOREG_BGnX x;
	IOREG_BGnY y;
	x.value = LOCAL_TO_LE_32(param.BGnX.value);
	y.value = LOCAL_TO_LE_32(param.BGnY.value);
	
#ifdef MSB_FIRST
	// This only seems to work in the unrotated/unscaled case.
	//
	// All the working values should be correct on big-endian, but there is something else wrong going
	// on somewhere else, but that remains a mystery at this time. In the meantime, this hack will have
	// to remain in order to fix a bunch of games that use affine or extended layers, just as long as
	// they don't perform any rotation/scaling.
	// - rogerman, 2017-10-17
	x.value = ((x.value & 0xFF000000) >> 16) | (x.value & 0x00FF00FF) | ((x.value & 0x0000FF00) << 16);
	y.value = ((y.value & 0xFF000000) >> 16) | (y.value & 0x00FF00FF) | ((y.value & 0x0000FF00) << 16);
#endif
	
	u8 index;
	u16 srcColor;
	
	// as an optimization, specially handle the fairly common case of
	// "unrotated + unscaled + no boundary checking required"
	if (dx == GPU_FRAMEBUFFER_NATIVE_WIDTH && dy == 0)
	{
		s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if ( WRAP || ((auxX >= 0) && (auxX + lineWidth <= wh) && (auxY >= 0) && (auxY < ht)) )
		{
			for (size_t i = 0; i < lineWidth; i++)
			{
				GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, srcColor);
				
				if (WILLDEFERCOMPOSITING)
				{
					this->_deferredIndexNative[i] = index;
					this->_deferredColorNative[i] = srcColor;
				}
				else
				{
					this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, i, srcColor, (index != 0));
				}
				
				auxX++;
				
				if (WRAP)
				{
					auxX &= wmask;
				}
			}
			
			return;
		}
	}
	
	for (size_t i = 0; i < lineWidth; i++, x.value+=dx, y.value+=dy)
	{
		const s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if (WRAP || ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht)))
		{
			GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, srcColor);
			
			if (WILLDEFERCOMPOSITING)
			{
				this->_deferredIndexNative[i] = index;
				this->_deferredColorNative[i] = srcColor;
			}
			else
			{
				this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, i, srcColor, (index != 0));
			}
		}
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_ApplyWrap(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	this->_RenderPixelIterate_Final<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, GetPixelFunc, WRAP>(compInfo, param, map, tile, pal);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc>
void GPUEngineBase::_RenderPixelIterate(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	if (compInfo.renderState.selectedBGLayer->isDisplayWrapped)
	{
		this->_RenderPixelIterate_ApplyWrap<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, GetPixelFunc, true>(compInfo, param, map, tile, pal);
	}
	else
	{
		this->_RenderPixelIterate_ApplyWrap<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, GetPixelFunc, false>(compInfo, param, map, tile, pal);
	}
}

TILEENTRY GPUEngineBase::_GetTileEntry(const u32 tileMapAddress, const u16 xOffset, const u16 layerWidthMask)
{
	TILEENTRY theTileEntry;
	
	const u16 tmp = (xOffset & layerWidthMask) >> 3;
	u32 mapinfo = tileMapAddress + (tmp & 0x1F) * 2;
	if (tmp > 31) mapinfo += 32*32*2;
	theTileEntry.val = LOCAL_TO_LE_16( *(u16 *)MMU_gpu_map(mapinfo) );
	
	return theTileEntry;
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_CompositePixelImmediate(GPUEngineCompositorInfo &compInfo, const size_t srcX, u16 srcColor16, bool opaque)
{
	if (MOSAIC)
	{
		//due to this early out, we will get incorrect behavior in cases where
		//we enable mosaic in the middle of a frame. this is deemed unlikely.
		
		if (compInfo.renderState.mosaicWidthBG[srcX].begin && compInfo.renderState.mosaicHeightBG[compInfo.line.indexNative].begin)
		{
			srcColor16 = (!opaque) ? 0xFFFF : (srcColor16 & 0x7FFF);
			this->_mosaicColors.bg[compInfo.renderState.selectedLayerID][srcX] = srcColor16;
		}
		else
		{
			srcColor16 = this->_mosaicColors.bg[compInfo.renderState.selectedLayerID][compInfo.renderState.mosaicWidthBG[srcX].trunc];
		}
		
		opaque = (srcColor16 != 0xFFFF);
	}
	
	if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][srcX] == 0) )
	{
		return;
	}
	
	if (!opaque)
	{
		return;
	}
	
	compInfo.target.xNative = srcX;
	compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHeadNative + srcX;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative + srcX;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHeadNative + srcX;
	
	const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
	this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG>(compInfo, srcColor16, enableColorEffect, 0, 0);
}

template <bool MOSAIC>
void GPUEngineBase::_PrecompositeNativeToCustomLineBG(GPUEngineCompositorInfo &compInfo)
{
	if (MOSAIC)
	{
#ifdef ENABLE_SSE2
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=8)
		{
			__m128i mosaicColor_vec128;
			u16 *mosaicColorBG = this->_mosaicColors.bg[compInfo.renderState.selectedLayerID];
			
			const __m128i index_vec128 = _mm_loadl_epi64((__m128i *)(this->_deferredIndexNative + x));
			const __m128i col_vec128 = _mm_load_si128((__m128i *)(this->_deferredColorNative + x));
			
			if (compInfo.renderState.mosaicHeightBG[compInfo.line.indexNative].begin)
			{
				const __m128i mosaicSetColorMask = _mm_cmpgt_epi16( _mm_and_si128(_mm_set1_epi16(0x00FF), _mm_loadu_si128((__m128i *)(compInfo.renderState.mosaicWidthBG + x))), _mm_setzero_si128() );
				const __m128i idxMask = _mm_cmpeq_epi16(_mm_unpacklo_epi8(index_vec128, _mm_setzero_si128()), _mm_setzero_si128());
				mosaicColor_vec128 = _mm_blendv_epi8(_mm_and_si128(col_vec128, _mm_set1_epi16(0x7FFF)), _mm_set1_epi16(0xFFFF), idxMask);
				
				_mm_storeu_si128( (__m128i *)(mosaicColorBG + x), _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(mosaicColorBG + x)), mosaicColor_vec128, mosaicSetColorMask) );
			}
			
			mosaicColor_vec128 = _mm_set_epi16(mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+7].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+6].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+5].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+4].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+3].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+2].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+1].trunc],
											   mosaicColorBG[compInfo.renderState.mosaicWidthBG[x+0].trunc]);
			
			const __m128i writeColorMask = _mm_cmpeq_epi16(mosaicColor_vec128, _mm_set1_epi16(0xFFFF));
			_mm_storel_epi64( (__m128i *)(this->_deferredIndexNative + x), _mm_andnot_si128(_mm_packs_epi16(writeColorMask, _mm_setzero_si128()), index_vec128) );
			_mm_store_si128( (__m128i *)(this->_deferredColorNative + x), _mm_blendv_epi8(mosaicColor_vec128, col_vec128, writeColorMask) );
		}
#else
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			u16 mosaicColor;
			
			if (compInfo.renderState.mosaicWidthBG[x].begin && compInfo.renderState.mosaicHeightBG[compInfo.line.indexNative].begin)
			{
				mosaicColor = (this->_deferredIndexNative[x] == 0) ? 0xFFFF : this->_deferredColorNative[x] & 0x7FFF;
				this->_mosaicColors.bg[compInfo.renderState.selectedLayerID][x] = mosaicColor;
			}
			else
			{
				mosaicColor = this->_mosaicColors.bg[compInfo.renderState.selectedLayerID][compInfo.renderState.mosaicWidthBG[x].trunc];
			}
			
			if (mosaicColor == 0xFFFF)
			{
				this->_deferredIndexNative[x] = 0;
			}
			else
			{
				this->_deferredColorNative[x] = mosaicColor;
			}
		}
#endif
	}
	
	CopyLineExpand<0xFFFF, false, false, 2>(this->_deferredColorCustom, this->_deferredColorNative, compInfo.line.widthCustom, 1);
	CopyLineExpand<0xFFFF, false, false, 1>(this->_deferredIndexCustom, this->_deferredIndexNative, compInfo.line.widthCustom, 1);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeNativeLineOBJ(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const FragmentColor *__restrict srcColorNative32)
{
	const bool isUsingSrc32 = (srcColorNative32 != NULL);
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
#ifdef ENABLE_SSE2
	const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[GPULayerID_OBJ];
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=16, srcColorNative16+=16, srcColorNative32+=16, compInfo.target.xNative+=16, compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
	{
		__m128i passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestNative[GPULayerID_OBJ] + i)), _mm_set1_epi8(1) );
			
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
		
		__m128i src[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			src[0] = _mm_load_si128((__m128i *)srcColorNative16 + 0);
			src[1] = _mm_load_si128((__m128i *)srcColorNative16 + 1);
		}
		else
		{
			if (isUsingSrc32)
			{
				src[0] = _mm_load_si128((__m128i *)srcColorNative32 + 0);
				src[1] = _mm_load_si128((__m128i *)srcColorNative32 + 1);
				src[2] = _mm_load_si128((__m128i *)srcColorNative32 + 2);
				src[3] = _mm_load_si128((__m128i *)srcColorNative32 + 3);
			}
			else
			{
				const __m128i src16[2] = {
					_mm_load_si128((__m128i *)srcColorNative16 + 0),
					_mm_load_si128((__m128i *)srcColorNative16 + 1)
				};
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
				{
					ColorspaceConvert555To6665Opaque_SSE2<false>(src16[0], src[0], src[1]);
					ColorspaceConvert555To6665Opaque_SSE2<false>(src16[1], src[2], src[3]);
				}
				else
				{
					ColorspaceConvert555To8888Opaque_SSE2<false>(src16[0], src[0], src[1]);
					ColorspaceConvert555To8888Opaque_SSE2<false>(src16[1], src[2], src[3]);
				}
			}
		}
		
		// Write out the pixels.
		this->_PixelComposite16_SSE2<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo,
																											didAllPixelsPass,
																											passMask8,
																											src[3], src[2], src[1], src[0],
																											srcEffectEnableMask,
																											this->_enableColorEffectNative[GPULayerID_OBJ] + i,
																											this->_sprAlpha[compInfo.line.indexNative] + i,
																											this->_sprType[compInfo.line.indexNative] + i);
	}
#else
	if (isUsingSrc32)
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++, srcColorNative32++, compInfo.target.xNative++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][i] == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][i] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, *srcColorNative32, enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][i], this->_sprType[compInfo.line.indexNative][i]);
		}
	}
	else
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++, srcColorNative16++, compInfo.target.xNative++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][i] == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][i] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, *srcColorNative16, enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][i], this->_sprType[compInfo.line.indexNative][i]);
		}
	}
#endif
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeLineDeferred(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorCustom16, const u8 *__restrict srcIndexCustom)
{
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % 16));
	const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[compInfo.renderState.selectedLayerID];
	
	for (; i < ssePixCount; i+=16, compInfo.target.xCustom+=16, compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		__m128i passMask8;
		int passMaskValue;
		bool didAllPixelsPass;
		
		if (WILLPERFORMWINDOWTEST || (LAYERTYPE == GPULayerType_BG))
		{
			if (WILLPERFORMWINDOWTEST)
			{
				// Do the window test.
				passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom)), _mm_set1_epi8(1) );
			}
			
			if (LAYERTYPE == GPULayerType_BG)
			{
				const __m128i tempPassMask = _mm_cmpeq_epi8(_mm_load_si128((__m128i *)(srcIndexCustom + compInfo.target.xCustom)), _mm_setzero_si128());
				
				// Do the index test. Pixels with an index value of 0 are rejected.
				if (WILLPERFORMWINDOWTEST)
				{
					passMask8 = _mm_andnot_si128(tempPassMask, passMask8);
				}
				else
				{
					passMask8 = _mm_xor_si128(tempPassMask, _mm_set1_epi32(0xFFFFFFFF));
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
		
		__m128i src[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			src[0] = _mm_load_si128((__m128i *)(srcColorCustom16 + compInfo.target.xCustom + 0));
			src[1] = _mm_load_si128((__m128i *)(srcColorCustom16 + compInfo.target.xCustom + 8));
		}
		else
		{
			const __m128i src16[2] = {
				_mm_load_si128((__m128i *)(srcColorCustom16 + compInfo.target.xCustom + 0)),
				_mm_load_si128((__m128i *)(srcColorCustom16 + compInfo.target.xCustom + 8))
			};
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
			{
				ColorspaceConvert555To6665Opaque_SSE2<false>(src16[0], src[0], src[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(src16[1], src[2], src[3]);
			}
			else
			{
				ColorspaceConvert555To8888Opaque_SSE2<false>(src16[0], src[0], src[1]);
				ColorspaceConvert555To8888Opaque_SSE2<false>(src16[1], src[2], src[3]);
			}
		}
		
		// Write out the pixels.
		this->_PixelComposite16_SSE2<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
																									 didAllPixelsPass,
																									 passMask8,
																									 src[3], src[2], src[1], src[0],
																									 srcEffectEnableMask,
																									 this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom,
																									 this->_sprAlphaCustom + compInfo.target.xCustom,
																									 this->_sprTypeCustom + compInfo.target.xCustom);
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < compInfo.line.pixelCount; i++, compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] == 0) )
		{
			continue;
		}
		
		if ( (LAYERTYPE == GPULayerType_BG) && (srcIndexCustom[compInfo.target.xCustom] == 0) )
		{
			continue;
		}
		
		const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] != 0) : true;
		this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE>(compInfo, srcColorCustom16[compInfo.target.xCustom], enableColorEffect, this->_sprAlphaCustom[compInfo.target.xCustom], this->_sprTypeCustom[compInfo.target.xCustom]);
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeVRAMLineDeferred(GPUEngineCompositorInfo &compInfo, const void *__restrict vramColorPtr)
{
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % 16));
	const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[compInfo.renderState.selectedLayerID];
	
	for (; i < ssePixCount; i+=16, compInfo.target.xCustom+=16, compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		__m128i passMask8;
		int passMaskValue;
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom)), _mm_set1_epi8(1) );
			
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
		
		__m128i src[4];
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
			{
				src[0] = _mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 0));
				src[1] = _mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 8));
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					__m128i tempPassMask = _mm_packus_epi16( _mm_srli_epi16(src[0], 15), _mm_srli_epi16(src[1], 15) );
					tempPassMask = _mm_cmpeq_epi8(tempPassMask, _mm_set1_epi8(1));
					
					passMask8 = _mm_and_si128(tempPassMask, passMask8);
					passMaskValue = _mm_movemask_epi8(passMask8);
				}
				break;
			}
				
			case NDSColorFormat_BGR666_Rev:
			{
				const __m128i src16[2] = {
					_mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 0)),
					_mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 8))
				};
				
				ColorspaceConvert555To6665Opaque_SSE2<false>(src16[0], src[0], src[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(src16[1], src[2], src[3]);
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					__m128i tempPassMask = _mm_packus_epi16( _mm_srli_epi16(src16[0], 15), _mm_srli_epi16(src16[1], 15) );
					tempPassMask = _mm_cmpeq_epi8(tempPassMask, _mm_set1_epi8(1));
					
					passMask8 = _mm_and_si128(tempPassMask, passMask8);
					passMaskValue = _mm_movemask_epi8(passMask8);
				}
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				src[0] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 0));
				src[1] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 4));
				src[2] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 8));
				src[3] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 12));
				
				if (LAYERTYPE != GPULayerType_OBJ)
				{
					__m128i tempPassMask = _mm_packus_epi16( _mm_packs_epi32(_mm_srli_epi32(src[0], 24), _mm_srli_epi32(src[1], 24)), _mm_packs_epi32(_mm_srli_epi32(src[2], 24), _mm_srli_epi32(src[3], 24)) );
					tempPassMask = _mm_cmpeq_epi8(tempPassMask, _mm_setzero_si128());
					
					passMask8 = _mm_andnot_si128(tempPassMask, passMask8);
					passMaskValue = _mm_movemask_epi8(passMask8);
				}
				break;
			}
		}
		
		// If none of the pixels within the vector pass, then reject them all at once.
		if (passMaskValue == 0)
		{
			continue;
		}
		
		// Write out the pixels.
		const bool didAllPixelsPass = (passMaskValue == 0xFFFF);
		this->_PixelComposite16_SSE2<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo,
																									 didAllPixelsPass,
																									 passMask8,
																									 src[3], src[2], src[1], src[0],
																									 srcEffectEnableMask,
																									 this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom,
																									 this->_sprAlphaCustom + compInfo.target.xCustom,
																									 this->_sprTypeCustom + compInfo.target.xCustom);
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < compInfo.line.pixelCount; i++, compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] == 0) )
		{
			continue;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			if ( (LAYERTYPE != GPULayerType_OBJ) && ((((u32 *)vramColorPtr)[i] & 0xFF000000) == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE>(compInfo, ((FragmentColor *)vramColorPtr)[i], enableColorEffect, this->_sprAlphaCustom[compInfo.target.xCustom], this->_sprTypeCustom[compInfo.target.xCustom]);
		}
		else
		{
			if ( (LAYERTYPE != GPULayerType_OBJ) && ((((u16 *)vramColorPtr)[i] & 0x8000) == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE>(compInfo, ((u16 *)vramColorPtr)[i], enableColorEffect, this->_sprAlphaCustom[compInfo.target.xCustom], this->_sprTypeCustom[compInfo.target.xCustom]);
		}
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_RenderLine_BGText(GPUEngineCompositorInfo &compInfo, const u16 XBG, const u16 YBG)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const u16 lineWidth = (COMPOSITORMODE == GPUCompositorMode_Debug) ? compInfo.renderState.selectedBGLayer->size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const u16 lg    = compInfo.renderState.selectedBGLayer->size.width;
	const u16 ht    = compInfo.renderState.selectedBGLayer->size.height;
	const u32 tile  = compInfo.renderState.selectedBGLayer->tileEntryAddress;
	const u16 wmask = lg - 1;
	const u16 hmask = ht - 1;
	
	const size_t pixCountLo = 8 - (XBG & 0x0007);
	size_t x = 0;
	size_t xoff = XBG;
	
	const u16 tmp = (YBG & hmask) >> 3;
	u32 map = compInfo.renderState.selectedBGLayer->tileMapAddress + (tmp & 31) * 64;
	if (tmp > 31)
		map += ADDRESS_STEP_512B << compInfo.renderState.selectedBGLayer->BGnCNT.ScreenSize;
	
	if (compInfo.renderState.selectedBGLayer->BGnCNT.PaletteMode == PaletteMode_16x16) // color: 16 palette entries
	{
		const u16 *__restrict pal = this->_paletteBG;
		const u16 yoff = (YBG & 0x0007) << 2;
		u8 index;
		u16 color;
		
		for (size_t xfin = pixCountLo; x < lineWidth; xfin = std::min<u16>(x+8, lineWidth))
		{
			const TILEENTRY tileEntry = this->_GetTileEntry(map, xoff, wmask);
			const u16 tilePalette = tileEntry.bits.Palette * 16;
			u8 *__restrict tileColorIdx = (u8 *)MMU_gpu_map(tile + (tileEntry.bits.TileNum * 0x20) + ((tileEntry.bits.VFlip) ? (7*4)-yoff : yoff));
			
			if (tileEntry.bits.HFlip)
			{
				tileColorIdx += 3 - ((xoff & 0x0007) >> 1);
				
				if (xoff & 1)
				{
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx & 0x0F;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx & 0x0F;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					tileColorIdx--;
				}
				
				for (; x < xfin; tileColorIdx--)
				{
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx >> 4;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx >> 4;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					
					if (x < xfin)
					{
						if (WILLDEFERCOMPOSITING)
						{
							this->_deferredIndexNative[x] = *tileColorIdx & 0x0F;
							this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
						}
						else
						{
							index = *tileColorIdx & 0x0F;
							color = LE_TO_LOCAL_16(pal[index + tilePalette]);
							this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
						}
						
						x++;
						xoff++;
					}
				}
			}
			else
			{
				tileColorIdx += ((xoff & 0x0007) >> 1);
				
				if (xoff & 1)
				{
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx >> 4;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx >> 4;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					tileColorIdx++;
				}
				
				for (; x < xfin; tileColorIdx++)
				{
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx & 0x0F;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx & 0x0F;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					
					if (x < xfin)
					{
						if (WILLDEFERCOMPOSITING)
						{
							this->_deferredIndexNative[x] = *tileColorIdx >> 4;
							this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
						}
						else
						{
							index = *tileColorIdx >> 4;
							color = LE_TO_LOCAL_16(pal[index + tilePalette]);
							this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
						}
						
						x++;
						xoff++;
					}
				}
			}
		}
	}
	else //256-color BG
	{
		const u16 *__restrict pal = (DISPCNT.ExBGxPalette_Enable) ? *(compInfo.renderState.selectedBGLayer->extPalette) : this->_paletteBG;
		const u32 extPalMask = -DISPCNT.ExBGxPalette_Enable;
		const u16 yoff = (YBG & 0x0007) << 3;
		size_t line_dir;
		
		for (size_t xfin = pixCountLo; x < lineWidth; xfin = std::min<u16>(x+8, lineWidth))
		{
			const TILEENTRY tileEntry = this->_GetTileEntry(map, xoff, wmask);
			const u16 *__restrict tilePal = (u16 *)((u8 *)pal + ((tileEntry.bits.Palette<<9) & extPalMask));
			const u8 *__restrict tileColorIdx = (u8 *)MMU_gpu_map(tile + (tileEntry.bits.TileNum * 0x40) + ((tileEntry.bits.VFlip) ? (7*8)-yoff : yoff));
			
			if (tileEntry.bits.HFlip)
			{
				tileColorIdx += (7 - (xoff & 0x0007));
				line_dir = -1;
			}
			else
			{
				tileColorIdx += (xoff & 0x0007);
				line_dir = 1;
			}
			
			for (; x < xfin; x++, xoff++, tileColorIdx += line_dir)
			{
				if (WILLDEFERCOMPOSITING)
				{
					this->_deferredIndexNative[x] = *tileColorIdx;
					this->_deferredColorNative[x] = LE_TO_LOCAL_16(tilePal[this->_deferredIndexNative[x]]);
				}
				else
				{
					const u8 index = *tileColorIdx;
					const u16 color = LE_TO_LOCAL_16(tilePal[index]);
					this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
				}
			}
		}
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_RenderLine_BGAffine(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param)
{
	this->_RenderPixelIterate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_tiled_8bit_entry>(compInfo, param, compInfo.renderState.selectedBGLayer->tileMapAddress, compInfo.renderState.selectedBGLayer->tileEntryAddress, this->_paletteBG);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_RenderLine_BGExtended(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, bool &outUseCustomVRAM)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	switch (compInfo.renderState.selectedBGLayer->type)
	{
		case BGType_AffineExt_256x16: // 16  bit bgmap entries
		{
			if (DISPCNT.ExBGxPalette_Enable)
			{
				this->_RenderPixelIterate< COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_tiled_16bit_entry<true> >(compInfo, param, compInfo.renderState.selectedBGLayer->tileMapAddress, compInfo.renderState.selectedBGLayer->tileEntryAddress, *(compInfo.renderState.selectedBGLayer->extPalette));
			}
			else
			{
				this->_RenderPixelIterate< COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_tiled_16bit_entry<false> >(compInfo, param, compInfo.renderState.selectedBGLayer->tileMapAddress, compInfo.renderState.selectedBGLayer->tileEntryAddress, this->_paletteBG);
			}
			break;
		}
			
		case BGType_AffineExt_256x1: // 256 colors
			this->_RenderPixelIterate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_256_map>(compInfo, param, compInfo.renderState.selectedBGLayer->BMPAddress, 0, this->_paletteBG);
			break;
			
		case BGType_AffineExt_Direct: // direct colors / BMP
		{
			outUseCustomVRAM = false;
			
			if (!MOSAIC)
			{
				const bool isRotationScaled = ( (param.BGnPA.value != 0x100) ||
				                                (param.BGnPC.value !=     0) ||
				                                (param.BGnX.value  !=     0) ||
				                                (param.BGnY.value  != (0x100 * compInfo.line.indexNative)) );
				if (!isRotationScaled)
				{
					const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(compInfo.renderState.selectedBGLayer->BMPAddress) - MMU.ARM9_LCD) / sizeof(u16);
					
					if (vramPixel < (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
					{
						const size_t blockID   = vramPixel >> 16;
						const size_t blockLine = (vramPixel >> 8) & 0x000000FF;
						
						GPU->GetEngineMain()->VerifyVRAMLineDidChange(blockID, compInfo.line.indexNative + blockLine);
						outUseCustomVRAM = !GPU->GetEngineMain()->IsLineCaptureNative(blockID, compInfo.line.indexNative + blockLine);
					}
				}
			}
			
			if (!outUseCustomVRAM)
			{
				this->_RenderPixelIterate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_BMP_map>(compInfo, param, compInfo.renderState.selectedBGLayer->BMPAddress, 0, this->_paletteBG);
			}
			else
			{
				if ((OUTPUTFORMAT != NDSColorFormat_BGR888_Rev) || GPU->GetDisplayInfo().isCustomSizeRequested)
				{
					this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
				}
			}
			break;
		}
			
		case BGType_Large8bpp: // large screen 256 colors
			this->_RenderPixelIterate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_256_map>(compInfo, param, compInfo.renderState.selectedBGLayer->largeBMPAddress, 0, this->_paletteBG);
			break;
			
		default:
			break;
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_LineText(GPUEngineCompositorInfo &compInfo)
{
	if (COMPOSITORMODE == GPUCompositorMode_Debug)
	{
		this->_RenderLine_BGText<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, 0, compInfo.line.indexNative);
	}
	else
	{
		this->_RenderLine_BGText<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, compInfo.renderState.selectedBGLayer->xOffset, compInfo.line.indexNative + compInfo.renderState.selectedBGLayer->yOffset);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_LineRot(GPUEngineCompositorInfo &compInfo)
{
	if (COMPOSITORMODE == GPUCompositorMode_Debug)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, (s32)compInfo.line.blockOffsetNative};
		this->_RenderLine_BGAffine<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, debugParams);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (compInfo.renderState.selectedLayerID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGAffine<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, *bgParams);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_LineExtRot(GPUEngineCompositorInfo &compInfo, bool &outUseCustomVRAM)
{
	if (COMPOSITORMODE == GPUCompositorMode_Debug)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, (s32)compInfo.line.blockOffsetNative};
		this->_RenderLine_BGExtended<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, debugParams, outUseCustomVRAM);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (compInfo.renderState.selectedLayerID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGExtended<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, *bgParams, outUseCustomVRAM);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

/*****************************************************************************/
//			SPRITE RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

template <bool ISDEBUGRENDER, bool ISOBJMODEBITMAP>
FORCEINLINE void GPUEngineBase::_RenderSpriteUpdatePixel(GPUEngineCompositorInfo &compInfo, size_t frameX,
														 const u16 *__restrict srcPalette, const u8 palIndex, const OBJMode objMode, const u8 prio, const u8 spriteNum,
														 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if ( (ISOBJMODEBITMAP && ((*srcPalette & 0x8000) == 0)) || (!ISOBJMODEBITMAP && (palIndex == 0)) )
	{
		return;
	}
	
	if (ISDEBUGRENDER)
	{
		dst[frameX] = (ISOBJMODEBITMAP) ? *srcPalette : LE_TO_LOCAL_16(srcPalette[palIndex]);
		return;
	}
	
	if ( !ISOBJMODEBITMAP && (objMode == OBJMode_Window) )
	{
		this->_sprWin[compInfo.line.indexNative][frameX] = 1;
		return;
	}
	
	if (prio < prioTab[frameX])
	{
		dst[frameX]           = (ISOBJMODEBITMAP) ? *srcPalette : LE_TO_LOCAL_16(srcPalette[palIndex]);
		dst_alpha[frameX]     = (ISOBJMODEBITMAP) ? palIndex : 0xFF;
		typeTab[frameX]       = (ISOBJMODEBITMAP) ? OBJMode_Bitmap : objMode;
		prioTab[frameX]       = prio;
		this->_sprNum[frameX] = spriteNum;
	}
}

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
template <bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSpriteBMP(GPUEngineCompositorInfo &compInfo,
									 const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									 const u8 spriteAlpha, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const u16 *__restrict vramBuffer = (u16 *)MMU_gpu_map(objAddress);
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	if (readXStep == 1)
	{
		if (ISDEBUGRENDER)
		{
			const size_t ssePixCount = length - (length % 8);
			for (; i < ssePixCount; i += 8, spriteX += 8, frameX += 8)
			{
				const __m128i color_vec128 = _mm_loadu_si128((__m128i *)(vramBuffer + spriteX));
				const __m128i alphaCompare = _mm_cmpeq_epi16( _mm_srli_epi16(color_vec128, 15), _mm_set1_epi16(0x0001) );
				_mm_storeu_si128( (__m128i *)(dst + frameX), _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst + frameX)), color_vec128, alphaCompare) );
			}
		}
		else
		{
			const __m128i prio_vec128 = _mm_set1_epi8(prio);
			
			const size_t ssePixCount = length - (length % 16);
			for (; i < ssePixCount; i += 16, spriteX += 16, frameX += 16)
			{
				const __m128i prioTab_vec128 = _mm_loadu_si128((__m128i *)(prioTab + frameX));
				const __m128i colorLo_vec128 = _mm_loadu_si128((__m128i *)(vramBuffer + spriteX));
				const __m128i colorHi_vec128 = _mm_loadu_si128((__m128i *)(vramBuffer + spriteX + 8));
				
				const __m128i prioCompare = _mm_cmplt_epi8(prio_vec128, prioTab_vec128);
				const __m128i alphaCompare = _mm_cmpeq_epi8( _mm_packs_epi16(_mm_srli_epi16(colorLo_vec128, 15), _mm_srli_epi16(colorHi_vec128, 15)), _mm_set1_epi8(0x01) );
				
				const __m128i combinedPackedCompare = _mm_and_si128(prioCompare, alphaCompare);
				const __m128i combinedLoCompare = _mm_unpacklo_epi8(combinedPackedCompare, combinedPackedCompare);
				const __m128i combinedHiCompare = _mm_unpackhi_epi8(combinedPackedCompare, combinedPackedCompare);
				
				// Just in case you're wondering why we're not using maskmovdqu, but instead using movdqu+pblendvb+movdqu, it's because
				// maskmovdqu won't keep the data in cache, and we really need the data in cache since we're about to render the sprite
				// to the framebuffer. In addition, the maskmovdqu instruction can be brutally slow on many non-Intel CPUs.
				_mm_storeu_si128( (__m128i *)(dst + frameX + 0),       _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst + frameX + 0)), colorLo_vec128, combinedLoCompare) );
				_mm_storeu_si128( (__m128i *)(dst + frameX + 8),       _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst + frameX + 8)), colorHi_vec128, combinedHiCompare) );
				_mm_storeu_si128( (__m128i *)(dst_alpha + frameX),     _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst_alpha + frameX)), _mm_set1_epi8(spriteAlpha + 1), combinedPackedCompare) );
				_mm_storeu_si128( (__m128i *)(typeTab + frameX),       _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(typeTab + frameX)), _mm_set1_epi8(OBJMode_Bitmap), combinedPackedCompare) );
				_mm_storeu_si128( (__m128i *)(prioTab + frameX),       _mm_blendv_epi8(prioTab_vec128, prio_vec128, combinedPackedCompare) );
				_mm_storeu_si128( (__m128i *)(this->_sprNum + frameX), _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(this->_sprNum + frameX)), _mm_set1_epi8(spriteNum), combinedPackedCompare) );
			}
		}
	}
#endif
	
	for (; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u16 vramColor = LE_TO_LOCAL_16(vramBuffer[spriteX]);
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, true>(compInfo, frameX, &vramColor, spriteAlpha+1, OBJMode_Bitmap, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite256(GPUEngineCompositorInfo &compInfo,
									 const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									 const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	for (size_t i = 0; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u32 palIndexAddress = objAddress + (u32)( (spriteX & 0x0007) + ((spriteX & 0xFFF8) << 3) );
		const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(palIndexAddress);
		const u8 idx8 = *palIndexBuffer;
		
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx8, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite16(GPUEngineCompositorInfo &compInfo,
									const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	for (size_t i = 0; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u32 spriteX_word = spriteX >> 1;
		const u32 palIndexAddress = objAddress + (spriteX_word & 0x0003) + ((spriteX_word & 0xFFFC) << 3);
		const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(palIndexAddress);
		const u8 palIndex = *palIndexBuffer;
		const u8 idx4 = (spriteX & 1) ? palIndex >> 4 : palIndex & 0x0F;
		
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx4, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

// return val means if the sprite is to be drawn or not
bool GPUEngineBase::_ComputeSpriteVars(GPUEngineCompositorInfo &compInfo, const OAMAttributes &spriteInfo, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir)
{
	x = 0;
	// get sprite location and size
	sprX = spriteInfo.X;
	sprY = spriteInfo.Y;
	sprSize = GPUEngineBase::_sprSizeTab[spriteInfo.Size][spriteInfo.Shape];
	lg = sprSize.width;
	
// FIXME: for rot/scale, a list of entries into the sprite should be maintained,
// that tells us where the first pixel of a screenline starts in the sprite,
// and how a step to the right in a screenline translates within the sprite

	//this wasn't really tested by anything. very unlikely to get triggered
	y = (compInfo.line.indexNative - sprY) & 0xFF;                        /* get the y line within sprite coords */
	if (y >= sprSize.height)
		return false;

	if ((sprX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || ((sprX+sprSize.width) <= 0))	/* sprite pixels outside of line */
		return false;				/* not to be drawn */

	// sprite portion out of the screen (LEFT)
	if (sprX < 0)
	{
		lg += sprX;	
		x = -(sprX);
		sprX = 0;
	}
	// sprite portion out of the screen (RIGHT)
	if ((sprX+sprSize.width) >= GPU_FRAMEBUFFER_NATIVE_WIDTH)
		lg = GPU_FRAMEBUFFER_NATIVE_WIDTH - sprX;

	// switch TOP<-->BOTTOM
	if (spriteInfo.VFlip)
		y = sprSize.height - y - 1;
	
	// switch LEFT<-->RIGHT
	if (spriteInfo.HFlip)
	{
		x = sprSize.width - x - 1;
		xdir = -1;
	}
	else
	{
		xdir = 1;
	}
	
	return true;
}

/*****************************************************************************/
//			SPRITE RENDERING
/*****************************************************************************/


//TODO - refactor this so there isnt as much duped code between rotozoomed and non-rotozoomed versions

u32 GPUEngineBase::_SpriteAddressBMP(GPUEngineCompositorInfo &compInfo, const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	if (DISPCNT.OBJ_BMP_mapping)
	{
		//tested by buffy sacrifice damage blood splatters in corner
		return this->_sprMem + (spriteInfo.TileIndex << compInfo.renderState.spriteBMPBoundary) + (y * sprSize.width * 2);
	}
	else
	{
		//2d mapping:
		//verified in rotozoomed mode by knights in the nightmare intro

		if (DISPCNT.OBJ_BMP_2D_dim)
			//256*256, verified by heroes of mana FMV intro
			return this->_sprMem + (((spriteInfo.TileIndex&0x3E0) * 64 + (spriteInfo.TileIndex&0x1F) * 8 + (y << 8)) << 1);
		else 
			//128*512, verified by harry potter and the order of the phoenix conversation portraits
			return this->_sprMem + (((spriteInfo.TileIndex&0x3F0) * 64 + (spriteInfo.TileIndex&0x0F) * 8 + (y << 7)) << 1);
	}
}

template <bool ISDEBUGRENDER>
void GPUEngineBase::_SpriteRender(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (compInfo.renderState.spriteRenderMode == SpriteRenderMode_Sprite1D)
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite1D, ISDEBUGRENDER>(compInfo, dst, dst_alpha, typeTab, prioTab);
	else
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite2D, ISDEBUGRENDER>(compInfo, dst, dst_alpha, typeTab, prioTab);
}

void GPUEngineBase::SpriteRenderDebug(const u16 lineIndex, u16 *dst)
{
	GPUEngineCompositorInfo compInfo;
	memset(&compInfo, 0, sizeof(compInfo));
	
	compInfo.renderState.displayOutputMode = GPUDisplayMode_Normal;
	compInfo.renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	compInfo.renderState.selectedLayerID = GPULayerID_OBJ;
	compInfo.renderState.colorEffect = ColorEffect_Disable;
	compInfo.renderState.masterBrightnessMode = GPUMasterBrightMode_Disable;
	compInfo.renderState.masterBrightnessIsFullIntensity = false;
	compInfo.renderState.masterBrightnessIsMaxOrMin = true;
	compInfo.renderState.spriteRenderMode = this->_currentRenderState.spriteRenderMode;
	compInfo.renderState.spriteBoundary = this->_currentRenderState.spriteBoundary;
	compInfo.renderState.spriteBMPBoundary = this->_currentRenderState.spriteBMPBoundary;
	
	compInfo.line.indexNative = lineIndex;
	compInfo.line.indexCustom = compInfo.line.indexNative;
	compInfo.line.widthCustom = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compInfo.line.renderCount = 1;
	compInfo.line.pixelCount = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compInfo.line.blockOffsetNative = compInfo.line.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compInfo.line.blockOffsetCustom = compInfo.line.blockOffsetNative;
	
	compInfo.target.lineColorHead = dst;
	compInfo.target.lineColorHeadNative = compInfo.target.lineColorHead;
	compInfo.target.lineColorHeadCustom = compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerIDHead = NULL;
	compInfo.target.lineLayerIDHeadNative = NULL;
	compInfo.target.lineLayerIDHeadCustom = NULL;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor = (void **)&compInfo.target.lineColor16;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = NULL;
	
	this->_SpriteRender<true>(compInfo, dst, NULL, NULL, NULL);
}

template <SpriteRenderMode MODE, bool ISDEBUGRENDER>
void GPUEngineBase::_SpriteRenderPerform(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	size_t cost = 0;
	
	for (size_t spriteNum = 0; spriteNum < 128; spriteNum++)
	{
		OAMAttributes spriteInfo = this->_oamList[spriteNum];
		
		//for each sprite:
		if (cost >= 2130)
		{
			//out of sprite rendering time
			//printf("sprite overflow!\n");
			//return;
		}
		
		//do we incur a cost if a sprite is disabled?? we guess so.
		cost += 2;
		
		// Check if sprite is disabled before everything
		if (spriteInfo.RotScale == 0 && spriteInfo.Disable != 0)
			continue;
		
		// Must explicitly convert endianness with attributes 1 and 2.
		spriteInfo.attr[1] = LOCAL_TO_LE_16(spriteInfo.attr[1]);
		spriteInfo.attr[2] = LOCAL_TO_LE_16(spriteInfo.attr[2]);
		
		const OBJMode objMode = (OBJMode)spriteInfo.Mode;
		
		SpriteSize sprSize;
		s32 frameX;
		s32 frameY;
		s32 spriteX;
		s32 spriteY;
		s32 length;
		s32 readXStep;
		u8 prio = spriteInfo.Priority;
		
		if (spriteInfo.RotScale != 0)
		{
			s32		fieldX, fieldY, auxX, auxY, realX, realY;
			u8		blockparameter;
			s16		dx, dmx, dy, dmy;
			
			// Get sprite positions and size
			frameX = spriteInfo.X;
			frameY = spriteInfo.Y;
			sprSize = GPUEngineBase::_sprSizeTab[spriteInfo.Size][spriteInfo.Shape];
			
			// Copy sprite size, to check change it if needed
			fieldX = sprSize.width;
			fieldY = sprSize.height;
			length = sprSize.width;
			
			// If we are using double size mode, double our control vars
			if (spriteInfo.DoubleSize != 0)
			{
				fieldX <<= 1;
				fieldY <<= 1;
				length <<= 1;
			}
			
			//check if the sprite is visible y-wise. unfortunately our logic for x and y is different due to our scanline based rendering
			//tested thoroughly by many large sprites in Super Robot Wars K which wrap around the screen
			spriteY = (compInfo.line.indexNative - frameY) & 0xFF;
			if (spriteY >= fieldY)
				continue;
			
			//check if sprite is visible x-wise.
			if ((frameX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || (frameX + fieldX <= 0))
				continue;
			
			cost += (sprSize.width * 2) + 10;
			
			// Get which four parameter block is assigned to this sprite
			blockparameter = (spriteInfo.RotScaleIndex + (spriteInfo.HFlip << 3) + (spriteInfo.VFlip << 4)) * 4;
			
			// Get rotation/scale parameters
			dx  = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+0].attr3);
			dmx = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+1].attr3);
			dy  = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+2].attr3);
			dmy = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+3].attr3);
			
			// Calculate fixed point 8.8 start offsets
			realX = (sprSize.width  << 7) - (fieldX >> 1)*dx - (fieldY >> 1)*dmx + spriteY*dmx;
			realY = (sprSize.height << 7) - (fieldX >> 1)*dy - (fieldY >> 1)*dmy + spriteY*dmy;
			
			if (frameX < 0)
			{
				// If sprite is not in the window
				if (frameX + fieldX <= 0)
					continue;

				// Otherwise, is partially visible
				length += frameX;
				realX -= frameX*dx;
				realY -= frameX*dy;
				frameX = 0;
			}
			else
			{
				if (frameX + fieldX > GPU_FRAMEBUFFER_NATIVE_WIDTH)
					length = GPU_FRAMEBUFFER_NATIVE_WIDTH - frameX;
			}
			
			if (objMode == OBJMode_Bitmap) // Rotozoomed direct color
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;
				
				const u32 objAddress = this->_SpriteAddressBMP(compInfo, spriteInfo, sprSize, 0);
				
				for (size_t j = 0; j < length; ++j, ++frameX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;
					
					//this is all very slow, and so much dup code with other rotozoomed modes.
					//dont bother fixing speed until this whole thing gets reworked
					
					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						size_t objOffset = 0;
						
						if (DISPCNT.OBJ_BMP_2D_dim)
						{
							//tested by knights in the nightmare
							objOffset = ((this->_SpriteAddressBMP(compInfo, spriteInfo, sprSize, auxY) - objAddress) / 2) + auxX;
						}
						else
						{
							//tested by lego indiana jones (somehow?)
							//tested by buffy sacrifice damage blood splatters in corner
							objOffset = (auxY * sprSize.width) + auxX;
						}
						
						const u32 vramAddress = objAddress + (objOffset << 1);
						const u16 *vramBuffer = (u16 *)MMU_gpu_map(vramAddress);
						const u16 vramColor = LE_TO_LOCAL_16(*vramBuffer);
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, true>(compInfo, frameX, &vramColor, spriteInfo.PaletteIndex, OBJMode_Bitmap, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
					}
					
					// Add the rotation/scale coefficients, here the rotation/scaling is performed
					realX += dx;
					realY += dy;
				}
			}
			else if (spriteInfo.PaletteMode == PaletteMode_1x256) // If we are using 1 palette of 256 colours
			{
				const u32 objAddress = this->_sprMem + (spriteInfo.TileIndex << compInfo.renderState.spriteBoundary);
				const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(objAddress);
				const u16 *__restrict palColorBuffer = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;
				
				for (size_t j = 0; j < length; ++j, ++frameX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX >> 8);
					auxY = (realY >> 8);
					
					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						const size_t palOffset = (MODE == SpriteRenderMode_Sprite2D) ? (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*8) :
						                                                               (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)*sprSize.width*8) + ((auxY&0x7)*8);
						const u8 idx8 = palIndexBuffer[palOffset];
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx8, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
					}
					
					// Add the rotation/scale coefficients, here the rotation/scaling is performed
					realX += dx;
					realY += dy;
				}
			}
			else // Rotozoomed 16/16 palette
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + (spriteInfo.TileIndex << 5) : this->_sprMem + (spriteInfo.TileIndex << compInfo.renderState.spriteBoundary);
				const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(objAddress);
				const u16 *__restrict palColorBuffer = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);
				
				for (size_t j = 0; j < length; ++j, ++frameX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						const size_t palOffset = (MODE == SpriteRenderMode_Sprite2D) ? ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*4) :
						                                                               ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)*sprSize.width*4) + ((auxY&0x7)*4);
						const u8 palIndex = palIndexBuffer[palOffset];
						const u8 idx4 = (auxX & 1) ? palIndex >> 4 : palIndex & 0x0F;
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx4, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
					}
					
					// Add the rotation/scale coeficients, here the rotation/scaling  is performed
					realX += dx;
					realY += dy;
				}
			}
		}
		else //NOT rotozoomed
		{
			if (!this->_ComputeSpriteVars(compInfo, spriteInfo, sprSize, frameX, frameY, spriteX, spriteY, length, readXStep))
				continue;
			
			cost += sprSize.width;
			
			if (objMode == OBJMode_Bitmap) //sprite is in BMP format
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;
				
				const u32 objAddress = this->_SpriteAddressBMP(compInfo, spriteInfo, sprSize, spriteY);
				
				this->_RenderSpriteBMP<ISDEBUGRENDER>(compInfo,
													  objAddress, length, frameX, spriteX, readXStep,
													  spriteInfo.PaletteIndex, OBJMode_Bitmap, prio, spriteNum,
													  dst, dst_alpha, typeTab, prioTab);
				
				// When rendering at a custom framebuffer size, save a copy of the OBJ address as a reference
				// for reading the custom VRAM.
				const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(objAddress) - MMU.ARM9_LCD) / sizeof(u16);
				if (vramPixel < (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
				{
					const size_t blockID   = vramPixel >> 16;
					const size_t blockLine = (vramPixel >> 8) & 0x000000FF;
					const size_t linePixel = vramPixel & 0x000000FF;
					
					if (!GPU->GetEngineMain()->IsLineCaptureNative(blockID, blockLine) && (linePixel == 0))
					{
						this->vramBlockOBJAddress = objAddress;
					}
				}
			}
			else if (spriteInfo.PaletteMode == PaletteMode_1x256) //256 colors; handles OBJ windows too
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((spriteY>>3)<<10) + ((spriteY&0x7)*8) :
				                                                             this->_sprMem + (spriteInfo.TileIndex<<compInfo.renderState.spriteBoundary) + ((spriteY>>3)*sprSize.width*8) + ((spriteY&0x7)*8);
				
				const u16 *__restrict palColorBuffer = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;
				
				this->_RenderSprite256<ISDEBUGRENDER>(compInfo,
													  objAddress, length, frameX, spriteX, readXStep,
													  palColorBuffer, objMode, prio, spriteNum,
													  dst, dst_alpha, typeTab, prioTab);
			}
			else // 16 colors; handles OBJ windows too
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((spriteY>>3)<<10) + ((spriteY&0x7)*4) :
				                                                             this->_sprMem + (spriteInfo.TileIndex<<compInfo.renderState.spriteBoundary) + ((spriteY>>3)*sprSize.width*4) + ((spriteY&0x7)*4);
				
				const u16 *__restrict palColorBuffer = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);
				
				this->_RenderSprite16<ISDEBUGRENDER>(compInfo,
													 objAddress, length, frameX, spriteX, readXStep,
													 palColorBuffer, objMode, prio, spriteNum,
													 dst, dst_alpha, typeTab, prioTab);
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_Layers(GPUEngineCompositorInfo &compInfo)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	itemsForPriority_t *item;
	
	// Optimization: For normal display mode, render straight to the output buffer when that is what we are going to end
	// up displaying anyway. Otherwise, we need to use the working buffer.
	compInfo.target.lineColorHeadNative = (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->_nativeBuffer + (compInfo.line.blockOffsetNative * dispInfo.pixelBytes) : (u8 *)this->_internalRenderLineTargetNative;
	compInfo.target.lineColorHeadCustom = (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->_customBuffer + (compInfo.line.blockOffsetCustom * dispInfo.pixelBytes) : (u8 *)this->_internalRenderLineTargetCustom + (compInfo.line.blockOffsetCustom * dispInfo.pixelBytes);
	compInfo.target.lineColorHead = compInfo.target.lineColorHeadNative;
	
	compInfo.target.lineLayerIDHeadNative = this->_renderLineLayerIDNative[compInfo.line.indexNative];
	compInfo.target.lineLayerIDHeadCustom = this->_renderLineLayerIDCustom + (compInfo.line.blockOffsetCustom * sizeof(u8));
	compInfo.target.lineLayerIDHead = compInfo.target.lineLayerIDHeadNative;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	compInfo.renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	
	this->_RenderLine_Clear<OUTPUTFORMAT>(compInfo);
	
	// for all the pixels in the line
	if (this->_isBGLayerShown[GPULayerID_OBJ])
	{
		this->vramBlockOBJAddress = 0;
		this->_RenderLine_SetupSprites(compInfo);
	}
	
	if (WILLPERFORMWINDOWTEST)
	{
		this->_PerformWindowTesting(compInfo);
	}
	
	// paint lower priorities first
	// then higher priorities on top
	for (size_t prio = NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(this->_itemsForPriority[prio]);
		// render BGs
		if (this->_isAnyBGLayerShown)
		{
			for (size_t i = 0; i < item->nbBGs; i++)
			{
				const GPULayerID layerID = (GPULayerID)item->BGs[i];
				
				if (this->_isBGLayerShown[layerID])
				{
					compInfo.renderState.selectedLayerID = layerID;
					compInfo.renderState.selectedBGLayer = &this->_BGLayer[layerID];
					
					if (this->_engineID == GPUEngineID_Main)
					{
						if ( (layerID == GPULayerID_BG0) && GPU->GetEngineMain()->WillRender3DLayer() )
						{
#ifndef DISABLE_COMPOSITOR_FAST_PATHS
							if ( !compInfo.renderState.dstAnyBlendEnable && (  (compInfo.renderState.colorEffect == ColorEffect_Disable) ||
																			   !compInfo.renderState.srcEffectEnable[GPULayerID_BG0] ||
																			 (((compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) || (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)) && (compInfo.renderState.blendEVY == 0)) ) )
							{
								GPU->GetEngineMain()->RenderLine_Layer3D<GPUCompositorMode_Copy, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_BG0] && (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) )
							{
								GPU->GetEngineMain()->RenderLine_Layer3D<GPUCompositorMode_BrightUp, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_BG0] && (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness) )
							{
								GPU->GetEngineMain()->RenderLine_Layer3D<GPUCompositorMode_BrightDown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							else
#endif
							{
								GPU->GetEngineMain()->RenderLine_Layer3D<GPUCompositorMode_Unknown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							continue;
						}
					}
										
#ifndef DISABLE_COMPOSITOR_FAST_PATHS
					if ( (compInfo.renderState.colorEffect == ColorEffect_Disable) ||
						 !compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID] ||
						((compInfo.renderState.colorEffect == ColorEffect_Blend) && !compInfo.renderState.dstAnyBlendEnable) ||
						(((compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) || (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)) && (compInfo.renderState.blendEVY == 0)) )
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_Copy, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					else if ( !WILLPERFORMWINDOWTEST && compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID] && (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) )
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_BrightUp, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					else if ( !WILLPERFORMWINDOWTEST && compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID] && (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness) )
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_BrightDown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					else
#endif
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_Unknown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					
					compInfo.renderState.previouslyRenderedLayerID = layerID;
				} //layer enabled
			}
		}
		
		// render sprite Pixels
		if ( this->_isBGLayerShown[GPULayerID_OBJ] && (item->nbPixelsX > 0) )
		{
			compInfo.renderState.selectedLayerID = GPULayerID_OBJ;
			compInfo.renderState.selectedBGLayer = NULL;
			
#ifndef DISABLE_COMPOSITOR_FAST_PATHS
			if ( !compInfo.renderState.dstAnyBlendEnable && (  (compInfo.renderState.colorEffect == ColorEffect_Disable) ||
															   !compInfo.renderState.srcEffectEnable[GPULayerID_OBJ] ||
															 (((compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) || (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)) && (compInfo.renderState.blendEVY == 0)) ) )
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_Copy, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_OBJ] && (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) )
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_BrightUp, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_OBJ] && (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness) )
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_BrightDown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			else
#endif
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_Unknown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			
			compInfo.renderState.previouslyRenderedLayerID = GPULayerID_OBJ;
		}
	}
}

void GPUEngineBase::_RenderLine_SetupSprites(GPUEngineCompositorInfo &compInfo)
{
	itemsForPriority_t *item;
	
	this->_needExpandSprColorCustom = false;
	
	//n.b. - this is clearing the sprite line buffer to the background color,
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_sprColor, compInfo.renderState.backdropColor16);
	
	//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
	//how it interacts with this. I wish we knew why we needed this
	
	this->_SpriteRender<false>(compInfo, this->_sprColor, this->_sprAlpha[compInfo.line.indexNative], this->_sprType[compInfo.line.indexNative], this->_sprPrio[compInfo.line.indexNative]);
	this->_MosaicSpriteLine(compInfo, this->_sprColor, this->_sprAlpha[compInfo.line.indexNative], this->_sprType[compInfo.line.indexNative], this->_sprPrio[compInfo.line.indexNative]);
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		// assign them to the good priority item
		const size_t prio = this->_sprPrio[compInfo.line.indexNative][i];
		if (prio >= 4) continue;
		
		item = &(this->_itemsForPriority[prio]);
		item->PixelsX[item->nbPixelsX] = i;
		item->nbPixelsX++;
	}
	
	if (compInfo.line.widthCustom > GPU_FRAMEBUFFER_NATIVE_WIDTH)
	{
		bool isLineComplete = false;
		
		for (size_t i = 0; i < NB_PRIORITIES; i++)
		{
			item = &(this->_itemsForPriority[i]);
			
			if (item->nbPixelsX == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				isLineComplete = true;
				break;
			}
		}
		
		if (isLineComplete)
		{
			this->_needExpandSprColorCustom = true;
			CopyLineExpandHinted<0xFFFF, false, false, false, 1>(compInfo.line, this->_sprAlpha[compInfo.line.indexNative], this->_sprAlphaCustom);
			CopyLineExpandHinted<0xFFFF, false, false, false, 1>(compInfo.line, this->_sprType[compInfo.line.indexNative], this->_sprTypeCustom);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_LayerOBJ(GPUEngineCompositorInfo &compInfo, itemsForPriority_t *__restrict item)
{
	bool useCustomVRAM = false;
	
	if (this->vramBlockOBJAddress != 0)
	{
		const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(this->vramBlockOBJAddress) - MMU.ARM9_LCD) / sizeof(u16);
		
		if (vramPixel < (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
		{
			const size_t blockID   = vramPixel >> 16;
			const size_t blockLine = (vramPixel >> 8) & 0x000000FF;
			
			GPU->GetEngineMain()->VerifyVRAMLineDidChange(blockID, blockLine);
			useCustomVRAM = !GPU->GetEngineMain()->IsLineCaptureNative(blockID, blockLine);
		}
	}
	
	if (useCustomVRAM && ((OUTPUTFORMAT != NDSColorFormat_BGR888_Rev) || GPU->GetDisplayInfo().isCustomSizeRequested))
	{
		this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
	}
	
	if (item->nbPixelsX == GPU_FRAMEBUFFER_NATIVE_WIDTH)
	{
		if (this->isLineRenderNative[compInfo.line.indexNative])
		{
			if ((OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && useCustomVRAM)
			{
				const FragmentColor *__restrict vramColorPtr = (FragmentColor *)GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->vramBlockOBJAddress, 0);
				this->_CompositeNativeLineOBJ<COMPOSITORMODE, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, NULL, vramColorPtr);
			}
			else
			{
				this->_CompositeNativeLineOBJ<COMPOSITORMODE, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, this->_sprColor, NULL);
			}
		}
		else
		{
			if (useCustomVRAM)
			{
				const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->vramBlockOBJAddress, 0);
				this->_CompositeVRAMLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo, vramColorPtr);
			}
			else
			{
				// Lazily expand the sprite color line since there are also many instances where the OBJ layer will be
				// reading directly out of VRAM instead of reading out of the rendered sprite line.
				if (this->_needExpandSprColorCustom)
				{
					this->_needExpandSprColorCustom = false;
					CopyLineExpandHinted<0xFFFF, false, false, false, 2>(compInfo.line, this->_sprColor, this->_sprColorCustom);
				}
				
				this->_CompositeLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo, this->_sprColorCustom, NULL);
			}
		}
	}
	else
	{
		if (this->isLineRenderNative[compInfo.line.indexNative])
		{
			if (useCustomVRAM && (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev))
			{
				const FragmentColor *__restrict vramColorPtr = (FragmentColor *)GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->vramBlockOBJAddress, 0);
				
				for (size_t i = 0; i < item->nbPixelsX; i++)
				{
					const size_t srcX = item->PixelsX[i];
					
					if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
					{
						continue;
					}
					
					compInfo.target.xNative = srcX;
					compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
					compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead + srcX;
					compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead + srcX;
					compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead + srcX;
					
					const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
					this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, vramColorPtr[srcX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
				}
			}
			else
			{
				for (size_t i = 0; i < item->nbPixelsX; i++)
				{
					const size_t srcX = item->PixelsX[i];
					
					if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
					{
						continue;
					}
					
					compInfo.target.xNative = srcX;
					compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
					compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead + srcX;
					compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead + srcX;
					compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead + srcX;
					
					const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
					this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, this->_sprColor[srcX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
				}
			}
		}
		else
		{
			void *__restrict dstColorPtr = compInfo.target.lineColorHead;
			u8 *__restrict dstLayerIDPtr = compInfo.target.lineLayerIDHead;
			
			if (useCustomVRAM)
			{
				const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->vramBlockOBJAddress, 0);
				
				for (size_t line = 0; line < compInfo.line.renderCount; line++)
				{
					compInfo.target.lineColor16 = (u16 *)dstColorPtr;
					compInfo.target.lineColor32 = (FragmentColor *)dstColorPtr;
					compInfo.target.lineLayerID = dstLayerIDPtr;
					
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
						{
							continue;
						}
						
						compInfo.target.xNative = srcX;
						compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = compInfo.target.xCustom + p;
							
							compInfo.target.lineColor16 = (u16 *)dstColorPtr + dstX;
							compInfo.target.lineColor32 = (FragmentColor *)dstColorPtr + dstX;
							compInfo.target.lineLayerID = dstLayerIDPtr + dstX;
							
							const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
							
							if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
							{
								this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, ((FragmentColor *)vramColorPtr)[dstX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
							}
							else
							{
								this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, ((u16 *)vramColorPtr)[dstX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
							}
						}
					}
					
					vramColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)vramColorPtr + compInfo.line.widthCustom) : (void *)((u16 *)vramColorPtr + compInfo.line.widthCustom);
					dstColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void *)((u16 *)dstColorPtr + compInfo.line.widthCustom) : (void *)((FragmentColor *)dstColorPtr + compInfo.line.widthCustom);
					dstLayerIDPtr += compInfo.line.widthCustom;
				}
			}
			else
			{
				for (size_t line = 0; line < compInfo.line.renderCount; line++)
				{
					compInfo.target.lineColor16 = (u16 *)dstColorPtr;
					compInfo.target.lineColor32 = (FragmentColor *)dstColorPtr;
					compInfo.target.lineLayerID = dstLayerIDPtr;
					
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
						{
							continue;
						}
						
						compInfo.target.xNative = srcX;
						compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = compInfo.target.xCustom + p;
							
							compInfo.target.lineColor16 = (u16 *)dstColorPtr + dstX;
							compInfo.target.lineColor32 = (FragmentColor *)dstColorPtr + dstX;
							compInfo.target.lineLayerID = dstLayerIDPtr + dstX;
							
							const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
							this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, this->_sprColor[srcX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
						}
					}
					
					dstColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void *)((u16 *)dstColorPtr + compInfo.line.widthCustom) : (void *)((FragmentColor *)dstColorPtr + compInfo.line.widthCustom);
					dstLayerIDPtr += compInfo.line.widthCustom;
				}
			}
		}
	}
}

void GPUEngineBase::UpdateMasterBrightnessDisplayInfo(NDSDisplayInfo &mutableInfo)
{
	const GPUEngineCompositorInfo &compInfoZero = this->_currentCompositorInfo[0];
	bool needsApply = false;
	bool processPerScanline = false;
	
	for (size_t line = 0; line < GPU_FRAMEBUFFER_NATIVE_HEIGHT; line++)
	{
		const GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[line];
		
		if ( !needsApply &&
			 (compInfo.renderState.masterBrightnessIntensity != 0) &&
			((compInfo.renderState.masterBrightnessMode == GPUMasterBrightMode_Up) || (compInfo.renderState.masterBrightnessMode == GPUMasterBrightMode_Down)) )
		{
			needsApply = true;
		}
		
		mutableInfo.masterBrightnessMode[this->_targetDisplayID][line] = compInfo.renderState.masterBrightnessMode;
		mutableInfo.masterBrightnessIntensity[this->_targetDisplayID][line] = compInfo.renderState.masterBrightnessIntensity;
		
		if ( !processPerScanline &&
			((compInfo.renderState.masterBrightnessMode != compInfoZero.renderState.masterBrightnessMode) ||
			 (compInfo.renderState.masterBrightnessIntensity != compInfoZero.renderState.masterBrightnessIntensity)) )
		{
			processPerScanline = true;
		}
	}
	
	mutableInfo.masterBrightnessDiffersPerLine[this->_targetDisplayID] = processPerScanline;
	mutableInfo.needApplyMasterBrightness[this->_targetDisplayID] = needsApply;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::ApplyMasterBrightness(const NDSDisplayInfo &displayInfo)
{
	// Most games maintain the exact same master brightness values for all 192 lines, so we
	// can easily apply the master brightness to the entire framebuffer at once, which is
	// faster than applying it per scanline.
	//
	// However, some games need to have the master brightness values applied on a per-scanline
	// basis since they can differ for each scanline. For example, Mega Man Zero Collection
	// purposely changes the master brightness intensity to 31 on line 0, 0 on line 16, and
	// then back to 31 on line 176. Since the MMZC are originally GBA games, the master
	// brightness intensity changes are done to disable the unused scanlines on the NDS.
	
	if (displayInfo.masterBrightnessDiffersPerLine[this->_targetDisplayID])
	{
		for (size_t line = 0; line < GPU_FRAMEBUFFER_NATIVE_HEIGHT; line++)
		{
			const GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[line];
			void *dstColorLine = (!displayInfo.didPerformCustomRender[this->_targetDisplayID]) ? ((u8 *)displayInfo.nativeBuffer[this->_targetDisplayID] + (compInfo.line.blockOffsetNative * displayInfo.pixelBytes)) : ((u8 *)displayInfo.customBuffer[this->_targetDisplayID] + (compInfo.line.blockOffsetCustom * displayInfo.pixelBytes));
			const size_t pixCount = (!displayInfo.didPerformCustomRender[this->_targetDisplayID]) ? GPU_FRAMEBUFFER_NATIVE_WIDTH : compInfo.line.pixelCount;
			
			this->ApplyMasterBrightness<OUTPUTFORMAT, false>(dstColorLine,
															 pixCount,
															 (GPUMasterBrightMode)displayInfo.masterBrightnessMode[this->_targetDisplayID][line],
															 displayInfo.masterBrightnessIntensity[this->_targetDisplayID][line]);
		}
	}
	else
	{
		this->ApplyMasterBrightness<OUTPUTFORMAT, false>(displayInfo.renderedBuffer[this->_targetDisplayID],
														 displayInfo.renderedWidth[this->_targetDisplayID] * displayInfo.renderedHeight[this->_targetDisplayID],
														 (GPUMasterBrightMode)displayInfo.masterBrightnessMode[this->_targetDisplayID][0],
														 displayInfo.masterBrightnessIntensity[this->_targetDisplayID][0]);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISFULLINTENSITYHINT>
void GPUEngineBase::ApplyMasterBrightness(void *dst, const size_t pixCount, const GPUMasterBrightMode mode, const u8 intensity)
{
	if (!ISFULLINTENSITYHINT && (intensity == 0)) return;
	
	const bool isFullIntensity = (intensity >= 16);
	const u8 intensityClamped = (isFullIntensity) ? 16 : intensity;
	
	switch (mode)
	{
		case GPUMasterBrightMode_Disable:
			break;
			
		case GPUMasterBrightMode_Up:
		{
			if (!ISFULLINTENSITYHINT && !isFullIntensity)
			{
				size_t i = 0;
				
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensityClamped);
						
						const size_t ssePixCount = pixCount - (pixCount % 8);
						for (; i < ssePixCount; i += 8)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((u16 *)dst + i));
							dstColor_vec128 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi16(0x8000));
							_mm_store_si128((__m128i *)((u16 *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((u16 *)dst)[i] = GPUEngineBase::_brightnessUpTable555[intensityClamped][ ((u16 *)dst)[i] & 0x7FFF ] | 0x8000;
						}
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensityClamped);
						
						const size_t ssePixCount = pixCount - (pixCount % 4);
						for (; i < ssePixCount; i += 4)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((FragmentColor *)dst + i));
							dstColor_vec128 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000));
							_mm_store_si128((__m128i *)((FragmentColor *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((FragmentColor *)dst)[i] = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(((FragmentColor *)dst)[i], intensityClamped);
							((FragmentColor *)dst)[i].a = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F : 0xFF;
						}
						break;
					}
						
					default:
						break;
				}
			}
			else
			{
				// all white (optimization)
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memset_u16(dst, 0xFFFF, pixCount);
						break;
						
					case NDSColorFormat_BGR666_Rev:
						memset_u32(dst, 0x1F3F3F3F, pixCount);
						break;
						
					case NDSColorFormat_BGR888_Rev:
						memset_u32(dst, 0xFFFFFFFF, pixCount);
						break;
						
					default:
						break;
				}
			}
			break;
		}
			
		case GPUMasterBrightMode_Down:
		{
			if (!ISFULLINTENSITYHINT && !isFullIntensity)
			{
				size_t i = 0;
				
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensityClamped);
						
						const size_t ssePixCount = pixCount - (pixCount % 8);
						for (; i < ssePixCount; i += 8)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((u16 *)dst + i));
							dstColor_vec128 = this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi16(0x8000));
							_mm_store_si128((__m128i *)((u16 *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((u16 *)dst)[i] = GPUEngineBase::_brightnessDownTable555[intensityClamped][ ((u16 *)dst)[i] & 0x7FFF ] | 0x8000;
						}
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensityClamped);
						
						const size_t ssePixCount = pixCount - (pixCount % 4);
						for (; i < ssePixCount; i += 4)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((FragmentColor *)dst + i));
							dstColor_vec128 = this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000));
							_mm_store_si128((__m128i *)((FragmentColor *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((FragmentColor *)dst)[i] = this->_ColorEffectDecreaseBrightness(((FragmentColor *)dst)[i], intensityClamped);
							((FragmentColor *)dst)[i].a = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F : 0xFF;
						}
						break;
					}
						
					default:
						break;
				}
			}
			else
			{
				// all black (optimization)
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memset_u16(dst, 0x8000, pixCount);
						break;
						
					case NDSColorFormat_BGR666_Rev:
						memset_u32(dst, 0x1F000000, pixCount);
						break;
						
					case NDSColorFormat_BGR888_Rev:
						memset_u32(dst, 0xFF000000, pixCount);
						break;
						
					default:
						break;
				}
			}
			break;
		}
			
		case GPUMasterBrightMode_Reserved:
			break;
	}
}

template <size_t WIN_NUM>
bool GPUEngineBase::_IsWindowInsideVerticalRange(GPUEngineCompositorInfo &compInfo)
{
	const u16 windowTop    = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Top : this->_IORegisterMap->WIN1V.Top;
	const u16 windowBottom = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Bottom : this->_IORegisterMap->WIN1V.Bottom;
	
	if (WIN_NUM == 0 && !compInfo.renderState.WIN0_ENABLED) goto allout;
	if (WIN_NUM == 1 && !compInfo.renderState.WIN1_ENABLED) goto allout;

	if (windowTop > windowBottom)
	{
		if ((compInfo.line.indexNative < windowTop) && (compInfo.line.indexNative > windowBottom)) goto allout;
	}
	else
	{
		if ((compInfo.line.indexNative < windowTop) || (compInfo.line.indexNative >= windowBottom)) goto allout;
	}

	//the x windows will apply for this scanline
	return true;
	
allout:
	return false;
}

template <size_t WIN_NUM>
void GPUEngineBase::_UpdateWINH(GPUEngineCompositorInfo &compInfo)
{
	//dont even waste any time in here if the window isnt enabled
	if (WIN_NUM == 0 && !compInfo.renderState.WIN0_ENABLED) return;
	if (WIN_NUM == 1 && !compInfo.renderState.WIN1_ENABLED) return;

	this->_needUpdateWINH[WIN_NUM] = false;
	const size_t windowLeft  = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0H.Left  : this->_IORegisterMap->WIN1H.Left;
	const size_t windowRight = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0H.Right : this->_IORegisterMap->WIN1H.Right;

	//the original logic: if you doubt the window code, please check it against the newer implementation below
	//if(windowLeft > windowRight)
	//{
	//	if((x < windowLeft) && (x > windowRight)) return false;
	//}
	//else
	//{
	//	if((x < windowLeft) || (x >= windowRight)) return false;
	//}

	if (windowLeft > windowRight)
	{
		memset(this->_h_win[WIN_NUM], 1, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
		memset(this->_h_win[WIN_NUM] + windowRight + 1, 0, (windowLeft - (windowRight + 1)) * sizeof(u8));
	}
	else
	{
		memset(this->_h_win[WIN_NUM], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
		memset(this->_h_win[WIN_NUM] + windowLeft, 1, (windowRight - windowLeft) * sizeof(u8));
	}
}

void GPUEngineBase::_PerformWindowTesting(GPUEngineCompositorInfo &compInfo)
{
	if (this->_needUpdateWINH[0]) this->_UpdateWINH<0>(compInfo);
	if (this->_needUpdateWINH[1]) this->_UpdateWINH<1>(compInfo);
	
	for (size_t layerID = GPULayerID_BG0; layerID <= GPULayerID_OBJ; layerID++)
	{
		if (!this->_isBGLayerShown[layerID])
		{
			continue;
		}
		
#ifdef ENABLE_SSE2
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=16)
		{
			__m128i win_vec128;
			
			__m128i didPassWindowTest = _mm_setzero_si128();
			__m128i enableColorEffect = _mm_setzero_si128();
			
			__m128i win0HandledMask = _mm_setzero_si128();
			__m128i win1HandledMask = _mm_setzero_si128();
			__m128i winOBJHandledMask = _mm_setzero_si128();
			
			// Window 0 has the highest priority, so always check this first.
			if (compInfo.renderState.WIN0_ENABLED && this->_IsWindowInsideVerticalRange<0>(compInfo))
			{
				win_vec128 = _mm_load_si128((__m128i *)(this->_h_win[0] + i));
				win0HandledMask = _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1));
				
				didPassWindowTest = _mm_and_si128(win0HandledMask, compInfo.renderState.WIN0_enable_SSE2[layerID]);
				enableColorEffect = _mm_and_si128(win0HandledMask, compInfo.renderState.WIN0_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]);
			}
			
			// Window 1 has medium priority, and is checked after Window 0.
			if (compInfo.renderState.WIN1_ENABLED && this->_IsWindowInsideVerticalRange<1>(compInfo))
			{
				win_vec128 = _mm_load_si128((__m128i *)(this->_h_win[1] + i));
				win1HandledMask = _mm_andnot_si128(win0HandledMask, _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)));
				
				didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(win1HandledMask, compInfo.renderState.WIN1_enable_SSE2[layerID]) );
				enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(win1HandledMask, compInfo.renderState.WIN1_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]) );
			}
			
			// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
			if (compInfo.renderState.WINOBJ_ENABLED)
			{
				win_vec128 = _mm_load_si128((__m128i *)(this->_sprWin[compInfo.line.indexNative] + i));
				winOBJHandledMask = _mm_andnot_si128( _mm_or_si128(win0HandledMask, win1HandledMask), _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)) );
				
				didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOBJHandledMask, compInfo.renderState.WINOBJ_enable_SSE2[layerID]) );
				enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOBJHandledMask, compInfo.renderState.WINOBJ_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]) );
			}
			
			// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
			// This has the lowest priority, and is always checked last.
			const __m128i winOUTHandledMask = _mm_xor_si128( _mm_or_si128(win0HandledMask, _mm_or_si128(win1HandledMask, winOBJHandledMask)), _mm_set1_epi32(0xFFFFFFFF) );
			didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOUTHandledMask, compInfo.renderState.WINOUT_enable_SSE2[layerID]) );
			enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOUTHandledMask, compInfo.renderState.WINOUT_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]) );
			
			_mm_store_si128((__m128i *)(this->_didPassWindowTestNative[layerID] + i), _mm_and_si128(didPassWindowTest, _mm_set1_epi8(0x01)));
			_mm_store_si128((__m128i *)(this->_enableColorEffectNative[layerID] + i), _mm_and_si128(enableColorEffect, _mm_set1_epi8(0x01)));
		}
#else
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			// Window 0 has the highest priority, so always check this first.
			if (compInfo.renderState.WIN0_ENABLED && this->_IsWindowInsideVerticalRange<0>(compInfo))
			{
				if (this->_h_win[0][i] != 0)
				{
					this->_didPassWindowTestNative[layerID][i] = compInfo.renderState.WIN0_enable[layerID];
					this->_enableColorEffectNative[layerID][i] = compInfo.renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG];
					continue;
				}
			}
			
			// Window 1 has medium priority, and is checked after Window 0.
			if (compInfo.renderState.WIN1_ENABLED && this->_IsWindowInsideVerticalRange<1>(compInfo))
			{
				if (this->_h_win[1][i] != 0)
				{
					this->_didPassWindowTestNative[layerID][i] = compInfo.renderState.WIN1_enable[layerID];
					this->_enableColorEffectNative[layerID][i] = compInfo.renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG];
					continue;
				}
			}
			
			// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
			if (compInfo.renderState.WINOBJ_ENABLED)
			{
				if (this->_sprWin[compInfo.line.indexNative][i] != 0)
				{
					this->_didPassWindowTestNative[layerID][i] = compInfo.renderState.WINOBJ_enable[layerID];
					this->_enableColorEffectNative[layerID][i] = compInfo.renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG];
					continue;
				}
			}
			
			// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
			// This has the lowest priority, and is always checked last.
			this->_didPassWindowTestNative[layerID][i] = compInfo.renderState.WINOUT_enable[layerID];
			this->_enableColorEffectNative[layerID][i] = compInfo.renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG];
		}
#endif
		if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 1))
		{
			CopyLineExpand<1, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 1, 1);
			CopyLineExpand<1, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 1, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 2))
		{
			CopyLineExpand<2, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 2, 1);
			CopyLineExpand<2, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 2, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 3))
		{
			CopyLineExpand<3, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 3, 1);
			CopyLineExpand<3, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 3, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 4))
		{
			CopyLineExpand<4, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 4, 1);
			CopyLineExpand<4, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 4, 1);
		}
		else if ((compInfo.line.widthCustom % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0)
		{
			CopyLineExpand<0xFFFF, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compInfo.line.widthCustom, 1);
			CopyLineExpand<0xFFFF, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compInfo.line.widthCustom, 1);
		}
		else
		{
			CopyLineExpand<-1, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compInfo.line.widthCustom, 1);
			CopyLineExpand<-1, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compInfo.line.widthCustom, 1);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
FORCEINLINE void GPUEngineBase::_RenderLine_LayerBG_Final(GPUEngineCompositorInfo &compInfo)
{
	bool useCustomVRAM = false;
	
	if (WILLDEFERCOMPOSITING)
	{
		// Because there is no guarantee for any given pixel to be written out, we need
		// to zero out the deferred index buffer so that unwritten pixels can properly
		// fail in _CompositeLineDeferred(). If we don't do this, then previously rendered
		// layers may leave garbage indices for the current layer to mistakenly use if
		// the current layer just so happens to have unwritten pixels.
		//
		// Test case: The score screen in Sonic Rush will be taken over by BG2, filling
		// the screen with blue, unless this initialization is done each time.
		memset(this->_deferredIndexNative, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	}
	
	switch (compInfo.renderState.selectedBGLayer->baseType)
	{
		case BGType_Text: this->_LineText<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo); break;
		case BGType_Affine: this->_LineRot<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo); break;
		case BGType_AffineExt: this->_LineExtRot<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, useCustomVRAM); break;
		case BGType_Large8bpp: this->_LineExtRot<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, useCustomVRAM); break;
		case BGType_Invalid:
			PROGINFO("Attempting to render an invalid BG type\n");
			break;
		default:
			break;
	}
	
	// If compositing at the native size, each pixel is composited immediately. However, if
	// compositing at a custom size, pixel gathering and pixel compositing are split up into
	// separate steps. If compositing at a custom size, composite the entire line now.
	if ( (COMPOSITORMODE != GPUCompositorMode_Debug) && (WILLDEFERCOMPOSITING || !this->isLineRenderNative[compInfo.line.indexNative] || (useCustomVRAM && (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && !GPU->GetDisplayInfo().isCustomSizeRequested)) )
	{
		if (useCustomVRAM)
		{
			const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(compInfo.renderState.selectedBGLayer->BMPAddress, compInfo.line.blockOffsetCustom);
			this->_CompositeVRAMLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG, WILLPERFORMWINDOWTEST>(compInfo, vramColorPtr);
		}
		else
		{
			this->_PrecompositeNativeToCustomLineBG<MOSAIC>(compInfo);
			this->_CompositeLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG, WILLPERFORMWINDOWTEST>(compInfo, this->_deferredColorCustom, this->_deferredIndexCustom);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_RenderLine_LayerBG_ApplyMosaic(GPUEngineCompositorInfo &compInfo)
{
	if (this->isLineRenderNative[compInfo.line.indexNative])
	{
		this->_RenderLine_LayerBG_Final<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, false>(compInfo);
	}
	else
	{
		this->_RenderLine_LayerBG_Final<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, true>(compInfo);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_RenderLine_LayerBG(GPUEngineCompositorInfo &compInfo)
{
#ifndef DISABLE_MOSAIC
	if (compInfo.renderState.selectedBGLayer->isMosaic && compInfo.renderState.isBGMosaicSet)
	{
		this->_RenderLine_LayerBG_ApplyMosaic<COMPOSITORMODE, OUTPUTFORMAT, true, WILLPERFORMWINDOWTEST>(compInfo);
	}
	else
#endif
	{
		this->_RenderLine_LayerBG_ApplyMosaic<COMPOSITORMODE, OUTPUTFORMAT, false, WILLPERFORMWINDOWTEST>(compInfo);
	}
}

void GPUEngineBase::RenderLayerBG(const GPULayerID layerID, u16 *dstColorBuffer)
{
	GPUEngineCompositorInfo compInfo;
	memset(&compInfo, 0, sizeof(compInfo));
	
	compInfo.renderState.displayOutputMode = GPUDisplayMode_Normal;
	compInfo.renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	compInfo.renderState.selectedLayerID = layerID;
	compInfo.renderState.selectedBGLayer = &this->_BGLayer[layerID];
	compInfo.renderState.colorEffect = ColorEffect_Disable;
	compInfo.renderState.masterBrightnessMode = GPUMasterBrightMode_Disable;
	compInfo.renderState.masterBrightnessIsFullIntensity = false;
	compInfo.renderState.masterBrightnessIsMaxOrMin = true;
	compInfo.renderState.spriteRenderMode = this->_currentRenderState.spriteRenderMode;
	compInfo.renderState.spriteBoundary = this->_currentRenderState.spriteBoundary;
	compInfo.renderState.spriteBMPBoundary = this->_currentRenderState.spriteBMPBoundary;
	
	const size_t layerWidth = compInfo.renderState.selectedBGLayer->size.width;
	const size_t layerHeight = compInfo.renderState.selectedBGLayer->size.height;
	compInfo.line.widthCustom = layerWidth;
	compInfo.line.renderCount = 1;
	
	compInfo.target.lineLayerIDHead = NULL;
	compInfo.target.lineLayerIDHeadNative = NULL;
	compInfo.target.lineLayerIDHeadCustom = NULL;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = compInfo.target.xNative;
	compInfo.target.lineColor = (void **)&compInfo.target.lineColor16;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = NULL;
	
	for (size_t lineIndex = 0; lineIndex < layerHeight; lineIndex++)
	{
		compInfo.line.indexNative = lineIndex;
		compInfo.line.indexCustom = compInfo.line.indexNative;
		compInfo.line.pixelCount = layerWidth;
		compInfo.line.blockOffsetNative = compInfo.line.indexNative * layerWidth;
		compInfo.line.blockOffsetCustom = compInfo.line.blockOffsetNative;
		
		compInfo.target.lineColorHead = (u16 *)dstColorBuffer + compInfo.line.blockOffsetNative;
		compInfo.target.lineColorHeadNative = compInfo.target.lineColorHead;
		compInfo.target.lineColorHeadCustom = compInfo.target.lineColorHeadNative;
		
		this->_RenderLine_LayerBG_Final<GPUCompositorMode_Debug, NDSColorFormat_BGR555_Rev, false, false, false>(compInfo);
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_HandleDisplayModeOff(const size_t l)
{
	// Native rendering only.
	// In this display mode, the display is cleared to white.
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u16 *)this->_nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFF);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u32 *)this->_nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0x1F3F3F3F);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u32 *)this->_nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFFFFFF);
			break;
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_HandleDisplayModeNormal(const size_t l)
{
	if (!this->isLineRenderNative[l])
	{
		this->isLineOutputNative[l] = false;
		this->nativeLineOutputCount--;
	}
}

template <size_t WINNUM>
void GPUEngineBase::ParseReg_WINnH()
{
	this->_needUpdateWINH[WINNUM] = true;
}

void GPUEngineBase::ParseReg_WININ()
{
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.WIN0_enable[GPULayerID_BG0] = this->_IORegisterMap->WIN0IN.BG0_Enable;
	renderState.WIN0_enable[GPULayerID_BG1] = this->_IORegisterMap->WIN0IN.BG1_Enable;
	renderState.WIN0_enable[GPULayerID_BG2] = this->_IORegisterMap->WIN0IN.BG2_Enable;
	renderState.WIN0_enable[GPULayerID_BG3] = this->_IORegisterMap->WIN0IN.BG3_Enable;
	renderState.WIN0_enable[GPULayerID_OBJ] = this->_IORegisterMap->WIN0IN.OBJ_Enable;
	renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WIN0IN.Effect_Enable;
	
	renderState.WIN1_enable[GPULayerID_BG0] = this->_IORegisterMap->WIN1IN.BG0_Enable;
	renderState.WIN1_enable[GPULayerID_BG1] = this->_IORegisterMap->WIN1IN.BG1_Enable;
	renderState.WIN1_enable[GPULayerID_BG2] = this->_IORegisterMap->WIN1IN.BG2_Enable;
	renderState.WIN1_enable[GPULayerID_BG3] = this->_IORegisterMap->WIN1IN.BG3_Enable;
	renderState.WIN1_enable[GPULayerID_OBJ] = this->_IORegisterMap->WIN1IN.OBJ_Enable;
	renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WIN1IN.Effect_Enable;
	
#if defined(ENABLE_SSE2)
	renderState.WIN0_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG0_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN0_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG1_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN0_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG2_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN0_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG3_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN0_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.OBJ_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN0_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.Effect_Enable != 0) ? 0xFF : 0x00);
	
	renderState.WIN1_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG0_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN1_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG1_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN1_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG2_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN1_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG3_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN1_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.OBJ_Enable != 0) ? 0xFF : 0x00);
	renderState.WIN1_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.Effect_Enable != 0) ? 0xFF : 0x00);
#endif
}

void GPUEngineBase::ParseReg_WINOUT()
{
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.WINOUT_enable[GPULayerID_BG0] = this->_IORegisterMap->WINOUT.BG0_Enable;
	renderState.WINOUT_enable[GPULayerID_BG1] = this->_IORegisterMap->WINOUT.BG1_Enable;
	renderState.WINOUT_enable[GPULayerID_BG2] = this->_IORegisterMap->WINOUT.BG2_Enable;
	renderState.WINOUT_enable[GPULayerID_BG3] = this->_IORegisterMap->WINOUT.BG3_Enable;
	renderState.WINOUT_enable[GPULayerID_OBJ] = this->_IORegisterMap->WINOUT.OBJ_Enable;
	renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WINOUT.Effect_Enable;
	
	renderState.WINOBJ_enable[GPULayerID_BG0] = this->_IORegisterMap->WINOBJ.BG0_Enable;
	renderState.WINOBJ_enable[GPULayerID_BG1] = this->_IORegisterMap->WINOBJ.BG1_Enable;
	renderState.WINOBJ_enable[GPULayerID_BG2] = this->_IORegisterMap->WINOBJ.BG2_Enable;
	renderState.WINOBJ_enable[GPULayerID_BG3] = this->_IORegisterMap->WINOBJ.BG3_Enable;
	renderState.WINOBJ_enable[GPULayerID_OBJ] = this->_IORegisterMap->WINOBJ.OBJ_Enable;
	renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WINOBJ.Effect_Enable;
	
#if defined(ENABLE_SSE2)
	renderState.WINOUT_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG0_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOUT_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG1_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOUT_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG2_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOUT_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG3_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOUT_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.OBJ_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOUT_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.Effect_Enable != 0) ? 0xFF : 0x00);
	
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG0_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG1_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG2_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOBJ_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG3_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOBJ_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.OBJ_Enable != 0) ? 0xFF : 0x00);
	renderState.WINOBJ_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.Effect_Enable != 0) ? 0xFF : 0x00);
#endif
}

void GPUEngineBase::ParseReg_MOSAIC()
{
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.mosaicWidthBG = this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.BG_MosaicH];
	renderState.mosaicHeightBG = this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.BG_MosaicV];
	renderState.mosaicWidthOBJ = this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.OBJ_MosaicH];
	renderState.mosaicHeightOBJ = this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.OBJ_MosaicV];
	
	renderState.isBGMosaicSet = (this->_IORegisterMap->MOSAIC.BG_MosaicH != 0) || (this->_IORegisterMap->MOSAIC.BG_MosaicV != 0);
	renderState.isOBJMosaicSet = (this->_IORegisterMap->MOSAIC.OBJ_MosaicH != 0) || (this->_IORegisterMap->MOSAIC.OBJ_MosaicV != 0);
}

void GPUEngineBase::ParseReg_BLDCNT()
{
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.colorEffect = (ColorEffect)BLDCNT.ColorEffect;
	
	renderState.srcEffectEnable[GPULayerID_BG0] = (BLDCNT.BG0_Target1 != 0);
	renderState.srcEffectEnable[GPULayerID_BG1] = (BLDCNT.BG1_Target1 != 0);
	renderState.srcEffectEnable[GPULayerID_BG2] = (BLDCNT.BG2_Target1 != 0);
	renderState.srcEffectEnable[GPULayerID_BG3] = (BLDCNT.BG3_Target1 != 0);
	renderState.srcEffectEnable[GPULayerID_OBJ] = (BLDCNT.OBJ_Target1 != 0);
	renderState.srcEffectEnable[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target1 != 0);
	
	renderState.dstBlendEnable[GPULayerID_BG0] = (BLDCNT.BG0_Target2 != 0);
	renderState.dstBlendEnable[GPULayerID_BG1] = (BLDCNT.BG1_Target2 != 0);
	renderState.dstBlendEnable[GPULayerID_BG2] = (BLDCNT.BG2_Target2 != 0);
	renderState.dstBlendEnable[GPULayerID_BG3] = (BLDCNT.BG3_Target2 != 0);
	renderState.dstBlendEnable[GPULayerID_OBJ] = (BLDCNT.OBJ_Target2 != 0);
	renderState.dstBlendEnable[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target2 != 0);
	
	renderState.dstAnyBlendEnable = renderState.dstBlendEnable[GPULayerID_BG0] ||
	                                renderState.dstBlendEnable[GPULayerID_BG1] ||
	                                renderState.dstBlendEnable[GPULayerID_BG2] ||
	                                renderState.dstBlendEnable[GPULayerID_BG3] ||
	                                renderState.dstBlendEnable[GPULayerID_OBJ] ||
	                                renderState.dstBlendEnable[GPULayerID_Backdrop];
	
#ifdef ENABLE_SSE2
	const __m128i one_vec128 = _mm_set1_epi8(1);
	
	renderState.srcEffectEnable_SSE2[GPULayerID_BG0] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG0_Target1), one_vec128);
	renderState.srcEffectEnable_SSE2[GPULayerID_BG1] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG1_Target1), one_vec128);
	renderState.srcEffectEnable_SSE2[GPULayerID_BG2] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG2_Target1), one_vec128);
	renderState.srcEffectEnable_SSE2[GPULayerID_BG3] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG3_Target1), one_vec128);
	renderState.srcEffectEnable_SSE2[GPULayerID_OBJ] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.OBJ_Target1), one_vec128);
	renderState.srcEffectEnable_SSE2[GPULayerID_Backdrop] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.Backdrop_Target1), one_vec128);
	
#ifdef ENABLE_SSSE3
	renderState.dstBlendEnable_SSSE3 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
													BLDCNT.Backdrop_Target2,
													BLDCNT.OBJ_Target2,
													BLDCNT.BG3_Target2,
													BLDCNT.BG2_Target2,
													BLDCNT.BG1_Target2,
													BLDCNT.BG0_Target2);
#else
	renderState.dstBlendEnable_SSE2[GPULayerID_BG0] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG0_Target2), one_vec128);
	renderState.dstBlendEnable_SSE2[GPULayerID_BG1] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG1_Target2), one_vec128);
	renderState.dstBlendEnable_SSE2[GPULayerID_BG2] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG2_Target2), one_vec128);
	renderState.dstBlendEnable_SSE2[GPULayerID_BG3] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG3_Target2), one_vec128);
	renderState.dstBlendEnable_SSE2[GPULayerID_OBJ] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.OBJ_Target2), one_vec128);
	renderState.dstBlendEnable_SSE2[GPULayerID_Backdrop] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.Backdrop_Target2), one_vec128);
#endif
	
#endif // ENABLE_SSE2
}

void GPUEngineBase::ParseReg_BLDALPHA()
{
	const IOREG_BLDALPHA &BLDALPHA = this->_IORegisterMap->BLDALPHA;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.blendEVA = (BLDALPHA.EVA >= 16) ? 16 : BLDALPHA.EVA;
	renderState.blendEVB = (BLDALPHA.EVB >= 16) ? 16 : BLDALPHA.EVB;
	renderState.blendTable555 = (TBlendTable *)&GPUEngineBase::_blendTable555[renderState.blendEVA][renderState.blendEVB][0][0];
}

void GPUEngineBase::ParseReg_BLDY()
{
	const IOREG_BLDY &BLDY = this->_IORegisterMap->BLDY;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.blendEVY = (BLDY.EVY >= 16) ? 16 : BLDY.EVY;
	renderState.brightnessUpTable555 = &GPUEngineBase::_brightnessUpTable555[renderState.blendEVY][0];
	renderState.brightnessUpTable666 = &GPUEngineBase::_brightnessUpTable666[renderState.blendEVY][0];
	renderState.brightnessUpTable888 = &GPUEngineBase::_brightnessUpTable888[renderState.blendEVY][0];
	renderState.brightnessDownTable555 = &GPUEngineBase::_brightnessDownTable555[renderState.blendEVY][0];
	renderState.brightnessDownTable666 = &GPUEngineBase::_brightnessDownTable666[renderState.blendEVY][0];
	renderState.brightnessDownTable888 = &GPUEngineBase::_brightnessDownTable888[renderState.blendEVY][0];
}

const BGLayerInfo& GPUEngineBase::GetBGLayerInfoByID(const GPULayerID layerID)
{
	return this->_BGLayer[layerID];
}

NDSDisplayID GPUEngineBase::GetTargetDisplayByID() const
{
	return this->_targetDisplayID;
}

void GPUEngineBase::SetTargetDisplayByID(const NDSDisplayID theDisplayID)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	void *newCustomBufferPtr = (theDisplayID == NDSDisplayID_Main) ? dispInfo.customBuffer[NDSDisplayID_Main] : dispInfo.customBuffer[NDSDisplayID_Touch];
	
	if (!this->_asyncClearUseInternalCustomBuffer && (newCustomBufferPtr != this->_customBuffer))
	{
		// Games SHOULD only be changing the engine/display association outside the V-blank.
		// However, if there is a situation where a game changes this association mid-frame,
		// then we need to force any asynchronous clearing to finish so that we can return
		// control of _customBuffer to this thread.
		this->RenderLineClearAsyncFinish();
		this->_asyncClearTransitionedLineFromBackdropCount = 0;
	}
	
	this->_nativeBuffer = (theDisplayID == NDSDisplayID_Main) ? dispInfo.nativeBuffer[NDSDisplayID_Main] : dispInfo.nativeBuffer[NDSDisplayID_Touch];
	this->_customBuffer = newCustomBufferPtr;
	
	this->_targetDisplayID = theDisplayID;
}

GPUEngineID GPUEngineBase::GetEngineID() const
{
	return this->_engineID;
}

void* GPUEngineBase::GetRenderedBuffer() const
{
	return this->_renderedBuffer;
}

size_t GPUEngineBase::GetRenderedWidth() const
{
	return this->_renderedWidth;
}

size_t GPUEngineBase::GetRenderedHeight() const
{
	return this->_renderedHeight;
}

void GPUEngineBase::SetCustomFramebufferSize(size_t w, size_t h)
{
	void *oldWorkingLineColor = this->_internalRenderLineTargetCustom;
	u8 *oldWorkingLineLayerID = this->_renderLineLayerIDCustom;
	u8 *oldDeferredIndexCustom = this->_deferredIndexCustom;
	u16 *oldDeferredColorCustom = this->_deferredColorCustom;
	u16 *oldSprColorCustom = this->_sprColorCustom;
	u8 *oldSprAlphaCustom = this->_sprAlphaCustom;
	u8 *oldSprTypeCustom = this->_sprTypeCustom;
	u8 *oldDidPassWindowTestCustomMasterPtr = this->_didPassWindowTestCustomMasterPtr;
	
	this->_internalRenderLineTargetCustom = malloc_alignedPage(w * h * GPU->GetDisplayInfo().pixelBytes);
	this->_renderLineLayerIDCustom = (u8 *)malloc_alignedPage(w * (h + (_gpuLargestDstLineCount * 4)) * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	this->_deferredIndexCustom = (u8 *)malloc_alignedPage(w * sizeof(u8));
	this->_deferredColorCustom = (u16 *)malloc_alignedPage(w * sizeof(u16));
	
	this->_sprColorCustom = (u16 *)malloc_alignedPage(w * sizeof(u16));
	this->_sprAlphaCustom = (u8 *)malloc_alignedPage(w * sizeof(u8));
	this->_sprTypeCustom = (u8 *)malloc_alignedPage(w * sizeof(u8));
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->_nativeBuffer = (this->_targetDisplayID == NDSDisplayID_Main) ? dispInfo.nativeBuffer[NDSDisplayID_Main] : dispInfo.nativeBuffer[NDSDisplayID_Touch];
	this->_customBuffer = (this->_targetDisplayID == NDSDisplayID_Main) ? dispInfo.customBuffer[NDSDisplayID_Main] : dispInfo.customBuffer[NDSDisplayID_Touch];
	
	if (this->nativeLineOutputCount == GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		this->_renderedBuffer = this->_nativeBuffer;
		this->_renderedWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	else
	{
		this->_renderedBuffer = this->_customBuffer;
		this->_renderedWidth  = dispInfo.customWidth;
		this->_renderedHeight = dispInfo.customHeight;
	}
	
	u8 *newDidPassWindowTestCustomMasterPtr = (u8 *)malloc_alignedPage(w * 10 * sizeof(u8));
	
	this->_didPassWindowTestCustomMasterPtr = newDidPassWindowTestCustomMasterPtr;
	this->_didPassWindowTestCustom[GPULayerID_BG0] = this->_didPassWindowTestCustomMasterPtr + (0 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_BG1] = this->_didPassWindowTestCustomMasterPtr + (1 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_BG2] = this->_didPassWindowTestCustomMasterPtr + (2 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_BG3] = this->_didPassWindowTestCustomMasterPtr + (3 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_OBJ] = this->_didPassWindowTestCustomMasterPtr + (4 * w * sizeof(u8));
	
	this->_enableColorEffectCustomMasterPtr = newDidPassWindowTestCustomMasterPtr + (w * 5 * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG0] = this->_enableColorEffectCustomMasterPtr + (0 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG1] = this->_enableColorEffectCustomMasterPtr + (1 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG2] = this->_enableColorEffectCustomMasterPtr + (2 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG3] = this->_enableColorEffectCustomMasterPtr + (3 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_OBJ] = this->_enableColorEffectCustomMasterPtr + (4 * w * sizeof(u8));
	
	this->_needUpdateWINH[0] = true;
	this->_needUpdateWINH[1] = true;
	
	for (size_t line = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		this->_currentCompositorInfo[line].line = GPU->GetLineInfoAtIndex(line);
		this->_currentCompositorInfo[line].target.lineColor = (GPU->GetDisplayInfo().colorFormat == NDSColorFormat_BGR555_Rev) ? (void **)&this->_currentCompositorInfo[line].target.lineColor16 : (void **)&this->_currentCompositorInfo[line].target.lineColor32;
	}
	
	free_aligned(oldWorkingLineColor);
	free_aligned(oldWorkingLineLayerID);
	free_aligned(oldDeferredIndexCustom);
	free_aligned(oldDeferredColorCustom);
	free_aligned(oldSprColorCustom);
	free_aligned(oldSprAlphaCustom);
	free_aligned(oldSprTypeCustom);
	free_aligned(oldDidPassWindowTestCustomMasterPtr);
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::ResolveCustomRendering()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	if (this->nativeLineOutputCount == GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return;
	}
	else if (this->nativeLineOutputCount == 0)
	{
		this->_renderedWidth = dispInfo.customWidth;
		this->_renderedHeight = dispInfo.customHeight;
		this->_renderedBuffer = this->_customBuffer;
		return;
	}
	
	// Resolve any remaining native lines to the custom buffer
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const u16 *__restrict src = (u16 *__restrict)this->_nativeBuffer;
		u16 *__restrict dst = (u16 *__restrict)this->_customBuffer;
		
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[y].line;
			
			if (this->isLineOutputNative[y])
			{
				CopyLineExpandHinted<0xFFFF, true, false, false, 2>(lineInfo, src, dst);
				this->isLineOutputNative[y] = false;
			}
			
			src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
			dst += lineInfo.pixelCount;
		}
	}
	else
	{
		const u32 *__restrict src = (u32 *__restrict)this->_nativeBuffer;
		u32 *__restrict dst = (u32 *__restrict)this->_customBuffer;
		
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[y].line;
			
			if (this->isLineOutputNative[y])
			{
				CopyLineExpandHinted<0xFFFF, true, false, false, 4>(lineInfo, src, dst);
				this->isLineOutputNative[y] = false;
			}
			
			src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
			dst += lineInfo.pixelCount;
		}
	}
	
	this->nativeLineOutputCount = 0;
	this->_renderedWidth = dispInfo.customWidth;
	this->_renderedHeight = dispInfo.customHeight;
	this->_renderedBuffer = this->_customBuffer;
}

void GPUEngineBase::ResolveToCustomFramebuffer(NDSDisplayInfo &mutableInfo)
{
	if (mutableInfo.didPerformCustomRender[this->_targetDisplayID])
	{
		return;
	}
	
	if (mutableInfo.isCustomSizeRequested)
	{
		if (mutableInfo.pixelBytes == 2)
		{
			const u16 *__restrict src = (u16 *__restrict)mutableInfo.nativeBuffer[this->_targetDisplayID];
			u16 *__restrict dst = (u16 *__restrict)mutableInfo.customBuffer[this->_targetDisplayID];
			
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[y].line;
				CopyLineExpandHinted<0xFFFF, true, false, false, 2>(lineInfo, src, dst);
				src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
				dst += lineInfo.pixelCount;
			}
		}
		else if (mutableInfo.pixelBytes == 4)
		{
			const u32 *__restrict src = (u32 *__restrict)mutableInfo.nativeBuffer[this->_targetDisplayID];
			u32 *__restrict dst = (u32 *__restrict)mutableInfo.customBuffer[this->_targetDisplayID];
			
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[y].line;
				CopyLineExpandHinted<0xFFFF, true, false, false, 4>(lineInfo, src, dst);
				src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
				dst += lineInfo.pixelCount;
			}
		}
	}
	else
	{
		memcpy(mutableInfo.customBuffer[this->_targetDisplayID], mutableInfo.nativeBuffer[this->_targetDisplayID], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * mutableInfo.pixelBytes);
	}
	
	mutableInfo.didPerformCustomRender[this->_targetDisplayID] = true;
}

void GPUEngineBase::RefreshAffineStartRegs()
{
	//this is speculative. the idea is as follows:
	//whenever the user updates the affine start position regs, it goes into the active regs immediately
	//(this is handled on the set event from MMU)
	//maybe it shouldnt take effect until the next hblank or something..
	//this is a based on a combination of:
	//heroes of mana intro FMV
	//SPP level 3-8 rotoscale room
	//NSMB raster fx backdrops
	//bubble bobble revolution classic mode
	//NOTE:
	//I am REALLY unsatisfied with this logic now. But it seems to be working.
	
	this->_IORegisterMap->BG2X = this->savedBG2X;
	this->_IORegisterMap->BG2Y = this->savedBG2Y;
	this->_IORegisterMap->BG3X = this->savedBG3X;
	this->_IORegisterMap->BG3Y = this->savedBG3Y;
}

// normally should have same addresses
void GPUEngineBase::REG_DISPx_pack_test()
{
	const GPU_IOREG *r = this->_IORegisterMap;
	
	printf("%08lx %02x\n", (uintptr_t)r, (u32)((uintptr_t)(&r->DISPCNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISPSTAT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->VCOUNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BGnCNT[GPULayerID_BG0]) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BGnOFS[GPULayerID_BG0]) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BG2Param) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BG3Param) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISP3DCNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISPCAPCNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISP_MMEM_FIFO) - (uintptr_t)r) );
}

void GPUEngineBase::ParseAllRegisters()
{
	this->ParseReg_DISPCNT();
	// No need to call ParseReg_BGnCNT(), since it is already called by ParseReg_DISPCNT().
	
	this->ParseReg_BGnHOFS<GPULayerID_BG0>();
	this->ParseReg_BGnHOFS<GPULayerID_BG1>();
	this->ParseReg_BGnHOFS<GPULayerID_BG2>();
	this->ParseReg_BGnHOFS<GPULayerID_BG3>();
	this->ParseReg_BGnVOFS<GPULayerID_BG0>();
	this->ParseReg_BGnVOFS<GPULayerID_BG1>();
	this->ParseReg_BGnVOFS<GPULayerID_BG2>();
	this->ParseReg_BGnVOFS<GPULayerID_BG3>();
	
	this->ParseReg_BGnX<GPULayerID_BG2>();
	this->ParseReg_BGnY<GPULayerID_BG2>();
	this->ParseReg_BGnX<GPULayerID_BG3>();
	this->ParseReg_BGnY<GPULayerID_BG3>();
	
	this->ParseReg_WINnH<0>();
	this->ParseReg_WINnH<1>();
	this->ParseReg_WININ();
	this->ParseReg_WINOUT();
	
	this->ParseReg_MOSAIC();
	this->ParseReg_BLDCNT();
	this->ParseReg_BLDALPHA();
	this->ParseReg_BLDY();
	this->ParseReg_MASTER_BRIGHT();
}

GPUEngineA::GPUEngineA()
{
	_engineID = GPUEngineID_Main;
	_targetDisplayID = NDSDisplayID_Main;
	_IORegisterMap = (GPU_IOREG *)MMU.ARM9_REG;
	_paletteBG = (u16 *)MMU.ARM9_VMEM;
	_paletteOBJ = (u16 *)(MMU.ARM9_VMEM + ADDRESS_STEP_512B);
	_oamList = (OAMAttributes *)(MMU.ARM9_OAM);
	_sprMem = MMU_AOBJ;
	
	_VRAMNativeBlockPtr[0] = (u16 *)MMU.ARM9_LCD;
	_VRAMNativeBlockPtr[1] = _VRAMNativeBlockPtr[0] + (1 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockPtr[2] = _VRAMNativeBlockPtr[0] + (2 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockPtr[3] = _VRAMNativeBlockPtr[0] + (3 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	memset(this->_VRAMNativeBlockCaptureCopy, 0, GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
	_VRAMNativeBlockCaptureCopyPtr[0] = this->_VRAMNativeBlockCaptureCopy;
	_VRAMNativeBlockCaptureCopyPtr[1] = _VRAMNativeBlockCaptureCopyPtr[0] + (1 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockCaptureCopyPtr[2] = _VRAMNativeBlockCaptureCopyPtr[0] + (2 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockCaptureCopyPtr[3] = _VRAMNativeBlockCaptureCopyPtr[0] + (3 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	_nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
	_nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
	_nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
	_nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		_isLineCaptureNative[0][l] = true;
		_isLineCaptureNative[1][l] = true;
		_isLineCaptureNative[2][l] = true;
		_isLineCaptureNative[3][l] = true;
	}
	
	_3DFramebufferMain = (FragmentColor *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(FragmentColor));
	_3DFramebuffer16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	_captureWorkingDisplay16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingA16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingB16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingA32 = (FragmentColor *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(FragmentColor));
	_captureWorkingB32 = (FragmentColor *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(FragmentColor));
}

GPUEngineA::~GPUEngineA()
{
	free_aligned(this->_3DFramebufferMain);
	free_aligned(this->_3DFramebuffer16);
	free_aligned(this->_captureWorkingDisplay16);
	free_aligned(this->_captureWorkingA16);
	free_aligned(this->_captureWorkingB16);
	free_aligned(this->_captureWorkingA32);
	free_aligned(this->_captureWorkingB32);
}

GPUEngineA* GPUEngineA::Allocate()
{
	return new(malloc_aligned64(sizeof(GPUEngineA))) GPUEngineA();
}

void GPUEngineA::FinalizeAndDeallocate()
{
	this->~GPUEngineA();
	free_aligned(this);
}

void GPUEngineA::Reset()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->_Reset_Base();
	
	memset(&this->_dispCapCnt, 0, sizeof(DISPCAPCNT_parsed));
	this->_displayCaptureEnable = false;
	
	this->_BGLayer[GPULayerID_BG0].BMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].BMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].BMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].BMPAddress = MMU_ABG;
	
	this->_BGLayer[GPULayerID_BG0].largeBMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].largeBMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].largeBMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].largeBMPAddress = MMU_ABG;
	
	this->_BGLayer[GPULayerID_BG0].tileMapAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].tileMapAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].tileMapAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].tileMapAddress = MMU_ABG;
	
	this->_BGLayer[GPULayerID_BG0].tileEntryAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].tileEntryAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].tileEntryAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].tileEntryAddress = MMU_ABG;
	
	memset(this->_VRAMNativeBlockCaptureCopy, 0, GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
	
	this->ResetCaptureLineStates(0);
	this->ResetCaptureLineStates(1);
	this->ResetCaptureLineStates(2);
	this->ResetCaptureLineStates(3);
	this->SetTargetDisplayByID(NDSDisplayID_Main);
	
	memset(this->_3DFramebufferMain, 0, dispInfo.customWidth * dispInfo.customHeight * sizeof(FragmentColor));
	memset(this->_3DFramebuffer16, 0, dispInfo.customWidth * dispInfo.customHeight * sizeof(u16));
	memset(this->_captureWorkingDisplay16, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingA16, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingB16, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingA32, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(FragmentColor));
	memset(this->_captureWorkingB32, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(FragmentColor));
}

void GPUEngineA::ResetCaptureLineStates(const size_t blockID)
{
	if (this->_nativeLineCaptureCount[blockID] == GPU_VRAM_BLOCK_LINES)
	{
		return;
	}
	
	this->_nativeLineCaptureCount[blockID] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		this->_isLineCaptureNative[blockID][l] = true;
	}
}

void GPUEngineA::ParseReg_DISPCAPCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	this->_dispCapCnt.EVA = (DISPCAPCNT.EVA >= 16) ? 16 : DISPCAPCNT.EVA;
	this->_dispCapCnt.EVB = (DISPCAPCNT.EVB >= 16) ? 16 : DISPCAPCNT.EVB;
	this->_dispCapCnt.readOffset = (DISPCNT.DisplayMode == GPUDisplayMode_VRAM) ? 0 : DISPCAPCNT.VRAMReadOffset;
	
	switch (DISPCAPCNT.CaptureSize)
	{
		case DisplayCaptureSize_128x128:
			this->_dispCapCnt.capy = 128;
			break;
			
		case DisplayCaptureSize_256x64:
			this->_dispCapCnt.capy = 64;
			break;
			
		case DisplayCaptureSize_256x128:
			this->_dispCapCnt.capy = 128;
			break;
			
		case DisplayCaptureSize_256x192:
			this->_dispCapCnt.capy = 192;
			break;
			
		default:
			break;
	}
	
	/*INFO("Capture 0x%X:\n EVA=%i, EVB=%i, wBlock=%i, wOffset=%i, capX=%i, capY=%i\n rBlock=%i, rOffset=%i, srcCap=%i, dst=0x%X, src=0x%X\n srcA=%i, srcB=%i\n\n",
	 val, this->_dispCapCnt.EVA, this->_dispCapCnt.EVB, this->_dispCapCnt.writeBlock, this->_dispCapCnt.writeOffset,
	 this->_dispCapCnt.capy, this->_dispCapCnt.readBlock, this->_dispCapCnt.readOffset,
	 this->_dispCapCnt.capSrc, this->_dispCapCnt.dst - MMU.ARM9_LCD, this->_dispCapCnt.src - MMU.ARM9_LCD,
	 this->_dispCapCnt.srcA, this->_dispCapCnt.srcB);*/
}

FragmentColor* GPUEngineA::Get3DFramebufferMain() const
{
	return this->_3DFramebufferMain;
}

u16* GPUEngineA::Get3DFramebuffer16() const
{
	return this->_3DFramebuffer16;
}

bool GPUEngineA::IsLineCaptureNative(const size_t blockID, const size_t blockLine)
{
	return this->_isLineCaptureNative[blockID][blockLine];
}

void* GPUEngineA::GetCustomVRAMBlockPtr(const size_t blockID)
{
	return this->_VRAMCustomBlockPtr[blockID];
}

void GPUEngineA::SetCustomFramebufferSize(size_t w, size_t h)
{
	this->GPUEngineBase::SetCustomFramebufferSize(w, h);
	
	FragmentColor *old3DFramebufferMain = this->_3DFramebufferMain;
	u16 *old3DFramebuffer16 = this->_3DFramebuffer16;
	u16 *oldCaptureWorkingDisplay16 = this->_captureWorkingDisplay16;
	u16 *oldCaptureWorkingA16 = this->_captureWorkingA16;
	u16 *oldCaptureWorkingB16 = this->_captureWorkingB16;
	FragmentColor *oldCaptureWorkingA32 = this->_captureWorkingA32;
	FragmentColor *oldCaptureWorkingB32 = this->_captureWorkingB32;
	
	this->_3DFramebufferMain = (FragmentColor *)malloc_alignedPage(w * h * sizeof(FragmentColor));
	this->_3DFramebuffer16 = (u16 *)malloc_alignedPage(w * h * sizeof(u16));
	this->_captureWorkingDisplay16 = (u16 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(u16));
	this->_captureWorkingA16 = (u16 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(u16));
	this->_captureWorkingB16 = (u16 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(u16));
	this->_captureWorkingA32 = (FragmentColor *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(FragmentColor));
	this->_captureWorkingB32 = (FragmentColor *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(FragmentColor));
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[GPU_VRAM_BLOCK_LINES].line;
	
	if (dispInfo.colorFormat == NDSColorFormat_BGR888_Rev)
	{
		this->_VRAMCustomBlockPtr[0] = (FragmentColor *)GPU->GetCustomVRAMBuffer();
		this->_VRAMCustomBlockPtr[1] = (FragmentColor *)this->_VRAMCustomBlockPtr[0] + (1 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[2] = (FragmentColor *)this->_VRAMCustomBlockPtr[0] + (2 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[3] = (FragmentColor *)this->_VRAMCustomBlockPtr[0] + (3 * lineInfo.indexCustom * w);
	}
	else
	{
		this->_VRAMCustomBlockPtr[0] = (u16 *)GPU->GetCustomVRAMBuffer();
		this->_VRAMCustomBlockPtr[1] = (u16 *)this->_VRAMCustomBlockPtr[0] + (1 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[2] = (u16 *)this->_VRAMCustomBlockPtr[0] + (2 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[3] = (u16 *)this->_VRAMCustomBlockPtr[0] + (3 * lineInfo.indexCustom * w);
	}
	
	free_aligned(old3DFramebufferMain);
	free_aligned(old3DFramebuffer16);
	free_aligned(oldCaptureWorkingDisplay16);
	free_aligned(oldCaptureWorkingA16);
	free_aligned(oldCaptureWorkingB16);
	free_aligned(oldCaptureWorkingA32);
	free_aligned(oldCaptureWorkingB32);
}

bool GPUEngineA::WillRender3DLayer()
{
	return ( this->_isBGLayerShown[GPULayerID_BG0] && (this->_IORegisterMap->DISPCNT.BG0_3D != 0) );
}

bool GPUEngineA::WillCapture3DLayerDirect(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return ( this->WillDisplayCapture(l) && (DISPCAPCNT.SrcA != 0) && (DISPCAPCNT.CaptureSrc != 1) );
}

bool GPUEngineA::WillDisplayCapture(const size_t l)
{
	//we must block captures when the capture dest is not mapped to LCDC.
	//mario kart does this (maybe due to a programming bug, but maybe emulation timing error) when spamming confirm key during course intro and through black transition
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return this->_displayCaptureEnable && (vramConfiguration.banks[DISPCAPCNT.VRAMWriteBlock].purpose == VramConfiguration::LCDC) && (l < this->_dispCapCnt.capy);
}

void GPUEngineA::SetDisplayCaptureEnable()
{
	this->_displayCaptureEnable = (this->_IORegisterMap->DISPCAPCNT.CaptureEnable != 0);
}

void GPUEngineA::ResetDisplayCaptureEnable()
{
	IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	if (this->_displayCaptureEnable)
	{
		DISPCAPCNT.CaptureEnable = 0;
		this->_displayCaptureEnable = false;
	}
}

bool GPUEngineA::VerifyVRAMLineDidChange(const size_t blockID, const size_t l)
{
	// This method must be called for ALL instances where captured lines in VRAM may be read back.
	//
	// If a line is captured at a custom size, we need to ensure that the line hasn't been changed between
	// capture time and read time. If the captured line has changed, then we need to fallback to using the
	// native captured line instead.
	
	if (this->_isLineCaptureNative[blockID][l])
	{
		return false;
	}
	
	u16 *__restrict capturedNativeLine = this->_VRAMNativeBlockCaptureCopyPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	const u16 *__restrict currentNativeLine = this->_VRAMNativeBlockPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	const bool didVRAMLineChange = (memcmp(currentNativeLine, capturedNativeLine, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) != 0);
	if (didVRAMLineChange)
	{
		CopyLineExpandHinted<1, true, true, false, 2>(this->_currentCompositorInfo[l].line, this->_VRAMNativeBlockPtr[blockID], this->_VRAMNativeBlockCaptureCopyPtr[blockID]);
		this->_isLineCaptureNative[blockID][l] = true;
		this->_nativeLineCaptureCount[blockID]++;
	}
	
	return didVRAMLineChange;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::RenderLine(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	const bool isDisplayCaptureNeeded = this->WillDisplayCapture(l);
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	
	// Render the line
	if ( (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) || isDisplayCaptureNeeded )
	{
		if (compInfo.renderState.isAnyWindowEnabled)
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, true>(compInfo);
		}
		else
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, false>(compInfo);
		}
	}
	
	if (compInfo.line.indexNative >= 191)
	{
		this->RenderLineClearAsyncFinish();
	}
	
	// Fill the display output
	switch (compInfo.renderState.displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off (Display white)
			this->_HandleDisplayModeOff<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			this->_HandleDisplayModeNormal<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_VRAM: // Display VRAM framebuffer
			this->_HandleDisplayModeVRAM<OUTPUTFORMAT>(compInfo.line);
			break;
			
		case GPUDisplayMode_MainMemory: // Display Memory FIFO
			this->_HandleDisplayModeMainMemory<OUTPUTFORMAT>(compInfo.line);
			break;
	}
	
	//capture after displaying so that we can safely display vram before overwriting it here
	
	//BUG!!! if someone is capturing and displaying both from the fifo, then it will have been
	//consumed above by the display before we get here
	//(is that even legal? i think so)
	if (isDisplayCaptureNeeded)
	{
		if (DISPCAPCNT.CaptureSize == DisplayCaptureSize_128x128)
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH/2>(compInfo);
		}
		else
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH>(compInfo);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineA::RenderLine_Layer3D(GPUEngineCompositorInfo &compInfo)
{
	const FragmentColor *__restrict framebuffer3D = CurrentRenderer->GetFramebuffer();
	if (framebuffer3D == NULL)
	{
		return;
	}
	
	if (!CurrentRenderer->IsFramebufferNativeSize())
	{
		this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
	}
	
	const float customWidthScale = (float)compInfo.line.widthCustom / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const FragmentColor *__restrict srcLinePtr = framebuffer3D + compInfo.line.blockOffsetCustom;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	// Horizontally offset the 3D layer by this amount.
	// Test case: Blowing up large objects in Nanostray 2 will cause the main screen to shake horizontally.
	const u16 hofs = (u16)( ((float)compInfo.renderState.selectedBGLayer->xOffset * customWidthScale) + 0.5f );
	
	if (hofs == 0)
	{
		size_t i = 0;
		
#ifdef ENABLE_SSE2
		const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % 16));
		const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[GPULayerID_BG0];
		
		for (; i < ssePixCount; i+=16, srcLinePtr+=16, compInfo.target.xCustom+=16, compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
		{
			if (compInfo.target.xCustom >= compInfo.line.widthCustom)
			{
				compInfo.target.xCustom -= compInfo.line.widthCustom;
			}
			
			// Determine which pixels pass by doing the window test and the alpha test.
			__m128i passMask8;
			int passMaskValue;
			
			if (WILLPERFORMWINDOWTEST)
			{
				// Do the window test.
				passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[GPULayerID_BG0] + compInfo.target.xCustom)), _mm_set1_epi8(1) );
				
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
			
			const __m128i src[4] = {
				_mm_load_si128((__m128i *)srcLinePtr + 0),
				_mm_load_si128((__m128i *)srcLinePtr + 1),
				_mm_load_si128((__m128i *)srcLinePtr + 2),
				_mm_load_si128((__m128i *)srcLinePtr + 3)
			};
			
			// Do the alpha test. Pixels with an alpha value of 0 are rejected.
			const __m128i srcAlpha = _mm_packs_epi16( _mm_packs_epi32(_mm_srli_epi32(src[0], 24), _mm_srli_epi32(src[1], 24)),
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
			this->_PixelComposite16_SSE2<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D, WILLPERFORMWINDOWTEST>(compInfo,
																											   didAllPixelsPass,
																											   passMask8,
																											   src[3], src[2], src[1], src[0],
																											   srcEffectEnableMask,
																											   this->_enableColorEffectCustom[GPULayerID_BG0] + compInfo.target.xCustom,
																											   NULL,
																											   NULL);
		}
#endif
		
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; i < compInfo.line.pixelCount; i++, srcLinePtr++, compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if (compInfo.target.xCustom >= compInfo.line.widthCustom)
			{
				compInfo.target.xCustom -= compInfo.line.widthCustom;
			}
			
			if ( (srcLinePtr->a == 0) || (WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[GPULayerID_BG0][compInfo.target.xCustom] == 0)) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[GPULayerID_BG0][compInfo.target.xCustom] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D>(compInfo, *srcLinePtr, enableColorEffect, 0, 0);
		}
	}
	else
	{
		for (size_t line = 0; line < compInfo.line.renderCount; line++)
		{
			for (compInfo.target.xCustom = 0; compInfo.target.xCustom < compInfo.line.widthCustom; compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
			{
				if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[GPULayerID_BG0][compInfo.target.xCustom] == 0) )
				{
					continue;
				}
				
				size_t srcX = compInfo.target.xCustom + hofs;
				if (srcX >= compInfo.line.widthCustom * 2)
				{
					srcX -= compInfo.line.widthCustom * 2;
				}
				
				if ( (srcX >= compInfo.line.widthCustom) || (srcLinePtr[srcX].a == 0) )
				{
					continue;
				}
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[GPULayerID_BG0][compInfo.target.xCustom] != 0) : true;
				this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D>(compInfo, srcLinePtr[srcX], enableColorEffect, 0, 0);
			}
			
			srcLinePtr += compInfo.line.widthCustom;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCaptureCustom(const IOREG_DISPCAPCNT &DISPCAPCNT,
												  const GPUEngineLineInfo &lineInfo,
												  const bool isReadDisplayLineNative,
												  const bool isReadVRAMLineNative,
												  const void *srcAPtr,
												  const void *srcBPtr,
												  void *dstCustomPtr)
{
	const size_t captureLengthExt = (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? lineInfo.widthCustom : lineInfo.widthCustom / 2;
	
	switch (DISPCAPCNT.value & 0x63000000)
	{
		case 0x00000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x02000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		{
			if (isReadDisplayLineNative)
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, true, false>(lineInfo, srcAPtr, dstCustomPtr, captureLengthExt);
			}
			else
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, false, false>(lineInfo, srcAPtr, dstCustomPtr, captureLengthExt);
			}
			break;
		}
			
		case 0x01000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x03000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 1, CAPTURELENGTH, false, false>(lineInfo, srcAPtr, dstCustomPtr, captureLengthExt);
			break;
			
		case 0x20000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x21000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		{
			if (isReadVRAMLineNative)
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, true, false>(lineInfo, srcBPtr, dstCustomPtr, captureLengthExt);
			}
			else
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, false, false>(lineInfo, srcBPtr, dstCustomPtr, captureLengthExt);
			}
			break;
		}
			
		case 0x22000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x23000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		{
			if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
			{
				ColorspaceConvertBuffer555To8888Opaque<false, false>(this->_fifoLine16, (u32 *)srcBPtr, GPU_FRAMEBUFFER_NATIVE_WIDTH);
			}
			
			this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 1, CAPTURELENGTH, true, false>(lineInfo, srcBPtr, dstCustomPtr, captureLengthExt);
			break;
		}
			
		case 0x40000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x41000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x42000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x43000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		case 0x60000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x62000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x61000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x63000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		{
			if ((DISPCAPCNT.SrcA == 0) && isReadDisplayLineNative)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					CopyLineExpandHinted<0xFFFF, true, false, false, 2>(lineInfo, srcAPtr, this->_captureWorkingA16);
					srcAPtr = this->_captureWorkingA16;
				}
				else
				{
					CopyLineExpandHinted<0xFFFF, true, false, false, 4>(lineInfo, srcAPtr, this->_captureWorkingA32);
					srcAPtr = this->_captureWorkingA32;
				}
			}
			
			if ((DISPCAPCNT.SrcB != 0) || isReadVRAMLineNative)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					CopyLineExpandHinted<0xFFFF, true, false, false, 2>(lineInfo, srcBPtr, this->_captureWorkingB16);
					srcBPtr = this->_captureWorkingB16;
				}
				else
				{
					if ((OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && (DISPCAPCNT.SrcB != 0))
					{
						ColorspaceConvertBuffer555To8888Opaque<false, false>(this->_fifoLine16, (u32 *)srcBPtr, GPU_FRAMEBUFFER_NATIVE_WIDTH);
					}
					
					CopyLineExpandHinted<0xFFFF, true, false, false, 4>(lineInfo, srcBPtr, this->_captureWorkingB32);
					srcBPtr = this->_captureWorkingB32;
				}
			}
			
			this->_RenderLine_DispCapture_Blend<OUTPUTFORMAT, CAPTURELENGTH, false, false, false>(lineInfo, srcAPtr, srcBPtr, dstCustomPtr, captureLengthExt);
			break;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCapture(const GPUEngineCompositorInfo &compInfo)
{
	assert( (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH/2) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) );
	
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	const size_t writeLineIndexWithOffset = (DISPCAPCNT.VRAMWriteOffset * 64) + compInfo.line.indexNative;
	const size_t readLineIndexWithOffset = (this->_dispCapCnt.readOffset * 64) + compInfo.line.indexNative;
	
	const bool isReadDisplayLineNative = this->isLineRenderNative[compInfo.line.indexNative];
	const bool isRead3DLineNative = CurrentRenderer->IsFramebufferNativeSize();
	const bool isReadVRAMLineNative = this->_isLineCaptureNative[DISPCNT.VRAM_Block][readLineIndexWithOffset];
	
	bool willReadNativeVRAM = isReadVRAMLineNative;
	bool willWriteVRAMLineNative = true;
	bool needCaptureNative = true;
	bool needConvertDisplayLine23 = false;
	bool needConvertDisplayLine32 = false;
	
	//128-wide captures should write linearly into memory, with no gaps
	//this is tested by hotel dusk
	size_t dstNativeOffset = (DISPCAPCNT.VRAMWriteOffset * 64 * GPU_FRAMEBUFFER_NATIVE_WIDTH) + (compInfo.line.indexNative * CAPTURELENGTH);
	
	//Read/Write block wrap to 00000h when exceeding 1FFFFh (128k)
	//this has not been tested yet (I thought I needed it for hotel dusk, but it was fixed by the above)
	dstNativeOffset &= 0x0000FFFF;
	
	// Convert VRAM for native VRAM capture.
	const u16 *vramNative16 = (u16 *)MMU.blank_memory;
	
	if ( (DISPCAPCNT.SrcB == 0) && (DISPCAPCNT.CaptureSrc != 0) && (vramConfiguration.banks[DISPCNT.VRAM_Block].purpose == VramConfiguration::LCDC) )
	{
		size_t vramNativeOffset = readLineIndexWithOffset * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		vramNativeOffset &= 0x0000FFFF;
		vramNative16 = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + vramNativeOffset;
		
		this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, readLineIndexWithOffset);
		
		willReadNativeVRAM = this->_isLineCaptureNative[DISPCNT.VRAM_Block][readLineIndexWithOffset];
	}
	
	switch (DISPCAPCNT.value & 0x63000000)
	{
		case 0x00000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x02000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isReadDisplayLineNative;
			needConvertDisplayLine23 = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev);
			needConvertDisplayLine32 = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev);
			break;
			
		case 0x01000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x03000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isRead3DLineNative;
			break;
			
		case 0x20000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x21000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			willWriteVRAMLineNative = willReadNativeVRAM;
			break;
			
		case 0x22000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x23000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			this->_RenderLine_DispCapture_FIFOToBuffer(this->_fifoLine16);
			willWriteVRAMLineNative = true;
			break;
			
		case 0x40000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x60000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			willWriteVRAMLineNative = (isReadDisplayLineNative && willReadNativeVRAM);
			needConvertDisplayLine23 = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev);
			needConvertDisplayLine32 = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev);
			break;
			
		case 0x42000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x62000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isReadDisplayLineNative;
			needConvertDisplayLine23 = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev);
			needConvertDisplayLine32 = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev);
			this->_RenderLine_DispCapture_FIFOToBuffer(this->_fifoLine16); // fifo - tested by splinter cell chaos theory thermal view
			break;
			
		case 0x41000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x61000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			willWriteVRAMLineNative = (isRead3DLineNative && willReadNativeVRAM);
			break;
			
		case 0x43000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		case 0x63000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isRead3DLineNative;
			this->_RenderLine_DispCapture_FIFOToBuffer(this->_fifoLine16); // fifo - tested by splinter cell chaos theory thermal view
			break;
	}
	
	// Capturing an RGB888 line will always use the custom VRAM buffer.
	willWriteVRAMLineNative = willWriteVRAMLineNative && (OUTPUTFORMAT != NDSColorFormat_BGR888_Rev);
	
	const void *srcAPtr;
	const void *srcBPtr;
	u16 *dstNative16 = this->_VRAMNativeBlockPtr[DISPCAPCNT.VRAMWriteBlock] + dstNativeOffset;
	
	if (!willWriteVRAMLineNative)
	{
		void *dstCustomPtr;
		const size_t captureLengthExt = (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? compInfo.line.widthCustom : compInfo.line.widthCustom / 2;
		const GPUEngineLineInfo &lineInfoBlock = this->_currentCompositorInfo[DISPCAPCNT.VRAMWriteOffset * 64].line;
		
		size_t dstCustomOffset = lineInfoBlock.blockOffsetCustom + (compInfo.line.indexCustom * captureLengthExt);
		while (dstCustomOffset >= _gpuVRAMBlockOffset)
		{
			dstCustomOffset -= _gpuVRAMBlockOffset;
		}
		
		const u16 *vramCustom16 = (u16 *)GPU->GetCustomVRAMBlankBuffer();
		const FragmentColor *vramCustom32 = (FragmentColor *)GPU->GetCustomVRAMBlankBuffer();
		
		if (!willReadNativeVRAM)
		{
			size_t vramCustomOffset = (lineInfoBlock.indexCustom + compInfo.line.indexCustom) * compInfo.line.widthCustom;
			while (vramCustomOffset >= _gpuVRAMBlockOffset)
			{
				vramCustomOffset -= _gpuVRAMBlockOffset;
			}
			
			vramCustom16 = (u16 *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + vramCustomOffset;
			vramCustom32 = (FragmentColor *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + vramCustomOffset;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			if ( (DISPCAPCNT.SrcB == 0) && (DISPCAPCNT.CaptureSrc != 0) && (vramConfiguration.banks[DISPCNT.VRAM_Block].purpose == VramConfiguration::LCDC) )
			{
				if (willReadNativeVRAM)
				{
					ColorspaceConvertBuffer555To8888Opaque<false, false>(vramNative16, (u32 *)vramCustom32, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				}
			}
			
			srcAPtr = (DISPCAPCNT.SrcA == 0) ? (FragmentColor *)compInfo.target.lineColorHead : (FragmentColor *)CurrentRenderer->GetFramebuffer() + compInfo.line.blockOffsetCustom;
			srcBPtr = (DISPCAPCNT.SrcB == 0) ? vramCustom32 : this->_fifoLine32;
			dstCustomPtr = (FragmentColor *)this->_VRAMCustomBlockPtr[DISPCAPCNT.VRAMWriteBlock] + dstCustomOffset;
		}
		else
		{
			const u16 *vramPtr16 = (willReadNativeVRAM) ? vramNative16 : vramCustom16;
			
			srcAPtr = (DISPCAPCNT.SrcA == 0) ? (u16 *)compInfo.target.lineColorHead : this->_3DFramebuffer16 + compInfo.line.blockOffsetCustom;
			srcBPtr = (DISPCAPCNT.SrcB == 0) ? vramPtr16 : this->_fifoLine16;
			dstCustomPtr = (u16 *)this->_VRAMCustomBlockPtr[DISPCAPCNT.VRAMWriteBlock] + dstCustomOffset;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			// Note that although RGB666 colors are 32-bit values, this particular mode uses 16-bit color depth for line captures.
			if (needConvertDisplayLine23)
			{
				ColorspaceConvertBuffer6665To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingDisplay16, compInfo.line.pixelCount);
				srcAPtr = this->_captureWorkingDisplay16;
				needConvertDisplayLine23 = false;
			}
			
			this->_RenderLine_DisplayCaptureCustom<NDSColorFormat_BGR555_Rev, CAPTURELENGTH>(DISPCAPCNT,
																							 compInfo.line,
																							 isReadDisplayLineNative,
																							 (srcBPtr == vramNative16),
																							 srcAPtr,
																							 srcBPtr,
																							 dstCustomPtr);
			
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				CopyLineReduceHinted<0xFFFF, false, false, 2>(compInfo.line, dstCustomPtr, dstNative16);
				needCaptureNative = false;
			}
		}
		else
		{
			this->_RenderLine_DisplayCaptureCustom<OUTPUTFORMAT, CAPTURELENGTH>(DISPCAPCNT,
																				compInfo.line,
																				isReadDisplayLineNative,
																				(srcBPtr == vramNative16),
																				srcAPtr,
																				srcBPtr,
																				dstCustomPtr);
			
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				u32 *dstNative32 = (u32 *)dstCustomPtr;
				
				if (compInfo.line.widthCustom > GPU_FRAMEBUFFER_NATIVE_WIDTH)
				{
					dstNative32 = (u32 *)this->_captureWorkingA32; // We're going to reuse _captureWorkingA32, since we should already be done with it by now.
					CopyLineReduceHinted<0xFFFF, false, false, 4>(compInfo.line, dstCustomPtr, dstNative32);
				}
				
				ColorspaceConvertBuffer8888To5551<false, false>(dstNative32, dstNative16, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				needCaptureNative = false;
			}
		}
	}
	
	if (needCaptureNative)
	{
		srcAPtr = (DISPCAPCNT.SrcA == 0) ? (u16 *)compInfo.target.lineColorHead : this->_3DFramebuffer16 + compInfo.line.blockOffsetCustom;
		srcBPtr = (DISPCAPCNT.SrcB == 0) ? vramNative16 : this->_fifoLine16;
		
		// Convert 18-bit and 24-bit framebuffers to 15-bit for native screen capture.
		if ((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) && needConvertDisplayLine23)
		{
			ColorspaceConvertBuffer6665To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingDisplay16, compInfo.line.pixelCount);
			srcAPtr = this->_captureWorkingDisplay16;
		}
		else if ((OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && needConvertDisplayLine32)
		{
			ColorspaceConvertBuffer8888To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingDisplay16, compInfo.line.pixelCount);
			srcAPtr = this->_captureWorkingDisplay16;
		}
		
		switch (DISPCAPCNT.value & 0x63000000)
		{
			case 0x00000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x02000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			{
				if (isReadDisplayLineNative)
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				else
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				break;
			}
				
			case 0x01000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			case 0x03000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			{
				if (isRead3DLineNative)
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				else
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, false, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				break;
			}
				
			case 0x20000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x21000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
				this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(compInfo.line, srcBPtr, dstNative16, CAPTURELENGTH);
				break;
				
			case 0x22000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			case 0x23000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
				this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(compInfo.line, srcBPtr, dstNative16, CAPTURELENGTH);
				break;
				
			case 0x40000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x41000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			case 0x42000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			case 0x43000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			case 0x60000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x62000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			case 0x61000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			case 0x63000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			{
				if ( ((DISPCAPCNT.SrcA == 0) && isReadDisplayLineNative) || ((DISPCAPCNT.SrcA != 0) && isRead3DLineNative) )
				{
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, true, true, true>(compInfo.line, srcAPtr, srcBPtr, dstNative16, CAPTURELENGTH);
				}
				else
				{
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, false, true, true>(compInfo.line, srcAPtr, srcBPtr, dstNative16, CAPTURELENGTH);
				}
				break;
			}
		}
	}
	
#ifdef ENABLE_SSE2
	MACRODO_N( CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_stream_si128((__m128i *)(this->_VRAMNativeBlockCaptureCopyPtr[DISPCAPCNT.VRAMWriteBlock] + dstNativeOffset) + (X), _mm_load_si128((__m128i *)dstNative16 + (X))) );
#else
	memcpy(this->_VRAMNativeBlockCaptureCopyPtr[DISPCAPCNT.VRAMWriteBlock] + dstNativeOffset, dstNative16, CAPTURELENGTH * sizeof(u16));
#endif
	
	if (this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] && !willWriteVRAMLineNative)
	{
		this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] = false;
		this->_nativeLineCaptureCount[DISPCAPCNT.VRAMWriteBlock]--;
	}
	else if (!this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] && willWriteVRAMLineNative)
	{
		this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] = true;
		this->_nativeLineCaptureCount[DISPCAPCNT.VRAMWriteBlock]++;
	}
}

void GPUEngineA::_RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer)
{
#ifdef ENABLE_SSE2
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
	{
		const u32 srcA = DISP_FIFOrecv();
		const u32 srcB = DISP_FIFOrecv();
		const u32 srcC = DISP_FIFOrecv();
		const u32 srcD = DISP_FIFOrecv();
		const __m128i fifoColor = _mm_setr_epi32(srcA, srcB, srcC, srcD);
		_mm_store_si128((__m128i *)fifoLineBuffer + i, fifoColor);
	}
#else
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
	{
		((u32 *)fifoLineBuffer)[i] = LE_TO_LOCAL_32( DISP_FIFOrecv() );
	}
#endif
}

template<NDSColorFormat COLORFORMAT, int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
void GPUEngineA::_RenderLine_DispCapture_Copy(const GPUEngineLineInfo &lineInfo, const void *src, void *dst, const size_t captureLengthExt)
{
	const u16 alphaBit16 = (SOURCESWITCH == 0) ? 0x8000 : 0x0000;
	const u32 alphaBit32 = (SOURCESWITCH == 0) ? ((COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF000000 : 0x1F000000) : 0x00000000;
	
#ifdef ENABLE_SSE2
	const __m128i alpha_vec128 = (COLORFORMAT == NDSColorFormat_BGR555_Rev) ? _mm_set1_epi16(alphaBit16) : _mm_set1_epi32(alphaBit32);
#endif
	
	if (CAPTURETONATIVEDST)
	{
		if (CAPTUREFROMNATIVESRC)
		{
#ifdef ENABLE_SSE2
			switch (COLORFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					MACRODO_N(CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_store_si128((__m128i *)dst + (X), _mm_or_si128( _mm_load_si128( (__m128i *)src + (X)), alpha_vec128 ) ));
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					MACRODO_N(CAPTURELENGTH / (sizeof(__m128i) / sizeof(u32)), _mm_store_si128((__m128i *)dst + (X), _mm_or_si128( _mm_load_si128( (__m128i *)src + (X)), alpha_vec128 ) ));
					break;
			}
#else
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
						break;
				}
			}
#endif
		}
		else
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[_gpuDstPitchIndex[i]] | alphaBit16);
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[_gpuDstPitchIndex[i]] | alphaBit32);
						break;
				}
			}
		}
	}
	else
	{
		if (CAPTUREFROMNATIVESRC)
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				for (size_t p = 0; p < _gpuDstPitchCount[i]; p++)
				{
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
							((u16 *)dst)[_gpuDstPitchIndex[i] + p] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							break;
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
							((u32 *)dst)[_gpuDstPitchIndex[i] + p] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							break;
					}
				}
			}
			
			for (size_t l = 1; l < lineInfo.renderCount; l++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memcpy((u16 *)dst + (l * lineInfo.widthCustom), dst, captureLengthExt * sizeof(u16));
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						memcpy((u32 *)dst + (l * lineInfo.widthCustom), dst, captureLengthExt * sizeof(u32));
						break;
				}
			}
		}
		else
		{
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				const size_t pixCountExt = captureLengthExt * lineInfo.renderCount;
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
						const size_t ssePixCount = pixCountExt - (pixCountExt % 8);
						for (; i < ssePixCount; i += 8)
						{
							_mm_store_si128((__m128i *)((u16 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u16 *)src + i)), alpha_vec128 ) );
						}
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
						const size_t ssePixCount = pixCountExt - (pixCountExt % 4);
						for (; i < ssePixCount; i += 4)
						{
							_mm_store_si128((__m128i *)((u32 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u32 *)src + i)), alpha_vec128 ) );
						}
						break;
					}
				}
#endif
				
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
				for (; i < pixCountExt; i++)
				{
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
							((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							break;
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
							((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							break;
					}
				}
			}
			else
			{
				for (size_t l = 0; l < lineInfo.renderCount; l++)
				{
					size_t i = 0;
					
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
						{
#ifdef ENABLE_SSE2
							const size_t ssePixCount = captureLengthExt - (captureLengthExt % 8);
							for (; i < ssePixCount; i += 8)
							{
								_mm_store_si128((__m128i *)((u16 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u16 *)src + i)), alpha_vec128 ) );
							}
#endif
							
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
							for (; i < captureLengthExt; i++)
							{
								((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							}
							
							src = (u16 *)src + lineInfo.widthCustom;
							dst = (u16 *)dst + lineInfo.widthCustom;
							break;
						}
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
						{
#ifdef ENABLE_SSE2
							const size_t ssePixCount = captureLengthExt - (captureLengthExt % 4);
							for (; i < ssePixCount; i += 4)
							{
								_mm_store_si128((__m128i *)((u32 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u32 *)src + i)), alpha_vec128 ) );
							}
#endif
							
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
							for (; i < captureLengthExt; i++)
							{
								((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							}
							
							src = (u32 *)src + lineInfo.widthCustom;
							dst = (u32 *)dst + lineInfo.widthCustom;
							break;
						}
					}
				}
			}
		}
	}
}

u16 GPUEngineA::_RenderLine_DispCapture_BlendFunc(const u16 srcA, const u16 srcB, const u8 blendEVA, const u8 blendEVB)
{
	u16 a = 0;
	u16 r = 0;
	u16 g = 0;
	u16 b = 0;
	u16 a_alpha = srcA & 0x8000;
	u16 b_alpha = srcB & 0x8000;
	
	if (a_alpha)
	{
		a = 0x8000;
		r =  ((srcA        & 0x001F) * blendEVA);
		g = (((srcA >>  5) & 0x001F) * blendEVA);
		b = (((srcA >> 10) & 0x001F) * blendEVA);
	}
	
	if (b_alpha)
	{
		a = 0x8000;
		r +=  ((srcB        & 0x001F) * blendEVB);
		g += (((srcB >>  5) & 0x001F) * blendEVB);
		b += (((srcB >> 10) & 0x001F) * blendEVB);
	}
	
	r >>= 4;
	g >>= 4;
	b >>= 4;
	
	//freedom wings sky will overflow while doing some fsaa/motionblur effect without this
	r = (r > 31) ? 31 : r;
	g = (g > 31) ? 31 : g;
	b = (b > 31) ? 31 : b;
	
	return LOCAL_TO_LE_16(a | (b << 10) | (g << 5) | r);
}

template<NDSColorFormat COLORFORMAT>
FragmentColor GPUEngineA::_RenderLine_DispCapture_BlendFunc(const FragmentColor srcA, const FragmentColor srcB, const u8 blendEVA, const u8 blendEVB)
{
	FragmentColor outColor;
	outColor.color = 0;
	
	u16 r = 0;
	u16 g = 0;
	u16 b = 0;
	
	if (srcA.a > 0)
	{
		outColor.a  = (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
		r  = srcA.r * blendEVA;
		g  = srcA.g * blendEVA;
		b  = srcA.b * blendEVA;
	}
	
	if (srcB.a > 0)
	{
		outColor.a  = (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
		r += srcB.r * blendEVB;
		g += srcB.g * blendEVB;
		b += srcB.b * blendEVB;
	}
	
	r >>= 4;
	g >>= 4;
	b >>= 4;
	
	//freedom wings sky will overflow while doing some fsaa/motionblur effect without this
	if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		outColor.r = (r > 255) ? 255 : r;
		outColor.g = (g > 255) ? 255 : g;
		outColor.b = (b > 255) ? 255 : b;
	}
	else
	{
		outColor.r = (r > 63) ? 63 : r;
		outColor.g = (g > 63) ? 63 : g;
		outColor.b = (b > 63) ? 63 : b;
	}
	
	return outColor;
}

#ifdef ENABLE_SSE2
template <NDSColorFormat COLORFORMAT>
__m128i GPUEngineA::_RenderLine_DispCapture_BlendFunc_SSE2(const __m128i &srcA, const __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB)
{
#ifdef ENABLE_SSSE3
	__m128i blendAB = _mm_or_si128( blendEVA, _mm_slli_epi16(blendEVB, 8) );
#endif
	
	switch (COLORFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
		{
			__m128i srcA_alpha = _mm_and_si128(srcA, _mm_set1_epi16(0x8000));
			__m128i srcB_alpha = _mm_and_si128(srcB, _mm_set1_epi16(0x8000));
			__m128i srcA_masked = _mm_andnot_si128( _mm_cmpeq_epi16(srcA_alpha, _mm_setzero_si128()), srcA );
			__m128i srcB_masked = _mm_andnot_si128( _mm_cmpeq_epi16(srcB_alpha, _mm_setzero_si128()), srcB );
			__m128i colorBitMask = _mm_set1_epi16(0x001F);
			
			__m128i ra;
			__m128i ga;
			__m128i ba;
			
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
			
			__m128i rb = _mm_and_si128(               srcB_masked,      colorBitMask);
			__m128i gb = _mm_and_si128(_mm_srli_epi16(srcB_masked,  5), colorBitMask);
			__m128i bb = _mm_and_si128(_mm_srli_epi16(srcB_masked, 10), colorBitMask);
			
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
			
			return _mm_or_si128( _mm_or_si128(_mm_or_si128(ra, _mm_slli_epi16(ga,  5)), _mm_slli_epi16(ba, 10)), _mm_or_si128(srcA_alpha, srcB_alpha) );
		}
			
		case NDSColorFormat_BGR666_Rev:
		case NDSColorFormat_BGR888_Rev:
		{
			// Get color masks based on if the alpha value is 0. Colors with an alpha value
			// equal to 0 are rejected.
			__m128i srcA_alpha = _mm_and_si128(srcA, _mm_set1_epi32(0xFF000000));
			__m128i srcB_alpha = _mm_and_si128(srcB, _mm_set1_epi32(0xFF000000));
			__m128i srcA_masked = _mm_andnot_si128(_mm_cmpeq_epi32(srcA_alpha, _mm_setzero_si128()), srcA);
			__m128i srcB_masked = _mm_andnot_si128(_mm_cmpeq_epi32(srcB_alpha, _mm_setzero_si128()), srcB);
			
			__m128i outColorLo;
			__m128i outColorHi;
			__m128i outColor;
			
			// Temporarily convert the color component values from 8-bit to 16-bit, and then
			// do the blend calculation.
#ifdef ENABLE_SSSE3
			outColorLo = _mm_unpacklo_epi8(srcA_masked, srcB_masked);
			outColorHi = _mm_unpackhi_epi8(srcA_masked, srcB_masked);
			
			outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
			outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
#else
			__m128i srcA_maskedLo = _mm_unpacklo_epi8(srcA_masked, _mm_setzero_si128());
			__m128i srcA_maskedHi = _mm_unpackhi_epi8(srcA_masked, _mm_setzero_si128());
			__m128i srcB_maskedLo = _mm_unpacklo_epi8(srcB_masked, _mm_setzero_si128());
			__m128i srcB_maskedHi = _mm_unpackhi_epi8(srcB_masked, _mm_setzero_si128());
			
			outColorLo = _mm_add_epi16( _mm_mullo_epi16(srcA_maskedLo, blendEVA), _mm_mullo_epi16(srcB_maskedLo, blendEVB) );
			outColorHi = _mm_add_epi16( _mm_mullo_epi16(srcA_maskedHi, blendEVA), _mm_mullo_epi16(srcB_maskedHi, blendEVB) );
#endif
			
			outColorLo = _mm_srli_epi16(outColorLo, 4);
			outColorHi = _mm_srli_epi16(outColorHi, 4);
			
			// Convert the color components back from 16-bit to 8-bit using a saturated pack.
			outColor = _mm_packus_epi16(outColorLo, outColorHi);
			
			// When the color format is 8888, the packuswb instruction will naturally clamp
			// the color component values to 255. However, when the color format is 6665, the
			// color component values must be clamped to 63. In this case, we must call pminub
			// to do the clamp.
			if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
			{
				outColor = _mm_min_epu8(outColor, _mm_set1_epi8(63));
			}
			
			// Add the alpha components back in.
			outColor = _mm_and_si128(outColor, _mm_set1_epi32(0x00FFFFFF));
			outColor = _mm_or_si128(outColor, srcA_alpha);
			outColor = _mm_or_si128(outColor, srcB_alpha);
			
			return outColor;
		}
	}
}
#endif

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_RenderLine_DispCapture_BlendToCustomDstBuffer(const void *srcA, const void *srcB, void *dst, const u8 blendEVA, const u8 blendEVB, const size_t length)
{
#ifdef ENABLE_SSE2
	const __m128i blendEVA_vec128 = _mm_set1_epi16(blendEVA);
	const __m128i blendEVB_vec128 = _mm_set1_epi16(blendEVB);
#endif
	
	size_t i = 0;
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
	{
		const FragmentColor *srcA_32 = (const FragmentColor *)srcA;
		const FragmentColor *srcB_32 = (const FragmentColor *)srcB;
		FragmentColor *dst32 = (FragmentColor *)dst;
		
#ifdef ENABLE_SSE2
		const size_t ssePixCount = length - (length % 4);
		for (; i < ssePixCount; i+=4)
		{
			const __m128i srcA_vec128 = _mm_load_si128((__m128i *)(srcA_32 + i));
			const __m128i srcB_vec128 = _mm_load_si128((__m128i *)(srcB_32 + i));
			
			_mm_store_si128( (__m128i *)(dst32 + i), this->_RenderLine_DispCapture_BlendFunc_SSE2<OUTPUTFORMAT>(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
		}
#endif
		
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; i < length; i++)
		{
			const FragmentColor colorA = srcA_32[i];
			const FragmentColor colorB = srcB_32[i];
			
			dst32[i] = this->_RenderLine_DispCapture_BlendFunc<OUTPUTFORMAT>(colorA, colorB, blendEVA, blendEVB);
		}
	}
	else
	{
		const u16 *srcA_16 = (const u16 *)srcA;
		const u16 *srcB_16 = (const u16 *)srcB;
		u16 *dst16 = (u16 *)dst;
		
#ifdef ENABLE_SSE2
		const size_t ssePixCount = length - (length % 8);
		for (; i < ssePixCount; i+=8)
		{
			const __m128i srcA_vec128 = _mm_load_si128((__m128i *)(srcA_16 + i));
			const __m128i srcB_vec128 = _mm_load_si128((__m128i *)(srcB_16 + i));
			
			_mm_store_si128( (__m128i *)(dst16 + i), this->_RenderLine_DispCapture_BlendFunc_SSE2<NDSColorFormat_BGR555_Rev>(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
		}
#endif
		
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; i < length; i++)
		{
			const u16 colorA = srcA_16[i];
			const u16 colorB = srcB_16[i];
			
			dst16[i] = this->_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB, bool CAPTURETONATIVEDST>
void GPUEngineA::_RenderLine_DispCapture_Blend(const GPUEngineLineInfo &lineInfo, const void *srcA, const void *srcB, void *dst, const size_t captureLengthExt)
{
	const u8 blendEVA = this->_dispCapCnt.EVA;
	const u8 blendEVB = this->_dispCapCnt.EVB;
	
	if (CAPTURETONATIVEDST)
	{
#ifdef ENABLE_SSE2
		const __m128i blendEVA_vec128 = _mm_set1_epi16(blendEVA);
		const __m128i blendEVB_vec128 = _mm_set1_epi16(blendEVB);
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			const u32 *srcA_32 = (const u32 *)srcA;
			const u32 *srcB_32 = (const u32 *)srcB;
			FragmentColor *dst32 = (FragmentColor *)dst;
			
			for (size_t i = 0; i < CAPTURELENGTH; i+=4)
			{
				__m128i srcA_vec128 = (CAPTUREFROMNATIVESRCA) ? _mm_load_si128((__m128i *)(srcA_32 + i)) : _mm_set_epi32(srcA_32[_gpuDstPitchIndex[i+3]],
																														 srcA_32[_gpuDstPitchIndex[i+2]],
																														 srcA_32[_gpuDstPitchIndex[i+1]],
																														 srcA_32[_gpuDstPitchIndex[i+0]]);
				
				__m128i srcB_vec128 = (CAPTUREFROMNATIVESRCB) ? _mm_load_si128((__m128i *)(srcB_32 + i)) : _mm_set_epi32(srcB_32[_gpuDstPitchIndex[i+3]],
																														 srcB_32[_gpuDstPitchIndex[i+2]],
																														 srcB_32[_gpuDstPitchIndex[i+1]],
																														 srcB_32[_gpuDstPitchIndex[i+0]]);
				
				_mm_store_si128( (__m128i *)(dst32 + i), this->_RenderLine_DispCapture_BlendFunc_SSE2<OUTPUTFORMAT>(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
			}
		}
		else
		{
			const u16 *srcA_16 = (const u16 *)srcA;
			const u16 *srcB_16 = (const u16 *)srcB;
			u16 *dst16 = (u16 *)dst;
			
			for (size_t i = 0; i < CAPTURELENGTH; i+=8)
			{
				__m128i srcA_vec128 = (CAPTUREFROMNATIVESRCA) ? _mm_load_si128((__m128i *)(srcA_16 + i)) : _mm_set_epi16(srcA_16[_gpuDstPitchIndex[i+7]],
																														 srcA_16[_gpuDstPitchIndex[i+6]],
																														 srcA_16[_gpuDstPitchIndex[i+5]],
																														 srcA_16[_gpuDstPitchIndex[i+4]],
																														 srcA_16[_gpuDstPitchIndex[i+3]],
																														 srcA_16[_gpuDstPitchIndex[i+2]],
																														 srcA_16[_gpuDstPitchIndex[i+1]],
																														 srcA_16[_gpuDstPitchIndex[i+0]]);
				
				__m128i srcB_vec128 = (CAPTUREFROMNATIVESRCB) ? _mm_load_si128((__m128i *)(srcB_16 + i)) : _mm_set_epi16(srcB_16[_gpuDstPitchIndex[i+7]],
																														 srcB_16[_gpuDstPitchIndex[i+6]],
																														 srcB_16[_gpuDstPitchIndex[i+5]],
																														 srcB_16[_gpuDstPitchIndex[i+4]],
																														 srcB_16[_gpuDstPitchIndex[i+3]],
																														 srcB_16[_gpuDstPitchIndex[i+2]],
																														 srcB_16[_gpuDstPitchIndex[i+1]],
																														 srcB_16[_gpuDstPitchIndex[i+0]]);
				
				_mm_store_si128( (__m128i *)(dst16 + i), this->_RenderLine_DispCapture_BlendFunc_SSE2<NDSColorFormat_BGR555_Rev>(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
			}
		}
#else
		for (size_t i = 0; i < CAPTURELENGTH; i++)
		{
			if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
			{
				const FragmentColor colorA = (CAPTUREFROMNATIVESRCA) ? ((const FragmentColor *)srcA)[i] : ((const FragmentColor *)srcA)[_gpuDstPitchIndex[i]];
				const FragmentColor colorB = (CAPTUREFROMNATIVESRCB) ? ((const FragmentColor *)srcB)[i] : ((const FragmentColor *)srcB)[_gpuDstPitchIndex[i]];
				
				((FragmentColor *)dst)[i] = this->_RenderLine_DispCapture_BlendFunc<OUTPUTFORMAT>(colorA, colorB, blendEVA, blendEVB);
			}
			else
			{
				const u16 colorA = (CAPTUREFROMNATIVESRCA) ? ((u16 *)srcA)[i] : ((u16 *)srcA)[_gpuDstPitchIndex[i]];
				const u16 colorB = (CAPTUREFROMNATIVESRCB) ? ((u16 *)srcB)[i] : ((u16 *)srcB)[_gpuDstPitchIndex[i]];
				
				((u16 *)dst)[i] = this->_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
			}
		}
#endif
	}
	else
	{
		if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			this->_RenderLine_DispCapture_BlendToCustomDstBuffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt * lineInfo.renderCount);
		}
		else
		{
			for (size_t line = 0; line < lineInfo.renderCount; line++)
			{
				this->_RenderLine_DispCapture_BlendToCustomDstBuffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt);
				srcA = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)srcA + lineInfo.widthCustom) : (void *)((u16 *)srcA + lineInfo.widthCustom);
				srcB = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)srcB + lineInfo.widthCustom) : (void *)((u16 *)srcB + lineInfo.widthCustom);
				dst = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)dst + lineInfo.widthCustom) : (void *)((u16 *)dst + lineInfo.widthCustom);
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeVRAM(const GPUEngineLineInfo &lineInfo)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, lineInfo.indexNative);
	
	if (this->_isLineCaptureNative[DISPCNT.VRAM_Block][lineInfo.indexNative])
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				CopyLineExpandHinted<1, true, true, true, 2>(lineInfo, this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block], this->_nativeBuffer);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + lineInfo.blockOffsetNative;
				u32 *dst = (u32 *)this->_nativeBuffer + lineInfo.blockOffsetNative;
				ColorspaceConvertBuffer555To6665Opaque<false, false>(src, dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				const u16 *src = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + lineInfo.blockOffsetNative;
				u32 *dst = (u32 *)this->_nativeBuffer + lineInfo.blockOffsetNative;
				ColorspaceConvertBuffer555To8888Opaque<false, false>(src, dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				break;
			}
		}
	}
	else
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				CopyLineExpandHinted<0, true, true, true, 2>(lineInfo, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], this->_customBuffer);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = (u16 *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + lineInfo.blockOffsetCustom;
				u32 *dst = (u32 *)this->_customBuffer + lineInfo.blockOffsetCustom;
				ColorspaceConvertBuffer555To6665Opaque<false, false>(src, dst, lineInfo.pixelCount);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				if (GPU->GetDisplayInfo().isCustomSizeRequested)
				{
					CopyLineExpandHinted<0, true, true, true, 4>(lineInfo, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], this->_customBuffer);
				}
				else
				{
					CopyLineExpandHinted<1, true, true, true, 4>(lineInfo, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], this->_nativeBuffer);
				}
				break;
			}
		}
		
		if ((OUTPUTFORMAT != NDSColorFormat_BGR888_Rev) || GPU->GetDisplayInfo().isCustomSizeRequested)
		{
			this->isLineOutputNative[lineInfo.indexNative] = false;
			this->nativeLineOutputCount--;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeMainMemory(const GPUEngineLineInfo &lineInfo)
{
	// Displays video using color data directly read from main memory.
	// Doing this should always result in an output line that is at the native size (192px x 1px).
	
#ifdef ENABLE_SSE2
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
		{
			u32 *__restrict dst = (u32 *__restrict)((u16 *)this->_nativeBuffer + (lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH));
			const __m128i alphaBit = _mm_set1_epi16(0x8000);
			
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
			{
				const u32 srcA = DISP_FIFOrecv();
				const u32 srcB = DISP_FIFOrecv();
				const u32 srcC = DISP_FIFOrecv();
				const u32 srcD = DISP_FIFOrecv();
				const __m128i fifoColor = _mm_setr_epi32(srcA, srcB, srcC, srcD);
				_mm_store_si128((__m128i *)dst + i, _mm_or_si128(fifoColor, alphaBit));
			}
			break;
		}
			
		case NDSColorFormat_BGR666_Rev:
		case NDSColorFormat_BGR888_Rev:
		{
			FragmentColor *__restrict dst = (FragmentColor *__restrict)this->_nativeBuffer + (lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH);
			__m128i dstLo;
			__m128i dstHi;
			
			for (size_t i = 0, d = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++, d+=2)
			{
				const u32 srcA = DISP_FIFOrecv();
				const u32 srcB = DISP_FIFOrecv();
				const u32 srcC = DISP_FIFOrecv();
				const u32 srcD = DISP_FIFOrecv();
				const __m128i fifoColor = _mm_setr_epi32(srcA, srcB, srcC, srcD);
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
				{
					ColorspaceConvert555To6665Opaque_SSE2<false>(fifoColor, dstLo, dstHi);
				}
				else if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
				{
					ColorspaceConvert555To8888Opaque_SSE2<false>(fifoColor, dstLo, dstHi);
				}
				
				_mm_store_si128((__m128i *)dst + d + 0, dstLo);
				_mm_store_si128((__m128i *)dst + d + 1, dstHi);
			}
			break;
		}
	}
#else
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
		{
			u32 *__restrict dst = (u32 *__restrict)((u16 *)this->_nativeBuffer + (lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH));
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
			{
				const u32 src = DISP_FIFOrecv();
#ifdef MSB_FIRST
				dst[i] = (src >> 16) | (src << 16) | 0x80008000;
#else
				dst[i] = src | 0x80008000;
#endif
			}
			break;
		}
			
		case NDSColorFormat_BGR666_Rev:
		{
			FragmentColor *__restrict dst = (FragmentColor *__restrict)this->_nativeBuffer + (lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH);
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				const u32 src = DISP_FIFOrecv();
				dst[i+0].color = COLOR555TO6665_OPAQUE((src >>  0) & 0x7FFF);
				dst[i+1].color = COLOR555TO6665_OPAQUE((src >> 16) & 0x7FFF);
			}
			break;
		}
			
		case NDSColorFormat_BGR888_Rev:
		{
			FragmentColor *__restrict dst = (FragmentColor *__restrict)this->_nativeBuffer + (lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH);
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				const u32 src = DISP_FIFOrecv();
				dst[i+0].color = COLOR555TO8888_OPAQUE((src >>  0) & 0x7FFF);
				dst[i+1].color = COLOR555TO8888_OPAQUE((src >> 16) & 0x7FFF);
			}
			break;
		}
	}
#endif
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineA::_LineLarge8bpp(GPUEngineCompositorInfo &compInfo)
{
	u16 XBG = compInfo.renderState.selectedBGLayer->xOffset;
	u16 YBG = compInfo.line.indexNative + compInfo.renderState.selectedBGLayer->yOffset;
	u16 lg = compInfo.renderState.selectedBGLayer->size.width;
	u16 ht = compInfo.renderState.selectedBGLayer->size.height;
	u16 wmask = (lg-1);
	u16 hmask = (ht-1);
	YBG &= hmask;
	
	//TODO - handle wrapping / out of bounds correctly from rot_scale_op?
	
	u32 tmp_map = compInfo.renderState.selectedBGLayer->largeBMPAddress + lg * YBG;
	u8 *__restrict map = (u8 *)MMU_gpu_map(tmp_map);
	
	for (size_t x = 0; x < lg; ++x, ++XBG)
	{
		XBG &= wmask;
		
		if (WILLDEFERCOMPOSITING)
		{
			this->_deferredIndexNative[x] = map[XBG];
			this->_deferredColorNative[x] = LE_TO_LOCAL_16(this->_paletteBG[this->_deferredIndexNative[x]]);
		}
		else
		{
			const u8 index = map[XBG];
			const u16 color = LE_TO_LOCAL_16(this->_paletteBG[index]);
			this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (color != 0));
		}
	}
}

void GPUEngineA::LastLineProcess()
{
	this->GPUEngineBase::LastLineProcess();
	DISP_FIFOreset();
}

GPUEngineB::GPUEngineB()
{
	_engineID = GPUEngineID_Sub;
	_targetDisplayID = NDSDisplayID_Touch;
	_IORegisterMap = (GPU_IOREG *)(&MMU.ARM9_REG[REG_DISPB]);
	_paletteBG = (u16 *)(MMU.ARM9_VMEM + ADDRESS_STEP_1KB);
	_paletteOBJ = (u16 *)(MMU.ARM9_VMEM + ADDRESS_STEP_1KB + ADDRESS_STEP_512B);
	_oamList = (OAMAttributes *)(MMU.ARM9_OAM + ADDRESS_STEP_1KB);
	_sprMem = MMU_BOBJ;
}

GPUEngineB::~GPUEngineB()
{
}

GPUEngineB* GPUEngineB::Allocate()
{
	return new(malloc_aligned64(sizeof(GPUEngineB))) GPUEngineB();
}

void GPUEngineB::FinalizeAndDeallocate()
{
	this->~GPUEngineB();
	free_aligned(this);
}

void GPUEngineB::Reset()
{
	this->_Reset_Base();
	
	this->_BGLayer[GPULayerID_BG0].BMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].BMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].BMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].BMPAddress = MMU_BBG;
	
	this->_BGLayer[GPULayerID_BG0].largeBMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].largeBMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].largeBMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].largeBMPAddress = MMU_BBG;
	
	this->_BGLayer[GPULayerID_BG0].tileMapAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].tileMapAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].tileMapAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].tileMapAddress = MMU_BBG;
	
	this->_BGLayer[GPULayerID_BG0].tileEntryAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].tileEntryAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].tileEntryAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].tileEntryAddress = MMU_BBG;
	
	this->SetTargetDisplayByID(NDSDisplayID_Touch);
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineB::RenderLine(const size_t l)
{
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	
	switch (compInfo.renderState.displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff<OUTPUTFORMAT>(l);
			break;
		
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
		{
			if (compInfo.renderState.isAnyWindowEnabled)
			{
				this->_RenderLine_Layers<OUTPUTFORMAT, true>(compInfo);
			}
			else
			{
				this->_RenderLine_Layers<OUTPUTFORMAT, false>(compInfo);
			}
			
			this->_HandleDisplayModeNormal<OUTPUTFORMAT>(l);
			break;
		}
			
		default:
			break;
	}
	
	if (compInfo.line.indexNative >= 191)
	{
		this->RenderLineClearAsyncFinish();
	}
}

GPUSubsystem::GPUSubsystem()
{
	ColorspaceHandlerInit();
	
	_defaultEventHandler = new GPUEventHandlerDefault;
	_event = _defaultEventHandler;
	
	gfx3d_init();
	
	for (size_t line = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		GPUEngineLineInfo &lineInfo = this->_lineInfo[line];
		
		lineInfo.indexNative = line;
		lineInfo.indexCustom = lineInfo.indexNative;
		lineInfo.widthCustom = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.renderCount = 1;
		lineInfo.pixelCount = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetNative = lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetCustom = lineInfo.indexCustom * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	}
	
	_engineMain = GPUEngineA::Allocate();
	_engineSub = GPUEngineB::Allocate();
	
	_display[NDSDisplayID_Main] = new NDSDisplay(NDSDisplayID_Main);
	_display[NDSDisplayID_Main]->SetEngine(_engineMain);
	_display[NDSDisplayID_Touch] = new NDSDisplay(NDSDisplayID_Touch);
	_display[NDSDisplayID_Touch]->SetEngine(_engineSub);
	
	if (CommonSettings.num_cores > 1)
	{
		_asyncEngineBufferSetupTask = new Task;
		_asyncEngineBufferSetupTask->start(false);
	}
	else
	{
		_asyncEngineBufferSetupTask = NULL;
	}
	
	_asyncEngineBufferSetupIsRunning = false;
	
	_pending3DRendererID = RENDERID_NULL;
	_needChange3DRenderer = false;
	
	_videoFrameIndex = 0;
	_render3DFrameCount = 0;
	_frameNeedsFinish = false;
	_willFrameSkip = false;
	_willPostprocessDisplays = true;
	_willAutoResolveToCustomBuffer = true;
	
	//TODO OSD
	//OSDCLASS *previousOSD = osd;
	//osd = new OSDCLASS(-1);
	//delete previousOSD;
	
	_customVRAM = NULL;
	_customVRAMBlank = NULL;
	
	_displayInfo.colorFormat = NDSColorFormat_BGR555_Rev;
	_displayInfo.pixelBytes = sizeof(u16);
	_displayInfo.isCustomSizeRequested = false;
	_displayInfo.customWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.customHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_displayInfo.framebufferPageSize = ((GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT) + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT)) * 2 * _displayInfo.pixelBytes;
	_displayInfo.framebufferPageCount = 1;
	_masterFramebuffer = malloc_alignedPage(_displayInfo.framebufferPageSize * _displayInfo.framebufferPageCount);
	_displayInfo.masterFramebufferHead = _masterFramebuffer;
	
	_displayInfo.isDisplayEnabled[NDSDisplayID_Main]  = true;
	_displayInfo.isDisplayEnabled[NDSDisplayID_Touch] = true;
	
	_displayInfo.bufferIndex = 0;
	_displayInfo.sequenceNumber = 0;
	_displayInfo.masterNativeBuffer = _masterFramebuffer;
	_displayInfo.masterCustomBuffer = (u8 *)_masterFramebuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * _displayInfo.pixelBytes);
	
	_displayInfo.nativeBuffer[NDSDisplayID_Main]  = _displayInfo.masterNativeBuffer;
	_displayInfo.nativeBuffer[NDSDisplayID_Touch] = (u8 *)_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * _displayInfo.pixelBytes);
	_displayInfo.customBuffer[NDSDisplayID_Main]  = _displayInfo.masterCustomBuffer;
	_displayInfo.customBuffer[NDSDisplayID_Touch] = (u8 *)_displayInfo.masterCustomBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * _displayInfo.pixelBytes);
	
	_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_displayInfo.renderedBuffer[NDSDisplayID_Main]  = _displayInfo.nativeBuffer[NDSDisplayID_Main];
	_displayInfo.renderedBuffer[NDSDisplayID_Touch] = _displayInfo.nativeBuffer[NDSDisplayID_Touch];
	
	_displayInfo.engineID[NDSDisplayID_Main]  = GPUEngineID_Main;
	_displayInfo.engineID[NDSDisplayID_Touch] = GPUEngineID_Sub;
	
	_displayInfo.didPerformCustomRender[NDSDisplayID_Main]  = false;
	_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	
	_displayInfo.masterBrightnessDiffersPerLine[NDSDisplayID_Main]  = false;
	_displayInfo.masterBrightnessDiffersPerLine[NDSDisplayID_Touch] = false;
	memset(_displayInfo.masterBrightnessMode[NDSDisplayID_Main],  GPUMasterBrightMode_Disable, sizeof(_displayInfo.masterBrightnessMode[NDSDisplayID_Main]));
	memset(_displayInfo.masterBrightnessMode[NDSDisplayID_Touch], GPUMasterBrightMode_Disable, sizeof(_displayInfo.masterBrightnessMode[NDSDisplayID_Touch]));
	memset(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Main],  0, sizeof(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Main]));
	memset(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Touch], 0, sizeof(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Touch]));
	
	_displayInfo.backlightIntensity[NDSDisplayID_Main]  = 1.0f;
	_displayInfo.backlightIntensity[NDSDisplayID_Touch] = 1.0f;
	_displayInfo.needConvertColorFormat[NDSDisplayID_Main]  = false;
	_displayInfo.needConvertColorFormat[NDSDisplayID_Touch] = false;
	_displayInfo.needApplyMasterBrightness[NDSDisplayID_Main]  = false;
	_displayInfo.needApplyMasterBrightness[NDSDisplayID_Touch] = false;
	
	ClearWithColor(0x8000);
}

GPUSubsystem::~GPUSubsystem()
{
	//TODO OSD
	//delete osd;
	//osd = NULL;
	
	if (this->_asyncEngineBufferSetupTask != NULL)
	{
		this->AsyncSetupEngineBuffersFinish();
		delete this->_asyncEngineBufferSetupTask;
		this->_asyncEngineBufferSetupTask = NULL;
	}
	
	free_aligned(this->_masterFramebuffer);
	free_aligned(this->_customVRAM);
	
	free_aligned(_gpuDstToSrcIndex);
	_gpuDstToSrcIndex = NULL;
	
	free_aligned(_gpuDstToSrcSSSE3_u8_8e);
	_gpuDstToSrcSSSE3_u8_8e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u8_16e);
	_gpuDstToSrcSSSE3_u8_16e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u16_8e);
	_gpuDstToSrcSSSE3_u16_8e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u32_4e);
	_gpuDstToSrcSSSE3_u32_4e = NULL;
	
	delete _display[NDSDisplayID_Main];
	delete _display[NDSDisplayID_Touch];
	_engineMain->FinalizeAndDeallocate();
	_engineSub->FinalizeAndDeallocate();
	
	gfx3d_deinit();
	
	delete _defaultEventHandler;
}

void GPUSubsystem::_UpdateFPSRender3D()
{
	this->_videoFrameIndex++;
	if (this->_videoFrameIndex == 60)
	{
		this->_render3DFrameCount = gfx3d.render3DFrameCount;
		gfx3d.render3DFrameCount = 0;
		this->_videoFrameIndex = 0;
	}
}

void GPUSubsystem::SetEventHandler(GPUEventHandler *eventHandler)
{
	this->_event = eventHandler;
}

GPUEventHandler* GPUSubsystem::GetEventHandler()
{
	return this->_event;
}

void GPUSubsystem::Reset()
{
	this->_engineMain->RenderLineClearAsyncFinish();
	this->_engineSub->RenderLineClearAsyncFinish();
	this->AsyncSetupEngineBuffersFinish();
	
	if (this->_customVRAM == NULL)
	{
		this->SetCustomFramebufferSize(this->_displayInfo.customWidth, this->_displayInfo.customHeight);
	}
	
	this->_willFrameSkip = false;
	this->_videoFrameIndex = 0;
	this->_render3DFrameCount = 0;
	this->_backlightIntensityTotal[NDSDisplayID_Main]  = 0.0f;
	this->_backlightIntensityTotal[NDSDisplayID_Touch] = 0.0f;
	
	this->ClearWithColor(0xFFFF);
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main]  = false;
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterNativeBuffer;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterCustomBuffer + (this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes);
	this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	
	this->_displayInfo.engineID[NDSDisplayID_Main] = GPUEngineID_Main;
	this->_displayInfo.engineID[NDSDisplayID_Touch] = GPUEngineID_Sub;
	
	this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = 1.0f;
	this->_displayInfo.backlightIntensity[NDSDisplayID_Touch] = 1.0f;
	
	this->_display[NDSDisplayID_Main]->SetEngineByID(GPUEngineID_Main);
	this->_display[NDSDisplayID_Touch]->SetEngineByID(GPUEngineID_Sub);
	
	gfx3d_reset();
	this->_engineMain->Reset();
	this->_engineSub->Reset();
	
	DISP_FIFOreset();

	//historically, we reset the OSD here. maybe because we would want a clean drawing surface? anyway this is not the right point to be doing OSD work
	//osd->clear();
}

void GPUSubsystem::ForceRender3DFinishAndFlush(bool willFlush)
{
	CurrentRenderer->RenderFinish();
	CurrentRenderer->RenderFlush(willFlush, willFlush);
}

void GPUSubsystem::ForceFrameStop()
{
	if (CurrentRenderer->GetRenderNeedsFinish())
	{
		this->ForceRender3DFinishAndFlush(true);
		CurrentRenderer->SetRenderNeedsFinish(false);
		this->_event->DidRender3DEnd();
	}
	
	if (this->_frameNeedsFinish)
	{
		this->_frameNeedsFinish = false;
		this->_displayInfo.sequenceNumber++;
		this->_event->DidFrameEnd(this->_willFrameSkip, this->_displayInfo);
	}
}

bool GPUSubsystem::GetWillFrameSkip() const
{
	return this->_willFrameSkip;
}

void GPUSubsystem::SetWillFrameSkip(const bool willFrameSkip)
{
	this->_willFrameSkip = willFrameSkip;
}

void GPUSubsystem::SetDisplayCaptureEnable()
{
	this->_engineMain->SetDisplayCaptureEnable();
}

void GPUSubsystem::ResetDisplayCaptureEnable()
{
	this->_engineMain->ResetDisplayCaptureEnable();
}

void GPUSubsystem::UpdateRenderProperties()
{
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes;
	const size_t customFramebufferSize = this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes;
	
	this->_displayInfo.masterNativeBuffer = (u8 *)this->_masterFramebuffer + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize);
	this->_displayInfo.masterCustomBuffer = (u8 *)this->_masterFramebuffer + (nativeFramebufferSize * 2) + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize);
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterNativeBuffer;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterNativeBuffer + nativeFramebufferSize;
	this->_displayInfo.customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterCustomBuffer + customFramebufferSize;
	
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
	this->_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = false;
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	
	void *nativeBufferA  = (this->_engineMain->GetTargetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.nativeBuffer[NDSDisplayID_Main] : this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	void *customBufferA  = (this->_engineMain->GetTargetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.customBuffer[NDSDisplayID_Main] : this->_displayInfo.customBuffer[NDSDisplayID_Touch];
	void *nativeBufferB = (this->_engineSub->GetTargetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.nativeBuffer[NDSDisplayID_Main] : this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	void *customBufferB = (this->_engineSub->GetTargetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.customBuffer[NDSDisplayID_Main] : this->_displayInfo.customBuffer[NDSDisplayID_Touch];
	
	this->_engineMain->SetupRenderStates(nativeBufferA, customBufferA);
	this->_engineSub->SetupRenderStates(nativeBufferB, customBufferB);
	
	if (!this->_displayInfo.isCustomSizeRequested && (this->_displayInfo.colorFormat != NDSColorFormat_BGR888_Rev))
	{
		return;
	}
	
	// Iterate through VRAM banks A-D and determine if they will be used for this frame.
	for (size_t i = 0; i < 4; i++)
	{
		switch (vramConfiguration.banks[i].purpose)
		{
			case VramConfiguration::ABG:
			case VramConfiguration::BBG:
			case VramConfiguration::LCDC:
			case VramConfiguration::AOBJ:
			case VramConfiguration::BOBJ:
				break;
				
			default:
				this->_engineMain->ResetCaptureLineStates(i);
				break;
		}
	}
}

const NDSDisplayInfo& GPUSubsystem::GetDisplayInfo()
{
	return this->_displayInfo;
}

const GPUEngineLineInfo& GPUSubsystem::GetLineInfoAtIndex(size_t l)
{
	return this->_lineInfo[l];
}

u32 GPUSubsystem::GetFPSRender3D() const
{
	return this->_render3DFrameCount;
}

GPUEngineA* GPUSubsystem::GetEngineMain()
{
	return this->_engineMain;
}

GPUEngineB* GPUSubsystem::GetEngineSub()
{
	return this->_engineSub;
}

NDSDisplay* GPUSubsystem::GetDisplayMain()
{
	return this->_display[NDSDisplayID_Main];
}

NDSDisplay* GPUSubsystem::GetDisplayTouch()
{
	return this->_display[NDSDisplayID_Touch];
}

size_t GPUSubsystem::GetFramebufferPageCount() const
{
	return this->_displayInfo.framebufferPageCount;
}

void GPUSubsystem::SetFramebufferPageCount(size_t pageCount)
{
	if (pageCount > MAX_FRAMEBUFFER_PAGES)
	{
		pageCount = MAX_FRAMEBUFFER_PAGES;
	}
	
	this->_displayInfo.framebufferPageCount = pageCount;
}

size_t GPUSubsystem::GetCustomFramebufferWidth() const
{
	return this->_displayInfo.customWidth;
}

size_t GPUSubsystem::GetCustomFramebufferHeight() const
{
	return this->_displayInfo.customHeight;
}

void GPUSubsystem::SetCustomFramebufferSize(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return;
	}
	
	this->_engineMain->RenderLineClearAsyncFinish();
	this->_engineSub->RenderLineClearAsyncFinish();
	this->AsyncSetupEngineBuffersFinish();
	
	const float customWidthScale = (float)w / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const float customHeightScale = (float)h / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	const float newGpuLargestDstLineCount = (size_t)ceilf(customHeightScale);
	
	u16 *oldGpuDstToSrcIndexPtr = _gpuDstToSrcIndex;
	u8 *oldGpuDstToSrcSSSE3_u8_8e = _gpuDstToSrcSSSE3_u8_8e;
	u8 *oldGpuDstToSrcSSSE3_u8_16e = _gpuDstToSrcSSSE3_u8_16e;
	u8 *oldGpuDstToSrcSSSE3_u16_8e = _gpuDstToSrcSSSE3_u16_8e;
	u8 *oldGpuDstToSrcSSSE3_u32_4e = _gpuDstToSrcSSSE3_u32_4e;
	
	for (size_t srcX = 0, currentPitchCount = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; srcX++)
	{
		const size_t pitch = (size_t)ceilf((srcX+1) * customWidthScale) - currentPitchCount;
		_gpuDstPitchCount[srcX] = pitch;
		_gpuDstPitchIndex[srcX] = currentPitchCount;
		currentPitchCount += pitch;
	}
	
	for (size_t line = 0, currentLineCount = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		const size_t lineCount = (size_t)ceilf((line+1) * customHeightScale) - currentLineCount;
		GPUEngineLineInfo &lineInfo = this->_lineInfo[line];
		
		lineInfo.indexNative = line;
		lineInfo.indexCustom = currentLineCount;
		lineInfo.widthCustom = w;
		lineInfo.renderCount = lineCount;
		lineInfo.pixelCount = lineInfo.widthCustom * lineInfo.renderCount;
		lineInfo.blockOffsetNative = lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetCustom = lineInfo.indexCustom * lineInfo.widthCustom;
		
		currentLineCount += lineCount;
	}
	
	u16 *newGpuDstToSrcIndex = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	u16 *newGpuDstToSrcPtr = newGpuDstToSrcIndex;
	for (size_t y = 0, dstIdx = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
	{
		if (this->_lineInfo[y].renderCount < 1)
		{
			continue;
		}
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				newGpuDstToSrcIndex[dstIdx++] = (y * GPU_FRAMEBUFFER_NATIVE_WIDTH) + x;
			}
		}
		
		for (size_t l = 1; l < this->_lineInfo[y].renderCount; l++)
		{
			memcpy(newGpuDstToSrcPtr + (w * l), newGpuDstToSrcPtr, w * sizeof(u16));
		}
		
		newGpuDstToSrcPtr += (w * this->_lineInfo[y].renderCount);
		dstIdx += (w * (this->_lineInfo[y].renderCount - 1));
	}
	
	u8 *newGpuDstToSrcSSSE3_u8_8e = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u8_16e = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u16_8e = (u8 *)malloc_alignedCacheLine(w * sizeof(u16));
	u8 *newGpuDstToSrcSSSE3_u32_4e = (u8 *)malloc_alignedCacheLine(w * sizeof(u32));
	
	for (size_t i = 0; i < w; i++)
	{
		const u8 value_u8_4 = newGpuDstToSrcIndex[i] & 0x03;
		const u8 value_u8_8 = newGpuDstToSrcIndex[i] & 0x07;
		const u8 value_u8_16 = newGpuDstToSrcIndex[i] & 0x0F;
		const u8 value_u16 = (value_u8_8 << 1);
		const u8 value_u32 = (value_u8_4 << 2);
		
		newGpuDstToSrcSSSE3_u8_8e[i] = value_u8_8;
		newGpuDstToSrcSSSE3_u8_16e[i] = value_u8_16;
		
		newGpuDstToSrcSSSE3_u16_8e[(i << 1) + 0] = value_u16 + 0;
		newGpuDstToSrcSSSE3_u16_8e[(i << 1) + 1] = value_u16 + 1;
		
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 0] = value_u32 + 0;
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 1] = value_u32 + 1;
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 2] = value_u32 + 2;
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 3] = value_u32 + 3;
	}
	
	_gpuLargestDstLineCount = newGpuLargestDstLineCount;
	_gpuVRAMBlockOffset = this->_lineInfo[GPU_VRAM_BLOCK_LINES].indexCustom * w;
	_gpuDstToSrcIndex = newGpuDstToSrcIndex;
	_gpuDstToSrcSSSE3_u8_8e = newGpuDstToSrcSSSE3_u8_8e;
	_gpuDstToSrcSSSE3_u8_16e = newGpuDstToSrcSSSE3_u8_16e;
	_gpuDstToSrcSSSE3_u16_8e = newGpuDstToSrcSSSE3_u16_8e;
	_gpuDstToSrcSSSE3_u32_4e = newGpuDstToSrcSSSE3_u32_4e;
	
	CurrentRenderer->RenderFinish();
	CurrentRenderer->SetRenderNeedsFinish(false);
	
	this->_displayInfo.isCustomSizeRequested = ( (w != GPU_FRAMEBUFFER_NATIVE_WIDTH) || (h != GPU_FRAMEBUFFER_NATIVE_HEIGHT) );
	this->_displayInfo.customWidth = w;
	this->_displayInfo.customHeight = h;
	
	if (!this->_displayInfo.isCustomSizeRequested)
	{
		this->_engineMain->ResetCaptureLineStates(0);
		this->_engineMain->ResetCaptureLineStates(1);
		this->_engineMain->ResetCaptureLineStates(2);
		this->_engineMain->ResetCaptureLineStates(3);
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, w, h, this->_displayInfo.framebufferPageCount);
	
	free_aligned(oldGpuDstToSrcIndexPtr);
	free_aligned(oldGpuDstToSrcSSSE3_u8_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u8_16e);
	free_aligned(oldGpuDstToSrcSSSE3_u16_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u32_4e);
}

NDSColorFormat GPUSubsystem::GetColorFormat() const
{
	return this->_displayInfo.colorFormat;
}

void GPUSubsystem::SetColorFormat(const NDSColorFormat outputFormat)
{
	if (this->_displayInfo.colorFormat == outputFormat)
	{
		return;
	}
	
	this->_engineMain->RenderLineClearAsyncFinish();
	this->_engineSub->RenderLineClearAsyncFinish();
	this->AsyncSetupEngineBuffersFinish();
	
	CurrentRenderer->RenderFinish();
	CurrentRenderer->SetRenderNeedsFinish(false);
	
	this->_displayInfo.colorFormat = outputFormat;
	this->_displayInfo.pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(FragmentColor);
	
	if (!this->_displayInfo.isCustomSizeRequested)
	{
		this->_engineMain->ResetCaptureLineStates(0);
		this->_engineMain->ResetCaptureLineStates(1);
		this->_engineMain->ResetCaptureLineStates(2);
		this->_engineMain->ResetCaptureLineStates(3);
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, this->_displayInfo.customWidth, this->_displayInfo.customHeight, this->_displayInfo.framebufferPageCount);
}

void GPUSubsystem::_AllocateFramebuffers(NDSColorFormat outputFormat, size_t w, size_t h, size_t pageCount)
{
	void *oldMasterFramebuffer = this->_masterFramebuffer;
	void *oldCustomVRAM = this->_customVRAM;
	
	const size_t pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(FragmentColor);
	const size_t newCustomVRAMBlockSize = this->_lineInfo[GPU_VRAM_BLOCK_LINES].indexCustom * w;
	const size_t newCustomVRAMBlankSize = _gpuLargestDstLineCount * GPU_VRAM_BLANK_REGION_LINES * w;
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * pixelBytes;
	const size_t customFramebufferSize = w * h * pixelBytes;
	
	void *newCustomVRAM = NULL;
	
	this->_displayInfo.framebufferPageCount = pageCount;
	this->_displayInfo.framebufferPageSize = (nativeFramebufferSize * 2) + (customFramebufferSize * 2);
	this->_masterFramebuffer = malloc_alignedPage(this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount);
	
	this->_displayInfo.masterFramebufferHead = this->_masterFramebuffer;
	this->_displayInfo.masterNativeBuffer = (u8 *)this->_masterFramebuffer + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize);
	this->_displayInfo.masterCustomBuffer = (u8 *)this->_masterFramebuffer + (nativeFramebufferSize * 2) + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize);
	
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main]  = this->_displayInfo.masterNativeBuffer;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]  = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch] = (u8 *)this->_displayInfo.masterNativeBuffer + nativeFramebufferSize;
	this->_displayInfo.customBuffer[NDSDisplayID_Touch] = (u8 *)this->_displayInfo.masterCustomBuffer + customFramebufferSize;
	
	if (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main])
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_displayInfo.customBuffer[NDSDisplayID_Main];
		this->_displayInfo.renderedWidth[NDSDisplayID_Main]  = this->_displayInfo.customWidth;
		this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_displayInfo.customHeight;
	}
	else
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
		this->_displayInfo.renderedWidth[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_displayInfo.renderedHeight[NDSDisplayID_Main] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	if (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch])
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.customBuffer[NDSDisplayID_Touch];
		this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = this->_displayInfo.customWidth;
		this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_displayInfo.customHeight;
	}
	else
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
		this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	switch (outputFormat)
	{
		case NDSColorFormat_BGR555_Rev:
			newCustomVRAM = (void *)malloc_alignedPage(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset_u16(this->_masterFramebuffer, 0x8000, (this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount) / sizeof(u16));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (u16 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			newCustomVRAM = (void *)malloc_alignedPage(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset_u32(this->_masterFramebuffer, 0x1F000000, (this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount) / sizeof(FragmentColor));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (u16 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			newCustomVRAM = (void *)malloc_alignedPage(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(FragmentColor));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(FragmentColor));
			memset_u32(this->_masterFramebuffer, 0xFF000000, (this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount) / sizeof(FragmentColor));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (FragmentColor *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		default:
			break;
	}
	
	this->_engineMain->SetCustomFramebufferSize(w, h);
	this->_engineSub->SetCustomFramebufferSize(w, h);
	
	BaseRenderer->SetFramebufferSize(w, h); // Since BaseRenderer is persistent, we need to update this manually.
	if (CurrentRenderer != BaseRenderer)
	{
		CurrentRenderer->RequestColorFormat(outputFormat);
		CurrentRenderer->SetFramebufferSize(w, h);
	}
	
	free_aligned(oldMasterFramebuffer);
	free_aligned(oldCustomVRAM);
}

int GPUSubsystem::Get3DRendererID()
{
	return this->_pending3DRendererID;
}

void GPUSubsystem::Set3DRendererByID(int rendererID)
{
	Render3DInterface *newRenderInterface = core3DList[rendererID];
	if (newRenderInterface->NDS_3D_Init == NULL)
	{
		return;
	}
	
	this->_pending3DRendererID = rendererID;
	this->_needChange3DRenderer = true;
}

bool GPUSubsystem::Change3DRendererByID(int rendererID)
{
	bool result = false;
	
	// Whether pass or fail, the 3D renderer will have only one chance to be
	// lazily changed via the flag set by Set3DRendererByID().
	this->_needChange3DRenderer = false;
	
	Render3DInterface *newRenderInterface = core3DList[rendererID];
	if (newRenderInterface->NDS_3D_Init == NULL)
	{
		return result;
	}
	
	// Some resources are shared between renderers, such as the texture cache,
	// so we need to shut down the current renderer now to ensure that any
	// shared resources aren't in use.
	const bool didRenderBegin = CurrentRenderer->GetRenderNeedsFinish();
	CurrentRenderer->RenderFinish();
	gpu3D->NDS_3D_Close();
	gpu3D = &gpu3DNull;
	cur3DCore = RENDERID_NULL;
	BaseRenderer->SetRenderNeedsFinish(didRenderBegin);
	CurrentRenderer = BaseRenderer;
	
	Render3D *newRenderer = newRenderInterface->NDS_3D_Init();
	if (newRenderer == NULL)
	{
		return result;
	}
	
	newRenderer->RequestColorFormat(GPU->GetDisplayInfo().colorFormat);
	
	Render3DError error = newRenderer->SetFramebufferSize(GPU->GetCustomFramebufferWidth(), GPU->GetCustomFramebufferHeight());
	if (error != RENDER3DERROR_NOERR)
	{
		newRenderInterface->NDS_3D_Close();
		printf("GPU: 3D framebuffer resize error. 3D rendering will be disabled for this renderer. (Error code = %d)\n", (int)error);
		return result;
	}
	
	gpu3D = newRenderInterface;
	cur3DCore = rendererID;
	newRenderer->SetRenderNeedsFinish( BaseRenderer->GetRenderNeedsFinish() );
	CurrentRenderer = newRenderer;
	
	result = true;
	return result;
}

bool GPUSubsystem::Change3DRendererIfNeeded()
{
	if (!this->_needChange3DRenderer)
	{
		return true;
	}
	
	return this->Change3DRendererByID(this->_pending3DRendererID);
}

void* GPUSubsystem::GetCustomVRAMBuffer()
{
	return this->_customVRAM;
}

void* GPUSubsystem::GetCustomVRAMBlankBuffer()
{
	return this->_customVRAMBlank;
}

template <NDSColorFormat COLORFORMAT>
void* GPUSubsystem::GetCustomVRAMAddressUsingMappedAddress(const u32 mappedAddr, const size_t offset)
{
	const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(mappedAddr) - MMU.ARM9_LCD) / sizeof(u16);
	if (vramPixel >= (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
	{
		return this->_customVRAMBlank;
	}
	
	const size_t blockID   = vramPixel >> 16;				// blockID   = vramPixel / (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES)
	const size_t blockLine = (vramPixel >> 8) & 0x000000FF;	// blockLine = (vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES)) / GPU_FRAMEBUFFER_NATIVE_WIDTH
	const size_t linePixel = vramPixel & 0x000000FF;		// linePixel = (vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES)) % GPU_FRAMEBUFFER_NATIVE_WIDTH
	
	return (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (this->_lineInfo[blockLine].indexCustom * this->_lineInfo[blockLine].widthCustom) + _gpuDstPitchIndex[linePixel] + offset) : (void *)((u16 *)this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (this->_lineInfo[blockLine].indexCustom * this->_lineInfo[blockLine].widthCustom) + _gpuDstPitchIndex[linePixel] + offset);
}

bool GPUSubsystem::GetWillPostprocessDisplays() const
{
	return this->_willPostprocessDisplays;
}

void GPUSubsystem::SetWillPostprocessDisplays(const bool willPostprocess)
{
	this->_willPostprocessDisplays = willPostprocess;
}

void GPUSubsystem::PostprocessDisplay(const NDSDisplayID displayID, NDSDisplayInfo &mutableInfo)
{
	if (mutableInfo.isDisplayEnabled[displayID])
	{
		if (mutableInfo.colorFormat == NDSColorFormat_BGR666_Rev)
		{
			if (mutableInfo.needConvertColorFormat[displayID])
			{
				ColorspaceConvertBuffer6665To8888<false, false>((u32 *)mutableInfo.renderedBuffer[displayID], (u32 *)mutableInfo.renderedBuffer[displayID], mutableInfo.renderedWidth[displayID] * mutableInfo.renderedHeight[displayID]);
			}
			
			if (mutableInfo.needApplyMasterBrightness[displayID])
			{
				this->_display[displayID]->GetEngine()->ApplyMasterBrightness<NDSColorFormat_BGR888_Rev>(mutableInfo);
			}
		}
		else
		{
			if (mutableInfo.needApplyMasterBrightness[displayID])
			{
				switch (mutableInfo.colorFormat)
				{
					case NDSColorFormat_BGR555_Rev:
						this->_display[displayID]->GetEngine()->ApplyMasterBrightness<NDSColorFormat_BGR555_Rev>(mutableInfo);
						break;
						
					case NDSColorFormat_BGR666_Rev:
						this->_display[displayID]->GetEngine()->ApplyMasterBrightness<NDSColorFormat_BGR666_Rev>(mutableInfo);
						break;
						
					case NDSColorFormat_BGR888_Rev:
						this->_display[displayID]->GetEngine()->ApplyMasterBrightness<NDSColorFormat_BGR888_Rev>(mutableInfo);
						break;
						
					default:
						break;
				}
			}
		}
	}
	else
	{
		if (mutableInfo.colorFormat == NDSColorFormat_BGR555_Rev)
		{
			memset(mutableInfo.renderedBuffer[displayID], 0, mutableInfo.renderedWidth[displayID] * mutableInfo.renderedHeight[displayID] * sizeof(u16));
		}
		else
		{
			memset(mutableInfo.renderedBuffer[displayID], 0, mutableInfo.renderedWidth[displayID] * mutableInfo.renderedHeight[displayID] * sizeof(u32));
		}
	}
	
	mutableInfo.needConvertColorFormat[displayID] = false;
	mutableInfo.needApplyMasterBrightness[displayID] = false;
}

void GPUSubsystem::ResolveDisplayToCustomFramebuffer(const NDSDisplayID displayID, NDSDisplayInfo &mutableInfo)
{
	this->_display[displayID]->GetEngine()->ResolveToCustomFramebuffer(mutableInfo);
}

bool GPUSubsystem::GetWillAutoResolveToCustomBuffer() const
{
	return this->_willAutoResolveToCustomBuffer;
}

void GPUSubsystem::SetWillAutoResolveToCustomBuffer(const bool willAutoResolve)
{
	this->_willAutoResolveToCustomBuffer = willAutoResolve;
}

void GPUSubsystem::SetupEngineBuffers()
{
	this->_engineMain->SetupBuffers();
	this->_engineSub->SetupBuffers();
}

void* GPUSubsystem_AsyncSetupEngineBuffers(void *arg)
{
	GPUSubsystem *gpuSubystem = (GPUSubsystem *)arg;
	gpuSubystem->SetupEngineBuffers();
	
	return NULL;
}

void GPUSubsystem::AsyncSetupEngineBuffersStart()
{
	if (this->_asyncEngineBufferSetupTask == NULL)
	{
		return;
	}
	
	this->AsyncSetupEngineBuffersFinish();
	this->_asyncEngineBufferSetupTask->execute(&GPUSubsystem_AsyncSetupEngineBuffers, this);
	this->_asyncEngineBufferSetupIsRunning = true;
}

void GPUSubsystem::AsyncSetupEngineBuffersFinish()
{
	if (!this->_asyncEngineBufferSetupIsRunning)
	{
		return;
	}
	
	this->_asyncEngineBufferSetupTask->finish();
	this->_asyncEngineBufferSetupIsRunning = false;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUSubsystem::RenderLine(const size_t l)
{
	if (!this->_frameNeedsFinish)
	{
		this->_event->DidApplyGPUSettingsBegin();
		this->_engineMain->ApplySettings();
		this->_engineSub->ApplySettings();
		this->_event->DidApplyGPUSettingsEnd();
		
		this->_event->DidFrameBegin(l, this->_willFrameSkip, this->_displayInfo.framebufferPageCount, this->_displayInfo.bufferIndex);
		this->_frameNeedsFinish = true;
	}
	
	const bool isDisplayCaptureNeeded = this->_engineMain->WillDisplayCapture(l);
	const bool isFramebufferRenderNeeded[2]	= { this->_engineMain->GetEnableStateApplied(), this->_engineSub->GetEnableStateApplied() };
	
	if (l == 0)
	{
		if (!this->_willFrameSkip)
		{
			if (this->_asyncEngineBufferSetupIsRunning)
			{
				this->AsyncSetupEngineBuffersFinish();
			}
			else
			{
				this->SetupEngineBuffers();
			}
			
			this->UpdateRenderProperties();
		}
	}
	
	if (!this->_willFrameSkip)
	{
		this->_engineMain->UpdateRenderStates<OUTPUTFORMAT>(l);
		this->_engineSub->UpdateRenderStates<OUTPUTFORMAT>(l);
	}
	
	if ( (isFramebufferRenderNeeded[GPUEngineID_Main] || isDisplayCaptureNeeded) && !this->_willFrameSkip )
	{
		// GPUEngineA:WillRender3DLayer() and GPUEngineA:WillCapture3DLayerDirect() both rely on register
		// states that might change on a per-line basis. Therefore, we need to check these states on a
		// per-line basis as well. While most games will set up these states by line 0 and keep these
		// states constant all the way through line 191, this may not always be the case.
		//
		// Test case: If a conversation occurs in Advance Wars: Dual Strike where the conversation
		// originates from the top of the screen, the BG0 layer will only be enabled at line 46. This
		// means that we need to check the states at that particular time to ensure that the 3D renderer
		// finishes before we read the 3D framebuffer. Otherwise, the map will render incorrectly.
		
		const bool need3DCaptureFramebuffer = this->_engineMain->WillCapture3DLayerDirect(l);
		const bool need3DDisplayFramebuffer = this->_engineMain->WillRender3DLayer() || ((OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && need3DCaptureFramebuffer);
		
		if (need3DCaptureFramebuffer || need3DDisplayFramebuffer)
		{
			if (CurrentRenderer->GetRenderNeedsFinish())
			{
				CurrentRenderer->RenderFinish();
				CurrentRenderer->SetRenderNeedsFinish(false);
				this->_event->DidRender3DEnd();
			}
			
			CurrentRenderer->RenderFlush(need3DDisplayFramebuffer && CurrentRenderer->GetRenderNeedsFlushMain(),
			                             need3DCaptureFramebuffer && CurrentRenderer->GetRenderNeedsFlush16());
		}
		
		this->_engineMain->RenderLine<OUTPUTFORMAT>(l);
	}
	else
	{
		this->_engineMain->UpdatePropertiesWithoutRender(l);
	}
	
	if (isFramebufferRenderNeeded[GPUEngineID_Sub] && !this->_willFrameSkip)
	{
		this->_engineSub->RenderLine<OUTPUTFORMAT>(l);
	}
	else
	{
		this->_engineSub->UpdatePropertiesWithoutRender(l);
	}
	
	if (l == 191)
	{
		this->_engineMain->LastLineProcess();
		this->_engineSub->LastLineProcess();
		
		this->_UpdateFPSRender3D();
		
		if (!this->_willFrameSkip)
		{
			if (this->_displayInfo.isCustomSizeRequested)
			{
				this->_engineMain->ResolveCustomRendering<OUTPUTFORMAT>();
				this->_engineSub->ResolveCustomRendering<OUTPUTFORMAT>();
			}
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = (this->_display[NDSDisplayID_Main]->GetEngine()->nativeLineOutputCount < GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetEngine()->GetRenderedBuffer();
			this->_displayInfo.renderedWidth[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetEngine()->GetRenderedWidth();
			this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetEngine()->GetRenderedHeight();
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = (this->_display[NDSDisplayID_Touch]->GetEngine()->nativeLineOutputCount < GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->GetRenderedBuffer();
			this->_displayInfo.renderedWidth[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->GetRenderedWidth();
			this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->GetRenderedHeight();
			
			this->_displayInfo.engineID[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->GetEngineID();
			this->_displayInfo.engineID[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngineID();
			
			this->_displayInfo.isDisplayEnabled[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->GetEngine()->GetEnableStateApplied();
			this->_displayInfo.isDisplayEnabled[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->GetEnableStateApplied();
			
			this->_displayInfo.needConvertColorFormat[NDSDisplayID_Main]  = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev);
			this->_displayInfo.needConvertColorFormat[NDSDisplayID_Touch] = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev);
			
			// Set the average backlight intensity over 263 H-blanks.
			this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = this->_backlightIntensityTotal[NDSDisplayID_Main]  / 263.0f;
			this->_displayInfo.backlightIntensity[NDSDisplayID_Touch] = this->_backlightIntensityTotal[NDSDisplayID_Touch] / 263.0f;
			
			this->_engineMain->UpdateMasterBrightnessDisplayInfo(this->_displayInfo);
			this->_engineSub->UpdateMasterBrightnessDisplayInfo(this->_displayInfo);
			
			if (this->_willPostprocessDisplays)
			{
				this->PostprocessDisplay(NDSDisplayID_Main,  this->_displayInfo);
				this->PostprocessDisplay(NDSDisplayID_Touch, this->_displayInfo);
			}
			
			if (this->_willAutoResolveToCustomBuffer)
			{
				this->ResolveDisplayToCustomFramebuffer(NDSDisplayID_Main,  this->_displayInfo);
				this->ResolveDisplayToCustomFramebuffer(NDSDisplayID_Touch, this->_displayInfo);
			}
			
			this->AsyncSetupEngineBuffersStart();
		}
		
		// Reset the current backlight intensity total.
		this->_backlightIntensityTotal[NDSDisplayID_Main]  = 0.0f;
		this->_backlightIntensityTotal[NDSDisplayID_Touch] = 0.0f;
		
		if (this->_frameNeedsFinish)
		{
			this->_frameNeedsFinish = false;
			this->_displayInfo.sequenceNumber++;
			this->_event->DidFrameEnd(this->_willFrameSkip, this->_displayInfo);
		}
	}
}

void GPUSubsystem::UpdateAverageBacklightIntensityTotal()
{
	// The values in this table are, more or less, arbitrarily chosen.
	//
	// It would be nice to get some real testing done on a hardware NDS to have some more
	// accurate intensity values for each backlight level.
	static const float backlightLevelToIntensityTable[] = {0.100, 0.300, 0.600, 1.000};
	
	IOREG_POWERMANCTL POWERMANCTL;
	IOREG_BACKLIGHTCTL BACKLIGHTCTL;
	POWERMANCTL.value = MMU.powerMan_Reg[0];
	BACKLIGHTCTL.value = MMU.powerMan_Reg[4];
	
	if (POWERMANCTL.MainBacklight_Enable != 0)
	{
		const BacklightLevel level = ((BACKLIGHTCTL.ExternalPowerState != 0) && (BACKLIGHTCTL.ForceMaxBrightnessWithExtPower_Enable != 0)) ? BacklightLevel_Maximum : (BacklightLevel)BACKLIGHTCTL.Level;
		this->_backlightIntensityTotal[NDSDisplayID_Main]  += backlightLevelToIntensityTable[level];
	}
	
	if (POWERMANCTL.TouchBacklight_Enable != 0)
	{
		const BacklightLevel level = ((BACKLIGHTCTL.ExternalPowerState != 0) && (BACKLIGHTCTL.ForceMaxBrightnessWithExtPower_Enable != 0)) ? BacklightLevel_Maximum : (BacklightLevel)BACKLIGHTCTL.Level;
		this->_backlightIntensityTotal[NDSDisplayID_Touch] += backlightLevelToIntensityTable[level];
	}
}

void GPUSubsystem::ClearWithColor(const u16 colorBGRA5551)
{
	u16 color16 = colorBGRA5551;
	FragmentColor color32;
	
	switch (this->_displayInfo.colorFormat)
	{
		case NDSColorFormat_BGR555_Rev:
			color16 = colorBGRA5551 | 0x8000;
			break;
			
		case NDSColorFormat_BGR666_Rev:
			color32.color = COLOR555TO6665_OPAQUE(colorBGRA5551 & 0x7FFF);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			color32.color = COLOR555TO8888_OPAQUE(colorBGRA5551 & 0x7FFF);
			break;
			
		default:
			break;
	}
	
	switch (this->_displayInfo.pixelBytes)
	{
		case 2:
			memset_u16(this->_masterFramebuffer, color16, (this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount) / this->_displayInfo.pixelBytes);
			break;
			
		case 4:
			memset_u32(this->_masterFramebuffer, color32.color, (this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount) / this->_displayInfo.pixelBytes);
			break;
			
		default:
			break;
	}
}

u8* GPUSubsystem::_DownscaleAndConvertForSavestate(const NDSDisplayID displayID, void *__restrict intermediateBuffer)
{
	u8 *dstBuffer = NULL;
	bool isIntermediateBufferMissing = false; // Flag to check if intermediateBuffer is NULL, but only if it's actually needed.
	
	if ( (this->_displayInfo.colorFormat == NDSColorFormat_BGR555_Rev) && !this->_displayInfo.didPerformCustomRender[displayID] )
	{
		dstBuffer = (u8 *)this->_displayInfo.nativeBuffer[displayID];
	}
	else
	{
		if (this->_displayInfo.isDisplayEnabled[displayID])
		{
			if (this->_displayInfo.didPerformCustomRender[displayID])
			{
				if (this->_displayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
				{
					const u16 *__restrict src = (u16 *__restrict)this->_displayInfo.customBuffer[displayID];
					u16 *__restrict dst = (u16 *__restrict)this->_displayInfo.nativeBuffer[displayID];
					
					for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
					{
						CopyLineReduceHinted<0xFFFF, false, true, 2>(this->_lineInfo[l], src, dst);
						src += this->_lineInfo[l].pixelCount;
						dst += GPU_FRAMEBUFFER_NATIVE_WIDTH;
					}
				}
				else
				{
					isIntermediateBufferMissing = (intermediateBuffer == NULL);
					if (!isIntermediateBufferMissing)
					{
						const u32 *__restrict src = (u32 *__restrict)this->_displayInfo.customBuffer[displayID];
						u32 *__restrict dst = (u32 *__restrict)intermediateBuffer;
						
						for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
						{
							CopyLineReduceHinted<0xFFFF, false, true, 4>(this->_lineInfo[l], src, dst);
							src += this->_lineInfo[l].pixelCount;
							dst += GPU_FRAMEBUFFER_NATIVE_WIDTH;
						}
						
						switch (this->_displayInfo.colorFormat)
						{
							case NDSColorFormat_BGR666_Rev:
								ColorspaceConvertBuffer6665To5551<false, false>((const u32 *__restrict)intermediateBuffer, (u16 *__restrict)this->_displayInfo.nativeBuffer[displayID], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
								break;
								
							case NDSColorFormat_BGR888_Rev:
								ColorspaceConvertBuffer8888To5551<false, false>((const u32 *__restrict)intermediateBuffer, (u16 *__restrict)this->_displayInfo.nativeBuffer[displayID], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
								break;
								
							default:
								break;
						}
					}
				}
				
				dstBuffer = (u8 *)this->_displayInfo.nativeBuffer[displayID];
			}
			else
			{
				isIntermediateBufferMissing = (intermediateBuffer == NULL);
				if (!isIntermediateBufferMissing)
				{
					switch (this->_displayInfo.colorFormat)
					{
						case NDSColorFormat_BGR666_Rev:
							ColorspaceConvertBuffer6665To5551<false, false>((const u32 *__restrict)this->_displayInfo.nativeBuffer[displayID], (u16 *__restrict)intermediateBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
							break;
							
						case NDSColorFormat_BGR888_Rev:
							ColorspaceConvertBuffer8888To5551<false, false>((const u32 *__restrict)this->_displayInfo.nativeBuffer[displayID], (u16 *__restrict)intermediateBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
							break;
							
						default:
							break;
					}
					
					dstBuffer = (u8 *)intermediateBuffer;
				}
				else
				{
					dstBuffer = (u8 *)this->_displayInfo.nativeBuffer[displayID];
				}
			}
		}
		
		if (!this->_displayInfo.isDisplayEnabled[displayID] || isIntermediateBufferMissing)
		{
			memset(this->_displayInfo.nativeBuffer[displayID], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
			dstBuffer = (u8 *)this->_displayInfo.nativeBuffer[displayID];
		}
	}
	
	return dstBuffer;
}

void GPUSubsystem::SaveState(EMUFILE &os)
{
	// Savestate chunk version
	os.write_32LE(2);
	
	// Version 0
	u8 *__restrict intermediateBuffer = NULL;
	u8 *savestateColorBuffer = NULL;
	
	if ( (this->_displayInfo.colorFormat != NDSColorFormat_BGR555_Rev) &&
		 (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Main] || this->_displayInfo.isDisplayEnabled[NDSDisplayID_Touch]) )
	{
		intermediateBuffer = (u8 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u32));
	}
	
	// Downscale and color convert the display framebuffers.
	savestateColorBuffer = this->_DownscaleAndConvertForSavestate(NDSDisplayID_Main, intermediateBuffer);
	os.fwrite(savestateColorBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	
	savestateColorBuffer = this->_DownscaleAndConvertForSavestate(NDSDisplayID_Touch, intermediateBuffer);
	os.fwrite(savestateColorBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	
	free_aligned(intermediateBuffer);
	intermediateBuffer = NULL;
	
	// Version 1
	os.write_32LE(this->_engineMain->savedBG2X.value);
	os.write_32LE(this->_engineMain->savedBG2Y.value);
	os.write_32LE(this->_engineMain->savedBG3X.value);
	os.write_32LE(this->_engineMain->savedBG3Y.value);
	os.write_32LE(this->_engineSub->savedBG2X.value);
	os.write_32LE(this->_engineSub->savedBG2Y.value);
	os.write_32LE(this->_engineSub->savedBG3X.value);
	os.write_32LE(this->_engineSub->savedBG3Y.value);
	
	// Version 2
	os.write_floatLE(_backlightIntensityTotal[NDSDisplayID_Main]);
	os.write_floatLE(_backlightIntensityTotal[NDSDisplayID_Touch]);
}

bool GPUSubsystem::LoadState(EMUFILE &is, int size)
{
	u32 version;
	
	//sigh.. shouldve used a new version number
	if (size == GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2)
	{
		version = 0;
	}
	else if (size == 0x30024)
	{
		is.read_32LE(version);
		version = 1;
	}
	else
	{
		if (is.read_32LE(version) < 1) return false;
	}
	
	if (version > 2) return false;
	
	// Version 0
	if (this->_displayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
	{
		is.fread((u8 *)this->_displayInfo.nativeBuffer[NDSDisplayID_Main],  GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
		is.fread((u8 *)this->_displayInfo.nativeBuffer[NDSDisplayID_Touch], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	}
	else
	{
		is.fread((u8 *)this->_displayInfo.customBuffer[NDSDisplayID_Main],  GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
		is.fread((u8 *)this->_displayInfo.customBuffer[NDSDisplayID_Touch], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
		
		switch (this->_displayInfo.colorFormat)
		{
			case NDSColorFormat_BGR666_Rev:
			{
				if (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Main])
				{
					ColorspaceConvertBuffer555To6665Opaque<false, false>((u16 *)this->_displayInfo.customBuffer[NDSDisplayID_Main], (u32 *)this->_displayInfo.nativeBuffer[NDSDisplayID_Main], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				else
				{
					memset(this->_displayInfo.nativeBuffer[NDSDisplayID_Main], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
				}
				
				if (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Touch])
				{
					ColorspaceConvertBuffer555To6665Opaque<false, false>((u16 *)this->_displayInfo.customBuffer[NDSDisplayID_Touch], (u32 *)this->_displayInfo.nativeBuffer[NDSDisplayID_Touch], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				else
				{
					memset(this->_displayInfo.nativeBuffer[NDSDisplayID_Touch], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
				}
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				if (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Main])
				{
					ColorspaceConvertBuffer555To8888Opaque<false, false>((u16 *)this->_displayInfo.customBuffer[NDSDisplayID_Main], (u32 *)this->_displayInfo.nativeBuffer[NDSDisplayID_Main], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				else
				{
					memset(this->_displayInfo.nativeBuffer[NDSDisplayID_Main], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
				}
				
				if (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Touch])
				{
					ColorspaceConvertBuffer555To8888Opaque<false, false>((u16 *)this->_displayInfo.customBuffer[NDSDisplayID_Touch], (u32 *)this->_displayInfo.nativeBuffer[NDSDisplayID_Touch], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				}
				else
				{
					memset(this->_displayInfo.nativeBuffer[NDSDisplayID_Touch], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
				}
				break;
			}
				
			default:
				break;
		}
	}
	
	if (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main])
	{
		if (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Main])
		{
			switch (this->_displayInfo.colorFormat)
			{
				case NDSColorFormat_BGR555_Rev:
				{
					const u16 *__restrict src = (u16 *__restrict)this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
					u16 *__restrict dst = (u16 *__restrict)this->_displayInfo.customBuffer[NDSDisplayID_Main];
					
					for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
					{
						CopyLineExpandHinted<0xFFFF, true, false, true, 2>(this->_lineInfo[l], src, dst);
						src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
						dst += this->_lineInfo[l].pixelCount;
					}
					break;
				}
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
				{
					const u32 *__restrict src = (u32 *__restrict)this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
					u32 *__restrict dst = (u32 *__restrict)this->_displayInfo.customBuffer[NDSDisplayID_Main];
					
					for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
					{
						CopyLineExpandHinted<0xFFFF, true, false, true, 4>(this->_lineInfo[l], src, dst);
						src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
						dst += this->_lineInfo[l].pixelCount;
					}
					break;
				}
			}
		}
		else
		{
			memset(this->_displayInfo.customBuffer[NDSDisplayID_Main], 0, this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes);
		}
	}
	
	if (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch])
	{
		if (this->_displayInfo.isDisplayEnabled[NDSDisplayID_Touch])
		{
			switch (this->_displayInfo.colorFormat)
			{
				case NDSColorFormat_BGR555_Rev:
				{
					const u16 *__restrict src = (u16 *__restrict)this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
					u16 *__restrict dst = (u16 *__restrict)this->_displayInfo.customBuffer[NDSDisplayID_Touch];
					
					for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
					{
						CopyLineExpandHinted<0xFFFF, true, false, true, 2>(this->_lineInfo[l], src, dst);
						src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
						dst += this->_lineInfo[l].pixelCount;
					}
					break;
				}
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
				{
					const u32 *__restrict src = (u32 *__restrict)this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
					u32 *__restrict dst = (u32 *__restrict)this->_displayInfo.customBuffer[NDSDisplayID_Touch];
					
					for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
					{
						CopyLineExpandHinted<0xFFFF, true, false, true, 4>(this->_lineInfo[l], src, dst);
						src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
						dst += this->_lineInfo[l].pixelCount;
					}
					break;
				}
			}
		}
		else
		{
			memset(this->_displayInfo.customBuffer[NDSDisplayID_Touch], 0, this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes);
		}
	}
	
	// Version 1
	if (version >= 1)
	{
		is.read_32LE(this->_engineMain->savedBG2X.value);
		is.read_32LE(this->_engineMain->savedBG2Y.value);
		is.read_32LE(this->_engineMain->savedBG3X.value);
		is.read_32LE(this->_engineMain->savedBG3Y.value);
		is.read_32LE(this->_engineSub->savedBG2X.value);
		is.read_32LE(this->_engineSub->savedBG2Y.value);
		is.read_32LE(this->_engineSub->savedBG3X.value);
		is.read_32LE(this->_engineSub->savedBG3Y.value);
		//removed per nitsuja feedback. anyway, this same thing will happen almost immediately in gpu line=0
		//this->_engineMain->refreshAffineStartRegs(-1,-1);
		//this->_engineSub->refreshAffineStartRegs(-1,-1);
	}
	
	// Version 2
	if (version >= 2)
	{
		is.read_floatLE(this->_backlightIntensityTotal[NDSDisplayID_Main]);
		is.read_floatLE(this->_backlightIntensityTotal[NDSDisplayID_Touch]);
		this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = this->_backlightIntensityTotal[NDSDisplayID_Main]  / 71.0f;
		this->_displayInfo.backlightIntensity[NDSDisplayID_Touch] = this->_backlightIntensityTotal[NDSDisplayID_Touch] / 71.0f;
	}
	else
	{
		// UpdateAverageBacklightIntensityTotal() adds to _backlightIntensityTotal, and is called 263 times per frame.
		// Of these, 71 calls are after _displayInfo.backlightIntensity is set.
		// This emulates those calls as a way of guessing what the backlight values were in a savestate which doesn't contain that information.
		this->_backlightIntensityTotal[0] = 0.0f;
		this->_backlightIntensityTotal[1] = 0.0f;
		this->UpdateAverageBacklightIntensityTotal();
		this->_displayInfo.backlightIntensity[0] = this->_backlightIntensityTotal[0];
		this->_displayInfo.backlightIntensity[1] = this->_backlightIntensityTotal[1];
		this->_backlightIntensityTotal[0] *= 71;
		this->_backlightIntensityTotal[1] *= 71;
	}
	
	// Parse all GPU engine related registers based on a previously read MMU savestate chunk.
	this->_engineMain->ParseAllRegisters();
	this->_engineSub->ParseAllRegisters();
	
	return !is.fail();
}

void GPUEventHandlerDefault::DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut)
{
	if ( (pageCount > 1) && (line == 0) && !isFrameSkipRequested )
	{
		selectedBufferIndexInOut = ((selectedBufferIndexInOut + 1) % pageCount);
	}
}

GPUClientFetchObject::GPUClientFetchObject()
{
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		memset(&_fetchDisplayInfo[i], 0, sizeof(NDSDisplayInfo));
	}
	
	_clientData = NULL;
	_lastFetchIndex = 0;
}

void GPUClientFetchObject::Init()
{
	// Do nothing. This is implementation dependent.
}

void GPUClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	const size_t nativeSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * currentDisplayInfo.pixelBytes;
	const size_t customSize = currentDisplayInfo.customWidth * currentDisplayInfo.customHeight * currentDisplayInfo.pixelBytes;
	
	for (size_t i = 0; i < currentDisplayInfo.framebufferPageCount; i++)
	{
		this->_fetchDisplayInfo[i] = currentDisplayInfo;
		
		if (i == 0)
		{
			this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Main]  = (u8 *)currentDisplayInfo.masterFramebufferHead;
			this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Touch] = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 1);
			this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Main]  = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 2);
			this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Touch] = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 2) + customSize;
		}
		else
		{
			this->_fetchDisplayInfo[i].nativeBuffer[NDSDisplayID_Main]  = (u8 *)this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Main]  + (currentDisplayInfo.framebufferPageSize * i);
			this->_fetchDisplayInfo[i].nativeBuffer[NDSDisplayID_Touch] = (u8 *)this->_fetchDisplayInfo[0].nativeBuffer[NDSDisplayID_Touch] + (currentDisplayInfo.framebufferPageSize * i);
			this->_fetchDisplayInfo[i].customBuffer[NDSDisplayID_Main]  = (u8 *)this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Main]  + (currentDisplayInfo.framebufferPageSize * i);
			this->_fetchDisplayInfo[i].customBuffer[NDSDisplayID_Touch] = (u8 *)this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Touch] + (currentDisplayInfo.framebufferPageSize * i);
		}
	}
}

void GPUClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	if (this->_fetchDisplayInfo[index].isDisplayEnabled[NDSDisplayID_Main])
	{
		if (!this->_fetchDisplayInfo[index].didPerformCustomRender[NDSDisplayID_Main])
		{
			this->_FetchNativeDisplayByID(NDSDisplayID_Main, index);
		}
		else
		{
			this->_FetchCustomDisplayByID(NDSDisplayID_Main, index);
		}
	}
	
	if (this->_fetchDisplayInfo[index].isDisplayEnabled[NDSDisplayID_Touch])
	{
		if (!this->_fetchDisplayInfo[index].didPerformCustomRender[NDSDisplayID_Touch])
		{
			this->_FetchNativeDisplayByID(NDSDisplayID_Touch, index);
		}
		else
		{
			this->_FetchCustomDisplayByID(NDSDisplayID_Touch, index);
		}
	}
	
	this->SetLastFetchIndex(index);
}

void GPUClientFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// Do nothing. This is implementation dependent.
}

void GPUClientFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// Do nothing. This is implementation dependent.
}

const NDSDisplayInfo& GPUClientFetchObject::GetFetchDisplayInfoForBufferIndex(const u8 bufferIndex) const
{
	return this->_fetchDisplayInfo[bufferIndex];
}

void GPUClientFetchObject::SetFetchDisplayInfo(const NDSDisplayInfo &displayInfo)
{
	this->_fetchDisplayInfo[displayInfo.bufferIndex] = displayInfo;
}

u8 GPUClientFetchObject::GetLastFetchIndex() const
{
	return this->_lastFetchIndex;
}

void GPUClientFetchObject::SetLastFetchIndex(const u8 fetchIndex)
{
	this->_lastFetchIndex = fetchIndex;
}

void* GPUClientFetchObject::GetClientData() const
{
	return this->_clientData;
}

void GPUClientFetchObject::SetClientData(void *clientData)
{
	this->_clientData = clientData;
}

NDSDisplay::NDSDisplay()
{
	_ID = NDSDisplayID_Main;
	_gpu = NULL;
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID)
{
	_ID = displayID;
	_gpu = NULL;
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID, GPUEngineBase *theEngine)
{
	_ID = displayID;
	_gpu = theEngine;
}

GPUEngineBase* NDSDisplay::GetEngine()
{
	return this->_gpu;
}

void NDSDisplay::SetEngine(GPUEngineBase *theEngine)
{
	this->_gpu = theEngine;
}

GPUEngineID NDSDisplay::GetEngineID()
{
	return this->_gpu->GetEngineID();
}

void NDSDisplay::SetEngineByID(const GPUEngineID theID)
{
	this->_gpu = (theID == GPUEngineID_Main) ? (GPUEngineBase *)GPU->GetEngineMain() : (GPUEngineBase *)GPU->GetEngineSub();
	this->_gpu->SetTargetDisplayByID(this->_ID);
}

template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_WINnH<0>();
template void GPUEngineBase::ParseReg_WINnH<1>();

template void GPUEngineBase::ParseReg_BGnX<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnY<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnX<GPULayerID_BG3>();
template void GPUEngineBase::ParseReg_BGnY<GPULayerID_BG3>();

template void GPUSubsystem::RenderLine<NDSColorFormat_BGR555_Rev>(const size_t l);
template void GPUSubsystem::RenderLine<NDSColorFormat_BGR666_Rev>(const size_t l);
template void GPUSubsystem::RenderLine<NDSColorFormat_BGR888_Rev>(const size_t l);

// These functions are used in gfx3d.cpp
template void CopyLineExpandHinted<0xFFFF, true, false, true, 4>(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer);
template void CopyLineReduceHinted<0xFFFF, false, true, 4>(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer);
