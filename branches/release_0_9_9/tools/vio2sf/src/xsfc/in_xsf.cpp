extern unsigned long dwInterpolation;

#ifndef ENABLE_UNICODE_PLUGIN
#define ENABLE_UNICODE_PLUGIN 0
#endif
#ifndef ENABLE_GETEXTENDINFO
#define ENABLE_GETEXTENDINFO 1
#endif
#ifndef ENABLE_GETEXTENDINFOW
#define ENABLE_GETEXTENDINFOW 1
#endif
#ifndef ENABLE_TAGWRITER
#define ENABLE_TAGWRITER 1
#endif
#ifndef ENABLE_EXTENDREADER
#define ENABLE_EXTENDREADER 1
#endif

#define WIN32_LEAN_AND_MEAN
#define NOGDI
#include "leakchk.h"

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#if _MSC_VER >= 1400
#include <intrin.h>
#endif

#if ENABLE_UNICODE_PLUGIN
#define UNICODE_INPUT_PLUGIN
#else
#undef UNICODE_INPUT_PLUGIN
#endif

/* Winamp SDK headers */
#include <winamp/in2.h>
#include <winamp/wa_ipc.h>

#if !defined(_DEBUG)
#include "../loadpe/loadpe.h"
#else
#define XLoadLibraryW LoadLibraryW
#define XLoadLibraryA LoadLibraryA
#define XGetProcAddress(h,n) GetProcAddress((HMODULE)h,n)
#define XFreeLibrary(h) FreeLibrary((HMODULE)h)
#endif

#include "xsfdrv.h"

#include "../pversion.h"
#include "tagget.h"

#include "in_xsfcfg.h"

#if _MSC_VER >= 1200
#pragma comment(linker, "/EXPORT:winampGetInModule2=_winampGetInModule2")

#if ENABLE_GETEXTENDINFO
#pragma comment(linker, "/EXPORT:winampGetExtendedFileInfo=_winampGetExtendedFileInfo")
#endif

#if ENABLE_GETEXTENDINFOW
#pragma comment(linker, "/EXPORT:winampGetExtendedFileInfoW=_winampGetExtendedFileInfoW")
#endif

#if ENABLE_TAGWRITER
#pragma comment(linker, "/EXPORT:winampSetExtendedFileInfo=_winampSetExtendedFileInfo")
#pragma comment(linker, "/EXPORT:winampSetExtendedFileInfoW=_winampSetExtendedFileInfoW")
#pragma comment(linker, "/EXPORT:winampWriteExtendedFileInfo=_winampWriteExtendedFileInfo")
#endif

#if ENABLE_GETEXTENDINFO || ENABLE_GETEXTENDINFOW || ENABLE_TAGWRITER
#pragma comment(linker, "/EXPORT:winampUseUnifiedFileInfoDlg=_winampUseUnifiedFileInfoDlg")
#pragma comment(linker, "/EXPORT:winampAddUnifiedFileInfoPane=_winampAddUnifiedFileInfoPane")
#endif

#if ENABLE_EXTENDREADER
#ifdef UNICODE_INPUT_PLUGIN
#pragma comment(linker, "/EXPORT:winampGetExtendedRead_openW=_winampGetExtendedRead_openW")
#else
#pragma comment(linker, "/EXPORT:winampGetExtendedRead_open=_winampGetExtendedRead_open")
#endif
#pragma comment(linker, "/EXPORT:winampGetExtendedRead_getData=_winampGetExtendedRead_getData")
#pragma comment(linker, "/EXPORT:winampGetExtendedRead_setTime=_winampGetExtendedRead_setTime")
#pragma comment(linker, "/EXPORT:winampGetExtendedRead_close=_winampGetExtendedRead_close")
#endif

#endif

#if defined(_MSC_VER) && !defined(_DEBUG)
#pragma comment(linker,"/MERGE:.rdata=.text")
#endif

namespace
{

#define CHECK_SILENCE_BIAS 0x8000000
#ifndef CHECK_SILENCE_LEVEL
#define CHECK_SILENCE_LEVEL 7
#endif

#define SIZE_OF_BUFFER (576 * 4)

typedef signed short xsfsample_t;

static void wstrncpychk(wchar_t *d, const wchar_t *s, size_t m)
{
	size_t l = wcslen(s) + 1;
	wcsncpy(d, s, (l > m) ? m : l);
}

static void mstrncpychk(char *d, const char *s, size_t m)
{
	size_t l = strlen(s) + 1;
	strncpy(d, s, (l > m) ? m : l);
}

static void icsncpy(in_char *d, const in_char *s, size_t m)
{
#if _MSC_VER >= 1200
#ifndef STATUS_ACCESS_VIOLATION
#define STATUS_ACCESS_VIOLATION ((DWORD )0xC0000005L) 
#endif
	__try
	{
#endif
#ifdef UNICODE_INPUT_PLUGIN
		wstrncpychk(d, s, m);
#else
		mstrncpychk(d, s, m);
#endif
#if _MSC_VER >= 1200
	}
	__except(GetExceptionCode() == STATUS_ACCESS_VIOLATION)
	{
	}
#endif
}

class XSFDriver
{
public:

	IXSFDRV *lpif;

	xsfc::TAutoBuffer<char> xsfdata;

	unsigned cur_smp;
	unsigned len_smp;
	unsigned fad_smp;

	int hasvolume;
	bool haslength;
	float volume;
	enum
	{
#ifndef XSFDRIVER_SAMPLERATE
		samplerate = 44100,
#else
		samplerate = XSFDRIVER_SAMPLERATE,
#endif
		samplebits = 16,
		ch = 2,
		blockshift = 2,
	};
	int length_in_ms; int fade_in_ms;

	volatile unsigned seek_pos;
	volatile int seek_req;
	volatile int seek_kil;

protected:
	unsigned detectedSilenceSec;
	unsigned detectedSilenceSample;
	unsigned skipSilenceOnStartSec;
	unsigned long prevSampleL;
	unsigned long prevSampleR;

	void *hDriver;
	xsfc::TString sLibBasePath;
	bool fUTF8;

	static void *liballoc_cb(void *pwork, size_t s)
	{
		IXSFDRV *lpif = static_cast<IXSFDRV *>(pwork);
		return lpif->LibAlloc(DWORD(s));
	}
	static void libfree_cb(void *pwork, void *p)
	{
		IXSFDRV *lpif = static_cast<IXSFDRV *>(pwork);
		lpif->LibFree(p);
	}

	static int PASCAL XSFGETLIBCALLBACK(void *lpWork, LPSTR lpszFilename, void **ppBuffer, DWORD *pdwSize)
	{
		XSFDriver *lpDrv = static_cast<XSFDriver *>(lpWork);

		size_t libsize;
		xsfc::TString sLibName = xsfc::TString(lpDrv->fUTF8, lpszFilename);

		void *libdata = xsfc::TWin32::LoadEx(xsfc::TWin32::CanonicalizePath(lpDrv->sLibBasePath + sLibName), libsize, lpDrv->lpif, liballoc_cb, libfree_cb);
		if (!libdata)
		{
			/* in_zip.dll */
			libdata = xsfc::TWin32::LoadEx(lpDrv->sLibBasePath + xsfc::TWin32::ExtractFileName(sLibName), libsize, lpDrv->lpif, liballoc_cb, libfree_cb);
		}
		if (!libdata)
			return 1;

		*ppBuffer = libdata;
		*pdwSize = DWORD(libsize);

		return 0;
	}

	void SeekTop()
	{
		cur_smp = 0;

		skipSilenceOnStartSec = CFGGetSkipSilenceOnStartSec();

		detectedSilenceSec = 0;
		detectedSilenceSample = 0;

		prevSampleL = CHECK_SILENCE_BIAS;
		prevSampleR = CHECK_SILENCE_BIAS;
	}

public:

	XSFDriver() throw()
		: hDriver(0), lpif(0)
	{
	}
	~XSFDriver()
	{
	}

	void Free()
	{
		if (lpif)
		{
			lpif->Term();
			lpif = 0;
		}
		if (hDriver)
		{
			XFreeLibrary(hDriver);
			hDriver = 0;
		}
		xsfdata.Free();
	}

	bool Init(HINSTANCE hDll)
	{
		xsfc::TString sDrvPath = xsfc::TWin32::ExtractPath(xsfc::TWin32::ModulePath(hDll)) + XSFDRIVER_MODULENAME;
		hDriver = xsfc::TWin32::IsUnicodeSupportedOS() ? XLoadLibraryW(sDrvPath) : XLoadLibraryA(xsfc::TStringM(sDrvPath));
		if (!hDriver)
			return false;
		LPFNXSFDRVSETUP lpfnXSFSetup = (LPFNXSFDRVSETUP)XGetProcAddress(hDriver, XSFDRIVER_ENTRYNAME);
		lpif = lpfnXSFSetup ? lpfnXSFSetup(XSFGETLIBCALLBACK ,this) : 0;
		if (!lpif)
		{
			Free();
			return false;
		}

		lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_ADDINSTRUMENT,					addInstrument);
		lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTMUTED,				isInstrumentMuted);
		lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_GETINSTRUMENTVOLUME,			getInstrumentVolume);
		lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_ISINSTRUMENTSELECTIONACTIVE,	isInstrumentSelectionActive);
		lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_OPENSOUNDVIEW,		openSoundView);

		
		return true;
	}

	bool Load(xsfc::TString fn)
	{

		length_in_ms = CFGGetDefaultLength();
		fade_in_ms = CFGGetDefaultFade();
		hasvolume = 0;
		volume = 1;

		sLibBasePath = xsfc::TWin32::ExtractPath(fn);

		xsfdata = xsfc::TWin32::Map(fn);
		if (!xsfdata.Ptr())
			return false;
		
		fUTF8 = XSFTag::Exists("utf8", xsfdata.Ptr(), xsfdata.Len()) != 0;
		if (lpif->dwInterfaceVersion >= 3)
		{
			lpif->SetExtendParam(1, CFGGetExtendParam1());
			lpif->SetExtendParam(2, CFGGetExtendParam2());
		}
		if (lpif->Start(xsfdata.Ptr(), xsfdata.Len()))
		{
			return false;
		}

		if (!XSFTag::GetVolume(&volume, "volume", xsfdata, 0))
			hasvolume = 1;
		length_in_ms = XSFTag::GetLengthMS(xsfdata, CFGGetDefaultLength());
		fade_in_ms = XSFTag::GetFadeMS(xsfdata, CFGGetDefaultFade());

		haslength = XSFTag::Exists("length", xsfdata.Ptr(), xsfdata.Len()) != 0;

		return true;
	}

	void Start()
	{
		len_smp = MulDiv(length_in_ms, samplerate, 1000);
		fad_smp = MulDiv(fade_in_ms, samplerate, 1000);
		seek_pos = 0;
		seek_kil = 0;
		seek_req = 0;
		SeekTop();
	}

	int Gen(void *pbuf, unsigned bufsize)
	{
		xsfsample_t *ptop = static_cast<xsfsample_t *>(pbuf); 
		unsigned detectSilence = CFGGetDetectSilenceSec();
		unsigned pos = 0;
		if (lpif->dwInterfaceVersion >= 2)
		{
			bool output = false;
			for (int i = 0; i < 4; i++)
			{
				unsigned long mute = CFGGetChannelMute(i);
				output |= (mute != ~unsigned long(0));
				lpif->SetChannelMute(i, mute);
			}
			if (!output) detectSilence = 0;
		}
		if (lpif->dwInterfaceVersion >= 4)
		{
			lpif->SetExtendParamImmediate(EXTEND_PARAM_IMMEDIATE_INTERPOLATION, &dwInterpolation);
		}
		while (pos < bufsize)
		{
			unsigned ofs;
			xsfsample_t *pblk = ptop + (pos << 1);
			unsigned remain = bufsize - pos;
			lpif->Gen(pblk, remain);
			if (detectSilence || skipSilenceOnStartSec)
			{
				xsfsample_t *pskp = 0;
				xsfsample_t *pcur = pblk;
				for (ofs = 0; ofs < remain; ofs++)
				{
					long smpl = pcur[0];
					long smpr = pcur[1];
					bool silence = (((unsigned long)(smpl + CHECK_SILENCE_BIAS + CHECK_SILENCE_LEVEL)) - prevSampleL <= (CHECK_SILENCE_LEVEL) * 2) && (((unsigned long)(smpr + CHECK_SILENCE_BIAS + CHECK_SILENCE_LEVEL)) - prevSampleR <= (CHECK_SILENCE_LEVEL) * 2);

					if (silence)
					{
						if (++detectedSilenceSample >= samplerate)
						{
							detectedSilenceSample -= samplerate;
							detectedSilenceSec++;
							if (skipSilenceOnStartSec && detectedSilenceSec >= skipSilenceOnStartSec)
							{
								skipSilenceOnStartSec = 0;
								detectedSilenceSec = 0;
								if (pblk != pcur) pskp = pcur;
							}
						}
					}
					else
					{
						detectedSilenceSample = 0;
						detectedSilenceSec = 0;
						if (skipSilenceOnStartSec)
						{
							skipSilenceOnStartSec = 0;
							if (pblk != pcur) pskp = pcur;
						}
					}
					prevSampleL = smpl + CHECK_SILENCE_BIAS;
					prevSampleR = smpr + CHECK_SILENCE_BIAS;
					pcur += 2;
				}
				if (skipSilenceOnStartSec)
				{
				}
				else if (pskp)
				{
					while (pskp < pcur)
					{
						*(pblk++)= *(pskp++);
						*(pblk++)= *(pskp++);
						pos++;
					}
				}
				else
				{
					pos += remain;
				}
			}
			else
			{
				pos += remain;
			}
		}
		return bufsize;
	}
	unsigned DetectedSilenceSec()
	{
		return detectedSilenceSec;
	}

	int FillBuffer(void *pbuf, unsigned bufbytes, unsigned *writebytes, unsigned isdecoder)
	{
		int end_flag = 0;
		unsigned bufsize = bufbytes >> blockshift;
		unsigned cur = MulDiv(cur_smp, 1000, samplerate);
		int len = bufsize;
		Gen(pbuf, bufsize);

		/* detect end */
		if (!CFGGetPlayInfinitely() || isdecoder)
		{
			if (cur_smp >= len_smp + fad_smp)
			{
				end_flag = 1;
				*writebytes = 0;
				return 1;
			}
			if (cur_smp + len >= len_smp + fad_smp)
			{
				len = len_smp + fad_smp - cur_smp;
				end_flag = 1;
			}
		}

		double cfgvolume = 1;
		bool hascfgvolume = !isdecoder && CFGGetVolume(cfgvolume);
		/* volume */
		if (hasvolume || hascfgvolume)
		{
			float scale = float(volume *  cfgvolume);
			xsfsample_t *p = static_cast<xsfsample_t *>(pbuf);
			unsigned i;
			for (i = cur_smp; i < cur_smp + len; i++)
			{
				float s1, s2;
				s1 = float(p[0]);
				s2 = float(p[1]);
				s1 *= scale;
				if (s1 > float(0x7fff))
					s1 = float(0x7fff);
				else if (s1 < float(-0x8000))
					s1 = float(-0x8000);
				s2 *= scale;
				if (s2 > float(0x7fff))
					s2 = float(0x7fff);
				else if (s2 < float(-0x8000))
					s2 = float(-0x8000);
				p[0] = xsfsample_t(s1);
				p[1] = xsfsample_t(s2);
				p += 2;
			}
		}

		/* fader */
		if ((!CFGGetPlayInfinitely() || isdecoder) && fad_smp && cur_smp + len >= len_smp)
		{
			xsfsample_t *p = static_cast<xsfsample_t *>(pbuf);
			unsigned i;
			for (i = cur_smp; i < cur_smp + len; i++)
			{
				if (i < len_smp)
					;
				else if (i < len_smp + fad_smp)
				{
					int scale = MulDiv(len_smp + fad_smp - i, 0x10000, fad_smp);
					p[0] = (p[0] * scale) >> 16;
					p[1] = (p[1] * scale) >> 16;
				}
				else
				{
					p[0] = 0;
					p[1] = 0;
				}
				p += 2;
			}
		}

		cur_smp += len ;
		*writebytes = len << blockshift;
		return end_flag;
	}

	int Seek(unsigned seek_pos, volatile int *killswitch, void *pbuf, unsigned bufbytes, Out_Module *outMod)
	{
		unsigned bufsize = bufbytes >> blockshift;
		unsigned seek_smp = MulDiv(seek_pos, samplerate, 1000);
		DWORD prevTimer = outMod ? GetTickCount() : 0;
		if (seek_smp < cur_smp)
		{
			lpif->Start(xsfdata.Ptr(), xsfdata.Len());
			SeekTop();
		}
		while (seek_smp - cur_smp > bufsize)
		{
			unsigned cur = MulDiv(cur_smp, 1000, samplerate);
			if (killswitch && *killswitch)
				return 1;
			if (outMod)
			{
				DWORD curTimer = GetTickCount();
				if (DWORD(curTimer - prevTimer) >= 500)
				{
					prevTimer = curTimer;
					outMod->Flush(cur);
				}
			}

			Gen(pbuf, bufsize);
			cur_smp += bufsize;
		}
		if (seek_smp - cur_smp > 0)
		{
			Gen(pbuf, seek_smp - cur_smp);
			cur_smp = seek_smp;
		}
		if (outMod) outMod->Flush(seek_pos);
		return 0;
	}

	void Reinit()
	{
		Free();
		seek_pos = 0;
	}
};

enum REQUEST
{
	REQUEST_STOP,
	REQUEST_SEEK,
	REQUEST_RESTART
};

#if ENABLE_GETEXTENDINFO || ENABLE_GETEXTENDINFOW || ENABLE_TAGWRITER

static const char * const tag_table[] =
{
	"title","title",
	"artist","artist",
	"album","game",
	"albumartist","copyright",
	"year","year",
	"genre","genre",
	"composer","composer",
	"publisher",WINAMPPLUGIN_TAG_XSFBY,
	"disc","disc",
	"track","track",
	"replaygain_album_gain","replaygain_album_gain",
	"replaygain_album_peak","replaygain_album_peak",
	"replaygain_track_gain","replaygain_track_gain",
	"replaygain_track_peak","replaygain_track_peak",

	"GracenoteFileID",0,
	"GracenoteExtData",0,
	"streamtype",0,
	"bpm",0,

	"type","\x01",
	"streamname","\x02",
	"length","\x03",
	"comment","\x04",
	"formatinformation","\x05",

	0,0
};

enum EXINFO
{
	EXINFO_NOT_SUPPORTED = 0,
	EXINFO_SUPPORTED = 1
};

enum XSF_TAG_TYPE
{
	XSF_TAG_TYPE_DIRECTSUPPORT = 0,
	XSF_TAG_TYPE_EXTENDSUPPORT = 1,
	XSF_TAG_TYPE_SYSTEM = 2,
	XSF_TAG_TYPE_UNKNOWN = -1,
	XSF_TAG_TYPE_ALL = -2
};

class TagReader
{
protected:
	typedef struct
	{
		char *ptr;
		const char *tag;
		int taglen;
		int count;
		unsigned need;
		enum XSF_TAG_TYPE mode;
	} xsf_multitagget_work_t;

	static enum XSF_TAG_TYPE xsf_multitagget_type(const char *tag, int taglen)
	{
		const char * const *ptbl;
		if (!taglen)
			return XSF_TAG_TYPE_UNKNOWN;
		if (XSFDRIVER_ISSYSTEMTAG(taglen, tag))
			return XSF_TAG_TYPE_SYSTEM;
		for (ptbl = tag_table; ptbl[0]; ptbl += 2)
		{
			if (!ptbl[1])
				continue;
			if ('\x00' < ptbl[1][0] && ptbl[1][0] <= '\x05')
				continue;
			if (!_strnicmp(tag, ptbl[1], taglen))
				return XSF_TAG_TYPE_DIRECTSUPPORT;
		}
		return XSF_TAG_TYPE_EXTENDSUPPORT;
	}

	static enum XSFTag::enum_callback_returnvalue getmultitaqg_cb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		xsf_multitagget_work_t *pwork = static_cast<xsf_multitagget_work_t *>(pWork);
		enum XSF_TAG_TYPE tagtype = xsf_multitagget_type(pNameTop, pNameEnd - pNameTop);
		if (pwork->mode != XSF_TAG_TYPE_ALL && pwork->mode != tagtype)
			return XSFTag::enum_continue;
		if (pwork->mode != XSF_TAG_TYPE_ALL && tagtype == XSF_TAG_TYPE_DIRECTSUPPORT)
		{
			if (pNameEnd - pNameTop != pwork->taglen || _strnicmp(pNameTop, pwork->tag, pwork->taglen))
				return XSFTag::enum_continue;
			if (pwork->ptr)
			{
				if (0 != pwork->count++)
				{
					pwork->ptr[0] = ' ';
					pwork->ptr[1] = ';';
					pwork->ptr[2] = ' ';
					pwork->ptr += 3;
				}
				if (pValueEnd-pValueTop > 0)
					memcpy(pwork->ptr, pValueTop, pValueEnd-pValueTop);
				pwork->ptr += pValueEnd-pValueTop;
			}
			else
			{
				if (0 != pwork->count++)
					pwork->need += 3;
				pwork->need += pValueEnd-pValueTop;
			}
		}
		else
		{
			if (pwork->ptr)
			{
				if (0 != pwork->count++)
				{
					pwork->ptr[0] = '\x0d';
					pwork->ptr[1] = '\x0a';
					pwork->ptr += 2;
				}
				if (pNameEnd-pNameTop > 0)
					memcpy(pwork->ptr, pNameTop, pNameEnd-pNameTop);
				pwork->ptr += pNameEnd-pNameTop;
				pwork->ptr[0] = '=';
				pwork->ptr += 1;
				if (pValueEnd-pValueTop > 0)
					memcpy(pwork->ptr, pValueTop, pValueEnd-pValueTop);
				pwork->ptr += pValueEnd-pValueTop;
			}
			else
			{
				if (0 != pwork->count++)
				{
					pwork->need += 2;
				}
				pwork->need += pNameEnd-pNameTop + 1 + pValueEnd-pValueTop;
			}
		}
		return XSFTag::enum_continue;
	}

	static wchar_t *TagFunc(const wchar_t * tag, void * p)
	{
		return 0;
	}

	static void TagFree(wchar_t *tag, void *p)
	{
	}

	static int get_tag_formattitleW(HWND hwndWinamp, const wchar_t *fn, wchar_t *title, unsigned l)
	{
		waFormatTitleExtended fmt;
		fmt.filename = fn;
		fmt.useExtendedInfo = 1;
		fmt.spec = NULL;
		fmt.p = NULL;
		fmt.out = title;
		fmt.out_len = l;
		fmt.TAGFUNC = TagFunc;
		fmt.TAGFREEFUNC = TagFree;
		if (l > 0)
		{
			title[0] = 0;
			xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &fmt, IPC_FORMAT_TITLE_EXTENDED);
			if (title[0])
				return 1;
		}
		return 0;
	}

#ifdef UNICODE_INPUT_PLUGIN
#else
	static int get_tag_formattitleA(HWND hwndWinamp, const char *fn, char *title, unsigned l)
	{
		int r = 0;
		wchar_t wbuf[GETFILEINFO_TITLE_LENGTH];
		r = get_tag_formattitleW(hwndWinamp, xsfc::TString(fn), wbuf, GETFILEINFO_TITLE_LENGTH);
		xsfc::TStringM mbuf(wbuf, GETFILEINFO_TITLE_LENGTH);
		icsncpy(title, mbuf, l);
		return r;
	}
#endif

public:

	static xsfc::TString XSFTagGetMulti(xsfc::TAutoBuffer<char> idata, const char *metadata, enum XSF_TAG_TYPE mode)
	{
		xsfc::TString ret;
		xsfc::TSimpleArray<char> tag;
		xsf_multitagget_work_t cbw;
		cbw.mode = mode;
		cbw.tag = metadata;
		cbw.taglen = metadata ? strlen(metadata) : 0;
		cbw.count = 0;
		cbw.need = 0;
		cbw.ptr = 0;
		XSFTag::Enum(getmultitaqg_cb, &cbw, idata.Ptr(), idata.Len());
		if (cbw.need && tag.Resize(cbw.need + 1))
		{
			bool futf8 = XSFTag::Exists("utf8", idata.Ptr(), idata.Len());
			char *ptr = tag.Ptr();
			ptr[cbw.need] = '\x00';
			cbw.count = 0;
			cbw.ptr = ptr;
			XSFTag::Enum(getmultitaqg_cb, &cbw, idata.Ptr(), idata.Len());
			ret = xsfc::TString(futf8, ptr);
		}
		return ret;
	}
#endif

	static void get_tag_title(in_char *title, unsigned l, xsfc::TAutoBuffer<char> idata)
	{
		xsfc::TString tagtitle;
#if ENABLE_GETEXTENDINFO || ENABLE_GETEXTENDINFOW || ENABLE_TAGWRITER
		tagtitle = XSFTagGetMulti(idata, "title", XSF_TAG_TYPE_DIRECTSUPPORT);
#else
		tagtitle = XSFTag::Get("title", idata.Ptr(), idata.Len());
#endif
		if (l > 0) title[0] = 0;
		if (tagtitle[0])
		{
#ifdef UNICODE_INPUT_PLUGIN
			icsncpy(title, tagtitle, l);
#else
			icsncpy(title, xsfc::TStringM(tagtitle), l);
#endif
		}
	}

	static void get_tag_formattitle(HWND hwndWinamp, const in_char *fn, in_char *title, unsigned l, xsfc::TAutoBuffer<char> idata)
	{
		if (l > 0) title[0] = 0;
#ifdef UNICODE_INPUT_PLUGIN
		get_tag_formattitleW(hwndWinamp, fn, title, l);
#else
		get_tag_formattitleA(hwndWinamp, fn, title, l);
#endif
		if (l > 0 && !title[0])
		{
			get_tag_title(title, l, idata);
		}
		if (l > 0 && !title[0])
		{
			icsncpy(title, xsfc::TWin32T<in_char>::ExtractFilename(fn), l);
		}
	}

#if ENABLE_GETEXTENDINFO || ENABLE_GETEXTENDINFOW || ENABLE_TAGWRITER
	class TagReturn
	{
	protected:
		xsfc::TString ret;
		size_t l;
		wchar_t *wbuf;
		char *mbuf;
	public:
		TagReturn()
			: l(0), wbuf(0), mbuf(0)
		{
		}
		TagReturn(char *buf, size_t buflen)
			: l(buflen), wbuf(0), mbuf(buf)
		{
		}
		TagReturn(wchar_t *buf, size_t buflen)
			: l(buflen), wbuf(buf), mbuf(0)
		{
		}
		~TagReturn()
		{
		}
		void Set(const xsfc::TString &value)
		{
			ret = value;
			if (l && wbuf)
				wstrncpychk(wbuf, ret, l);
			if (l && mbuf)
				mstrncpychk(mbuf, xsfc::TStringM(ret), l);
		}
		void Set(bool futf8, const char *tag)
		{
			Set(xsfc::TString(futf8, tag));
		}
		xsfc::TString Get()
		{
			return ret;
		}
	};

	static int getfinfo(xsfc::TAutoBuffer<char> idata, const char *metadata, TagReturn &result)
	{
		bool futf8 = XSFTag::Exists("utf8", idata.Ptr(), idata.Len());
		int taglen = strlen(metadata);
		const char * const *ptbl;
		for (ptbl = tag_table; ptbl[0]; ptbl += 2)
		{
			if (!_strnicmp(metadata, ptbl[0], taglen))
			{
				if (!ptbl[1])
					break;
				else if (ptbl[1][0] == '\x01')
				{
					result.Set(futf8, "0");
				}
				else if (ptbl[1][0] == '\x02')
				{
					result.Set(futf8, "");
				}
				else if (ptbl[1][0] == '\x03')
				{
					char tbuf[16];
					unsigned length = XSFTag::GetLengthMS(idata, CFGGetDefaultLength());
					unsigned fade = XSFTag::GetFadeMS(idata, CFGGetDefaultFade());
#if defined(HAVE_SPRINTF_S)
					sprintf_s(tbuf, sizeof(tbuf), "%10d", (length + fade));
#elif defined(HAVE_SNPRINTF)
					snprintf(tbuf, sizeof(tbuf), "%10d", (length + fade));
#elif defined(HAVE_SNPRINTF_)
					_snprintf(tbuf, sizeof(tbuf), "%10d", (length + fade));
#else
					sprintf(tbuf, "%10d", (length + fade));
#endif
					result.Set(futf8, tbuf);
				}
				else if (ptbl[1][0] == '\x04')
				{
					xsfc::TString tag = XSFTagGetMulti(idata, 0, XSF_TAG_TYPE_EXTENDSUPPORT);
					result.Set(futf8, xsfc::TStringM(futf8, tag));
				}
				else if (ptbl[1][0] == '\x05')
				{
					xsfc::TString tag = XSFTagGetMulti(idata, 0, XSF_TAG_TYPE_SYSTEM);
					result.Set(futf8, xsfc::TStringM(futf8, tag));
				}
				else
				{
					xsfc::TString tag = XSFTagGetMulti(idata, ptbl[1], XSF_TAG_TYPE_DIRECTSUPPORT);
					result.Set(futf8, xsfc::TStringM(futf8, tag));
				}
				return EXINFO_SUPPORTED;
			}
		}
		return EXINFO_NOT_SUPPORTED;
	}
#endif

#if ENABLE_GETEXTENDINFO || ENABLE_GETEXTENDINFOW
	static bool getfinfo_map(xsfc::TAutoBuffer<char> &idata, const wchar_t *fn)
	{
		idata = xsfc::TWin32::Map(fn);
		return idata.Ptr() != 0;
	}
#endif

};

class WinampPlugin
{	
protected:
	static XSFDriver xsfdrv;
	static HMODULE m_hDLL;
	static HANDLE m_hThread;
	static HANDLE m_hEvent;
	static HANDLE m_hEvent2;
	static volatile LONG m_lRequest;
	static int m_paused;
	static in_char m_title[GETFILEINFO_TITLE_LENGTH];
	static HANDLE m_hFindWindowThread;

public:
	static In_Module mod;

	static void Init(HINSTANCE hinstDLL)
	{
		m_hDLL = (HMODULE)hinstDLL;
	}

	static HMODULE ConfigInit()
	{
		winamp_config_load(mod.hMainWindow);
		return m_hDLL;
	}

protected:
	static DWORD __stdcall PlayWorkerThread(LPVOID argp)
	{
		try
		{
			unsigned char buffer[SIZE_OF_BUFFER * 2];
			unsigned seek_pos;
			unsigned end_flag;

			int request1;

			xsfdrv.Start();

			m_paused = 0;
			request1 = 0;
			::ResetEvent(m_hEvent);

			end_flag = 0;
			do
			{
				unsigned timeout;
				unsigned writebytes;
				int len;
				unsigned detectsilencesec = CFGGetDetectSilenceSec();
				unsigned playinfinitely = CFGGetPlayInfinitely();
				unsigned cur = MulDiv(xsfdrv.cur_smp, 1000, xsfdrv.samplerate);
				bool haslength = xsfdrv.haslength;
				end_flag = xsfdrv.FillBuffer(buffer, SIZE_OF_BUFFER, &writebytes, 0);

				if (!playinfinitely && !haslength && detectsilencesec && detectsilencesec <= xsfdrv.DetectedSilenceSec())
					end_flag = 1;

				len = writebytes >> xsfdrv.blockshift;
				if (len == 0)
				{
					end_flag = 1;
					continue;
				}

				if (len >= 576)
				{
					mod.SAAddPCMData(buffer, xsfdrv.ch, xsfdrv.samplebits, cur);
					mod.VSAAddPCMData(buffer, xsfdrv.ch, xsfdrv.samplebits, cur);
				}
				if(mod.dsp_isactive())
					len = mod.dsp_dosamples((xsfsample_t *)(buffer), len, xsfdrv.samplebits, xsfdrv.ch, xsfdrv.samplerate);
				len <<= xsfdrv.blockshift;
				timeout = 0;
				request1 = 0;
				do
				{
					if (::WaitForSingleObject(m_hEvent, timeout) != WAIT_TIMEOUT)
					{
						request1 = ::InterlockedExchange((LPLONG)&m_lRequest, 0);
						break;
					}
					timeout = 10;
				} while (mod.outMod->CanWrite() < len);
				if (request1 & (1 << REQUEST_STOP))
					break;
				if (request1 & (1 << REQUEST_RESTART))
				{
					seek_pos = 0;
					request1 |= (1 << REQUEST_SEEK);
				}
				else if (request1 & (1 << REQUEST_SEEK))
				{
					seek_pos = xsfdrv.seek_pos;
					if (!seek_pos)
						request1 &= ~(1 << REQUEST_SEEK);
				}
				if (request1 & (1 << REQUEST_SEEK))
				{
					xsfdrv.seek_kil = 0;
					xsfdrv.Seek(seek_pos, &xsfdrv.seek_kil, buffer, SIZE_OF_BUFFER, mod.outMod);
					xsfdrv.seek_pos = 0;
					continue;
				}
				if (mod.outMod->CanWrite() >= len)
					mod.outMod->Write((char *)(buffer), len);
			} while (!end_flag);

			if (end_flag)
			{
				unsigned timeout = 0;
				do
				{
					if (::WaitForSingleObject(m_hEvent, timeout) != WAIT_TIMEOUT)
					{
						int request2 = ::InterlockedExchange((LPLONG)&m_lRequest, 0);
						if (request2 & (1 << REQUEST_STOP))
						{
							request1 |= (1 << REQUEST_STOP);
							break;
						}
					}
					timeout = 10;
				} while (mod.outMod->IsPlaying());
			}

			void killSoundView();
			killSoundView();
			xsfdrv.Free();
			
			if (!(request1 & (1 << REQUEST_STOP)))
			{
				xsfc::TWin32::WndMsgPost(mod.hMainWindow, WM_WA_MPEG_EOF);
			}
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
		return 0;
	}

	static int __cdecl play(const in_char *fn)
	{
		try
		{
			DWORD dwThreadId;
			int maxlatency;

			if (m_hThread)
				return 1;

			if (!m_hEvent)
				return 1;

			xsfdrv.Free();

			ConfigInit();

			if (!xsfdrv.Init(m_hDLL) || !xsfdrv.Load(xsfc::TString(fn)))
			{
				xsfdrv.Free();
				return 1;
			}

			if (!XSFTag::GetVolume(&xsfdrv.volume, "replaygain_album_gain", xsfdrv.xsfdata, 1))
				xsfdrv.hasvolume = 1;
			else if (!XSFTag::GetVolume(&xsfdrv.volume, "replaygain_track_gain", xsfdrv.xsfdata, 1))
				xsfdrv.hasvolume = 1;

			TagReader::get_tag_formattitle(mod.hMainWindow, fn, m_title, GETFILEINFO_TITLE_LENGTH, xsfdrv.xsfdata);

			maxlatency = mod.outMod->Open(xsfdrv.samplerate, xsfdrv.ch, xsfdrv.samplebits, 0, 0);
			if (maxlatency < 0)
			{
				xsfdrv.Free();
				return 1;
			}

			xsfc::TWin32::WndMsgSend(mod.hMainWindow, WM_WA_IPC, 0, IPC_UPDTITLE);

			mod.SetInfo(xsfdrv.samplerate * xsfdrv.samplebits * xsfdrv.ch / 1000, xsfdrv.samplerate / 1000, xsfdrv.ch, 0);
			mod.SAVSAInit(maxlatency, xsfdrv.samplerate);
			mod.VSASetInfo(xsfdrv.samplerate, xsfdrv.ch);
			mod.outMod->SetVolume(-666);
			m_hThread = ::CreateThread(NULL, 0, PlayWorkerThread, NULL, 0, &dwThreadId);
			if (!m_hThread)
			{
				mod.SAVSADeInit();
				mod.outMod->Close();
				xsfdrv.Free();
				return 1;
			}
			return 0;
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
		return 1;
	}

	static void Request(enum REQUEST request_code)
	{
		if (m_hEvent)
		{
			m_lRequest |= 1 << request_code;
			::SetEvent(m_hEvent);
		}
	}

	static void __cdecl stop(void)
	{
		if (m_hThread)
		{
			do
			{
				xsfdrv.seek_kil = 1;
				Request(REQUEST_STOP);
				
				MSG msg;
				while( PeekMessage( &msg, 0, 0, 0, PM_NOREMOVE ) )
					if( GetMessage( &msg, 0,  0, 0)>0 ) {
						TranslateMessage(&msg);
						DispatchMessage(&msg);
					}


			} while (::WaitForSingleObject(m_hThread, 20) == WAIT_TIMEOUT);
			::CloseHandle(m_hThread);
			m_hThread = NULL;
			mod.SAVSADeInit();
			mod.outMod->Close();
		}
	}

	typedef struct
	{
		bool found;
		HWND hwnd;
	} ftw_work_t;

	static BOOL CALLBACK ftw_cb(HWND hwnd, LPARAM lParam)
	{
		ftw_work_t *pftww = (ftw_work_t *)lParam;
		char buf[32];
		if (::GetClassNameA(hwnd, buf, sizeof(buf)))
		{
			if (!::lstrcmpA(buf, "Winamp v1.x"))
			{
				pftww->found = true;
				pftww->hwnd = hwnd;
				return FALSE;
			}
		}
		return TRUE;
	}

	static DWORD CALLBACK FindThreadWindow(LPVOID argp)
	{
		ftw_work_t ftww = { false, 0 };
		DWORD dwThreadId = (DWORD)argp;
		do
		{
			Sleep(500);
			::EnumThreadWindows(dwThreadId, ftw_cb, (LPARAM)&ftww);
		}
		while (!ftww.found && (*(volatile HANDLE *)&m_hEvent) != 0);
		if (ftww.found)
			winamp_config_add_prefs(ftww.hwnd);
		return 0;
	}

	static void closeevent(void)
	{
		if (m_hEvent)
		{
			::CloseHandle(m_hEvent);
			m_hEvent = 0;
		}
	}
	static void termfindwindowthread(void)
	{
		if (m_hFindWindowThread)
		{
			TerminateThread(m_hFindWindowThread, 1);
			::CloseHandle(m_hFindWindowThread);
			m_hFindWindowThread = 0;
		}
	}

	static void __cdecl init(void)
	{
		try
		{
			m_lRequest = 0;
			m_hThread = 0;
			m_title[0] = '\0';
			closeevent();
			m_hEvent = ::CreateEventA(NULL, FALSE, FALSE, NULL);
			xsfdrv.Reinit();
			ConfigInit();
			if (mod.hMainWindow)
				winamp_config_add_prefs(mod.hMainWindow);
			else
			{
				DWORD dwThreadId;
				termfindwindowthread();
				m_hFindWindowThread = ::CreateThread(NULL, 0, FindThreadWindow, (LPVOID)::GetCurrentThreadId(), 0, &dwThreadId);
			}
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
	}

	static void __cdecl quit(void)
	{
		try
		{
			termfindwindowthread();
			winamp_config_remove_prefs(mod.hMainWindow);
			xsfdrv.Free();
			closeevent();
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
	}

	static void __cdecl getfileinfo(const in_char *fn, in_char *title, int *length_in_ms)
	{
		try
		{
			if (title) *title = '\0';
			if (length_in_ms) *length_in_ms = CFGGetDefaultLength() + CFGGetDefaultFade();
			if (!fn || !*fn)
			{
				if (title)
					icsncpy(title, m_title, GETFILEINFO_TITLE_LENGTH);
				if (length_in_ms)
					*length_in_ms = xsfdrv.length_in_ms + xsfdrv.fade_in_ms;
				return;
			}
			xsfc::TAutoBuffer<char> idata = xsfc::TWin32::Map(xsfc::TString(fn));
			if (idata.Ptr())
			{
				if (title)
				{
					TagReader::get_tag_formattitle(mod.hMainWindow, fn, title, GETFILEINFO_TITLE_LENGTH, idata);
				}

				if (length_in_ms) 
				{
					unsigned length = XSFTag::GetLengthMS(idata, CFGGetDefaultLength());
					unsigned fade = XSFTag::GetFadeMS(idata, CFGGetDefaultFade());
					*length_in_ms = length + fade;
				}
			}
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
	}

	static void __cdecl pause(void) { m_paused = 1; mod.outMod->Pause(1); }
	static void __cdecl unpause(void) { m_paused = 0; mod.outMod->Pause(0); }
	static int __cdecl getoutputtime(void) { return mod.outMod->GetOutputTime(); }
	static void __cdecl setvolume(int volume) { mod.outMod->SetVolume(volume); }
	static void __cdecl setpan(int pan) { mod.outMod->SetPan(pan); }
	static void __cdecl config(HWND hwnd)
	{
		winamp_config_dialog(mod.hMainWindow, hwnd);
	}
	static void __cdecl about(HWND hwnd) { MessageBoxA(hwnd, WINAMPPLUGIN_COPYRIGHT , WINAMPPLUGIN_NAME, MB_OK); }
	static int __cdecl infobox(const in_char *fn, HWND hwnd)
	{
		try
		{
			/* This function is obsolete in Winamp5. */
			/* Current version of 'in_zip.dll' and XMPlay cannot handle 'winampUseUnifiedFileInfoDlg' function. */
			xsfc::TAutoBuffer<char> idata = xsfc::TWin32::Map(xsfc::TString(fn));
			if (idata.Ptr())
			{
				int futf8 = XSFTag::Exists("utf8", idata.Ptr(), idata.Len());
				xsfc::TString tag = TagReader::XSFTagGetMulti(idata, 0, XSF_TAG_TYPE_ALL);
				if (tag[0])
				{
#ifdef UNICODE_INPUT_PLUGIN
					MessageBoxW(hwnd, tag, xsfc::TWin32T<in_char>::ExtractFilename(fn), MB_OK);
#else
					MessageBoxA(hwnd, xsfc::TStringM(tag), xsfc::TWin32T<in_char>::ExtractFilename(fn), MB_OK);
#endif
					return 1;
				}
			}
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
		return 0;
	}
	static int __cdecl isourfile(const in_char *fn) { return 0; }
	static void __cdecl eq_set(int on, char data[10], int preamp) {}
	static int __cdecl ispaused(void) { return m_paused; }
	static int __cdecl getlength(void) { return xsfdrv.length_in_ms + xsfdrv.fade_in_ms; }
	static void __cdecl setoutputtime(int time_in_ms)
	{
		xsfdrv.seek_kil = 1;
		if (time_in_ms)
		{
			xsfdrv.seek_pos = time_in_ms;
			Request(REQUEST_SEEK);
		}
		else
		{
			Request(REQUEST_RESTART);
		}
	}

};

XSFDriver WinampPlugin::xsfdrv;
HMODULE WinampPlugin::m_hDLL = 0;
HANDLE WinampPlugin::m_hThread = 0;
HANDLE WinampPlugin::m_hEvent = 0;
volatile LONG WinampPlugin::m_lRequest = 0;
int WinampPlugin::m_paused = 0;
in_char WinampPlugin::m_title[GETFILEINFO_TITLE_LENGTH];
HANDLE WinampPlugin::m_hFindWindowThread = 0;

In_Module WinampPlugin::mod =
{
	IN_VER,
	WINAMPPLUGIN_NAME,
	0,	/* hMainWindow */
	0,	/* hDllInstance */
	WINAMPPLUGIN_EXTS,
	1,	/* is_seekable */
	1,	/* UsesOutputPlug */
	WinampPlugin::config,
	WinampPlugin::about,
	WinampPlugin::init,
	WinampPlugin::quit,
	WinampPlugin::getfileinfo,
	WinampPlugin::infobox,
	WinampPlugin::isourfile,
	WinampPlugin::play,
	WinampPlugin::pause,
	WinampPlugin::unpause,
	WinampPlugin::ispaused,
	WinampPlugin::stop,

	WinampPlugin::getlength,
	WinampPlugin::getoutputtime,
	WinampPlugin::setoutputtime,
	WinampPlugin::setvolume,
	WinampPlugin::setpan,

	/* pointers filled in by winamp */
	0,0,0,0,0,0,0,0,0,	/* vis stuff */
	0,0,	/* dsp */

	WinampPlugin::eq_set,	/* EQ, not used */
	NULL,	/* setinfo */
	0		/* out_mod */
};


extern "C" In_Module * __cdecl winampGetInModule2(void)
{
	return &WinampPlugin::mod;
}

#if ENABLE_GETEXTENDINFO
extern "C" int __cdecl winampGetExtendedFileInfo(char *filename, char *metadata, char *ret, int retlen)
{
	int e = EXINFO_NOT_SUPPORTED;
	try
	{
		xsfc::TAutoBuffer<char> idata;
		WinampPlugin::ConfigInit();
		if (filename && *filename && TagReader::getfinfo_map(idata, xsfc::TString(filename)))
		{
			TagReader::TagReturn result(ret, retlen);
			e = TagReader::getfinfo(idata, metadata, result);
//			if (ret && retlen > 0 && !ret[0] && !strcmp(metadata, "title"))
//			{
//				mstrncpychk(ret, xsfc::TWin32T<char>::ExtractFilename(filename), retlen);
//				e = EXINFO_SUPPORTED;
//			}
		}
		else if (!strcmp(metadata, "type"))
		{
			if (retlen > 0) ret[0] = '0';
			if (retlen > 1) ret[1] = 0;
			e = EXINFO_SUPPORTED;
		}
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return e;
}
#endif

#if ENABLE_GETEXTENDINFOW
extern "C" int __cdecl winampGetExtendedFileInfoW(WCHAR *filename, char *metadata, WCHAR *ret, int retlen)
{
	int e = EXINFO_NOT_SUPPORTED;
	try
	{
		xsfc::TAutoBuffer<char> idata;
		WinampPlugin::ConfigInit();
		if (filename && *filename && TagReader::getfinfo_map(idata, filename))
		{
			TagReader::TagReturn result(ret, retlen);
			e = TagReader::getfinfo(idata, metadata, result);
//			if (ret && retlen > 0 && !ret[0] && !strcmp(metadata, "title"))
//			{
//				wstrncpychk(ret, xsfc::TWin32T<wchar_t>::ExtractFilename(filename), retlen);
//				e = EXINFO_SUPPORTED;
//			}
		}
		else if (!strcmp(metadata, "type"))
		{
			if (retlen > 0) ret[0] = L'0';
			if (retlen > 1) ret[1] = 0;
			e = EXINFO_SUPPORTED;
		}
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return e;
}
#endif

#if ENABLE_TAGWRITER

class TagWriter
{
protected:

	class tagwriternode
	{
	public:
		class tagwriternode *pnext;
		xsfc::TStringM tag;
		xsfc::TStringM valueu8;

		tagwriternode() throw()
			: pnext(0)
		{
		}
		~tagwriternode()
		{
		}

		static tagwriternode *get_node(tagwriternode *ptop, const char *tag)
		{
			tagwriternode *pcur = ptop;
			while (pcur)
			{
				tagwriternode *pnext = pcur->pnext;
				if (pcur->tag && !strcmp(pcur->tag, tag))
					break;
				pcur = pnext;
			}
			return pcur;
		}
	};

	class  tagwriterfile
	{
	public:
		xsfc::TStringM filenameu8;
		tagwriternode *ptop;
		tagwriterfile() throw()
			: ptop(0)
		{
		}
		~tagwriterfile()
		{
			tagwriternode *pcur = ptop;
			while (pcur)
			{
				tagwriternode *pnext = pcur->pnext;
				delete pcur;
				pcur = pnext;
			}
		}
	};

	static tagwriterfile * volatile ptarget;
	static CRITICAL_SECTION cs;

	static tagwriterfile *GetTarget(tagwriterfile *pnewtarget)
	{
		tagwriterfile *poldtarget;
		::EnterCriticalSection(&cs);
		poldtarget = ptarget;
		ptarget = pnewtarget;
		::LeaveCriticalSection(&cs);
		return poldtarget;
	}

	static void FreeFile(tagwriterfile *pfile)
	{
		if (pfile)
		{
			delete pfile;
		}
	}
	TagWriter()
	{
	}
	~TagWriter()
	{
	}

	static xsfc::TStringM Get(tagwriternode *ptop, xsfc::TAutoBuffer<char> idata, const char *tag)
	{
		tagwriternode *pnode = tagwriternode::get_node(ptop, tag);
		xsfc::TStringM valueu8;
		if (pnode)
			valueu8 = pnode->valueu8;
		else
		{
			TagReader::TagReturn result;
			TagReader::getfinfo(idata, tag, result);
			valueu8 = xsfc::TStringM(true, result.Get());
		}
		return valueu8;
	}

	static void WriteSingle(HANDLE h, bool futf8, const char *pname, unsigned uname, const char *pvalue, unsigned uvalue)
	{
		if (uname && uvalue)
		{
			DWORD dwNumberOfByteWritten;
			if (futf8)
			{
				WriteFile(h, pname, uname, &dwNumberOfByteWritten, NULL);
				WriteFile(h, "=", 1, &dwNumberOfByteWritten, NULL);
				WriteFile(h, pvalue, uvalue, &dwNumberOfByteWritten, NULL);
				WriteFile(h, "\x0a", 1, &dwNumberOfByteWritten, NULL);
			}
			else
			{
				xsfc::TStringM name(xsfc::TString(true, pname, uname));
				xsfc::TStringM value(xsfc::TString(true, pvalue, uvalue));
				WriteFile(h, name, strlen(name), &dwNumberOfByteWritten, NULL);
				WriteFile(h, "=", 1, &dwNumberOfByteWritten, NULL);
				WriteFile(h, value, strlen(value), &dwNumberOfByteWritten, NULL);
				WriteFile(h, "\x0a", 1, &dwNumberOfByteWritten, NULL);
			}
		}
	}

	static const char *Split(const char *p)
	{
		int l;
		for (l = 0; p[l]; l++)
		{
			if (p[l + 0] == ' ' && p[l + 1] == ';' && p[l + 2] == ' ')
				break;
		}
		return p + l;
	}

	static void WriteMulti(HANDLE h, bool futf8, const char *tag, const char *value)
	{
		int taglen = strlen(tag);
		if (!value) return;
		do
		{
			const char *top = value;
			value = Split(value);
			WriteSingle(h, futf8, tag, taglen, top, value-top);
			if (*value) value += 3;
		} while (*value);
	}

	typedef struct
	{
		HANDLE h;
		bool futf8;
		char ignoresystemtag;
	} writetagmultiline_work_t;

	static XSFTag::enum_callback_returnvalue tagwriter_wmlcb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		writetagmultiline_work_t *pwork = static_cast<writetagmultiline_work_t *>(pWork);
		if (pwork->ignoresystemtag && XSFDRIVER_ISSYSTEMTAG(pNameEnd - pNameTop, pNameTop))
			return XSFTag::enum_continue;
		WriteSingle(pwork->h, pwork->futf8, pNameTop, pNameEnd-pNameTop, pValueTop, pValueEnd-pValueTop);
		return XSFTag::enum_continue;
	}

	static void WriteMultiLine(HANDLE h, bool futf8, const char *value)
	{
		writetagmultiline_work_t cbw;
		cbw.h = h;
		cbw.futf8 = futf8;
		cbw.ignoresystemtag = 1;
		if (!value) return;
		XSFTag::EnumRaw(tagwriter_wmlcb, &cbw, value, strlen(value));
	}

	static void WriteSysMultiLine(HANDLE h, bool futf8, const char *value)
	{
		writetagmultiline_work_t cbw;
		cbw.h = h;
		cbw.futf8 = futf8;
		cbw.ignoresystemtag = 0;
		if (!value) return;
		XSFTag::EnumRaw(tagwriter_wmlcb, &cbw, value, strlen(value));
	}

	static int WriteAll(HANDLE h, tagwriternode *ptop, xsfc::TAutoBuffer<char> idata)
	{
		const char * const *ptbl;
		bool futf8 = false;
		xsfc::TStringM comment = Get(ptop, idata, "comment");
		xsfc::TStringM formatinformation = Get(0, idata, "formatinformation");
		xsfc::TString utf8 = XSFTag::GetRaw("utf8", comment.GetM(), strlen(comment));
		if (utf8[0]) futf8 = true;
		if (formatinformation[0])
			WriteSysMultiLine(h, futf8, formatinformation);
		if (comment[0])
			WriteMultiLine(h, futf8, comment);
		for (ptbl = tag_table; ptbl[0]; ptbl += 2)
		{
			if (!ptbl[1])
				continue;
			if ('\x00' < ptbl[1][0] && ptbl[1][0] <= '\x05')
				continue;
			WriteMulti(h, futf8, ptbl[1], Get(ptop, idata, ptbl[0]));
		}
		return EXINFO_SUPPORTED;
	}

	static xsfc::TString fnextend(const wchar_t *fn)
	{
		if (fn && fn[0] == L'\\' && fn[1] == L'\\' && fn[2] != L'?')
			return xsfc::TString("\\\\?\\UNC") + (fn + 1);
		if (fn && fn[0] && fn[1] == L':' && fn[2] == L'\\')
			return xsfc::TString("\\\\?\\") + fn;
		return xsfc::TString();
	}

	static HANDLE createfileW(const WCHAR *fn)
	{
		HANDLE h = INVALID_HANDLE_VALUE;
		if (xsfc::TWin32::IsUnicodeSupportedOS())
		{
			xsfc::TString fnw = fnextend(fn);
			h = CreateFileW(fnw[0] ? fnw : fn, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		else
		{
			h = CreateFileA(xsfc::TStringM(fn), GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		return h;
	}

	static int WriteTarget(tagwriterfile *pfile)
	{
		int e = EXINFO_NOT_SUPPORTED;
		if (pfile && pfile->filenameu8 && pfile->ptop)
		{
			xsfc::TString fnw(true, pfile->filenameu8); 
			xsfc::TAutoBuffer<char> idata = xsfc::TWin32::Map(fnw);
			if (idata.Ptr())
			{
				DWORD dwTagOffset = XSFTag::SearchRaw(idata.Ptr(), idata.Len());
				if (dwTagOffset)
				{
					HANDLE h = createfileW(fnw);
					if (h != INVALID_HANDLE_VALUE)
					{
						DWORD dwNumberOfByteWritten;
						WriteFile(h, idata.Ptr(), dwTagOffset, &dwNumberOfByteWritten, NULL);
						WriteFile(h, "[TAG]", 5, &dwNumberOfByteWritten, NULL);
						e = WriteAll(h, pfile->ptop, idata);
						::CloseHandle(h);
					}
				}
			}
		}
		return e;
	}

public:
	static int Write()
	{
		int e = EXINFO_NOT_SUPPORTED;
		tagwriterfile *ptarget = GetTarget(0);
		if (ptarget)
		{
			e = WriteTarget(ptarget);
			FreeFile(ptarget);
		}
		return e;
	}

	static int SetFInfo(xsfc::TStringM filenameu8, xsfc::TStringM tag, xsfc::TStringM valueu8)
	{
		tagwriterfile *pfile = GetTarget(0);
		if (pfile)
		{
			if (strcmp(pfile->filenameu8, filenameu8))
			{
				FreeFile(pfile);
				pfile = 0;
			}
		}
		if (!pfile)
		{
			pfile = new(xsfc::nothrow) tagwriterfile;
			if (pfile)
			{
				pfile->filenameu8 = filenameu8;
				pfile->ptop = 0;
			}
		}

		if (pfile)
		{
			tagwriternode *pnode = tagwriternode::get_node(pfile->ptop, tag);
			if (!pnode)
			{
				pnode = new(xsfc::nothrow) tagwriternode;
				if (pnode)
				{
					pnode->pnext = pfile->ptop;
					pnode->tag = tag;
					pfile->ptop = pnode;
				}
			}
			if (pnode)
			{
				if (strcmp(pnode->valueu8, valueu8))
					pnode->valueu8 = valueu8;
			}
		}

		pfile = GetTarget(pfile);
		FreeFile(pfile);
		return EXINFO_SUPPORTED;
	}

	static void Init()
	{
		ptarget = 0;
		::InitializeCriticalSection(&cs);
	}
	static void Term()
	{
		FreeFile(GetTarget(0));
		::DeleteCriticalSection(&cs);
	}
};

TagWriter::tagwriterfile * volatile TagWriter::ptarget = 0;
CRITICAL_SECTION TagWriter::cs;

extern "C" int __cdecl winampSetExtendedFileInfo(char *filename,char *metadata,char *ret,int retlen)
{
	try
	{
		xsfc::TStringM fnu8(true, xsfc::TString(filename));
		xsfc::TStringM tgu8(true, xsfc::TString(metadata));
		xsfc::TStringM tvu8(true, xsfc::TString(ret));
		return TagWriter::SetFInfo(fnu8, tgu8, tvu8);
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return EXINFO_NOT_SUPPORTED;
}
extern "C" int __cdecl winampSetExtendedFileInfoW(WCHAR *filename,char *metadata,WCHAR *ret,int retlen)
{
	try
	{
		xsfc::TStringM fnu8(true, filename);
		xsfc::TStringM tgu8(true, xsfc::TString(metadata));
		xsfc::TStringM tvu8(true, ret);
		return TagWriter::SetFInfo(fnu8, tgu8, tvu8);
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return EXINFO_NOT_SUPPORTED;
}
extern "C" int __cdecl winampWriteExtendedFileInfo(void)
{
	try
	{
		return TagWriter::Write();
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return EXINFO_NOT_SUPPORTED;
}
#endif

#if ENABLE_GETEXTENDINFO || ENABLE_GETEXTENDINFOW || ENABLE_TAGWRITER
/*
	return 1 if you want winamp to show it's own file info dialogue, 0 if you want to show your own (via In_Module.InfoBox)
	if returning 1, remember to implement winampGetExtendedFileInfo("formatinformation")!
*/
extern "C" int __cdecl winampUseUnifiedFileInfoDlg(const wchar_t * fn)
{
	return EXINFO_SUPPORTED;
}
/*
	should return a child window of 513x271 pixels (341x164 in msvc dlg units), or return NULL for no tab.
	Fill in name (a buffer of namelen characters), this is the title of the tab (defaults to "Advanced").
	filename will be valid for the life of your window. n is the tab number. This function will first be
	called with n == 0, then n == 1 and so on until you return NULL (so you can add as many tabs as you like).
	The window you return will recieve WM_COMMAND, IDOK/IDCANCEL messages when the user clicks OK or Cancel.
	when the user edits a field which is duplicated in another pane, do a SendMessage(GetParent(hwnd),WM_USER,(WPARAM)L"fieldname",(LPARAM)L"newvalue");
	this will be broadcast to all panes (including yours) as a WM_USER.
*/
extern "C" HWND __cdecl winampAddUnifiedFileInfoPane(int n, const wchar_t * filename, HWND parent, wchar_t *name, size_t namelen)
{
	return NULL;
}
#endif

#if ENABLE_EXTENDREADER
static void * __cdecl winampGetExtendedRead_opensub(xsfc::TString fn, int *size, int *bps, int *nch, int *srate)
{
	try
	{
		xsfc::TAutoPtr<XSFDriver> pDrv(new(xsfc::nothrow) XSFDriver);
		if (!pDrv)
			return NULL;
		if (!pDrv->Init(WinampPlugin::ConfigInit()) || !pDrv->Load(fn))
		{
			pDrv->Free();
			return NULL;
		}
		pDrv->Start();
		if (size) *size = (pDrv->len_smp + pDrv->fad_smp) << pDrv->blockshift; 
		if (bps) *bps = pDrv->samplebits;
		if (nch) *nch = pDrv->ch;
		if (srate) *srate = pDrv->samplerate;
		return pDrv.Detach();
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return NULL;
}
extern "C" void * __cdecl winampGetExtendedRead_openW(const wchar_t *fn, int *size, int *bps, int *nch, int *srate)
{
	return winampGetExtendedRead_opensub(xsfc::TString(fn), size, bps, nch, srate);
}
extern "C" void * __cdecl winampGetExtendedRead_open(const char *fn, int *size, int *bps, int *nch, int *srate)
{
	return winampGetExtendedRead_opensub(xsfc::TString(fn), size, bps, nch, srate);
}
static int winampGetExtendedRead_getData_seeksub(XSFDriver *pDrv, unsigned seek_pos, volatile int *killswitch)
{
	char buf[576 * 4];
	return pDrv->Seek(seek_pos, killswitch, buf, sizeof(buf), 0);
}

extern "C" int __cdecl winampGetExtendedRead_getData(void *handle, char *dest, int len, volatile int *killswitch)
{
	try
	{
		XSFDriver *pDrv = static_cast<XSFDriver *>(handle);
		const int block = 576 * 4;
		int genlen = 0;
		int endflag = 0;
		if (!pDrv)
			return 0;
		if (pDrv->seek_req)
		{
			unsigned seek_pos = pDrv->seek_pos;
			pDrv->seek_req = 0;
			if (winampGetExtendedRead_getData_seeksub(pDrv, seek_pos, killswitch))
				return genlen;
		}
		while (genlen < len && !endflag)
		{
			unsigned writebytes;
			endflag = pDrv->FillBuffer(dest + genlen, ((len - genlen) > block) ? block : (len - genlen), &writebytes, 1);
			genlen += writebytes;
			if (killswitch && *killswitch)
				break;
		}
		return genlen;
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return 0;
}
extern "C" int __cdecl winampGetExtendedRead_setTime(void *handle, int millisecs)
{
	try
	{
		XSFDriver *pDrv = static_cast<XSFDriver *>(handle);
		if (pDrv)
		{
			pDrv->seek_pos = millisecs;
			pDrv->seek_req = 1;
			return 1;
		}
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
	return 0;
}
extern "C" void __cdecl winampGetExtendedRead_close(void *handle)
{
	try
	{
		XSFDriver *pDrv = static_cast<XSFDriver *>(handle);
		if (pDrv)
		{
			pDrv->Free();
			delete pDrv;
		}
	}
	catch (xsfc::EShortOfMemory ex)
	{
	}
}
#endif

}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
	if (fdwReason == DLL_PROCESS_ATTACH)
	{
		try
		{
#if defined(_MSC_VER) && defined(_DEBUG)
			_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_CHECK_ALWAYS_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
			WinampPlugin::Init(hinstDLL);
			winamp_config_init(hinstDLL);
#if ENABLE_TAGWRITER
			TagWriter::Init();
#endif
			DisableThreadLibraryCalls(hinstDLL);
		}
		catch (xsfc::EShortOfMemory ex)
		{
			return FALSE;
		}
	}
	else if (fdwReason == DLL_PROCESS_DETACH)
	{
#if ENABLE_TAGWRITER
		try
		{
			TagWriter::Term();
		}
		catch (xsfc::EShortOfMemory ex)
		{
		}
#endif
	}
	return TRUE;
}

