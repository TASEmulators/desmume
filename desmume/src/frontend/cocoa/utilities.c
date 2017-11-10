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
#include <ApplicationServices/ApplicationServices.h>

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

bool IsOSXVersion(const unsigned int major, const unsigned int minor, const unsigned int revision)
{
	bool result = false;
	
	if (!isSystemVersionAlreadyRead)
	{
		ReadSystemVersionPListFile();
	}
	
	result = (isSystemVersionAlreadyRead && (OSXVersionMajor == major) && (OSXVersionMinor == minor) && (OSXVersionRevision == revision));
	return result;
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
