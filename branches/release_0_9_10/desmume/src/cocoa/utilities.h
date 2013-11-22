/*
	Copyright (C) 2013 DeSmuME team

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

#ifndef _COCOA_PORT_UTILITIES_
#define _COCOA_PORT_UTILITIES_

#include <ApplicationServices/ApplicationServices.h>
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

bool IsOSXVersionSupported(const unsigned int major, const unsigned int minor, const unsigned int revision);

uint32_t RGB555ToRGBA8888(const uint16_t color16);
uint32_t RGBA8888ForceOpaque(const uint32_t color32);
void RGB555ToRGBA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount);
void RGBA8888ForceOpaqueBuffer(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount);

CGSize GetTransformedBounds(const double normalBoundsWidth, const double normalBoundsHeight,
							const double scalar,
							const double angleDegrees);

double GetMaxScalarInBounds(const double normalBoundsWidth, const double normalBoundsHeight,
							const double keepInBoundsWidth, const double keepInBoundsHeight);

CGPoint GetNormalPointFromTransformedPoint(const double transformedPointX, const double transformedPointY,
										   const double normalBoundsWidth, const double normalBoundsHeight,
										   const double transformBoundsWidth, const double transformBoundsHeight,
										   const double scalar,
										   const double angleDegrees);
	
uint32_t GetNearestPositivePOT(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // _COCOA_PORT_UTILITIES_
