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

#include "colorspacehandler_AVX2.h"

#ifndef ENABLE_AVX2
	#error This code requires AVX2 support.
#else

#include <string.h>
#include <immintrin.h>

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888_AVX2(const v256u16 &srcColor, const v256u32 &srcAlphaBits32Lo, const v256u32 &srcAlphaBits32Hi, v256u32 &dstLo, v256u32 &dstHi)
{
	v256u32 src32;
	
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	src32 = _mm256_cvtepu16_epi32( _mm256_extracti128_si256(srcColor, 0) );
	dstLo = (SWAP_RB) ? _mm256_or_si256(_mm256_slli_epi32(src32, 19), _mm256_srli_epi32(src32, 7)) : _mm256_or_si256(_mm256_slli_epi32(src32, 3), _mm256_slli_epi32(src32, 9));
	dstLo = _mm256_and_si256( dstLo, _mm256_set1_epi32(0x00F800F8) );
	dstLo = _mm256_or_si256( dstLo, _mm256_and_si256(_mm256_slli_epi32(src32, 6), _mm256_set1_epi32(0x0000F800)) );
	dstLo = _mm256_or_si256( dstLo, _mm256_and_si256(_mm256_srli_epi32(dstLo, 5), _mm256_set1_epi32(0x00070707)) );
	dstLo = _mm256_or_si256( dstLo, srcAlphaBits32Lo );
	
	src32 = _mm256_cvtepu16_epi32( _mm256_extracti128_si256(srcColor, 1) );
	dstHi = (SWAP_RB) ? _mm256_or_si256(_mm256_slli_epi32(src32, 19), _mm256_srli_epi32(src32, 7)) : _mm256_or_si256(_mm256_slli_epi32(src32, 3), _mm256_slli_epi32(src32, 9));
	dstHi = _mm256_and_si256( dstHi, _mm256_set1_epi32(0x00F800F8) );
	dstHi = _mm256_or_si256( dstHi, _mm256_and_si256(_mm256_slli_epi32(src32, 6), _mm256_set1_epi32(0x0000F800)) );
	dstHi = _mm256_or_si256( dstHi, _mm256_and_si256(_mm256_srli_epi32(dstHi, 5), _mm256_set1_epi32(0x00070707)) );
	dstHi = _mm256_or_si256( dstHi, srcAlphaBits32Hi );
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665_AVX2(const v256u16 &srcColor, const v256u32 &srcAlphaBits32Lo, const v256u32 &srcAlphaBits32Hi, v256u32 &dstLo, v256u32 &dstHi)
{
	v256u32 src32;
	
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	src32 = _mm256_cvtepu16_epi32( _mm256_extracti128_si256(srcColor, 0) );
	dstLo = (SWAP_RB) ? _mm256_or_si256(_mm256_slli_epi32(src32, 17), _mm256_srli_epi32(src32, 9)) : _mm256_or_si256(_mm256_slli_epi32(src32, 1), _mm256_slli_epi32(src32, 7));
	dstLo = _mm256_and_si256( dstLo, _mm256_set1_epi32(0x003E003E) );
	dstLo = _mm256_or_si256( dstLo, _mm256_and_si256(_mm256_slli_epi32(src32, 4), _mm256_set1_epi32(0x00003E00)) );
	dstLo = _mm256_or_si256( dstLo, _mm256_and_si256(_mm256_srli_epi32(dstLo, 5), _mm256_set1_epi32(0x00010101)) );
	dstLo = _mm256_or_si256( dstLo, srcAlphaBits32Lo );
	
	src32 = _mm256_cvtepu16_epi32( _mm256_extracti128_si256(srcColor, 1) );
	dstHi = (SWAP_RB) ? _mm256_or_si256(_mm256_slli_epi32(src32, 17), _mm256_srli_epi32(src32, 9)) : _mm256_or_si256(_mm256_slli_epi32(src32, 1), _mm256_slli_epi32(src32, 7));
	dstHi = _mm256_and_si256( dstHi, _mm256_set1_epi32(0x003E003E) );
	dstHi = _mm256_or_si256( dstHi, _mm256_and_si256(_mm256_slli_epi32(src32, 4), _mm256_set1_epi32(0x00003E00)) );
	dstHi = _mm256_or_si256( dstHi, _mm256_and_si256(_mm256_srli_epi32(dstHi, 5), _mm256_set1_epi32(0x00010101)) );
	dstHi = _mm256_or_si256( dstHi, srcAlphaBits32Hi );
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888Opaque_AVX2(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi)
{
	const v256u32 srcAlphaBits32 = _mm256_set1_epi32(0xFF000000);
	ColorspaceConvert555To8888_AVX2<SWAP_RB>(srcColor, srcAlphaBits32, srcAlphaBits32, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665Opaque_AVX2(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi)
{
	const v256u32 srcAlphaBits32 = _mm256_set1_epi32(0x1F000000);
	ColorspaceConvert555To6665_AVX2<SWAP_RB>(srcColor, srcAlphaBits32, srcAlphaBits32, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE v256u32 ColorspaceConvert8888To6665_AVX2(const v256u32 &src)
{
	// Conversion algorithm:
	//    RGB   8-bit to 6-bit formula: dstRGB6 = (srcRGB8 >> 2)
	//    Alpha 8-bit to 6-bit formula: dstA5   = (srcA8   >> 3)
	v256u32 rgb;
	const v256u32 a = _mm256_and_si256( _mm256_srli_epi32(src, 3), _mm256_set1_epi32(0x1F000000) );
	
	if (SWAP_RB)
	{
		rgb = _mm256_and_si256( _mm256_srli_epi32(src, 2), _mm256_set1_epi32(0x003F3F3F) );
		rgb = _mm256_shuffle_epi8( rgb, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
	}
	else
	{
		rgb = _mm256_and_si256( _mm256_srli_epi32(src, 2), _mm256_set1_epi32(0x003F3F3F) );
	}
	
	return _mm256_or_si256(rgb, a);
}

template <bool SWAP_RB>
FORCEINLINE v256u32 ColorspaceConvert6665To8888_AVX2(const v256u32 &src)
{
	// Conversion algorithm:
	//    RGB   6-bit to 8-bit formula: dstRGB8 = (srcRGB6 << 2) | ((srcRGB6 >> 4) & 0x03)
	//    Alpha 5-bit to 8-bit formula: dstA8   = (srcA5   << 3) | ((srcA5   >> 2) & 0x07)
	      v256u32 rgb = _mm256_or_si256( _mm256_and_si256(_mm256_slli_epi32(src, 2), _mm256_set1_epi32(0x00FCFCFC)), _mm256_and_si256(_mm256_srli_epi32(src, 4), _mm256_set1_epi32(0x00030303)) );
	const v256u32 a   = _mm256_or_si256( _mm256_and_si256(_mm256_slli_epi32(src, 3), _mm256_set1_epi32(0xF8000000)), _mm256_and_si256(_mm256_srli_epi32(src, 2), _mm256_set1_epi32(0x07000000)) );
	
	if (SWAP_RB)
	{
		rgb = _mm256_shuffle_epi8( rgb, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
	}
	
	return _mm256_or_si256(rgb, a);
}

template <NDSColorFormat COLORFORMAT, bool SWAP_RB>
FORCEINLINE v256u16 _ConvertColorBaseTo5551_AVX2(const v256u32 &srcLo, const v256u32 &srcHi)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		return srcLo;
	}
	
	v256u32 rgbLo;
	v256u32 rgbHi;
	v256u16 alpha;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                        _mm256_and_si256(_mm256_srli_epi32(srcLo, 17), _mm256_set1_epi32(0x0000001F));
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_srli_epi32(srcLo,  4), _mm256_set1_epi32(0x000003E0)) );
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_slli_epi32(srcLo,  9), _mm256_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm256_and_si256(_mm256_srli_epi32(srcHi, 17), _mm256_set1_epi32(0x0000001F));
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_srli_epi32(srcHi,  4), _mm256_set1_epi32(0x000003E0)) );
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_slli_epi32(srcHi,  9), _mm256_set1_epi32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                        _mm256_and_si256(_mm256_srli_epi32(srcLo,  1), _mm256_set1_epi32(0x0000001F));
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_srli_epi32(srcLo,  4), _mm256_set1_epi32(0x000003E0)) );
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_srli_epi32(srcLo,  7), _mm256_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm256_and_si256(_mm256_srli_epi32(srcHi,  1), _mm256_set1_epi32(0x0000001F));
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_srli_epi32(srcHi,  4), _mm256_set1_epi32(0x000003E0)) );
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_srli_epi32(srcHi,  7), _mm256_set1_epi32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = _mm256_packus_epi32( _mm256_and_si256(_mm256_srli_epi32(srcLo, 24), _mm256_set1_epi32(0x0000001F)), _mm256_and_si256(_mm256_srli_epi32(srcHi, 24), _mm256_set1_epi32(0x0000001F)) );
		alpha = _mm256_permute4x64_epi64(alpha, 0xD8);
		alpha = _mm256_cmpgt_epi16(alpha, _mm256_setzero_si256());
		alpha = _mm256_and_si256(alpha, _mm256_set1_epi16(0x8000));
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                        _mm256_and_si256(_mm256_srli_epi32(srcLo, 19), _mm256_set1_epi32(0x0000001F));
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_srli_epi32(srcLo,  6), _mm256_set1_epi32(0x000003E0)) );
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_slli_epi32(srcLo,  7), _mm256_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm256_and_si256(_mm256_srli_epi32(srcHi, 19), _mm256_set1_epi32(0x0000001F));
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_srli_epi32(srcHi,  6), _mm256_set1_epi32(0x000003E0)) );
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_slli_epi32(srcHi,  7), _mm256_set1_epi32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                        _mm256_and_si256(_mm256_srli_epi32(srcLo,  3), _mm256_set1_epi32(0x0000001F));
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_srli_epi32(srcLo,  6), _mm256_set1_epi32(0x000003E0)) );
			rgbLo = _mm256_or_si256(rgbLo, _mm256_and_si256(_mm256_srli_epi32(srcLo,  9), _mm256_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm256_and_si256(_mm256_srli_epi32(srcHi,  3), _mm256_set1_epi32(0x0000001F));
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_srli_epi32(srcHi,  6), _mm256_set1_epi32(0x000003E0)) );
			rgbHi = _mm256_or_si256(rgbHi, _mm256_and_si256(_mm256_srli_epi32(srcHi,  9), _mm256_set1_epi32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = _mm256_packus_epi32( _mm256_srli_epi32(srcLo, 24), _mm256_srli_epi32(srcHi, 24) );
		alpha = _mm256_permute4x64_epi64(alpha, 0xD8);
		alpha = _mm256_cmpgt_epi16(alpha, _mm256_setzero_si256());
		alpha = _mm256_and_si256(alpha, _mm256_set1_epi16(0x8000));
	}
	
	return _mm256_or_si256( _mm256_permute4x64_epi64(_mm256_packus_epi32(rgbLo, rgbHi), 0xD8), alpha );
}

template <bool SWAP_RB>
FORCEINLINE v256u16 ColorspaceConvert8888To5551_AVX2(const v256u32 &srcLo, const v256u32 &srcHi)
{
	return _ConvertColorBaseTo5551_AVX2<NDSColorFormat_BGR888_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v256u16 ColorspaceConvert6665To5551_AVX2(const v256u32 &srcLo, const v256u32 &srcHi)
{
	return _ConvertColorBaseTo5551_AVX2<NDSColorFormat_BGR666_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v256u32 ColorspaceConvert888XTo8888Opaque_AVX2(const v256u32 &src)
{
	if (SWAP_RB)
	{
		return _mm256_or_si256( _mm256_shuffle_epi8(src, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)), _mm256_set1_epi32(0xFF000000) );
	}
	
	return _mm256_or_si256(src, _mm256_set1_epi32(0xFF000000));
}

template <bool SWAP_RB>
FORCEINLINE v256u16 ColorspaceCopy16_AVX2(const v256u16 &src)
{
	if (SWAP_RB)
	{
		return _mm256_or_si256( _mm256_or_si256(_mm256_srli_epi16(_mm256_and_si256(src, _mm256_set1_epi16(0x7C00)), 10), _mm256_or_si256(_mm256_and_si256(src, _mm256_set1_epi16(0x0E30)), _mm256_slli_epi16(_mm256_and_si256(src, _mm256_set1_epi16(0x001F)), 10))), _mm256_and_si256(src, _mm256_set1_epi16(0x8000)) );
	}
	
	return src;
}

template <bool SWAP_RB>
FORCEINLINE v256u32 ColorspaceCopy32_AVX2(const v256u32 &src)
{
	if (SWAP_RB)
	{
		return _mm256_shuffle_epi8(src, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2));
	}
	
	return src;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer555To8888Opaque_AVX2(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=16)
	{
		v256u16 src_vec256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(src+i)) : _mm256_load_si256((v256u16 *)(src+i));
		v256u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To8888Opaque_AVX2<SWAP_RB>(src_vec256, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256((v256u32 *)(dst+i+0), dstConvertedLo);
			_mm256_storeu_si256((v256u32 *)(dst+i+8), dstConvertedHi);
		}
		else
		{
			_mm256_store_si256((v256u32 *)(dst+i+0), dstConvertedLo);
			_mm256_store_si256((v256u32 *)(dst+i+8), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555To6665Opaque_AVX2(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=16)
	{
		v256u16 src_vec256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(src+i)) : _mm256_load_si256((v256u16 *)(src+i));
		v256u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To6665Opaque_AVX2<SWAP_RB>(src_vec256, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256((v256u32 *)(dst+i+0), dstConvertedLo);
			_mm256_storeu_si256((v256u32 *)(dst+i+8), dstConvertedHi);
		}
		else
		{
			_mm256_store_si256((v256u32 *)(dst+i+0), dstConvertedLo);
			_mm256_store_si256((v256u32 *)(dst+i+8), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To6665_AVX2(const u32 *src, u32 *dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=8)
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u32 *)(dst+i), ColorspaceConvert8888To6665_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i))) );
		}
		else
		{
			_mm256_store_si256( (v256u32 *)(dst+i), ColorspaceConvert8888To6665_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To8888_AVX2(const u32 *src, u32 *dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=8)
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u32 *)(dst+i), ColorspaceConvert6665To8888_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i))) );
		}
		else
		{
			_mm256_store_si256( (v256u32 *)(dst+i), ColorspaceConvert6665To8888_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To5551_AVX2(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=16)
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u16 *)(dst+i), ColorspaceConvert8888To5551_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i)), _mm256_loadu_si256((v256u32 *)(src+i+8))) );
		}
		else
		{
			_mm256_store_si256( (v256u16 *)(dst+i), ColorspaceConvert8888To5551_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i)), _mm256_load_si256((v256u32 *)(src+i+8))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To5551_AVX2(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=16)
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u16 *)(dst+i), ColorspaceConvert6665To5551_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i)), _mm256_loadu_si256((v256u32 *)(src+i+8))) );
		}
		else
		{
			_mm256_store_si256( (v256u16 *)(dst+i), ColorspaceConvert6665To5551_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i)), _mm256_load_si256((v256u32 *)(src+i+8))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo8888Opaque_AVX2(const u32 *src, u32 *dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=8)
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u32 *)(dst+i), ColorspaceConvert888XTo8888Opaque_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i))) );
		}
		else
		{
			_mm256_store_si256( (v256u32 *)(dst+i), ColorspaceConvert888XTo8888Opaque_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer16_AVX2(const u16 *src, u16 *dst, size_t pixCountVec256)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec256 * sizeof(u16));
		return pixCountVec256;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=16)
	{
		v256u16 src_vec256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(src+i)) : _mm256_load_si256((v256u16 *)(src+i));
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256((v256u16 *)(dst+i), ColorspaceCopy16_AVX2<SWAP_RB>(src_vec256));
		}
		else
		{
			_mm256_store_si256((v256u16 *)(dst+i), ColorspaceCopy16_AVX2<SWAP_RB>(src_vec256));
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer32_AVX2(const u32 *src, u32 *dst, size_t pixCountVec256)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec256 * sizeof(u32));
		return pixCountVec256;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=8)
	{
		v256u32 src_vec256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u32 *)(src+i)) : _mm256_load_si256((v256u32 *)(src+i));
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256((v256u32 *)(dst+i), ColorspaceCopy32_AVX2<SWAP_RB>(src_vec256));
		}
		else
		{
			_mm256_store_si256((v256u32 *)(dst+i), ColorspaceCopy32_AVX2<SWAP_RB>(src_vec256));
		}
	}
	
	return i;
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To8888Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To8888Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To6665Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555To6665Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To6665_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To6665_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To8888_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To8888_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer8888To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer6665To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo8888Opaque_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo8888Opaque_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::CopyBuffer16_SwapRB_IsUnaligned(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::CopyBuffer32_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_AVX2<true, true>(src, dst, pixCount);
}

template void ColorspaceConvert555To8888_AVX2<true>(const v256u16 &srcColor, const v256u32 &srcAlphaBits32Lo, const v256u32 &srcAlphaBits32Hi, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555To8888_AVX2<false>(const v256u16 &srcColor, const v256u32 &srcAlphaBits32Lo, const v256u32 &srcAlphaBits32Hi, v256u32 &dstLo, v256u32 &dstHi);

template void ColorspaceConvert555To6665_AVX2<true>(const v256u16 &srcColor, const v256u32 &srcAlphaBits32Lo, const v256u32 &srcAlphaBits32Hi, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555To6665_AVX2<false>(const v256u16 &srcColor, const v256u32 &srcAlphaBits32Lo, const v256u32 &srcAlphaBits32Hi, v256u32 &dstLo, v256u32 &dstHi);

template void ColorspaceConvert555To8888Opaque_AVX2<true>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555To8888Opaque_AVX2<false>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);

template void ColorspaceConvert555To6665Opaque_AVX2<true>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555To6665Opaque_AVX2<false>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);

template v256u32 ColorspaceConvert8888To6665_AVX2<true>(const v256u32 &src);
template v256u32 ColorspaceConvert8888To6665_AVX2<false>(const v256u32 &src);

template v256u32 ColorspaceConvert6665To8888_AVX2<true>(const v256u32 &src);
template v256u32 ColorspaceConvert6665To8888_AVX2<false>(const v256u32 &src);

template v256u16 ColorspaceConvert8888To5551_AVX2<true>(const v256u32 &srcLo, const v256u32 &srcHi);
template v256u16 ColorspaceConvert8888To5551_AVX2<false>(const v256u32 &srcLo, const v256u32 &srcHi);

template v256u16 ColorspaceConvert6665To5551_AVX2<true>(const v256u32 &srcLo, const v256u32 &srcHi);
template v256u16 ColorspaceConvert6665To5551_AVX2<false>(const v256u32 &srcLo, const v256u32 &srcHi);

template v256u32 ColorspaceConvert888XTo8888Opaque_AVX2<true>(const v256u32 &src);
template v256u32 ColorspaceConvert888XTo8888Opaque_AVX2<false>(const v256u32 &src);

template v256u16 ColorspaceCopy16_AVX2<true>(const v256u16 &src);
template v256u16 ColorspaceCopy16_AVX2<false>(const v256u16 &src);

template v256u32 ColorspaceCopy32_AVX2<true>(const v256u32 &src);
template v256u32 ColorspaceCopy32_AVX2<false>(const v256u32 &src);

#endif // ENABLE_AVX2
