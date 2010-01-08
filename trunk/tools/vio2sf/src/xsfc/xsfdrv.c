#define STRICT
#define WIN32_LEAN_AND_MEAN
#include "leakchk.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

#include "../pversion.h"

#include "drvimpl.h"
#include "xsfdrv.h"

long dwInterpolation = 0;

#if _MSC_VER >= 1200
#pragma comment(linker, "/EXPORT:XSFSetup=_XSFSetup@8")
#endif

#if defined(_MSC_VER) && !defined(_DEBUG)
#pragma comment(linker,"/MERGE:.rdata=.text")
#endif

static void * PASCAL XSFLibAlloc(DWORD dwSize)
{
	return malloc(dwSize);
}
static void PASCAL XSFLibFree(void *lpPtr)
{
	free(lpPtr);
}
static int PASCAL XSFStart(void *lpPtr, DWORD dwSize)
{
	return !xsf_start(lpPtr, dwSize);
}
static void PASCAL XSFGen(void *lpPtr, DWORD dwSamples)
{
	xsf_gen(lpPtr, dwSamples);
}
static void PASCAL XSFTerm(void)
{
	xsf_term();
}

unsigned long dwChannelMute = 0;

static void PASCAL XSFSetChannelMute(DWORD dwPage, DWORD dwMute)
{
	if (dwPage == 0)
		dwChannelMute = dwMute;
}

#ifdef XSFDRIVER_EXTENDPARAM1NAME
static void PASCAL XSFSetExtendParam(DWORD dwId, LPCWSTR lpPtr)
{
	//xsf_set_extend_param(dwId, lpPtr);
}
#endif

static void PASCAL XSFSetExtendParamImmediate(DWORD dwId, LPVOID lpPtr)
{
	switch(dwId)
	{
	case EXTEND_PARAM_IMMEDIATE_INTERPOLATION:
		dwInterpolation = *(unsigned long*)lpPtr;
		break;
	}
}

static void *lpUserWrok = 0;
static LPFNGETLIB_XSFDRV lpfnGetLib = 0;
static IXSFDRV ifaossf =
{
	XSFLibAlloc,
	XSFLibFree,
	XSFStart,
	XSFGen,
	XSFTerm,
	4,
	XSFSetChannelMute,
	XSFSetExtendParam,
	XSFSetExtendParamImmediate
};

IXSFDRV * PASCAL XSFSetup(LPFNGETLIB_XSFDRV lpfn, void *lpWork)
{
	lpfnGetLib = lpfn;
	lpUserWrok = lpWork;
	return &ifaossf;
}

int xsf_get_lib(char *pfilename, void **ppbuffer, unsigned *plength)
{
	DWORD length32;
	if (!lpfnGetLib || lpfnGetLib(lpUserWrok, pfilename, ppbuffer, &length32)) return 0;
	if (plength) *plength = length32;
	return 1;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
#if defined(_MSC_VER) && defined(_DEBUG)
		_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
		DisableThreadLibraryCalls(hinstDLL);
	}
	return TRUE;
}


