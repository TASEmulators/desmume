/*  Copyright (C) 2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "filter.h"
#include "types.h"
#include "interp.h"

// transforms each 1 pixel into a 2x2 block of output pixels
// where each corner is selected based on equivalence of neighboring pixels
void RenderEPX (SSurface Src, SSurface Dst)
{
	uint32 *lpSrc;

	const uint32 srcHeight = Src.Height;
	const uint32 srcWidth = Src.Width;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<uint32 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	uint32 *lpDst = (uint32*)Dst.Surface;

	for(int j = 0; j < srcHeight; j++)
	{
		uint32* SrcLine = lpSrc + srcPitch*j;
		uint32* DstLine1 = lpDst + dstPitch*(j*2);
		uint32* DstLine2 = lpDst + dstPitch*(j*2+1);
		for(int i = 0; i < srcWidth; i++)
		{
			uint32 L = *(SrcLine-1);
			uint32 C = *(SrcLine);
			uint32 R = *(SrcLine+1);
			if(L != R)
			{
				uint32 U = *(SrcLine-srcPitch);
				uint32 D = *(SrcLine+srcPitch);
				if(U != D)
				{
					*DstLine1++ = (U == L) ? U : C;
					*DstLine1++ = (R == U) ? R : C;
					*DstLine2++ = (L == D) ? L : C;
					*DstLine2++ = (D == R) ? D : C;
					SrcLine++;
					continue;
				}
			}
			*DstLine1++ = C; 
			*DstLine1++ = C; 
			*DstLine2++ = C; 
			*DstLine2++ = C; 
			SrcLine++;
		}
	}
}

// transforms each 2x2 block of pixels into a 3x3 block of output pixels
// where each pixel is selected based on equivalence of neighboring pixels
void RenderEPX_1Point5x (SSurface Src, SSurface Dst)
{
	u32 *lpSrc;

	u32 srcHeight = Src.Height;
	u32 srcWidth = Src.Width;
	u32 dstHeight = Dst.Height;
	u32 dstWidth = Dst.Width;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<u32 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	u32 *lpDst = (u32*)Dst.Surface;

	for(int yi=0, yo=0; yi < srcHeight; yi+=2, yo+=3)
	{
		u32* SrcLine = lpSrc + srcPitch*yi;
		u32* DstLine1 = lpDst + dstPitch*(yo);
		u32* DstLine2 = lpDst + dstPitch*(yo+1);
		u32* DstLine3 = lpDst + dstPitch*(yo+2);
		for(int xi=0; xi < srcWidth; xi+=2)
		{
			u32                                s10 = *(SrcLine-srcPitch),   s20 = *(SrcLine-srcPitch+1),   s30 = *(SrcLine-srcPitch+2);
			u32 s01 = *(SrcLine-1),            s11 = *(SrcLine),            s21 = *(SrcLine+1),            s31 = *(SrcLine+2);
			u32 s02 = *(SrcLine+srcPitch-1),   s12 = *(SrcLine+srcPitch),   s22 = *(SrcLine+srcPitch+1),   s32 = *(SrcLine+srcPitch+2);
			u32 s03 = *(SrcLine+2*srcPitch-1), s13 = *(SrcLine+2*srcPitch), s23 = *(SrcLine+2*srcPitch+1), s33 = *(SrcLine+2*srcPitch+2);
			*DstLine1++ =  s01==s10 && s10!=s21 && s01!=s12
			                                                             ? s01:s11;
			*DstLine1++ =  s10==s21 && s10!=s01 && s21!=s12
			                                                             ? s21:s11;
			*DstLine1++ = (s11==s20 && s20!=s31 && s11!=s22 && s21!=s30)
			           || (s20==s31 && s20!=s11 && s31!=s22 && s21!=s10) ? s20:s21;
			*DstLine2++ =  s01==s12 && s01!=s10 && s12!=s21
			                                                             ? s01:s11;
			*DstLine2++ =  s12==s21 && s01!=s12 && s10!=s21
			                                                             ? s21:s11;
			*DstLine2++ = (s11==s22 && s11!=s20 && s22!=s31 && s21!=s32)
			           || (s22==s31 && s11!=s22 && s20!=s31 && s21!=s12) ? s22:s21;
			*DstLine3++ = (s02==s11 && s11!=s22 && s02!=s13 && s12!=s03)
			           || (s02==s13 && s02!=s11 && s13!=s22 && s12!=s01) ? s02:s12;
			*DstLine3++ = (s11==s22 && s11!=s02 && s22!=s13 && s12!=s23)
			           || (s13==s22 && s02!=s13 && s11!=s22 && s12!=s21) ? s22:s12;
		    *DstLine3++ = s22;
			SrcLine+=2;
		}
	}
}

static u32 min(u32 a, u32 b) { return (a < b) ? a : b; }
static u32 min3(u32 a, u32 b, u32 c) { return min(a,min(b,c)); }
static u32 dist(u32 a, u32 b)
{
	return ABS( (a & 0x0000FF)      -  (b & 0x0000FF))*2
	     + ABS(((a & 0x00FF00)>>8)  - ((b & 0x00FF00)>>8))*3
	     + ABS(((a & 0xFF0000)>>16) - ((b & 0xFF0000)>>16))*3;
}
// note: we only use mix to make the arbitrary choice between two almost-equal colors.
// this filter doesn't really do much interpolating or have the appearance of doing any.
#define mix interp_32_11

// transforms each 1 pixel into a 2x2 block of output pixels
// where each corner is selected based on relative equivalence of neighboring pixels
void RenderEPXPlus (SSurface Src, SSurface Dst)
{
	uint32 *lpSrc;

	const uint32 srcHeight = Src.Height;
	const uint32 srcWidth = Src.Width;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<uint32 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	uint32 *lpDst = (uint32*)Dst.Surface;

	for(int j = 0; j < srcHeight; j++)
	{
		uint32* SrcLine = lpSrc + srcPitch*j;
		uint32* DstLine1 = lpDst + dstPitch*(j*2);
		uint32* DstLine2 = lpDst + dstPitch*(j*2+1);
		for(int i = 0; i < srcWidth; i++)
		{
			uint32 L = *(SrcLine-1);
			uint32 C = *(SrcLine);
			uint32 R = *(SrcLine+1);
			uint32 U = *(SrcLine-srcPitch);
			uint32 D = *(SrcLine+srcPitch);
			*DstLine1++ = dist(L,U) < min(dist(L,D),dist(R,U)) ? mix(L,U) : C;
			*DstLine1++ = dist(R,U) < min(dist(L,U),dist(R,D)) ? mix(R,U) : C;
			*DstLine2++ = dist(L,D) < min(dist(L,U),dist(R,D)) ? mix(L,D) : C;
			*DstLine2++ = dist(R,D) < min(dist(L,D),dist(R,U)) ? mix(R,D) : C;
			SrcLine++;
		}
	}
}

// transforms each 2x2 block of pixels into a 3x3 block of output pixels
// where each pixel is selected based on relative equivalence of neighboring pixels
void RenderEPXPlus_1Point5x (SSurface Src, SSurface Dst)
{
	u32 *lpSrc;

	u32 srcHeight = Src.Height;
	u32 srcWidth = Src.Width;
	u32 dstHeight = Dst.Height;
	u32 dstWidth = Dst.Width;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<u32 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	u32 *lpDst = (u32*)Dst.Surface;

	for(int yi=0, yo=0; yi < srcHeight; yi+=2, yo+=3)
	{
		u32* SrcLine = lpSrc + srcPitch*yi;
		u32* DstLine1 = lpDst + dstPitch*(yo);
		u32* DstLine2 = lpDst + dstPitch*(yo+1);
		u32* DstLine3 = lpDst + dstPitch*(yo+2);
		for(int xi=0; xi < srcWidth; xi+=2)
		{
			u32                                s10 = *(SrcLine-srcPitch),   s20 = *(SrcLine-srcPitch+1),   s30 = *(SrcLine-srcPitch+2);
			u32 s01 = *(SrcLine-1),            s11 = *(SrcLine),            s21 = *(SrcLine+1),            s31 = *(SrcLine+2);
			u32 s02 = *(SrcLine+srcPitch-1),   s12 = *(SrcLine+srcPitch),   s22 = *(SrcLine+srcPitch+1),   s32 = *(SrcLine+srcPitch+2);
			u32 s03 = *(SrcLine+2*srcPitch-1), s13 = *(SrcLine+2*srcPitch), s23 = *(SrcLine+2*srcPitch+1), s33 = *(SrcLine+2*srcPitch+2);
			*DstLine1++ =  dist(s01,s10) < min( dist(s10,s21),dist(s01,s12))
			                                                                                ? mix(s01,s10):s11;
			*DstLine1++ =  dist(s10,s21) < min( dist(s10,s01),dist(s21,s12))
			                                                                                ? mix(s10,s21):s11;
			*DstLine1++ =  dist(s11,s20) < min3(dist(s20,s31),dist(s11,s22),dist(s21,s30))  ? mix(s11,s20):
			               dist(s20,s31) < min3(dist(s20,s11),dist(s31,s22),dist(s21,s10))  ? mix(s20,s31):s21;
			*DstLine2++ =  dist(s01,s12) < min( dist(s01,s10),dist(s12,s21))
			                                                                                ? mix(s01,s12):s11;
			*DstLine2++ =  dist(s12,s21) < min( dist(s01,s12),dist(s10,s21))
			                                                                                ? mix(s12,s21):s11;
			*DstLine2++ =  dist(s11,s22) < min3(dist(s11,s20),dist(s22,s31),dist(s21,s32))  ? mix(s11,s22):
			               dist(s22,s31) < min3(dist(s11,s22),dist(s20,s31),dist(s21,s12))  ? mix(s22,s31):s21;
			*DstLine3++ =  dist(s02,s11) < min3(dist(s11,s22),dist(s02,s13),dist(s12,s03))  ? mix(s02,s11):
			               dist(s02,s13) < min3(dist(s02,s11),dist(s13,s22),dist(s12,s01))  ? mix(s02,s13):s12;
			*DstLine3++ =  dist(s11,s22) < min3(dist(s11,s02),dist(s22,s13),dist(s12,s23))  ? mix(s11,s22):
			               dist(s13,s22) < min3(dist(s02,s13),dist(s11,s22),dist(s12,s21))  ? mix(s13,s22):s12;
		    *DstLine3++ = s22;
			SrcLine+=2;
		}
	}
}



// transforms each 2x2 block of pixels into 3x3 output which is
// a 2x2 block that has 1 block of padding on the right and bottom sides
// which are selected stupidly from neighboring pixels in the original 2x2 block
void RenderNearest_1Point5x (SSurface Src, SSurface Dst)
{
	uint32 *lpSrc;

	uint32 srcHeight = Src.Height;
	uint32 srcWidth = Src.Width;
	uint32 dstHeight = Dst.Height;
	uint32 dstWidth = Dst.Width;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<uint32 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	uint32 *lpDst = (uint32*)Dst.Surface;

	for(int yi = 0, yo = 0; yi < srcHeight; yi+=2, yo+=3)
	{
		u32* srcPix1 = lpSrc + srcPitch*(yi);
		u32* srcPix2 = lpSrc + srcPitch*(yi+1);
		u32* dstPix1 = lpDst + dstPitch*(yo);
		u32* dstPix2 = lpDst + dstPitch*(yo+1);
		u32* dstPix3 = lpDst + dstPitch*(yo+2);

		for(int xi = 0; xi < srcWidth; xi+=2)
		{
			*dstPix1++ = *srcPix1++;
			*dstPix1++ = *srcPix1;
			*dstPix1++ = *srcPix1++;
			*dstPix2++ = *srcPix2;
			*dstPix3++ = *srcPix2++;
			*dstPix2++ = *srcPix2;
			*dstPix3++ = *srcPix2;
			*dstPix2++ = *srcPix2;
			*dstPix3++ = *srcPix2++;
		}
	}
}

// transforms each 2x2 block of pixels into 3x3 output which is
// a 2x2 block that has 1 block of padding on the right and bottom sides
// which are selected from neighboring pixels depending on matching diagonals
void RenderNearestPlus_1Point5x (SSurface Src, SSurface Dst)
{
	uint32 *lpSrc;

	uint32 srcHeight = Src.Height;
	uint32 srcWidth = Src.Width;
	uint32 dstHeight = Dst.Height;
	uint32 dstWidth = Dst.Width;

	const unsigned int srcPitch = Src.Pitch >> 1;
	lpSrc = reinterpret_cast<uint32 *>(Src.Surface);

	const unsigned int dstPitch = Dst.Pitch >> 1;
	uint32 *lpDst = (uint32*)Dst.Surface;

	for(int j = 0, y = 0; j < srcHeight; j+=2, y+=3)
	{
		u32* srcPix = lpSrc + srcPitch*j;
		u32* dstPix = lpDst + dstPitch*y;

#define GET(dx,dy) *(srcPix+(dy)*srcPitch+(dx))
#define SET(dx,dy,val) *(dstPix+(dy)*dstPitch+(dx)) = (val)
#define BETTER(dx,dy,dx2,dy2) (GET(dx,dy) == GET(dx2,dy2) && GET(dx2,dy) != GET(dx,dy2))

		for(int i = 0, x = 0; i < srcWidth; i+=2, x+=3, srcPix+=2, dstPix+=3)
		{
			SET(0,0,GET(0,0));
			SET(1,0,GET(1,0));
			SET(2,0,GET(BETTER(2,0,1,-1)? 2:1,0));
			SET(0,1,GET(0,1));
			SET(1,1,GET(1,1));
			SET(2,1,GET(BETTER(1,0, 2,1)? 2:1,1));
			SET(0,2,GET(BETTER(0,2,-1,1)?-1:0,1));
			SET(1,2,GET(BETTER(0,1, 1,2)? 0:1,1));
			SET(2,2,GET(BETTER(2,1, 1,2)? 2:1,1));
		}

#undef GET
#undef SET
#undef BETTER
	}
}
