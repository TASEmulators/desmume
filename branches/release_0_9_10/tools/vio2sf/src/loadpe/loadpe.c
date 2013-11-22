/*
	libloadpe
    Copyright (C)2007  Ku-Zu

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

	loadpe.c

	0.5 (2008/03/19)
		Added support for WindowsNT4.0/NT3.x/Win32s.

	0.4 (2008/02/07)
		Leak checker.

  	0.3 (2008/01/08)
	    Fixed to protect memory after relocation and importing.

	0.2 (2007/12/25)
		Fixed not to free depending libraries before DLL_PROCESS_DETACH.

	0.1 (2007/12/24)
		First release.

*/


#define STRICT
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "loadpe.h"

#define MEMORY_CHECK 0
/*
	0:Off
	1:Check MSVCRT.DLL
	2:Check CRTDLL.DLL
*/

#if MEMORY_CHECK
static int chkload = 0;
static int chkcnt = 0;

typedef void * (__cdecl *LPFNMALLOC)(size_t s);
typedef void (__cdecl *LPFNFREE)(void *p);
typedef void * (__cdecl *LPFNREALLOC)(void *p, size_t s);
typedef const char * (__cdecl *LPFNSTRDUP)(const char *s);
typedef void * (__cdecl *LPFNCALLOC)(size_t num, size_t size);

static LPFNMALLOC pmalloc = 0;
static LPFNFREE pfree = 0;
static LPFNREALLOC prealloc = 0;
static LPFNSTRDUP pstrdup = 0;
/*
functions not implemented yet

calloc
_wcsdup,_mbsdup
_getcwd,_wgetcwd
_tempnam,_wtempnam,tmpnam,_wtmpnam
_fullpath,_wfullpath

*/

static void * __cdecl lpe_chk_malloc(size_t s)
{
	void *r = (pmalloc) ? pmalloc(s) : 0;
	if (r) chkcnt++;
	return r;
}
static void __cdecl lpe_chk_free(void *p)
{
	if (p)
	{
		if (pfree)
		{
			chkcnt--;
			pfree(p);
		}
	}
}
static void * __cdecl lpe_chk_realloc(void *p, size_t s)
{
	if (p)
	{
		if (s)
			return prealloc(p, s);
		else
			lpe_chk_free(p);
	}
	else
	{
		if (s)
			return lpe_chk_malloc(s);
	}
	return 0;
}
static const char * __cdecl lpe_chk_strdup(const char *p)
{
	char *r = 0;
	if (p)
	{
		size_t s = lstrlenA(p) + 1;
		r = lpe_chk_malloc(s);
		if (r) MoveMemory(r, p, s);
	}
	return r;
}
#endif

typedef BOOL (WINAPI *LPFNDLLENTRY)(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved);
typedef struct
{
	LPBYTE lpImageBase;
	DWORD dwImageSize;
	LPFNDLLENTRY lpEntry;
	LPBYTE lpImport;
	LPBYTE lpExport;
	HMODULE *hModules;
} LOADPE;

static WORD getwordle(BYTE *pData)
{
	return (WORD)(pData[0] | (((WORD)pData[1]) << 8));
}

static DWORD getdwordle(BYTE *pData)
{
	return pData[0] | (((DWORD)pData[1]) << 8) | (((DWORD)pData[2]) << 16) | (((DWORD)pData[3]) << 24);
}

static void setdwordle(LPBYTE lpPtr, DWORD dwDat)
{
	lpPtr[0] = (BYTE)((dwDat >> 0) & 0xff);
	lpPtr[1] = (BYTE)((dwDat >> 8) & 0xff);
	lpPtr[2] = (BYTE)((dwDat >> 16) & 0xff);
	lpPtr[3] = (BYTE)((dwDat >> 24) & 0xff);
}

static void setwordle(LPBYTE lpPtr, WORD wDat)
{
	lpPtr[0] = (BYTE)((wDat >> 0) & 0xff);
	lpPtr[1] = (BYTE)((wDat >> 8) & 0xff);
}

static void *XLoadLibraryH(HANDLE h)
{
	LOADPE *ret = NULL;
	LOADPE *lpe = NULL;
	HMODULE *hModules = NULL;
	LPBYTE lpImageBase = 0;
	LPBYTE lpHeader = 0;

#define iMZHeaderSize (0x40)
#define iPEHeaderSize (4 + 20 + 224)
	DWORD dwOffsetPE;
	DWORD dwNumberOfBytesRead;

	DWORD dwNumberOfSections;
	DWORD dwSizeOfOptionalHeader;
	DWORD dwAddressOfEntryPoint;
	DWORD dwImageBase;
	DWORD dwSizeOfImage;
	DWORD dwSizeOfHeaders;
	DWORD dwExportRVA;
	DWORD dwExportSize;
	DWORD dwImportRVA;
	DWORD dwImportSize;
	DWORD dwRelocationRVA;
	DWORD dwRelocationSize;
	DWORD dwIATRVA;
	DWORD dwIATSize;

	LPBYTE lpSectionHeader;
	LPBYTE lpSectionHeaderTop;
	LPBYTE lpSectionHeaderEnd;
	
	DWORD i;

	BYTE peheader[iPEHeaderSize];

	switch ((INT_PTR)h)
	{
	case (INT_PTR)INVALID_HANDLE_VALUE:
		return 0;
	default:
		if (!ReadFile(h, peheader, iMZHeaderSize, &dwNumberOfBytesRead, NULL) || dwNumberOfBytesRead < iMZHeaderSize)
			break;
		dwOffsetPE = getdwordle(peheader + 0x3c);
		if (0xffffffff == SetFilePointer(h, dwOffsetPE, 0, FILE_BEGIN))
			break;
		if (!ReadFile(h, peheader, iPEHeaderSize, &dwNumberOfBytesRead, NULL) || dwNumberOfBytesRead < iPEHeaderSize)
			break;

		/* check PE signature */
		if (getdwordle(peheader) != 0x00004550)
			break;

		/* COFF file header */
		dwNumberOfSections = getwordle(peheader + 4 + 2);
		dwSizeOfOptionalHeader = getwordle(peheader + 4 + 16);
		/* optional header standard field */
		dwAddressOfEntryPoint = getdwordle(peheader + 4 + 20 + 16);
		/* optional header WindowsNT field */
		dwImageBase = getdwordle(peheader + 4 + 20 + 28);
		dwSizeOfImage = getdwordle(peheader + 4 + 20 + 56);
		dwSizeOfHeaders = getdwordle(peheader + 4 + 20 + 60);
		/* optinal header data directory */
		dwExportRVA = getdwordle(peheader + 4 + 20 + 96);
		dwExportSize = getdwordle(peheader + 4 + 20 + 100);
		dwImportRVA = getdwordle(peheader + 4 + 20 + 104);
		dwImportSize = getdwordle(peheader + 4 + 20 + 108);
		dwRelocationRVA = getdwordle(peheader + 4 + 20 + 136);
		dwRelocationSize = getdwordle(peheader + 4 + 20 + 140);
		dwIATRVA = getdwordle(peheader + 4 + 20 + 192);
		dwIATSize = getdwordle(peheader + 4 + 20 + 196);

		lpImageBase = VirtualAlloc(0, dwSizeOfImage + dwSizeOfHeaders, MEM_RESERVE, PAGE_NOACCESS);
		if (!lpImageBase)
			break;
		
		if (0xffffffff == SetFilePointer(h, 0, 0, FILE_BEGIN))
			break;

		lpHeader = VirtualAlloc(lpImageBase, dwSizeOfHeaders, MEM_COMMIT, PAGE_READWRITE);
		if (lpHeader != lpImageBase)
			break;

		if (!ReadFile(h, lpImageBase, dwSizeOfHeaders, &dwNumberOfBytesRead, NULL) || dwNumberOfBytesRead < dwSizeOfHeaders)
			break;

		if (0xffffffff == SetFilePointer(h, dwOffsetPE + 4 + 20 + dwSizeOfOptionalHeader, 0, FILE_BEGIN))
			break;

		lpSectionHeaderTop = lpImageBase +  dwOffsetPE + 4 + 20 + dwSizeOfOptionalHeader;
		lpSectionHeaderEnd = lpSectionHeaderTop + dwNumberOfSections * 40;

		if (lpSectionHeaderEnd >= lpImageBase + dwSizeOfHeaders)
			break;

		/* load sections */
		lpSectionHeader = lpSectionHeaderTop;
		for (i = 0; i < dwNumberOfSections; i++)
		{
			DWORD dwVirtualSize;
			DWORD dwVirtualAddress;
			DWORD dwSizeOfRawData;
			DWORD dwPointerToRawData;
			LPBYTE lpSection;

			dwVirtualSize = getdwordle(lpSectionHeader + 8);
			dwVirtualAddress = getdwordle(lpSectionHeader + 12);
			dwSizeOfRawData = getdwordle(lpSectionHeader + 16);
			dwPointerToRawData = getdwordle(lpSectionHeader + 20);
			
			lpSection = VirtualAlloc(lpImageBase + dwVirtualAddress, dwVirtualSize, MEM_COMMIT, PAGE_READWRITE);
			if (!lpSection)
				break;

			ZeroMemory(lpImageBase + dwVirtualAddress, dwVirtualSize);
			if (0xffffffff == SetFilePointer(h, dwPointerToRawData, 0, FILE_BEGIN))
				break;

			if (!ReadFile(h, lpImageBase + dwVirtualAddress, dwSizeOfRawData, &dwNumberOfBytesRead, NULL) || dwNumberOfBytesRead < dwSizeOfRawData)
				break;

			lpSectionHeader += 40;
		}
		if (lpSectionHeader != lpSectionHeaderEnd) break;

		/* resolve relocations */
		if (dwRelocationRVA)
		{
			int iERROR = 0;
			LPBYTE lpReloc = lpImageBase + dwRelocationRVA;
			while (lpReloc < lpImageBase + dwRelocationRVA + dwRelocationSize)
			{
				DWORD dwPageRVA = getdwordle(lpReloc + 0);
				DWORD dwBlockSize = getdwordle(lpReloc + 4);
				for (i = 4; i < (dwBlockSize >> 1); i++)
				{
					DWORD dwOffset = getwordle(lpReloc + (i << 1));
					DWORD dwType = (dwOffset >> 12) & 0xf;
					LPBYTE lpRPtr = lpImageBase + dwPageRVA + (dwOffset & 0xfff);
					DWORD dwRDat;
					switch (dwType)
					{
					case IMAGE_REL_BASED_ABSOLUTE:
						break;
					case IMAGE_REL_BASED_HIGHLOW:
						dwRDat = getdwordle(lpRPtr);
						dwRDat = dwRDat + ((INT_PTR)lpImageBase) - dwImageBase;
						setdwordle(lpRPtr, dwRDat);
						break;
					case IMAGE_REL_BASED_HIGH:
						dwRDat = getwordle(lpRPtr) << 16;
						dwRDat = dwRDat + ((INT_PTR)lpImageBase) - dwImageBase;
						setwordle(lpRPtr, (WORD)((dwRDat >> 16) & 0xffff));
						break;
					case IMAGE_REL_BASED_LOW:
						dwRDat = getwordle(lpRPtr);
						dwRDat = dwRDat + ((INT_PTR)lpImageBase) - dwImageBase;
						setwordle(lpRPtr, (WORD)(dwRDat & 0xffff));
						break;
					case IMAGE_REL_BASED_HIGHADJ:
					default:
						iERROR = 1;
						break;
					}
				}
				lpReloc += dwBlockSize;
			}
			if (iERROR)
				break;
		}

		/* resolve imports */
		if (dwImportRVA)
		{
			int iError = 0;
			LPBYTE lpImport;
			DWORD dwModuleNumber = 0;
			lpImport = lpImageBase + dwImportRVA;
			while (getdwordle(lpImport + 12) && getdwordle(lpImport + 16))
			{
				dwModuleNumber++;
				lpImport += 20;
			}
			hModules = malloc(sizeof(HMODULE) * (dwModuleNumber + 1));
			if (!hModules)
				break;

			i = 0;
			lpImport = lpImageBase + dwImportRVA;
			while (getdwordle(lpImport + 12) && getdwordle(lpImport + 16))
			{
				LPCSTR lpszDllName = (LPCSTR)(lpImageBase + getdwordle(lpImport + 12));
				LPBYTE lpIAT = lpImageBase + getdwordle(lpImport + 16);
				HMODULE hDll = LoadLibraryA(lpszDllName);
				if (!hDll)
				{
					iError = 1;
					break;
				}
#if MEMORY_CHECK
#if MEMORY_CHECK == 2
				if (!lstrcmpi(lpszDllName, "CRTDLL.DLL"))
#else
				if (!lstrcmpi(lpszDllName, "MSVCRT.DLL"))
#endif
				{
					pmalloc = (LPFNMALLOC)GetProcAddress(hDll, "malloc");
					pfree = (LPFNFREE)GetProcAddress(hDll, "free");
					prealloc = (LPFNREALLOC)GetProcAddress(hDll, "realloc");
					pstrdup = (LPFNSTRDUP)GetProcAddress(hDll, "_strdup");
				}
#endif
				hModules[i++] = hDll;
				while (getdwordle(lpIAT))
				{
					DWORD dwIA = getdwordle(lpIAT);
					LPCSTR lpszProcName = (dwIA & 0x80000000) ? MAKEINTRESOURCEA(dwIA & 0x7fffffff) : (LPCSTR)(lpImageBase + dwIA + 2);
					DWORD dwProc = (DWORD)(INT_PTR)GetProcAddress(hDll, lpszProcName);
#if MEMORY_CHECK
					if (dwProc && dwProc == (DWORD)pmalloc)
						dwProc = (DWORD)lpe_chk_malloc;
					if (dwProc && dwProc == (DWORD)pfree)
						dwProc = (DWORD)lpe_chk_free;
					if (dwProc && dwProc == (DWORD)prealloc)
						dwProc = (DWORD)lpe_chk_realloc;
					if (dwProc && dwProc == (DWORD)pstrdup)
						dwProc = (DWORD)lpe_chk_strdup;
#endif
					if (dwProc)
						setdwordle(lpIAT, dwProc);
					else
						iError = 1;
					lpIAT += 4;
				}
				lpImport += 20;
			}
			hModules[i] = 0;

			if (iError)
				break;
		}

		lpe = malloc(sizeof(LOADPE));
		if (!lpe)
			break;

		lpe->lpEntry = (LPFNDLLENTRY)(INT_PTR)(dwAddressOfEntryPoint ? lpImageBase + dwAddressOfEntryPoint: 0);
		lpe->lpImageBase = lpImageBase;
		lpe->lpImport  = dwImportRVA ? lpImageBase + dwImportRVA: 0; 
		lpe->dwImageSize = dwSizeOfImage + dwSizeOfHeaders;
		lpe->lpExport = dwExportRVA ? lpImageBase + dwExportRVA: 0;
		lpe->hModules = hModules;

		/* protect sections */
		lpSectionHeader = lpSectionHeaderTop;
		for (i = 0; i < dwNumberOfSections; i++)
		{
			DWORD dwVirtualSize;
			DWORD dwVirtualAddress;
			DWORD dwCharacteristics;

			DWORD fNewProtect;
			DWORD fOldProtect;			

			dwVirtualSize = getdwordle(lpSectionHeader + 8);
			dwVirtualAddress = getdwordle(lpSectionHeader + 12);
			dwCharacteristics = getdwordle(lpSectionHeader + 36);
			
			switch ((dwCharacteristics >> 29) & 7)
			{
			default:
			case 0: fNewProtect = PAGE_NOACCESS; break;
			case 1: fNewProtect = PAGE_EXECUTE; break;
			case 2: fNewProtect = PAGE_READONLY; break;
			case 3: fNewProtect = PAGE_EXECUTE_READ; break;
			case 4:
			case 6: fNewProtect = PAGE_READWRITE; break;
			case 5:
			case 7: fNewProtect = PAGE_EXECUTE_READWRITE; break;
			}
			VirtualProtect(lpImageBase + dwVirtualAddress, dwVirtualSize, fNewProtect, &fOldProtect);

			lpSectionHeader += 40;
		}

		/* call entry point */
		if (lpe->lpEntry)
		{
			BOOL fResult;
			fResult = lpe->lpEntry((HMODULE)lpe->lpImageBase, DLL_PROCESS_ATTACH, 0);
			if (fResult == FALSE)
				break;
		}

		ret = lpe;
		lpe = 0;
		lpImageBase = 0;
		hModules = 0;
		break;
	}
	if (hModules)
	{
		for (i = 0; hModules[i]; i++)
			FreeLibrary(hModules[i]);
		free(hModules);
	}
	if (lpImageBase)
	{
		VirtualFree(lpImageBase, dwSizeOfImage + dwSizeOfHeaders, MEM_DECOMMIT);
		VirtualFree(lpImageBase, 0, MEM_RELEASE);
	}
	if (lpe)
		free(lpe);
	CloseHandle(h);
#if MEMORY_CHECK
	if (ret) chkload++;
#endif
	return ret;
}

void *XLoadLibraryA(const char *lpszFileName)
{
	HANDLE h = CreateFileA(lpszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	return XLoadLibraryH(h);
}

void *XLoadLibraryW(const WCHAR *lpszFileName)
{
	HANDLE h = CreateFileW(lpszFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	return XLoadLibraryH(h);
}

void XFreeLibrary(void *hModule)
{
	LOADPE *lpe = hModule;
	if (lpe)
	{
		if (lpe->lpEntry)
		{
			lpe->lpEntry((HMODULE)lpe->lpImageBase, DLL_PROCESS_DETACH, 0);
		}

		if (lpe->hModules)
		{
			HMODULE *hModules;
			for (hModules = lpe->hModules; *hModules; hModules++)
				FreeLibrary(*hModules);
			free(lpe->hModules);
		}

		if (lpe->lpImageBase)
		{
			VirtualFree(lpe->lpImageBase, lpe->dwImageSize, MEM_DECOMMIT);
			VirtualFree(lpe->lpImageBase, 0, MEM_RELEASE);
		}
		free(lpe);
#if MEMORY_CHECK
		if (--chkload == 0)
		{
			if (chkcnt)
			{
				char buf[1024];
				wsprintfA(buf, "loadpe - memory leak - count %d\n", chkcnt);
				OutputDebugStringA(buf);
			}
		}
#endif
	}
}

FARPROC XGetProcAddress(void *hModule, const char *lpfn)
{
	LOADPE *lpe = hModule;
	if (lpe)
	{
		if (lpe->lpExport)
		{
			DWORD dwOrdinal;
			DWORD dwOrdinalBase =  getdwordle(lpe->lpExport + 20);
			DWORD dwAddressTableEntries = getdwordle(lpe->lpExport + 20);
			DWORD dwNumberofNamePointers = getdwordle(lpe->lpExport + 24);
			LPBYTE lpEAT = lpe->lpImageBase + getdwordle(lpe->lpExport + 28);
			LPBYTE lpNPT = lpe->lpImageBase + getdwordle(lpe->lpExport + 32);
			LPBYTE lpOT = lpe->lpImageBase + getdwordle(lpe->lpExport + 36);
			if (((INT_PTR)lpfn) & (~((INT_PTR)0xffff)))
			{
				DWORD i;
				for (i = 0; i < dwNumberofNamePointers; i++)
				{
					if (lstrcmpA(lpfn, (LPCSTR)(lpe->lpImageBase + getdwordle(lpNPT + (i << 2)))) == 0)
						break;
				}
				if (i == dwNumberofNamePointers)
					return 0;
				dwOrdinal = getwordle(lpOT + (i << 1)) + dwOrdinalBase;
			}
			else
				dwOrdinal = (DWORD)(((INT_PTR)lpfn) & 0xffff);
			if (dwOrdinal >= dwOrdinalBase && dwOrdinal < dwOrdinalBase + dwAddressTableEntries)
				return (FARPROC)(INT_PTR)(lpe->lpImageBase + getdwordle(lpEAT + ((dwOrdinal - dwOrdinalBase) << 3) + 0));
		}
	}
	return 0;
}
