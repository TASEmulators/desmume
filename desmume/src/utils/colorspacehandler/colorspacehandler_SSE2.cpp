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
FORCEINLINE void ColorspaceConvert555To8888_SSE2(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	if (SWAP_RB)
	{
		v128u16 rb = _mm_or_si128( _mm_slli_epi16(srcColor,11), _mm_and_si128(_mm_srli_epi16(srcColor, 7), _mm_set1_epi16(0x00F8)) );
		rb = _mm_or_si128(rb, _mm_and_si128(_mm_srli_epi16(rb, 5), _mm_set1_epi16(0x0707)));
		
		v128u16 ga = _mm_and_si128(_mm_srli_epi16(srcColor, 2), _mm_set1_epi16(0x00F8) );
		ga = _mm_or_si128(ga, _mm_srli_epi16(ga, 5));
		ga = _mm_or_si128(ga, srcAlphaBits);
		
		dstLo = _mm_unpacklo_epi8(rb, ga);
		dstHi = _mm_unpackhi_epi8(rb, ga);
	}
	else
	{
		const v128u16 r = _mm_and_si128( _mm_slli_epi16(srcColor, 3), _mm_set1_epi16(0x00F8) );
		v128u16 rg = _mm_or_si128( r, _mm_and_si128(_mm_slli_epi16(srcColor, 6), _mm_set1_epi16(0xF800)) );
		rg = _mm_or_si128( rg, _mm_and_si128(_mm_srli_epi16(rg, 5), _mm_set1_epi16(0x0707)) );
		
		v128u16 ba = _mm_and_si128( _mm_srli_epi16(srcColor, 7), _mm_set1_epi16(0x00F8) );
		ba = _mm_or_si128(ba, _mm_srli_epi16(ba, 5));
		ba = _mm_or_si128(ba, srcAlphaBits);
		
		dstLo = _mm_unpacklo_epi16(rg, ba);
		dstHi = _mm_unpackhi_epi16(rg, ba);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555XTo888X_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	if (SWAP_RB)
	{
		v128u16 rb = _mm_or_si128( _mm_slli_epi16(srcColor,11), _mm_and_si128(_mm_srli_epi16(srcColor, 7), _mm_set1_epi16(0x00F8)) );
		rb = _mm_or_si128(rb, _mm_and_si128(_mm_srli_epi16(rb, 5), _mm_set1_epi16(0x0707)));
		
		v128u16 g = _mm_and_si128(_mm_srli_epi16(srcColor, 2), _mm_set1_epi16(0x00F8) );
		g = _mm_or_si128(g, _mm_srli_epi16(g, 5));
		
		dstLo = _mm_unpacklo_epi8(rb, g);
		dstHi = _mm_unpackhi_epi8(rb, g);
	}
	else
	{
		const v128u16 r = _mm_and_si128( _mm_slli_epi16(srcColor, 3), _mm_set1_epi16(0x00F8) );
		v128u16 rg = _mm_or_si128( r, _mm_and_si128(_mm_slli_epi16(srcColor, 6), _mm_set1_epi16(0xF800)) );
		rg = _mm_or_si128( rg, _mm_and_si128(_mm_srli_epi16(rg, 5), _mm_set1_epi16(0x0707)) );
		
		v128u16 b = _mm_and_si128( _mm_srli_epi16(srcColor, 7), _mm_set1_epi16(0x00F8) );
		b = _mm_or_si128(b, _mm_srli_epi16(b, 5));
		
		dstLo = _mm_unpacklo_epi16(rg, b);
		dstHi = _mm_unpackhi_epi16(rg, b);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665_SSE2(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	if (SWAP_RB)
	{
		v128u16 rb = _mm_and_si128( _mm_or_si128( _mm_slli_epi16(srcColor,9), _mm_srli_epi16(srcColor, 9)), _mm_set1_epi16(0x3E3E) );
		rb = _mm_or_si128(rb, _mm_and_si128(_mm_srli_epi16(rb, 5), _mm_set1_epi16(0x0101)));
		
		v128u16 ga = _mm_and_si128(_mm_srli_epi16(srcColor, 4), _mm_set1_epi16(0x003E) );
		ga = _mm_or_si128(ga, _mm_srli_epi16(ga, 5));
		ga = _mm_or_si128(ga, srcAlphaBits);
		
		dstLo = _mm_unpacklo_epi8(rb, ga);
		dstHi = _mm_unpackhi_epi8(rb, ga);
	}
	else
	{
		const v128u16 r = _mm_and_si128( _mm_slli_epi16(srcColor, 1), _mm_set1_epi16(0x003E) );
		const v128u16 b = _mm_and_si128( _mm_srli_epi16(srcColor, 9), _mm_set1_epi16(0x003E) );
		
		v128u16 rg = _mm_or_si128( r, _mm_and_si128(_mm_slli_epi16(srcColor, 4), _mm_set1_epi16(0x3E00)) );
		rg = _mm_or_si128( rg, _mm_and_si128(_mm_srli_epi16(rg, 5), _mm_set1_epi16(0x0101)) );
		
		v128u16 ba = _mm_or_si128(b, _mm_srli_epi16(b, 5));
		ba = _mm_or_si128(ba, srcAlphaBits);
		
		dstLo = _mm_unpacklo_epi16(rg, ba);
		dstHi = _mm_unpackhi_epi16(rg, ba);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555XTo666X_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	if (SWAP_RB)
	{
		v128u16 rb = _mm_and_si128( _mm_or_si128( _mm_slli_epi16(srcColor,9), _mm_srli_epi16(srcColor, 9)), _mm_set1_epi16(0x3E3E) );
		rb = _mm_or_si128(rb, _mm_and_si128(_mm_srli_epi16(rb, 5), _mm_set1_epi16(0x0101)));
		
		v128u16 g = _mm_and_si128(_mm_srli_epi16(srcColor, 4), _mm_set1_epi16(0x003E) );
		g = _mm_or_si128(g, _mm_srli_epi16(g, 5));
		
		dstLo = _mm_unpacklo_epi8(rb, g);
		dstHi = _mm_unpackhi_epi8(rb, g);
	}
	else
	{
		const v128u16 r = _mm_and_si128( _mm_slli_epi16(srcColor, 1), _mm_set1_epi16(0x003E) );
		v128u16 rg = _mm_or_si128( r, _mm_and_si128(_mm_slli_epi16(srcColor, 4), _mm_set1_epi16(0x3E00)) );
		rg = _mm_or_si128( rg, _mm_and_si128(_mm_srli_epi16(rg, 5), _mm_set1_epi16(0x0101)) );
		
		v128u16 b = _mm_and_si128( _mm_srli_epi16(srcColor, 9), _mm_set1_epi16(0x003E) );
		b = _mm_or_si128(b, _mm_srli_epi16(b, 5));
		
		dstLo = _mm_unpacklo_epi16(rg, b);
		dstHi = _mm_unpackhi_epi16(rg, b);
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888Opaque_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u16 srcAlphaBits16 = _mm_set1_epi16(0xFF00);
	ColorspaceConvert555To8888_SSE2<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665Opaque_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u16 srcAlphaBits16 = _mm_set1_epi16(0x1F00);
	ColorspaceConvert555To6665_SSE2<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
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

template<bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceApplyIntensity16_SSE2(const v128u16 &src, float intensity)
{
	v128u16 tempSrc = (SWAP_RB) ? _mm_or_si128( _mm_or_si128(_mm_srli_epi16(_mm_and_si128(src, _mm_set1_epi16(0x7C00)), 10), _mm_or_si128(_mm_and_si128(src, _mm_set1_epi16(0x0E30)), _mm_slli_epi16(_mm_and_si128(src, _mm_set1_epi16(0x001F)), 10))), _mm_and_si128(src, _mm_set1_epi16(0x8000)) ) : src;
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return _mm_and_si128(tempSrc, _mm_set1_epi16(0x8000));
	}
	
	v128u16 r = _mm_and_si128(                tempSrc,      _mm_set1_epi16(0x001F) );
	v128u16 g = _mm_and_si128( _mm_srli_epi16(tempSrc,  5), _mm_set1_epi16(0x001F) );
	v128u16 b = _mm_and_si128( _mm_srli_epi16(tempSrc, 10), _mm_set1_epi16(0x001F) );
	v128u16 a = _mm_and_si128(                tempSrc,      _mm_set1_epi16(0x8000) );
	
	const v128u16 intensity_v128 = _mm_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
	
	r =                 _mm_mulhi_epu16(r, intensity_v128);
	g = _mm_slli_epi16( _mm_mulhi_epu16(g, intensity_v128),  5 );
	b = _mm_slli_epi16( _mm_mulhi_epu16(b, intensity_v128), 10 );
	
	return _mm_or_si128( _mm_or_si128( _mm_or_si128(r, g), b), a);
}

template<bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceApplyIntensity32_SSE2(const v128u32 &src, float intensity)
{
#ifdef ENABLE_SSSE3
	v128u32 tempSrc = (SWAP_RB) ? _mm_shuffle_epi8(src, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)) : src;
#else
	v128u32 tempSrc = (SWAP_RB) ? _mm_or_si128( _mm_or_si128(_mm_srli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x00FF0000)), 16), _mm_or_si128(_mm_and_si128(src, _mm_set1_epi32(0x0000FF00)), _mm_slli_epi32(_mm_and_si128(src, _mm_set1_epi32(0x000000FF)), 16))), _mm_and_si128(src, _mm_set1_epi32(0xFF000000)) ) : src;
#endif
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return _mm_and_si128(tempSrc, _mm_set1_epi32(0xFF000000));
	}
	
	v128u16 rb = _mm_and_si128(                tempSrc,      _mm_set1_epi32(0x00FF00FF) );
	v128u16 g  = _mm_and_si128( _mm_srli_epi32(tempSrc,  8), _mm_set1_epi32(0x000000FF) );
	v128u32 a  = _mm_and_si128(                tempSrc,      _mm_set1_epi32(0xFF000000) );
	
	const v128u16 intensity_v128 = _mm_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
	
	rb =                 _mm_mulhi_epu16(rb, intensity_v128);
	g  = _mm_slli_epi32( _mm_mulhi_epu16( g, intensity_v128),  8 );
	
	return _mm_or_si128( _mm_or_si128(rb, g), a);
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer555To8888Opaque_SSE2(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		v128u16 src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(src+i)) : _mm_load_si128((v128u16 *)(src+i));
		v128u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To8888Opaque_SSE2<SWAP_RB>(src_vec128, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm_storeu_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
		else
		{
			_mm_store_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm_store_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555To6665Opaque_SSE2(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		v128u16 src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(src+i)) : _mm_load_si128((v128u16 *)(src+i));
		v128u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To6665Opaque_SSE2<SWAP_RB>(src_vec128, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm_storeu_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
		else
		{
			_mm_store_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm_store_si128((v128u32 *)(dst+i+(sizeof(v128u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To6665_SSE2(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
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
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
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
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
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
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
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
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
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

#ifdef ENABLE_SSSE3

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555XTo888_SSSE3(const u16 *__restrict src, u8 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	v128u16 src_v128u16[2];
	v128u32 src_v128u32[4];
	
	for (; i < pixCountVec128; i+=((sizeof(v128u16)/sizeof(u16)) * 2))
	{
		if (IS_UNALIGNED)
		{
			src_v128u16[0] = _mm_loadu_si128( (v128u16 *)(src + i + ((sizeof(v128u16)/sizeof(u16)) * 0)) );
			src_v128u16[1] = _mm_loadu_si128( (v128u16 *)(src + i + ((sizeof(v128u16)/sizeof(u16)) * 1)) );
		}
		else
		{
			src_v128u16[0] = _mm_load_si128( (v128u16 *)(src + i + ((sizeof(v128u16)/sizeof(u16)) * 0)) );
			src_v128u16[1] = _mm_load_si128( (v128u16 *)(src + i + ((sizeof(v128u16)/sizeof(u16)) * 1)) );
		}
		
		v128u16 rb = _mm_and_si128( _mm_or_si128(_mm_slli_epi16(src_v128u16[0], 11), _mm_srli_epi16(src_v128u16[0], 7)), _mm_set1_epi16(0xF8F8) );
		v128u16 g  = _mm_and_si128( _mm_srli_epi16(src_v128u16[0], 2), _mm_set1_epi16(0x00F8) );
		src_v128u32[0] = _mm_unpacklo_epi16(rb, g);
		src_v128u32[1] = _mm_unpackhi_epi16(rb, g);
		
		rb = _mm_and_si128( _mm_or_si128(_mm_slli_epi16(src_v128u16[1], 11), _mm_srli_epi16(src_v128u16[1], 7)), _mm_set1_epi16(0xF8F8) );
		g  = _mm_and_si128( _mm_srli_epi16(src_v128u16[1], 2), _mm_set1_epi16(0x00F8) );
		src_v128u32[2] = _mm_unpacklo_epi16(rb, g);
		src_v128u32[3] = _mm_unpackhi_epi16(rb, g);
		
		src_v128u32[0] = _mm_or_si128( src_v128u32[0], _mm_and_si128(_mm_srli_epi32(src_v128u32[0], 5), _mm_set1_epi32(0x00070707)) );
		src_v128u32[1] = _mm_or_si128( src_v128u32[1], _mm_and_si128(_mm_srli_epi32(src_v128u32[1], 5), _mm_set1_epi32(0x00070707)) );
		src_v128u32[2] = _mm_or_si128( src_v128u32[2], _mm_and_si128(_mm_srli_epi32(src_v128u32[2], 5), _mm_set1_epi32(0x00070707)) );
		src_v128u32[3] = _mm_or_si128( src_v128u32[3], _mm_and_si128(_mm_srli_epi32(src_v128u32[3], 5), _mm_set1_epi32(0x00070707)) );
		
		if (SWAP_RB)
		{
			src_v128u32[0] = _mm_shuffle_epi8( src_v128u32[0], _mm_set_epi8(15,11, 7, 3,   13,14,12, 9,   10, 8, 5, 6,    4, 1, 2, 0) );
			src_v128u32[1] = _mm_shuffle_epi8( src_v128u32[1], _mm_set_epi8( 4, 1, 2, 0,   15,11, 7, 3,   13,14,12, 9,   10, 8, 5, 6) );
			src_v128u32[2] = _mm_shuffle_epi8( src_v128u32[2], _mm_set_epi8(10, 8, 5, 6,    4, 1, 2, 0,   15,11, 7, 3,   13,14,12, 9) );
			src_v128u32[3] = _mm_shuffle_epi8( src_v128u32[3], _mm_set_epi8(13,14,12, 9,   10, 8, 5, 6,    4, 1, 2, 0,   15,11, 7, 3) );
		}
		else
		{
			src_v128u32[0] = _mm_shuffle_epi8( src_v128u32[0], _mm_set_epi8(15,11, 7, 3,   12,14,13, 8,   10, 9, 4, 6,    5, 0, 2, 1) );
			src_v128u32[1] = _mm_shuffle_epi8( src_v128u32[1], _mm_set_epi8( 5, 0, 2, 1,   15,11, 7, 3,   12,14,13, 8,   10, 9, 4, 6) );
			src_v128u32[2] = _mm_shuffle_epi8( src_v128u32[2], _mm_set_epi8(10, 9, 4, 6,    5, 0, 2, 1,   15,11, 7, 3,   12,14,13, 8) );
			src_v128u32[3] = _mm_shuffle_epi8( src_v128u32[3], _mm_set_epi8(12,14,13, 8,   10, 9, 4, 6,    5, 0, 2, 1,   15,11, 7, 3) );
		}
		
#ifdef ENABLE_SSE4_1
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_blend_epi16(src_v128u32[0], src_v128u32[1], 0xC0) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_blend_epi16(src_v128u32[1], src_v128u32[2], 0xF0) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_blend_epi16(src_v128u32[2], src_v128u32[3], 0xFC) );
		}
		else
		{
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_blend_epi16(src_v128u32[0], src_v128u32[1], 0xC0) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_blend_epi16(src_v128u32[1], src_v128u32[2], 0xF0) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_blend_epi16(src_v128u32[2], src_v128u32[3], 0xFC) );
		}
#else
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_or_si128(_mm_and_si128(src_v128u32[1], _mm_set_epi32(0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000)),               src_v128u32[0]) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_or_si128(_mm_and_si128(src_v128u32[2], _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000)), _mm_and_si128(src_v128u32[1], _mm_set_epi32(0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF))) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_or_si128(              src_v128u32[3],                                                                 _mm_and_si128(src_v128u32[2], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF))) );
		}
		else
		{
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_or_si128(_mm_and_si128(src_v128u32[1], _mm_set_epi32(0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000)),               src_v128u32[0]) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_or_si128(_mm_and_si128(src_v128u32[2], _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000)), _mm_and_si128(src_v128u32[1], _mm_set_epi32(0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF))) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_or_si128(              src_v128u32[3],                                                                 _mm_and_si128(src_v128u32[2], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF))) );
		}
#endif
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo888_SSSE3(const u32 *__restrict src, u8 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	v128u32 src_v128u32[4];
	
	for (; i < pixCountVec128; i+=((sizeof(v128u32)/sizeof(u32)) * 4))
	{
		if (IS_UNALIGNED)
		{
			src_v128u32[0] = _mm_loadu_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 0)) );
			src_v128u32[1] = _mm_loadu_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 1)) );
			src_v128u32[2] = _mm_loadu_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 2)) );
			src_v128u32[3] = _mm_loadu_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 3)) );
		}
		else
		{
			src_v128u32[0] = _mm_load_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 0)) );
			src_v128u32[1] = _mm_load_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 1)) );
			src_v128u32[2] = _mm_load_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 2)) );
			src_v128u32[3] = _mm_load_si128( (v128u32 *)(src + i + ((sizeof(v128u32)/sizeof(u32)) * 3)) );
		}
		
		if (SWAP_RB)
		{
			src_v128u32[0] = _mm_shuffle_epi8(src_v128u32[0], _mm_set_epi8(15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v128u32[1] = _mm_shuffle_epi8(src_v128u32[1], _mm_set_epi8( 6, 0, 1, 2,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5));
			src_v128u32[2] = _mm_shuffle_epi8(src_v128u32[2], _mm_set_epi8( 9,10, 4, 5,    6, 0, 1, 2,   15,11, 7, 3,   12,13,14, 8));
			src_v128u32[3] = _mm_shuffle_epi8(src_v128u32[3], _mm_set_epi8(12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   15,11, 7, 3));
		}
		else
		{
			src_v128u32[0] = _mm_shuffle_epi8(src_v128u32[0], _mm_set_epi8(15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v128u32[1] = _mm_shuffle_epi8(src_v128u32[1], _mm_set_epi8( 4, 2, 1, 0,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5));
			src_v128u32[2] = _mm_shuffle_epi8(src_v128u32[2], _mm_set_epi8( 9, 8, 6, 5,    4, 2, 1, 0,   15,11, 7, 3,   14,13,12,10));
			src_v128u32[3] = _mm_shuffle_epi8(src_v128u32[3], _mm_set_epi8(14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   15,11, 7, 3));
		}
		
#ifdef ENABLE_SSE4_1
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_blend_epi16(src_v128u32[0], src_v128u32[1], 0xC0) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_blend_epi16(src_v128u32[1], src_v128u32[2], 0xF0) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_blend_epi16(src_v128u32[2], src_v128u32[3], 0xFC) );
		}
		else
		{
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_blend_epi16(src_v128u32[0], src_v128u32[1], 0xC0) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_blend_epi16(src_v128u32[1], src_v128u32[2], 0xF0) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_blend_epi16(src_v128u32[2], src_v128u32[3], 0xFC) );
		}
#else
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_or_si128(_mm_and_si128(src_v128u32[1], _mm_set_epi32(0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000)), _mm_and_si128(src_v128u32[0], _mm_set_epi32(0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF))) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_or_si128(_mm_and_si128(src_v128u32[2], _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000)), _mm_and_si128(src_v128u32[1], _mm_set_epi32(0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF))) );
			_mm_storeu_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_or_si128(_mm_and_si128(src_v128u32[3], _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000)), _mm_and_si128(src_v128u32[2], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF))) );
		}
		else
		{
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 0)), _mm_or_si128(_mm_and_si128(src_v128u32[1], _mm_set_epi32(0xFFFFFFFF, 0x00000000, 0x00000000, 0x00000000)), _mm_and_si128(src_v128u32[0], _mm_set_epi32(0x00000000, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF))) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 1)), _mm_or_si128(_mm_and_si128(src_v128u32[2], _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0x00000000, 0x00000000)), _mm_and_si128(src_v128u32[1], _mm_set_epi32(0x00000000, 0x00000000, 0xFFFFFFFF, 0xFFFFFFFF))) );
			_mm_store_si128( (v128u8 *)(dst + (i * 3) + (sizeof(v128u32) * 2)), _mm_or_si128(_mm_and_si128(src_v128u32[3], _mm_set_epi32(0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0x00000000)), _mm_and_si128(src_v128u32[2], _mm_set_epi32(0x00000000, 0x00000000, 0x00000000, 0xFFFFFFFF))) );
		}
#endif
	}
	
	return i;
}

#endif

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer16_SSE2(const u16 *src, u16 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u16));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
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
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
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

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer16_SSE2(u16 *dst, size_t pixCountVec128, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
			{
				const v128u16 dst_v128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(dst+i)) : _mm_load_si128((v128u16 *)(dst+i));
				const v128u16 tempDst = _mm_or_si128( _mm_or_si128(_mm_srli_epi16(_mm_and_si128(dst_v128, _mm_set1_epi16(0x7C00)), 10), _mm_or_si128(_mm_and_si128(dst_v128, _mm_set1_epi16(0x0E30)), _mm_slli_epi16(_mm_and_si128(dst_v128, _mm_set1_epi16(0x001F)), 10))), _mm_and_si128(dst_v128, _mm_set1_epi16(0x8000)) );
				
				if (IS_UNALIGNED)
				{
					_mm_storeu_si128( (v128u16 *)(dst+i), tempDst);
				}
				else
				{
					_mm_store_si128( (v128u16 *)(dst+i), tempDst);
				}
			}
		}
		else
		{
			return pixCountVec128;
		}
	}
	else if (intensity < 0.001f)
	{
		for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
		{
			if (IS_UNALIGNED)
			{
				_mm_storeu_si128( (v128u16 *)(dst+i), _mm_and_si128(_mm_loadu_si128((v128u16 *)(dst+i)), _mm_set1_epi16(0x8000)) );
			}
			else
			{
				_mm_store_si128( (v128u16 *)(dst+i), _mm_and_si128(_mm_load_si128((v128u16 *)(dst+i)), _mm_set1_epi16(0x8000)) );
			}
		}
	}
	else
	{
		const v128u16 intensity_v128 = _mm_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
		{
			v128u16 dst_v128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u16 *)(dst+i)) : _mm_load_si128((v128u16 *)(dst+i));
			v128u16 tempDst = (SWAP_RB) ? _mm_or_si128( _mm_or_si128(_mm_srli_epi16(_mm_and_si128(dst_v128, _mm_set1_epi16(0x7C00)), 10), _mm_or_si128(_mm_and_si128(dst_v128, _mm_set1_epi16(0x0E30)), _mm_slli_epi16(_mm_and_si128(dst_v128, _mm_set1_epi16(0x001F)), 10))), _mm_and_si128(dst_v128, _mm_set1_epi16(0x8000)) ) : dst_v128;
			
			v128u16 r = _mm_and_si128(                tempDst,      _mm_set1_epi16(0x001F) );
			v128u16 g = _mm_and_si128( _mm_srli_epi16(tempDst,  5), _mm_set1_epi16(0x001F) );
			v128u16 b = _mm_and_si128( _mm_srli_epi16(tempDst, 10), _mm_set1_epi16(0x001F) );
			v128u16 a = _mm_and_si128(                tempDst,      _mm_set1_epi16(0x8000) );
			
			r =                 _mm_mulhi_epu16(r, intensity_v128);
			g = _mm_slli_epi16( _mm_mulhi_epu16(g, intensity_v128),  5 );
			b = _mm_slli_epi16( _mm_mulhi_epu16(b, intensity_v128), 10 );
			
			tempDst = _mm_or_si128( _mm_or_si128( _mm_or_si128(r, g), b), a);
			
			if (IS_UNALIGNED)
			{
				_mm_storeu_si128((v128u16 *)(dst+i), tempDst);
			}
			else
			{
				_mm_store_si128((v128u16 *)(dst+i), tempDst);
			}
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer32_SSE2(u32 *dst, size_t pixCountVec128, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
			{
				const v128u32 dst_v128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u32 *)(dst+i)) : _mm_load_si128((v128u32 *)(dst+i));
#ifdef ENABLE_SSSE3
				const v128u32 tempDst = _mm_shuffle_epi8(dst_v128, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2));
#else
				const v128u32 tempDst = _mm_or_si128( _mm_or_si128(_mm_srli_epi32(_mm_and_si128(dst_v128, _mm_set1_epi32(0x00FF0000)), 16), _mm_or_si128(_mm_and_si128(dst_v128, _mm_set1_epi32(0x0000FF00)), _mm_slli_epi32(_mm_and_si128(dst_v128, _mm_set1_epi32(0x000000FF)), 16))), _mm_and_si128(dst_v128, _mm_set1_epi32(0xFF000000)) );
#endif
				if (IS_UNALIGNED)
				{
					_mm_storeu_si128( (v128u32 *)(dst+i), tempDst);
				}
				else
				{
					_mm_store_si128( (v128u32 *)(dst+i), tempDst);
				}
			}
		}
		else
		{
			return pixCountVec128;
		}
	}
	else if (intensity < 0.001f)
	{
		for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
		{
			if (IS_UNALIGNED)
			{
				_mm_storeu_si128( (v128u32 *)(dst+i), _mm_and_si128(_mm_loadu_si128((v128u32 *)(dst+i)), _mm_set1_epi32(0xFF000000)) );
			}
			else
			{
				_mm_store_si128( (v128u32 *)(dst+i), _mm_and_si128(_mm_load_si128((v128u32 *)(dst+i)), _mm_set1_epi32(0xFF000000)) );
			}
		}
	}
	else
	{
		const v128u16 intensity_v128 = _mm_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
		{
			v128u32 dst_v128 = (IS_UNALIGNED) ? _mm_loadu_si128((v128u32 *)(dst+i)) : _mm_load_si128((v128u32 *)(dst+i));
#ifdef ENABLE_SSSE3
			v128u32 tempDst = (SWAP_RB) ? _mm_shuffle_epi8(dst_v128, _mm_set_epi8(15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)) : dst_v128;
#else
			v128u32 tempDst = (SWAP_RB) ? _mm_or_si128( _mm_or_si128(_mm_srli_epi32(_mm_and_si128(dst_v128, _mm_set1_epi32(0x00FF0000)), 16), _mm_or_si128(_mm_and_si128(dst_v128, _mm_set1_epi32(0x0000FF00)), _mm_slli_epi32(_mm_and_si128(dst_v128, _mm_set1_epi32(0x000000FF)), 16))), _mm_and_si128(dst_v128, _mm_set1_epi32(0xFF000000)) ) : dst_v128;
#endif
			
			v128u16 rb = _mm_and_si128(                tempDst,      _mm_set1_epi32(0x00FF00FF) );
			v128u16 g  = _mm_and_si128( _mm_srli_epi32(tempDst,  8), _mm_set1_epi32(0x000000FF) );
			v128u32 a  = _mm_and_si128(                tempDst,      _mm_set1_epi32(0xFF000000) );
			
			rb =                 _mm_mulhi_epu16(rb, intensity_v128);
			g  = _mm_slli_epi32( _mm_mulhi_epu16( g, intensity_v128),  8 );
			
			tempDst = _mm_or_si128( _mm_or_si128(rb, g), a);
			
			if (IS_UNALIGNED)
			{
				_mm_storeu_si128((v128u32 *)(dst+i), tempDst);
			}
			else
			{
				_mm_store_si128((v128u32 *)(dst+i), tempDst);
			}
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

#ifdef ENABLE_SSSE3

size_t ColorspaceHandler_SSE2::ConvertBuffer555XTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_SSSE3<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555XTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_SSSE3<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555XTo888_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_SSSE3<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer555XTo888_SwapRB_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_SSSE3<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_SSSE3<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_SSSE3<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo888_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_SSSE3<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_SSE2::ConvertBuffer888XTo888_SwapRB_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_SSSE3<true, true>(src, dst, pixCount);
}

#endif

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

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer16(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_SSE2<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer16_SwapRB(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_SSE2<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer16_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_SSE2<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer16_SwapRB_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_SSE2<true, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer32(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_SSE2<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer32_SwapRB(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_SSE2<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer32_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_SSE2<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_SSE2::ApplyIntensityToBuffer32_SwapRB_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_SSE2<true, true>(dst, pixCount, intensity);
}

template void ColorspaceConvert555To8888_SSE2<true>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To8888_SSE2<false>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555XTo888X_SSE2<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555XTo888X_SSE2<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To6665_SSE2<true>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To6665_SSE2<false>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555XTo666X_SSE2<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555XTo666X_SSE2<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

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

template v128u16 ColorspaceApplyIntensity16_SSE2<true>(const v128u16 &src, float intensity);
template v128u16 ColorspaceApplyIntensity16_SSE2<false>(const v128u16 &src, float intensity);

template v128u32 ColorspaceApplyIntensity32_SSE2<true>(const v128u32 &src, float intensity);
template v128u32 ColorspaceApplyIntensity32_SSE2<false>(const v128u32 &src, float intensity);

#endif // ENABLE_SSE2
