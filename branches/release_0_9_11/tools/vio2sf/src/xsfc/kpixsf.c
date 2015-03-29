#define STRICT
#define WIN32_LEAN_AND_MEAN
#include "leakchk.h"

#include <windows.h>
#include <shlwapi.h>
#include <stdio.h>
#include <stdlib.h>

#include "../kobarin/kmp_pi.h"

#include "xsfdrv.h"

#include "../pversion.h"
#include "tagget.h"

#ifndef ENABLE_LOADPE
#if !defined(_DEBUG)
#define ENABLE_LOADPE 1
#else
#define ENABLE_LOADPE 0
#endif
#endif

#if ENABLE_LOADPE
#include "../loadpe/loadpe.h"
#else
#define XLoadLibraryA LoadLibraryA
#define XLoadLibraryW LoadLibraryW
#define XGetProcAddress GetProcAddress
#define XFreeLibrary FreeLibrary
#endif

#if _MSC_VER >= 1200
#pragma comment(linker, "/EXPORT:kmp_GetTestModule=_kmp_GetTestModule@0")
#endif

#if defined(_MSC_VER) && !defined(_DEBUG)
#pragma comment(linker,"/MERGE:.rdata=.text")
#endif

static HMODULE hDLL = 0;
BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	(void)lpvReserved;
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		hDLL = hinstDLL;
		DisableThreadLibraryCalls(hinstDLL);
	}
	return TRUE;
}

typedef struct {
	char sBasepath[MAX_PATH];
	HMODULE hDrv;
	IXSFDRV *lpif;
	int bTop;
	int bVolume;
	float fVolume;
	DWORD dwFileLen;
	BYTE bFileBuf[4];
} KMP;


static void *loadfileA(const char *fn, unsigned *plength)
{
	void *ret = NULL;
	HANDLE h = CreateFileA(fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (h != INVALID_HANDLE_VALUE)
	{
		unsigned length;
		void *buf;
		length = GetFileSize(h, NULL);
		buf = malloc(length);
		if (buf)
		{
			DWORD dwNumberOfBytesRead;
			ReadFile(h, buf, length, &dwNumberOfBytesRead, NULL);
			ret = buf;
			if (plength) *plength = length;
		}
		CloseHandle(h);
	}
	return ret;
}


static int PASCAL XSFGETLIBCALLBACK(void *lpWork, LPSTR lpszFilename, void **ppBuffer, DWORD *pdwSize)
{
	DWORD dwNumberOfBytesRead;
	void *filebuf;
	DWORD size;
	KMP *p = lpWork;

	HANDLE h = CreateFileA(lpszFilename, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (h == INVALID_HANDLE_VALUE)
	{
		char sCanPath[MAX_PATH];
		char sExtPath[MAX_PATH];
		if (lstrlenA(p->sBasepath) + lstrlenA(lpszFilename) + 1  >= MAX_PATH)
		{
			return 1;
		}
		lstrcpyA(sExtPath, p->sBasepath);
		lstrcatA(sExtPath, lpszFilename);
		if (!PathCanonicalizeA(sCanPath, sExtPath))
			lstrcpyA(sCanPath, sExtPath);
		h = CreateFileA(sCanPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
		if (h == INVALID_HANDLE_VALUE)
		{
			lstrcpyA(sExtPath, p->sBasepath);
			lstrcatA(sExtPath, PathFindFileNameA(lpszFilename));
			if (!PathCanonicalizeA(sCanPath, sExtPath))
				lstrcpyA(sCanPath, sExtPath);
			h = CreateFileA(sCanPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			if (h == INVALID_HANDLE_VALUE)
			{
				return 1;
			}
		}
	}

	size = GetFileSize(h, NULL);

	filebuf = p->lpif->LibAlloc(size);

	if (!filebuf)
	{
		CloseHandle(h);
		return 1;
	}

	ReadFile(h, filebuf, size, &dwNumberOfBytesRead, NULL);
	CloseHandle(h);

	*ppBuffer = filebuf;
	*pdwSize = size;

	return 0;
}


static HKMP WINAPI OpenFromBufferSub(const char *cszFileName, const BYTE *Buffer, DWORD dwSize, SOUNDINFO *pInfo)
{
	HKMP ret = NULL;
	KMP *p;
	LPFNXSFDRVSETUP lpfnXSFSetup;
	LPSTR lpFilePart;
	char *taglength;
	char *tagvolume;
	DWORD dwLength;

	CHAR dllpath[MAX_PATH];
	CHAR binpath[MAX_PATH];

	if (!pInfo) return ret;
	p = (KMP *)malloc(sizeof(KMP) + dwSize - 4);
	if (!p) return ret;

	if (cszFileName)
	{
		LPSTR lpscut = 0;
		GetFullPathNameA(cszFileName,  MAX_PATH, p->sBasepath, &lpscut);
		if (lpscut) *lpscut = 0;
	}
	else
	{
		p->sBasepath[0] = 0;
	}

	p->lpif = 0;
	p->dwFileLen = dwSize;

	GetModuleFileNameA(hDLL, dllpath, MAX_PATH);
	GetFullPathNameA(dllpath, MAX_PATH, binpath, &lpFilePart);
	lstrcpyA(lpFilePart, XSFDRIVER_MODULENAME);

	p->hDrv = XLoadLibraryA(binpath);
	if (!p->hDrv)
	{
		free(p);
		return ret;
	}

	lpfnXSFSetup = (LPFNXSFDRVSETUP)XGetProcAddress(p->hDrv, XSFDRIVER_ENTRYNAME);
	if (!lpfnXSFSetup)
	{
		XFreeLibrary(p->hDrv);
		p->hDrv = 0;
		free(p);
		return ret;
	}

	CopyMemory(p->bFileBuf, Buffer, dwSize);

	p->lpif = lpfnXSFSetup(XSFGETLIBCALLBACK ,p);
	if (!p->lpif || p->lpif->Start(p->bFileBuf, p->dwFileLen))
	{
		if (p->lpif)
			p->lpif->Term();
		XFreeLibrary(p->hDrv);
		p->lpif = 0;
		p->hDrv = 0;
		free(p);
		return ret;
	}
	p->bTop = 1;

	ret = (HKMP)p;
#ifndef XSFDRIVER_SAMPLERATE
	pInfo->dwSamplesPerSec = 44100;
#else
	pInfo->dwSamplesPerSec = XSFDRIVER_SAMPLERATE;
#endif
	pInfo->dwChannels = 2;
	pInfo->dwBitsPerSample = 16;
	pInfo->dwLength = 0xFFFFFFFF;
	pInfo->dwSeekable = 0;
	pInfo->dwUnitRender = 0x100;
	pInfo->dwReserved1 = 1;
	pInfo->dwReserved2 = 0;
	taglength = xsf_tagget("length", p->bFileBuf,p->dwFileLen);
	if (taglength)
	{
		dwLength = tag2ms(taglength);
		if (dwLength)
		{
			pInfo->dwLength = dwLength;
		}
		free(taglength);
	}
	p->bVolume = 0;
	p->fVolume = 1.0;
	tagvolume = xsf_tagget("volume", p->bFileBuf,p->dwFileLen);
	if (tagvolume)
	{
		float volume = (float)atof(tagvolume);
		p->fVolume = volume;
		p->bVolume = (p->fVolume != 1.0);
		free(tagvolume);
	}
	return ret;
}

static HKMP WINAPI Open(const char *cszFileName, SOUNDINFO *pInfo)
{
	HKMP ret = NULL;
	void *buffer;
	DWORD size;
	if (!pInfo) return ret;
	buffer = loadfileA(cszFileName, &size);
	if (buffer)
	{
		ret = OpenFromBufferSub(cszFileName, (const BYTE *)buffer, size, pInfo);
		free(buffer);
	}
	return ret;
}


static void  WINAPI Close(HKMP hKMP)
{
	KMP *p = (KMP *)hKMP;
	if (p)
	{
		if (p->lpif)
		{
			p->lpif->Term();
			p->lpif = 0;
		}
		if (p->hDrv)
		{
			XFreeLibrary(p->hDrv);
			p->hDrv = 0;
		}
		free(p);
	}
}

static DWORD WINAPI Render(HKMP hKMP, BYTE* Buffer, DWORD dwSize)
{
	KMP *p = (KMP *)hKMP;
	p->bTop = 0;
	p->lpif->Gen(Buffer, (dwSize >> 2));
	if (p->bVolume)
	{
		DWORD i;
		for (i = 0; i < dwSize; i += 4)
		{
			float s1 = ((short *)(Buffer + i))[0];
			float s2 = ((short *)(Buffer + i))[1];
			s1 *= p->fVolume;
			if (s1 > (float)0x7fff)
				s1 = (float)0x7fff;
			else if (s1 < (float)-0x8000)
				s1 = (float)-0x8000;
			s2 *= p->fVolume;
			if (s2 > (float)0x7fff)
				s2 = (float)0x7fff;
			else if (s2 < (float)-0x8000)
				s2 = (float)-0x8000;
			((short *)(Buffer + i))[0] = (short)s1;
			((short *)(Buffer + i))[1] = (short)s2;
		}
	}
	return dwSize;
}

static DWORD WINAPI SetPosition(HKMP hKMP, DWORD dwPos)
{
	KMP *p = (KMP *)hKMP;
	if (p->bTop && dwPos == 0)
	{
	}
	else
	{
		p->lpif->Start(p->bFileBuf, p->dwFileLen);
		p->bTop = 1;
	}
	return 0;
}

KBMEDIAPLUGIN_EXTS(exts)

static const KMPMODULE mod =
{
	KMPMODULE_VERSION,
	KBMEDIAPLUGIN_VERSION,
	KBMEDIAPLUGIN_COPYRIGHT,
	KBMEDIAPLUGIN_NAME,
	(const char **)exts,
#if ENABLE_LOADPE
	1,
#else
	0,
#endif
	NULL,
	NULL,
	Open,
	NULL,
	Close,
	Render,
	SetPosition
};

KMPMODULE* WINAPI KMP_GETMODULE(void)
{
	return (KMPMODULE*)&mod;
}