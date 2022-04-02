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

#ifndef _COCOA_PORT_UTILITIES_
#define _COCOA_PORT_UTILITIES_

#include <stdbool.h>
#include <stdint.h>
#include <AvailabilityMacros.h>

// GCC diagnostic pragmas require at least GCC v4.6 (not normally available in Xcode)
// or Xcode Clang v3.0 or later from Xcode v4.2.
#if (!defined(__clang__) && ((__GNUC__ > 4) || ((__GNUC__ == 4) && (__GNUC_MINOR__ >= 6)))) || (__clang_major__ >= 3)

#define SILENCE_WARNING_UNUSED_VARIABLE_BEGIN \
_Pragma("GCC diagnostic push")\
_Pragma("GCC diagnostic ignored \"-Wunused-variable\"")
#define SILENCE_WARNING_UNUSED_VARIABLE_END _Pragma("GCC diagnostic pop")

#define SILENCE_DEPRECATION(expression) \
_Pragma("GCC diagnostic push")\
_Pragma("GCC diagnostic ignored \"-Wdeprecated-declarations\"")\
expression;\
_Pragma("GCC diagnostic pop")

#else

#define SILENCE_WARNING_UNUSED_VARIABLE_BEGIN
#define SILENCE_WARNING_UNUSED_VARIABLE_END
#define SILENCE_DEPRECATION(expression) expression;

#endif

#define SILENCE_DEPRECATION_MACOS_10_6(expression)  SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_7(expression)  SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_8(expression)  SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_9(expression)  SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_10(expression) SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_11(expression) SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_12(expression) SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_13(expression) SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_14(expression) SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_10_15(expression) SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_11_0(expression)  SILENCE_DEPRECATION(expression)
#define SILENCE_DEPRECATION_MACOS_12_0(expression)  SILENCE_DEPRECATION(expression)

#if defined(__clang__) && (__clang_major__ >= 9)
	#define HAVE_OSAVAILABLE 1
#else
	#define HAVE_OSAVAILABLE 0
#endif

#if defined(MAC_OS_X_VERSION_10_10) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_10)
	#define GUI_RESPONSE_OK     NSModalResponseOK
	#define GUI_RESPONSE_CANCEL NSModalResponseCancel
#else
	#define GUI_RESPONSE_OK     NSOKButton
	#define GUI_RESPONSE_CANCEL NSCancelButton
#endif

#if defined(MAC_OS_X_VERSION_10_12) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_12)
	#ifndef HAS_SIERRA_UNFAIR_LOCK
		#define HAS_SIERRA_UNFAIR_LOCK
	#endif

	#define ALERTSTYLE_CRITICAL NSAlertStyleCritical

	#define EVENT_MOUSEDOWN_LEFT  NSEventTypeLeftMouseDown
	#define EVENT_MOUSEDOWN_RIGHT NSEventTypeRightMouseDown
	#define EVENT_MOUSEDOWN_OTHER NSEventTypeOtherMouseDown

	#define EVENT_MODIFIERFLAG_SHIFT   NSEventModifierFlagShift

	#define WINDOWSTYLEMASK_BORDERLESS NSWindowStyleMaskBorderless
#else
	#define ALERTSTYLE_CRITICAL NSCriticalAlertStyle

	#define EVENT_MOUSEDOWN_LEFT  NSLeftMouseDown
	#define EVENT_MOUSEDOWN_RIGHT NSRightMouseDown
	#define EVENT_MOUSEDOWN_OTHER NSOtherMouseDown

	#define EVENT_MODIFIERFLAG_SHIFT   NSShiftKeyMask

	#define WINDOWSTYLEMASK_BORDERLESS NSBorderlessWindowMask
#endif

#if defined(MAC_OS_X_VERSION_10_14) && (MAC_OS_X_VERSION_MAX_ALLOWED >= MAC_OS_X_VERSION_10_14)
	#define GUI_STATE_MIXED NSControlStateValueMixed
	#define GUI_STATE_OFF   NSControlStateValueOff
	#define GUI_STATE_ON    NSControlStateValueOn

	#define PASTEBOARDTYPE_TIFF   NSPasteboardTypeTIFF
	#define PASTEBOARDTYPE_STRING NSPasteboardTypeString
	#define PASTEBOARDTYPE_URL    NSPasteboardTypeURL

	#define FILETYPE_TIFF     NSBitmapImageFileTypeTIFF
	#define FILETYPE_BMP      NSBitmapImageFileTypeBMP
	#define FILETYPE_GIF      NSBitmapImageFileTypeGIF
	#define FILETYPE_JPEG     NSBitmapImageFileTypeJPEG
	#define FILETYPE_PNG      NSBitmapImageFileTypePNG
	#define FILETYPE_JPEG2000 NSBitmapImageFileTypeJPEG2000
#else
	#define GUI_STATE_MIXED NSMixedState
	#define GUI_STATE_OFF   NSOffState
	#define GUI_STATE_ON    NSOnState

	#define PASTEBOARDTYPE_TIFF   NSTIFFPboardType
	#define PASTEBOARDTYPE_STRING NSStringPboardType
	#define PASTEBOARDTYPE_URL    NSURLPboardType

	#define FILETYPE_TIFF     NSTIFFFileType
	#define FILETYPE_BMP      NSBMPFileType
	#define FILETYPE_GIF      NSGIFFileType
	#define FILETYPE_JPEG     NSJPEGFileType
	#define FILETYPE_PNG      NSPNGFileType
	#define FILETYPE_JPEG2000 NSJPEG2000FileType
#endif

#ifdef __cplusplus
extern "C"
{
#endif

typedef void* apple_unfairlock_t;
void AppleUnfairLockSystemInitialize();
extern apple_unfairlock_t (*apple_unfairlock_create)();
extern void (*apple_unfairlock_destroy)(apple_unfairlock_t theLock);
extern void (*apple_unfairlock_lock)(apple_unfairlock_t theLock);
extern void (*apple_unfairlock_unlock)(apple_unfairlock_t theLock);

bool IsOSXVersionSupported(const unsigned int major, const unsigned int minor, const unsigned int revision);
bool IsOSXVersion(const unsigned int major, const unsigned int minor, const unsigned int revision);
uint32_t GetNearestPositivePOT(uint32_t value);

#ifdef __cplusplus
}
#endif

#endif // _COCOA_PORT_UTILITIES_
