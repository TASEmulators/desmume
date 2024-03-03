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

#include "../types.h"
#include "filter.h"

#define DEPOSTERIZE_THRESHOLD 23	// Possible values are [0-255], where lower a value prevents blending and a higher value allows for more blending

namespace
{
	template <u32 Den>
	struct UnpackedPixel
	{
		u32 r;
		u32 g;
		u32 b;
		u32 a;

		explicit UnpackedPixel() = default;
		UnpackedPixel(const UnpackedPixel &) = default;
		UnpackedPixel &operator=(const UnpackedPixel &) & = default;

		explicit UnpackedPixel(u32 pix) :
			r(Den * ( pix        & 0xff)),
			g(Den * ((pix >>  8) & 0xff)),
			b(Den * ((pix >> 16) & 0xff)),
			a(Den * ((pix >> 24) & 0xff))
		{}

		u32 pack() const
		{
			return r/Den +
				(g/Den << 8) +
				(b/Den << 16) +
				(a/Den << 24);
		}
	};

	template <u32 Mult, u32 Den, class Func>
	inline UnpackedPixel<Den*Mult> combine(const UnpackedPixel<Den> &pixA, const UnpackedPixel<Den> &pixB, Func &&func)
	{
		UnpackedPixel<Den*Mult> ret;
		ret.r = func(pixA.r, pixB.r);
		ret.g = func(pixA.g, pixB.g);
		ret.b = func(pixA.b, pixB.b);
		ret.a = func(pixA.a, pixB.a);
		return ret;
	}

	inline UnpackedPixel<2> Deposterize_InterpLTE(const UnpackedPixel<1> &pixA, const UnpackedPixel<1> &pixB)
	{
		return combine<2>(pixA, pixB, [&](u32 a, u32 b) {
			if (pixB.a == 0)
				return 2 * a;
			int diff = a - b;
			if (-DEPOSTERIZE_THRESHOLD <= diff && diff <= DEPOSTERIZE_THRESHOLD)
				return a + b;
			return 2 * a;
		});
	}

	template <u32 WeightA, u32 WeightB, u32 Den>
	inline UnpackedPixel<Den*(WeightA+WeightB)> Deposterize_Blend(const UnpackedPixel<Den> &pixA, const UnpackedPixel<Den> &pixB)
	{
		return combine<WeightA+WeightB>(pixA, pixB, [](u32 a, u32 b) {
			return a * WeightA + b * WeightB;
		});
	}

	u32 Deposterize_BlendPixel(const u32 color[9])
	{
		UnpackedPixel<1> center(color[0]);
		UnpackedPixel<2> center2 = Deposterize_Blend<1,1>(center, center);

		auto interp = [&](int i) {
			return Deposterize_InterpLTE(center, UnpackedPixel<1>(color[i]));
		};

		auto result = Deposterize_Blend<3, 1>(
			Deposterize_Blend<1, 1>(
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<2, 14>(center2, interp(5)),
					Deposterize_Blend<2, 14>(center2, interp(1))
				),
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<2, 14>(center2, interp(7)),
					Deposterize_Blend<2, 14>(center2, interp(3))
				)
			),
			Deposterize_Blend<1, 1>(
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<7, 9>(center2, interp(6)),
					Deposterize_Blend<7, 9>(center2, interp(2))
				),
				Deposterize_Blend<1, 1>(
					Deposterize_Blend<7, 9>(center2, interp(8)),
					Deposterize_Blend<7, 9>(center2, interp(4))
				)
			)
		);
		return result.pack();
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
		
		color[0] =				               src[i];
		color[1] = (x < w-1)	? src[i+1]   : src[i];
		color[2] = (x < w-1)	? src[i+w+1] : src[i];
		color[3] =				               src[i];
		color[4] = (x > 0)		? src[i+w-1] : src[i];
		color[5] = (x > 0)		? src[i-1]   : src[i];
		color[6] =				               src[i];
		color[7] =				               src[i];
		color[8] =				               src[i];
		
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
			
			color[0] =				               src[i];
			color[1] = (x < w-1)	? src[i+1]   : src[i];
			color[2] = (x < w-1)	? src[i+w+1] : src[i];
			color[3] =				  src[i+w];
			color[4] = (x > 0)		? src[i+w-1] : src[i];
			color[5] = (x > 0)		? src[i-1]   : src[i];
			color[6] = (x > 0)		? src[i-w-1] : src[i];
			color[7] =				  src[i-w];
			color[8] = (x < w-1)	? src[i-w+1] : src[i];
			
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
		
		color[0] =				               src[i];
		color[1] = (x < w-1)	? src[i+1]   : src[i];
		color[2] =				               src[i];
		color[3] =				               src[i];
		color[4] =				               src[i];
		color[5] = (x > 0)		? src[i-1]   : src[i];
		color[6] = (x > 0)		? src[i-w-1] : src[i];
		color[7] =				               src[i];
		color[8] = (x < w-1)	? src[i-w+1] : src[i];
		
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
		
		color[0] =				                      workingDst[i];
		color[1] = (x < w-1)	? workingDst[i+1]   : workingDst[i];
		color[2] = (x < w-1)	? workingDst[i+w+1] : workingDst[i];
		color[3] =				                      workingDst[i];
		color[4] = (x > 0)		? workingDst[i+w-1] : workingDst[i];
		color[5] = (x > 0)		? workingDst[i-1]   : workingDst[i];
		color[6] =				                      workingDst[i];
		color[7] =				                      workingDst[i];
		color[8] =				                      workingDst[i];
		
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
			
			color[0] =				                      workingDst[i];
			color[1] = (x < w-1)	? workingDst[i+1]   : workingDst[i];
			color[2] = (x < w-1)	? workingDst[i+w+1] : workingDst[i];
			color[3] =				  workingDst[i+w];
			color[4] = (x > 0)		? workingDst[i+w-1] : workingDst[i];
			color[5] = (x > 0)		? workingDst[i-1]   : workingDst[i];
			color[6] = (x > 0)		? workingDst[i-w-1] : workingDst[i];
			color[7] =				  workingDst[i-w];
			color[8] = (x < w-1)	? workingDst[i-w+1] : workingDst[i];
			
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
		
		color[0] =				                      workingDst[i];
		color[1] = (x < w-1)	? workingDst[i+1]   : workingDst[i];
		color[2] =				                      workingDst[i];
		color[3] =				                      workingDst[i];
		color[4] =				                      workingDst[i];
		color[5] = (x > 0)		? workingDst[i-1]   : workingDst[i];
		color[6] = (x > 0)		? workingDst[i-w-1] : workingDst[i];
		color[7] =				                      workingDst[i];
		color[8] = (x < w-1)	? workingDst[i-w+1] : workingDst[i];
		
		finalDst[i] = Deposterize_BlendPixel(color);
	}
}
