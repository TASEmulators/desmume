#define WIN32_LEAN_AND_MEAN
#include "leakchk.h"

#include <windows.h>
#include <windowsx.h>

/* Winamp SDK headers */
#include <winamp/wa_ipc.h>

#include "../pversion.h"
#include "in_xsfcfg.h"
#include "xsfui.rh"

#if _MSC_VER >= 1200
#pragma warning(disable:4290)
#endif

extern unsigned long dwInterpolation;

namespace
{

xsfc::TAutoPtr<IConfigIO> pcfg;
prefsDlgRec prefwork;
int prefreg = 0;
HMODULE hDLL = 0;

}

class IniConfig : public IConfigIO
{
	xsfc::TString sIniPath;
protected:
	IniConfig() throw()
	{
	}

	bool Initialize(HWND hwndWinamp) throw(xsfc::EShortOfMemory)
	{
		char *ini = (char *)(::SendMessageA(hwndWinamp, WM_WA_IPC, 0, IPC_GETINIFILE));
		if (ini && int(ini) != 1 && !IsBadStringPtrA(ini, MAX_PATH))
			sIniPath = xsfc::TString(ini);
		else
			sIniPath = xsfc::TWin32::ExtractPath(xsfc::TWin32::ModulePath(NULL)) + "winamp.ini";
		return true;
	}

public:
	~IniConfig() throw()
	{
	}

	void SetULong(const wchar_t *name, const unsigned long value) throw()
	{
		try
		{
			xsfc::TWin32::SetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(value));
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	unsigned long GetULong(const wchar_t *name, const unsigned long defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = xsfc::TWin32::GetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(defaultvalue));
			if (value[0]) return value.GetULong();
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
		return defaultvalue;
	}
	void SetFloat(const wchar_t *name, const double value) throw()
	{
		try
		{
			xsfc::TWin32::SetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(value));
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	double GetFloat(const wchar_t *name, const double defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = xsfc::TWin32::GetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, xsfc::TString(defaultvalue));
			if (value[0]) return value.GetFloat();
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
		return defaultvalue;
	}
	void SetString(const wchar_t *name, const wchar_t *value) throw()
	{
		try
		{
			xsfc::TWin32::SetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, value);
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	xsfc::TString GetString(const wchar_t *name, const wchar_t *defaultvalue = 0) throw(xsfc::EShortOfMemory)
	{
		return xsfc::TWin32::GetPrivateProfile(sIniPath, xsfc::TString(XSFDRIVER_SIMPLENAME), name, defaultvalue);
	}

	static LPIConfigIO Create(HWND hwndWinamp) throw(xsfc::EShortOfMemory)
	{
		xsfc::TAutoPtr<IniConfig> cfg(new(xsfc::nothrow) IniConfig);
		return (cfg && cfg->Initialize(hwndWinamp)) ? cfg.Detach() : 0;
	}
};

static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, lParam))
		{
		case IDC_COMBO_INTERPOLATION:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
			{
				dwInterpolation = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION));
			}
			 break;
		case IDOK:
			CFGUpdate(pcfg, hwndDlg);
			CFGSave(pcfg);
			::EndDialog(hwndDlg, IDOK);
			break;
		case IDCANCEL:
			::EndDialog(hwndDlg, IDCANCEL);
			break;
		case 0x206:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == LBN_SELCHANGE)
			{
				CFGMuteChange(hwndDlg, 0x206);
			}
			break;
		default:
			return FALSE;
		}
		break;
	case WM_INITDIALOG:
		::SetWindowTextA(hwndDlg, WINAMPPLUGIN_NAME);
		::ShowWindow(GetDlgItem(hwndDlg, IDOK), SW_SHOWNA);
		::ShowWindow(GetDlgItem(hwndDlg, IDCANCEL), SW_SHOWNA);
		CFGLoad(pcfg);
		CFGReset(pcfg, hwndDlg);
		return TRUE;
	}
	return FALSE;
}

static BOOL CALLBACK DialogProcPref(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static bool initialized = false;
	switch (uMsg)
	{
	case WM_COMMAND:
		switch (GET_WM_COMMAND_ID(wParam, LpARAM))
		{
		case IDC_COMBO_INTERPOLATION:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == CBN_SELCHANGE)
			{
				dwInterpolation = ComboBox_GetCurSel(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION));
				CFGSave(pcfg);
			}
			 break;

		case 0x200: case 0x201: case 0x202:
		case 0x203: case 0x204: case 0x205:
			if (initialized)
			{
				CFGUpdate(pcfg, hwndDlg);
				CFGSave(pcfg);
			}
			break;
		case 0x206:
			if (GET_WM_COMMAND_CMD(wParam, lParam) == LBN_SELCHANGE)
			{
				CFGMuteChange(hwndDlg, 0x206);
			}
			break;
		default:
			return FALSE;
		}
		break;
	case WM_INITDIALOG:
		initialized = false;
		ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION), "None");
		ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION), "Linear");
		ComboBox_AddString(GetDlgItem(hwndDlg, IDC_COMBO_INTERPOLATION), "Cosine");
		CFGLoad(pcfg);
		CFGReset(pcfg, hwndDlg);
		initialized = true;
		return TRUE;
	}
	return FALSE;
}

void winamp_config_init(HINSTANCE hinstDLL)
{
	hDLL = hinstDLL;
}
void winamp_config_load(HWND hwndWinamp)
{
	if (!pcfg)
	{
		pcfg = IniConfig::Create(hwndWinamp);
		if (!pcfg) pcfg = NullConfig::Create();
		CFGLoad(pcfg);
	}
}


void winamp_config_add_prefs(HWND hwndWinamp)
{
	if (prefreg) return;

	if (((LONG)xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, 0, IPC_GETVERSION) & 0xffffU) >= 0x2090)
	{
		/* 2.9+ */
		prefwork.hInst = hDLL;
		prefwork.dlgID = 1;
		prefwork.proc = (void *)DialogProcPref;
		prefwork.name = XSFDRIVER_SIMPLENAME;
		prefwork.where = 0;
		prefwork._id = 0;
		prefwork.next = 0;
		prefreg = !(LONG)xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &prefwork, IPC_ADD_PREFS_DLG);
	}
}

void winamp_config_remove_prefs(HWND hwndWinamp)
{

	if (pcfg) pcfg.Release();
	if (prefreg)
	{
		xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &prefwork, IPC_REMOVE_PREFS_DLG);
		prefreg = 0;
	}
}

void winamp_config_dialog(HWND hwndWinamp, HWND hwndParent)
{
	HRSRC hrsrc;
	DWORD dwSize;
	HGLOBAL hGlobal;
	xsfc::TSimpleArray<BYTE> resbuf;
	if (prefreg)
	{
		if (((LONG)xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, 0, IPC_GETVERSION) & 0xffffU) >= 0x5000)
		{
			xsfc::TWin32::WndMsgSend(hwndWinamp, WM_WA_IPC, &prefwork, IPC_OPENPREFSTOPAGE);
			return; 
		}
	}

	hrsrc = ::FindResourceA(hDLL, MAKEINTRESOURCE(1), RT_DIALOG);
	if (!hrsrc) return;

	dwSize = ::SizeofResource(hDLL, hrsrc);
	hGlobal = ::LoadResource(hDLL, hrsrc);

	if (dwSize < 4 || !hGlobal) return;
	
	if (!resbuf.Resize(dwSize)) return;
	
	LPBYTE lpResource = resbuf.Ptr();

	::CopyMemory(lpResource, hGlobal, dwSize);

	lpResource[0] = '\xc0';
	lpResource[1] = '\x08';
	lpResource[2] = '\xc8';
	lpResource[3] = '\x80';

	xsfc::TWin32::DlgInvoke(::GetModuleHandleA(NULL), lpResource, NULL, DialogProc, NULL);
}