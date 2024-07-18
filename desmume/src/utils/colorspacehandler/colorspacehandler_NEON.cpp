/*
	Copyright (C) 2016-2024 DeSmuME team
 
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

#include "colorspacehandler_NEON.h"

#ifndef ENABLE_NEON_A64
	#error This code requires ARM64 NEON support.
#else

#include <arm_neon.h>

#define COLOR16_SWAPRB_NEON(src) vorrq_u16( vshlq_n_u16(vandq_u16(src,vdupq_n_u16(0x001F)),10), vorrq_u16( vandq_u16(src,vdupq_n_u16(0x03E0)), vorrq_u16(vshrq_n_u16(vandq_u16(src,vdupq_n_u16(0x7C00)),10), vandq_u16(src,vdupq_n_u16(0x8000))) ) )

#define COLOR32_SWAPRB_NEON(src) vreinterpretq_u32_u8( vqtbl1q_u8(vreinterpretq_u8_u32(src), ((v128u8){2,1,0,3,  6,5,4,7,  10,9,8,11,  14,13,12,15})) )

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555aTo8888_NEON(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	if (SWAP_RB)
	{
		v128u16 rb = vorrq_u16( vshlq_n_u16(srcColor,11), vandq_u16(vshrq_n_u16(srcColor, 7), vdupq_n_u16(0x00F8)) );
		rb = vorrq_u16(rb, vandq_u16(vshrq_n_u16(rb, 5), vdupq_n_u16(0x0707)));
		
		v128u16 ga = vandq_u16(vshrq_n_u16(srcColor, 2), vdupq_n_u16(0x00F8) );
		ga = vorrq_u16(ga, vshrq_n_u16(ga, 5));
		ga = vorrq_u16(ga, srcAlphaBits);
		
		dstLo = vreinterpretq_u32_u8( vzip1q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(ga)) );
		dstHi = vreinterpretq_u32_u8( vzip2q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(ga)) );
	}
	else
	{
		v128u16 rg = vorrq_u16( vandq_u16( vshlq_n_u16(srcColor,3), vdupq_n_u16(0x00F8) ), vandq_u16( vshlq_n_u16(srcColor,6), vdupq_n_u16(0xF800) ) );
		v128u16 ba = vandq_u16( vshrq_n_u16(srcColor,7), vdupq_n_u16(0x00F8) );
		
		rg = vorrq_u16( rg, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16(rg), 5)) );
		ba = vorrq_u16( ba, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16(ba), 5)) );
		ba = vorrq_u16( ba, srcAlphaBits );
		
		dstLo = vreinterpretq_u32_u16( vzip1q_u16(rg, ba) );
		dstHi = vreinterpretq_u32_u16( vzip2q_u16(rg, ba) );
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555xTo888x_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 8-bit formula: dstRGB8 = (srcRGB5 << 3) | ((srcRGB5 >> 2) & 0x07)
	
	if (SWAP_RB)
	{
		v128u16 rb = vorrq_u16( vshlq_n_u16(srcColor,11), vandq_u16(vshrq_n_u16(srcColor, 7), vdupq_n_u16(0x00F8)) );
		rb = vorrq_u16(rb, vandq_u16(vshrq_n_u16(rb, 5), vdupq_n_u16(0x0707)));
		
		v128u16 g = vandq_u16(vshrq_n_u16(srcColor, 2), vdupq_n_u16(0x00F8) );
		g = vorrq_u16(g, vshrq_n_u16(g, 5));
		
		dstLo = vreinterpretq_u32_u8( vzip1q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(g)) );
		dstHi = vreinterpretq_u32_u8( vzip2q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(g)) );
	}
	else
	{
		v128u16 rg = vorrq_u16( vandq_u16( vshlq_n_u16(srcColor,3), vdupq_n_u16(0x00F8) ), vandq_u16( vshlq_n_u16(srcColor,6), vdupq_n_u16(0xF800) ) );
		v128u16  b = vandq_u16( vshrq_n_u16(srcColor,7), vdupq_n_u16(0x00F8) );
		
		rg = vorrq_u16( rg, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16(rg), 5)) );
		 b = vorrq_u16(  b, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16( b), 5)) );
		
		dstLo = vreinterpretq_u32_u16( vzip1q_u16(rg, b) );
		dstHi = vreinterpretq_u32_u16( vzip2q_u16(rg, b) );
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555aTo6665_NEON(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	if (SWAP_RB)
	{
		v128u16 rb = vandq_u16( vorrq_u16( vshlq_n_u16(srcColor,9), vshrq_n_u16(srcColor, 9)), vdupq_n_u16(0x3E3E) );
		rb = vorrq_u16(rb, vandq_u16(vshrq_n_u16(rb, 5), vdupq_n_u16(0x0101)));
		
		v128u16 ga = vandq_u16(vshrq_n_u16(srcColor, 4), vdupq_n_u16(0x003E) );
		ga = vorrq_u16(ga, vshrq_n_u16(ga, 5));
		ga = vorrq_u16(ga, srcAlphaBits);
		
		dstLo = vreinterpretq_u32_u8( vzip1q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(ga)) );
		dstHi = vreinterpretq_u32_u8( vzip2q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(ga)) );
	}
	else
	{
		v128u16 rg = vorrq_u16( vandq_u16( vshlq_n_u16(srcColor,1), vdupq_n_u16(0x003E) ), vandq_u16( vshlq_n_u16(srcColor,4), vdupq_n_u16(0x3E00) ) );
		v128u16 ba = vandq_u16( vshrq_n_u16(srcColor,9), vdupq_n_u16(0x003E) );
		
		rg = vorrq_u16( rg, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16(rg), 5)) );
		ba = vorrq_u16( ba, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16(ba), 5)) );
		ba = vorrq_u16( ba, srcAlphaBits );
		
		dstLo = vreinterpretq_u32_u16( vzip1q_u16(rg, ba) );
		dstHi = vreinterpretq_u32_u16( vzip2q_u16(rg, ba) );
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555xTo666x_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	// Conversion algorithm:
	//    RGB   5-bit to 6-bit formula: dstRGB6 = (srcRGB5 << 1) | ((srcRGB5 >> 4) & 0x01)
	
	if (SWAP_RB)
	{
		v128u16 rb = vandq_u16( vorrq_u16( vshlq_n_u16(srcColor,9), vshrq_n_u16(srcColor, 9)), vdupq_n_u16(0x3E3E) );
		rb = vorrq_u16(rb, vandq_u16(vshrq_n_u16(rb, 5), vdupq_n_u16(0x0101)));
		
		v128u16 g = vandq_u16(vshrq_n_u16(srcColor, 4), vdupq_n_u16(0x003E) );
		g = vorrq_u16(g, vshrq_n_u16(g, 5));
		
		dstLo = vreinterpretq_u32_u8( vzip1q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(g)) );
		dstHi = vreinterpretq_u32_u8( vzip2q_u8(vreinterpretq_u8_u16(rb), vreinterpretq_u8_u16(g)) );
	}
	else
	{
		v128u16 rg = vorrq_u16( vandq_u16( vshlq_n_u16(srcColor,1), vdupq_n_u16(0x003E) ), vandq_u16( vshlq_n_u16(srcColor,4), vdupq_n_u16(0x3E00) ) );
		v128u16  b = vandq_u16( vshrq_n_u16(srcColor,9), vdupq_n_u16(0x003E) );
		
		rg = vorrq_u16( rg, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16(rg), 5)) );
		 b = vorrq_u16(  b, vreinterpretq_u16_u8(vshrq_n_u8(vreinterpretq_u8_u16( b), 5)) );
		
		dstLo = vreinterpretq_u32_u16( vzip1q_u16(rg, b) );
		dstHi = vreinterpretq_u32_u16( vzip2q_u16(rg, b) );
	}
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555xTo8888Opaque_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u16 srcAlphaBits16 = vdupq_n_u16(0xFF00);
	ColorspaceConvert555aTo8888_NEON<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert555xTo6665Opaque_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128u16 srcAlphaBits16 = vdupq_n_u16(0x1F00);
	ColorspaceConvert555aTo6665_NEON<SWAP_RB>(srcColor, srcAlphaBits16, dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert5551To8888_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128s16 srcAlphaBits16 = vandq_s16( vcgtq_s16(vreinterpretq_u16_s16(srcColor), vdupq_n_s16(0xFFFF)), vdupq_n_s16(0xFF00) );
	ColorspaceConvert555aTo8888_NEON<SWAP_RB>(srcColor, vreinterpretq_s16_u16(srcAlphaBits16), dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE void ColorspaceConvert5551To6665_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi)
{
	const v128s16 srcAlphaBits16 = vandq_s16( vcgtq_s16(vreinterpretq_u16_s16(srcColor), vdupq_n_s16(0xFFFF)), vdupq_n_s16(0x1F00) );
	ColorspaceConvert555aTo6665_NEON<SWAP_RB>(srcColor, vreinterpretq_s16_u16(srcAlphaBits16), dstLo, dstHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert8888To6665_NEON(const v128u32 &src)
{
	// Conversion algorithm:
	//    RGB   8-bit to 6-bit formula: dstRGB6 = (srcRGB8 >> 2)
	//    Alpha 8-bit to 6-bit formula: dstA5   = (srcA8   >> 3)
	v128u32 rgba = vreinterpretq_u32_u8( vshlq_u8(vreinterpretq_u8_u32(src), ((v128s8){-2,-2,-2,-3,  -2,-2,-2,-3,  -2,-2,-2,-3,  -2,-2,-2,-3})) );
	
	if (SWAP_RB)
	{
		return COLOR32_SWAPRB_NEON(rgba);
	}
	
	return rgba;
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert6665To8888_NEON(const v128u32 &src)
{
	// Conversion algorithm:
	//    RGB   6-bit to 8-bit formula: dstRGB8 = (srcRGB6 << 2) | ((srcRGB6 >> 4) & 0x03)
	//    Alpha 5-bit to 8-bit formula: dstA8   = (srcA5   << 3) | ((srcA5   >> 2) & 0x07)
	v128u32 rgba = vreinterpretq_u32_u8( vorrq_u8( vshlq_u8(vreinterpretq_u8_u32(src), ((v128s8){2,2,2,3,  2,2,2,3,  2,2,2,3,  2,2,2,3})), vshlq_u8(vreinterpretq_u8_u32(src), ((v128s8){-4,-4,-4,-2,  -4,-4,-4,-2,  -4,-4,-4,-2,  -4,-4,-4,-2})) ) );
	
	if (SWAP_RB)
	{
		return COLOR32_SWAPRB_NEON(rgba);
	}
	
	return rgba;
}

template <NDSColorFormat COLORFORMAT, bool SWAP_RB>
FORCEINLINE v128u16 _ConvertColorBaseTo5551_NEON(const v128u32 &srcLo, const v128u32 &srcHi)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		return vreinterpretq_u16_u32(srcLo);
	}
	
	v128u32 rgbLo;
	v128u32 rgbHi;
	v128u16 alpha;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                  vandq_u32(vshrq_n_u32(srcLo, 17), vdupq_n_u32(0x0000001F));
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshrq_n_u32(srcLo,  4), vdupq_n_u32(0x000003E0)) );
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshlq_n_u32(srcLo,  9), vdupq_n_u32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                  vandq_u32(vshrq_n_u32(srcHi, 17), vdupq_n_u32(0x0000001F));
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshrq_n_u32(srcHi,  4), vdupq_n_u32(0x000003E0)) );
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshlq_n_u32(srcHi,  9), vdupq_n_u32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                  vandq_u32(vshrq_n_u32(srcLo,  1), vdupq_n_u32(0x0000001F));
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshrq_n_u32(srcLo,  4), vdupq_n_u32(0x000003E0)) );
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshrq_n_u32(srcLo,  7), vdupq_n_u32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                  vandq_u32(vshrq_n_u32(srcHi,  1), vdupq_n_u32(0x0000001F));
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshrq_n_u32(srcHi,  4), vdupq_n_u32(0x000003E0)) );
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshrq_n_u32(srcHi,  7), vdupq_n_u32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = vuzp1q_u16( vreinterpretq_u16_u32(vandq_u32(vshrq_n_u32(srcLo, 24), vdupq_n_u32(0x0000001F))), vreinterpretq_u16_u32(vandq_u32(vshrq_n_u32(srcHi, 24), vdupq_n_u32(0x0000001F))) );
		alpha = vcgtq_u16(alpha, vdupq_n_u16(0));
		alpha = vandq_u16(alpha, vdupq_n_u16(0x8000));
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		if (SWAP_RB)
		{
			// Convert color from low bits
			rgbLo =                  vandq_u32(vshrq_n_u32(srcLo, 19), vdupq_n_u32(0x0000001F));
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshrq_n_u32(srcLo,  6), vdupq_n_u32(0x000003E0)) );
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshlq_n_u32(srcLo,  7), vdupq_n_u32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                  vandq_u32(vshrq_n_u32(srcHi, 19), vdupq_n_u32(0x0000001F));
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshrq_n_u32(srcHi,  6), vdupq_n_u32(0x000003E0)) );
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshlq_n_u32(srcHi,  7), vdupq_n_u32(0x00007C00)) );
		}
		else
		{
			// Convert color from low bits
			rgbLo =                  vandq_u32(vshrq_n_u32(srcLo,  3), vdupq_n_u32(0x0000001F));
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshrq_n_u32(srcLo,  6), vdupq_n_u32(0x000003E0)) );
			rgbLo = vorrq_u32(rgbLo, vandq_u32(vshrq_n_u32(srcLo,  9), vdupq_n_u32(0x00007C00)) );
			
			// Convert color from high bits
			rgbHi =                  vandq_u32(vshrq_n_u32(srcHi,  3), vdupq_n_u32(0x0000001F));
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshrq_n_u32(srcHi,  6), vdupq_n_u32(0x000003E0)) );
			rgbHi = vorrq_u32(rgbHi, vandq_u32(vshrq_n_u32(srcHi,  9), vdupq_n_u32(0x00007C00)) );
		}
		
		// Convert alpha
		alpha = vuzp1q_u16( vreinterpretq_u16_u32(vshrq_n_u32(srcLo, 24)), vreinterpretq_u16_u32(vshrq_n_u32(srcHi, 24)) );
		alpha = vcgtq_u16(alpha, vdupq_n_u16(0));
		alpha = vandq_u16(alpha, vdupq_n_u16(0x8000));
	}
	
	return vorrq_u16( vuzp1q_u16(vreinterpretq_u16_u32(rgbLo), vreinterpretq_u16_u32(rgbHi)), alpha );
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceConvert8888To5551_NEON(const v128u32 &srcLo, const v128u32 &srcHi)
{
	return _ConvertColorBaseTo5551_NEON<NDSColorFormat_BGR888_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceConvert6665To5551_NEON(const v128u32 &srcLo, const v128u32 &srcHi)
{
	return _ConvertColorBaseTo5551_NEON<NDSColorFormat_BGR666_Rev, SWAP_RB>(srcLo, srcHi);
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceConvert888xTo8888Opaque_NEON(const v128u32 &src)
{
	if (SWAP_RB)
	{
		return vorrq_u32( COLOR32_SWAPRB_NEON(src), vdupq_n_u32(0xFF000000) );
	}
	
	return vorrq_u32( src, vdupq_n_u32(0xFF000000) );
}

template <bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceCopy16_NEON(const v128u16 &src)
{
	if (SWAP_RB)
	{
		return COLOR16_SWAPRB_NEON(src);
	}
	
	return src;
}

template <bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceCopy32_NEON(const v128u32 &src)
{
	if (SWAP_RB)
	{
		return COLOR32_SWAPRB_NEON(src);
	}
	
	return src;
}

template<bool SWAP_RB>
FORCEINLINE v128u16 ColorspaceApplyIntensity16_NEON(const v128u16 &src, float intensity)
{
	v128u16 tempSrc = ColorspaceCopy16_NEON<SWAP_RB>(src);
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return vandq_u16(tempSrc, vdupq_n_u16(0x8000));
	}
	
	v128u16 r = vandq_u16(             tempSrc,      vdupq_n_u16(0x001F) );
	v128u16 g = vandq_u16( vshrq_n_u16(tempSrc,  5), vdupq_n_u16(0x001F) );
	v128u16 b = vandq_u16( vshrq_n_u16(tempSrc, 10), vdupq_n_u16(0x001F) );
	v128u16 a = vandq_u16(             tempSrc,      vdupq_n_u16(0x8000) );
	
	const uint16x4_t intensityVec = vdup_n_u16( (u16)(intensity * (float)(0xFFFF)) );
	
	r =              vuzp2q_u16( vreinterpretq_u16_u32(vmull_u16(vget_low_u16(r), intensityVec)), vreinterpretq_u16_u32(vmull_u16(vget_high_u16(r), intensityVec)) );
	g = vshlq_n_u16( vuzp2q_u16( vreinterpretq_u16_u32(vmull_u16(vget_low_u16(g), intensityVec)), vreinterpretq_u16_u32(vmull_u16(vget_high_u16(g), intensityVec)) ),  5 );
	b = vshlq_n_u16( vuzp2q_u16( vreinterpretq_u16_u32(vmull_u16(vget_low_u16(b), intensityVec)), vreinterpretq_u16_u32(vmull_u16(vget_high_u16(b), intensityVec)) ), 10 );
	
	return vorrq_u16( vorrq_u16( vorrq_u16(r, g), b), a);
}

template<bool SWAP_RB>
FORCEINLINE v128u32 ColorspaceApplyIntensity32_NEON(const v128u32 &src, float intensity)
{
	v128u32 tempSrc = ColorspaceCopy32_NEON<SWAP_RB>(src);
	
	if (intensity > 0.999f)
	{
		return tempSrc;
	}
	else if (intensity < 0.001f)
	{
		return vandq_u32(tempSrc, vdupq_n_u32(0xFF000000));
	}
	
	v128u32 rb = vandq_u32(             tempSrc,      vdupq_n_u32(0x00FF00FF) );
	v128u32 g  = vandq_u32( vshrq_n_u32(tempSrc,  8), vdupq_n_u32(0x000000FF) );
	v128u32 a  = vandq_u32(             tempSrc,      vdupq_n_u32(0xFF000000) );
	
	const uint16x4_t intensityVec = vdup_n_u16( (u16)(intensity * (float)(0xFFFF)) );
	
	rb =              vuzp2q_u32( vmull_u16(vget_low_u16(vreinterpretq_u16_u32(rb)), intensityVec), vmull_u16(vget_high_u16(vreinterpretq_u16_u32(rb)), intensityVec) );
	g  = vshlq_n_u32( vuzp2q_u32( vmull_u16(vget_low_u16(vreinterpretq_u16_u32(g) ), intensityVec), vmull_u16(vget_high_u16(vreinterpretq_u16_u32(g) ), intensityVec) ),  8 );
	
	return vorrq_u32( vorrq_u32(rb, g), a);
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer555xTo8888Opaque_NEON(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec128)
{
	size_t i = 0;
	v128u16 srcVec;
	uint32x4x2_t dstVec;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		srcVec = vld1q_u16(src+i);
		ColorspaceConvert555xTo8888Opaque_NEON<SWAP_RB>(srcVec, dstVec.val[0], dstVec.val[1]);
		vst1q_u32_x2(dst+i, dstVec);
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555xTo6665Opaque_NEON(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	v128u16 srcVec;
	uint32x4x2_t dstVec;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		srcVec = vld1q_u16(src+i);
		ColorspaceConvert555xTo6665Opaque_NEON<SWAP_RB>(srcVec, dstVec.val[0], dstVec.val[1]);
		vst1q_u32_x2(dst+i, dstVec);
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
static size_t ColorspaceConvertBuffer5551To8888_NEON(const u16 *__restrict src, u32 *__restrict dst, const size_t pixCountVec128)
{
	size_t i = 0;
	v128u16 srcVec;
	uint32x4x2_t dstVec;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		srcVec = vld1q_u16(src+i);
		ColorspaceConvert5551To8888_NEON<SWAP_RB>(srcVec, dstVec.val[0], dstVec.val[1]);
		vst1q_u32_x2(dst+i, dstVec);
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer5551To6665_NEON(const u16 *__restrict src, u32 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	v128u16 srcVec;
	uint32x4x2_t dstVec;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		srcVec = vld1q_u16(src+i);
		ColorspaceConvert5551To6665_NEON<SWAP_RB>(srcVec, dstVec.val[0], dstVec.val[1]);
		vst1q_u32_x2(dst+i, dstVec);
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To6665_NEON(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
	{
		vst1q_u32( dst+i, ColorspaceConvert8888To6665_NEON<SWAP_RB>(vld1q_u32(src+i)) );
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To8888_NEON(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
	{
		vst1q_u32( dst+i, ColorspaceConvert6665To8888_NEON<SWAP_RB>(vld1q_u32(src+i)) );
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer8888To5551_NEON(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	uint32x4x2_t srcVec;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		srcVec = vld1q_u32_x2(src+i);
		vst1q_u16( dst+i, ColorspaceConvert8888To5551_NEON<SWAP_RB>(srcVec.val[0], srcVec.val[1]) );
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer6665To5551_NEON(const u32 *__restrict src, u16 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	uint32x4x2_t srcVec;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		srcVec = vld1q_u32_x2(src+i);
		vst1q_u16( dst+i, ColorspaceConvert6665To5551_NEON<SWAP_RB>(srcVec.val[0], srcVec.val[1]) );
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888xTo8888Opaque_NEON(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	size_t i = 0;
	uint8x16x4_t srcVec_x4;
	
	for (; i < pixCountVec128; i+=((sizeof(v128u32)/sizeof(u32)) * 4))
	{
		srcVec_x4 = vld4q_u8((u8 *)(src+i));
		
		if (SWAP_RB)
		{
			srcVec_x4.val[3] = srcVec_x4.val[0]; // Use the alpha channel as temp storage since we're overwriting it anyways.
			srcVec_x4.val[0] = srcVec_x4.val[2];
			srcVec_x4.val[2] = srcVec_x4.val[3];
		}
		
		srcVec_x4.val[3] = vdupq_n_u8(0xFF);
		vst4q_u8((u8 *)(dst+i), *((uint8x16x4_t *)&srcVec_x4));
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer555xTo888_NEON(const u16 *__restrict src, u8 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	uint16x8x2_t srcVec;
	uint8x16x3_t dstVec;
	uint16x8_t tempRBLo;
	uint16x8_t tempRBHi;
	
	for (; i < pixCountVec128; i+=((sizeof(v128u16)/sizeof(u16)) * 2))
	{
		srcVec = vld1q_u16_x2(src+i);
		tempRBLo = vorrq_u16( vshlq_n_u16(srcVec.val[0], 11), vshrq_n_u16(srcVec.val[0], 7) );
		tempRBHi = vorrq_u16( vshlq_n_u16(srcVec.val[1], 11), vshrq_n_u16(srcVec.val[1], 7) );
		
		if (SWAP_RB)
		{
			dstVec.val[2] = vandq_u8( vuzp1q_u8(vreinterpretq_u8_u16(tempRBLo), vreinterpretq_u8_u16(tempRBHi)), vdupq_n_u8(0xF8) );
			dstVec.val[0] = vandq_u8( vuzp2q_u8(vreinterpretq_u8_u16(tempRBLo), vreinterpretq_u8_u16(tempRBHi)), vdupq_n_u8(0xF8) );
		}
		else
		{
			dstVec.val[0] = vandq_u8( vuzp1q_u8(vreinterpretq_u8_u16(tempRBLo), vreinterpretq_u8_u16(tempRBHi)), vdupq_n_u8(0xF8) );
			dstVec.val[2] = vandq_u8( vuzp2q_u8(vreinterpretq_u8_u16(tempRBLo), vreinterpretq_u8_u16(tempRBHi)), vdupq_n_u8(0xF8) );
		}
		
		dstVec.val[1] = vandq_u8( vuzp1q_u8( vreinterpretq_u8_u16(vshrq_n_u16(srcVec.val[0], 2)), vreinterpretq_u8_u16(vshrq_n_u16(srcVec.val[1], 2)) ), vdupq_n_u8(0xF8) );
		
		dstVec.val[0] = vorrq_u8(dstVec.val[0], vshrq_n_u8(dstVec.val[0], 5));
		dstVec.val[1] = vorrq_u8(dstVec.val[1], vshrq_n_u8(dstVec.val[1], 5));
		dstVec.val[2] = vorrq_u8(dstVec.val[2], vshrq_n_u8(dstVec.val[2], 5));
		
		vst3q_u8(dst+(i*3), dstVec);
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceConvertBuffer888xTo888_NEON(const u32 *__restrict src, u8 *__restrict dst, size_t pixCountVec128)
{
	size_t i = 0;
	uint8x16x4_t srcVec_x4;
	
	for (; i < pixCountVec128; i+=((sizeof(v128u32)/sizeof(u32)) * 4))
	{
		srcVec_x4 = vld4q_u8((u8 *)(src+i));
		
		if (SWAP_RB)
		{
			srcVec_x4.val[3] = srcVec_x4.val[0]; // Use the alpha channel as temp storage since we're dropping it anyways.
			srcVec_x4.val[0] = srcVec_x4.val[2];
			srcVec_x4.val[2] = srcVec_x4.val[3];
		}
		
		vst3q_u8(dst+(i*3), *((uint8x16x3_t *)&srcVec_x4));
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer16_NEON(const u16 *src, u16 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u16));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
	{
		v128u16 src_vec128 = vld1q_u16(src+i);
		vst1q_u16(dst+i, ColorspaceCopy16_NEON<SWAP_RB>(src_vec128));
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceCopyBuffer32_NEON(const u32 *src, u32 *dst, size_t pixCountVec128)
{
	if (!SWAP_RB)
	{
		memcpy(dst, src, pixCountVec128 * sizeof(u32));
		return pixCountVec128;
	}
	
	size_t i = 0;
	
	for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
	{
		v128u32 src_vec128 = vld1q_u32(src+i);
		vst1q_u32(dst+i, ColorspaceCopy32_NEON<SWAP_RB>(src_vec128));
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer16_NEON(u16 *dst, size_t pixCountVec128, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
			{
				const v128u16 dstVec = vld1q_u16(dst+i);
				const v128u16 tempDst = COLOR16_SWAPRB_NEON(dstVec);
				vst1q_u16(dst+i, tempDst);
			}
		}
		else
		{
			return pixCountVec128;
		}
	}
	else if (intensity < 0.001f)
	{
		const uint16x8_t alphaMask = vdupq_n_u16(0x8000);
		uint16x8x4_t src;
		
		for (; i < pixCountVec128; i+=((sizeof(v128u16)/sizeof(u16))*4))
		{
			src = vld1q_u16_x4(dst+i);
			src.val[0] = vandq_u16(src.val[0], alphaMask);
			src.val[1] = vandq_u16(src.val[1], alphaMask);
			src.val[2] = vandq_u16(src.val[2], alphaMask);
			src.val[3] = vandq_u16(src.val[3], alphaMask);
			
			vst1q_u16_x4(dst+i, src);
		}
	}
	else
	{
		const uint16x4_t intensityVec = vdup_n_u16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec128; i+=(sizeof(v128u16)/sizeof(u16)))
		{
			const v128u16 dstVec = vld1q_u16(dst+i);
			v128u16 tempDst = (SWAP_RB) ? COLOR16_SWAPRB_NEON(dstVec) : dstVec;
			
			v128u16 r = vandq_u16(             tempDst,      vdupq_n_u16(0x001F) );
			v128u16 g = vandq_u16( vshrq_n_u16(tempDst,  5), vdupq_n_u16(0x001F) );
			v128u16 b = vandq_u16( vshrq_n_u16(tempDst, 10), vdupq_n_u16(0x001F) );
			v128u16 a = vandq_u16(             tempDst,      vdupq_n_u16(0x8000) );
	  
			r =              vuzp2q_u16( vreinterpretq_u16_u32(vmull_u16(vget_low_u16(r), intensityVec)), vreinterpretq_u16_u32(vmull_u16(vget_high_u16(r), intensityVec)) );
			g = vshlq_n_u16( vuzp2q_u16( vreinterpretq_u16_u32(vmull_u16(vget_low_u16(g), intensityVec)), vreinterpretq_u16_u32(vmull_u16(vget_high_u16(g), intensityVec)) ),  5 );
			b = vshlq_n_u16( vuzp2q_u16( vreinterpretq_u16_u32(vmull_u16(vget_low_u16(b), intensityVec)), vreinterpretq_u16_u32(vmull_u16(vget_high_u16(b), intensityVec)) ), 10 );
			
			tempDst = vorrq_u16( vorrq_u16( vorrq_u16(r, g), b), a);
			
			vst1q_u16(dst+i, tempDst);
		}
	}
	
	return i;
}

template <bool SWAP_RB, bool IS_UNALIGNED>
size_t ColorspaceApplyIntensityToBuffer32_NEON(u32 *dst, size_t pixCountVec128, float intensity)
{
	size_t i = 0;
	
	if (intensity > 0.999f)
	{
		if (SWAP_RB)
		{
			uint32x4x4_t src;
			
			for (; i < pixCountVec128; i+=((sizeof(v128u32)/sizeof(u32))*4))
			{
				src = vld1q_u32_x4(dst+i);
				src.val[0] = COLOR32_SWAPRB_NEON(src.val[0]);
				src.val[1] = COLOR32_SWAPRB_NEON(src.val[1]);
				src.val[2] = COLOR32_SWAPRB_NEON(src.val[2]);
				src.val[3] = COLOR32_SWAPRB_NEON(src.val[3]);
				
				vst1q_u32_x4(dst+i, src);
			}
		}
		else
		{
			return pixCountVec128;
		}
	}
	else if (intensity < 0.001f)
	{
		const uint32x4_t alphaMask = vdupq_n_u32(0xFF000000);
		uint32x4x4_t src;
		
		for (; i < pixCountVec128; i+=((sizeof(v128u32)/sizeof(u32))*4))
		{
			src = vld1q_u32_x4(dst+i);
			src.val[0] = vandq_u32(src.val[0], alphaMask);
			src.val[1] = vandq_u32(src.val[1], alphaMask);
			src.val[2] = vandq_u32(src.val[2], alphaMask);
			src.val[3] = vandq_u32(src.val[3], alphaMask);
			
			vst1q_u32_x4(dst+i, src);
		}
	}
	else
	{
		const uint16x4_t intensityVec = vdup_n_u16( (u16)(intensity * (float)(0xFFFF)) );
		
		for (; i < pixCountVec128; i+=(sizeof(v128u32)/sizeof(u32)))
		{
			v128u32 dstVec = vld1q_u32(dst+i);
			v128u32 tempDst = (SWAP_RB) ? COLOR32_SWAPRB_NEON(dstVec) : dstVec;
			
			v128u32 rb = vandq_u32(             tempDst,      vdupq_n_u32(0x00FF00FF) );
			v128u32 g  = vandq_u32( vshrq_n_u32(tempDst,  8), vdupq_n_u32(0x000000FF) );
			v128u32 a  = vandq_u32(             tempDst,      vdupq_n_u32(0xFF000000) );
			
			rb =              vuzp2q_u32( vmull_u16(vget_low_u16(vreinterpretq_u16_u32(rb)), intensityVec), vmull_u16(vget_high_u16(vreinterpretq_u16_u32(rb)), intensityVec) );
			g  = vshlq_n_u32( vuzp2q_u32( vmull_u16(vget_low_u16(vreinterpretq_u16_u32(g) ), intensityVec), vmull_u16(vget_high_u16(vreinterpretq_u16_u32(g) ), intensityVec) ),  8 );
			
			tempDst = vorrq_u32( vorrq_u32(rb, g), a);
			vst1q_u32(dst+i, tempDst);
		}
	}
	
	return i;
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo8888Opaque_NEON<false, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo8888Opaque_NEON<true, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo8888Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo8888Opaque_NEON<false, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo8888Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo8888Opaque_NEON<true, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo6665Opaque_NEON<false, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo6665Opaque_NEON<true, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo6665Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo6665Opaque_NEON<false, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer555xTo6665Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo6665Opaque_NEON<true, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To8888(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To8888_NEON<false, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To8888_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To8888_NEON<true, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To8888_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To8888_NEON<false, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To8888_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To8888_NEON<true, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To6665(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To6665_NEON<false, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To6665_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To6665_NEON<true, false>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To6665_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To6665_NEON<false, true>(src, dst, pixCount);
}

template <BESwapFlags BE_BYTESWAP>
size_t ColorspaceHandler_NEON::ConvertBuffer5551To6665_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer5551To6665_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To6665_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To6665_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To6665_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To8888_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To8888_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To8888_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer8888To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer8888To5551_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer6665To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer6665To5551_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo8888Opaque_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo8888Opaque_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo8888Opaque_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo8888Opaque_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo8888Opaque_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo8888Opaque_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer555xTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo888_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer555xTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo888_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer555xTo888_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo888_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer555xTo888_SwapRB_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer555xTo888_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo888_NEON<false, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo888_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo888_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo888_NEON<false, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ConvertBuffer888xTo888_SwapRB_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const
{
	return ColorspaceConvertBuffer888xTo888_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::CopyBuffer16_SwapRB_IsUnaligned(const u16 *src, u16 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer16_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_NEON<true, false>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::CopyBuffer32_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const
{
	return ColorspaceCopyBuffer32_NEON<true, true>(src, dst, pixCount);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer16(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_NEON<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer16_SwapRB(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_NEON<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer16_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_NEON<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer16_SwapRB_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer16_NEON<true, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer32(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_NEON<false, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer32_SwapRB(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_NEON<true, false>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer32_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_NEON<false, true>(dst, pixCount, intensity);
}

size_t ColorspaceHandler_NEON::ApplyIntensityToBuffer32_SwapRB_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const
{
	return ColorspaceApplyIntensityToBuffer32_NEON<true, true>(dst, pixCount, intensity);
}

template void ColorspaceConvert555aTo8888_NEON<true>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555aTo8888_NEON<false>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555xTo888x_NEON<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555xTo888x_NEON<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555aTo6665_NEON<true>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555aTo6665_NEON<false>(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555xTo666x_NEON<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555xTo666x_NEON<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555xTo8888Opaque_NEON<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555xTo8888Opaque_NEON<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert555xTo6665Opaque_NEON<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert555xTo6665Opaque_NEON<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert5551To8888_NEON<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert5551To8888_NEON<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template void ColorspaceConvert5551To6665_NEON<true>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template void ColorspaceConvert5551To6665_NEON<false>(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);

template v128u32 ColorspaceConvert8888To6665_NEON<true>(const v128u32 &src);
template v128u32 ColorspaceConvert8888To6665_NEON<false>(const v128u32 &src);

template v128u32 ColorspaceConvert6665To8888_NEON<true>(const v128u32 &src);
template v128u32 ColorspaceConvert6665To8888_NEON<false>(const v128u32 &src);

template v128u16 ColorspaceConvert8888To5551_NEON<true>(const v128u32 &srcLo, const v128u32 &srcHi);
template v128u16 ColorspaceConvert8888To5551_NEON<false>(const v128u32 &srcLo, const v128u32 &srcHi);

template v128u16 ColorspaceConvert6665To5551_NEON<true>(const v128u32 &srcLo, const v128u32 &srcHi);
template v128u16 ColorspaceConvert6665To5551_NEON<false>(const v128u32 &srcLo, const v128u32 &srcHi);

template v128u32 ColorspaceConvert888xTo8888Opaque_NEON<true>(const v128u32 &src);
template v128u32 ColorspaceConvert888xTo8888Opaque_NEON<false>(const v128u32 &src);

template v128u16 ColorspaceCopy16_NEON<true>(const v128u16 &src);
template v128u16 ColorspaceCopy16_NEON<false>(const v128u16 &src);

template v128u32 ColorspaceCopy32_NEON<true>(const v128u32 &src);
template v128u32 ColorspaceCopy32_NEON<false>(const v128u32 &src);

template v128u16 ColorspaceApplyIntensity16_NEON<true>(const v128u16 &src, float intensity);
template v128u16 ColorspaceApplyIntensity16_NEON<false>(const v128u16 &src, float intensity);

template v128u32 ColorspaceApplyIntensity32_NEON<true>(const v128u32 &src, float intensity);
template v128u32 ColorspaceApplyIntensity32_NEON<false>(const v128u32 &src, float intensity);

#endif // ENABLE_NEON_A64
