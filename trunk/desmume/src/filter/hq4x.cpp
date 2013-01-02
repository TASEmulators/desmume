/*
 * This file is part of the Advance project.
 *
 * Copyright (C) 2003, 2008 Andrea Mazzoleni
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 * In addition, as a special exception, Andrea Mazzoleni
 * gives permission to link the code of this program with
 * the MAME library (or with modified versions of MAME that use the
 * same license as MAME), and distribute linked combinations including
 * the two.  You must obey the GNU General Public License in all
 * respects for all of the code used other than MAME.  If you modify
 * this file, you may extend this exception to your version of the
 * file, but you are not obligated to do so.  If you do not wish to
 * do so, delete this exception statement from your version.
 */

#include "filter.h"
#include "interp.h"

/***************************************************************************/
/* HQ4x C implementation */

/*
 * This effect is a rewritten implementation of the hq4x effect made by Maxim Stepin
 */


void hq4x_32_def(u32* dst0, u32* dst1, u32* dst2, u32* dst3,
				 const u32* src0, const u32* src1, const u32* src2,
				 unsigned count, unsigned flag)
{
	unsigned i;

	for(i=0;i<count;++i) {
		unsigned char mask;

		u32 c[9];
		
		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];
		
		if (i > 0)
		{
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		}
		else
		{
			c[0] = src0[0];
			c[3] = src1[0];
			c[6] = src2[0];
		}
		
		if (i < count-1)
		{
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		}
		else
		{
			c[2] = src0[0];
			c[5] = src1[0];
			c[8] = src2[0];
		}

		mask = 0;

		if (interp_32_diff(c[0], c[4]))
			mask |= 1 << 0;
		if (interp_32_diff(c[1], c[4]))
			mask |= 1 << 1;
		if (interp_32_diff(c[2], c[4]))
			mask |= 1 << 2;
		if (interp_32_diff(c[3], c[4]))
			mask |= 1 << 3;
		if (interp_32_diff(c[5], c[4]))
			mask |= 1 << 4;
		if (interp_32_diff(c[6], c[4]))
			mask |= 1 << 5;
		if (interp_32_diff(c[7], c[4]))
			mask |= 1 << 6;
		if (interp_32_diff(c[8], c[4]))
			mask |= 1 << 7;

#define P(a, b) dst##b[a]
#define MUR interp_32_diff(c[1], c[5])
#define MDR interp_32_diff(c[5], c[7])
#define MDL interp_32_diff(c[7], c[3])
#define MUL interp_32_diff(c[3], c[1])
#define I1(p0) c[p0]
#define I2(i0, i1, p0, p1) interp_32_##i0##i1(c[p0], c[p1])
#define I3(i0, i1, i2, p0, p1, p2) interp_32_##i0##i1##i2(c[p0], c[p1], c[p2])

		switch (mask) {
		#include "hq4x.dat"
		}

#undef P
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef I1
#undef I2
#undef I3

		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 4;
		dst1 += 4;
		dst2 += 4;
		dst3 += 4;
	}
}

void hq4xS_32_def(u32* dst0, u32* dst1, u32* dst2, u32* dst3,
				  const u32* src0, const u32* src1, const u32* src2,
				  unsigned count, unsigned flag)
{
	unsigned i;
	
	for(i=0;i<count;++i) {
		unsigned char mask;
		
		u32 c[9];
		
		c[1] = src0[0];
		c[4] = src1[0];
		c[7] = src2[0];
		
		if (i > 0)
		{
			c[0] = src0[-1];
			c[3] = src1[-1];
			c[6] = src2[-1];
		}
		else
		{
			c[0] = src0[0];
			c[3] = src1[0];
			c[6] = src2[0];
		}
		
		if (i < count-1)
		{
			c[2] = src0[1];
			c[5] = src1[1];
			c[8] = src2[1];
		}
		else
		{
			c[2] = src0[0];
			c[5] = src1[0];
			c[8] = src2[0];
		}
		
		mask = 0;
		// hq4xS dynamic edge detection:
		// simply comparing the center color against its surroundings will give bad results in many cases,
		// so, instead, compare the center color relative to the max difference in brightness of this 3x3 block
		int brightArray[9];
		int maxBright = 0, minBright = 999999;
		for(int j = 0 ; j < 9 ; j++)
		{
			const int b = (int)((c[j] & 0xF8));
			const int g = (int)((c[j] & 0xF800)) >> 8;
			const int r = (int)((c[j] & 0xF80000)) >> 16;
			const int bright = r+r+r + g+g+g + b+b;
			if(bright > maxBright) maxBright = bright;
			if(bright < minBright) minBright = bright;
			
			brightArray[j] = bright;
		}
		unsigned int diffBright = ((maxBright - minBright) * 7) >> 4;
		if(diffBright > 7)
		{
			const int centerBright = brightArray[4];
			if(ABS(brightArray[0] - centerBright) > diffBright)
				mask |= 1 << 0;
			if(ABS(brightArray[1] - centerBright) > diffBright)
				mask |= 1 << 1;
			if(ABS(brightArray[2] - centerBright) > diffBright)
				mask |= 1 << 2;
			if(ABS(brightArray[3] - centerBright) > diffBright)
				mask |= 1 << 3;
			if(ABS(brightArray[5] - centerBright) > diffBright)
				mask |= 1 << 4;
			if(ABS(brightArray[6] - centerBright) > diffBright)
				mask |= 1 << 5;
			if(ABS(brightArray[7] - centerBright) > diffBright)
				mask |= 1 << 6;
			if(ABS(brightArray[8] - centerBright) > diffBright)
				mask |= 1 << 7;
		}
		
#define P(a, b) dst##b[a]
#define MUR false//(ABS(brightArray[1] - brightArray[5]) > diffBright) // top-right
#define MDR false//(ABS(brightArray[5] - brightArray[7]) > diffBright) // bottom-right
#define MDL false//(ABS(brightArray[7] - brightArray[3]) > diffBright) // bottom-left
#define MUL false//(ABS(brightArray[3] - brightArray[1]) > diffBright) // top-left
#define I1(p0) c[p0]
#define I2(i0, i1, p0, p1) interp_32_##i0##i1(c[p0], c[p1])
#define I3(i0, i1, i2, p0, p1, p2) interp_32_##i0##i1##i2(c[p0], c[p1], c[p2])
		
		switch (mask) {
#include "hq4x.dat"
		}
		
#undef P
#undef MUR
#undef MDR
#undef MDL
#undef MUL
#undef I1
#undef I2
#undef I3
		
		src0 += 1;
		src1 += 1;
		src2 += 1;
		dst0 += 4;
		dst1 += 4;
		dst2 += 4;
		dst3 += 4;
	}
}

void hq4x32(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
			u8 *dstPtr, u32 dstPitch, int width, int height)
{
	u32 *dst0 = (u32 *)dstPtr;
	u32 *dst1 = dst0 + (dstPitch >> 2);
	u32 *dst2 = dst1 + (dstPitch >> 2);
	u32 *dst3 = dst2 + (dstPitch >> 2);
	
	u32 *src0 = (u32 *)srcPtr;
	u32 *src1 = src0 + (srcPitch >> 2);
	u32 *src2 = src1 + (srcPitch >> 2);
	hq4x_32_def(dst0, dst1, dst2, dst3, src0, src0, src1, width, 0);
	
	int count = height;
	
	count -= 2;
	while(count) {
		dst0 += dstPitch;
		dst1 += dstPitch;
		dst2 += dstPitch;
		dst3 += dstPitch;
		hq4x_32_def(dst0, dst1, dst2, dst3, src0, src1, src2, width, 0);
		src0 = src1;
		src1 = src2;
		src2 += srcPitch >> 2;
		--count;
	}
	
	dst0 += dstPitch;
	dst1 += dstPitch;
	dst2 += dstPitch;
	dst3 += dstPitch;
	hq4x_32_def(dst0, dst1, dst2, dst3, src0, src1, src1, width, 0);
}

void hq4x32S(u8 *srcPtr, u32 srcPitch, u8 * /* deltaPtr */,
			 u8 *dstPtr, u32 dstPitch, int width, int height)
{
	u32 *dst0 = (u32 *)dstPtr;
	u32 *dst1 = dst0 + (dstPitch >> 2);
	u32 *dst2 = dst1 + (dstPitch >> 2);
	u32 *dst3 = dst2 + (dstPitch >> 2);
	
	u32 *src0 = (u32 *)srcPtr;
	u32 *src1 = src0 + (srcPitch >> 2);
	u32 *src2 = src1 + (srcPitch >> 2);
	hq4xS_32_def(dst0, dst1, dst2, dst3, src0, src0, src1, width, 0);
	
	int count = height;
	
	count -= 2;
	while(count) {
		dst0 += dstPitch;
		dst1 += dstPitch;
		dst2 += dstPitch;
		dst3 += dstPitch;
		hq4xS_32_def(dst0, dst1, dst2, dst3, src0, src1, src2, width, 0);
		src0 = src1;
		src1 = src2;
		src2 += srcPitch >> 2;
		--count;
	}
	
	dst0 += dstPitch;
	dst1 += dstPitch;
	dst2 += dstPitch;
	dst3 += dstPitch;
	hq4xS_32_def(dst0, dst1, dst2, dst3, src0, src1, src1, width, 0);
}

void RenderHQ4X (SSurface Src, SSurface Dst)
{
	unsigned char *lpSrc, *lpDst;
	
	lpSrc = Src.Surface;
	lpDst = Dst.Surface;
	
	hq4x32 (lpSrc, Src.Pitch*2,
			lpSrc,
			lpDst, Dst.Pitch*2 , Src.Width, Src.Height);
}

void RenderHQ4XS (SSurface Src, SSurface Dst)
{
	unsigned char *lpSrc, *lpDst;
	
	lpSrc = Src.Surface;
	lpDst = Dst.Surface;
	
	hq4x32S (lpSrc, Src.Pitch*2,
			 lpSrc,
			 lpDst, Dst.Pitch*2 , Src.Width, Src.Height);
}
