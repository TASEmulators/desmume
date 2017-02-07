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


static u32 Deposterize_InterpLTE(const u32 pixA, const u32 pixB)
{
	const u32 aB = (pixB & 0xFF000000) >> 24;
	if (aB == 0)
	{
		return pixA;
	}
	
	const u32 rA = (pixA & 0x000000FF);
	const u32 gA = (pixA & 0x0000FF00) >> 8;
	const u32 bA = (pixA & 0x00FF0000) >> 16;
	const u32 aA = (pixA & 0xFF000000) >> 24;
	
	const u32 rB = (pixB & 0x000000FF);
	const u32 gB = (pixB & 0x0000FF00) >> 8;
	const u32 bB = (pixB & 0x00FF0000) >> 16;
	
	const u32 rC = ( (rB - rA <= DEPOSTERIZE_THRESHOLD) || (rA - rB <= DEPOSTERIZE_THRESHOLD) ) ? ((rA+rB)>>1) : rA;
	const u32 gC = ( (gB - gA <= DEPOSTERIZE_THRESHOLD) || (gA - gB <= DEPOSTERIZE_THRESHOLD) ) ? ((gA+gB)>>1) : gA;
	const u32 bC = ( (bB - bA <= DEPOSTERIZE_THRESHOLD) || (bA - bB <= DEPOSTERIZE_THRESHOLD) ) ? ((bA+bB)>>1) : bA;
	const u32 aC = ( (aB - aA <= DEPOSTERIZE_THRESHOLD) || (aA - aB <= DEPOSTERIZE_THRESHOLD) ) ? ((aA+aB)>>1) : aA;
		
	return (rC | (gC << 8) | (bC << 16) | (aC << 24));
}

static u32 Deposterize_Blend(const u32 pixA, const u32 pixB, const u32 weightA, const u32 weightB)
{
	const u32  aB = (pixB & 0xFF000000) >> 24;
	if (aB == 0)
	{
		return pixA;
	}
	
	const u32 rbA =  pixA & 0x00FF00FF;
	const u32  gA =  pixA & 0x0000FF00;
	const u32  aA = (pixA & 0xFF000000) >> 24;
	
	const u32 rbB =  pixB & 0x00FF00FF;
	const u32  gB =  pixB & 0x0000FF00;
	
	// Note: The sum of weightA and weightB must equal 16.
	const u32 rbC = ( ((rbA * weightA) + (rbB * weightB)) / 16 ) & 0x00FF00FF;
	const u32  gC = ( (( gA * weightA) + ( gB * weightB)) / 16 ) & 0x0000FF00;
	const u32  aC = ( (( aA * weightA) + ( aB * weightB)) / 16 ) << 24;
	
	return (rbC | gC | aC);
}

static u32 Deposterize_BlendPixel(const u32 color[9])
{
	const u32 blend[9] = {
		color[0],
		Deposterize_InterpLTE(color[0], color[1]),
		Deposterize_InterpLTE(color[0], color[2]),
		Deposterize_InterpLTE(color[0], color[3]),
		Deposterize_InterpLTE(color[0], color[4]),
		Deposterize_InterpLTE(color[0], color[5]),
		Deposterize_InterpLTE(color[0], color[6]),
		Deposterize_InterpLTE(color[0], color[7]),
		Deposterize_InterpLTE(color[0], color[8])
	};
	
	return Deposterize_Blend(Deposterize_Blend(Deposterize_Blend(Deposterize_Blend(blend[0], blend[5], 2, 14),
																 Deposterize_Blend(blend[0], blend[1], 2, 14),
																 8, 8),
											   Deposterize_Blend(Deposterize_Blend(blend[0], blend[7], 2, 14),
																 Deposterize_Blend(blend[0], blend[3], 2, 14),
																 8, 8),
											   8, 8),
							 Deposterize_Blend(Deposterize_Blend(Deposterize_Blend(blend[0], blend[6], 7, 9),
																 Deposterize_Blend(blend[0], blend[2], 7, 9),
																 8, 8),
											   Deposterize_Blend(Deposterize_Blend(blend[0], blend[8], 7, 9),
																 Deposterize_Blend(blend[0], blend[4], 7, 9),
																 8, 8),
											   8, 8),
							 12, 4);
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
