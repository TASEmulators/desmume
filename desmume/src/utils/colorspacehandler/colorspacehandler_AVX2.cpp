/*
	Copyright (C) 2016-2019 DeSmuME team
 
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
FORCEINLINE void ColorspaceConvert555To8888_AVX2(const v256u16 &srcColor, const v256u16 &srcAlphaBits, v256u32 &dstLo, v256u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	if (SWAP_RB)
	{
		v256u16 rb = _mm256_or_si256( _mm256_slli_epi16(srcColor,11), _mm256_and_si256(_mm256_srli_epi16(srcColor, 7), _mm256_set1_epi16(0x00F8)) );
		rb = _mm256_or_si256(rb, _mm256_and_si256(_mm256_srli_epi16(rb, 5), _mm256_set1_epi16(0x0707)));
		
		v256u16 ga = _mm256_and_si256(_mm256_srli_epi16(srcColor, 2), _mm256_set1_epi16(0x00F8) );
		ga = _mm256_or_si256(ga, _mm256_srli_epi16(ga, 5));
		ga = _mm256_or_si256(ga, srcAlphaBits);
		
		rb = _mm256_permute4x64_epi64(rb, 0xD8);
		ga = _mm256_permute4x64_epi64(ga, 0xD8);
		
		dstLo = _mm256_unpacklo_epi8(rb, ga);
		dstHi = _mm256_unpackhi_epi8(rb, ga);
	}
	else
	{
		const v256u16 r = _mm256_and_si256( _mm256_slli_epi16(srcColor, 3), _mm256_set1_epi16(0x00F8) );
		v256u16 rg = _mm256_or_si256( r, _mm256_and_si256(_mm256_slli_epi16(srcColor, 6), _mm256_set1_epi16(0xF800)) );
		rg = _mm256_or_si256( rg, _mm256_and_si256(_mm256_srli_epi16(rg, 5), _mm256_set1_epi16(0x0707)) );
		
		v256u16 ba = _mm256_and_si256( _mm256_srli_epi16(srcColor, 7), _mm256_set1_epi16(0x00F8) );
		ba = _mm256_or_si256(ba, _mm256_srli_epi16(ba, 5));
		ba = _mm256_or_si256(ba, srcAlphaBits);
		
		rg = _mm256_permute4x64_epi64(rg, 0xD8);
		ba = _mm256_permute4x64_epi64(ba, 0xD8);
		
		dstLo = _mm256_unpacklo_epi16(rg, ba);
		dstHi = _mm256_unpackhi_epi16(rg, ba);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555XTo888X_AVX2(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	if (SWAP_RB)
	{
		v256u16 rb = _mm256_or_si256( _mm256_slli_epi16(srcColor,11), _mm256_and_si256(_mm256_srli_epi16(srcColor, 7), _mm256_set1_epi16(0x00F8)) );
		rb = _mm256_or_si256(rb, _mm256_and_si256(_mm256_srli_epi16(rb, 5), _mm256_set1_epi16(0x0707)));
		
		v256u16 g = _mm256_and_si256(_mm256_srli_epi16(srcColor, 2), _mm256_set1_epi16(0x00F8) );
		g = _mm256_or_si256(g, _mm256_srli_epi16(g, 5));
		
		rb = _mm256_permute4x64_epi64(rb, 0xD8);
		g  = _mm256_permute4x64_epi64( g, 0xD8);
		
		dstLo = _mm256_unpacklo_epi8(rb, g);
		dstHi = _mm256_unpackhi_epi8(rb, g);
	}
	else
	{
		const v256u16 r = _mm256_and_si256( _mm256_slli_epi16(srcColor, 3), _mm256_set1_epi16(0x00F8) );
		v256u16 rg = _mm256_or_si256( r, _mm256_and_si256(_mm256_slli_epi16(srcColor, 6), _mm256_set1_epi16(0xF800)) );
		rg = _mm256_or_si256( rg, _mm256_and_si256(_mm256_srli_epi32(rg, 5), _mm256_set1_epi16(0x0707)) );
		
		v256u16 b = _mm256_and_si256( _mm256_srli_epi16(srcColor, 7), _mm256_set1_epi16(0x00F8) );
		b = _mm256_or_si256(b, _mm256_srli_epi32(b, 5));
		
		rg = _mm256_permute4x64_epi64(rg, 0xD8);
		b  = _mm256_permute4x64_epi64( b, 0xD8);
		
		dstLo = _mm256_unpacklo_epi16(rg, b);
		dstHi = _mm256_unpackhi_epi16(rg, b);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665_AVX2(const v256u16 &srcColor, const v256u16 &srcAlphaBits, v256u32 &dstLo, v256u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	if (SWAP_RB)
	{
		v256u16 rb = _mm256_and_si256( _mm256_or_si256( _mm256_slli_epi16(srcColor,9), _mm256_srli_epi16(srcColor, 9)), _mm256_set1_epi16(0x3E3E) );
		rb = _mm256_or_si256(rb, _mm256_and_si256(_mm256_srli_epi16(rb, 5), _mm256_set1_epi16(0x0101)));
		
		v256u16 ga = _mm256_and_si256(_mm256_srli_epi16(srcColor, 4), _mm256_set1_epi16(0x003E) );
		ga = _mm256_or_si256(ga, _mm256_srli_epi16(ga, 5));
		ga = _mm256_or_si256(ga, srcAlphaBits);
		
		rb = _mm256_permute4x64_epi64(rb, 0xD8);
		ga = _mm256_permute4x64_epi64(ga, 0xD8);
		
		dstLo = _mm256_unpacklo_epi8(rb, ga);
		dstHi = _mm256_unpackhi_epi8(rb, ga);
	}
	else
	{
		const v256u16 r = _mm256_and_si256( _mm256_slli_epi16(srcColor, 1), _mm256_set1_epi16(0x003E) );
		const v256u16 b = _mm256_and_si256( _mm256_srli_epi16(srcColor, 9), _mm256_set1_epi16(0x003E) );
		
		v256u16 rg = _mm256_or_si256( r, _mm256_and_si256(_mm256_slli_epi16(srcColor, 4), _mm256_set1_epi16(0x3E00)) );
		rg = _mm256_or_si256( rg, _mm256_and_si256(_mm256_srli_epi16(rg, 5), _mm256_set1_epi16(0x0101)) );
		
		v256u16 ba = _mm256_or_si256(b, _mm256_srli_epi16(b, 5));
		ba = _mm256_or_si256(ba, srcAlphaBits);
		
		rg = _mm256_permute4x64_epi64(rg, 0xD8);
		ba = _mm256_permute4x64_epi64(ba, 0xD8);
		
		dstLo = _mm256_unpacklo_epi16(rg, ba);
		dstHi = _mm256_unpackhi_epi16(rg, ba);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555XTo666X_AVX2(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	if (SWAP_RB)
	{
		v256u16 rb = _mm256_and_si256( _mm256_or_si256( _mm256_slli_epi16(srcColor,9), _mm256_srli_epi16(srcColor, 9)), _mm256_set1_epi16(0x3E3E) );
		rb = _mm256_or_si256(rb, _mm256_and_si256(_mm256_srli_epi16(rb, 5), _mm256_set1_epi16(0x0101)));
		
		v256u16 g = _mm256_and_si256(_mm256_srli_epi16(srcColor, 4), _mm256_set1_epi16(0x003E) );
		g = _mm256_or_si256(g, _mm256_srli_epi16(g, 5));
		
		rb = _mm256_permute4x64_epi64(rb, 0xD8);
		g  = _mm256_permute4x64_epi64( g, 0xD8);
		
		dstLo = _mm256_unpacklo_epi8(rb, g);
		dstHi = _mm256_unpackhi_epi8(rb, g);
	}
	else
	{
		const v256u16 r = _mm256_and_si256( _mm256_slli_epi16(srcColor, 1), _mm256_set1_epi16(0x003E) );
		v256u16 rg = _mm256_or_si256( r, _mm256_and_si256(_mm256_slli_epi16(srcColor, 4), _mm256_set1_epi16(0x3E00)) );
		rg = _mm256_or_si256( rg, _mm256_and_si256(_mm256_srli_epi16(rg, 5), _mm256_set1_epi16(0x0101)) );
		
		v256u16 b = _mm256_and_si256( _mm256_srli_epi16(srcColor, 9), _mm256_set1_epi16(0x003E) );
		b = _mm256_or_si256(b, _mm256_srli_epi16(b, 5));
		
		rg = _mm256_permute4x64_epi64(rg, 0xD8);
		b  = _mm256_permute4x64_epi64( b, 0xD8);
		
		dstLo = _mm256_unpacklo_epi16(rg, b);
		dstHi = _mm256_unpackhi_epi16(rg, b);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888Opaque_AVX2(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi)
{
	const v256u16 srcAlphaBits16 = _mm256_set1_epi16(0xFF00);
	ColorspaceConvert555To8888_AVX2<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665Opaque_AVX2(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi)
{
	const v256u16 srcAlphaBits16 = _mm256_set1_epi16(0x1F00);
	ColorspaceConvert555To6665_AVX2<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE v256u32 ColorspaceConvert8888To6665_AVX2(const v256u32 &src)
{
	// Conversion algorithm:
	//    RGB   8-bit to 6-bit formula: dstRGB6 = (srcRGB8 >> 2)
	//    Alpha 8-bit to 6-bit formula: dstA5   = (srcA8   >> 3)
	      v256u32 rgb = _mm256_and_si256( _mm256_srli_epi32(src, 2), _mm256_set1_epi32(0x003F3F3F) );
	const v256u32 a   = _mm256_and_si256( _mm256_srli_epi32(src, 3), _mm256_set1_epi32(0x1F000000) );
	
	if (SWAP_RB)
	{
		rgb = _mm256_shuffle_epi8( rgb, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
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

template<bool SWAP_RB>
FORCEINLINE v256u16 ColorspaceApplyIntensity16_AVX2(const v256u16 &src, float intensity)
{
	v256u16 tempSrc = (SWAP_RB) ? _mm256_or_si256( _mm256_or_si256(_mm256_srli_epi16(_mm256_and_si256(src, _mm256_set1_epi16(0x7C00)), 10), _mm256_or_si256(_mm256_and_si256(src, _mm256_set1_epi16(0x0E30)), _mm256_slli_epi16(_mm256_and_si256(src, _mm256_set1_epi16(0x001F)), 10))), _mm256_and_si256(src, _mm256_set1_epi16(0x8000)) ) : src;
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return _mm256_and_si256(tempSrc, _mm256_set1_epi16(0x8000));
	}
	
	v256u16 r = _mm256_and_si256(                   tempSrc,      _mm256_set1_epi16(0x001F) );
	v256u16 g = _mm256_and_si256( _mm256_srli_epi16(tempSrc,  5), _mm256_set1_epi16(0x001F) );
	v256u16 b = _mm256_and_si256( _mm256_srli_epi16(tempSrc, 10), _mm256_set1_epi16(0x001F) );
	v256u16 a = _mm256_and_si256(                   tempSrc,      _mm256_set1_epi16(0x8000) );
	
	const v256u16 intensity_v256 = _mm256_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
	
	r =                    _mm256_mulhi_epu16(r, intensity_v256);
	g = _mm256_slli_epi16( _mm256_mulhi_epu16(g, intensity_v256),  5 );
	b = _mm256_slli_epi16( _mm256_mulhi_epu16(b, intensity_v256), 10 );
	
	return _mm256_or_si256( _mm256_or_si256( _mm256_or_si256(r, g), b), a);
}

template<bool SWAP_RB>
FORCEINLINE v256u32 ColorspaceApplyIntensity32_AVX2(const v256u32 &src, float intensity)
{
	v256u32 tempSrc = (SWAP_RB) ? _mm256_shuffle_epi8(src, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)) : src;
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return _mm256_and_si256(tempSrc, _mm256_set1_epi32(0xFF000000));
	}
	
	v256u16 rb = _mm256_and_si256(                   tempSrc,      _mm256_set1_epi32(0x00FF00FF) );
	v256u16 g  = _mm256_and_si256( _mm256_srli_epi32(tempSrc,  8), _mm256_set1_epi32(0x000000FF) );
	v256u32 a  = _mm256_and_si256(                   tempSrc,      _mm256_set1_epi32(0xFF000000) );
	
	const v256u16 intensity_v256 = _mm256_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
	
	rb =                    _mm256_mulhi_epu16(rb, intensity_v256);
	g  = _mm256_slli_epi32( _mm256_mulhi_epu16( g, intensity_v256),  8 );
	
	return _mm256_or_si256( _mm256_or_si256(rb, g), a);
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer555To8888Opaque_AVX2(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
	{
		v256u16 src_vec256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(src+i)) : _mm256_load_si256((v256u16 *)(src+i));
		v256u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To8888Opaque_AVX2<SWAP_RB>(src_vec256, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm256_storeu_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
		else
		{
			_mm256_store_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm256_store_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555To6665Opaque_AVX2(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
	{
		v256u16 src_vec256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(src+i)) : _mm256_load_si256((v256u16 *)(src+i));
		v256u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To6665Opaque_AVX2<SWAP_RB>(src_vec256, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm256_storeu_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
		else
		{
			_mm256_store_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm256_store_si256((v256u32 *)(dst+i+(sizeof(v256u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To6665_AVX2(const u32 *src, u32 *dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
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
	
	for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
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
	
	for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u16 *)(dst+i), ColorspaceConvert8888To5551_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i)), _mm256_loadu_si256((v256u32 *)(src+i+(sizeof(v256u32)/sizeof(u32))))) );
		}
		else
		{
			_mm256_store_si256( (v256u16 *)(dst+i), ColorspaceConvert8888To5551_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i)), _mm256_load_si256((v256u32 *)(src+i+(sizeof(v256u32)/sizeof(u32))))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To5551_AVX2(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
	{
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u16 *)(dst+i), ColorspaceConvert6665To5551_AVX2<SWAP_RB>(_mm256_loadu_si256((v256u32 *)(src+i)), _mm256_loadu_si256((v256u32 *)(src+i+(sizeof(v256u32)/sizeof(u32))))) );
		}
		else
		{
			_mm256_store_si256( (v256u16 *)(dst+i), ColorspaceConvert6665To5551_AVX2<SWAP_RB>(_mm256_load_si256((v256u32 *)(src+i)), _mm256_load_si256((v256u32 *)(src+i+(sizeof(v256u32)/sizeof(u32))))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo8888Opaque_AVX2(const u32 *src, u32 *dst, size_t pixCountVec256)
{
	size_t i = 0;
	
	for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
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
size_t ColorspaceConvertBuffer555XTo888_AVX2(const u16 *__restrict src, u8 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	v256u16 src_v256u16[2];
	v256u32 src_v256u32[4];
	
	for (; i < pixCountVec256; i+=((sizeof(v256u16)/sizeof(u16)) * 2))
	{
		if (IS_UNALIGNED)
		{
			src_v256u16[0] = _mm256_loadu_si256( (v256u16 *)(src + i + ((sizeof(v256u16)/sizeof(u16)) * 0)) );
			src_v256u16[1] = _mm256_loadu_si256( (v256u16 *)(src + i + ((sizeof(v256u16)/sizeof(u16)) * 1)) );
		}
		else
		{
			src_v256u16[0] = _mm256_load_si256( (v256u16 *)(src + i + ((sizeof(v256u16)/sizeof(u16)) * 0)) );
			src_v256u16[1] = _mm256_load_si256( (v256u16 *)(src + i + ((sizeof(v256u16)/sizeof(u16)) * 1)) );
		}
		
		v256u16 rb = _mm256_and_si256( _mm256_or_si256(_mm256_slli_epi16(src_v256u16[0], 11), _mm256_srli_epi16(src_v256u16[0], 7)), _mm256_set1_epi16(0xF8F8) );
		v256u16 g  = _mm256_and_si256( _mm256_srli_epi16(src_v256u16[0], 2), _mm256_set1_epi16(0x00F8) );
		rb = _mm256_permute4x64_epi64(rb, 0xD8);
		g  = _mm256_permute4x64_epi64( g, 0xD8);
		src_v256u32[0] = _mm256_unpacklo_epi16(rb, g);
		src_v256u32[1] = _mm256_unpackhi_epi16(rb, g);
		
		rb = _mm256_and_si256( _mm256_or_si256(_mm256_slli_epi16(src_v256u16[1], 11), _mm256_srli_epi16(src_v256u16[1], 7)), _mm256_set1_epi16(0xF8F8) );
		g  = _mm256_and_si256( _mm256_srli_epi16(src_v256u16[1], 2), _mm256_set1_epi16(0x00F8) );
		rb = _mm256_permute4x64_epi64(rb, 0xD8);
		g  = _mm256_permute4x64_epi64( g, 0xD8);
		src_v256u32[2] = _mm256_unpacklo_epi16(rb, g);
		src_v256u32[3] = _mm256_unpackhi_epi16(rb, g);
		
		src_v256u32[0] = _mm256_or_si256( src_v256u32[0], _mm256_and_si256(_mm256_srli_epi32(src_v256u32[0], 5), _mm256_set1_epi32(0x00070707)) );
		src_v256u32[1] = _mm256_or_si256( src_v256u32[1], _mm256_and_si256(_mm256_srli_epi32(src_v256u32[1], 5), _mm256_set1_epi32(0x00070707)) );
		src_v256u32[2] = _mm256_or_si256( src_v256u32[2], _mm256_and_si256(_mm256_srli_epi32(src_v256u32[2], 5), _mm256_set1_epi32(0x00070707)) );
		src_v256u32[3] = _mm256_or_si256( src_v256u32[3], _mm256_and_si256(_mm256_srli_epi32(src_v256u32[3], 5), _mm256_set1_epi32(0x00070707)) );
		
		if (SWAP_RB)
		{
			src_v256u32[0] = _mm256_shuffle_epi8( src_v256u32[0], _mm256_set_epi8(31,27,23,19,   29,30,28,25,   26,24,21,22,   20,17,18,16,   15,11, 7, 3,   13,14,12, 9,   10, 8, 5, 6,    4, 1, 2, 0) );
			src_v256u32[1] = _mm256_shuffle_epi8( src_v256u32[1], _mm256_set_epi8(31,27,23,19,   29,30,28,25,   26,24,21,22,   20,17,18,16,   15,11, 7, 3,   13,14,12, 9,   10, 8, 5, 6,    4, 1, 2, 0) );
			src_v256u32[2] = _mm256_shuffle_epi8( src_v256u32[2], _mm256_set_epi8(31,27,23,19,   29,30,28,25,   26,24,21,22,   20,17,18,16,   15,11, 7, 3,   13,14,12, 9,   10, 8, 5, 6,    4, 1, 2, 0) );
			src_v256u32[3] = _mm256_shuffle_epi8( src_v256u32[3], _mm256_set_epi8(31,27,23,19,   29,30,28,25,   26,24,21,22,   20,17,18,16,   15,11, 7, 3,   13,14,12, 9,   10, 8, 5, 6,    4, 1, 2, 0) );
		}
		else
		{
			src_v256u32[0] = _mm256_shuffle_epi8( src_v256u32[0], _mm256_set_epi8(31,27,23,19,   28,30,29,24,   26,25,20,22,   21,16,18,17,   15,11, 7, 3,   12,14,13, 8,   10, 9, 4, 6,    5, 0, 2, 1) );
			src_v256u32[1] = _mm256_shuffle_epi8( src_v256u32[1], _mm256_set_epi8(31,27,23,19,   28,30,29,24,   26,25,20,22,   21,16,18,17,   15,11, 7, 3,   12,14,13, 8,   10, 9, 4, 6,    5, 0, 2, 1) );
			src_v256u32[2] = _mm256_shuffle_epi8( src_v256u32[2], _mm256_set_epi8(31,27,23,19,   28,30,29,24,   26,25,20,22,   21,16,18,17,   15,11, 7, 3,   12,14,13, 8,   10, 9, 4, 6,    5, 0, 2, 1) );
			src_v256u32[3] = _mm256_shuffle_epi8( src_v256u32[3], _mm256_set_epi8(31,27,23,19,   28,30,29,24,   26,25,20,22,   21,16,18,17,   15,11, 7, 3,   12,14,13, 8,   10, 9, 4, 6,    5, 0, 2, 1) );
		}
		
		// This is necessary because vpshufb cannot shuffle bits across 128-bit lanes, but vpermd can.
		src_v256u32[0] = _mm256_permutevar8x32_epi32( src_v256u32[0], _mm256_set_epi32(7, 3, 6, 5, 4, 2, 1, 0) );
		src_v256u32[1] = _mm256_permutevar8x32_epi32( src_v256u32[1], _mm256_set_epi32(1, 0, 7, 3, 6, 5, 4, 2) );
		src_v256u32[2] = _mm256_permutevar8x32_epi32( src_v256u32[2], _mm256_set_epi32(4, 2, 1, 0, 7, 3, 6, 5) );
		src_v256u32[3] = _mm256_permutevar8x32_epi32( src_v256u32[3], _mm256_set_epi32(6, 5, 4, 2, 1, 0, 7, 3) );
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 0)), _mm256_blend_epi32(src_v256u32[0], src_v256u32[1], 0xC0) );
			_mm256_storeu_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 1)), _mm256_blend_epi32(src_v256u32[1], src_v256u32[2], 0xF0) );
			_mm256_storeu_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 2)), _mm256_blend_epi32(src_v256u32[2], src_v256u32[3], 0xFC) );
		}
		else
		{
			_mm256_store_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 0)), _mm256_blend_epi32(src_v256u32[0], src_v256u32[1], 0xC0) );
			_mm256_store_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 1)), _mm256_blend_epi32(src_v256u32[1], src_v256u32[2], 0xF0) );
			_mm256_store_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 2)), _mm256_blend_epi32(src_v256u32[2], src_v256u32[3], 0xFC) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo888_AVX2(const u32 *__restrict src, u8 *__restrict dst, size_t pixCountVec256)
{
	size_t i = 0;
	v256u32 src_v256u32[4];
	
	for (; i < pixCountVec256; i+=((sizeof(v256u32)/sizeof(u32)) * 4))
	{
		if (IS_UNALIGNED)
		{
			src_v256u32[0] = _mm256_loadu_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 0)) );
			src_v256u32[1] = _mm256_loadu_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 1)) );
			src_v256u32[2] = _mm256_loadu_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 2)) );
			src_v256u32[3] = _mm256_loadu_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 3)) );
		}
		else
		{
			src_v256u32[0] = _mm256_load_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 0)) );
			src_v256u32[1] = _mm256_load_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 1)) );
			src_v256u32[2] = _mm256_load_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 2)) );
			src_v256u32[3] = _mm256_load_si256( (v256u32 *)(src + i + ((sizeof(v256u32)/sizeof(u32)) * 3)) );
		}
		
		if (SWAP_RB)
		{
			src_v256u32[0] = _mm256_shuffle_epi8(src_v256u32[0], _mm256_set_epi8(31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v256u32[1] = _mm256_shuffle_epi8(src_v256u32[1], _mm256_set_epi8(31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v256u32[2] = _mm256_shuffle_epi8(src_v256u32[2], _mm256_set_epi8(31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v256u32[3] = _mm256_shuffle_epi8(src_v256u32[3], _mm256_set_epi8(31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
		}
		else
		{
			src_v256u32[0] = _mm256_shuffle_epi8(src_v256u32[0], _mm256_set_epi8(31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v256u32[1] = _mm256_shuffle_epi8(src_v256u32[1], _mm256_set_epi8(31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v256u32[2] = _mm256_shuffle_epi8(src_v256u32[2], _mm256_set_epi8(31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v256u32[3] = _mm256_shuffle_epi8(src_v256u32[3], _mm256_set_epi8(31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
		}
		
		// This is necessary because vpshufb cannot shuffle bits across 128-bit lanes, but vpermd can.
		src_v256u32[0] = _mm256_permutevar8x32_epi32( src_v256u32[0], _mm256_set_epi32(7, 3, 6, 5, 4, 2, 1, 0) );
		src_v256u32[1] = _mm256_permutevar8x32_epi32( src_v256u32[1], _mm256_set_epi32(1, 0, 7, 3, 6, 5, 4, 2) );
		src_v256u32[2] = _mm256_permutevar8x32_epi32( src_v256u32[2], _mm256_set_epi32(4, 2, 1, 0, 7, 3, 6, 5) );
		src_v256u32[3] = _mm256_permutevar8x32_epi32( src_v256u32[3], _mm256_set_epi32(6, 5, 4, 2, 1, 0, 7, 3) );
		
		if (IS_UNALIGNED)
		{
			_mm256_storeu_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 0)), _mm256_blend_epi32(src_v256u32[0], src_v256u32[1], 0xC0) );
			_mm256_storeu_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 1)), _mm256_blend_epi32(src_v256u32[1], src_v256u32[2], 0xF0) );
			_mm256_storeu_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 2)), _mm256_blend_epi32(src_v256u32[2], src_v256u32[3], 0xFC) );
		}
		else
		{
			_mm256_store_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 0)), _mm256_blend_epi32(src_v256u32[0], src_v256u32[1], 0xC0) );
			_mm256_store_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 1)), _mm256_blend_epi32(src_v256u32[1], src_v256u32[2], 0xF0) );
			_mm256_store_si256( (v256u8 *)(dst + (i * 3) + (sizeof(v256u32) * 2)), _mm256_blend_epi32(src_v256u32[2], src_v256u32[3], 0xFC) );
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
	
	for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
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
	
	for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
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

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer16_AVX2(u16 *dst, size_t pixCountVec256, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
			{
				const v256u16 dst_v256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(dst+i)) : _mm256_load_si256((v256u16 *)(dst+i));
				const v256u16 tempDst = _mm256_or_si256( _mm256_or_si256(_mm256_srli_epi16(_mm256_and_si256(dst_v256, _mm256_set1_epi16(0x7C00)), 10), _mm256_or_si256(_mm256_and_si256(dst_v256, _mm256_set1_epi16(0x0E30)), _mm256_slli_epi16(_mm256_and_si256(dst_v256, _mm256_set1_epi16(0x001F)), 10))), _mm256_and_si256(dst_v256, _mm256_set1_epi16(0x8000)) );
				
				if (IS_UNALIGNED)
				{
					_mm256_storeu_si256( (v256u16 *)(dst+i), tempDst);
				}
				else
				{
					_mm256_store_si256( (v256u16 *)(dst+i), tempDst);
				}
			}
		}
		else
		{
			return pixCountVec256;
		}
	}
	else if (intensity < 0.001f)
	{
		for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
		{
			if (IS_UNALIGNED)
			{
				_mm256_storeu_si256( (v256u16 *)(dst+i), _mm256_and_si256(_mm256_loadu_si256((v256u16 *)(dst+i)), _mm256_set1_epi16(0x8000)) );
			}
			else
			{
				_mm256_store_si256( (v256u16 *)(dst+i), _mm256_and_si256(_mm256_load_si256((v256u16 *)(dst+i)), _mm256_set1_epi16(0x8000)) );
			}
		}
	}
	else
	{
		const v256u16 intensity_v256 = _mm256_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec256; i+=(sizeof(v256u16)/sizeof(u16)))
		{
			v256u16 dst_v256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u16 *)(dst+i)) : _mm256_load_si256((v256u16 *)(dst+i));
			v256u16 tempDst = (SWAP_RB) ? _mm256_or_si256( _mm256_or_si256(_mm256_srli_epi16(_mm256_and_si256(dst_v256, _mm256_set1_epi16(0x7C00)), 10), _mm256_or_si256(_mm256_and_si256(dst_v256, _mm256_set1_epi16(0x0E30)), _mm256_slli_epi16(_mm256_and_si256(dst_v256, _mm256_set1_epi16(0x001F)), 10))), _mm256_and_si256(dst_v256, _mm256_set1_epi16(0x8000)) ) : dst_v256;
			
			v256u16 r = _mm256_and_si256(                   tempDst,      _mm256_set1_epi16(0x001F) );
			v256u16 g = _mm256_and_si256( _mm256_srli_epi16(tempDst,  5), _mm256_set1_epi16(0x001F) );
			v256u16 b = _mm256_and_si256( _mm256_srli_epi16(tempDst, 10), _mm256_set1_epi16(0x001F) );
			v256u16 a = _mm256_and_si256(                   tempDst,      _mm256_set1_epi16(0x8000) );
			
			r =                    _mm256_mulhi_epu16(r, intensity_v256);
			g = _mm256_slli_epi32( _mm256_mulhi_epu16(g, intensity_v256),  5 );
			b = _mm256_slli_epi32( _mm256_mulhi_epu16(b, intensity_v256), 10 );
			
			tempDst = _mm256_or_si256( _mm256_or_si256( _mm256_or_si256(r, g), b), a);
			
			if (IS_UNALIGNED)
			{
				_mm256_storeu_si256((v256u16 *)(dst+i), tempDst);
			}
			else
			{
				_mm256_store_si256((v256u16 *)(dst+i), tempDst);
			}
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer32_AVX2(u32 *dst, size_t pixCountVec256, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
			{
				const v256u32 dst_v256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u32 *)(dst+i)) : _mm256_load_si256((v256u32 *)(dst+i));
				const v256u32 tempDst = _mm256_shuffle_epi8(dst_v256, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2));
				
				if (IS_UNALIGNED)
				{
					_mm256_storeu_si256( (v256u32 *)(dst+i), tempDst);
				}
				else
				{
					_mm256_store_si256( (v256u32 *)(dst+i), tempDst);
				}
			}
		}
		else
		{
			return pixCountVec256;
		}
	}
	else if (intensity < 0.001f)
	{
		for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
		{
			if (IS_UNALIGNED)
			{
				_mm256_storeu_si256( (v256u32 *)(dst+i), _mm256_and_si256(_mm256_loadu_si256((v256u32 *)(dst+i)), _mm256_set1_epi32(0xFF000000)) );
			}
			else
			{
				_mm256_store_si256( (v256u32 *)(dst+i), _mm256_and_si256(_mm256_load_si256((v256u32 *)(dst+i)), _mm256_set1_epi32(0xFF000000)) );
			}
		}
	}
	else
	{
		const v256u16 intensity_v256 = _mm256_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec256; i+=(sizeof(v256u32)/sizeof(u32)))
		{
			v256u32 dst_v256 = (IS_UNALIGNED) ? _mm256_loadu_si256((v256u32 *)(dst+i)) : _mm256_load_si256((v256u32 *)(dst+i));
			v256u32 tempDst = (SWAP_RB) ? _mm256_shuffle_epi8(dst_v256, _mm256_set_epi8(31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)) : dst_v256;
			
			v256u16 rb = _mm256_and_si256(                   tempDst,      _mm256_set1_epi32(0x00FF00FF) );
			v256u16 g  = _mm256_and_si256( _mm256_srli_epi32(tempDst,  8), _mm256_set1_epi32(0x000000FF) );
			v256u32 a  = _mm256_and_si256(                   tempDst,      _mm256_set1_epi32(0xFF000000) );
			
			rb =                    _mm256_mulhi_epu16(rb, intensity_v256);
			g  = _mm256_slli_epi32( _mm256_mulhi_epu16( g, intensity_v256),  8 );
			
			tempDst = _mm256_or_si256( _mm256_or_si256(rb, g), a);
			
			if (IS_UNALIGNED)
			{
				_mm256_storeu_si256((v256u32 *)(dst+i), tempDst);
			}
			else
			{
				_mm256_store_si256((v256u32 *)(dst+i), tempDst);
			}
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

size_t ColorspaceHandler_AVX2::ConvertBuffer555XTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555XTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555XTo888_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer555XTo888_SwapRB_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX2<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX2<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX2<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo888_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX2<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX2::ConvertBuffer888XTo888_SwapRB_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX2<true, true>(src, dst, pixCount);
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

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer16(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX2<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer16_SwapRB(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX2<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer16_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX2<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer16_SwapRB_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX2<true, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer32(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX2<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer32_SwapRB(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX2<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer32_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX2<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX2::ApplyIntensityToBuffer32_SwapRB_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX2<true, true>(dst, pixCount, intensity);
}

template void ColorspaceConvert555To8888_AVX2<true>(const v256u16 &srcColor, const v256u16 &srcAlphaBits, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555To8888_AVX2<false>(const v256u16 &srcColor, const v256u16 &srcAlphaBits, v256u32 &dstLo, v256u32 &dstHi);

template void ColorspaceConvert555XTo888X_AVX2<true>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555XTo888X_AVX2<false>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);

template void ColorspaceConvert555To6665_AVX2<true>(const v256u16 &srcColor, const v256u16 &srcAlphaBits, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555To6665_AVX2<false>(const v256u16 &srcColor, const v256u16 &srcAlphaBits, v256u32 &dstLo, v256u32 &dstHi);

template void ColorspaceConvert555XTo666X_AVX2<true>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);
template void ColorspaceConvert555XTo666X_AVX2<false>(const v256u16 &srcColor, v256u32 &dstLo, v256u32 &dstHi);

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

template v256u16 ColorspaceApplyIntensity16_AVX2<true>(const v256u16 &src, float intensity);
template v256u16 ColorspaceApplyIntensity16_AVX2<false>(const v256u16 &src, float intensity);

template v256u32 ColorspaceApplyIntensity32_AVX2<true>(const v256u32 &src, float intensity);
template v256u32 ColorspaceApplyIntensity32_AVX2<false>(const v256u32 &src, float intensity);

#endif // ENABLE_AVX2
