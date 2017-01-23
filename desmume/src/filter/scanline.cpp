/*
	Copyright (C) 2009-2017 DeSmuME team

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

#include "filter.h"
#include "types.h"
#include <stdio.h>
#include <string.h>

extern int scanline_filter_a, scanline_filter_b, scanline_filter_c, scanline_filter_d;
static int fac_a, fac_b, fac_c, fac_d;

#if defined(ENABLE_SSE2)
template <size_t LINEWIDTH>
FORCEINLINE static void ScanLine32_FastSSE2(u32 *__restrict lpDst, const u32 *__restrict lpSrc, const int fac_left, const int fac_right)
{
	const v128u16 weight = _mm_set_epi16(16, fac_right, fac_right, fac_right, 16, fac_left, fac_left, fac_left);
	
	for (size_t i = 0; i < LINEWIDTH; i+=4, lpSrc+=4, lpDst+=8)
	{
		const v128u32 src = _mm_load_si128((v128u32 *__restrict)lpSrc);
		const v128u32 srcLo = _mm_unpacklo_epi32(src, src);
		const v128u32 srcHi = _mm_unpackhi_epi32(src, src);
		
		const v128u32 srcLo0 = _mm_srli_epi16( _mm_mullo_epi16(_mm_unpacklo_epi8(srcLo, _mm_setzero_si128()), weight), 4 );
		const v128u32 srcLo1 = _mm_srli_epi16( _mm_mullo_epi16(_mm_unpackhi_epi8(srcLo, _mm_setzero_si128()), weight), 4 );
		const v128u32 srcHi0 = _mm_srli_epi16( _mm_mullo_epi16(_mm_unpacklo_epi8(srcHi, _mm_setzero_si128()), weight), 4 );
		const v128u32 srcHi1 = _mm_srli_epi16( _mm_mullo_epi16(_mm_unpackhi_epi8(srcHi, _mm_setzero_si128()), weight), 4 );
		
		_mm_stream_si128( (v128u32 *__restrict)(lpDst + 0), _mm_packus_epi16(srcLo0, srcLo1) );
		_mm_stream_si128( (v128u32 *__restrict)(lpDst + 4), _mm_packus_epi16(srcHi0, srcHi1) );
	}
}
#endif

FORCEINLINE static void ScanLine32(u32 *__restrict lpDst, const u32 *__restrict lpSrc, const size_t lineWidth, const int fac_left, const int fac_right)
{
	const u8 *__restrict src8 = (const u8 *__restrict)lpSrc;
	u8 *__restrict dst8 = (u8 *__restrict)lpDst;
	
	for (size_t i = 0; i < lineWidth; i++, lpSrc++, lpDst+=2)
	{
#ifdef MSB_FIRST
		dst8[(i*8)+1] = src8[(i*4)+1] * fac_left / 16;
		dst8[(i*8)+2] = src8[(i*4)+2] * fac_left / 16;
		dst8[(i*8)+3] = src8[(i*4)+3] * fac_left / 16;
		
		dst8[(i*8)+5] = src8[(i*4)+1] * fac_right / 16;
		dst8[(i*8)+6] = src8[(i*4)+2] * fac_right / 16;
		dst8[(i*8)+7] = src8[(i*4)+3] * fac_right / 16;
#else
		dst8[(i*8)+0] = src8[(i*4)+0] * fac_left / 16;
		dst8[(i*8)+1] = src8[(i*4)+1] * fac_left / 16;
		dst8[(i*8)+2] = src8[(i*4)+2] * fac_left / 16;
		
		dst8[(i*8)+4] = src8[(i*4)+0] * fac_right / 16;
		dst8[(i*8)+5] = src8[(i*4)+1] * fac_right / 16;
		dst8[(i*8)+6] = src8[(i*4)+2] * fac_right / 16;
#endif
	}
}

template <size_t LINEWIDTH>
FORCEINLINE static void DoubleLine32_Fast(u32 *__restrict lpDst, const u32 *__restrict lpSrc)
{
	for (size_t i = 0; i < LINEWIDTH; i++)
	{
		lpDst[(i*2)+0] = lpSrc[i];
		lpDst[(i*2)+1] = lpSrc[i];
	}
}

FORCEINLINE static void DoubleLine32(u32 *__restrict lpDst, const u32 *__restrict lpSrc, const size_t lineWidth)
{
	for (size_t i = 0; i < lineWidth; i++)
	{
		lpDst[(i*2)+0] = lpSrc[i];
		lpDst[(i*2)+1] = lpSrc[i];
	}
}

void RenderScanline(SSurface Src, SSurface Dst)
{
	fac_a = (16-scanline_filter_a);
	fac_b = (16-scanline_filter_b);
	fac_c = (16-scanline_filter_c);
	fac_d = (16-scanline_filter_d);
	size_t dstLineIndex = 0;

	const size_t srcHeight = Src.Height;
	const size_t srcPitch = Src.Pitch >> 1;
	u32 *lpSrc = (u32 *)Src.Surface;

	const size_t dstPitch = Dst.Pitch >> 1;
	u32 *lpDst = (u32 *)Dst.Surface;
	
#ifdef ENABLE_SSE2
	if (Src.Width == 256)
	{
		for (; dstLineIndex < srcHeight; dstLineIndex++, lpSrc += srcPitch)
		{
			ScanLine32_FastSSE2<256>(lpDst, lpSrc, fac_a, fac_b);
			lpDst += dstPitch;
			ScanLine32_FastSSE2<256>(lpDst, lpSrc, fac_c, fac_d);
			lpDst += dstPitch;
		}
	}
	else
#endif
	{
		for (; dstLineIndex < srcHeight; dstLineIndex++, lpSrc += srcPitch)
		{
			ScanLine32(lpDst, lpSrc, Src.Width, fac_a, fac_b);
			lpDst += dstPitch;
			ScanLine32(lpDst, lpSrc, Src.Width, fac_c, fac_d);
			lpDst += dstPitch;
		}
	}
}

void RenderNearest2X(SSurface Src, SSurface Dst)
{
	size_t dstLineIndex = 0;
	
	const size_t srcHeight = Src.Height;
	const size_t srcPitch = Src.Pitch >> 1;
	const u32 *lpSrc = (u32 *)Src.Surface;

	const size_t dstPitch = Dst.Pitch >> 1;
	u32 *lpDst = (u32 *)Dst.Surface;
	
	if (Src.Width == 256)
	{
		for (; dstLineIndex < srcHeight; dstLineIndex++, lpSrc += srcPitch)
		{
			DoubleLine32_Fast<256>(lpDst, lpSrc);
			lpDst += dstPitch;
			DoubleLine32_Fast<256>(lpDst, lpSrc);
			lpDst += dstPitch;
		}
	}
	else
	{
		for (; dstLineIndex < srcHeight; dstLineIndex++, lpSrc += srcPitch)
		{
			DoubleLine32(lpDst, lpSrc, Src.Width);
			lpDst += dstPitch;
			DoubleLine32(lpDst, lpSrc, Src.Width);
			lpDst += dstPitch;
		}
	}
}
