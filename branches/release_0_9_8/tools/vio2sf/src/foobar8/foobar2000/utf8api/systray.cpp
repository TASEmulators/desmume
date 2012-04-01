#include "utf8api.h"

#include <shellapi.h>

#ifdef UNICODE

//mega-fucko: i dont have latest definitions in my platform sdk that came with msvc6

#define NIF_MESSAGE     0x00000001
#define NIF_ICON        0x00000002
#define NIF_TIP         0x00000004
#define NIF_STATE       0x00000008
#define NIF_INFO        0x00000010
#define NIF_GUID        0x00000020

#define NIIF_INFO       0x00000001
#define NIIF_WARNING    0x00000002
#define NIIF_ERROR      0x00000003
#define NIIF_ICON_MASK  0x0000000F
#define NIIF_NOSOUND    0x00000010

#pragma pack(push,4)

typedef struct {
        DWORD cbSize;
        HWND hWnd;
        UINT uID;
        UINT uFlags;
        UINT uCallbackMessage;
        HICON hIcon;
        WCHAR  szTip[128];
        DWORD dwState;
        DWORD dwStateMask;
        WCHAR  szInfo[256];
        union {
            UINT  uTimeout;
            UINT  uVersion;
        } DUMMYUNIONNAME;
        WCHAR  szInfoTitle[64];
        DWORD dwInfoFlags;
} NOTIFYICONDATA_NEW;


#pragma pack(pop)

static bool is_win2k_or_newer()
{
	OSVERSIONINFO ov;
    ov.dwOSVersionInfoSize = sizeof(ov);
    GetVersionEx(&ov);
	switch(ov.dwPlatformId)
	{
	default:
	case VER_PLATFORM_WIN32_WINDOWS:
	case VER_PLATFORM_WIN32s:
		return false;
	case VER_PLATFORM_WIN32_NT:
		return ov.dwMajorVersion>4;
	}
}


#endif

UTF8API_EXPORT void uFixAmpersandChars(const char * src,string_base & out)
{
	out.reset();
	while(*src)
	{
		if (*src=='&')
		{
			out.add_string("&&&");
			src++;
			while(*src=='&')
			{
				out.add_string("&&");
				src++;
			}
		}
		else out.add_byte(*(src++));
	}
}

UTF8API_EXPORT void uFixAmpersandChars_v2(const char * src,string_base & out)
{
	out.reset();
	while(*src)
	{
		if (*src=='&')
		{
			out.add_string("&&");
			src++;
		}
		else out.add_byte(*(src++));
	}
}

static BOOL do_action(DWORD action,NOTIFYICONDATA * data)
{
	if (Shell_NotifyIcon(action,data)) return TRUE;
	if (action==NIM_MODIFY)
	{
		if (Shell_NotifyIcon(NIM_ADD,data)) return TRUE;
	}
	return FALSE;
}

extern "C"
{

	UTF8API_EXPORT BOOL uShellNotifyIcon(DWORD action,HWND wnd,UINT id,UINT callbackmsg,HICON icon,const char * tip)
	{
		NOTIFYICONDATA nid;
		NOTIFYICONDATA * p_nid = &nid;
#ifdef UNICODE
		NOTIFYICONDATA_NEW nid_new;
		if (is_win2k_or_newer())
		{
			memset(&nid_new,0,sizeof(nid_new));
			nid_new.cbSize = sizeof(nid_new);
			nid_new.hWnd = wnd;
			nid_new.uID = id;
			nid_new.uFlags = 0;
			if (callbackmsg)
			{
				nid_new.uFlags |= NIF_MESSAGE;
				nid_new.uCallbackMessage = callbackmsg;
			}
			if (icon)
			{
				nid_new.uFlags |= NIF_ICON;
				nid_new.hIcon = icon;
			}			
			if (tip)
			{
				nid_new.uFlags |= NIF_TIP;
				_tcsncpy(nid_new.szTip,string_os_from_utf8(tip),tabsize(nid_new.szTip)-1);
			}

			p_nid = reinterpret_cast<NOTIFYICONDATAW*>(&nid_new);
		}
		else
#endif
		{
			memset(&nid,0,sizeof(nid));
			nid.cbSize = sizeof(nid);
			nid.hWnd = wnd;
			nid.uID = id;
			nid.uFlags = 0;
			if (callbackmsg)
			{
				nid.uFlags |= NIF_MESSAGE;
				nid.uCallbackMessage = callbackmsg;
			}
			if (icon)
			{
				nid.uFlags |= NIF_ICON;
				nid.hIcon = icon;
			}			
			if (tip)
			{
				nid.uFlags |= NIF_TIP;
				_tcsncpy(nid.szTip,string_os_from_utf8(tip),tabsize(nid.szTip)-1);
			}
		}

		return do_action(action,p_nid);
	}

	UTF8API_EXPORT BOOL uShellNotifyIconEx(DWORD action,HWND wnd,UINT id,UINT callbackmsg,HICON icon,const char * tip,const char * balloon_title,const char * balloon_msg)
	{
		//if (!balloon_title && !balloon_msg) return uShellNotifyIcon(action,wnd,id,callbackmsg,icon,tip);
#ifdef UNICODE
		if (!is_win2k_or_newer()) return FALSE;

		NOTIFYICONDATA_NEW nid;
		memset(&nid,0,sizeof(nid));
		nid.cbSize = sizeof(nid);
		nid.hWnd = wnd;
		nid.uID = id;
		if (callbackmsg)
		{
			nid.uFlags |= NIF_MESSAGE;
			nid.uCallbackMessage = callbackmsg;
		}
		if (icon)
		{
			nid.uFlags |= NIF_ICON;
			nid.hIcon = icon;
		}			
		if (tip)
		{
			nid.uFlags |= NIF_TIP;
			_tcsncpy(nid.szTip,string_os_from_utf8(tip),tabsize(nid.szTip)-1);
		}

		nid.dwInfoFlags = NIIF_INFO | NIIF_NOSOUND;
		//if (balloon_title || balloon_msg)
		{
			nid.uFlags |= NIF_INFO;
			if (balloon_title) _tcsncpy(nid.szInfoTitle,string_os_from_utf8(balloon_title),tabsize(nid.szInfoTitle)-1);
			if (balloon_msg) _tcsncpy(nid.szInfo,string_os_from_utf8(balloon_msg),tabsize(nid.szInfo)-1);	
		}
		return do_action(action,reinterpret_cast<NOTIFYICONDATA*>(&nid));

#else
		return FALSE;
#endif

	}

}//extern "C"