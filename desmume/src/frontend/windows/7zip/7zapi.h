#ifndef _7ZAPI_HEADER
#define _7ZAPI_HEADER

// Dynamic loader for the official 7z.dll (adapted from vba-rr-modernized).
//
// Archive handlers are not compiled into DeSmuME; they are reached through
// GetProcAddress so the architecture-matching DLL can be shipped next to the
// executable. A user-supplied DLL takes precedence, as LGPL requires.
//
// Search order:
//   1. 7z.dll next to the executable
//   2. %LOCALAPPDATA%\DeSmuME\7z-<arch>-<tag>.dll left by a previous run
//
// Bump SEVENZ_DLL_TAG when the shipped DLL version changes, so stale copies
// from an older build are not picked up.

#include "7z/C/7zTypes.h"
#include "7z/CPP/7zip/Archive/IArchive.h"

#ifndef SEVENZ_DLL_TAG
#define SEVENZ_DLL_TAG "1"
#endif

// These three are the exports the archive code uses (see 7-Zip's
// Archive/Archive2.def); the definitions below forward to the loaded DLL.
STDAPI GetNumberOfFormats(UINT32* numFormats);
STDAPI GetHandlerProperty2(UInt32 formatIndex, PROPID propID, PROPVARIANT* value);
STDAPI CreateObject(const GUID* clsid, const GUID* iid, void** outObject);

bool SevenZipLoad();
void SevenZipUnload();
bool SevenZipIsLoaded();

#endif
