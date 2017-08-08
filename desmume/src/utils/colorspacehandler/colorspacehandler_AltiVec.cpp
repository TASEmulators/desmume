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

#include "colorspacehandler_Altivec.h"

#ifndef ENABLE_ALTIVEC
	#error This code requires PowerPC AltiVec support.
#else

#include <string.h>

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888_AltiVec(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	dstLo = vec_unpackl((vector pixel)srcColor);
	dstLo = vec_or( vec_sl((v128u8)dstLo, ((v128u8){0,3,3,3,  0,3,3,3,  0,3,3,3,  0,3,3,3})), vec_sr((v128u8)dstLo, ((v128u8){0,2,2,2,  0,2,2,2,  0,2,2,2,  0,2,2,2})) );
	dstLo = vec_sel(dstLo, srcAlphaBits32Lo, ((v128u32){0xFF000000,0xFF000000,0xFF000000,0xFF000000}));
	
	dstHi = vec_unpackh((vector pixel)srcColor);
	dstHi = vec_or( vec_sl((v128u8)dstHi, ((v128u8){0,3,3,3,  0,3,3,3,  0,3,3,3,  0,3,3,3})), vec_sr((v128u8)dstHi, ((v128u8){0,2,2,2,  0,2,2,2,  0,2,2,2,  0,2,2,2})) );
	dstHi = vec_sel(dstHi, srcAlphaBits32Hi, ((v128u32){0xFF000000,0xFF000000,0xFF000000,0xFF000000}));
	
	if (SWAP_RB)
	{
		dstLo = vec_perm(dstLo, dstLo, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}));
		dstHi = vec_perm(dstHi, dstHi, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}));
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665_AltiVec(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi)
{	
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	dstLo = vec_unpackl((vector pixel)srcColor);
	dstLo = vec_or( vec_sl((v128u8)dstLo, ((v128u8){0,1,1,1,  0,1,1,1,  0,1,1,1,  0,1,1,1})), vec_sr((v128u8)dstLo, ((v128u8){0,4,4,4,  0,4,4,4,  0,4,4,4,  0,4,4,4})) );
	dstLo = vec_sel(dstLo, srcAlphaBits32Lo, ((v128u32){0xFF000000,0xFF000000,0xFF000000,0xFF000000}));
	
	dstHi = vec_unpackh((vector pixel)srcColor);
	dstHi = vec_or( vec_sl((v128u8)dstHi, ((v128u8){0,1,1,1,  0,1,1,1,  0,1,1,1,  0,1,1,1})), vec_sr((v128u8)dstHi, ((v128u8){0,4,4,4,  0,4,4,4,  0,4,4,4,  0,4,4,4})) );
	dstHi = vec_sel(dstHi, srcAlphaBits32Hi, ((v128u32){0xFF000000,0xFF000000,0xFF000000,0xFF000000}));
	
	if (SWAP_RB)
	{
		dstLo = vec_perm(dstLo, dstLo, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}));
		dstHi = vec_perm(dstHi, dstHi, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}));
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To8888Opaque_AltiVec(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u32 srcAlphaBits32 = {0xFF000000, 0xFF000000, 0xFF000000, 0xFF000000};
	ColorspaceConvert555To8888_AltiVec<SWAP_RB>(srcColor, srcAlphaBits32, srcAlphaBits32, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555To6665Opaque_AltiVec(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u32 srcAlphaBits32 = {0x1F000000, 0x1F000000, 0x1F000000, 0x1F000000};
	ColorspaceConvert555To6665_AltiVec<SWAP_RB>(srcColor, srcAlphaBits32, srcAlphaBits32, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert8888To6665_AltiVec(const v128u32 &src)
{
	// Conversion algorithm:
	//    RGB   8-bit to 6-bit formula: dstRGB6 = (srcRGB8 >> 2)
	//    Alpha 8-bit to 6-bit formula: dstA5   = (srcA8   >> 3)	
	v128u8 rgba = vec_sr( (v128u8)src, ((v128u8){3,2,2,2,  3,2,2,2,  3,2,2,2,  3,2,2,2}) );
	
	if (SWAP_RB)
	{
		rgba = vec_perm( rgba, rgba, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}) );
	}
	
	return (v128u32)rgba;
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert6665To8888_AltiVec(const v128u32 &src)
{
	// Conversion algorithm:
	//    RGB   6-bit to 8-bit formula: dstRGB8 = (srcRGB6 << 2) | ((srcRGB6 >> 4) & 0x03)
	//    Alpha 5-bit to 8-bit formula: dstA8   = (srcA5   << 3) | ((srcA5   >> 2) & 0x07)
	v128u8 rgba = vec_or( vec_sl((v128u8)src, ((v128u8){3,2,2,2,  3,2,2,2,  3,2,2,2,  3,2,2,2})), vec_sr((v128u8)src, ((v128u8){2,4,4,4,  2,4,4,4,  2,4,4,4,  2,4,4,4})) );
	
	if (SWAP_RB)
	{		
		rgba = vec_perm( rgba, rgba, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}) );
	}
	
	return (v128u32)rgba;
}

template <NDSColorFormat COLORFORMAT, bool SWAP_RB>
FORCEINLINE v128u16 _ConvertColorBaseTo5551_AltiVec(const v128u32 &srcLo, const v128u32 &srcHi)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		return srcLo;
	}
	
	v128u32 rgbLo;
	v128u32 rgbHi;
	
	v128u16 dstColor;
	v128u16 dstAlpha;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		// Convert alpha
		dstAlpha = vec_packsu( vec_and(vec_sr(srcLo, ((v128u32){24,24,24,24})), ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F})), vec_and(vec_sr(srcHi, ((v128u32){24,24,24,24})), ((v128u32){0x0000001F,0x0000001F,0x0000001F,0x0000001F})) );
		dstAlpha = vec_cmpgt(dstAlpha, ((v128u16){0,0,0,0,0,0,0,0}));
		dstAlpha = vec_and(dstAlpha, ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}));
		
		// Convert RGB
		if (SWAP_RB)
		{
			rgbLo = vec_perm( srcLo, srcLo, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}) );
			rgbHi = vec_perm( srcHi, srcHi, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}) );
			
			rgbLo = vec_sl( rgbLo, ((v128u32){2,2,2,2}) );
			rgbHi = vec_sl( rgbHi, ((v128u32){2,2,2,2}) );
			
			dstColor = (v128u16)vec_packpx(rgbLo, rgbHi);
		}
		else
		{
			rgbLo = vec_sl( srcLo, ((v128u32){2,2,2,2}) );
			rgbHi = vec_sl( srcHi, ((v128u32){2,2,2,2}) );
			
			dstColor = (v128u16)vec_packpx(rgbLo, rgbHi);
		}
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		// Convert alpha
		dstAlpha = vec_packsu( vec_sr(srcLo, ((v128u32){24,24,24,24})), vec_sr(srcHi, ((v128u32){24,24,24,24})) );
		dstAlpha = vec_cmpgt(dstAlpha, ((v128u16){0,0,0,0,0,0,0,0}));
		dstAlpha = vec_and(dstAlpha, ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}));
		
		// Convert RGB
		if (SWAP_RB)
		{
			rgbLo = vec_perm( srcLo, srcLo, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}) );
			rgbHi = vec_perm( srcHi, srcHi, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}) );
			
			dstColor = (v128u16)vec_packpx(rgbLo, rgbHi);
		}
		else
		{
			dstColor = (v128u16)vec_packpx(srcLo, srcHi);
		}
	}
	
	dstColor = vec_and(dstColor, ((v128u16){0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF,0x7FFF}));
	return vec_or(dstColor, dstAlpha);
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceConvert8888To5551_AltiVec(const v128u32 &srcLo, const v128u32 &srcHi)
{
	return _ConvertColorBaseTo5551_AltiVec<NDSColorFormat_BGR888_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceConvert6665To5551_AltiVec(const v128u32 &srcLo, const v128u32 &srcHi)
{
	return _ConvertColorBaseTo5551_AltiVec<NDSColorFormat_BGR666_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert888XTo8888Opaque_AltiVec(const v128u32 &src)
{
	if (SWAP_RB)
	{
		return vec_or( vec_perm(src, src, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13})), ((v128u32){0xFF000000,0xFF000000,0xFF000000,0xFF000000}) );
	}
	
	return vec_or(src, ((v128u32){0xFF000000,0xFF000000,0xFF000000,0xFF000000}));
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceCopy16_AltiVec(const v128u16 &src)
{
	if (SWAP_RB)
	{
		return vec_or( vec_or(vec_sr(vec_and(src, ((v128u16){0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00,0x7C00})), ((v128u16){10,10,10,10,10,10,10,10})), vec_or(vec_and(src, ((v128u16){0x0E30,0x0E30,0x0E30,0x0E30,0x0E30,0x0E30,0x0E30,0x0E30})), vec_sl(vec_and(src, ((v128u16){0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F,0x001F})), ((v128u16){10,10,10,10,10,10,10,10})))), vec_and(src, ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000})) );
	}
	
	return src;
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceCopy32_AltiVec(const v128u32 &src)
{
	if (SWAP_RB)
	{
		return vec_perm(src, src, ((v128u8){0,3,2,1,  4,7,6,5,  8,11,10,9,  12,15,14,13}));
	}
	
	return src;
}

template <bool SWAP_RB>
static size_t ColorspaceConvertBuffer555To8888Opaque_AltiVec(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		v128u32 dstConvertedLo, dstConvertedHi;
		
		ColorspaceConvert555To8888Opaque_AltiVec<SWAP_RB>( vec_ld(0, src+i), dstConvertedLo, dstConvertedHi );
		vec_st(dstConvertedHi,  0, dst+i);
		vec_st(dstConvertedLo, 16, dst+i);
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceConvertBuffer555To6665Opaque_AltiVec(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		v128u32 dstConvertedLo, dstConvertedHi;
		
		ColorspaceConvert555To6665Opaque_AltiVec<SWAP_RB>( vec_ld(0, src+i), dstConvertedLo, dstConvertedHi );
		vec_st(dstConvertedHi,  0, dst+i);
		vec_st(dstConvertedLo, 16, dst+i);
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceConvertBuffer8888To6665_AltiVec(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		vec_st( ColorspaceConvert8888To6665_AltiVec<SWAP_RB>(vec_ld(0, src+i)), 0, dst+i );
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceConvertBuffer6665To8888_AltiVec(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		vec_st( ColorspaceConvert6665To8888_AltiVec<SWAP_RB>(vec_ld(0, src+i)), 0, dst+i );
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceConvertBuffer8888To5551_AltiVec(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		vec_st( ColorspaceConvert8888To5551_AltiVec<SWAP_RB>(vec_ld(0, src+i), vec_ld(16, src+i)), 0, dst+i );
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceConvertBuffer6665To5551_AltiVec(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		vec_st( ColorspaceConvert6665To5551_AltiVec<SWAP_RB>(vec_ld(0, src+i), vec_ld(16, src+i)), 0, dst+i );
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceConvertBuffer888XTo8888Opaque_AltiVec(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		vec_st( ColorspaceConvert888XTo8888Opaque_AltiVec<SWAP_RB>(vec_ld(0, src+i)), 0, dst+i );
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceCopyBuffer16_AltiVec(const u16 *src, u16 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u16));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=8)
	{
		vec_st( ColorspaceCopy16_AltiVec<SWAP_RB>(vec_ld(0, src+i)), 0, dst+i );
	}
	
	return i;
}

template <bool SWAP_RB>
size_t ColorspaceCopyBuffer32_AltiVec(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u32));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=4)
	{
		vec_st( ColorspaceCopy32_AltiVec<SWAP_RB>(vec_ld(0, src+i)), 0, dst+i );
	}
	
	return i;
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer555To8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To8888Opaque_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer555To6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555To6665Opaque_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer888XTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AltiVec<false>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::ConvertBuffer888XTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888XTo8888Opaque_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_AltiVec<true>(src, dst, pixCount);
}

size_t ColorspaceHandler_AltiVec::CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_AltiVec<true>(src, dst, pixCount);
}

template void ColorspaceConvert555To8888_AltiVec<true>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To8888_AltiVec<false>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To6665_AltiVec<true>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To6665_AltiVec<false>(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To8888Opaque_AltiVec<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To8888Opaque_AltiVec<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555To6665Opaque_AltiVec<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555To6665Opaque_AltiVec<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template v128u32 ColorspaceConvert8888To6665_AltiVec<true>(const v128u32 &src);
template v128u32 ColorspaceConvert8888To6665_AltiVec<false>(const v128u32 &src);

template v128u32 ColorspaceConvert6665To8888_AltiVec<true>(const v128u32 &src);
template v128u32 ColorspaceConvert6665To8888_AltiVec<false>(const v128u32 &src);

template v128u16 ColorspaceConvert8888To5551_AltiVec<true>(const v128u32 &srcLo, const v128u32 &srcHi);
template v128u16 ColorspaceConvert8888To5551_AltiVec<false>(const v128u32 &srcLo, const v128u32 &srcHi);

template v128u16 ColorspaceConvert6665To5551_AltiVec<true>(const v128u32 &srcLo, const v128u32 &srcHi);
template v128u16 ColorspaceConvert6665To5551_AltiVec<false>(const v128u32 &srcLo, const v128u32 &srcHi);

template v128u32 ColorspaceConvert888XTo8888Opaque_AltiVec<true>(const v128u32 &src);
template v128u32 ColorspaceConvert888XTo8888Opaque_AltiVec<false>(const v128u32 &src);

template v128u16 ColorspaceCopy16_AltiVec<true>(const v128u16 &src);
template v128u16 ColorspaceCopy16_AltiVec<false>(const v128u16 &src);

template v128u32 ColorspaceCopy32_AltiVec<true>(const v128u32 &src);
template v128u32 ColorspaceCopy32_AltiVec<false>(const v128u32 &src);

#endif // ENABLE_SSE2
