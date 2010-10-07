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

TYPE_EXTEND_PARAM_IMMEDIATE_ADDINSTRUMENT		addInstrument=0;
TYPE_EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTMUTED	isInstrumentMuted=0;
TYPE_EXTEND_PARAM_IMMEDIATE_GETINSTRUMENTVOLUME getInstrumentVolume=0;
TYPE_EXTEND_PARAM_IMMEDIATE_OPENSOUNDVIEW openSoundView=0;

int SoundView_DlgOpen(HINSTANCE hAppIndst);
void SoundView_DlgClose();

void doOpenSoundView(void* hinst)
{
	SoundView_DlgOpen(hinst);
}

void killSoundView(void* hinst)
{
	SoundView_DlgClose();
}

typedef void (*TOpenSoundView)(void* hinst);
typedef struct CallbackSet
{
	TOpenSoundView doOpenSoundView;
	TOpenSoundView killSoundView;
};
struct CallbackSet soundViewCallbacks = { &doOpenSoundView, &killSoundView };



static void PASCAL XSFSetExtendParamImmediate(DWORD dwId, LPVOID lpPtr)
{
	switch(dwId)
	{
	case EXTEND_PARAM_IMMEDIATE_INTERPOLATION:
		dwInterpolation = *(unsigned long*)lpPtr;
		break;

	case EXTEND_PARAM_IMMEDIATE_ADDINSTRUMENT:			addInstrument =			(TYPE_EXTEND_PARAM_IMMEDIATE_ADDINSTRUMENT)			lpPtr; break;
	case EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTMUTED:		isInstrumentMuted =		(TYPE_EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTMUTED)		lpPtr; break;
	case EXTEND_PARAM_IMMEDIATE_GETINSTRUMENTVOLUME:	getInstrumentVolume =	(TYPE_EXTEND_PARAM_IMMEDIATE_GETINSTRUMENTVOLUME)	lpPtr; break;
	case EXTEND_PARAM_IMMEDIATE_OPENSOUNDVIEW:			openSoundView =	(TYPE_EXTEND_PARAM_IMMEDIATE_OPENSOUNDVIEW)	lpPtr; 
		openSoundView(&soundViewCallbacks);
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
