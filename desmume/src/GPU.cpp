/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
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
static CACHE_ALIGN size_t _gpuDstLineCount[GPU_FRAMEBUFFER_NATIVE_HEIGHT];	// Key: Source line index / Value: Number of destination lines for the source line
static CACHE_ALIGN size_t _gpuDstLineIndex[GPU_FRAMEBUFFER_NATIVE_HEIGHT];	// Key: Source line index / Value: First destination line that maps to the source line
static CACHE_ALIGN size_t _gpuCaptureLineCount[GPU_VRAM_BLOCK_LINES + 1];	// Key: Source line index / Value: Number of destination lines for the source line
static CACHE_ALIGN size_t _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES + 1];	// Key: Source line index / Value: First destination line that maps to the source line

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

template <s32 INTEGERSCALEHINT, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand_C(void *__restrict dst, const void *__restrict src, size_t dstLength)
{
	if (INTEGERSCALEHINT == 0)
	{
#if defined(MSB_FIRST)
		if (NEEDENDIANSWAP && (ELEMENTSIZE != 1))
		{
			for (size_t i = 0; i < dstLength; i++)
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
			memcpy(dst, src, dstLength * ELEMENTSIZE);
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
	}
}

#ifdef ENABLE_SSE2
template <s32 INTEGERSCALEHINT, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand_SSE2(void *__restrict dst, const void *__restrict src, size_t dstLength)
{
	if (INTEGERSCALEHINT == 0)
	{
		memcpy(dst, src, dstLength * ELEMENTSIZE);
	}
	else if (INTEGERSCALEHINT == 1)
	{
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / ELEMENTSIZE), _mm_store_si128((__m128i *)dst + (X), _mm_load_si128((__m128i *)src + (X))) );
	}
	else if (INTEGERSCALEHINT == 2)
	{
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; )
		{
			if (ELEMENTSIZE == 1)
			{
				const __m128i src8  = _mm_load_si128((__m128i *)( (u8 *)src + srcX));
				const __m128i src8out[2]  = { _mm_unpacklo_epi8(src8, src8), _mm_unpackhi_epi8(src8, src8) };
				
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX +  0), src8out[0]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 16), src8out[1]);
				
				srcX += 16;
				dstX += 32;
			}
			else if (ELEMENTSIZE == 2)
			{
				const __m128i src16 = _mm_load_si128((__m128i *)((u16 *)src + srcX));
				const __m128i src16out[2] = { _mm_unpacklo_epi16(src16, src16), _mm_unpackhi_epi16(src16, src16) };
				
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  0), src16out[0]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  8), src16out[1]);
				
				srcX += 8;
				dstX += 16;
			}
			else if (ELEMENTSIZE == 4)
			{
				const __m128i src32 = _mm_load_si128((__m128i *)((u32 *)src + srcX));
				const __m128i src32out[2] = { _mm_unpacklo_epi32(src32, src32), _mm_unpackhi_epi32(src32, src32) };
				
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  0), src32out[0]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  4), src32out[1]);
				
				srcX += 4;
				dstX += 8;
			}
		}
	}
	else if ((INTEGERSCALEHINT == 3) && (ELEMENTSIZE != 1))
	{
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; )
		{
			if (ELEMENTSIZE == 2)
			{
				const __m128i src16 = _mm_load_si128((__m128i *)((u16 *)src + srcX));
				const __m128i src16lo = _mm_shuffle_epi32(src16, 0x44);
				const __m128i src16hi = _mm_shuffle_epi32(src16, 0xEE);
				const __m128i src16out[3] = { _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16lo, 0x40), 0xA5), _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16, 0xFE), 0x40), _mm_shufflehi_epi16(_mm_shufflelo_epi16(src16hi, 0xA5), 0xFE) };
				
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  0), src16out[0]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  8), src16out[1]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX + 16), src16out[2]);
				
				srcX += 8;
				dstX += 24;
			}
			else if (ELEMENTSIZE == 4)
			{
				const __m128i src32 = _mm_load_si128((__m128i *)((u32 *)src + srcX));
				const __m128i src32out[3] = { _mm_shuffle_epi32(src32, 0x40), _mm_shuffle_epi32(src32, 0xA5), _mm_shuffle_epi32(src32, 0xFE) };
				
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  0), src32out[0]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  4), src32out[1]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  8), src32out[2]);
				
				srcX += 4;
				dstX += 12;
			}
		}
	}
	else if (INTEGERSCALEHINT == 4)
	{
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH;)
		{
			if (ELEMENTSIZE == 1)
			{
				const __m128i src8  = _mm_load_si128((__m128i *)( (u8 *)src + srcX));
				const __m128i src8_lo  = _mm_unpacklo_epi8(src8, src8);
				const __m128i src8_hi  = _mm_unpackhi_epi8(src8, src8);
				const __m128i src8out[4] = { _mm_unpacklo_epi8(src8_lo, src8_lo), _mm_unpackhi_epi8(src8_lo, src8_lo), _mm_unpacklo_epi8(src8_hi, src8_hi), _mm_unpackhi_epi8(src8_hi, src8_hi) };
				
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX +  0), src8out[0]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 16), src8out[1]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 32), src8out[2]);
				_mm_store_si128((__m128i *)( (u8 *)dst + dstX + 48), src8out[3]);
				
				srcX += 16;
				dstX += 64;
			}
			else if (ELEMENTSIZE == 2)
			{
				const __m128i src16 = _mm_load_si128((__m128i *)((u16 *)src + srcX));
				const __m128i src16_lo = _mm_unpacklo_epi16(src16, src16);
				const __m128i src16_hi = _mm_unpackhi_epi16(src16, src16);
				const __m128i src16out[4] = { _mm_unpacklo_epi16(src16_lo, src16_lo), _mm_unpackhi_epi16(src16_lo, src16_lo), _mm_unpacklo_epi16(src16_hi, src16_hi), _mm_unpackhi_epi16(src16_hi, src16_hi) };
				
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  0), src16out[0]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX +  8), src16out[1]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX + 16), src16out[2]);
				_mm_store_si128((__m128i *)((u16 *)dst + dstX + 24), src16out[3]);
				
				srcX += 8;
				dstX += 32;
			}
			else if (ELEMENTSIZE == 4)
			{
				const __m128i src32 = _mm_load_si128((__m128i *)((u32 *)src + srcX));
				const __m128i src32_lo = _mm_unpacklo_epi32(src32, src32);
				const __m128i src32_hi = _mm_unpackhi_epi32(src32, src32);
				const __m128i src32out[4] = { _mm_unpacklo_epi32(src32_lo, src32_lo), _mm_unpackhi_epi32(src32_lo, src32_lo), _mm_unpacklo_epi32(src32_hi, src32_hi), _mm_unpackhi_epi32(src32_hi, src32_hi) };
				
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  0), src32out[0]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  4), src32out[1]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX +  8), src32out[2]);
				_mm_store_si128((__m128i *)((u32 *)dst + dstX + 12), src32out[3]);
				
				srcX += 4;
				dstX += 16;
			}
		}
	}
#ifdef ENABLE_SSSE3
	else if (INTEGERSCALEHINT >= 0)
	{
		const size_t scale = dstLength / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
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
	}
}
#endif

template <s32 INTEGERSCALEHINT, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineExpand(void *__restrict dst, const void *__restrict src, size_t dstLength)
{
	// Use INTEGERSCALEHINT to provide a hint to CopyLineExpand() for the fastest execution path.
	// INTEGERSCALEHINT represents the scaling value of the framebuffer width, and is always
	// assumed to be a positive integer.
	//
	// Use cases:
	// - Passing a value of 0 causes CopyLineExpand() to perform a simple copy, using dstLength
	//   to copy dstLength elements.
	// - Passing a value of 1 causes CopyLineExpand() to perform a simple copy, ignoring dstLength
	//   and always copying GPU_FRAMEBUFFER_NATIVE_WIDTH elements.
	// - Passing any negative value causes CopyLineExpand() to assume that the framebuffer width
	//   is NOT scaled by an integer value, and will therefore take the safest (but slowest)
	//   execution path.
	// - Passing any positive value greater than 1 causes CopyLineExpand() to expand the line
	//   using the integer scaling value.
	
#ifdef ENABLE_SSE2
	CopyLineExpand_SSE2<INTEGERSCALEHINT, ELEMENTSIZE>(dst, src, dstLength);
#else
	CopyLineExpand_C<INTEGERSCALEHINT, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, dstLength);
#endif
}

template <bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
static FORCEINLINE void CopyLineReduce(void *__restrict dst, const void *__restrict src)
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
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const GPUEngineA *mainEngine = GPU->GetEngineMain();
	const GPUEngineB *subEngine = GPU->GetEngineSub();
	
	//version
	os.write_32LE(1);
	
	os.fwrite((u8 *)dispInfo.masterCustomBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);
	
	os.write_32LE(mainEngine->savedBG2X.value);
	os.write_32LE(mainEngine->savedBG2Y.value);
	os.write_32LE(mainEngine->savedBG3X.value);
	os.write_32LE(mainEngine->savedBG3Y.value);
	os.write_32LE(subEngine->savedBG2X.value);
	os.write_32LE(subEngine->savedBG2Y.value);
	os.write_32LE(subEngine->savedBG3X.value);
	os.write_32LE(subEngine->savedBG3Y.value);
}

bool gpu_loadstate(EMUFILE &is, int size)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	GPUEngineA *mainEngine = GPU->GetEngineMain();
	GPUEngineB *subEngine = GPU->GetEngineSub();
	
	//read version
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
		if (is.read_32LE(version) != 1) return false;
	}
	
	if (version > 1) return false;
	
	is.fread((u8 *)dispInfo.masterCustomBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);
	
	if (version == 1)
	{
		is.read_32LE(mainEngine->savedBG2X.value);
		is.read_32LE(mainEngine->savedBG2Y.value);
		is.read_32LE(mainEngine->savedBG3X.value);
		is.read_32LE(mainEngine->savedBG3Y.value);
		is.read_32LE(subEngine->savedBG2X.value);
		is.read_32LE(subEngine->savedBG2Y.value);
		is.read_32LE(subEngine->savedBG3X.value);
		is.read_32LE(subEngine->savedBG3Y.value);
		//removed per nitsuja feedback. anyway, this same thing will happen almost immediately in gpu line=0
		//mainEngine->refreshAffineStartRegs(-1,-1);
		//subEngine->refreshAffineStartRegs(-1,-1);
	}
	
	mainEngine->ParseAllRegisters();
	subEngine->ParseAllRegisters();
	
	return !is.fail();
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
	free_aligned(this->_internalRenderLineTargetCustom);
	this->_internalRenderLineTargetCustom = NULL;
	free_aligned(this->_renderLineLayerIDCustom);
	this->_renderLineLayerIDCustom = NULL;
	free_aligned(this->_deferredIndexCustom);
	this->_deferredIndexCustom = NULL;
	free_aligned(this->_deferredColorCustom);
	this->_deferredColorCustom = NULL;
	
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
	
	memset(this->_sprColor, 0, sizeof(this->_sprColor));
	memset(this->_sprAlpha, 0, sizeof(this->_sprAlpha));
	memset(this->_sprType, OBJMode_Normal, sizeof(this->_sprType));
	memset(this->_sprPrio, 0x7F, sizeof(this->_sprPrio));
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
		memset(this->_internalRenderLineTargetCustom, 0, dispInfo.customWidth * _gpuLargestDstLineCount * dispInfo.pixelBytes);
	}
	if (this->_renderLineLayerIDCustom != NULL)
	{
		memset(this->_renderLineLayerIDCustom, 0, dispInfo.customWidth * _gpuLargestDstLineCount * 4 * sizeof(u8));
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
	renderState.selectedLayerID = GPULayerID_BG0;
	renderState.selectedBGLayer = &this->_BGLayer[GPULayerID_BG0];
	renderState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
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
	
	this->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->renderedBuffer = this->nativeBuffer;
	
	for (size_t line = 0; line < GPU_FRAMEBUFFER_NATIVE_HEIGHT; line++)
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

template <NDSColorFormat COLORFORMAT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend(const __m128i &colA, const __m128i &colB, const __m128i &blendEVA, const __m128i &blendEVB)
{
#ifdef ENABLE_SSSE3
	__m128i blendAB = _mm_or_si128(blendEVA, _mm_slli_epi16(blendEVB, 8));
#endif
	
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
		outColorLo = _mm_unpacklo_epi8(colA, colB);
		outColorHi = _mm_unpackhi_epi8(colA, colB);
		
		outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
		outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
#else
		__m128i colALo = _mm_unpacklo_epi8(colA, _mm_setzero_si128());
		__m128i colAHi = _mm_unpackhi_epi8(colA, _mm_setzero_si128());
		__m128i colBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
		__m128i colBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
		
		outColorLo = _mm_add_epi16( _mm_mullo_epi16(colALo, blendEVA), _mm_mullo_epi16(colBLo, blendEVB) );
		outColorHi = _mm_add_epi16( _mm_mullo_epi16(colAHi, blendEVA), _mm_mullo_epi16(colBHi, blendEVB) );
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
void GPUEngineBase::_RenderLine_Clear(GPUEngineCompositorInfo &compInfo)
{
	// Clear the current line with the clear color
	u16 dstClearColor16 = compInfo.renderState.backdropColor16;
	
	if (compInfo.renderState.srcEffectEnable[GPULayerID_Backdrop])
	{
		if (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness)
		{
			dstClearColor16 = compInfo.renderState.brightnessUpTable555[compInfo.renderState.backdropColor16];
		}
		else if (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)
		{
			dstClearColor16 = compInfo.renderState.brightnessDownTable555[compInfo.renderState.backdropColor16];
		}
	}
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(*compInfo.target.lineColor, dstClearColor16);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(*compInfo.target.lineColor, COLOR555TO666(LOCAL_TO_LE_16(dstClearColor16)));
			break;
			
		case NDSColorFormat_BGR888_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(*compInfo.target.lineColor, COLOR555TO888(LOCAL_TO_LE_16(dstClearColor16)));
			break;
	}
	
	memset(this->_renderLineLayerIDNative, GPULayerID_Backdrop, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprWin, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	// init pixels priorities
	assert(NB_PRIORITIES == 4);
	this->_itemsForPriority[0].nbPixelsX = 0;
	this->_itemsForPriority[1].nbPixelsX = 0;
	this->_itemsForPriority[2].nbPixelsX = 0;
	this->_itemsForPriority[3].nbPixelsX = 0;
}

void GPUEngineBase::UpdateRenderStates(const size_t l)
{
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	
	this->_currentRenderState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	compInfo.renderState = this->_currentRenderState;
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

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void GPUEngineBase::_LineCopy(void *__restrict dstBuffer, const void *__restrict srcBuffer, const size_t l)
{
	switch (INTEGERSCALEHINT)
	{
		case 0:
		{
			const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
			const size_t lineIndex = _gpuCaptureLineIndex[l];
			const size_t lineCount = _gpuCaptureLineCount[l];
			
			const void *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (lineIndex * lineWidth * ELEMENTSIZE) : (u8 *)srcBuffer;
			void *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (lineIndex * lineWidth * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			CopyLineExpand<INTEGERSCALEHINT, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, lineWidth * lineCount);
			break;
		}
			
		case 1:
		{
			const void *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)srcBuffer;
			void *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			CopyLineExpand<INTEGERSCALEHINT, NEEDENDIANSWAP, ELEMENTSIZE>(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH);
			break;
		}
			
		default:
		{
			const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
			const size_t lineCount = _gpuCaptureLineCount[l];
			const size_t lineIndex = _gpuCaptureLineIndex[l];
			
			const void *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * ELEMENTSIZE) : (u8 *)srcBuffer;
			u8 *__restrict dstLineHead = (USELINEINDEX) ? (u8 *)dstBuffer + (lineIndex * lineWidth * ELEMENTSIZE) : (u8 *)dstBuffer;
			
			// TODO: Determine INTEGERSCALEHINT earlier in the pipeline, preferably when the framebuffer is first initialized.
			//
			// The implementation below is a stopgap measure for getting the faster code paths to run.
			// However, this setup is not ideal, since the code size will greatly increase in order to
			// include all possible code paths, possibly causing cache misses on lesser CPUs.
			if (lineWidth == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 2))
			{
				CopyLineExpand<2, NEEDENDIANSWAP, ELEMENTSIZE>(dstLineHead, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 2);
			}
			else if (lineWidth == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 3))
			{
				CopyLineExpand<3, NEEDENDIANSWAP, ELEMENTSIZE>(dstLineHead, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 3);
			}
			else if (lineWidth == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 4))
			{
				CopyLineExpand<4, NEEDENDIANSWAP, ELEMENTSIZE>(dstLineHead, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
			}
			else if ((lineWidth % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0)
			{
				CopyLineExpand<0xFFFF, NEEDENDIANSWAP, ELEMENTSIZE>(dstLineHead, src, lineWidth);
			}
			else
			{
				CopyLineExpand<-1, NEEDENDIANSWAP, ELEMENTSIZE>(dstLineHead, src, lineWidth);
			}
			
			u8 *__restrict dst = (u8 *)dstLineHead + (lineWidth * ELEMENTSIZE);
			
			for (size_t line = 1; line < lineCount; line++)
			{
				memcpy(dst, dstLineHead, lineWidth * ELEMENTSIZE);
				dst += (lineWidth * ELEMENTSIZE);
			}
			
			break;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_TransitionLineNativeToCustom(GPUEngineCompositorInfo &compInfo)
{
	if (this->isLineRenderNative[compInfo.line.indexNative])
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineCopy<0xFFFF, false, false, 2>(compInfo.target.lineColorHeadCustom, compInfo.target.lineColorHeadNative, 0);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			case NDSColorFormat_BGR888_Rev:
				this->_LineCopy<0xFFFF, false, false, 4>(compInfo.target.lineColorHeadCustom, compInfo.target.lineColorHeadNative, 0);
				break;
		}
		
		this->_LineCopy<0xFFFF, false, false, 1>(compInfo.target.lineLayerIDHeadCustom, compInfo.target.lineLayerIDHeadNative, 0);
		
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
FORCEINLINE void GPUEngineBase::_PixelUnknownEffect(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const u8 spriteAlpha, const bool enableColorEffect)
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
		const OBJMode objMode = (OBJMode)this->_sprType[compInfo.target.xNative];
		const bool isObjTranslucentType = (objMode == OBJMode_Transparent) || (objMode == OBJMode_Bitmap);
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
FORCEINLINE void GPUEngineBase::_PixelUnknownEffect(GPUEngineCompositorInfo &compInfo, const FragmentColor srcColor32, const u8 spriteAlpha, const bool enableColorEffect)
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
		const OBJMode objMode = (OBJMode)this->_sprType[compInfo.target.xNative];
		const bool isObjTranslucentType = (objMode == OBJMode_Transparent) || (objMode == OBJMode_Bitmap);
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
FORCEINLINE void GPUEngineBase::_PixelComposite(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const u8 spriteAlpha, const bool enableColorEffect)
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
			this->_PixelUnknownEffect<OUTPUTFORMAT, LAYERTYPE>(compInfo, srcColor16, spriteAlpha, enableColorEffect);
			break;
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
FORCEINLINE void GPUEngineBase::_PixelComposite(GPUEngineCompositorInfo &compInfo, FragmentColor srcColor32, const u8 spriteAlpha, const bool enableColorEffect)
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
			this->_PixelUnknownEffect<OUTPUTFORMAT, LAYERTYPE>(compInfo, srcColor32, spriteAlpha, enableColorEffect);
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
																   const __m128i &spriteAlpha,
																   const __m128i &srcEffectEnableMask,
																   const __m128i &enableColorEffectMask,
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
	__m128i eva_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVA);
	__m128i evb_vec128 = _mm_set1_epi16(compInfo.renderState.blendEVB);
	__m128i forceDstTargetBlendMask = (LAYERTYPE == GPULayerType_3D) ? dstTargetBlendEnableMask : _mm_setzero_si128();
	
	if (LAYERTYPE == GPULayerType_OBJ)
	{
		const __m128i objMode_vec128 = _mm_loadu_si128((__m128i *)(this->_sprType + compInfo.target.xNative));
		const __m128i isObjTranslucentMask = _mm_and_si128( dstTargetBlendEnableMask, _mm_or_si128(_mm_cmpeq_epi8(objMode_vec128, _mm_set1_epi8(OBJMode_Transparent)), _mm_cmpeq_epi8(objMode_vec128, _mm_set1_epi8(OBJMode_Bitmap))) );
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
		
		if (LAYERTYPE == GPULayerType_3D)
		{
			blendSrc16[0] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src0, src1, dst0);
			blendSrc16[1] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src2, src3, dst1);
		}
		else
		{
			blendSrc16[0] = this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[0], dst0, eva_vec128, evb_vec128);
			blendSrc16[1] = this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[1], dst1, eva_vec128, evb_vec128);
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
		
		if (LAYERTYPE == GPULayerType_3D)
		{
			blendSrc32[0] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src0, src0, dst0);
			blendSrc32[1] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src1, src1, dst1);
			blendSrc32[2] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src2, src2, dst2);
			blendSrc32[3] = this->_ColorEffectBlend3D<OUTPUTFORMAT>(src3, src3, dst3);
		}
		else
		{
			blendSrc32[0] = this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[0], dst0, eva_vec128, evb_vec128);
			blendSrc32[1] = this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[1], dst1, eva_vec128, evb_vec128);
			blendSrc32[2] = this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[2], dst2, eva_vec128, evb_vec128);
			blendSrc32[3] = this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[3], dst3, eva_vec128, evb_vec128);
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
													   const __m128i &srcEffectEnableMask)
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
				const __m128i spriteAlpha = _mm_setzero_si128();
				const __m128i enableColorEffectMask = (WILLPERFORMWINDOWTEST) ? _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom)), _mm_set1_epi8(1) ) : _mm_set1_epi8(0xFF);
				
				this->_PixelUnknownEffectWithMask16_SSE2<OUTPUTFORMAT, LAYERTYPE>(compInfo,
																				  passMask8,
																				  src3, src2, src1, src0,
																				  spriteAlpha,
																				  srcEffectEnableMask,
																				  enableColorEffectMask,
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
	this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG>(compInfo, srcColor16, 0, enableColorEffect);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeLineDeferred(GPUEngineCompositorInfo &compInfo)
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
	
	CopyLineExpand<0xFFFF, false, 2>(this->_deferredColorCustom, this->_deferredColorNative, compInfo.line.widthCustom);
	CopyLineExpand<0xFFFF, false, 1>(this->_deferredIndexCustom, this->_deferredIndexNative, compInfo.line.widthCustom);
	
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = (compInfo.line.widthCustom - (compInfo.line.widthCustom % 16));
	const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[compInfo.renderState.selectedLayerID];
#endif
	
	for (size_t l = 0; l < compInfo.line.renderCount; l++)
	{
		compInfo.target.xNative = 0;
		compInfo.target.xCustom = 0;
		
#ifdef ENABLE_SSE2
		for (; compInfo.target.xCustom < ssePixCount; compInfo.target.xCustom+=16, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
		{
			__m128i passMask8;
			
			if (WILLPERFORMWINDOWTEST)
			{
				// Do the window test.
				passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom)), _mm_set1_epi8(1) );
			}
			else
			{
				passMask8 = _mm_set1_epi8(0xFF);
			}
			
			// Do the index test. Pixels with an index value of 0 are rejected.
			passMask8 = _mm_andnot_si128(_mm_cmpeq_epi8(_mm_load_si128((__m128i *)(this->_deferredIndexCustom + compInfo.target.xCustom)), _mm_setzero_si128()), passMask8);
			
			const int passMaskValue = _mm_movemask_epi8(passMask8);
			
			// If none of the pixels within the vector pass, then reject them all at once.
			if (passMaskValue == 0)
			{
				continue;
			}
			
			__m128i src[4];
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				src[0] = _mm_load_si128((__m128i *)(this->_deferredColorCustom + compInfo.target.xCustom + 0));
				src[1] = _mm_load_si128((__m128i *)(this->_deferredColorCustom + compInfo.target.xCustom + 8));
			}
			else
			{
				const __m128i src16[2] = { _mm_load_si128((__m128i *)(this->_deferredColorCustom + compInfo.target.xCustom + 0)),
										   _mm_load_si128((__m128i *)(this->_deferredColorCustom + compInfo.target.xCustom + 8)) };
				
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
			const bool didAllPixelsPass = (passMaskValue == 0xFFFF);
			this->_PixelComposite16_SSE2<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG, WILLPERFORMWINDOWTEST>(compInfo,
																											   didAllPixelsPass,
																											   passMask8,
																											   src[3], src[2], src[1], src[0],
																											   srcEffectEnableMask);
		}
#endif
		
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; compInfo.target.xCustom < compInfo.line.widthCustom; compInfo.target.xCustom++, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] == 0) )
			{
				continue;
			}
			
			if (this->_deferredIndexCustom[compInfo.target.xCustom] == 0)
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG>(compInfo, this->_deferredColorCustom[compInfo.target.xCustom], 0, enableColorEffect);
		}
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeVRAMLineDeferred(GPUEngineCompositorInfo &compInfo)
{
	const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(compInfo.renderState.selectedBGLayer->BMPAddress, compInfo.line.blockOffsetCustom);
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[compInfo.renderState.selectedLayerID];
	
	const size_t ssePixCount = (compInfo.line.pixelCount - (compInfo.line.pixelCount % 16));
	for (; i < ssePixCount; i+=16, compInfo.target.xCustom+=16, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
	{
		__m128i src[4];
		__m128i passMask8;
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
			{
				const __m128i src16[2] = { _mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 0)), _mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 8)) };
				src[0] = src16[0];
				src[1] = src16[1];
				passMask8 = _mm_packus_epi16( _mm_srli_epi16(src16[0], 15), _mm_srli_epi16(src16[1], 15) );
				passMask8 = _mm_cmpeq_epi8(passMask8, _mm_set1_epi8(1));
				break;
			}
				
			case NDSColorFormat_BGR666_Rev:
			{
				const __m128i src16[2] = { _mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 0)), _mm_load_si128((__m128i *)((u16 *)vramColorPtr + i + 8)) };
				ColorspaceConvert555To6665Opaque_SSE2<false>(src16[0], src[0], src[1]);
				ColorspaceConvert555To6665Opaque_SSE2<false>(src16[1], src[2], src[3]);
				passMask8 = _mm_packus_epi16( _mm_srli_epi16(src16[0], 15), _mm_srli_epi16(src16[1], 15) );
				passMask8 = _mm_cmpeq_epi8(passMask8, _mm_set1_epi8(1));
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
				src[0] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 0));
				src[1] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 4));
				src[2] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 8));
				src[3] = _mm_load_si128((__m128i *)((FragmentColor *)vramColorPtr + i + 12));
				passMask8 = _mm_packus_epi16( _mm_packs_epi32(_mm_srli_epi32(src[0], 24), _mm_srli_epi32(src[1], 24)), _mm_packs_epi32(_mm_srli_epi32(src[2], 24), _mm_srli_epi32(src[3], 24)) );
				passMask8 = _mm_cmpeq_epi8(passMask8, _mm_setzero_si128());
				passMask8 = _mm_xor_si128(passMask8, _mm_set1_epi32(0xFFFFFFFF));
				break;
		}
		
		if (WILLPERFORMWINDOWTEST)
		{
			// Do the window test.
			passMask8 = _mm_andnot_si128(_mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom)), _mm_setzero_si128()), passMask8);
		}
		
		const int passMaskValue = _mm_movemask_epi8(passMask8);
		
		// If none of the pixels within the vector pass, then reject them all at once.
		if (passMaskValue == 0)
		{
			continue;
		}
		
		// Write out the pixels.
		const bool didAllPixelsPass = (passMaskValue == 0xFFFF);
		this->_PixelComposite16_SSE2<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG, WILLPERFORMWINDOWTEST>(compInfo,
																										   didAllPixelsPass,
																										   passMask8,
																										   src[3], src[2], src[1], src[0],
																										   srcEffectEnableMask);
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < compInfo.line.pixelCount; i++, compInfo.target.xCustom++, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
	{
		if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] == 0) )
		{
			continue;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			if ((((u32 *)vramColorPtr)[i] & 0xFF000000) == 0)
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG>(compInfo, ((u32 *)vramColorPtr)[i], 0, enableColorEffect);
		}
		else
		{
			if ((((u16 *)vramColorPtr)[i] & 0x8000) == 0)
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
			this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG>(compInfo, ((u16 *)vramColorPtr)[i], 0, enableColorEffect);
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
						outUseCustomVRAM = !GPU->GetEngineMain()->isLineCaptureNative[blockID][compInfo.line.indexNative + blockLine];
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
FORCEINLINE void GPUEngineBase::_RenderSpriteUpdatePixel(size_t frameX,
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
		this->_sprWin[frameX] = 1;
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
void GPUEngineBase::_RenderSpriteBMP(const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
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
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, true>(frameX, &vramColor, spriteAlpha+1, OBJMode_Bitmap, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite256(const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									 const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	for (size_t i = 0; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u32 palIndexAddress = objAddress + (u32)( (spriteX & 0x0007) + ((spriteX & 0xFFF8) << 3) );
		const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(palIndexAddress);
		const u8 idx8 = *palIndexBuffer;
		
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(frameX, palColorBuffer, idx8, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite16(const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
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
		
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(frameX, palColorBuffer, idx4, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
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
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, true>(frameX, &vramColor, spriteInfo.PaletteIndex, OBJMode_Bitmap, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
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
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(frameX, palColorBuffer, idx8, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
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
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(frameX, palColorBuffer, idx4, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
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
				
				this->_RenderSpriteBMP<ISDEBUGRENDER>(objAddress, length, frameX, spriteX, readXStep,
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
					
					if (!GPU->GetEngineMain()->isLineCaptureNative[blockID][blockLine] && (linePixel == 0))
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
				
				this->_RenderSprite256<ISDEBUGRENDER>(objAddress, length, frameX, spriteX, readXStep,
													  palColorBuffer, objMode, prio, spriteNum,
													  dst, dst_alpha, typeTab, prioTab);
			}
			else // 16 colors; handles OBJ windows too
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((spriteY>>3)<<10) + ((spriteY&0x7)*4) :
				                                                             this->_sprMem + (spriteInfo.TileIndex<<compInfo.renderState.spriteBoundary) + ((spriteY>>3)*sprSize.width*4) + ((spriteY&0x7)*4);
				
				const u16 *__restrict palColorBuffer = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);
				
				this->_RenderSprite16<ISDEBUGRENDER>(objAddress, length, frameX, spriteX, readXStep,
													 palColorBuffer, objMode, prio, spriteNum,
													 dst, dst_alpha, typeTab, prioTab);
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_Layers(const size_t l)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	itemsForPriority_t *item;
	
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	
	// Optimization: For normal display mode, render straight to the output buffer when that is what we are going to end
	// up displaying anyway. Otherwise, we need to use the working buffer.
	compInfo.target.lineColorHeadNative = (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->nativeBuffer + (compInfo.line.blockOffsetNative * dispInfo.pixelBytes) : (u8 *)this->_internalRenderLineTargetNative;
	compInfo.target.lineColorHeadCustom = (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->customBuffer + (compInfo.line.blockOffsetCustom * dispInfo.pixelBytes) : (u8 *)this->_internalRenderLineTargetCustom;
	compInfo.target.lineColorHead = compInfo.target.lineColorHeadNative;
	
	compInfo.target.lineLayerIDHeadNative = this->_renderLineLayerIDNative;
	compInfo.target.lineLayerIDHeadCustom = this->_renderLineLayerIDCustom;
	compInfo.target.lineLayerIDHead = compInfo.target.lineLayerIDHeadNative;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
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
		}
	}
}

void GPUEngineBase::_RenderLine_SetupSprites(GPUEngineCompositorInfo &compInfo)
{
	itemsForPriority_t *item;
	
	//n.b. - this is clearing the sprite line buffer to the background color,
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_sprColor, compInfo.renderState.backdropColor16);
	memset(this->_sprAlpha, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprType, OBJMode_Normal, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprPrio, 0x7F, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
	//how it interacts with this. I wish we knew why we needed this
	
	this->_SpriteRender<false>(compInfo, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
	this->_MosaicSpriteLine(compInfo, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		// assign them to the good priority item
		const size_t prio = this->_sprPrio[i];
		if (prio >= 4) continue;
		
		item = &(this->_itemsForPriority[prio]);
		item->PixelsX[item->nbPixelsX] = i;
		item->nbPixelsX++;
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
			useCustomVRAM = !GPU->GetEngineMain()->isLineCaptureNative[blockID][blockLine];
		}
	}
	
	if (useCustomVRAM && ((OUTPUTFORMAT != NDSColorFormat_BGR888_Rev) || GPU->GetDisplayInfo().isCustomSizeRequested))
	{
		this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
	}
	
	if (this->isLineRenderNative[compInfo.line.indexNative])
	{
		if (useCustomVRAM && (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev))
		{
			const FragmentColor *__restrict vramColorPtr = (FragmentColor *)GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->vramBlockOBJAddress, 0);
			
			for (size_t i = 0; i < item->nbPixelsX; i++)
			{
				const size_t srcX = item->PixelsX[i];
				
				if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][srcX] == 0) )
				{
					continue;
				}
				
				compInfo.target.xNative = srcX;
				compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
				compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead + srcX;
				compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead + srcX;
				compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead + srcX;
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
				this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, vramColorPtr[srcX], this->_sprAlpha[srcX], enableColorEffect);
			}
		}
		else
		{
			for (size_t i = 0; i < item->nbPixelsX; i++)
			{
				const size_t srcX = item->PixelsX[i];
				
				if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][srcX] == 0) )
				{
					continue;
				}
				
				compInfo.target.xNative = srcX;
				compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
				compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead + srcX;
				compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead + srcX;
				compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead + srcX;
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
				this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, this->_sprColor[srcX], this->_sprAlpha[srcX], enableColorEffect);
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
					
					if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][srcX] == 0) )
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
						
						const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
						
						if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
						{
							this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, ((FragmentColor *)vramColorPtr)[dstX], this->_sprAlpha[srcX], enableColorEffect);
						}
						else
						{
							this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, ((u16 *)vramColorPtr)[dstX], this->_sprAlpha[srcX], enableColorEffect);
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
					
					if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][srcX] == 0) )
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
						
						const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
						this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, this->_sprColor[srcX], this->_sprAlpha[srcX], enableColorEffect);
					}
				}
				
				dstColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void *)((u16 *)dstColorPtr + compInfo.line.widthCustom) : (void *)((FragmentColor *)dstColorPtr + compInfo.line.widthCustom);
				dstLayerIDPtr += compInfo.line.widthCustom;
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
				win_vec128 = _mm_load_si128((__m128i *)(this->_sprWin + i));
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
				if (this->_sprWin[i] != 0)
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
			CopyLineExpand<1, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH);
			CopyLineExpand<1, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 2))
		{
			CopyLineExpand<2, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 2);
			CopyLineExpand<2, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 2);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 3))
		{
			CopyLineExpand<3, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 3);
			CopyLineExpand<3, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 3);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 4))
		{
			CopyLineExpand<4, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
			CopyLineExpand<4, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
		}
		else if ((compInfo.line.widthCustom % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0)
		{
			CopyLineExpand<0xFFFF, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compInfo.line.widthCustom);
			CopyLineExpand<0xFFFF, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compInfo.line.widthCustom);
		}
		else
		{
			CopyLineExpand<-1, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compInfo.line.widthCustom);
			CopyLineExpand<-1, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compInfo.line.widthCustom);
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
			this->_CompositeVRAMLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo);
		}
		else
		{
			this->_CompositeLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo);
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
			memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u16 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFF);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u32 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0x1F3F3F3F);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u32 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFFFFFF);
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
	this->nativeBuffer = (theDisplayID == NDSDisplayID_Main) ? dispInfo.nativeBuffer[NDSDisplayID_Main] : dispInfo.nativeBuffer[NDSDisplayID_Touch];
	this->customBuffer = (theDisplayID == NDSDisplayID_Main) ? dispInfo.customBuffer[NDSDisplayID_Main] : dispInfo.customBuffer[NDSDisplayID_Touch];
	
	this->_targetDisplayID = theDisplayID;
}

GPUEngineID GPUEngineBase::GetEngineID() const
{
	return this->_engineID;
}

void GPUEngineBase::SetCustomFramebufferSize(size_t w, size_t h)
{
	void *oldWorkingLineColor = this->_internalRenderLineTargetCustom;
	u8 *oldWorkingLineLayerID = this->_renderLineLayerIDCustom;
	u8 *oldDeferredIndexCustom = this->_deferredIndexCustom;
	u16 *oldDeferredColorCustom = this->_deferredColorCustom;
	u8 *oldDidPassWindowTestCustomMasterPtr = this->_didPassWindowTestCustomMasterPtr;
	
	void *newWorkingLineColor = malloc_alignedCacheLine(w * _gpuLargestDstLineCount * GPU->GetDisplayInfo().pixelBytes);
	u8 *newWorkingLineLayerID = (u8 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * 4 * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	u8 *newDeferredIndexCustom = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u16 *newDeferredColorCustom = (u16 *)malloc_alignedCacheLine(w * sizeof(u16));
	u8 *newDidPassWindowTestCustomMasterPtr = (u8 *)malloc_alignedCacheLine(w * 10 * sizeof(u8));
	
	this->_internalRenderLineTargetCustom = newWorkingLineColor;
	this->_renderLineLayerIDCustom = newWorkingLineLayerID;
	this->_deferredIndexCustom = newDeferredIndexCustom;
	this->_deferredColorCustom = newDeferredColorCustom;
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->nativeBuffer = (this->_targetDisplayID == NDSDisplayID_Main) ? dispInfo.nativeBuffer[NDSDisplayID_Main] : dispInfo.nativeBuffer[NDSDisplayID_Touch];
	this->customBuffer = (this->_targetDisplayID == NDSDisplayID_Main) ? dispInfo.customBuffer[NDSDisplayID_Main] : dispInfo.customBuffer[NDSDisplayID_Touch];
	
	if (this->nativeLineOutputCount == GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		this->renderedBuffer = this->nativeBuffer;
		this->renderedWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	else
	{
		this->renderedBuffer = this->customBuffer;
		this->renderedWidth  = dispInfo.customWidth;
		this->renderedHeight = dispInfo.customHeight;
	}
	
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
	
	for (size_t line = 0; line < GPU_FRAMEBUFFER_NATIVE_HEIGHT; line++)
	{
		GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[line].line;
		
		lineInfo.indexNative = line;
		lineInfo.indexCustom = _gpuDstLineIndex[lineInfo.indexNative];
		lineInfo.widthCustom = GPU->GetDisplayInfo().customWidth;
		lineInfo.renderCount = _gpuDstLineCount[lineInfo.indexNative];
		lineInfo.pixelCount = lineInfo.widthCustom * lineInfo.renderCount;
		lineInfo.blockOffsetNative = lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetCustom = lineInfo.indexCustom * lineInfo.widthCustom;
		
		this->_currentCompositorInfo[line].target.lineColor = (GPU->GetDisplayInfo().colorFormat == NDSColorFormat_BGR555_Rev) ? (void **)&this->_currentCompositorInfo[line].target.lineColor16 : (void **)&this->_currentCompositorInfo[line].target.lineColor32;
	}
	
	free_aligned(oldWorkingLineColor);
	free_aligned(oldWorkingLineLayerID);
	free_aligned(oldDeferredIndexCustom);
	free_aligned(oldDeferredColorCustom);
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
		this->renderedWidth = dispInfo.customWidth;
		this->renderedHeight = dispInfo.customHeight;
		this->renderedBuffer = this->customBuffer;
		return;
	}
	
	// Resolve any remaining native lines to the custom buffer
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			if (this->isLineOutputNative[y])
			{
				this->_LineCopy<0xFFFF, true, false, 2>(this->customBuffer, this->nativeBuffer, y);
				this->isLineOutputNative[y] = false;
			}
		}
	}
	else
	{
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			if (this->isLineOutputNative[y])
			{
				this->_LineCopy<0xFFFF, true, false, 4>(this->customBuffer, this->nativeBuffer, y);
				this->isLineOutputNative[y] = false;
			}
		}
	}
	
	this->nativeLineOutputCount = 0;
	this->renderedWidth = dispInfo.customWidth;
	this->renderedHeight = dispInfo.customHeight;
	this->renderedBuffer = this->customBuffer;
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
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				this->_LineCopy<0xFFFF, true, false, 2>(mutableInfo.customBuffer[this->_targetDisplayID], mutableInfo.nativeBuffer[this->_targetDisplayID], y);
			}
		}
		else if (mutableInfo.pixelBytes == 4)
		{
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				this->_LineCopy<0xFFFF, true, false, 4>(mutableInfo.customBuffer[this->_targetDisplayID], mutableInfo.nativeBuffer[this->_targetDisplayID], y);
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
	
	nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
	nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
	nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
	nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		isLineCaptureNative[0][l] = true;
		isLineCaptureNative[1][l] = true;
		isLineCaptureNative[2][l] = true;
		isLineCaptureNative[3][l] = true;
	}
	
	_3DFramebufferMain = (FragmentColor *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(FragmentColor));
	_3DFramebuffer16 = (u16 *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	_captureWorkingA16 = (u16 *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingB16 = (u16 *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingA32 = (FragmentColor *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(FragmentColor));
	_captureWorkingB32 = (FragmentColor *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(FragmentColor));
	gfx3d_Update3DFramebuffers(_3DFramebufferMain, _3DFramebuffer16);
}

GPUEngineA::~GPUEngineA()
{
	free_aligned(this->_3DFramebufferMain);
	free_aligned(this->_3DFramebuffer16);
	free_aligned(this->_captureWorkingA16);
	free_aligned(this->_captureWorkingB16);
	free_aligned(this->_captureWorkingA32);
	free_aligned(this->_captureWorkingB32);
	gfx3d_Update3DFramebuffers(NULL, NULL);
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
	
	this->ResetCaptureLineStates();
	this->SetTargetDisplayByID(NDSDisplayID_Main);
	
	memset(this->_3DFramebufferMain, 0, dispInfo.customWidth * dispInfo.customHeight * sizeof(FragmentColor));
	memset(this->_3DFramebuffer16, 0, dispInfo.customWidth * dispInfo.customHeight * sizeof(u16));
	memset(this->_captureWorkingA16, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingB16, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingA32, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(FragmentColor));
	memset(this->_captureWorkingB32, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(FragmentColor));
}

void GPUEngineA::ResetCaptureLineStates()
{
	this->nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
	this->nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
	this->nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
	this->nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		this->isLineCaptureNative[0][l] = true;
		this->isLineCaptureNative[1][l] = true;
		this->isLineCaptureNative[2][l] = true;
		this->isLineCaptureNative[3][l] = true;
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

void* GPUEngineA::GetCustomVRAMBlockPtr(const size_t blockID)
{
	return this->_VRAMCustomBlockPtr[blockID];
}

void GPUEngineA::SetCustomFramebufferSize(size_t w, size_t h)
{
	this->GPUEngineBase::SetCustomFramebufferSize(w, h);
	
	FragmentColor *old3DFramebufferMain = this->_3DFramebufferMain;
	u16 *old3DFramebuffer16 = this->_3DFramebuffer16;
	u16 *oldCaptureWorkingA16 = this->_captureWorkingA16;
	u16 *oldCaptureWorkingB16 = this->_captureWorkingB16;
	FragmentColor *oldCaptureWorkingA32 = this->_captureWorkingA32;
	FragmentColor *oldCaptureWorkingB32 = this->_captureWorkingB32;
	
	FragmentColor *new3DFramebufferMain = (FragmentColor *)malloc_alignedCacheLine(w * h * sizeof(FragmentColor));
	u16 *new3DFramebuffer16 = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	u16 *newCaptureWorkingA16 = (u16 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(u16));
	u16 *newCaptureWorkingB16 = (u16 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(u16));
	FragmentColor *newCaptureWorkingA32 = (FragmentColor *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(FragmentColor));
	FragmentColor *newCaptureWorkingB32 = (FragmentColor *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(FragmentColor));
	
	this->_3DFramebufferMain = new3DFramebufferMain;
	this->_3DFramebuffer16 = new3DFramebuffer16;
	this->_captureWorkingA16 = newCaptureWorkingA16;
	this->_captureWorkingB16 = newCaptureWorkingB16;
	this->_captureWorkingA32 = newCaptureWorkingA32;
	this->_captureWorkingB32 = newCaptureWorkingB32;
	gfx3d_Update3DFramebuffers(this->_3DFramebufferMain, this->_3DFramebuffer16);
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	if (dispInfo.colorFormat == NDSColorFormat_BGR888_Rev)
	{
		this->_VRAMCustomBlockPtr[0] = (FragmentColor *)GPU->GetCustomVRAMBuffer();
		this->_VRAMCustomBlockPtr[1] = (FragmentColor *)this->_VRAMCustomBlockPtr[0] + (1 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
		this->_VRAMCustomBlockPtr[2] = (FragmentColor *)this->_VRAMCustomBlockPtr[0] + (2 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
		this->_VRAMCustomBlockPtr[3] = (FragmentColor *)this->_VRAMCustomBlockPtr[0] + (3 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	}
	else
	{
		this->_VRAMCustomBlockPtr[0] = (u16 *)GPU->GetCustomVRAMBuffer();
		this->_VRAMCustomBlockPtr[1] = (u16 *)this->_VRAMCustomBlockPtr[0] + (1 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
		this->_VRAMCustomBlockPtr[2] = (u16 *)this->_VRAMCustomBlockPtr[0] + (2 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
		this->_VRAMCustomBlockPtr[3] = (u16 *)this->_VRAMCustomBlockPtr[0] + (3 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	}
	
	free_aligned(old3DFramebufferMain);
	free_aligned(old3DFramebuffer16);
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
	
	if (this->isLineCaptureNative[blockID][l])
	{
		return false;
	}
	
	u16 *__restrict capturedNativeLine = this->_VRAMNativeBlockCaptureCopyPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	const u16 *__restrict currentNativeLine = this->_VRAMNativeBlockPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	const bool didVRAMLineChange = (memcmp(currentNativeLine, capturedNativeLine, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) != 0);
	if (didVRAMLineChange)
	{
		this->_LineCopy<1, true, false, 2>(this->_VRAMNativeBlockCaptureCopyPtr[blockID], this->_VRAMNativeBlockPtr[blockID], l);
		this->isLineCaptureNative[blockID][l] = true;
		this->nativeLineCaptureCount[blockID]++;
	}
	
	return didVRAMLineChange;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::RenderLine(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	const bool isDisplayCaptureNeeded = this->WillDisplayCapture(l);
	const GPUEngineRenderState &renderState = this->_currentCompositorInfo[l].renderState;
	
	// Render the line
	if ( (renderState.displayOutputMode == GPUDisplayMode_Normal) || isDisplayCaptureNeeded )
	{
		if (renderState.isAnyWindowEnabled)
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, true>(l);
		}
		else
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, false>(l);
		}
	}
	
	// Fill the display output
	switch (renderState.displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			this->_HandleDisplayModeNormal<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_VRAM: // Display vram framebuffer
			this->_HandleDisplayModeVRAM<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_MainMemory: // Display memory FIFO
			this->_HandleDisplayModeMainMemory<OUTPUTFORMAT>(l);
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
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH/2>(l);
		}
		else
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH>(l);
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
	
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (FragmentColor *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	// Horizontally offset the 3D layer by this amount.
	// Test case: Blowing up large objects in Nanostray 2 will cause the main screen to shake horizontally.
	const u16 hofs = (u16)( ((float)compInfo.renderState.selectedBGLayer->xOffset * customWidthScale) + 0.5f );
	
	if (hofs == 0)
	{
#ifdef ENABLE_SSE2
		const size_t ssePixCount = (compInfo.line.widthCustom - (compInfo.line.widthCustom % 16));
		const __m128i srcEffectEnableMask = compInfo.renderState.srcEffectEnable_SSE2[compInfo.renderState.selectedLayerID];
#endif
		
		for (size_t line = 0; line < compInfo.line.renderCount; line++)
		{
			compInfo.target.xNative = 0;
			compInfo.target.xCustom = 0;
			
#ifdef ENABLE_SSE2
			for (; compInfo.target.xCustom < ssePixCount; srcLinePtr+=16, compInfo.target.xCustom+=16, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16+=16, compInfo.target.lineColor32+=16, compInfo.target.lineLayerID+=16)
			{
				const __m128i src[4]	= { _mm_load_si128((__m128i *)srcLinePtr + 0),
										    _mm_load_si128((__m128i *)srcLinePtr + 1),
										    _mm_load_si128((__m128i *)srcLinePtr + 2),
										    _mm_load_si128((__m128i *)srcLinePtr + 3) };
				
				// Determine which pixels pass by doing the alpha test and the window test.
				const __m128i srcAlpha = _mm_packs_epi16( _mm_packs_epi32(_mm_srli_epi32(src[0], 24), _mm_srli_epi32(src[1], 24)),
														  _mm_packs_epi32(_mm_srli_epi32(src[2], 24), _mm_srli_epi32(src[3], 24)) );
				__m128i passMask8;
				
				if (WILLPERFORMWINDOWTEST)
				{
					// Do the window test.
					passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID] + compInfo.target.xCustom)), _mm_set1_epi8(1) );
				}
				else
				{
					passMask8 = _mm_set1_epi8(0xFF);
				}
				
				// Do the alpha test. Pixels with an alpha value of 0 are rejected.
				passMask8 = _mm_andnot_si128(_mm_cmpeq_epi8(srcAlpha, _mm_setzero_si128()), passMask8);
				
				const int passMaskValue = _mm_movemask_epi8(passMask8);
				
				// If none of the pixels within the vector pass, then reject them all at once.
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
																												   srcEffectEnableMask);
			}
#endif
			
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
			for (; compInfo.target.xCustom < compInfo.line.widthCustom; srcLinePtr++, compInfo.target.xCustom++, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
			{
				if ( (srcLinePtr->a == 0) || (WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] == 0)) )
				{
					continue;
				}
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] != 0) : true;
				this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D>(compInfo, *srcLinePtr, 0, enableColorEffect);
			}
		}
	}
	else
	{
		for (size_t line = 0; line < compInfo.line.renderCount; line++)
		{
			for (compInfo.target.xNative = 0, compInfo.target.xCustom = 0; compInfo.target.xCustom < compInfo.line.widthCustom; compInfo.target.xCustom++, compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom], compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
			{
				if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] == 0) )
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
				
				compInfo.target.xNative = _gpuDstToSrcIndex[compInfo.target.xCustom];
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] != 0) : true;
				this->_PixelComposite<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D>(compInfo, srcLinePtr[srcX], 0, enableColorEffect);
			}
			
			srcLinePtr += compInfo.line.widthCustom;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCapture(const u16 l)
{
	assert( (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH/2) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) );
	
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const bool is3DFramebufferNativeSize = CurrentRenderer->IsFramebufferNativeSize();
	const u8 vramWriteBlock = DISPCAPCNT.VRAMWriteBlock;
	const u8 vramReadBlock = DISPCNT.VRAM_Block;
	const size_t writeLineIndexWithOffset = (DISPCAPCNT.VRAMWriteOffset * 64) + l;
	const size_t readLineIndexWithOffset = (this->_dispCapCnt.readOffset * 64) + l;
	bool newCaptureLineNativeState = true;
	
	//128-wide captures should write linearly into memory, with no gaps
	//this is tested by hotel dusk
	size_t dstNativeOffset = (DISPCAPCNT.VRAMWriteOffset * 64 * GPU_FRAMEBUFFER_NATIVE_WIDTH) + (l * CAPTURELENGTH);
	
	//Read/Write block wrap to 00000h when exceeding 1FFFFh (128k)
	//this has not been tested yet (I thought I needed it for hotel dusk, but it was fixed by the above)
	dstNativeOffset &= 0x0000FFFF;
	
	const u16 *vramNative16 = (u16 *)MMU.blank_memory;
	const u16 *vramCustom16 = (u16 *)GPU->GetCustomVRAMBlankBuffer();
	const u32 *vramCustom32 = (u32 *)GPU->GetCustomVRAMBlankBuffer();
	u16 *dstNative16 = this->_VRAMNativeBlockPtr[vramWriteBlock] + dstNativeOffset;
	bool readNativeVRAM = true;
	bool captureLineNativeState32 = newCaptureLineNativeState;
	
	// Convert 18-bit and 24-bit framebuffers to 15-bit for native screen capture.
	if ( (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.CaptureSrc != 1) )
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				break;
				
			case NDSColorFormat_BGR666_Rev:
				ColorspaceConvertBuffer6665To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingA16, compInfo.line.pixelCount);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				ColorspaceConvertBuffer8888To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingA16, compInfo.line.pixelCount);
				break;
		}
	}
	
	// Convert VRAM for native VRAM capture.
	if ( (DISPCAPCNT.SrcB == 0) && (DISPCAPCNT.CaptureSrc != 0) && (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC) )
	{
		size_t vramNativeOffset = readLineIndexWithOffset * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		vramNativeOffset &= 0x0000FFFF;
		vramNative16 = this->_VRAMNativeBlockPtr[vramReadBlock] + vramNativeOffset;
		
		this->VerifyVRAMLineDidChange(vramReadBlock, readLineIndexWithOffset);
		
		if (!this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset])
		{
			size_t vramCustomOffset = ((this->_dispCapCnt.readOffset * _gpuCaptureLineIndex[64]) + _gpuCaptureLineIndex[l]) * dispInfo.customWidth;
			while (vramCustomOffset >= _gpuVRAMBlockOffset)
			{
				vramCustomOffset -= _gpuVRAMBlockOffset;
			}
			
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
				case NDSColorFormat_BGR666_Rev:
					vramCustom16 = (u16 *)this->_VRAMCustomBlockPtr[vramReadBlock] + vramCustomOffset;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					vramCustom32 = (u32 *)this->_VRAMCustomBlockPtr[vramReadBlock] + vramCustomOffset;
					break;
			}
			
			readNativeVRAM = false;
		}
	}
	
	static CACHE_ALIGN u16 fifoLine16[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	const u16 *srcA16 = (DISPCAPCNT.SrcA == 0) ? ((OUTPUTFORMAT != NDSColorFormat_BGR555_Rev) ? this->_captureWorkingA16 : (u16 *)compInfo.target.lineColorHead) : this->_3DFramebuffer16 + compInfo.line.blockOffsetCustom;
	const u16 *srcB16 = (DISPCAPCNT.SrcB == 0) ? vramNative16 : fifoLine16;
	
	switch (DISPCAPCNT.CaptureSrc)
	{
		case 0: // Capture source is SourceA
		{
			//INFO("Capture source is SourceA\n");
			switch (DISPCAPCNT.SrcA)
			{
				case 0: // Capture screen (BG + OBJ + 3D)
				{
					//INFO("Capture screen (BG + OBJ + 3D)\n");
					if (this->isLineRenderNative[l])
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(srcA16, dstNative16, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, true>(srcA16, dstNative16, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = this->isLineRenderNative[l];
					break;
				}
					
				case 1: // Capture 3D
				{
					//INFO("Capture 3D\n");
					if (is3DFramebufferNativeSize)
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(srcA16, dstNative16, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, false, true>(srcA16, dstNative16, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = is3DFramebufferNativeSize;
					break;
				}
			}
			break;
		}
			
		case 1: // Capture source is SourceB
		{
			//INFO("Capture source is SourceB\n");
			switch (DISPCAPCNT.SrcB)
			{
				case 0: // Capture VRAM
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(srcB16, dstNative16, CAPTURELENGTH, 1);
					newCaptureLineNativeState = this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset];
					break;
				}
					
				case 1: // Capture dispfifo (not yet tested)
				{
					this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine16);
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(srcB16, dstNative16, CAPTURELENGTH, 1);
					newCaptureLineNativeState = true;
					break;
				}
			}
			break;
		}
			
		default: // Capture source is SourceA+B blended
		{
			//INFO("Capture source is SourceA+B blended\n");
			if (DISPCAPCNT.SrcB != 0)
			{
				// fifo - tested by splinter cell chaos theory thermal view
				this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine16);
			}
			
			if (DISPCAPCNT.SrcA == 0)
			{
				if (this->isLineRenderNative[l])
				{
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, true, true, true>(srcA16, srcB16, dstNative16, CAPTURELENGTH, 1);
				}
				else
				{
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, false, true, true>(srcA16, srcB16, dstNative16, CAPTURELENGTH, 1);
				}
				
				newCaptureLineNativeState = this->isLineRenderNative[l] && ((DISPCAPCNT.SrcB != 0) || this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset]);
			}
			else
			{
				if (is3DFramebufferNativeSize)
				{
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, true, true, true>(srcA16, srcB16, dstNative16, CAPTURELENGTH, 1);
					newCaptureLineNativeState = (DISPCAPCNT.SrcB != 0) || this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset];
				}
				else
				{
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, false, true, true>(srcA16, srcB16, dstNative16, CAPTURELENGTH, 1);
					newCaptureLineNativeState = false;
				}
			}
			break;
		}
	}
	
#ifdef ENABLE_SSE2
	MACRODO_N( CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_stream_si128((__m128i *)(this->_VRAMNativeBlockCaptureCopyPtr[vramWriteBlock] + dstNativeOffset) + (X), _mm_load_si128((__m128i *)dstNative16 + (X))) );
#else
	memcpy(this->_VRAMNativeBlockCaptureCopyPtr[vramWriteBlock] + dstNativeOffset, dstNative16, CAPTURELENGTH * sizeof(u16));
#endif
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
	{
		captureLineNativeState32 = newCaptureLineNativeState;
		newCaptureLineNativeState = false;
	}
	
	if (this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] && !newCaptureLineNativeState)
	{
		this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] = false;
		this->nativeLineCaptureCount[vramWriteBlock]--;
	}
	else if (!this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] && newCaptureLineNativeState)
	{
		this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] = true;
		this->nativeLineCaptureCount[vramWriteBlock]++;
	}
	
	if (!this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset])
	{
		const size_t captureLengthExt = (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? dispInfo.customWidth : dispInfo.customWidth / 2;
		const size_t captureLineCount = _gpuCaptureLineCount[l];
		
		size_t dstCustomOffset = (DISPCAPCNT.VRAMWriteOffset * _gpuCaptureLineIndex[64] * dispInfo.customWidth) + (_gpuCaptureLineIndex[l] * captureLengthExt);
		while (dstCustomOffset >= _gpuVRAMBlockOffset)
		{
			dstCustomOffset -= _gpuVRAMBlockOffset;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			static CACHE_ALIGN FragmentColor fifoLine32[GPU_FRAMEBUFFER_NATIVE_WIDTH];
			FragmentColor *dstCustom32 = (FragmentColor *)this->_VRAMCustomBlockPtr[vramWriteBlock] + dstCustomOffset;
			bool isLineCaptureNative32 = ( (vramWriteBlock == vramReadBlock) && (writeLineIndexWithOffset == readLineIndexWithOffset) ) ? captureLineNativeState32 : this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset];
			
			if ( (DISPCAPCNT.SrcB == 1) && (DISPCAPCNT.CaptureSrc != 0) )
			{
				ColorspaceConvertBuffer555To8888Opaque<false, false>(fifoLine16, (u32 *)fifoLine32, GPU_FRAMEBUFFER_NATIVE_WIDTH);
			}
			
			if ( (DISPCAPCNT.SrcB == 0) && (DISPCAPCNT.CaptureSrc != 0) && (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC) )
			{
				if (readNativeVRAM)
				{
					ColorspaceConvertBuffer555To8888Opaque<false, false>(vramNative16, (u32 *)vramCustom32, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				}
			}
			
			const u32 *srcA32 = (DISPCAPCNT.SrcA == 0) ? (u32 *)compInfo.target.lineColorHead : (u32 *)CurrentRenderer->GetFramebuffer() + compInfo.line.blockOffsetCustom;
			const u32 *srcB32 = (DISPCAPCNT.SrcB == 0) ? vramCustom32 : (u32 *)fifoLine32;
			
			switch (DISPCAPCNT.CaptureSrc)
			{
				case 0: // Capture source is SourceA
				{
					switch (DISPCAPCNT.SrcA)
					{
						case 0: // Capture screen (BG + OBJ + 3D)
						{
							if (this->isLineRenderNative[l])
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 0, CAPTURELENGTH, true, false>(srcA32, dstCustom32, captureLengthExt, captureLineCount);
							}
							else
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 0, CAPTURELENGTH, false, false>(srcA32, dstCustom32, captureLengthExt, captureLineCount);
							}
							break;
						}
							
						case 1: // Capture 3D
						{
							if (is3DFramebufferNativeSize)
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 1, CAPTURELENGTH, true, false>(srcA32, dstCustom32, captureLengthExt, captureLineCount);
							}
							else
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 1, CAPTURELENGTH, false, false>(srcA32, dstCustom32, captureLengthExt, captureLineCount);
							}
							break;
						}
					}
					break;
				}
					
				case 1: // Capture source is SourceB
				{
					switch (DISPCAPCNT.SrcB)
					{
						case 0: // Capture VRAM
						{
							if (isLineCaptureNative32)
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 0, CAPTURELENGTH, true, false>(srcB32, dstCustom32, captureLengthExt, captureLineCount);
							}
							else
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 0, CAPTURELENGTH, false, false>(srcB32, dstCustom32, captureLengthExt, captureLineCount);
							}
							break;
						}
							
						case 1: // Capture dispfifo (not yet tested)
						{
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR888_Rev, 1, CAPTURELENGTH, true, false>(srcB32, dstCustom32, captureLengthExt, captureLineCount);
							break;
						}
					}
					break;
				}
					
				default: // Capture source is SourceA+B blended
				{
					u32 *srcCustomA32 = (u32 *)srcA32;
					u32 *srcCustomB32 = (u32 *)srcB32;
					
					if ( (DISPCAPCNT.SrcB == 1) || isLineCaptureNative32 )
					{
						srcCustomB32 = (u32 *)this->_captureWorkingB32;
						this->_LineCopy<0xFFFF, false, false, 4>(srcCustomB32, srcB32, 0);
					}
					
					if (DISPCAPCNT.SrcA == 0)
					{
						if (this->isLineRenderNative[l])
						{
							srcCustomA32 = (u32 *)this->_captureWorkingA32;
							this->_LineCopy<0xFFFF, false, false, 4>(srcCustomA32, srcA32, 0);
						}
					}
					else
					{
						if (is3DFramebufferNativeSize)
						{
							srcCustomA32 = (u32 *)this->_captureWorkingA32;
							this->_LineCopy<0xFFFF, false, false, 4>(srcCustomA32, srcA32, 0);
						}
					}
					
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR888_Rev, CAPTURELENGTH, false, false, false>(srcCustomA32, srcCustomB32, dstCustom32, captureLengthExt, captureLineCount);
					break;
				}
			}
		}
		else
		{
			if (!this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] && (DISPCAPCNT.SrcB == 0))
			{
				srcB16 = vramCustom16;
			}
			
			u16 *dstCustom16 = (u16 *)this->_VRAMCustomBlockPtr[vramWriteBlock] + dstCustomOffset;
			
			switch (DISPCAPCNT.CaptureSrc)
			{
				case 0: // Capture source is SourceA
				{
					switch (DISPCAPCNT.SrcA)
					{
						case 0: // Capture screen (BG + OBJ + 3D)
						{
							if (this->isLineRenderNative[l])
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, false>(srcA16, dstCustom16, captureLengthExt, captureLineCount);
							}
							else
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, false>(srcA16, dstCustom16, captureLengthExt, captureLineCount);
							}
							break;
						}
							
						case 1: // Capture 3D
						{
							if (is3DFramebufferNativeSize)
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, false>(srcA16, dstCustom16, captureLengthExt, captureLineCount);
							}
							else
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, false, false>(srcA16, dstCustom16, captureLengthExt, captureLineCount);
							}
							break;
						}
					}
					break;
				}
					
				case 1: // Capture source is SourceB
				{
					switch (DISPCAPCNT.SrcB)
					{
						case 0: // Capture VRAM
						{
							if (this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset])
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, false>(srcB16, dstCustom16, captureLengthExt, captureLineCount);
							}
							else
							{
								this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, false>(srcB16, dstCustom16, captureLengthExt, captureLineCount);
							}
							break;
						}
							
						case 1: // Capture dispfifo (not yet tested)
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, false>(srcB16, dstCustom16, captureLengthExt, captureLineCount);
							break;
					}
					break;
				}
					
				default: // Capture source is SourceA+B blended
				{
					u16 *srcCustomA16 = (u16 *)srcA16;
					u16 *srcCustomB16 = (u16 *)srcB16;
					
					if ( (DISPCAPCNT.SrcB == 1) || this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
					{
						srcCustomB16 = this->_captureWorkingB16;
						this->_LineCopy<0xFFFF, false, false, 2>(srcCustomB16, srcB16, 0);
					}
					
					if (DISPCAPCNT.SrcA == 0)
					{
						if (this->isLineRenderNative[l])
						{
							srcCustomA16 = this->_captureWorkingA16;
							this->_LineCopy<0xFFFF, false, false, 2>(srcCustomA16, srcA16, 0);
						}
					}
					else
					{
						if (is3DFramebufferNativeSize)
						{
							srcCustomA16 = this->_captureWorkingA16;
							this->_LineCopy<0xFFFF, false, false, 2>(srcCustomA16, srcA16, 0);
						}
					}
					
					this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, false, false, false>(srcCustomA16, srcCustomB16, dstCustom16, captureLengthExt, captureLineCount);
					break;
				}
			}
		}
	}
}

void GPUEngineA::_RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer)
{
#ifdef ENABLE_SSE2
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
	{
		const __m128i fifoColor = _mm_setr_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
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
void GPUEngineA::_RenderLine_DispCapture_Copy(const void *src, void *dst, const size_t captureLengthExt, const size_t captureLineCount)
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
		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		
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
			
			for (size_t line = 1; line < captureLineCount; line++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memcpy((u16 *)dst + (line * dispInfo.customWidth), dst, captureLengthExt * sizeof(u16));
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						memcpy((u32 *)dst + (line * dispInfo.customWidth), dst, captureLengthExt * sizeof(u32));
						break;
				}
			}
		}
		else
		{
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				const size_t pixCountExt = captureLengthExt * captureLineCount;
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
				for (size_t line = 0; line < captureLineCount; line++)
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
							
							src = (u16 *)src + dispInfo.customWidth;
							dst = (u16 *)dst + dispInfo.customWidth;
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
							
							src = (u32 *)src + dispInfo.customWidth;
							dst = (u32 *)dst + dispInfo.customWidth;
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
void GPUEngineA::_RenderLine_DispCapture_BlendToCustomDstBuffer(const void *srcA, const void *srcB, void *dst, const u8 blendEVA, const u8 blendEVB, const size_t length, size_t l)
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
void GPUEngineA::_RenderLine_DispCapture_Blend(const void *srcA, const void *srcB, void *dst, const size_t captureLengthExt, const size_t l)
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
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t captureLineCount = _gpuCaptureLineCount[l];
		
		if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			this->_RenderLine_DispCapture_BlendToCustomDstBuffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt * captureLineCount, l);
		}
		else
		{
			for (size_t line = 0; line < captureLineCount; line++)
			{
				this->_RenderLine_DispCapture_BlendToCustomDstBuffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt, l);
				srcA = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)srcA + lineWidth) : (void *)((u16 *)srcA + lineWidth);
				srcB = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)srcB + lineWidth) : (void *)((u16 *)srcB + lineWidth);
				dst = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)dst + lineWidth) : (void *)((u16 *)dst + lineWidth);
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeVRAM(const size_t l)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, l);
	
	if (this->isLineCaptureNative[DISPCNT.VRAM_Block][l])
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineCopy<1, true, true, 2>(this->nativeBuffer, this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block], l);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				u32 *dst = (u32 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				ColorspaceConvertBuffer555To6665Opaque<false, false>(src, dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				const u16 *src = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				u32 *dst = (u32 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				ColorspaceConvertBuffer555To8888Opaque<false, false>(src, dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				break;
			}
		}
	}
	else
	{
		const size_t customWidth = GPU->GetDisplayInfo().customWidth;
		const size_t customPixCount = customWidth * _gpuDstLineCount[l];
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineCopy<0, true, true, 2>(this->customBuffer, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], l);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = (u16 *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + (_gpuDstLineIndex[l] * customWidth);
				u32 *dst = (u32 *)this->customBuffer + (_gpuDstLineIndex[l] * customWidth);
				ColorspaceConvertBuffer555To6665Opaque<false, false>(src, dst, customPixCount);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				if (GPU->GetDisplayInfo().isCustomSizeRequested)
				{
					this->_LineCopy<0, true, true, 4>(this->customBuffer, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], l);
				}
				else
				{
					this->_LineCopy<1, true, true, 4>(this->nativeBuffer, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], l);
				}
				break;
			}
		}
		
		if ((OUTPUTFORMAT != NDSColorFormat_BGR888_Rev) || GPU->GetDisplayInfo().isCustomSizeRequested)
		{
			this->isLineOutputNative[l] = false;
			this->nativeLineOutputCount--;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeMainMemory(const size_t l)
{
	// Native rendering only.
	//
	//this has not been tested since the dma timing for dispfifo was changed around the time of
	//newemuloop. it may not work.
	
	u32 *dstColorLine = (u32 *)((u16 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH));
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
		{
			u32 *dst = dstColorLine;
			
#ifdef ENABLE_SSE2
			const __m128i alphaBit = _mm_set1_epi16(0x8000);
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
			{
				const __m128i fifoColor = _mm_setr_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
				_mm_store_si128((__m128i *)dst + i, _mm_or_si128(fifoColor, alphaBit));
			}
#else
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
			{
				dst[i] = DISP_FIFOrecv() | 0x80008000;
			}
#endif
			break;
		}
			
		case NDSColorFormat_BGR666_Rev:
		{
			FragmentColor *dst = (FragmentColor *)dstColorLine;
			
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				u32 src = DISP_FIFOrecv();
				dst[i+0].color = COLOR555TO6665_OPAQUE((src >>  0) & 0x7FFF);
				dst[i+1].color = COLOR555TO6665_OPAQUE((src >> 16) & 0x7FFF);
			}
			break;
		}
			
		case NDSColorFormat_BGR888_Rev:
		{
			FragmentColor *dst = (FragmentColor *)dstColorLine;
			
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				u32 src = DISP_FIFOrecv();
				dst[i+0].color = COLOR555TO8888_OPAQUE((src >>  0) & 0x7FFF);
				dst[i+1].color = COLOR555TO8888_OPAQUE((src >> 16) & 0x7FFF);
			}
			break;
		}
	}
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
	const GPUEngineRenderState &renderState = this->_currentCompositorInfo[l].renderState;
	
	switch (renderState.displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff<OUTPUTFORMAT>(l);
			break;
		
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
		{
			if (renderState.isAnyWindowEnabled)
			{
				this->_RenderLine_Layers<OUTPUTFORMAT, true>(l);
			}
			else
			{
				this->_RenderLine_Layers<OUTPUTFORMAT, false>(l);
			}
			
			this->_HandleDisplayModeNormal<OUTPUTFORMAT>(l);
			break;
		}
			
		default:
			break;
	}
}

GPUSubsystem::GPUSubsystem()
{
	ColorspaceHandlerInit();
	
	_defaultEventHandler = new GPUEventHandlerDefault;
	_event = _defaultEventHandler;
	
	gfx3d_init();
	
	_engineMain = GPUEngineA::Allocate();
	_engineSub = GPUEngineB::Allocate();
	
	_display[NDSDisplayID_Main] = new NDSDisplay(NDSDisplayID_Main);
	_display[NDSDisplayID_Main]->SetEngine(_engineMain);
	_display[NDSDisplayID_Touch] = new NDSDisplay(NDSDisplayID_Touch);
	_display[NDSDisplayID_Touch]->SetEngine(_engineSub);
	
	_pending3DRendererID = RENDERID_NULL;
	_needChange3DRenderer = false;
	
	_videoFrameCount = 0;
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
	
	_displayInfo.framebufferSize = ((GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT) + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT)) * 2 * _displayInfo.pixelBytes;
	_masterFramebuffer = malloc_alignedPage(_displayInfo.framebufferSize * 2);
	_displayInfo.masterFramebufferHead = _masterFramebuffer;
	
	_displayInfo.isDisplayEnabled[NDSDisplayID_Main]  = true;
	_displayInfo.isDisplayEnabled[NDSDisplayID_Touch] = true;
	
	_displayInfo.bufferIndex = 0;
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
	this->_videoFrameCount++;
	if (this->_videoFrameCount == 60)
	{
		this->_render3DFrameCount = gfx3d.render3DFrameCount;
		gfx3d.render3DFrameCount = 0;
		this->_videoFrameCount = 0;
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
	if (this->_customVRAM == NULL)
	{
		this->SetCustomFramebufferSize(this->_displayInfo.customWidth, this->_displayInfo.customHeight);
	}
	
	this->_willFrameSkip = false;
	this->_videoFrameCount = 0;
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
		this->_event->DidFrameEnd(false, this->_displayInfo);
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
	this->_engineMain->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineMain->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineSub->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineSub->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->_engineMain->isLineRenderNative[l] = true;
		this->_engineMain->isLineOutputNative[l] = true;
		this->_engineSub->isLineRenderNative[l] = true;
		this->_engineSub->isLineOutputNative[l] = true;
	}
	
	this->_displayInfo.bufferIndex = (this->_displayInfo.bufferIndex + 1) & 0x01;
	
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes;
	const size_t customFramebufferSize = this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes;
	
	this->_displayInfo.masterNativeBuffer = (u8 *)this->_masterFramebuffer + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferSize);
	this->_displayInfo.masterCustomBuffer = (u8 *)this->_masterFramebuffer + (nativeFramebufferSize * 2) + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferSize);
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
	
	this->_engineMain->nativeBuffer = (this->_engineMain->GetTargetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.nativeBuffer[NDSDisplayID_Main] : this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	this->_engineMain->customBuffer = (this->_engineMain->GetTargetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.customBuffer[NDSDisplayID_Main] : this->_displayInfo.customBuffer[NDSDisplayID_Touch];
	this->_engineMain->renderedBuffer = this->_engineMain->nativeBuffer;
	this->_engineMain->renderedWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_engineMain->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	this->_engineSub->nativeBuffer  = (this->_engineSub->GetTargetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.nativeBuffer[NDSDisplayID_Main] : this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	this->_engineSub->customBuffer  = (this->_engineSub->GetTargetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.customBuffer[NDSDisplayID_Main] : this->_displayInfo.customBuffer[NDSDisplayID_Touch];
	this->_engineSub->renderedBuffer  = this->_engineSub->nativeBuffer;
	this->_engineSub->renderedWidth   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_engineSub->renderedHeight  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	if (!this->_displayInfo.isCustomSizeRequested && (this->_displayInfo.colorFormat != NDSColorFormat_BGR888_Rev))
	{
		return;
	}
	
	// Iterate through VRAM banks A-D and determine if they will be used for this frame.
	for (size_t i = 0; i < 4; i++)
	{
		if (this->_engineMain->nativeLineCaptureCount[i] == GPU_VRAM_BLOCK_LINES)
		{
			continue;
		}
		
		switch (vramConfiguration.banks[i].purpose)
		{
			case VramConfiguration::ABG:
			case VramConfiguration::BBG:
			case VramConfiguration::LCDC:
			case VramConfiguration::AOBJ:
			case VramConfiguration::BOBJ:
				break;
				
			default:
			{
				this->_engineMain->nativeLineCaptureCount[i] = GPU_VRAM_BLOCK_LINES;
				for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
				{
					this->_engineMain->isLineCaptureNative[i][l] = true;
				}
				break;
			}
		}
	}
}

const NDSDisplayInfo& GPUSubsystem::GetDisplayInfo()
{
	return this->_displayInfo;
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
	
	for (size_t srcY = 0, currentLineCount = 0; srcY < GPU_FRAMEBUFFER_NATIVE_HEIGHT; srcY++)
	{
		const size_t lineCount = (size_t)ceilf((srcY+1) * customHeightScale) - currentLineCount;
		_gpuDstLineCount[srcY] = lineCount;
		_gpuDstLineIndex[srcY] = currentLineCount;
		currentLineCount += lineCount;
	}
	
	for (size_t srcY = 0, currentLineCount = 0; srcY < GPU_VRAM_BLOCK_LINES + 1; srcY++)
	{
		const size_t lineCount = (size_t)ceilf((srcY+1) * customHeightScale) - currentLineCount;
		_gpuCaptureLineCount[srcY] = lineCount;
		_gpuCaptureLineIndex[srcY] = currentLineCount;
		currentLineCount += lineCount;
	}
	
	u16 *newGpuDstToSrcIndex = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	u16 *newGpuDstToSrcPtr = newGpuDstToSrcIndex;
	for (size_t y = 0, dstIdx = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
	{
		if (_gpuDstLineCount[y] < 1)
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
		
		for (size_t l = 1; l < _gpuDstLineCount[y]; l++)
		{
			memcpy(newGpuDstToSrcPtr + (w * l), newGpuDstToSrcPtr, w * sizeof(u16));
		}
		
		newGpuDstToSrcPtr += (w * _gpuDstLineCount[y]);
		dstIdx += (w * (_gpuDstLineCount[y] - 1));
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
	_gpuVRAMBlockOffset = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
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
		this->_engineMain->ResetCaptureLineStates();
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, w, h);
	
	free_aligned(oldGpuDstToSrcIndexPtr);
	free_aligned(oldGpuDstToSrcSSSE3_u8_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u8_16e);
	free_aligned(oldGpuDstToSrcSSSE3_u16_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u32_4e);
}

void GPUSubsystem::SetColorFormat(const NDSColorFormat outputFormat)
{
	if (this->_displayInfo.colorFormat == outputFormat)
	{
		return;
	}
	
	CurrentRenderer->RenderFinish();
	CurrentRenderer->SetRenderNeedsFinish(false);
	
	this->_displayInfo.colorFormat = outputFormat;
	this->_displayInfo.pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(FragmentColor);
	
	if (!this->_displayInfo.isCustomSizeRequested)
	{
		this->_engineMain->ResetCaptureLineStates();
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, this->_displayInfo.customWidth, this->_displayInfo.customHeight);
}

NDSColorFormat GPUSubsystem::GetColorFormat() const
{
	return this->_displayInfo.colorFormat;
}

void GPUSubsystem::_AllocateFramebuffers(NDSColorFormat outputFormat, size_t w, size_t h)
{
	void *oldMasterFramebuffer = this->_masterFramebuffer;
	void *oldCustomVRAM = this->_customVRAM;
	
	const size_t pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(FragmentColor);
	const size_t newCustomVRAMBlockSize = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
	const size_t newCustomVRAMBlankSize = _gpuLargestDstLineCount * GPU_VRAM_BLANK_REGION_LINES * w;
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * pixelBytes;
	const size_t customFramebufferSize = w * h * pixelBytes;
	
	void *newCustomVRAM = NULL;
	
	this->_displayInfo.framebufferSize = (nativeFramebufferSize * 2) + (customFramebufferSize * 2);
	this->_masterFramebuffer = malloc_alignedPage(this->_displayInfo.framebufferSize * 2);
	
	this->_displayInfo.masterFramebufferHead = this->_masterFramebuffer;
	this->_displayInfo.masterNativeBuffer = (u8 *)this->_masterFramebuffer + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferSize);
	this->_displayInfo.masterCustomBuffer = (u8 *)this->_masterFramebuffer + (nativeFramebufferSize * 2) + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferSize);
	
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
			newCustomVRAM = (void *)malloc_alignedCacheLine(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset_u16(this->_masterFramebuffer, 0x8000, (this->_displayInfo.framebufferSize * 2) / sizeof(u16));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (u16 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			newCustomVRAM = (void *)malloc_alignedCacheLine(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset_u32(this->_masterFramebuffer, 0x1F000000, (this->_displayInfo.framebufferSize * 2) / sizeof(FragmentColor));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (u16 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			newCustomVRAM = (void *)malloc_alignedCacheLine(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(FragmentColor));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(FragmentColor));
			memset_u32(this->_masterFramebuffer, 0xFF000000, (this->_displayInfo.framebufferSize * 2) / sizeof(FragmentColor));
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
	
	return (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((FragmentColor *)this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (_gpuCaptureLineIndex[blockLine] * this->_displayInfo.customWidth) + _gpuDstPitchIndex[linePixel] + offset) : (void *)((u16 *)this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (_gpuCaptureLineIndex[blockLine] * this->_displayInfo.customWidth) + _gpuDstPitchIndex[linePixel] + offset);
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

template <NDSColorFormat OUTPUTFORMAT>
void GPUSubsystem::RenderLine(const size_t l)
{
	if (!this->_frameNeedsFinish)
	{
		u8 targetBufferIndex = this->_displayInfo.bufferIndex;
		
		if ( (l == 0) && !this->_willFrameSkip )
		{
			targetBufferIndex = (targetBufferIndex + 1) & 0x01;
		}
		
		this->_event->DidApplyGPUSettingsBegin();
		this->_engineMain->ApplySettings();
		this->_engineSub->ApplySettings();
		this->_event->DidApplyGPUSettingsEnd();
		
		this->_event->DidFrameBegin(this->_willFrameSkip, targetBufferIndex, l);
		this->_frameNeedsFinish = true;
	}
	
	this->_engineMain->UpdateRenderStates(l);
	this->_engineSub->UpdateRenderStates(l);
	
	const bool isDisplayCaptureNeeded = this->_engineMain->WillDisplayCapture(l);
	const bool isFramebufferRenderNeeded[2]	= { this->_engineMain->GetEnableStateApplied(), this->_engineSub->GetEnableStateApplied() };
	
	if (l == 0)
	{
		if (!this->_willFrameSkip)
		{
			this->UpdateRenderProperties();
		}
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
			this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetEngine()->renderedBuffer;
			this->_displayInfo.renderedWidth[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetEngine()->renderedWidth;
			this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetEngine()->renderedHeight;
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = (this->_display[NDSDisplayID_Touch]->GetEngine()->nativeLineOutputCount < GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->renderedBuffer;
			this->_displayInfo.renderedWidth[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->renderedWidth;
			this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngine()->renderedHeight;
			
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
		}
		
		// Reset the current backlight intensity total.
		this->_backlightIntensityTotal[NDSDisplayID_Main]  = 0.0f;
		this->_backlightIntensityTotal[NDSDisplayID_Touch] = 0.0f;
		
		if (this->_frameNeedsFinish)
		{
			this->_frameNeedsFinish = false;
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
			memset_u16(this->_masterFramebuffer, color16, (this->_displayInfo.framebufferSize * 2) / this->_displayInfo.pixelBytes);
			break;
			
		case 4:
			memset_u32(this->_masterFramebuffer, color32.color, (this->_displayInfo.framebufferSize * 2) / this->_displayInfo.pixelBytes);
			break;
			
		default:
			break;
	}
}

GPUClientFetchObject::GPUClientFetchObject()
{
	memset(&_fetchDisplayInfo[0], 0, sizeof(NDSDisplayInfo));
	memset(&_fetchDisplayInfo[1], 0, sizeof(NDSDisplayInfo));
	_clientData = NULL;
	_lastFetchIndex = 0;
}

void GPUClientFetchObject::Init()
{
	// Do nothing. This is implementation dependent.
}

void GPUClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	// Do nothing. This is implementation dependent.
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
