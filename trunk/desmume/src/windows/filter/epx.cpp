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

void RenderEPX_1Point5x (SSurface Src, SSurface Dst)
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

