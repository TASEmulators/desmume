/*
	Copyright (C) 2026 DeSmuME Team

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

// xBRZ 1.9 for DeSmuME.
//
// The scaler lives in filter/xbrz19/ and is the VBA-M rework of upstream xBRZ:
// SSE2/NEON/scalar vectorization, colour distances computed on the fly instead
// of the 64 MB table that 1.4 built on first use, and the 6x/9x rules
// reinstated. This file is the only translation unit that is compiled; every
// build file in the tree already lists it, so the core is pulled in here rather
// than being added to each of them.
//
// Do not add filter/xbrz19/xbrz.cpp to a project: it would duplicate every
// symbol in this one.

#include "filter.h"
#include "xbrz.h"

#include "xbrz19/xbrz.cpp"


namespace xbrz
{

bool equalColorTest(uint32_t col1, uint32_t col2, ColorFormat colFmt, double luminanceWeight, double equalColorTolerance)
{
	// The 1.9 core folds the luminance weight into its fixed BT.2020 distance,
	// so only the tolerance is forwarded.
	(void)luminanceWeight;
	return xbrz19::equalColorTest2(col1, col2, toCoreFormat(colFmt), equalColorTolerance, 0.0);
}

void nearestNeighborScale(const uint32_t *src, int srcWidth, int srcHeight,
                          uint32_t *trg, int trgWidth, int trgHeight)
{
	xbrz19::nearestNeighborScale(src, srcWidth, srcHeight, trg, trgWidth, trgHeight);
}

} //namespace xbrz


// --- video filter entry points (filter.h) ----------------------------------

static void RenderXBRZScale(const SSurface &src, SSurface &dst, const size_t factor)
{
	if ( (src.Surface == NULL) || (dst.Surface == NULL) || (src.Width <= 0) || (src.Height <= 0) )
	{
		return;
	}

	// Surfaces are tightly packed, exactly like the buffers the old 1.4 code
	// handed to the scaler.
	const int srcPitch = src.Width * (int)sizeof(uint32_t);
	const int trgPitch = src.Width * (int)factor * (int)sizeof(uint32_t);

	xbrz19::scale(factor, (const uint32_t *)src.Surface, (uint32_t *)dst.Surface,
	              src.Width, src.Height, xbrz19::ColorFormat::rgb, srcPitch, trgPitch);
}

void Render2xBRZ(SSurface Src, SSurface Dst)
{
	RenderXBRZScale(Src, Dst, 2);
}

void Render3xBRZ(SSurface Src, SSurface Dst)
{
	RenderXBRZScale(Src, Dst, 3);
}

void Render4xBRZ(SSurface Src, SSurface Dst)
{
	RenderXBRZScale(Src, Dst, 4);
}

void Render5xBRZ(SSurface Src, SSurface Dst)
{
	RenderXBRZScale(Src, Dst, 5);
}

void Render6xBRZ(SSurface Src, SSurface Dst)
{
	RenderXBRZScale(Src, Dst, 6);
}
