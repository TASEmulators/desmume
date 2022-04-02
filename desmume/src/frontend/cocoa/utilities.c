/*
	Copyright (C) 2013-2022 DeSmuME team

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
#include <libkern/OSAtomic.h>

#ifdef HAS_SIERRA_UNFAIR_LOCK
	#include <os/lock.h>
#endif

static CFStringRef OSXProductName = NULL;
static CFStringRef OSXProductVersion = NULL;
static CFStringRef OSXProductBuildVersion = NULL;

static unsigned int OSXVersionMajor = 0;
static unsigned int OSXVersionMinor = 0;
static unsigned int OSXVersionRevision = 0;

static bool isSystemVersionAlreadyRead = false;

static void ReadSystemVersionPListFile()
{
	Boolean status = 0;
	bool isXMLFileFormat = false;
	CFDictionaryRef systemDict = NULL;
	
	// Read the SystemVersion.plist file.
	CFURLRef systemPListURL = CFURLCreateWithFileSystemPath(kCFAllocatorDefault, CFSTR("/System/Library/CoreServices/SystemVersion.plist"), kCFURLPOSIXPathStyle, false);
	CFReadStreamRef theStream = CFReadStreamCreateWithFile(kCFAllocatorDefault, systemPListURL);
	CFRelease(systemPListURL);
	systemPListURL = NULL;
	
	if (theStream == NULL)
	{
		return;
	}
	
	CFPropertyListFormat *xmlFormat = (CFPropertyListFormat *)malloc(sizeof(CFPropertyListFormat));
	*xmlFormat = 0;
	
	status = CFReadStreamOpen(theStream);
	if (!status)
	{
		CFRelease(theStream);
		free(xmlFormat);
		return;
	}
	
#if defined(MAC_OS_X_VERSION_10_6) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_6)
	if (kCFCoreFoundationVersionNumber >= kCFCoreFoundationVersionNumber10_6)
	{
		systemDict = CFPropertyListCreateWithStream(kCFAllocatorDefault, theStream, 0, kCFPropertyListImmutable, xmlFormat, NULL);
	}
	else
#endif
	{
		SILENCE_DEPRECATION_MACOS_10_10( systemDict = CFPropertyListCreateFromStream(kCFAllocatorDefault, theStream, 0, kCFPropertyListImmutable, xmlFormat, NULL) );
	}
	
	CFReadStreamClose(theStream);
	CFRelease(theStream);
	theStream = NULL;
	
	isXMLFileFormat = (*xmlFormat == kCFPropertyListXMLFormat_v1_0);
	free(xmlFormat);
	xmlFormat = NULL;
	
	if ( (systemDict == NULL) || !isXMLFileFormat )
	{
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
	
	// Release all resources now that the system version strings have been copied.
	CFRelease(systemDict);
	systemDict = NULL;
	
	// Mark that we've already read the SystemVersion.plist file so that we don't have to do this again.
	// However, this function can be called again at any time to force reload the system version strings
	// from the system file.
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

#ifdef HAS_SIERRA_UNFAIR_LOCK
static apple_unfairlock_t apple_unfairlock_create_sierra()
{
	apple_unfairlock_t theLock = (apple_unfairlock_t)malloc(sizeof(os_unfair_lock));
	*(os_unfair_lock *)theLock = OS_UNFAIR_LOCK_INIT;
	return theLock;
}

static void apple_unfairlock_destroy_sierra(apple_unfairlock_t theLock)
{
	free(theLock);
}

static inline void apple_unfairlock_lock_sierra(apple_unfairlock_t theLock)
{
	os_unfair_lock_lock((os_unfair_lock_t)theLock);
}

static inline void apple_unfairlock_unlock_sierra(apple_unfairlock_t theLock)
{
	os_unfair_lock_unlock((os_unfair_lock_t)theLock);
}
#endif

static apple_unfairlock_t apple_unfairlock_create_legacy()
{
	SILENCE_DEPRECATION_MACOS_10_12( apple_unfairlock_t theLock = (apple_unfairlock_t)malloc(sizeof(OSSpinLock)) );
	SILENCE_DEPRECATION_MACOS_10_12( *(OSSpinLock *)theLock = OS_SPINLOCK_INIT );
	return theLock;
}

static void apple_unfairlock_destroy_legacy(apple_unfairlock_t theLock)
{
	free(theLock);
}

static inline void apple_unfairlock_lock_legacy(apple_unfairlock_t theLock)
{
	SILENCE_DEPRECATION_MACOS_10_12( OSSpinLockLock((OSSpinLock *)theLock) );
}

static inline void apple_unfairlock_unlock_legacy(apple_unfairlock_t theLock)
{
	SILENCE_DEPRECATION_MACOS_10_12( OSSpinLockUnlock((OSSpinLock *)theLock) );
}

apple_unfairlock_t (*apple_unfairlock_create)() = &apple_unfairlock_create_legacy;
void (*apple_unfairlock_destroy)(apple_unfairlock_t theLock) = &apple_unfairlock_destroy_legacy;
void (*apple_unfairlock_lock)(apple_unfairlock_t theLock) = &apple_unfairlock_lock_legacy;
void (*apple_unfairlock_unlock)(apple_unfairlock_t theLock) = &apple_unfairlock_unlock_legacy;

void AppleUnfairLockSystemInitialize()
{
#ifdef HAS_SIERRA_UNFAIR_LOCK
	if (IsOSXVersionSupported(10, 12, 0))
	{
		apple_unfairlock_create = &apple_unfairlock_create_sierra;
		apple_unfairlock_destroy = &apple_unfairlock_destroy_sierra;
		apple_unfairlock_lock = &apple_unfairlock_lock_sierra;
		apple_unfairlock_unlock = &apple_unfairlock_unlock_sierra;
	}
	else
#endif
	{
		apple_unfairlock_create = &apple_unfairlock_create_legacy;
		apple_unfairlock_destroy = &apple_unfairlock_destroy_legacy;
		apple_unfairlock_lock = &apple_unfairlock_lock_legacy;
		apple_unfairlock_unlock = &apple_unfairlock_unlock_legacy;
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
