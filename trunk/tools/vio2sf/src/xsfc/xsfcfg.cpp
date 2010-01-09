#define WIN32_LEAN_AND_MEAN
#include "leakchk.h"

#include <windows.h>
#include <windowsx.h>

#include "xsfcfg.h"
#include "tagget.h"
#include "../pversion.h"
#include "xsfui.rh"

#include "vio2sf/desmume/spu.h"

unsigned long dwChannelMute[4] = { 0, 0, 0, 0 };

unsigned long dwInterpolation;

namespace
{

const int VolumeBase = 16;

unsigned long dwPlayInfinitely;
unsigned long dwSkipSilenceOnStartSec;
unsigned long dwDetectSilenceSec;
unsigned long dwDefaultLength;
unsigned long dwDefaultFade;
double dVolume;

xsfc::TString sDefaultLengthC(L"1:55");
xsfc::TString sDefaultFadeC(L"5");
xsfc::TString sDefaultLength;
xsfc::TString sDefaultFade;

typedef struct
{
	int devch;
	const char *devname;
} CHANNELMAP;

const CHANNELMAP chmap[] = XSFDRIVER_CHANNELMAP;

#ifdef XSFDRIVER_EXTENDPARAM1NAME
xsfc::TString sExtendParam1C(XSFDRIVER_EXTENDPARAM1DEFAULT);
xsfc::TString sExtendParam1;
#endif

#ifdef XSFDRIVER_EXTENDPARAM2NAME
xsfc::TString sExtendParam2C(XSFDRIVER_EXTENDPARAM2DEFAULT);
xsfc::TString sExtendParam2;
#endif

}

unsigned long CFGGetChannelMute(int page)
{
	return (page < 4) ? dwChannelMute[page] : ~unsigned long(0);
}

void CFGSetChannelMute(int ch, bool mute)
{
	int page = ch >> 5;
	if (page < 4)
	{
		if (mute)
			dwChannelMute[page] |= ((unsigned long(1)) << (ch & 0x1f));
		else
			dwChannelMute[page] &= ~((unsigned long(1)) << (ch & 0x1f));
	}
}


unsigned CFGGetPlayInfinitely(void)
{
	return dwPlayInfinitely;
}
unsigned CFGGetSkipSilenceOnStartSec(void)
{
	return dwSkipSilenceOnStartSec;
}
unsigned CFGGetDetectSilenceSec(void)
{
	return dwDetectSilenceSec;
}
unsigned CFGGetDefaultLength(void)
{
	return dwDefaultLength;
}
unsigned CFGGetDefaultFade(void)
{
	return dwDefaultFade;
}
bool CFGGetVolume(double &vol)
{
	int iVolume = int(dVolume);
	if (iVolume == 0 || iVolume == VolumeBase)
	{
		vol = 1;
		return false;
	}
	vol = dVolume / VolumeBase;
	return true;
}

const wchar_t *CFGGetExtendParam1(void)
{
#ifdef XSFDRIVER_EXTENDPARAM1NAME
	return sExtendParam1;
#else
	return 0;
#endif
}
const wchar_t *CFGGetExtendParam2(void)
{
#ifdef XSFDRIVER_EXTENDPARAM2NAME
	return sExtendParam2;
#else
	return 0;
#endif
}

void CFGDefault(void)
{
	dwPlayInfinitely = 0;
	dwInterpolation = (unsigned long)SPUInterpolation_Linear;
	dwSkipSilenceOnStartSec = 5;
	dwDetectSilenceSec = 5;
	dwDefaultLength = (1 * 60 + 55) * 1000;
	dwDefaultFade = 5 * 1000;
	dVolume = VolumeBase;

	sDefaultLength = sDefaultLengthC;
	sDefaultFade = sDefaultFadeC;

#ifdef XSFDRIVER_EXTENDPARAM1NAME
	sExtendParam1 = sExtendParam1C;
#endif
#ifdef XSFDRIVER_EXTENDPARAM2NAME
	sExtendParam2 = sExtendParam2C;
#endif

	int tch = 0;
	for (const CHANNELMAP *pchmap = &chmap[0]; pchmap->devch; pchmap++)
	{
		for (int ch = 1; ch <= pchmap->devch; ch++)
		{
			CFGSetChannelMute(tch++, false);
		}
	}
	while (tch < 32 * 4)
		CFGSetChannelMute(tch++, true);
}

void CFGLoad(LPIConfigIO pcfg)
{
	try
	{

		CFGDefault();
		dwInterpolation = pcfg->GetULong(L"Interpolation", dwInterpolation);
		dwPlayInfinitely = pcfg->GetULong(L"PlayInfinitely", dwPlayInfinitely);
		dwSkipSilenceOnStartSec = pcfg->GetULong(L"SkipSilenceOnStartSec", dwSkipSilenceOnStartSec);
		dwDetectSilenceSec = pcfg->GetULong(L"DetectSilenceSec", dwDetectSilenceSec);
		dVolume = pcfg->GetFloat(L"Volume", dVolume / dVolume) * VolumeBase;
		sDefaultLength = pcfg->GetString(L"DefaultLength", sDefaultLength);
		sDefaultFade = pcfg->GetString(L"DefaultFade", sDefaultFade);
#ifdef XSFDRIVER_EXTENDPARAM1NAME
		sExtendParam1 = pcfg->GetString(XSFDRIVER_EXTENDPARAM1NAME, sExtendParam1);
#endif
#ifdef XSFDRIVER_EXTENDPARAM2NAME
		sExtendParam2 = pcfg->GetString(XSFDRIVER_EXTENDPARAM2NAME, sExtendParam2);
#endif

		dwDefaultLength = XSFTag::ToMS(sDefaultLength);
		dwDefaultFade = XSFTag::ToMS(sDefaultFade);
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

void CFGSave(LPIConfigIO pcfg)
{
	try
	{
		pcfg->SetULong(L"PlayInfinitely", dwPlayInfinitely);
		pcfg->SetULong(L"Interpolation", dwInterpolation);
		pcfg->SetULong(L"SkipSilenceOnStartSec", dwSkipSilenceOnStartSec);
		pcfg->SetULong(L"DetectSilenceSec", dwDetectSilenceSec);
		pcfg->SetFloat(L"Volume", dVolume / VolumeBase);
		pcfg->SetString(L"DefaultLength", sDefaultLength);
		pcfg->SetString(L"DefaultFade", sDefaultFade);
#ifdef XSFDRIVER_EXTENDPARAM1NAME
		pcfg->SetString(XSFDRIVER_EXTENDPARAM1NAME, sExtendParam1);
#endif
#ifdef XSFDRIVER_EXTENDPARAM2NAME
		pcfg->SetString(XSFDRIVER_EXTENDPARAM2NAME, sExtendParam2);
#endif
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

void CFGReset(LPIConfigIO pcfg, void *hwndDlg)
{
	(void)pcfg;
	try
	{
		char buf[128];
		xsfc::TWin32::DlgSetCheck(hwndDlg, 0x200, dwPlayInfinitely == 1);
		xsfc::TWin32::DlgAddCombo(hwndDlg, IDC_COMBO_INTERPOLATION, xsfc::TString("No interpolation"));
		xsfc::TWin32::DlgAddCombo(hwndDlg, IDC_COMBO_INTERPOLATION, xsfc::TString("Linear interpolation"));
		xsfc::TWin32::DlgAddCombo(hwndDlg, IDC_COMBO_INTERPOLATION, xsfc::TString("Cosine interpolation"));
		ComboBox_SetCurSel(GetDlgItem((HWND)hwndDlg, IDC_COMBO_INTERPOLATION), dwInterpolation);
		xsfc::TWin32::DlgSetText(hwndDlg, 0x201, sDefaultLength);
		xsfc::TWin32::DlgSetText(hwndDlg, 0x202, sDefaultFade);
		xsfc::TWin32::DlgSetText(hwndDlg, 0x203, xsfc::TString(dwSkipSilenceOnStartSec));
		xsfc::TWin32::DlgSetText(hwndDlg, 0x204, xsfc::TString(dwDetectSilenceSec));
		xsfc::TWin32::DlgSetText(hwndDlg, 0x205, xsfc::TString(dVolume / VolumeBase));
#ifdef XSFDRIVER_EXTENDPARAM1NAME
		xsfc::TWin32::DlgSetText(hwndDlg, 0x207, xsfc::TString(XSFDRIVER_EXTENDPARAM1LABEL));
		xsfc::TWin32::DlgSetText(hwndDlg, 0x208, sExtendParam1);
		xsfc::TWin32::DlgSetEnabled(hwndDlg, 0x208, true);
#endif
#ifdef XSFDRIVER_EXTENDPARAM2NAME
		xsfc::TWin32::DlgSetText(hwndDlg, 0x209, xsfc::TString(XSFDRIVER_EXTENDPARAM2LABEL));
		xsfc::TWin32::DlgSetText(hwndDlg, 0x20a, sExtendParam2);
		xsfc::TWin32::DlgSetEnabled(hwndDlg, 0x20a, true);
#endif
		for (const CHANNELMAP *pchmap = &chmap[0]; pchmap->devch; pchmap++)
		{
			for (int ch = 1; ch <= pchmap->devch; ch++)
			{
#if defined(HAVE_SPRINTF_S)
				sprintf_s(buf, sizeof(buf), pchmap->devname, ch);
#elif defined(HAVE_SNPRINTF)
				snprintf(buf, sizeof(buf), pchmap->devname, ch);
				buf[sizeof(buf) - 1] = 0;
#elif defined(HAVE_SNPRINTF_)
				_snprintf(buf, sizeof(buf), pchmap->devname, ch);
#else
				sprintf(buf, pchmap->devname, ch);
#endif
				xsfc::TWin32::DlgAddList(hwndDlg, 0x206, buf);
			}
		}
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

void CFGUpdate(LPIConfigIO pcfg, void *hwndDlg)
{
	(void)pcfg;
	try
	{
		dwPlayInfinitely = xsfc::TWin32::DlgGetCheck(hwndDlg, 0x200) ? 1 : 0;
		dwInterpolation = xsfc::TWin32::DlgCurCombo(hwndDlg, IDC_COMBO_INTERPOLATION);
		sDefaultLength = xsfc::TWin32::DlgGetText(hwndDlg, 0x201);
		sDefaultFade = xsfc::TWin32::DlgGetText(hwndDlg, 0x202);
		dwSkipSilenceOnStartSec = xsfc::TWin32::DlgGetText(hwndDlg, 0x203).GetULong();
		dwDetectSilenceSec = xsfc::TWin32::DlgGetText(hwndDlg, 0x204).GetULong();
		dVolume = xsfc::TWin32::DlgGetText(hwndDlg, 0x205).GetFloat() * VolumeBase;
#ifdef XSFDRIVER_EXTENDPARAM1NAME
		sExtendParam1 = xsfc::TWin32::DlgGetText(hwndDlg, 0x208);
#endif
#ifdef XSFDRIVER_EXTENDPARAM2NAME
		sExtendParam2 = xsfc::TWin32::DlgGetText(hwndDlg, 0x20a);
#endif

		dwDefaultLength = XSFTag::ToMS(sDefaultLength);
		dwDefaultFade = XSFTag::ToMS(sDefaultFade);
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

void CFGMuteChange(void *hwndDlg, int itm)
{
	int cnt = xsfc::TWin32::DlgCntList(hwndDlg, itm);
	for (int ch = 0; ch < cnt; ch++)
	{
		bool mute = xsfc::TWin32::DlgGetList(hwndDlg, itm, ch);
		CFGSetChannelMute(ch, mute);
	}
}

