/*
	Copyright (C) 2013-2017 DeSmuME team

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

static const uint8_t bits5to8[] = {
	0x00, 0x08, 0x10, 0x19, 0x21, 0x29, 0x31, 0x3A,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC5, 0xCE, 0xD6, 0xDE, 0xE6, 0xEF, 0xF7, 0xFF
};

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
	return	(bits5to8[((color16 >> 0) & 0x001F)] << 0) |
			(bits5to8[((color16 >> 5) & 0x001F)] << 8) |
			(bits5to8[((color16 >> 10) & 0x001F)] << 16) |
			0xFF000000;
}

/********************************************************************************************
	RGB555ToBGRA8888() - INLINE

	Converts a color from 15-bit RGB555 format into 32-bit BGRA8888 format.

	Takes:
		color16 - The pixel in 15-bit RGB555 format.

	Returns:
		A 32-bit unsigned integer containing the BGRA8888 formatted color.

	Details:
		The input and output pixels are expected to have little-endian byte order.
 ********************************************************************************************/
inline uint32_t RGB555ToBGRA8888(const uint16_t color16)
{
	return	(bits5to8[((color16 >> 10) & 0x001F)] << 0) |
			(bits5to8[((color16 >> 5) & 0x001F)] << 8) |
			(bits5to8[((color16 >> 0) & 0x001F)] << 16) |
			0xFF000000;
}

/********************************************************************************************
	RGB888ToBGRA8888() - INLINE

	Converts a color from 24-bit RGB888 format into 32-bit BGRA8888 format.

	Takes:
		color32 - The pixel in 24-bit RGB888 format.

	Returns:
		A 32-bit unsigned integer containing the BGRA8888 formatted color.

	Details:
		The input and output pixels are expected to have little-endian byte order.
 ********************************************************************************************/
inline uint32_t RGB888ToBGRA8888(const uint32_t color32)
{
	return	((color32 & 0x000000FF) << 16) |
			((color32 & 0x0000FF00)      ) |
			((color32 & 0x00FF0000) >> 16) |
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
	RGB555ToBGRA8888Buffer()

	Copies a 15-bit RGB555 pixel buffer into a 32-bit BGRA8888 pixel buffer.

	Takes:
		srcBuffer - Pointer to the source 15-bit RGB555 pixel buffer.

		destBuffer - Pointer to the destination 32-bit BGRA8888 pixel buffer.

		pixelCount - The number of pixels to copy.

	Returns:
		Nothing.

	Details:
		The source and destination pixels are expected to have little-endian byte order.
		Also, it is the caller's responsibility to ensure that the source and destination
		buffers are large enough to accomodate the requested number of pixels.
 ********************************************************************************************/
void RGB555ToBGRA8888Buffer(const uint16_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB555ToBGRA8888(*srcBuffer++);
	}
}

/********************************************************************************************
	RGB888ToBGRA8888Buffer()

	Copies a 24-bit RGB888 pixel buffer into a 32-bit BGRA8888 pixel buffer.

	Takes:
		srcBuffer - Pointer to the source 24-bit RGB888 pixel buffer.

		destBuffer - Pointer to the destination 32-bit BGRA8888 pixel buffer.

		pixelCount - The number of pixels to copy.

	Returns:
		Nothing.

	Details:
		The source and destination pixels are expected to have little-endian byte order.
		Also, it is the caller's responsibility to ensure that the source and destination
		buffers are large enough to accomodate the requested number of pixels.
 ********************************************************************************************/
void RGB888ToBGRA8888Buffer(const uint32_t *__restrict__ srcBuffer, uint32_t *__restrict__ destBuffer, size_t pixelCount)
{
	const uint32_t *__restrict__ destBufferEnd = destBuffer + pixelCount;
	
	while (destBuffer < destBufferEnd)
	{
		*destBuffer++ = RGB888ToBGRA8888(*srcBuffer++);
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
