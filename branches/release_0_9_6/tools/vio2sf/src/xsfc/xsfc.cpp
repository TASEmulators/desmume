#ifdef HAVE_STDINT_H
#include <stdint.h>
#elif (defined(_MSC_VER) && (_MSC_VER >= 1200))
#define int64_t signed __int64
#define uint64_t unsigned __int64
#elif (defined(__BORLANDC__ ) && (__BORLANDC__ >= 0x530) && !defined(__STRICT_ANSI__))
#define int64_t signed __int64
#define uint64_t unsigned __int64
#elif (defined(__WATCOMC__) && (__WATCOMC__ >= 1100)) 
#define int64_t signed __int64
#define uint64_t unsigned __int64
#else
#error "64 bit integer variables are required."
#endif

#define WIN32_LEAN_AND_MEAN
#include "leakchk.h"

#include <windows.h>
#include <windowsx.h>

#include "xsfc.h"


#if _MSC_VER >= 1200
#pragma warning(disable:4290)
#endif

namespace
{

class FileHandle
{
protected:
	HANDLE h;

	inline HANDLE Handle()
	{
		return h;
	}

	static xsfc::TString FNExtend(const wchar_t *fn) throw(xsfc::EShortOfMemory)
	{
		if (fn && fn[0] == L'\\' && fn[1] == L'\\' && fn[2] != L'?')
			return xsfc::TString("\\\\?\\UNC") + (fn + 1);
		if (fn && fn[0] && fn[1] == L':' && fn[2] == L'\\')
			return xsfc::TString("\\\\?\\") + fn;
		return xsfc::TString();
	}

	static HANDLE ReadOpen(const WCHAR *fn) throw()
	{
		HANDLE h = INVALID_HANDLE_VALUE;
		try
		{
			if (xsfc::TWin32::IsUnicodeSupportedOS())
			{
				xsfc::TString fnw = FNExtend(fn);
				h = CreateFileW(fnw ? fnw : fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
/*
				if (h == INVALID_HANDLE_VALUE)
				{
					h = CreateFileW(fn, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
					if (h == INVALID_HANDLE_VALUE)
					{
						DWORD dw = GetLastError();
						OutputDebugStringA("");
						(void)dw;
					}
				}
*/
			}
			else
			{
				h = CreateFileA(xsfc::TStringM(fn), GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
			}
		}
		catch (xsfc::EShortOfMemory e)
		{
		}
		return h;
	}

public:
	inline bool Opened()
	{
		return Handle() != INVALID_HANDLE_VALUE;
	}

	FileHandle(HANDLE hfile = INVALID_HANDLE_VALUE)
		: h(hfile)
	{
	}

	FileHandle(const wchar_t *path)
		: h(ReadOpen(path))
	{
	}

	~FileHandle()
	{
		if (Opened())
		{
			CloseHandle(Handle());
		}
	}

	size_t Length()
	{
		if (Opened())
		{
			DWORD dwH = 0;
			DWORD dwL = GetFileSize(Handle(), &dwH);
			if (dwL != DWORD(0xFFFFFFFF) || GetLastError() == NO_ERROR)
			{
				if (dwH)
				{
					uint64_t s64 = ((((uint64_t)dwH) << 32) | dwL);
					size_t s = size_t(s64);
					if (s == s64) return s;
				}
				else
				{
					size_t s = size_t(dwL);
					if (s == dwL) return s;
				}
			}
		}
		return 0;
	}
	size_t Read(void *p, size_t l)
	{
		if (Opened())
		{
			DWORD nNumberOfBytesToRead = DWORD(l);
			DWORD dwNumberOfBytesRead = 0;
			if (l == nNumberOfBytesToRead)
			{
				if (!ReadFile(Handle(), p, nNumberOfBytesToRead, &dwNumberOfBytesRead, 0))
					dwNumberOfBytesRead = 0;
			}
			size_t s = size_t(dwNumberOfBytesRead);
			if (s == dwNumberOfBytesRead)
				return s;
		}
		return 0;
	}
};

}

namespace xsfc
{

static UINT GetFileCP(void) throw()
{
	return ::AreFileApisANSI() ? CP_ACP : CP_OEMCP;
}

size_t TIConv::ToWide(bool isUTF8, const char *s, size_t sl, wchar_t *d, size_t dl) throw()
{
	UINT cp = isUTF8 ? CP_UTF8 : GetFileCP();
	DWORD flag = 0;
	return ::MultiByteToWideChar(cp, flag, s, (int)sl, d, (int)dl);
}

size_t TIConv::ToMulti(bool isUTF8, const wchar_t *s, size_t sl, char *d, size_t dl) throw()
{
	UINT cp = isUTF8 ? CP_UTF8 : GetFileCP();
	DWORD flag = 0;
	return ::WideCharToMultiByte(cp, flag, s, (int)sl, d, (int)dl, 0, 0);
}

bool TWin32::IsUnicodeSupportedOS(void) throw()
{
	return !(::GetVersion() & 0x80000000);
}

bool TWin32::IsPathSeparator(char c) throw()
{
	return c == ':' || c == '\\' || c == '/' || c == '|';
}

bool TWin32::IsPathSeparator(wchar_t c) throw()
{
	return c == L':' || c == L'\\' || c == L'/' || c == L'|';
}

const char *TWin32::NextChar(const char *p)
{
	return ::IsDBCSLeadByte(*p) ? p + 2 : p + 1;
}

const wchar_t *TWin32::NextChar(const wchar_t *p)
{
	return p + 1;
}

template <class T1, class T2> class WndMsgSendHelper
{
public:
	static void *Invoke(void *hwnd, unsigned msg, T1 wp, T2 lp) throw()
	{
		return (void *)(TWin32::IsUnicodeSupportedOS() ? ::SendMessageW : ::SendMessageA) (static_cast<HWND>(hwnd), msg, WPARAM(wp), LPARAM(lp));
	}
};

void *TWin32::WndMsgSend(void *hwnd, unsigned msg, const void *wp, const void *lp)
{
	return WndMsgSendHelper<LPCVOID, LPCVOID>::Invoke(hwnd, msg, wp, lp);
}
void *TWin32::WndMsgSend(void *hwnd, unsigned msg, const void *wp, int lp)
{
	return WndMsgSendHelper<LPCVOID, int>::Invoke(hwnd, msg, wp, lp);
}
void *TWin32::WndMsgSend(void *hwnd, unsigned msg, int wp, const void *lp)
{
	return WndMsgSendHelper<int, LPCVOID>::Invoke(hwnd, msg, wp, lp);
}
void *TWin32::WndMsgSend(void *hwnd, unsigned msg, int wp, int lp)
{
	return WndMsgSendHelper<int, int>::Invoke(hwnd, msg, wp, lp);
}

void TWin32::WndMsgPost(void *hwnd, unsigned msg, const void *wp, const void *lp)
{
	(IsUnicodeSupportedOS() ? ::PostMessageW : ::PostMessageA) (static_cast<HWND>(hwnd), msg, WPARAM(wp), LPARAM(lp));
}

void *TWin32::WndGetLongPtr(void *hwnd, int idx)
{
#ifdef GetWindowLongPtr
	return (void *)(LONG_PTR)(IsUnicodeSupportedOS() ? ::GetWindowLongPtrW : ::GetWindowLongPtrA) (static_cast<HWND>(hwnd), idx);
#else
	return (void *)(IsUnicodeSupportedOS() ? ::GetWindowLongW : ::GetWindowLongA) (static_cast<HWND>(hwnd), idx);
#endif
}
void *TWin32::WndSetLongPtr(void *hwnd, int idx, void *ptr)
{
#ifdef SetWindowLongPtr
#ifdef _WIN64
	if (IsUnicodeSupportedOS())
		return (void *)(LONG_PTR) ::SetWindowLongPtrW(static_cast<HWND>(hwnd), idx, (LONG_PTR)ptr);
	else
		return (void *)(LONG_PTR) ::SetWindowLongPtrA(static_cast<HWND>(hwnd), idx, (LONG_PTR)ptr);
#else
	if (IsUnicodeSupportedOS())
		return (void *)(LONG_PTR) ::SetWindowLongW(static_cast<HWND>(hwnd), idx, (LONG)(LONG_PTR)ptr);
	else
		return (void *)(LONG_PTR) ::SetWindowLongA(static_cast<HWND>(hwnd), idx, (LONG)(LONG_PTR)ptr);
#endif
#else
	return (void *)(IsUnicodeSupportedOS() ? ::SetWindowLongW : ::SetWindowLongA) (static_cast<HWND>(hwnd), idx, (LONG)ptr);
#endif
}

TString TWin32::ModulePath(void *hmod) throw(EShortOfMemory)
{
	TString ret;
	if (IsUnicodeSupportedOS())
	{
		WCHAR wbuf[MAX_PATH];
		wbuf[0] = 0;
		::GetModuleFileNameW((HINSTANCE)hmod, wbuf, MAX_PATH);
		ret.SetW(wbuf, MAX_PATH);
	}
	else
	{
		char mbuf[MAX_PATH];
		mbuf[0] = 0;
		::GetModuleFileNameA((HINSTANCE)hmod, mbuf, MAX_PATH);
		ret.SetM(mbuf, MAX_PATH);
	}
	return ret;
}

TString TWin32::ExtractPath(const wchar_t *p) throw(EShortOfMemory)
{
	int i, s;
	for (s = i = 0; p[i]; i++)
	{
		if (IsPathSeparator(p[i]))
			s = i + 1;
	}

	return TString(p, s);
}

TString TWin32::ExtractFileName(const wchar_t *p) throw(EShortOfMemory)
{
	return TString(TWin32T<wchar_t>::ExtractFilename(p));
}

TString TWin32::CanonicalizePath(const wchar_t *p) throw(EShortOfMemory)
{
	size_t wl = StrNLen<wchar_t>(p);
	TAutoBuffer<wchar_t> buf;
	buf.Resize(wl + 1);
	wchar_t *b = buf.Ptr();
	::lstrcpyW(b, p);
	bool e;
	do
	{
		e = false;
		wchar_t *f = 0;
		for (wchar_t *c = b; *c; c++)
		{
			if (TWin32::IsPathSeparator(*c))
			{
				if (f && c[1] == L'.' && c[2] == L'.')
				{
					f += 1;
					c += 4;
					while (0 != (*(f++) = *(c++)));
					e = true;
					break;
				}
				else
				{
					f = c;
				}
			}
		}
	} while (e);
	TString ret(b);
	buf.Free();
	return ret;
}


TString TWin32::GetPrivateProfile(const wchar_t *ininame, const wchar_t *appname, const wchar_t *keyname, const wchar_t *defaultvalue) throw(EShortOfMemory)
{
	if (IsUnicodeSupportedOS())
	{
		size_t ret = 0;
		size_t prevret = 0;
		size_t bufsize = MAX_PATH;
		TAutoBuffer<WCHAR> buf;
		do
		{
			prevret = ret;
			bufsize += bufsize;
			if (!buf.Resize(bufsize))
				throw EShortOfMemory();
			ret = ::GetPrivateProfileStringW(appname, keyname, defaultvalue, buf.Ptr(), DWORD(bufsize), ininame);
		}
		while (ret + 1== bufsize && ret != prevret);
		return TString(buf.Ptr(), buf.Len());
	}
	else
	{
		size_t ret = 0;
		size_t prevret = 0;
		size_t bufsize = MAX_PATH;
		TAutoBuffer<char> buf;
		TStringM ininameM(ininame), appnameM(appname), keynameM(keyname), defaultvalueM(defaultvalue);
		do
		{
			prevret = ret;
			bufsize += bufsize;
			if (!buf.Resize(bufsize))
				throw EShortOfMemory();
			ret = ::GetPrivateProfileStringA(appnameM, keynameM, defaultvalueM, buf.Ptr(), DWORD(bufsize), ininameM);
		}
		while (ret + 1 == bufsize && ret != prevret);
		return TString(buf.Ptr(), buf.Len());
	}
}

void TWin32::SetPrivateProfile(const wchar_t *ininame, const wchar_t *appname, const wchar_t *keyname, const wchar_t *value) throw()
{
	try
	{
		if (IsUnicodeSupportedOS())
		{
			::WritePrivateProfileStringW(appname, keyname, value, ininame);
		}
		else
		{
			TStringM ininameM(ininame), appnameM(appname), keynameM(keyname), valueM(value);
			::WritePrivateProfileStringA(appnameM, keynameM, valueM, ininameM);
		}
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

void *TWin32::DlgCreate(void *hinst, int id, void *hwndParent, DLGPROC pProc, void *param)
{
	if (IsUnicodeSupportedOS())
		return (void *)::CreateDialogParamW(static_cast<HINSTANCE>(hinst), MAKEINTRESOURCEW(id), static_cast<HWND>(hwndParent), pProc, LPARAM(param));
	else
		return (void *)::CreateDialogParamA(static_cast<HINSTANCE>(hinst), MAKEINTRESOURCEA(id), static_cast<HWND>(hwndParent), pProc, LPARAM(param));
}
void *TWin32::DlgInvoke(void *hinst, const void *ptemp, void *hwndParent, DLGPROC pProc, void *param)
{
	if (IsUnicodeSupportedOS())
		return (void *)::DialogBoxIndirectParamW(static_cast<HINSTANCE>(hinst), static_cast<LPCDLGTEMPLATE>(ptemp), static_cast<HWND>(hwndParent), pProc, LPARAM(param));
	else
		return (void *)::DialogBoxIndirectParamA(static_cast<HINSTANCE>(hinst), static_cast<LPCDLGTEMPLATE>(ptemp), static_cast<HWND>(hwndParent), pProc, LPARAM(param));
}


void TWin32::DlgSetText(void *hwndDlg, int itm, TString text) throw()
{
	try
	{
		if (IsUnicodeSupportedOS())
			::SetDlgItemTextW(static_cast<HWND>(hwndDlg), itm, text);
		else
			::SetDlgItemTextA(static_cast<HWND>(hwndDlg), itm, TStringM(text));
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

TString TWin32::DlgGetText(void *hwndDlg, int itm) throw(EShortOfMemory)
{
	TString ret;
	if (IsUnicodeSupportedOS())
	{
		WCHAR buf[32];
		::GetDlgItemTextW(static_cast<HWND>(hwndDlg), itm, buf, 32);
		ret.SetW(buf, 32);
	}
	else
	{
		char buf[32];
		::GetDlgItemTextA(static_cast<HWND>(hwndDlg), itm, buf, 32);
		ret.SetM(buf, 32);
	}
	return ret;
}

static void *DlgItem(void *hwndDlg, int itm)
{
	return ::GetDlgItem(static_cast<HWND>(hwndDlg), itm);
}
	
void TWin32::DlgSetEnabled(void *hwndDlg, int itm, bool chk)
{
	::EnableWindow(::GetDlgItem(static_cast<HWND>(hwndDlg), itm), chk ? TRUE : FALSE); 
}


void TWin32::DlgSetCheck(void *hwndDlg, int itm, bool chk)
{
	WndMsgSend(DlgItem(hwndDlg, itm), BM_SETCHECK, chk ? BST_CHECKED : BST_UNCHECKED, 0); 
}

bool TWin32::DlgGetCheck(void *hwndDlg, int itm)
{
	return (INT_PTR)WndMsgSend(DlgItem(hwndDlg, itm), BM_GETCHECK, 0, 0) == BST_CHECKED;
}

void TWin32::DlgAddList(void *hwndDlg, int itm, TString item)
{
	try
	{
		if (IsUnicodeSupportedOS())
			WndMsgSend(DlgItem(hwndDlg, itm), LB_ADDSTRING, 0, item.GetW());
		else
			WndMsgSend(DlgItem(hwndDlg, itm), LB_ADDSTRING, 0, TStringM(item).GetM());
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

void TWin32::DlgAddCombo(void *hwndDlg, int itm, TString item)
{
	try
	{
		if (IsUnicodeSupportedOS())
			WndMsgSend(DlgItem(hwndDlg, itm), CB_ADDSTRING, 0, item.GetW());
		else
			WndMsgSend(DlgItem(hwndDlg, itm), CB_ADDSTRING, 0, TStringM(item).GetM());
	}
	catch (xsfc::EShortOfMemory e)
	{
	}
}

int TWin32::DlgCntList(void *hwndDlg, int itm)
{
	return (int)(INT_PTR)WndMsgSend(DlgItem(hwndDlg, itm), LB_GETCOUNT, 0, 0); 
}

int TWin32::DlgCurList(void *hwndDlg, int itm)
{
	return (int)(INT_PTR)WndMsgSend(DlgItem(hwndDlg, itm), LB_GETCURSEL, 0, 0); 
}

int TWin32::DlgCurCombo(void *hwndDlg, int itm)
{
	return (int)(INT_PTR)WndMsgSend(DlgItem(hwndDlg, itm), CB_GETCURSEL, 0, 0); 
}

bool TWin32::DlgGetList(void *hwndDlg, int itm, int cur)
{
	return (INT_PTR)WndMsgSend(DlgItem(hwndDlg, itm), LB_GETSEL, cur, 0) > 0; 
}

void *TWin32::LoadEx(const wchar_t *path, size_t &loadsize, void *pwork, lpfnalloc palloc, lpfnfree pfree)
{
	void *p = 0;
	void *ret = 0;
	try
	{
		FileHandle h(path);
		size_t s = h.Length();
		if (s && palloc)
		{
			p = palloc(pwork, s);
			if (p && s == h.Read(p, s))
			{
				loadsize = s;
				ret = p;
				p = 0;
			}
		}
	}
	catch (EShortOfMemory e)
	{
	}
	if (p && pfree) pfree(pwork, p);
	return ret;
}

TAutoBuffer<char> TWin32::Map(const wchar_t *path)
{
	TAutoBuffer<char> ret;
	try
	{
		FileHandle h(path);
		size_t s = h.Length();
		if (s && ret.Resize(s))
		{
			if (s != h.Read(ret.Ptr(), s))
			{
				ret.Free();
			}
		}
	}
	catch (EShortOfMemory e)
	{
	}
	return ret;
}

}
