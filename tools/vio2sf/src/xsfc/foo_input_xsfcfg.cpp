#include "leakchk.h"

#include "foobar2000/SDK/foobar2000.h"

#include "../pversion.h"

#include "tagget.h"
#include "xsfcfg.h"

namespace
{

HMODULE hDLL;

const GUID cfgguid1 = XSFDRIVER_GUID1;
const GUID cfgguid2 = XSFDRIVER_GUID2;
cfg_string cfg_fooxsf(cfgguid2, "");

class foo_config : public IConfigIO
{
protected:
	typedef struct
	{
		xsfc::TString output;
		xsfc::TString newvalue;
		xsfc::TStringM newname;
		size_t newnamelen;
		bool exist;
	}
	cbw_t;

	static enum XSFTag::enum_callback_returnvalue scv_cb(void *pWork, const char *pNameTop, const char *pNameEnd, const char *pValueTop, const char *pValueEnd)
	{
		cbw_t *pcbw = static_cast<cbw_t *>(pWork);
		xsfc::TString name(true, pNameTop, pNameEnd - pNameTop);
		if (pcbw->newnamelen == pNameEnd - pNameTop && !_strnicmp(pNameTop, pcbw->newname, pcbw->newnamelen))
		{
			pcbw->exist = true;
			pcbw->output = pcbw->output + name + xsfc::TString(L"=") + pcbw->newvalue + xsfc::TString(L"\n");
		}
		else
		{
			xsfc::TString value(true, pValueTop, pValueEnd - pValueTop);
			pcbw->output = pcbw->output + name + xsfc::TString(L"=") + value + xsfc::TString(L"\n");
		}
		return XSFTag::enum_continue;
	}
	
	void SetConfigValue(const wchar_t *name, xsfc::TString value)
	{
		const char *idata = cfg_fooxsf.get_ptr();
		size_t isize = xsfc::StrNLen<char>(idata);
		cbw_t cbw;
		cbw.newvalue = value;
		cbw.newname = xsfc::TStringM(true, name);
		cbw.newnamelen = xsfc::StrNLen<char>(cbw.newname);
		cbw.exist = false;
		XSFTag::EnumRaw(scv_cb, &cbw, idata, isize);
		if (!cbw.exist)
		{
			cbw.output = cbw.output + name + xsfc::TString(L"=") + value + xsfc::TString(L"\n");
		}
		xsfc::TStringM oututf8(true, cbw.output);
		cfg_fooxsf = oututf8;
	}

	xsfc::TString GetConfigValue(const wchar_t *name)
	{
		xsfc::TStringM tag(true, name);
		const char *idata = cfg_fooxsf.get_ptr();
		size_t isize = xsfc::StrNLen<char>(idata);
		return XSFTag::GetRaw(true, tag, idata, isize);
	}

public:
	foo_config()
	{
	}
	~foo_config()
	{
	}

	void SetULong(const wchar_t *name, const unsigned long value) throw()
	{
		try
		{
			xsfc::TString sValue(value);
			SetConfigValue(name, sValue);
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	unsigned long GetULong(const wchar_t *name, const unsigned long defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = GetConfigValue(name);
			if (value[0])
				return value.GetULong();
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
			xsfc::TString sValue(value);
			SetConfigValue(name, sValue);
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	double GetFloat(const wchar_t *name, const double defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = GetConfigValue(name);
			if (value[0])
				return value.GetFloat();
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
			xsfc::TString sValue(value);
			SetConfigValue(name, sValue);
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
	}
	xsfc::TString GetString(const wchar_t *name, const wchar_t *defaultvalue = 0) throw()
	{
		try
		{
			xsfc::TString value = GetConfigValue(name);
			if (value[0])
				return value;
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
		return defaultvalue;
	}
};

static foo_config icfg;

class foo_input_xsfcfg : public preferences_page_v2
{
protected:
	bool initialized;
	foo_config *pcfg;
	HWND hwnd;

	bool OnCommand(HWND hwndDlg, DWORD id, DWORD cmd)
	{
		switch (id)
		{
		case 0x200: case 0x201: case 0x202:
		case 0x203: case 0x204: case 0x205:
			if (initialized)
			{
				CFGUpdate(pcfg, hwndDlg);
				CFGSave(pcfg);
			}
			break;
		case 0x206:
			if (cmd == LBN_SELCHANGE)
			{
				CFGMuteChange(hwndDlg, 0x206);
			}
			break;
		default:
			break;
		}
		return FALSE;
	}

	bool OnInit(HWND hwndDlg)
	{
		hwnd = hwndDlg;
		pcfg = &icfg;
		xsfc::TWin32::WndSetLongPtr(hwndDlg, DWLP_USER, this);

		initialized = false;
		CFGReset(pcfg, hwndDlg);
		initialized = true;
		return TRUE;
	}

	static BOOL CALLBACK DialogProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
	{
		switch (uMsg)
		{
		case WM_COMMAND:
			{
				foo_input_xsfcfg *pthis = static_cast<foo_input_xsfcfg *>(xsfc::TWin32::WndGetLongPtr(hwndDlg, DWLP_USER));
				return pthis->OnCommand(hwndDlg, LOWORD(wParam), HIWORD(wParam));
			}

		case WM_INITDIALOG:
			{
				foo_input_xsfcfg *pthis = static_cast<foo_input_xsfcfg *>((void *)lParam);
				return pthis->OnInit(hwndDlg) ? TRUE : FALSE;
			}
		}
		return FALSE;
	}

public:
	HWND create(HWND p_parent)
	{
		return static_cast<HWND>(xsfc::TWin32::DlgCreate(hDLL, 1, p_parent, DialogProc, this));
	}
	const char * get_name() { return XSFDRIVER_SIMPLENAME; }
	GUID get_guid() { return cfgguid1; }
	GUID get_parent_guid() { return guid_input; }
	bool reset_query() { return true; }
	void reset()
	{
		CFGDefault();
		CFGReset(pcfg, hwnd);
		CFGSave(pcfg);
	}
	bool get_help_url(pfc::string_base & p_out) { return false; }
	double get_sort_priority() { return 0; }

	foo_input_xsfcfg()
	: initialized(false)
	{
	}
	~foo_input_xsfcfg()
	{
	}
};

class foo_initquit : public initquit
{
	void on_init()
	{
		CFGLoad(&icfg);
	}
	void on_quit()
	{
	}
};

static preferences_page_factory_t<foo_input_xsfcfg> g_input_xsfcfg_factory;

static initquit_factory_t<foo_initquit> g_input_xsfiq_factory;

}


extern "C" void fb2k_config_init(HINSTANCE hinstDLL)
{
	hDLL = (HMODULE)hinstDLL;
}
