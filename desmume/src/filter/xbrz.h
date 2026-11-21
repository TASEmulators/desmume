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

// The scaler itself is xBRZ 1.9 (see filter/xbrz19/), taken from VBA-M, which
// rewrote the distance computation with SSE2/NEON and dropped the 64 MB lookup
// table the 1.4 build built on first use.
//
// This header keeps the interface the renderers were written against
// (xbrz::scale<N, FORMAT>) while the core takes the scale factor and the colour
// format as runtime arguments. The core is compiled as part of filter/xbrz.cpp,
// so no build file has to list it.

#ifndef XBRZ_HEADER_3847894708239054
#define XBRZ_HEADER_3847894708239054

#include <cstddef> //size_t
#include <cstdint>
#include <limits>

#include "xbrz19/xbrz.h"

namespace xbrz
{

enum ColorFormat //from high bits -> low bits, 8 bit per channel
{
	ColorFormatRGB,
	ColorFormatARGB,
	// DeSmuME used this value to describe the byte order of its unpacked
	// texture buffer (alpha in the high byte). The 1.9 core reads that same
	// layout through ColorFormat::argb, so both values map to one implementation.
	ColorFormatARGB_1bitAlpha
};

typedef xbrz19::ScalerCfg ScalerCfg;

inline xbrz19::ColorFormat toCoreFormat(const ColorFormat format)
{
	return (format == ColorFormatRGB) ? xbrz19::ColorFormat::rgb : xbrz19::ColorFormat::argb;
}

template<size_t SCALEFACTOR, ColorFormat FORMAT> //valid range: 2 - 6 and 9
inline void scale(const uint32_t *src, uint32_t *trg, const int srcWidth, const int srcHeight,
                  const ScalerCfg &cfg = ScalerCfg(),
                  const int yFirst = 0, const int yLast = (std::numeric_limits<int>::max)())
{
	// Both buffers are tightly packed: the unpacked texture buffers and the
	// video filter surfaces are allocated that way.
	const int srcPitch = srcWidth * (int)sizeof(uint32_t);
	const int trgPitch = srcWidth * (int)SCALEFACTOR * (int)sizeof(uint32_t);

	xbrz19::scale(SCALEFACTOR, src, trg, srcWidth, srcHeight, toCoreFormat(FORMAT),
	              srcPitch, trgPitch, cfg, yFirst, yLast);
}

void nearestNeighborScale(const uint32_t *src, int srcWidth, int srcHeight,
                          uint32_t *trg, int trgWidth, int trgHeight);

//parameter tuning
bool equalColorTest(uint32_t col1, uint32_t col2, ColorFormat colFmt, double luminanceWeight, double equalColorTolerance);

}

#endif
