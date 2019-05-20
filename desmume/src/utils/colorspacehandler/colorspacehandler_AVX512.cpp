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

#include "colorspacehandler_AVX512.h"

#ifndef ENABLE_AVX512_1
	#error This code requires AVX-512 Tier-1 support.
#else

#include <string.h>
#include <immintrin.h>

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888_AVX512(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	const v512u16 r = (SWAP_RB) ? _mm512_and_si512( _mm512_srli_epi16(srcColor, 7), _mm512_set1_epi16(0x00F8) ) : _mm512_and_si512( _mm512_slli_epi16(srcColor, 3), _mm512_set1_epi16(0x00F8) );
	const v512u16 b = (SWAP_RB) ? _mm512_and_si512( _mm512_slli_epi16(srcColor, 3), _mm512_set1_epi16(0x00F8) ) : _mm512_and_si512( _mm512_srli_epi16(srcColor, 7), _mm512_set1_epi16(0x00F8) );
	
	v512u16 rg = _mm512_or_si512( r, _mm512_and_si512(_mm512_slli_epi16(srcColor, 6), _mm512_set1_epi16(0xF800)) );
	rg = _mm512_or_si512( rg, _mm512_and_si512(_mm512_srli_epi16(rg, 5), _mm512_set1_epi16(0x0707)) );
	
	v512u16 ba = _mm512_or_si512(b, _mm512_srli_epi16(b, 5));
	ba = _mm512_or_si512(ba, srcAlphaBits);
	
	dstLo = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x2F,0x0F,0x2E,0x0E,0x2D,0x0D,0x2C,0x0C, 0x2B,0x0B,0x2A,0x0A,0x29,0x09,0x28,0x08, 0x27,0x07,0x26,0x06,0x25,0x05,0x24,0x04, 0x23,0x03,0x22,0x02,0x21,0x01,0x20,0x00), ba);
	dstHi = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x3F,0x1F,0x3E,0x1E,0x3D,0x1D,0x3C,0x1C, 0x3B,0x1B,0x3A,0x1A,0x39,0x19,0x38,0x18, 0x37,0x17,0x36,0x16,0x35,0x15,0x34,0x14, 0x33,0x13,0x32,0x12,0x31,0x11,0x30,0x10), ba);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555XTo888X_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	const v512u16 r = (SWAP_RB) ? _mm512_and_si512( _mm512_srli_epi16(srcColor, 7), _mm512_set1_epi16(0x00F8) ) : _mm512_and_si512( _mm512_slli_epi16(srcColor, 3), _mm512_set1_epi16(0x00F8) );
	const v512u16 b = (SWAP_RB) ? _mm512_and_si512( _mm512_slli_epi16(srcColor, 3), _mm512_set1_epi16(0x00F8) ) : _mm512_and_si512( _mm512_srli_epi16(srcColor, 7), _mm512_set1_epi16(0x00F8) );
	
	v512u16 rg = _mm512_or_si512( r, _mm512_and_si512(_mm512_slli_epi16(srcColor, 6), _mm512_set1_epi16(0xF800)) );
	rg = _mm512_or_si512( rg, _mm512_and_si512(_mm512_srli_epi16(rg, 5), _mm512_set1_epi16(0x0707)) );
	
	v512u16 ba = _mm512_or_si512(b, _mm512_srli_epi16(b, 5));
	
	dstLo = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x2F,0x0F,0x2E,0x0E,0x2D,0x0D,0x2C,0x0C, 0x2B,0x0B,0x2A,0x0A,0x29,0x09,0x28,0x08, 0x27,0x07,0x26,0x06,0x25,0x05,0x24,0x04, 0x23,0x03,0x22,0x02,0x21,0x01,0x20,0x00), ba);
	dstHi = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x3F,0x1F,0x3E,0x1E,0x3D,0x1D,0x3C,0x1C, 0x3B,0x1B,0x3A,0x1A,0x39,0x19,0x38,0x18, 0x37,0x17,0x36,0x16,0x35,0x15,0x34,0x14, 0x33,0x13,0x32,0x12,0x31,0x11,0x30,0x10), ba);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665_AVX512(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	const v512u16 r = (SWAP_RB) ? _mm512_and_si512( _mm512_srli_epi16(srcColor, 9), _mm512_set1_epi16(0x003E) ) : _mm512_and_si512( _mm512_slli_epi16(srcColor, 1), _mm512_set1_epi16(0x003E) );
	const v512u16 b = (SWAP_RB) ? _mm512_and_si512( _mm512_slli_epi16(srcColor, 1), _mm512_set1_epi16(0x003E) ) : _mm512_and_si512( _mm512_srli_epi16(srcColor, 9), _mm512_set1_epi16(0x003E) );
	
	v512u16 rg = _mm512_or_si512( r, _mm512_and_si512(_mm512_slli_epi16(srcColor, 4), _mm512_set1_epi16(0x3E00)) );
	rg = _mm512_or_si512( rg, _mm512_and_si512(_mm512_srli_epi16(rg, 5), _mm512_set1_epi16(0x0101)) );
	
	v512u16 ba = _mm512_or_si512(b, _mm512_srli_epi16(b, 5));
	ba = _mm512_or_si512(ba, srcAlphaBits);
	
	dstLo = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x2F,0x0F,0x2E,0x0E,0x2D,0x0D,0x2C,0x0C, 0x2B,0x0B,0x2A,0x0A,0x29,0x09,0x28,0x08, 0x27,0x07,0x26,0x06,0x25,0x05,0x24,0x04, 0x23,0x03,0x22,0x02,0x21,0x01,0x20,0x00), ba);
	dstHi = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x3F,0x1F,0x3E,0x1E,0x3D,0x1D,0x3C,0x1C, 0x3B,0x1B,0x3A,0x1A,0x39,0x19,0x38,0x18, 0x37,0x17,0x36,0x16,0x35,0x15,0x34,0x14, 0x33,0x13,0x32,0x12,0x31,0x11,0x30,0x10), ba);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555XTo666X_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	const v512u16 r = (SWAP_RB) ? _mm512_and_si512( _mm512_srli_epi16(srcColor, 9), _mm512_set1_epi16(0x003E) ) : _mm512_and_si512( _mm512_slli_epi16(srcColor, 1), _mm512_set1_epi16(0x003E) );
	const v512u16 b = (SWAP_RB) ? _mm512_and_si512( _mm512_slli_epi16(srcColor, 1), _mm512_set1_epi16(0x003E) ) : _mm512_and_si512( _mm512_srli_epi16(srcColor, 9), _mm512_set1_epi16(0x003E) );
	
	v512u16 rg = _mm512_or_si512( r, _mm512_and_si512(_mm512_slli_epi16(srcColor, 4), _mm512_set1_epi16(0x3E00)) );
	rg = _mm512_or_si512( rg, _mm512_and_si512(_mm512_srli_epi16(rg, 5), _mm512_set1_epi16(0x0101)) );
	
	v512u16 ba = _mm512_or_si512(b, _mm512_srli_epi16(b, 5));
	
	dstLo = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x2F,0x0F,0x2E,0x0E,0x2D,0x0D,0x2C,0x0C, 0x2B,0x0B,0x2A,0x0A,0x29,0x09,0x28,0x08, 0x27,0x07,0x26,0x06,0x25,0x05,0x24,0x04, 0x23,0x03,0x22,0x02,0x21,0x01,0x20,0x00), ba);
	dstHi = _mm512_permutex2var_epi16(rg, _mm512_set_epi16(0x3F,0x1F,0x3E,0x1E,0x3D,0x1D,0x3C,0x1C, 0x3B,0x1B,0x3A,0x1A,0x39,0x19,0x38,0x18, 0x37,0x17,0x36,0x16,0x35,0x15,0x34,0x14, 0x33,0x13,0x32,0x12,0x31,0x11,0x30,0x10), ba);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888Opaque_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi)
{
	const v512u16 srcAlphaBits16 = _mm512_set1_epi16(0xFF00);
	ColorspaceConvert555To8888_AVX512<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665Opaque_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi)
{
	const v512u16 srcAlphaBits16 = _mm512_set1_epi16(0x1F00);
	ColorspaceConvert555To6665_AVX512<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE v512u32 ColorspaceConvert8888To6665_AVX512(const v512u32 &src)
{
	// Conversion algorithm:
	//    RGB   8-bit to 6-bit formula: dstRGB6 = (srcRGB8 >> 2)
	//    Alpha 8-bit to 6-bit formula: dstA5   = (srcA8   >> 3)
	      v512u32 rgb = _mm512_and_si512( _mm512_srli_epi32(src, 2), _mm512_set1_epi32(0x003F3F3F) );
	const v512u32 a   = _mm512_and_si512( _mm512_srli_epi32(src, 3), _mm512_set1_epi32(0x1F000000) );
	
	if (SWAP_RB)
	{
		rgb = _mm512_shuffle_epi8( rgb, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
	}
	
	return _mm512_or_si512(rgb, a);
}

template <bool SWAP_RB>
FORCEINLINE v512u32 ColorspaceConvert6665To8888_AVX512(const v512u32 &src)
{
	// Conversion algorithm:
	//    RGB   6-bit to 8-bit formula: dstRGB8 = (srcRGB6 << 2) | ((srcRGB6 >> 4) & 0x03)
	//    Alpha 5-bit to 8-bit formula: dstA8   = (srcA5   << 3) | ((srcA5   >> 2) & 0x07)
	      v512u32 rgb = _mm512_or_si512( _mm512_and_si512(_mm512_slli_epi32(src, 2), _mm512_set1_epi32(0x00FCFCFC)), _mm512_and_si512(_mm512_srli_epi32(src, 4), _mm512_set1_epi32(0x00030303)) );
	const v512u32 a   = _mm512_or_si512( _mm512_and_si512(_mm512_slli_epi32(src, 3), _mm512_set1_epi32(0xF8000000)), _mm512_and_si512(_mm512_srli_epi32(src, 2), _mm512_set1_epi32(0x07000000)) );
	
	if (SWAP_RB)
	{
		rgb = _mm512_shuffle_epi8( rgb, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2) );
	}
	
	return _mm512_or_si512(rgb, a);
}

template <NDSColorFormat COLORFORMAT, bool SWAP_RB>
FORCEINLINE v512u16 _ConvertColorBaseTo5551_AVX512(const v512u32 &srcLo, const v512u32 &srcHi)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		return srcLo;
	}
	
	v512u32 rgbLo;
	v512u32 rgbHi;
	v512u16 alpha;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                        _mm512_and_si512(_mm512_srli_epi32(srcLo, 17), _mm512_set1_epi32(0x0000001F));
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  4), _mm512_set1_epi32(0x000003E0)) );
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  9), _mm512_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm512_and_si512(_mm512_srli_epi32(srcHi, 17), _mm512_set1_epi32(0x0000001F));
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  4), _mm512_set1_epi32(0x000003E0)) );
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  9), _mm512_set1_epi32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                        _mm512_and_si512(_mm512_srli_epi32(srcLo,  1), _mm512_set1_epi32(0x0000001F));
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  4), _mm512_set1_epi32(0x000003E0)) );
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  7), _mm512_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm512_and_si512(_mm512_srli_epi32(srcHi,  1), _mm512_set1_epi32(0x0000001F));
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  4), _mm512_set1_epi32(0x000003E0)) );
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  7), _mm512_set1_epi32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = _mm512_packus_epi32( _mm512_and_si512(_mm512_srli_epi32(srcLo, 24), _mm512_set1_epi32(0x0000001F)), _mm512_and_si512(_mm512_srli_epi32(srcHi, 24), _mm512_set1_epi32(0x0000001F)) );
		alpha = _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0), alpha);
		alpha = _mm512_maskz_set1_epi16(_mm512_cmpgt_epi16_mask(alpha, _mm512_setzero_si512()), 0x8000);
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                        _mm512_and_si512(_mm512_srli_epi32(srcLo, 19), _mm512_set1_epi32(0x0000001F));
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  6), _mm512_set1_epi32(0x000003E0)) );
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_slli_epi32(srcLo,  7), _mm512_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm512_and_si512(_mm512_srli_epi32(srcHi, 19), _mm512_set1_epi32(0x0000001F));
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  6), _mm512_set1_epi32(0x000003E0)) );
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_slli_epi32(srcHi,  7), _mm512_set1_epi32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                        _mm512_and_si512(_mm512_srli_epi32(srcLo,  3), _mm512_set1_epi32(0x0000001F));
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  6), _mm512_set1_epi32(0x000003E0)) );
			rgbLo = _mm512_or_si512(rgbLo, _mm512_and_si512(_mm512_srli_epi32(srcLo,  9), _mm512_set1_epi32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                        _mm512_and_si512(_mm512_srli_epi32(srcHi,  3), _mm512_set1_epi32(0x0000001F));
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  6), _mm512_set1_epi32(0x000003E0)) );
			rgbHi = _mm512_or_si512(rgbHi, _mm512_and_si512(_mm512_srli_epi32(srcHi,  9), _mm512_set1_epi32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = _mm512_packus_epi32( _mm512_srli_epi32(srcLo, 24), _mm512_srli_epi32(srcHi, 24) );
		alpha = _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0), alpha);
		alpha = _mm512_maskz_set1_epi16(_mm512_cmpgt_epi16_mask(alpha, _mm512_setzero_si512()), 0x8000);
	}
	
	return _mm512_or_si512( _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0), _mm512_packus_epi32(rgbLo, rgbHi)), alpha );
}

template <bool SWAP_RB>
FORCEINLINE v512u16 ColorspaceConvert8888To5551_AVX512(const v512u32 &srcLo, const v512u32 &srcHi)
{
	return _ConvertColorBaseTo5551_AVX512<NDSColorFormat_BGR888_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v512u16 ColorspaceConvert6665To5551_AVX512(const v512u32 &srcLo, const v512u32 &srcHi)
{
	return _ConvertColorBaseTo5551_AVX512<NDSColorFormat_BGR666_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v512u32 ColorspaceConvert888XTo8888Opaque_AVX512(const v512u32 &src)
{
	if (SWAP_RB)
	{
		return _mm512_or_si512( _mm512_shuffle_epi8(src, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)), _mm512_set1_epi32(0xFF000000) );
	}
	
	return _mm512_or_si512(src, _mm512_set1_epi32(0xFF000000));
}

template <bool SWAP_RB>
FORCEINLINE v512u16 ColorspaceCopy16_AVX512(const v512u16 &src)
{
	if (SWAP_RB)
	{
		return _mm512_or_si512( _mm512_or_si512(_mm512_srli_epi16(_mm512_and_si512(src, _mm512_set1_epi16(0x7C00)), 10), _mm512_or_si512(_mm512_and_si512(src, _mm512_set1_epi16(0x0E30)), _mm512_slli_epi16(_mm512_and_si512(src, _mm512_set1_epi16(0x001F)), 10))), _mm512_and_si512(src, _mm512_set1_epi16(0x8000)) );
	}
	
	return src;
}

template <bool SWAP_RB>
FORCEINLINE v512u32 ColorspaceCopy32_AVX512(const v512u32 &src)
{
	if (SWAP_RB)
	{
		return _mm512_shuffle_epi8(src, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2));
	}
	
	return src;
}

template<bool SWAP_RB>
FORCEINLINE v512u16 ColorspaceApplyIntensity16_AVX512(const v512u16 &src, float intensity)
{
	v512u16 tempSrc = (SWAP_RB) ? _mm512_or_si512( _mm512_or_si512(_mm512_srli_epi16(_mm512_and_si512(src, _mm512_set1_epi16(0x7C00)), 10), _mm512_or_si512(_mm512_and_si512(src, _mm512_set1_epi16(0x0E30)), _mm512_slli_epi16(_mm512_and_si512(src, _mm512_set1_epi16(0x001F)), 10))), _mm512_and_si512(src, _mm512_set1_epi16(0x8000)) ) : src;
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return _mm512_and_si512(tempSrc, _mm512_set1_epi16(0x8000));
	}
	
	v512u16 r = _mm512_and_si512(                   tempSrc,      _mm512_set1_epi16(0x001F) );
	v512u16 g = _mm512_and_si512( _mm512_srli_epi16(tempSrc,  5), _mm512_set1_epi16(0x001F) );
	v512u16 b = _mm512_and_si512( _mm512_srli_epi16(tempSrc, 10), _mm512_set1_epi16(0x001F) );
	v512u16 a = _mm512_and_si512(                   tempSrc,      _mm512_set1_epi16(0x8000) );
	
	const v512u16 intensity_v512 = _mm512_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
	
	r =                    _mm512_mulhi_epu16(r, intensity_v512);
	g = _mm512_slli_epi16( _mm512_mulhi_epu16(g, intensity_v512),  5 );
	b = _mm512_slli_epi16( _mm512_mulhi_epu16(b, intensity_v512), 10 );
	
	return _mm512_or_si512( _mm512_or_si512( _mm512_or_si512(r, g), b), a);
}

template<bool SWAP_RB>
FORCEINLINE v512u32 ColorspaceApplyIntensity32_AVX512(const v512u32 &src, float intensity)
{
	v512u32 tempSrc = (SWAP_RB) ? _mm512_shuffle_epi8(src, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)) : src;
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return _mm512_and_si512(tempSrc, _mm512_set1_epi32(0xFF000000));
	}
	
	v512u16 rb = _mm512_and_si512(                   tempSrc,      _mm512_set1_epi32(0x00FF00FF) );
	v512u16 g  = _mm512_and_si512( _mm512_srli_epi32(tempSrc,  8), _mm512_set1_epi32(0x000000FF) );
	v512u32 a  = _mm512_and_si512(                   tempSrc,      _mm512_set1_epi32(0xFF000000) );
	
	const v512u16 intensity_v512 = _mm512_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
	
	rb =                    _mm512_mulhi_epu16(rb, intensity_v512);
	g  = _mm512_slli_epi32( _mm512_mulhi_epu16( g, intensity_v512),  8 );
	
	return _mm512_or_si512( _mm512_or_si512(rb, g), a);
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer555To8888Opaque_AVX512(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
	{
		v512u16 src_vec512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u16 *)(src+i)) : _mm512_load_si512((v512u16 *)(src+i));
		v512u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To8888Opaque_AVX512<SWAP_RB>(src_vec512, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm512_storeu_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
		else
		{
			_mm512_store_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm512_store_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555To6665Opaque_AVX512(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
	{
		v512u16 src_vec512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u16 *)(src+i)) : _mm512_load_si512((v512u16 *)(src+i));
		v512u32 dstConvertedLo, dstConvertedHi;
		ColorspaceConvert555To6665Opaque_AVX512<SWAP_RB>(src_vec512, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm512_storeu_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
		else
		{
			_mm512_store_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 0)), dstConvertedLo);
			_mm512_store_si512((v512u32 *)(dst+i+(sizeof(v512u32)/sizeof(u32) * 1)), dstConvertedHi);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To6665_AVX512(const u32 *src, u32 *dst, size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
	{
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u32 *)(dst+i), ColorspaceConvert8888To6665_AVX512<SWAP_RB>(_mm512_loadu_si512((v512u32 *)(src+i))) );
		}
		else
		{
			_mm512_store_si512( (v512u32 *)(dst+i), ColorspaceConvert8888To6665_AVX512<SWAP_RB>(_mm512_load_si512((v512u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To8888_AVX512(const u32 *src, u32 *dst, size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
	{
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u32 *)(dst+i), ColorspaceConvert6665To8888_AVX512<SWAP_RB>(_mm512_loadu_si512((v512u32 *)(src+i))) );
		}
		else
		{
			_mm512_store_si512( (v512u32 *)(dst+i), ColorspaceConvert6665To8888_AVX512<SWAP_RB>(_mm512_load_si512((v512u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To5551_AVX512(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
	{
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u16 *)(dst+i), ColorspaceConvert8888To5551_AVX512<SWAP_RB>(_mm512_loadu_si512((v512u32 *)(src+i)), _mm512_loadu_si512((v512u32 *)(src+i+(sizeof(v512u32)/sizeof(u32))))) );
		}
		else
		{
			_mm512_store_si512( (v512u16 *)(dst+i), ColorspaceConvert8888To5551_AVX512<SWAP_RB>(_mm512_load_si512((v512u32 *)(src+i)), _mm512_load_si512((v512u32 *)(src+i+(sizeof(v512u32)/sizeof(u32))))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To5551_AVX512(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
	{
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u16 *)(dst+i), ColorspaceConvert6665To5551_AVX512<SWAP_RB>(_mm512_loadu_si512((v512u32 *)(src+i)), _mm512_loadu_si512((v512u32 *)(src+i+(sizeof(v512u32)/sizeof(u32))))) );
		}
		else
		{
			_mm512_store_si512( (v512u16 *)(dst+i), ColorspaceConvert6665To5551_AVX512<SWAP_RB>(_mm512_load_si512((v512u32 *)(src+i)), _mm512_load_si512((v512u32 *)(src+i+(sizeof(v512u32)/sizeof(u32))))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo8888Opaque_AVX512(const u32 *src, u32 *dst, size_t pixCountVec512)
{
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
	{
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u32 *)(dst+i), ColorspaceConvert888XTo8888Opaque_AVX512<SWAP_RB>(_mm512_loadu_si512((v512u32 *)(src+i))) );
		}
		else
		{
			_mm512_store_si512( (v512u32 *)(dst+i), ColorspaceConvert888XTo8888Opaque_AVX512<SWAP_RB>(_mm512_load_si512((v512u32 *)(src+i))) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555XTo888_AVX512(const u16 *__restrict src, u8 *__restrict dst, size_t pixCountVec512)
{
	size_t i = 0;
	v512u16 src_v512u16[2];
	v512u32 src_v512u32[4];
	
	for (; i < pixCountVec512; i+=((sizeof(v512u16)/sizeof(u16)) * 2))
	{
		if (IS_UNALIGNED)
		{
			src_v512u16[0] = _mm512_loadu_si512( (v512u16 *)(src + i + ((sizeof(v512u16)/sizeof(u16)) * 0)) );
			src_v512u16[1] = _mm512_loadu_si512( (v512u16 *)(src + i + ((sizeof(v512u16)/sizeof(u16)) * 1)) );
		}
		else
		{
			src_v512u16[0] = _mm512_load_si512( (v512u16 *)(src + i + ((sizeof(v512u16)/sizeof(u16)) * 0)) );
			src_v512u16[1] = _mm512_load_si512( (v512u16 *)(src + i + ((sizeof(v512u16)/sizeof(u16)) * 1)) );
		}
		
		v512u16 rb = _mm512_and_si512( _mm512_or_si512(_mm512_slli_epi16(src_v512u16[0], 11), _mm512_srli_epi16(src_v512u16[0], 7)), _mm512_set1_epi16(0xF8F8) );
		v512u16 g  = _mm512_and_si512( _mm512_srli_epi16(src_v512u16[0], 2), _mm512_set1_epi16(0x00F8) );
		rb = _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0), rb);
		g  = _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0),  g);
		src_v512u32[0] = _mm512_unpacklo_epi16(rb, g);
		src_v512u32[1] = _mm512_unpackhi_epi16(rb, g);
		
		rb = _mm512_and_si512( _mm512_or_si512(_mm512_slli_epi16(src_v512u16[1], 11), _mm512_srli_epi16(src_v512u16[1], 7)), _mm512_set1_epi16(0xF8F8) );
		g  = _mm512_and_si512( _mm512_srli_epi16(src_v512u16[1], 2), _mm512_set1_epi16(0x00F8) );
		rb = _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0), rb);
		g  = _mm512_permutexvar_epi64(_mm512_set_epi64(7,5,3,1,6,4,2,0),  g);
		src_v512u32[2] = _mm512_unpacklo_epi16(rb, g);
		src_v512u32[3] = _mm512_unpackhi_epi16(rb, g);
		
		src_v512u32[0] = _mm512_or_si512( src_v512u32[0], _mm512_and_si512(_mm512_srli_epi32(src_v512u32[0], 5), _mm512_set1_epi32(0x00070707)) );
		src_v512u32[1] = _mm512_or_si512( src_v512u32[1], _mm512_and_si512(_mm512_srli_epi32(src_v512u32[1], 5), _mm512_set1_epi32(0x00070707)) );
		src_v512u32[2] = _mm512_or_si512( src_v512u32[2], _mm512_and_si512(_mm512_srli_epi32(src_v512u32[2], 5), _mm512_set1_epi32(0x00070707)) );
		src_v512u32[3] = _mm512_or_si512( src_v512u32[3], _mm512_and_si512(_mm512_srli_epi32(src_v512u32[3], 5), _mm512_set1_epi32(0x00070707)) );
		
#ifdef ENABLE_AVX512_2 // The vpermb instruction requires AVX512VBMI.
		if (SWAP_RB)
		{
			src_v512u32[0] = _mm512_permutexvar_epi8( _mm512_set_epi8(63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40,   41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21,   22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2), src_v512u32[0] );
			src_v512u32[1] = _mm512_permutexvar_epi8( _mm512_set_epi8(22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40,   41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21), src_v512u32[1] );
			src_v512u32[2] = _mm512_permutexvar_epi8( _mm512_set_epi8(41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21,   22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40), src_v512u32[2] );
			src_v512u32[3] = _mm512_permutexvar_epi8( _mm512_set_epi8(60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40,   41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21,   22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3), src_v512u32[3] );
		}
		else
		{
			src_v512u32[0] = _mm512_permutexvar_epi8( _mm512_set_epi8(63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42,   41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21,   20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0), src_v512u32[0] );
			src_v512u32[1] = _mm512_permutexvar_epi8( _mm512_set_epi8(20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42,   41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21), src_v512u32[1] );
			src_v512u32[2] = _mm512_permutexvar_epi8( _mm512_set_epi8(41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21,   20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42), src_v512u32[2] );
			src_v512u32[3] = _mm512_permutexvar_epi8( _mm512_set_epi8(62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42,   41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21,   20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3), src_v512u32[3] );
		}
#else
		if (SWAP_RB)
		{
			src_v512u32[0] = _mm512_shuffle_epi8(src_v512u32[0], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v512u32[1] = _mm512_shuffle_epi8(src_v512u32[1], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v512u32[2] = _mm512_shuffle_epi8(src_v512u32[2], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v512u32[3] = _mm512_shuffle_epi8(src_v512u32[3], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
		}
		else
		{
			src_v512u32[0] = _mm512_shuffle_epi8(src_v512u32[0], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v512u32[1] = _mm512_shuffle_epi8(src_v512u32[1], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v512u32[2] = _mm512_shuffle_epi8(src_v512u32[2], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v512u32[3] = _mm512_shuffle_epi8(src_v512u32[3], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
		}
		
		// This is necessary because vpshufb cannot shuffle bits across 128-bit lanes, but vpermd can.
		src_v512u32[0] = _mm512_permutexvar_epi32( _mm512_set_epi32(15,11, 7, 3,  14,13,12,10,   9, 8, 6, 5,   4, 2, 1, 0), src_v512u32[0] );
		src_v512u32[1] = _mm512_permutexvar_epi32( _mm512_set_epi32( 4, 2, 1, 0,  15,11, 7, 3,  14,13,12,10,   9, 8, 6, 5), src_v512u32[1] );
		src_v512u32[2] = _mm512_permutexvar_epi32( _mm512_set_epi32( 9, 8, 6, 5,   4, 2, 1, 0,  15,11, 7, 3,  14,13,12,10), src_v512u32[2] );
		src_v512u32[3] = _mm512_permutexvar_epi32( _mm512_set_epi32(14,13,12,10,   9, 8, 6, 5,   4, 2, 1, 0,  15,11, 7, 3), src_v512u32[3] );
#endif
		
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 0)), _mm512_mask_blend_epi32(0xF000, src_v512u32[0], src_v512u32[1]) );
			_mm512_storeu_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 1)), _mm512_mask_blend_epi32(0xFF00, src_v512u32[1], src_v512u32[2]) );
			_mm512_storeu_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 2)), _mm512_mask_blend_epi32(0xFFF0, src_v512u32[2], src_v512u32[3]) );
		}
		else
		{
			_mm512_store_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 0)), _mm512_mask_blend_epi32(0xF000, src_v512u32[0], src_v512u32[1]) );
			_mm512_store_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 1)), _mm512_mask_blend_epi32(0xFF00, src_v512u32[1], src_v512u32[2]) );
			_mm512_store_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 2)), _mm512_mask_blend_epi32(0xFFF0, src_v512u32[2], src_v512u32[3]) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888XTo888_AVX512(const u32 *__restrict src, u8 *__restrict dst, size_t pixCountVec512)
{
	size_t i = 0;
	v512u32 src_v512u32[4];
	
	for (; i < pixCountVec512; i+=((sizeof(v512u32)/sizeof(u32)) * 4))
	{
		if (IS_UNALIGNED)
		{
			src_v512u32[0] = _mm512_loadu_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 0)) );
			src_v512u32[1] = _mm512_loadu_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 1)) );
			src_v512u32[2] = _mm512_loadu_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 2)) );
			src_v512u32[3] = _mm512_loadu_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 3)) );
		}
		else
		{
			src_v512u32[0] = _mm512_load_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 0)) );
			src_v512u32[1] = _mm512_load_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 1)) );
			src_v512u32[2] = _mm512_load_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 2)) );
			src_v512u32[3] = _mm512_load_si512( (v512u32 *)(src + i + ((sizeof(v512u32)/sizeof(u32)) * 3)) );
		}
		
#ifdef ENABLE_AVX512_2 // The vpermb instruction requires AVX512VBMI.
		if (SWAP_RB)
		{
			src_v512u32[0] = _mm512_permutexvar_epi8( _mm512_set_epi8(63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40,   41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21,   22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2), src_v512u32[0] );
			src_v512u32[1] = _mm512_permutexvar_epi8( _mm512_set_epi8(22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40,   41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21), src_v512u32[1] );
			src_v512u32[2] = _mm512_permutexvar_epi8( _mm512_set_epi8(41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21,   22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40), src_v512u32[2] );
			src_v512u32[3] = _mm512_permutexvar_epi8( _mm512_set_epi8(60,61,62,56,   57,58,52,53,   54,48,49,50,   44,45,46,40,   41,42,36,37,   38,32,33,34,   28,29,30,24,   25,26,20,21,   22,16,17,18,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3), src_v512u32[3] );
		}
		else
		{
			src_v512u32[0] = _mm512_permutexvar_epi8( _mm512_set_epi8(63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42,   41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21,   20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0), src_v512u32[0] );
			src_v512u32[1] = _mm512_permutexvar_epi8( _mm512_set_epi8(20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42,   41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21), src_v512u32[1] );
			src_v512u32[2] = _mm512_permutexvar_epi8( _mm512_set_epi8(41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21,   20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3,   62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42), src_v512u32[2] );
			src_v512u32[3] = _mm512_permutexvar_epi8( _mm512_set_epi8(62,61,60,58,   57,56,54,53,   52,50,49,48,   46,45,44,42,   41,40,38,37,   36,34,33,32,   30,29,28,26,   25,24,22,21,   20,18,17,16,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0,   63,59,55,51,   47,43,39,35,   31,27,23,19,   15,11, 7, 3), src_v512u32[3] );
		}
#else
		if (SWAP_RB)
		{
			src_v512u32[0] = _mm512_shuffle_epi8(src_v512u32[0], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v512u32[1] = _mm512_shuffle_epi8(src_v512u32[1], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v512u32[2] = _mm512_shuffle_epi8(src_v512u32[2], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
			src_v512u32[3] = _mm512_shuffle_epi8(src_v512u32[3], _mm512_set_epi8(63,59,55,51,   60,61,62,56,   57,58,52,53,   54,48,49,50,   47,43,39,35,   44,45,46,40,   41,42,36,37,   38,32,33,34,   31,27,23,19,   28,29,30,24,   25,26,20,21,   22,16,17,18,   15,11, 7, 3,   12,13,14, 8,    9,10, 4, 5,    6, 0, 1, 2));
		}
		else
		{
			src_v512u32[0] = _mm512_shuffle_epi8(src_v512u32[0], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v512u32[1] = _mm512_shuffle_epi8(src_v512u32[1], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v512u32[2] = _mm512_shuffle_epi8(src_v512u32[2], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
			src_v512u32[3] = _mm512_shuffle_epi8(src_v512u32[3], _mm512_set_epi8(63,59,55,51,   62,61,60,58,   57,56,54,53,   52,50,49,48,   47,43,39,35,   46,45,44,42,   41,40,38,37,   36,34,33,32,   31,27,23,19,   30,29,28,26,   25,24,22,21,   20,18,17,16,   15,11, 7, 3,   14,13,12,10,    9, 8, 6, 5,    4, 2, 1, 0));
		}
		
		// This is necessary because vpshufb cannot shuffle bits across 128-bit lanes, but vpermd can.
		src_v512u32[0] = _mm512_permutexvar_epi32( _mm512_set_epi32(15,11, 7, 3,  14,13,12,10,   9, 8, 6, 5,   4, 2, 1, 0), src_v512u32[0] );
		src_v512u32[1] = _mm512_permutexvar_epi32( _mm512_set_epi32( 4, 2, 1, 0,  15,11, 7, 3,  14,13,12,10,   9, 8, 6, 5), src_v512u32[1] );
		src_v512u32[2] = _mm512_permutexvar_epi32( _mm512_set_epi32( 9, 8, 6, 5,   4, 2, 1, 0,  15,11, 7, 3,  14,13,12,10), src_v512u32[2] );
		src_v512u32[3] = _mm512_permutexvar_epi32( _mm512_set_epi32(14,13,12,10,   9, 8, 6, 5,   4, 2, 1, 0,  15,11, 7, 3), src_v512u32[3] );
#endif
		
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 0)), _mm512_mask_blend_epi32(0xF000, src_v512u32[0], src_v512u32[1]) );
			_mm512_storeu_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 1)), _mm512_mask_blend_epi32(0xFF00, src_v512u32[1], src_v512u32[2]) );
			_mm512_storeu_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 2)), _mm512_mask_blend_epi32(0xFFF0, src_v512u32[2], src_v512u32[3]) );
		}
		else
		{
			_mm512_store_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 0)), _mm512_mask_blend_epi32(0xF000, src_v512u32[0], src_v512u32[1]) );
			_mm512_store_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 1)), _mm512_mask_blend_epi32(0xFF00, src_v512u32[1], src_v512u32[2]) );
			_mm512_store_si512( (v512u8 *)(dst + (i * 3) + (sizeof(v512u32) * 2)), _mm512_mask_blend_epi32(0xFFF0, src_v512u32[2], src_v512u32[3]) );
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer16_AVX512(const u16 *src, u16 *dst, size_t pixCountVec512)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec512 * sizeof(u16));
		return pixCountVec512;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
	{
		v512u16 src_vec512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u16 *)(src+i)) : _mm512_load_si512((v512u16 *)(src+i));
		
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512((v512u16 *)(dst+i), ColorspaceCopy16_AVX512<SWAP_RB>(src_vec512));
		}
		else
		{
			_mm512_store_si512((v512u16 *)(dst+i), ColorspaceCopy16_AVX512<SWAP_RB>(src_vec512));
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer32_AVX512(const u32 *src, u32 *dst, size_t pixCountVec512)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec512 * sizeof(u32));
		return pixCountVec512;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
	{
		v512u32 src_vec512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u32 *)(src+i)) : _mm512_load_si512((v512u32 *)(src+i));
		
		if (IS_UNALIGNED)
		{
			_mm512_storeu_si512((v512u32 *)(dst+i), ColorspaceCopy32_AVX512<SWAP_RB>(src_vec512));
		}
		else
		{
			_mm512_store_si512((v512u32 *)(dst+i), ColorspaceCopy32_AVX512<SWAP_RB>(src_vec512));
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer16_AVX512(u16 *dst, size_t pixCountVec512, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
			{
				const v512u16 dst_v512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u16 *)(dst+i)) : _mm512_load_si512((v512u16 *)(dst+i));
				const v512u16 tempDst = _mm512_or_si512( _mm512_or_si512(_mm512_srli_epi16(_mm512_and_si512(dst_v512, _mm512_set1_epi16(0x7C00)), 10), _mm512_or_si512(_mm512_and_si512(dst_v512, _mm512_set1_epi16(0x0E30)), _mm512_slli_epi16(_mm512_and_si512(dst_v512, _mm512_set1_epi16(0x001F)), 10))), _mm512_and_si512(dst_v512, _mm512_set1_epi16(0x8000)) );
				
				if (IS_UNALIGNED)
				{
					_mm512_storeu_si512( (v512u16 *)(dst+i), tempDst);
				}
				else
				{
					_mm512_store_si512( (v512u16 *)(dst+i), tempDst);
				}
			}
		}
		else
		{
			return pixCountVec512;
		}
	}
	else if (intensity < 0.001f)
	{
		for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
		{
			if (IS_UNALIGNED)
			{
				_mm512_storeu_si512( (v512u16 *)(dst+i), _mm512_and_si512(_mm512_loadu_si512((v512u16 *)(dst+i)), _mm512_set1_epi16(0x8000)) );
			}
			else
			{
				_mm512_store_si512( (v512u16 *)(dst+i), _mm512_and_si512(_mm512_load_si512((v512u16 *)(dst+i)), _mm512_set1_epi16(0x8000)) );
			}
		}
	}
	else
	{
		const v512u16 intensity_v512 = _mm512_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec512; i+=(sizeof(v512u16)/sizeof(u16)))
		{
			v512u16 dst_v512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u16 *)(dst+i)) : _mm512_load_si512((v512u16 *)(dst+i));
			v512u16 tempDst = (SWAP_RB) ? _mm512_or_si512( _mm512_or_si512(_mm512_srli_epi16(_mm512_and_si512(dst_v512, _mm512_set1_epi16(0x7C00)), 10), _mm512_or_si512(_mm512_and_si512(dst_v512, _mm512_set1_epi16(0x0E30)), _mm512_slli_epi16(_mm512_and_si512(dst_v512, _mm512_set1_epi16(0x001F)), 10))), _mm512_and_si512(dst_v512, _mm512_set1_epi16(0x8000)) ) : dst_v512;
			
			v512u16 r = _mm512_and_si512(                   tempDst,      _mm512_set1_epi16(0x001F) );
			v512u16 g = _mm512_and_si512( _mm512_srli_epi16(tempDst,  5), _mm512_set1_epi16(0x001F) );
			v512u16 b = _mm512_and_si512( _mm512_srli_epi16(tempDst, 10), _mm512_set1_epi16(0x001F) );
			v512u16 a = _mm512_and_si512(                   tempDst,      _mm512_set1_epi16(0x8000) );
			
			r =                    _mm512_mulhi_epu16(r, intensity_v512);
			g = _mm512_slli_epi32( _mm512_mulhi_epu16(g, intensity_v512),  5 );
			b = _mm512_slli_epi32( _mm512_mulhi_epu16(b, intensity_v512), 10 );
			
			tempDst = _mm512_or_si512( _mm512_or_si512( _mm512_or_si512(r, g), b), a);
			
			if (IS_UNALIGNED)
			{
				_mm512_storeu_si512((v512u16 *)(dst+i), tempDst);
			}
			else
			{
				_mm512_store_si512((v512u16 *)(dst+i), tempDst);
			}
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer32_AVX512(u32 *dst, size_t pixCountVec512, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
			{
				const v512u32 dst_v512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u32 *)(dst+i)) : _mm512_load_si512((v512u32 *)(dst+i));
				const v512u32 tempDst = _mm512_shuffle_epi8(dst_v512, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2));
				
				if (IS_UNALIGNED)
				{
					_mm512_storeu_si512( (v512u32 *)(dst+i), tempDst);
				}
				else
				{
					_mm512_store_si512( (v512u32 *)(dst+i), tempDst);
				}
			}
		}
		else
		{
			return pixCountVec512;
		}
	}
	else if (intensity < 0.001f)
	{
		for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
		{
			if (IS_UNALIGNED)
			{
				_mm512_storeu_si512( (v512u32 *)(dst+i), _mm512_and_si512(_mm512_loadu_si512((v512u32 *)(dst+i)), _mm512_set1_epi32(0xFF000000)) );
			}
			else
			{
				_mm512_store_si512( (v512u32 *)(dst+i), _mm512_and_si512(_mm512_load_si512((v512u32 *)(dst+i)), _mm512_set1_epi32(0xFF000000)) );
			}
		}
	}
	else
	{
		const v512u16 intensity_v512 = _mm512_set1_epi16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec512; i+=(sizeof(v512u32)/sizeof(u32)))
		{
			v512u32 dst_v512 = (IS_UNALIGNED) ? _mm512_loadu_si512((v512u32 *)(dst+i)) : _mm512_load_si512((v512u32 *)(dst+i));
			v512u32 tempDst = (SWAP_RB) ? _mm512_shuffle_epi8(dst_v512, _mm512_set_epi8(63,60,61,62, 59,56,57,58, 55,52,53,54, 51,48,49,50, 47,44,45,46, 43,40,41,42, 39,36,37,38, 35,32,33,34, 31,28,29,30,  27,24,25,26,  23,20,21,22,  19,16,17,18,  15,12,13,14,  11,8,9,10,  7,4,5,6,  3,0,1,2)) : dst_v512;
			
			v512u16 rb = _mm512_and_si512(                   tempDst,      _mm512_set1_epi32(0x00FF00FF) );
			v512u16 g  = _mm512_and_si512( _mm512_srli_epi32(tempDst,  8), _mm512_set1_epi32(0x000000FF) );
			v512u32 a  = _mm512_and_si512(                   tempDst,      _mm512_set1_epi32(0xFF000000) );
			
			rb =                    _mm512_mulhi_epu16(rb, intensity_v512);
			g  = _mm512_slli_epi32( _mm512_mulhi_epu16( g, intensity_v512),  8 );
			
			tempDst = _mm512_or_si512( _mm512_or_si512(rb, g), a);
			
			if (IS_UNALIGNED)
			{
				_mm512_storeu_si512((v512u32 *)(dst+i), tempDst);
			}
			else
			{
				_mm512_store_si512((v512u32 *)(dst+i), tempDst);
			}
		}
	}
	
	return i;
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To8888Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To8888Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To6665Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555To6665Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To6665_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To6665_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To8888_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To8888_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer8888To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer6665To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo8888Opaque_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo8888Opaque_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555XTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555XTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555XTo888_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer555XTo888_SwapRB_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555XTo888_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX512<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo888_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX512<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ConvertBuffer888XTo888_SwapRB_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo888_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::CopyBuffer16_SwapRB_IsUnaligned(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_AVX512<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::CopyBuffer32_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_AVX512<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer16(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX512<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer16_SwapRB(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX512<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer16_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX512<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer16_SwapRB_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_AVX512<true, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer32(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX512<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer32_SwapRB(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX512<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer32_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX512<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_AVX512::ApplyIntensityToBuffer32_SwapRB_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_AVX512<true, true>(dst, pixCount, intensity);
}

template void ColorspaceConvert555To8888_AVX512<true>(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi);
template void ColorspaceConvert555To8888_AVX512<false>(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi);

template void ColorspaceConvert555XTo888X_AVX512<true>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template void ColorspaceConvert555XTo888X_AVX512<false>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);

template void ColorspaceConvert555To6665_AVX512<true>(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi);
template void ColorspaceConvert555To6665_AVX512<false>(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi);

template void ColorspaceConvert555XTo666X_AVX512<true>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template void ColorspaceConvert555XTo666X_AVX512<false>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);

template void ColorspaceConvert555To8888Opaque_AVX512<true>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template void ColorspaceConvert555To8888Opaque_AVX512<false>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);

template void ColorspaceConvert555To6665Opaque_AVX512<true>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template void ColorspaceConvert555To6665Opaque_AVX512<false>(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);

template v512u32 ColorspaceConvert8888To6665_AVX512<true>(const v512u32 &src);
template v512u32 ColorspaceConvert8888To6665_AVX512<false>(const v512u32 &src);

template v512u32 ColorspaceConvert6665To8888_AVX512<true>(const v512u32 &src);
template v512u32 ColorspaceConvert6665To8888_AVX512<false>(const v512u32 &src);

template v512u16 ColorspaceConvert8888To5551_AVX512<true>(const v512u32 &srcLo, const v512u32 &srcHi);
template v512u16 ColorspaceConvert8888To5551_AVX512<false>(const v512u32 &srcLo, const v512u32 &srcHi);

template v512u16 ColorspaceConvert6665To5551_AVX512<true>(const v512u32 &srcLo, const v512u32 &srcHi);
template v512u16 ColorspaceConvert6665To5551_AVX512<false>(const v512u32 &srcLo, const v512u32 &srcHi);

template v512u32 ColorspaceConvert888XTo8888Opaque_AVX512<true>(const v512u32 &src);
template v512u32 ColorspaceConvert888XTo8888Opaque_AVX512<false>(const v512u32 &src);

template v512u16 ColorspaceCopy16_AVX512<true>(const v512u16 &src);
template v512u16 ColorspaceCopy16_AVX512<false>(const v512u16 &src);

template v512u32 ColorspaceCopy32_AVX512<true>(const v512u32 &src);
template v512u32 ColorspaceCopy32_AVX512<false>(const v512u32 &src);

template v512u16 ColorspaceApplyIntensity16_AVX512<true>(const v512u16 &src, float intensity);
template v512u16 ColorspaceApplyIntensity16_AVX512<false>(const v512u16 &src, float intensity);

template v512u32 ColorspaceApplyIntensity32_AVX512<true>(const v512u32 &src, float intensity);
template v512u32 ColorspaceApplyIntensity32_AVX512<false>(const v512u32 &src, float intensity);

#endif // ENABLE_AVX512_1
