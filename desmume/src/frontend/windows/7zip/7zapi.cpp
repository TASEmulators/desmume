// Must come before any interface header: it defines INITGUID, so this is
// the single translation unit that provides the 7-Zip interface GUIDs.
// Adapted from vba-rr-modernized (GPL-2.0) for DeSmuME.
#include "7z/CPP/Common/MyInitGuid.h"
#include "7zapi.h"

#include <cstdio>
#include <cstring>
#include <oleauto.h>

// Version the interface headers were taken from (C/7zVersion.h).
#include "7z/C/7zVersion.h"

// DeSmuME does not embed 7z.dll; the matching arch DLL is shipped next to the
// executable. Keep the loader generic so a user-supplied DLL can replace it.
static void SevenZipDebug(const char* message)
{
	OutputDebugStringA(message);
	OutputDebugStringA("\n");
}

// 7z.dll exports GetModuleProp as well, but the SDK has no prototype for it
// and nothing in the emulator calls it: it is probed once to log the version.
// The declaration gives decltype a type to take.  It is never defined here and
// never called directly; 7zapi.h deliberately does not forward it.
STDAPI GetModuleProp(PROPID propID, PROPVARIANT* value);

// NModulePropID::kVersion (CPP/7zip/ICoder.h) reports a VT_UI4 holding
// (major << 16) | minor, which is exactly how the headers encode MY_VERSION.
static const PROPID kSevenZipVersionProp = 1;

// Name of the host architecture, used for logs and for the architecture-keyed
// cache file name of the embedded DLL.
static const char* SevenZipArch()
{
#if defined(_M_ARM64)
	return "arm64";
#elif defined(_M_X64) || defined(_WIN64)
	return "x64";
#else
	return "x86";
#endif
}

static HMODULE s_module = NULL;
static decltype(&GetNumberOfFormats)  s_getNumberOfFormats = NULL;
static decltype(&GetHandlerProperty2) s_getHandlerProperty2 = NULL;
static decltype(&CreateObject)        s_createObject = NULL;
static decltype(&GetModuleProp)       s_getModuleProp = NULL;

static bool SevenZipResolve(HMODULE module)
{
	s_getNumberOfFormats = reinterpret_cast<decltype(&GetNumberOfFormats)>(
		GetProcAddress(module, "GetNumberOfFormats"));
	s_getHandlerProperty2 = reinterpret_cast<decltype(&GetHandlerProperty2)>(
		GetProcAddress(module, "GetHandlerProperty2"));
	s_createObject = reinterpret_cast<decltype(&CreateObject)>(
		GetProcAddress(module, "CreateObject"));

	// Optional: only used to report the version.  Older DLLs may not export it.
	s_getModuleProp = reinterpret_cast<decltype(&GetModuleProp)>(
		GetProcAddress(module, "GetModuleProp"));

	if (!s_getNumberOfFormats || !s_getHandlerProperty2 || !s_createObject)
	{
		s_getNumberOfFormats = NULL;
		s_getHandlerProperty2 = NULL;
		s_createObject = NULL;
		s_getModuleProp = NULL;
		return false;
	}
	return true;
}

// Says which DLL was loaded and whether it agrees with the headers the archive
// callbacks were compiled against.  A differing patch level is fine (the
// interfaces do not change between those), a differing major version is not.
static void SevenZipLogVersion(const char* path)
{
	unsigned major = 0;
	unsigned minor = 0;
	bool known = false;

	if (s_getModuleProp)
	{
		PROPVARIANT var;
		var.vt = VT_EMPTY;

		if (s_getModuleProp(kSevenZipVersionProp, &var) == S_OK && var.vt == VT_UI4)
		{
			major = (var.ulVal >> 16) & 0xFFFF;
			minor = var.ulVal & 0xFFFF;
			known = (major != 0);
		}

		VariantClear((VARIANTARG*)&var);
	}

	char version[32];
	if (known)
		_snprintf(version, sizeof(version), "%u.%02u", major, minor);
	else
		strcpy(version, "unknown");

	const char* verdict;
	if (!known)
		verdict = "the DLL reports no version, the headers may not match";
	else if (major == MY_VER_MAJOR && minor == MY_VER_MINOR)
		verdict = "exact match";
	else if (major == MY_VER_MAJOR)
		verdict = "patch level only, acceptable";
	else
		verdict = "MISMATCH";

	char message[512];
	_snprintf(message, sizeof(message),
		"7-Zip: loaded %s (%s, version %s); headers %s, %s",
		path, SevenZipArch(), version, MY_VERSION_NUMBERS, verdict);
	SevenZipDebug(message);
}

static bool SevenZipTryLoad(const char* path)
{
	HMODULE module = LoadLibraryA(path);
	if (!module)
		return false;

	if (!SevenZipResolve(module))
	{
		FreeLibrary(module);
		return false;
	}

	s_module = module;
	SevenZipLogVersion(path);
	return true;
}

static bool SevenZipExeDllPath(char* path, size_t size)
{
	DWORD length = GetModuleFileNameA(NULL, path, (DWORD)size);
	if (length == 0 || length >= size)
		return false;

	char* slash = strrchr(path, '\\');
	if (!slash)
		return false;
	slash[1] = '\0';

	if (strlen(path) + strlen("7z.dll") >= size)
		return false;
	strcat(path, "7z.dll");
	return true;
}

static bool SevenZipCachePath(char* path, size_t size)
{
	char directory[MAX_PATH];
	DWORD length = GetEnvironmentVariableA("LOCALAPPDATA", directory, (DWORD)sizeof(directory));

	if (length == 0 || length >= sizeof(directory))
	{
		if (GetTempPathA((DWORD)sizeof(directory), directory) == 0)
			return false;
	}

	// Make sure the path ends with a separator before appending the folder.
	size_t used = strlen(directory);
	if (used == 0 || directory[used - 1] != '\\')
	{
		if (used + 1 >= sizeof(directory))
			return false;
		directory[used++] = '\\';
		directory[used] = '\0';
	}

	if (used + strlen("DeSmuME") >= sizeof(directory))
		return false;
	strcat(directory, "DeSmuME");
	CreateDirectoryA(directory, NULL);

	const char* arch = SevenZipArch();
	int written = _snprintf(path, size, "%s\\7z-%s-%s.dll", directory, arch, SEVENZ_DLL_TAG);
	if (written < 0 || (size_t)written >= size)
		return false;
	return true;
}

bool SevenZipLoad()
{
	if (s_module)
		return true;

	char path[MAX_PATH];

	// 1. A copy next to the executable wins, so the user can replace it.
	if (SevenZipExeDllPath(path, sizeof(path)) && SevenZipTryLoad(path))
		return true;

	// 2. Accept a previously cached copy left by an older run.
	if (SevenZipCachePath(path, sizeof(path)) && SevenZipTryLoad(path))
		return true;

	SevenZipDebug("7-Zip: no usable 7z.dll was found; archive loading is disabled");
	return false;
}

void SevenZipUnload()
{
	if (s_module)
	{
		FreeLibrary(s_module);
		s_module = NULL;
	}

	s_getNumberOfFormats = NULL;
	s_getHandlerProperty2 = NULL;
	s_createObject = NULL;
	s_getModuleProp = NULL;
}

bool SevenZipIsLoaded()
{
	return s_module != NULL;
}

STDAPI GetNumberOfFormats(UINT32* numFormats)
{
	if (!s_getNumberOfFormats)
	{
		if (numFormats)
			*numFormats = 0;
		return E_FAIL;
	}
	return s_getNumberOfFormats(numFormats);
}

STDAPI GetHandlerProperty2(UInt32 formatIndex, PROPID propID, PROPVARIANT* value)
{
	if (!s_getHandlerProperty2)
		return E_FAIL;
	return s_getHandlerProperty2(formatIndex, propID, value);
}

STDAPI CreateObject(const GUID* clsid, const GUID* iid, void** outObject)
{
	if (!s_createObject)
	{
		if (outObject)
			*outObject = NULL;
		return E_FAIL;
	}
	return s_createObject(clsid, iid, outObject);
}
