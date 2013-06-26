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

#include "utilities.h"
#include <Accelerate/Accelerate.h>


static CFStringRef OSXProductName = NULL;
static CFStringRef OSXProductVersion = NULL;
static CFStringRef OSXProductBuildVersion = NULL;

static unsigned int OSXVersionMajor = 0;
static unsigned int OSXVersionMinor = 0;
static unsigned int OSXVersionRevision = 0;

static bool isSystemVersionAlreadyRead = false;

static void ReadSystemVersionPListFile()
{
	// Read the SystemVersion.plist file.
	CFDataRef resourceData = NULL;
	CFURLRef systemPListURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/CoreServices/SystemVersion.plist"), kCFURLPOSIXPathStyle, false);
	Boolean status = CFURLCreateDataAndPropertiesFromResource(kCFAllocatorDefault, systemPListURL, &resourceData, NULL, NULL, NULL);
	if (!status)
	{
		CFRelease(systemPListURL);
		return;
	}
	
	CFDictionaryRef systemDict = (CFDictionaryRef)CFPropertyListCreateFromXMLData(kCFAllocatorDefault, resourceData, kCFPropertyListImmutable, NULL);
	if (systemDict == NULL)
	{
		CFRelease(resourceData);
		CFRelease(systemPListURL);
		return;
	}
	
	// Get the system version string.
	if (OSXProductVersion != NULL)
	{
		CFRelease(OSXProductVersion);
	}
	
	OSXProductVersion = (CFStringRef)CFDictionaryGetValue(systemDict, CFSTR("ProductVersion"));
	
	if (OSXProductVersion != NULL)
	{
		// Copy the system version.
		CFRetain(OSXProductVersion);
		char versionCString[256] = {0};
		CFStringGetCString(OSXProductVersion, versionCString, 256, kCFStringEncodingUTF8);
		sscanf(versionCString, "%u.%u.%u", &OSXVersionMajor, &OSXVersionMinor, &OSXVersionRevision);
	}
	
	if (OSXProductName != NULL)
	{
		CFRelease(OSXProductName);
	}
	
	OSXProductName = (CFStringRef)CFDictionaryGetValue(systemDict, CFSTR("ProductName"));
	
	if (OSXProductName != NULL)
	{
		CFRetain(OSXProductName);
	}
	
	if (OSXProductBuildVersion != NULL)
	{
		CFRelease(OSXProductBuildVersion);
	}
	
	OSXProductBuildVersion = (CFStringRef)CFDictionaryGetValue(systemDict, CFSTR("ProductBuildVersion"));
	
	if (OSXProductBuildVersion != NULL)
	{
		CFRetain(OSXProductBuildVersion);
	}
	
	// Release all resources now that the system version string has been copied.
	CFRelease(resourceData);
	CFRelease(systemPListURL);
	CFRelease(systemDict);
	
	// Mark that we've already read the SystemVersion.plist file so that we don't
	// have to do this again.
	isSystemVersionAlreadyRead = true;
}

bool IsOSXVersionSupported(const unsigned int major, const unsigned int minor, const unsigned int revision)
{
	bool result = false;
	
	if (!isSystemVersionAlreadyRead)
	{
		ReadSystemVersionPListFile();
	}
	
	if (isSystemVersionAlreadyRead &&
		((OSXVersionMajor > major) ||
		 (OSXVersionMajor >= major && OSXVersionMinor > minor) ||
		 (OSXVersionMajor >= major && OSXVersionMinor >= minor && OSXVersionRevision >= revision)) )
	{
		result = true;
	}
	
	return result;
}

/********************************************************************************************
	RGB555ToRGBA8888() - INLINE

	Converts a color from 15-bit RGB555 format into 32-bit RGBA8888 format.

	Takes:
		color16 - The pixel in 15-bit RGB555 format.

	Returns:
		A 32-bit unsigned integer containing the RGBA8888 formatted color.

	Details:
		The input and output pixels are expected to have little-endian byte order.
 ********************************************************************************************/
inline uint32_t RGB555ToRGBA8888(const uint16_t color16)
{
	return	((color16 & 0x001F) << 3) |
			((color16 & 0x03E0) << 6) |
			((color16 & 0x7C00) << 9) |
			0xFF000000;
}

/********************************************************************************************
	RGBA8888ForceOpaque() - INLINE

	Forces the alpha channel of a 32-bit RGBA8888 color to a value of 0xFF.

	Takes:
		color32 - The pixel in 32-bit RGBA8888 format.

	Returns:
		A 32-bit unsigned integer containing the RGBA8888 formatted color.

	Details:
		The input and output pixels are expected to have little-endian byte order.
 ********************************************************************************************/
inline uint32_t RGBA8888ForceOpaque(const uint32_t color32)
{
	return color32 | 0xFF000000;
}

/********************************************************************************************
	RGB555ToRGBA8888Buffer()

	Copies a 15-bit RGB555 pixel buffer into a 32-bit RGBA8888 pixel buffer.

	Takes:
		srcBuffer - Pointer to the source 15-bit RGB555 pixel buffer.

		destBuffer - Pointer to the destination 32-bit RGBA8888 pixel buffer.

		pixelCount - The number of pixels to copy.

	Returns:
		Nothing.

	Details:
		The source and destination pixels are expected to have little-endian byte order.
		Also, it is the caller's responsibility to ensure that the source and destination
		buffers are large enough to accomodate the requested number of pixels.
 ********************************************************************************************/
void RGB555ToRGBA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB555ToRGBA8888(*srcBuffer++);
	}
}

/********************************************************************************************
	RGBA8888ForceOpaqueBuffer()

	Copies a 32-bit RGBA8888 pixel buffer into another 32-bit RGBA8888 pixel buffer.
	The pixels in the destination buffer will have an alpha value of 0xFF.

	Takes:
		srcBuffer - Pointer to the source 32-bit RGBA8888 pixel buffer.

		destBuffer - Pointer to the destination 32-bit RGBA8888 pixel buffer.

		pixelCount - The number of pixels to copy.

	Returns:
		Nothing.

	Details:
		The source and destination pixels are expected to have little-endian byte order.
		Also, it is the caller's responsibility to ensure that the source and destination
		buffers are large enough to accomodate the requested number of pixels.
 ********************************************************************************************/
void RGBA8888ForceOpaqueBuffer(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGBA8888ForceOpaque(*srcBuffer++);
	}
}

/********************************************************************************************
	GetTransformedBounds()

	Returns the bounds of a normalized 2D surface using affine transformations.

	Takes:
		normalBoundsWidth - The width of the normal 2D surface.

		normalBoundsHeight - The height of the normal 2D surface.

		scalar - The scalar used to transform the 2D surface.

		angleDegrees - The rotation angle, in degrees, to transform the 2D surface.

	Returns:
		The bounds of a normalized 2D surface using affine transformations.

	Details:
		The returned bounds is always a normal rectangle. Ignoring the scaling, the
		returned bounds will always be at its smallest when the angle is at 0, 90, 180,
		or 270 degrees, and largest when the angle is at 45, 135, 225, or 315 degrees.
 ********************************************************************************************/
CGSize GetTransformedBounds(const double normalBoundsWidth, const double normalBoundsHeight,
							const double scalar,
							const double angleDegrees)
{
	const double angleRadians = angleDegrees * (M_PI/180.0);
	
	// The points are as follows:
	//
	// (x[3], y[3])    (x[2], y[2])
	//
	//
	//
	// (x[0], y[0])    (x[1], y[1])
	
	// Do our scale and rotate transformations.
#ifdef __ACCELERATE__
	
	// Note that although we only need to calculate 3 points, we include 4 points
	// here because Accelerate prefers 16-byte alignment.
	double x[] = {0.0, normalBoundsWidth, normalBoundsWidth, 0.0};
	double y[] = {0.0, 0.0, normalBoundsHeight, normalBoundsHeight};
	
	cblas_drot(4, x, 1, y, 1, scalar * cos(angleRadians), scalar * sin(angleRadians));
	
#else // Keep a C-version of this transformation for reference purposes.
	
	const double w = scalar * normalBoundsWidth;
	const double h = scalar * normalBoundsHeight;
	const double d = hypot(w, h);
	const double dAngle = atan2(h, w);
	
	const double px = w * cos(angleRadians);
	const double py = w * sin(angleRadians);
	const double qx = d * cos(dAngle + angleRadians);
	const double qy = d * sin(dAngle + angleRadians);
	const double rx = h * cos((M_PI/2.0) + angleRadians);
	const double ry = h * sin((M_PI/2.0) + angleRadians);
	
	const double x[] = {0.0, px, qx, rx};
	const double y[] = {0.0, py, qy, ry};
	
#endif
	
	// Determine the transformed width, which is dependent on the location of
	// the x-coordinate of point (x[2], y[2]).
	CGSize transformBounds = {0.0, 0.0};
	
	if (x[2] > 0.0)
	{
		if (x[2] < x[3])
		{
			transformBounds.width = x[3] - x[1];
		}
		else if (x[2] < x[1])
		{
			transformBounds.width = x[1] - x[3];
		}
		else
		{
			transformBounds.width = x[2];
		}
	}
	else if (x[2] < 0.0)
	{
		if (x[2] > x[3])
		{
			transformBounds.width = -(x[3] - x[1]);
		}
		else if (x[2] > x[1])
		{
			transformBounds.width = -(x[1] - x[3]);
		}
		else
		{
			transformBounds.width = -x[2];
		}
	}
	else
	{
		transformBounds.width = abs(x[1] - x[3]);
	}
	
	// Determine the transformed height, which is dependent on the location of
	// the y-coordinate of point (x[2], y[2]).
	if (y[2] > 0.0)
	{
		if (y[2] < y[3])
		{
			transformBounds.height = y[3] - y[1];
		}
		else if (y[2] < y[1])
		{
			transformBounds.height = y[1] - y[3];
		}
		else
		{
			transformBounds.height = y[2];
		}
	}
	else if (y[2] < 0.0)
	{
		if (y[2] > y[3])
		{
			transformBounds.height = -(y[3] - y[1]);
		}
		else if (y[2] > y[1])
		{
			transformBounds.height = -(y[1] - y[3]);
		}
		else
		{
			transformBounds.height = -y[2];
		}
	}
	else
	{
		transformBounds.height = abs(y[3] - y[1]);
	}
	
	return transformBounds;
}

/********************************************************************************************
	GetMaxScalarInBounds()

	Returns the maximum scalar that a rectangle can grow, while maintaining its aspect
	ratio, within a boundary.

	Takes:
		normalBoundsWidth - The width of the normal 2D surface.

		normalBoundsHeight - The height of the normal 2D surface.

		keepInBoundsWidth - The width of the keep-in 2D surface.

		keepInBoundsHeight - The height of the keep-in 2D surface.

	Returns:
		The maximum scalar that a rectangle can grow, while maintaining its aspect ratio,
		within a boundary.

	Details:
		If keepInBoundsWidth or keepInBoundsHeight are less than or equal to zero, the
		returned scalar will be zero.
 ********************************************************************************************/
double GetMaxScalarInBounds(const double normalBoundsWidth, const double normalBoundsHeight,
							const double keepInBoundsWidth, const double keepInBoundsHeight)
{
	const double maxX = (normalBoundsWidth <= 0.0) ? 0.0 : keepInBoundsWidth / normalBoundsWidth;
	const double maxY = (normalBoundsHeight <= 0.0) ? 0.0 : keepInBoundsHeight / normalBoundsHeight;
	
	return (maxX <= maxY) ? maxX : maxY;
}

/********************************************************************************************
	GetNormalPointFromTransformedPoint()

	Returns a normalized point from a point from a 2D transformed surface.

	Takes:
		transformedPointX - The X coordinate of a 2D point as it exists on a 2D
			transformed surface.

		transformedPointY - The Y coordinate of a 2D point as it exists on a 2D
			transformed surface.

		normalBoundsWidth - The width of the normal 2D surface.

		normalBoundsHeight - The height of the normal 2D surface.

		transformBoundsWidth - The width of the transformed 2D surface.

		transformBoundsHeight - The height of the transformed 2D surface.

		scalar - The scalar used on the transformed 2D surface.

		angleDegrees - The rotation angle, in degrees, of the transformed 2D surface.

	Returns:
		A normalized point from a point from a 2D transformed surface.

	Details:
		It may help to call GetTransformedBounds() for the transformBounds parameter.
 ********************************************************************************************/
CGPoint GetNormalPointFromTransformedPoint(const double transformedPointX, const double transformedPointY,
										   const double normalBoundsWidth, const double normalBoundsHeight,
										   const double transformBoundsWidth, const double transformBoundsHeight,
										   const double scalar,
										   const double angleDegrees)
{
	// Get the coordinates of the transformed point and translate the coordinate
	// system so that the origin becomes the center.
	const double transformedX = transformedPointX - (transformBoundsWidth / 2.0);
	const double transformedY = transformedPointY - (transformBoundsHeight / 2.0);
	
	// Perform rect-polar conversion.
	
	// Get the radius r with respect to the origin.
	const double r = hypot(transformedX, transformedY) / scalar;
	
	// Get the angle theta with respect to the origin.
	double theta = 0.0;
	
	if (transformedX == 0.0)
	{
		if (transformedY > 0.0)
		{
			theta = M_PI / 2.0;
		}
		else if (transformedY < 0.0)
		{
			theta = M_PI * 1.5;
		}
	}
	else if (transformedX < 0.0)
	{
		theta = M_PI - atan2(transformedY, -transformedX);
	}
	else if (transformedY < 0.0)
	{
		theta = atan2(transformedY, transformedX) + (M_PI * 2.0);
	}
	else
	{
		theta = atan2(transformedY, transformedX);
	}
	
	// Get the normalized angle and use it to rotate about the origin.
	// Then do polar-rect conversion and translate back to transformed coordinates
	// with a 0 degree rotation.
	const double angleRadians = angleDegrees * (M_PI/180.0);
	const double normalizedAngle = theta - angleRadians;
	const double normalizedX = (r * cos(normalizedAngle)) + (normalBoundsWidth / 2.0);
	const double normalizedY = (r * sin(normalizedAngle)) + (normalBoundsHeight / 2.0);
	
	return CGPointMake(normalizedX, normalizedY);
}

/********************************************************************************************
	GetNearestPositivePOT()

	Returns the next highest power of two of a 32-bit integer value.

	Takes:
		value - A 32-bit integer value.

	Returns:
		A 32-bit integer with the next highest power of two compared to the input value.

	Details:
		If the input value is already a power of two, this function returns the same
		value.
 ********************************************************************************************/
uint32_t GetNearestPositivePOT(uint32_t value)
{
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;
	
	return value;
}
