/*
	Copyright (C) 2026 DeSmuME team

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

// Native replacement for the DirectX 8 SDK's dxerr8.lib.
//
// The Windows frontend used to link a prebuilt "directx-<arch>-ddraw-dinput8-
// dsound-dxerr8-dxguid.lib" blob that bundled the import libraries of ddraw,
// dinput8 and dsound together with dxerr8 and dxguid. Everything in that blob is
// still shipped by the Windows SDK except dxerr8, whose functions were dropped
// long ago along with the legacy DirectX SDK. This file supplies them so the
// project can link %(WindowsSdkDir) import libraries directly.
//
// Only the Visual Studio project compiles this file. The MinGW build gets the
// same API from its own libdxerr8.a (see snddx.cpp's __MINGW32__ branch).
//
// The only callers are the DirectSound start-up diagnostics in snddx.cpp, so the
// tables cover the DSERR_* codes plus the generic COM HRESULTs those calls can
// return. Anything else falls back to the numeric value, which is still more
// useful than a bare "Unknown error".

#include <windows.h>
#include <stdio.h>

#include "directx/dsound.h"
#include "directx/dxerr8.h"

namespace {

struct DXERR_ENTRY
{
	HRESULT hr;
	const char* name;
	const char* description;
};

const DXERR_ENTRY s_dxErrors[] =
{
	// Succeeded
	{ S_OK,                      "S_OK",                      "The operation completed successfully." },
	{ S_FALSE,                   "S_FALSE",                   "The operation completed, but not in the expected way." },

	// Generic COM failures - the DirectX APIs return these as-is
	{ E_FAIL,                    "E_FAIL",                    "An unspecified error occurred." },
	{ E_OUTOFMEMORY,             "E_OUTOFMEMORY",             "There is not enough memory to complete the operation." },
	{ E_INVALIDARG,              "E_INVALIDARG",              "One or more arguments are invalid." },
	{ E_NOINTERFACE,             "E_NOINTERFACE",             "The requested interface is not supported by this object." },
	{ E_POINTER,                 "E_POINTER",                 "An invalid pointer was supplied." },
	{ E_HANDLE,                  "E_HANDLE",                  "An invalid handle was supplied." },
	{ E_NOTIMPL,                 "E_NOTIMPL",                 "The requested functionality is not implemented." },
	{ E_ACCESSDENIED,            "E_ACCESSDENIED",            "Access to the resource was denied." },
	{ E_PENDING,                 "E_PENDING",                 "The data needed to complete the operation is not yet available." },
	{ E_ABORT,                   "E_ABORT",                   "The operation was aborted." },
	{ E_UNEXPECTED,              "E_UNEXPECTED",              "An unexpected failure occurred." },
	{ CLASS_E_NOAGGREGATION,     "CLASS_E_NOAGGREGATION",     "The object does not support aggregation." },
	{ REGDB_E_CLASSNOTREG,       "REGDB_E_CLASSNOTREG",       "The class is not registered on this machine." },
	{ CO_E_NOTINITIALIZED,       "CO_E_NOTINITIALIZED",       "The COM library was not initialized on this thread." },

	// DirectSound
	{ DS_NO_VIRTUALIZATION,      "DS_NO_VIRTUALIZATION",      "The requested virtualization is not available." },
	{ DSERR_ALLOCATED,           "DSERR_ALLOCATED",           "The request failed because resources, such as a priority level, were already in use by another caller." },
	{ DSERR_CONTROLUNAVAIL,      "DSERR_CONTROLUNAVAIL",      "A control (volume, pan, and so on) was requested that the object does not support. Controls have to be named in the buffer description when the buffer is created." },
	{ DSERR_INVALIDCALL,         "DSERR_INVALIDCALL",         "The call is not valid for the current state of this object." },
	{ DSERR_PRIOLEVELNEEDED,     "DSERR_PRIOLEVELNEEDED",     "A cooperative level of DSSCL_PRIORITY or higher is required before this call can succeed." },
	{ DSERR_BADFORMAT,           "DSERR_BADFORMAT",           "The specified WAVE format is not supported by the device." },
	{ DSERR_NODRIVER,            "DSERR_NODRIVER",            "No sound driver is available for use, or the given GUID is not a valid DirectSound device ID." },
	{ DSERR_ALREADYINITIALIZED,  "DSERR_ALREADYINITIALIZED",  "The object is already initialized." },
	{ DSERR_BUFFERLOST,          "DSERR_BUFFERLOST",          "The buffer memory has been lost and must be restored." },
	{ DSERR_OTHERAPPHASPRIO,     "DSERR_OTHERAPPHASPRIO",     "Another application has a higher priority level, preventing this call from succeeding." },
	{ DSERR_UNINITIALIZED,       "DSERR_UNINITIALIZED",       "The IDirectSound8::Initialize method has not been called or has not succeeded." },
	{ DSERR_BUFFERTOOSMALL,      "DSERR_BUFFERTOOSMALL",      "The buffer size, in bytes, is too small to be usable." },
	{ DSERR_DS8_REQUIRED,        "DSERR_DS8_REQUIRED",        "A DirectSound object of version 8 or later is required for this operation." },
	{ DSERR_SENDLOOP,            "DSERR_SENDLOOP",            "The send loop is closed: a buffer cannot be sent to itself, directly or indirectly." },
	{ DSERR_BADSENDBUFFERGUID,   "DSERR_BADSENDBUFFERGUID",   "The GUID of the send buffer does not match any buffer known to this object." },
	{ DSERR_OBJECTNOTFOUND,      "DSERR_OBJECTNOTFOUND",      "The specified object was not found." },
	{ DSERR_FXUNAVAILABLE,       "DSERR_FXUNAVAILABLE",       "The requested effects are not available." },
};

const DXERR_ENTRY* FindErrorEntry(HRESULT hr)
{
	const size_t count = sizeof(s_dxErrors) / sizeof(s_dxErrors[0]);
	for (size_t i = 0; i < count; i++)
	{
		if (s_dxErrors[i].hr == hr)
			return &s_dxErrors[i];
	}
	return NULL;
}

// DXGetErrorString8 returns a pointer to storage the caller does not own, so the
// fallback text and the wide conversions live in per-thread buffers. Each of the
// four entry points gets its own buffer so a single printf-style call can use a
// name and a description at the same time.
char* GetNameBuffer()
{
	static thread_local char buf[64];
	return buf;
}

char* GetDescriptionBuffer()
{
	static thread_local char buf[256];
	return buf;
}

WCHAR* GetWideNameBuffer()
{
	static thread_local WCHAR buf[64];
	return buf;
}

WCHAR* GetWideDescriptionBuffer()
{
	static thread_local WCHAR buf[256];
	return buf;
}

WCHAR* ToWide(const char* src, WCHAR* dst, size_t dstCount)
{
	if (src == NULL)
		src = "";
	MultiByteToWideChar(CP_ACP, 0, src, -1, dst, (int)dstCount);
	dst[dstCount - 1] = L'\0';
	return dst;
}

} // anonymous namespace

extern "C" {

const char* WINAPI DXGetErrorString8A(HRESULT hr)
{
	const DXERR_ENTRY* entry = FindErrorEntry(hr);
	if (entry != NULL)
		return entry->name;

	char* buf = GetNameBuffer();
	snprintf(buf, 64, "0x%08X", (unsigned int)hr);
	buf[63] = '\0';
	return buf;
}

const WCHAR* WINAPI DXGetErrorString8W(HRESULT hr)
{
	return ToWide(DXGetErrorString8A(hr), GetWideNameBuffer(), 64);
}

const char* WINAPI DXGetErrorDescription8A(HRESULT hr)
{
	const DXERR_ENTRY* entry = FindErrorEntry(hr);
	if (entry != NULL)
		return entry->description;

	char* buf = GetDescriptionBuffer();
	snprintf(buf, 256, "Unknown error 0x%08X.", (unsigned int)hr);
	buf[255] = '\0';
	return buf;
}

const WCHAR* WINAPI DXGetErrorDescription8W(HRESULT hr)
{
	return ToWide(DXGetErrorDescription8A(hr), GetWideDescriptionBuffer(), 256);
}

HRESULT WINAPI DXTraceA(const char* strFile, DWORD dwLine, HRESULT hr, const char* strMsg, BOOL bPopMsgBox)
{
	char text[1024];
	snprintf(text, sizeof(text), "%s(%lu): %s (%s)%s%s",
		(strFile != NULL) ? strFile : "", (unsigned long)dwLine,
		DXGetErrorString8A(hr), DXGetErrorDescription8A(hr),
		(strMsg != NULL) ? " - " : "", (strMsg != NULL) ? strMsg : "");
	text[sizeof(text) - 1] = '\0';

	OutputDebugStringA(text);
	if (bPopMsgBox)
		MessageBoxA(NULL, text, "DirectX Error", MB_OK | MB_ICONERROR);

	return hr;
}

HRESULT WINAPI DXTraceW(const char* strFile, DWORD dwLine, HRESULT hr, const WCHAR* strMsg, BOOL bPopMsgBox)
{
	char text[1024];
	const char* narrowMsg = NULL;
	char narrowMsgBuf[512];

	if (strMsg != NULL)
	{
		WideCharToMultiByte(CP_ACP, 0, strMsg, -1, narrowMsgBuf, sizeof(narrowMsgBuf), NULL, NULL);
		narrowMsgBuf[sizeof(narrowMsgBuf) - 1] = '\0';
		narrowMsg = narrowMsgBuf;
	}

	snprintf(text, sizeof(text), "%s(%lu): %s (%s)%s%s",
		(strFile != NULL) ? strFile : "", (unsigned long)dwLine,
		DXGetErrorString8A(hr), DXGetErrorDescription8A(hr),
		(narrowMsg != NULL) ? " - " : "", (narrowMsg != NULL) ? narrowMsg : "");
	text[sizeof(text) - 1] = '\0';

	OutputDebugStringA(text);
	if (bPopMsgBox)
		MessageBoxA(NULL, text, "DirectX Error", MB_OK | MB_ICONERROR);

	return hr;
}

} // extern "C"
