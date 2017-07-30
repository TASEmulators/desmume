/*
	Copyright (C) 2016-2017 DeSmuME team
 
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

#include "colorspacehandler_SSE2.h"

#ifndef ENABLE_SSE2
	#error This code requires SSE2 support.
#else

#include <string.h>
#include <emmintrin.h>

#ifdef ENABLE_SSSE3
#include <tmmintrin.h>
#endif

#ifdef ENABLE_SSE4_1
#include <smmintrin.h>
#endif

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888_SSE2(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi)
{
	v128u32 src32;
	
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
#ifdef ENABLE_SSE4_1
	src32 = _mm_cvtepu16_epi32(srcColor);
#else
	src32 = _mm_unpacklo_epi16(srcColor, _mm_setzero_si128());
#endif
	
	dstLo = (SWAP_RB) ? _mm_or_si128(_mm_slli_epi32(src32, 19), _mm_srli_epi32(src32, 7)) : _mm_or_si128(_mm_slli_epi32(src32, 3), _mm_slli_epi32(src32, 9));
	dstLo = _mm_and_si128( dstLo, _mm_set1_epi32(0x00F800F8) );
	dstLo = _mm_or_si128( dstLo, _mm_and_si128(_mm_slli_epi32(src32, 6), _mm_set1_epi32(0x0000F800)) );
	dstLo = _mm_or_si128( dstLo, _mm_and_si128(_mm_srli_epi32(dstLo, 5), _mm_set1_epi32(0x00070707)) );
	dstLo = _mm_or_si128( dstLo, srcAlphaBits32Lo );
	
#ifdef ENABLE_SSE4_1
	src32 = _mm_cvtepu16_epi32( _mm_srli_si128(srcColor, 8) );
#else
	src32 = _mm_unpackhi_epi16(srcColor, _mm_setzero_si128());
#endif
	
	dstHi = (SWAP_RB) ? _mm_or_si128(_mm_slli_epi32(src32, 19), _mm_srli_epi32(src32, 7)) : _mm_or_si128(_mm_slli_epi32(src32, 3), _mm_slli_epi32(src32, 9));
	dstHi = _mm_and_si128( dstHi, _mm_set1_epi32(0x00F800F8) );
	dstHi = _mm_or_si128( dstHi, _mm_and_si128(_mm_slli_epi32(src32, 6), _mm_set1_epi32(0x0000F800)) );
	dstHi = _mm_or_si128( dstHi, _mm_and_si128(_mm_srli_epi32(dstHi, 5), _mm_set1_epi32(0x00070707)) );
	dstHi = _mm_or_si128( dstHi, srcAlphaBits32Hi );
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665_SSE2(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi)
{
	v128u32 src32;
	
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
#ifdef ENABLE_SSE4_1
	src32 = _mm_cvtepu16_epi32(srcColor);
#else
	src32 = _mm_unpacklo_epi16(srcColor, _mm_setzero_si128());
#endif
	
	dstLo = (SWAP_RB) ? _mm_or_si128(_mm_slli_epi32(src32, 17), _mm_srli_epi32(src32, 9)) : _mm_or_si128(_mm_slli_epi32(src32, 1), _mm_slli_epi32(src32, 7));
	dstLo = _mm_and_si128( dstLo, _mm_set1_epi32(0x003E003E) );
	dstLo = _mm_or_si128( dstLo, _mm_and_si128(_mm_slli_epi32(src32, 4), _mm_set1_epi32(0x00003E00)) );
	dstLo = _mm_or_si128( dstLo, _mm_and_si128(_mm_srli_epi32(dstLo, 5), _mm_set1_epi32(0x00010101)) );
	dstLo = _mm_or_si128( dstLo, srcAlphaBits32Lo );
	
#ifdef ENABLE_SSE4_1
	src32 = _mm_cvtepu16_epi32( _mm_srli_si128(srcColor, 8) );
#else
	src32 = _mm_unpackhi_epi16(srcColor, _mm_setzero_si128());
#endif
	
	dstHi = (SWAP_RB) ? _mm_or_si128(_mm_slli_epi32(src32, 17), _mm_srli_epi32(src32, 9)) : _mm_or_si128(_mm_slli_epi32(src32, 1), _mm_slli_epi32(src32, 7));
	dstHi = _mm_and_si128( dstHi, _mm_set1_epi32(0x003E003E) );
	dstHi = _mm_or_si128( dstHi, _mm_and_si128(_mm_slli_epi32(src32, 4), _mm_set1_epi32(0x00003E00)) );
	dstHi = _mm_or_si128( dstHi, _mm_and_si128(_mm_srli_epi32(dstHi, 5), _mm_set1_epi32(0x00010101)) );
	dstHi = _mm_or_si128( dstHi, srcAlphaBits32Hi );
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888Opaque_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u32 srcAlphaBits32 = _mm_set1_epi32(0xFF000000);
	ColorspaceConvert555To8888_SSE2<SWAP_RB>(srcColor, srcAlphaBits32, srcAlphaBits32, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665Opaque_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u32 srcAlphaBits32 = _mm_set1_epi32(0x1F000000);
	ColorspaceConvert555To6665_SSE2<SWAP_RB>(srcColor, srcAlphaBits32, srcAlphaBits32, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert8888To6665_SSE2(const v128u32 &src)
{
	// Conversion algorithm:
	//    RGB   8-bit to 6-bit formula: dstRGB6 = (srcRGB8 >> 2)
	//    Alpha 8-bit to 6-bit formula: dstA5   = (srcA8   >> 3)
	v128u32 rgb;
	const v128u32 a = _mm_and_si128( _mm_srli_epi32(src, 3), _mm_set1_epi32(0x1F000000) );
	
	if (SWAP_RB)
	{
#ifdef ENABLE_SSSE3
		rgb = _mm_and_si128( _mm_srli_epi32(src, 2), _mm_set1_epi32(0x003F3F3F) );
		rgb = _mm_shuffle_epi8( rgb, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
#else
		rgb = _mm_or_si128( _mm_and_si128(_mm_srli_epi32(src, 18), _mm_set1_epi32(0x0000003F)), _mm_or_si128( _mm_and_si128(_mm_srli_epi32(src,  2), _mm_set1_epi32(0x00003F00)), _mm_and_si128(_mm_slli_epi32(src, 14), _mm_set1_epi32(0x003F0000))) );
#endif
	}
	else
	{
		rgb = _mm_and_si128( _mm_srli_epi32(src, 2), _mm_set1_epi32(0x003F3F3F) );
	}
	
	return _mm_or_si128(rgb, a);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert6665To8888_SSE2(const v128u32 &src)
{
	// Conversion algorithm:
	//    RGB   6-bit to 8-bit formula: dstRGB8 = (srcRGB6 << 2) | ((srcRGB6 >> 4) & 0x03)
	//    Alpha 5-bit to 8-bit formula: dstA8   = (srcA5   << 3) | ((srcA5   >> 2) & 0x07)
	      v128u32 rgb = _mm_or_si128( _mm_and_si128(_mm_slli_epi32(src, 2), _mm_set1_epi32(0x00FCFCFC)), _mm_and_si128(_mm_srli_epi32(src, 4), _mm_set1_epi32(0x00030303)) );
	const v128u32 a   = _mm_or_si128( _mm_and_si128(_mm_slli_epi32(src, 3), _mm_set1_epi32(0xF8000000)), _mm_and_si128(_mm_srli_epi32(src, 2), _mm_set1_epi32(0x07000000)) );
	
	if (SWAP_RB)
	{
#ifdef ENABLE_SSSE3
		rgb = _mm_shuffle_epi8( rgb, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
#else
		rgb = _mm_or_si128( _mm_srli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x00FF0000)), 16), _mm_or_si128(_mm_and_si128(src, _mm_set1_epi32(0x0000FF00)), _mm_slli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x000000FF)), 16)) );
#endif
	}
	
	return _mm_or_si128(rgb, a);
}

template <NDSColorFormat COLORFORMAT, bool SWAP_RB>
FORCEINLINE v128u16 _ConvertColorBaseTo5551_SSE2(const v128u32 &srcLo, const v128u32 &srcHi)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		return srcLo;
	}
	
	v128u32 rgbLo;
	v128u32 rgbHi;
	v128u16 alpha;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                     _mm_and_si128(_mm_srli_epi32(srcLo, 17), _mm_set1_epi32(0x0000001F));
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_srli_epi32(srcLo,  4), _mm_set1_epi32(0x000003E0)) );
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_slli_epi32(srcLo,  9), _mm_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                     _mm_and_si128(_mm_srli_epi32(srcHi, 17), _mm_set1_epi32(0x0000001F));
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_srli_epi32(srcHi,  4), _mm_set1_epi32(0x000003E0)) );
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_slli_epi32(srcHi,  9), _mm_set1_epi32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                     _mm_and_si128(_mm_srli_epi32(srcLo,  1), _mm_set1_epi32(0x0000001F));
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_srli_epi32(srcLo,  4), _mm_set1_epi32(0x000003E0)) );
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_srli_epi32(srcLo,  7), _mm_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                     _mm_and_si128(_mm_srli_epi32(srcHi,  1), _mm_set1_epi32(0x0000001F));
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_srli_epi32(srcHi,  4), _mm_set1_epi32(0x000003E0)) );
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_srli_epi32(srcHi,  7), _mm_set1_epi32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = _mm_packs_epi32( _mm_and_si128(_mm_srli_epi32(srcLo, 24), _mm_set1_epi32(0x0000001F)), _mm_and_si128(_mm_srli_epi32(srcHi, 24), _mm_set1_epi32(0x0000001F)) );
		alpha = _mm_cmpgt_epi16(alpha, _mm_setzero_si128());
		alpha = _mm_and_si128(alpha, _mm_set1_epi16(0x8000));
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                     _mm_and_si128(_mm_srli_epi32(srcLo, 19), _mm_set1_epi32(0x0000001F));
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_srli_epi32(srcLo,  6), _mm_set1_epi32(0x000003E0)) );
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_slli_epi32(srcLo,  7), _mm_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                     _mm_and_si128(_mm_srli_epi32(srcHi, 19), _mm_set1_epi32(0x0000001F));
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_srli_epi32(srcHi,  6), _mm_set1_epi32(0x000003E0)) );
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_slli_epi32(srcHi,  7), _mm_set1_epi32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                     _mm_and_si128(_mm_srli_epi32(srcLo,  3), _mm_set1_epi32(0x0000001F));
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_srli_epi32(srcLo,  6), _mm_set1_epi32(0x000003E0)) );
			rgbLo = _mm_or_si128(rgbLo, _mm_and_si128(_mm_srli_epi32(srcLo,  9), _mm_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                     _mm_and_si128(_mm_srli_epi32(srcHi,  3), _mm_set1_epi32(0x0000001F));
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_srli_epi32(srcHi,  6), _mm_set1_epi32(0x000003E0)) );
			rgbHi = _mm_or_si128(rgbHi, _mm_and_si128(_mm_srli_epi32(srcHi,  9), _mm_set1_epi32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = _mm_packs_epi32( _mm_srli_epi32(srcLo, 24), _mm_srli_epi32(srcHi, 24) );
		alpha = _mm_cmpgt_epi16(alpha, _mm_setzero_si128());
		alpha = _mm_and_si128(alpha, _mm_set1_epi16(0x8000));
	}
	
	return _mm_or_si128(_mm_packs_epi32(rgbLo, rgbHi), alpha);
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceConvert8888To5551_SSE2(const v128u32 &srcLo, const v128u32 &srcHi)
{
	return _ConvertColorBaseTo5551_SSE2<NDSColorFormat_BGR888_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceConvert6665To5551_SSE2(const v128u32 &srcLo, const v128u32 &srcHi)
{
	return _ConvertColorBaseTo5551_SSE2<NDSColorFormat_BGR666_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert888XTo8888Opaque_SSE2(const v128u32 &src)
{
	if (SWAP_RB)
	{
#ifdef ENABLE_SSSE3
		return _mm_or_si128( _mm_shuffle_epi8(src, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)), _mm_set1_epi32(0xFF000000) );
#else
		return _mm_or_si128( _mm_or_si128(_mm_srli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x00FF0000)), 16), _mm_or_si128(_mm_and_si128(src, _mm_set1_epi32(0x0000FF00)), _mm_slli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x000000FF)), 16))), _mm_set1_epi32(0xFF000000) );
#endif
	}
	
	return _mm_or_si128(src, _mm_set1_epi32(0xFF000000));
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceCopy16_SSE2(const v128u16 &src)
{
	if (SWAP_RB)
	{
		return _mm_or_si128( _mm_or_si128(_mm_srli_epi16(_mm_and_si128(src, _mm_set1_epi16(0x7C00)), 10), _mm_or_si128(_mm_and_si128(src, _mm_set1_epi16(0x0E30)), _mm_slli_epi16(_mm_and_si128(src, _mm_set1_epi16(0x001F)), 10))), _mm_and_si128(src, _mm_set1_epi16(0x8000)) );
	}
	
	return src;
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceCopy32_SSE2(const v128u32 &src)
{
	if (SWAP_RB)
	{
#ifdef ENABLE_SSSE3
		return _mm_shuffle_epi8(src, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2));
#else
		return _mm_or_si128( _mm_or_si128(_mm_srli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x00FF0000)), 16), _mm_or_si128(_mm_and_si128(src, _mm_set1_epi32(0x0000FF00)), _mm_slli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x000000FF)), 16))), _mm_and_si128(src, _mm_set1_epi32(0xFF000000)) );
#endif
	}
	
	return src;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer555To8888Opaque_SSE2(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		v128u16 src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(src+i)) : _mm_load_si128((v128u16 *)(src+i));
		v128u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To8888Opaque_SSE2<SWAP_RB>(src_vec128, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((v128u32 *)(dst+i+0), dstConvertedLo);
			_mm_storeu_si128((v128u32 *)(dst+i+4), dstConvertedHi);
		}
		else
		{
			_mm_store_si128((v128u32 *)(dst+i+0), dstConvertedLo);
			_mm_store_si128((v128u32 *)(dst+i+4), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555To6665Opaque_SSE2(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		v128u16 src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(src+i)) : _mm_load_si128((v128u16 *)(src+i));
		v128u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To6665Opaque_SSE2<SWAP_RB>(src_vec128, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((v128u32 *)(dst+i+0), dstConvertedLo);
			_mm_storeu_si128((v128u32 *)(dst+i+4), dstConvertedHi);
		}
		else
		{
			_mm_store_si128((v128u32 *)(dst+i+0), dstConvertedLo);
			_mm_store_si128((v128u32 *)(dst+i+4), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To6665_SSE2(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u32 *)(dst+i), ColorspaceConvert8888To6665_SSE2<SWAP_RB>(_mm_loadu_si128((v128u32 *)(src+i))) );
		}
		else
		{
			_mm_store_si128( (v128u32 *)(dst+i), ColorspaceConvert8888To6665_SSE2<SWAP_RB>(_mm_load_si128((v128u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To8888_SSE2(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u32 *)(dst+i), ColorspaceConvert6665To8888_SSE2<SWAP_RB>(_mm_loadu_si128((v128u32 *)(src+i))) );
		}
		else
		{
			_mm_store_si128( (v128u32 *)(dst+i), ColorspaceConvert6665To8888_SSE2<SWAP_RB>(_mm_load_si128((v128u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To5551_SSE2(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u16 *)(dst+i), ColorspaceConvert8888To5551_SSE2<SWAP_RB>(_mm_loadu_si128((v128u32 *)(src+i)), _mm_loadu_si128((v128u32 *)(src+i+4))) );
		}
		else
		{
			_mm_store_si128( (v128u16 *)(dst+i), ColorspaceConvert8888To5551_SSE2<SWAP_RB>(_mm_load_si128((v128u32 *)(src+i)), _mm_load_si128((v128u32 *)(src+i+4))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To5551_SSE2(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u16 *)(dst+i), ColorspaceConvert6665To5551_SSE2<SWAP_RB>(_mm_loadu_si128((v128u32 *)(src+i)), _mm_loadu_si128((v128u32 *)(src+i+4))) );
		}
		else
		{
			_mm_store_si128( (v128u16 *)(dst+i), ColorspaceConvert6665To5551_SSE2<SWAP_RB>(_mm_load_si128((v128u32 *)(src+i)), _mm_load_si128((v128u32 *)(src+i+4))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo8888Opaque_SSE2(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u32 *)(dst+i), ColorspaceConvert888XTo8888Opaque_SSE2<SWAP_RB>(_mm_loadu_si128((v128u32 *)(src+i))) );
		}
		else
		{
			_mm_store_si128( (v128u32 *)(dst+i), ColorspaceConvert888XTo8888Opaque_SSE2<SWAP_RB>(_mm_load_si128((v128u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer16_SSE2(const u16 *src, u16 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u16));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		v128u16 src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(src+i)) : _mm_load_si128((v128u16 *)(src+i));
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((v128u16 *)(dst+i), ColorspaceCopy16_SSE2<SWAP_RB>(src_vec128));
		}
		else
		{
			_mm_store_si128((v128u16 *)(dst+i), ColorspaceCopy16_SSE2<SWAP_RB>(src_vec128));
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer32_SSE2(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u32));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		v128u32 src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u32 *)(src+i)) : _mm_load_si128((v128u32 *)(src+i));
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((v128u32 *)(dst+i), ColorspaceCopy32_SSE2<SWAP_RB>(src_vec128));
		}
		else
		{
			_mm_store_si128((v128u32 *)(dst+i), ColorspaceCopy32_SSE2<SWAP_RB>(src_vec128));
		}
	}
	
	return i;
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To8888Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To8888Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To6665Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555To6665Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To6665_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To6665_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To8888_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To8888_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer8888To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer6665To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_SSE2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo8888Opaque_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_SSE2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo8888Opaque_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::CopyBuffer16_SwapRB_IsUnaligned(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_SSE2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_SSE2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::CopyBuffer32_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_SSE2<true, true>(src, dst, pixCount);
}

template void ColorspaceConvert555To8888_SSE2<true>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To8888_SSE2<false>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To6665_SSE2<true>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To6665_SSE2<false>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To8888Opaque_SSE2<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To8888Opaque_SSE2<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To6665Opaque_SSE2<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To6665Opaque_SSE2<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template v128u32 ColorspaceConvert8888To6665_SSE2<true>(const v128u32 &src);
template v128u32 ColorspaceConvert8888To6665_SSE2<false>(const v128u32 &src);

template v128u32 ColorspaceConvert6665To8888_SSE2<true>(const v128u32 &src);
template v128u32 ColorspaceConvert6665To8888_SSE2<false>(const v128u32 &src);

template v128u16 ColorspaceConvert8888To5551_SSE2<true>(const v128u32 &srcLo, const v128u32 &srcHi);
template v128u16 ColorspaceConvert8888To5551_SSE2<false>(const v128u32 &srcLo, const v128u32 &srcHi);

template v128u16 ColorspaceConvert6665To5551_SSE2<true>(const v128u32 &srcLo, const v128u32 &srcHi);
template v128u16 ColorspaceConvert6665To5551_SSE2<false>(const v128u32 &srcLo, const v128u32 &srcHi);

template v128u32 ColorspaceConvert888XTo8888Opaque_SSE2<true>(const v128u32 &src);
template v128u32 ColorspaceConvert888XTo8888Opaque_SSE2<false>(const v128u32 &src);

template v128u16 ColorspaceCopy16_SSE2<true>(const v128u16 &src);
template v128u16 ColorspaceCopy16_SSE2<false>(const v128u16 &src);

template v128u32 ColorspaceCopy32_SSE2<true>(const v128u32 &src);
template v128u32 ColorspaceCopy32_SSE2<false>(const v128u32 &src);

#endif // ENABLE_SSE2
