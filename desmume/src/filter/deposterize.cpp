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

#include "../types.h"
#include "filter.h"

#define DEPOSTERIZE_THRESHOLD 23	// Possible values are [0-255], where lower a value prevents blending and a higher value allows for more blending


namespace
{
	template <u32 DEN>
	struct UnpackedPixel
	{
		u32 r;
		u32 g;
		u32 b;
		u32 a;
		
		u32 pack() const
		{
			return ( ((r/DEN) <<  0) |
			         ((g/DEN) <<  8) |
			         ((b/DEN) << 16) |
			         ((a/DEN) << 24) );
		}
	};
	
	static FORCEINLINE UnpackedPixel<2> Deposterize_InterpLTE(const UnpackedPixel<1> &pixA, const UnpackedPixel<1> &pixB)
	{
		UnpackedPixel<2> pixOut = {
			pixA.r,
			pixA.g,
			pixA.b,
			pixA.a
		};
		
		if (pixB.a == 0)
		{
			pixOut.r = pixOut.r << 1;
			pixOut.g = pixOut.g << 1;
			pixOut.b = pixOut.b << 1;
			pixOut.a = pixOut.a << 1;
			return pixOut;
		}
		
		const s32 rDiff = pixA.r - pixB.r;
		const s32 gDiff = pixA.g - pixB.g;
		const s32 bDiff = pixA.b - pixB.b;
		const s32 aDiff = pixA.a - pixB.a;
		
		pixOut.r = ( (-DEPOSTERIZE_THRESHOLD <= rDiff) && (rDiff <= DEPOSTERIZE_THRESHOLD) ) ? (pixOut.r + pixB.r) : (pixOut.r << 1);
		pixOut.g = ( (-DEPOSTERIZE_THRESHOLD <= gDiff) && (gDiff <= DEPOSTERIZE_THRESHOLD) ) ? (pixOut.g + pixB.g) : (pixOut.g << 1);
		pixOut.b = ( (-DEPOSTERIZE_THRESHOLD <= bDiff) && (bDiff <= DEPOSTERIZE_THRESHOLD) ) ? (pixOut.b + pixB.b) : (pixOut.b << 1);
		pixOut.a = ( (-DEPOSTERIZE_THRESHOLD <= aDiff) && (aDiff <= DEPOSTERIZE_THRESHOLD) ) ? (pixOut.a + pixB.a) : (pixOut.a << 1);
		
		return pixOut;
	}
	
	static FORCEINLINE UnpackedPixel<2> Deposterize_InterpLTE(const UnpackedPixel<1> &pixA, const u32 color32B)
	{
		const UnpackedPixel<1> pixB = {
			(color32B >>  0) & 0x000000FF,
			(color32B >>  8) & 0x000000FF,
			(color32B >> 16) & 0x000000FF,
			(color32B >> 24) & 0x000000FF
		};
		
		return Deposterize_InterpLTE(pixA, pixB);
	}
	
	template <u32 WEIGHTA, u32 WEIGHTB, u32 DEN>
	static FORCEINLINE UnpackedPixel<DEN*(WEIGHTA+WEIGHTB)> Deposterize_Blend(const UnpackedPixel<DEN> &pixA, const UnpackedPixel<DEN> &pixB)
	{
		UnpackedPixel<DEN*(WEIGHTA+WEIGHTB)> ret;
		ret.r = (pixA.r * WEIGHTA) + (pixB.r * WEIGHTB);
		ret.g = (pixA.g * WEIGHTA) + (pixB.g * WEIGHTB);
		ret.b = (pixA.b * WEIGHTA) + (pixB.b * WEIGHTB);
		ret.a = (pixA.a * WEIGHTA) + (pixB.a * WEIGHTB);
		
		return ret;
	}
	
	static u32 Deposterize_BlendPixel(const u32 color32[9])
	{
		const UnpackedPixel<1> center = {
			(color32[0] >>  0) & 0x000000FF,
			(color32[0] >>  8) & 0x000000FF,
			(color32[0] >> 16) & 0x000000FF,
			(color32[0] >> 24) & 0x000000FF
		};
		
		const UnpackedPixel<2> center2 = {
			center.r << 1,
			center.g << 1,
			center.b << 1,
			center.a << 1
		};
		
#define DF_INTERP(i) Deposterize_InterpLTE(center, color32[i])
		
		UnpackedPixel<512> pixOut = Deposterize_Blend<3, 1>(
			Deposterize_Blend<1, 1>(
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<2, 14>(center2, DF_INTERP(5)),
					Deposterize_Blend<2, 14>(center2, DF_INTERP(1))
				),
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<2, 14>(center2, DF_INTERP(7)),
					Deposterize_Blend<2, 14>(center2, DF_INTERP(3))
				)
			),
			Deposterize_Blend<1, 1>(
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<7, 9>(center2, DF_INTERP(6)),
					Deposterize_Blend<7, 9>(center2, DF_INTERP(2))
				),
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<7, 9>(center2, DF_INTERP(8)),
					Deposterize_Blend<7, 9>(center2, DF_INTERP(4))
				)
			)
		);
		
#undef DF_INTERP
		
		return pixOut.pack();
	}
}

void RenderDeposterize(SSurface Src, SSurface Dst)
{
	//---------------------------------------\n\
	// Input Pixel Mapping:  06|07|08
	//                       05|00|01
	//                       04|03|02
	//
	// Output Pixel Mapping:    00
	
	const size_t w = Src.Width;
	const size_t h = Src.Height;
	
	u32 color[9];
	u32 *src = (u32 *)Src.Surface;
	u32 *workingDst = (u32 *)Dst.workingSurface[0];
	u32 *finalDst = (u32 *)Dst.Surface;
	
	size_t i = 0;
	
	for (size_t x = 0; x < w; x++, i++)
	{
		if ((src[i] & 0xFF000000) == 0)
		{
			workingDst[i] = src[i];
			continue;
		}
		
		color[0] =                             src[i];
		color[1] = (x < w-1)    ? src[i+1]   : src[i];
		color[2] = (x < w-1)    ? src[i+w+1] : src[i];
		color[3] =                             src[i];
		color[4] = (x > 0)      ? src[i+w-1] : src[i];
		color[5] = (x > 0)      ? src[i-1]   : src[i];
		color[6] =                             src[i];
		color[7] =                             src[i];
		color[8] =                             src[i];
		
		workingDst[i] = Deposterize_BlendPixel(color);
	}
	
	for (size_t y = 1; y < h-1; y++)
	{
		for (size_t x = 0; x < w; x++, i++)
		{
			if ((src[i] & 0xFF000000) == 0)
			{
				workingDst[i] = src[i];
				continue;
			}
			
			color[0] =                             src[i];
			color[1] = (x < w-1)    ? src[i+1]   : src[i];
			color[2] = (x < w-1)    ? src[i+w+1] : src[i];
			color[3] =                src[i+w];
			color[4] = (x > 0)      ? src[i+w-1] : src[i];
			color[5] = (x > 0)      ? src[i-1]   : src[i];
			color[6] = (x > 0)      ? src[i-w-1] : src[i];
			color[7] =                src[i-w];
			color[8] = (x < w-1)    ? src[i-w+1] : src[i];
			
			workingDst[i] = Deposterize_BlendPixel(color);
		}
	}
	
	for (size_t x = 0; x < w; x++, i++)
	{
		if ((src[i] & 0xFF000000) == 0)
		{
			workingDst[i] = src[i];
			continue;
		}
		
		color[0] =                             src[i];
		color[1] = (x < w-1)    ? src[i+1]   : src[i];
		color[2] =                             src[i];
		color[3] =                             src[i];
		color[4] =                             src[i];
		color[5] = (x > 0)      ? src[i-1]   : src[i];
		color[6] = (x > 0)      ? src[i-w-1] : src[i];
		color[7] =                             src[i];
		color[8] = (x < w-1)    ? src[i-w+1] : src[i];
		
		workingDst[i] = Deposterize_BlendPixel(color);
	}
	
	i = 0;
	
	for (size_t x = 0; x < w; x++, i++)
	{
		if ((src[i] & 0xFF000000) == 0)
		{
			finalDst[i] = src[i];
			continue;
		}
		
		color[0] =                                    workingDst[i];
		color[1] = (x < w-1)    ? workingDst[i+1]   : workingDst[i];
		color[2] = (x < w-1)    ? workingDst[i+w+1] : workingDst[i];
		color[3] =                                    workingDst[i];
		color[4] = (x > 0)      ? workingDst[i+w-1] : workingDst[i];
		color[5] = (x > 0)      ? workingDst[i-1]   : workingDst[i];
		color[6] =                                    workingDst[i];
		color[7] =                                    workingDst[i];
		color[8] =                                    workingDst[i];
		
		finalDst[i] = Deposterize_BlendPixel(color);
	}
	
	for (size_t y = 1; y < h-1; y++)
	{
		for (size_t x = 0; x < w; x++, i++)
		{
			if ((src[i] & 0xFF000000) == 0)
			{
				finalDst[i] = src[i];
				continue;
			}
			
			color[0] =                                    workingDst[i];
			color[1] = (x < w-1)    ? workingDst[i+1]   : workingDst[i];
			color[2] = (x < w-1)    ? workingDst[i+w+1] : workingDst[i];
			color[3] =                workingDst[i+w];
			color[4] = (x > 0)      ? workingDst[i+w-1] : workingDst[i];
			color[5] = (x > 0)      ? workingDst[i-1]   : workingDst[i];
			color[6] = (x > 0)      ? workingDst[i-w-1] : workingDst[i];
			color[7] =                workingDst[i-w];
			color[8] = (x < w-1)    ? workingDst[i-w+1] : workingDst[i];
			
			finalDst[i] = Deposterize_BlendPixel(color);
		}
	}
	
	for (size_t x = 0; x < w; x++, i++)
	{
		if ((src[i] & 0xFF000000) == 0)
		{
			finalDst[i] = src[i];
			continue;
		}
		
		color[0] =                                    workingDst[i];
		color[1] = (x < w-1)    ? workingDst[i+1]   : workingDst[i];
		color[2] =                                    workingDst[i];
		color[3] =                                    workingDst[i];
		color[4] =                                    workingDst[i];
		color[5] = (x > 0)      ? workingDst[i-1]   : workingDst[i];
		color[6] = (x > 0)      ? workingDst[i-w-1] : workingDst[i];
		color[7] =                                    workingDst[i];
		color[8] = (x < w-1)    ? workingDst[i-w+1] : workingDst[i];
		
		finalDst[i] = Deposterize_BlendPixel(color);
	}
}
