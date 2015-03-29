#include "stdafx.h"

void registerclass_scope_delayed::toggle_on(UINT p_style,WNDPROC p_wndproc,int p_clsextra,int p_wndextra,HICON p_icon,HCURSOR p_cursor,HBRUSH p_background,const TCHAR * p_class_name,const TCHAR * p_menu_name) {
	toggle_off();
	WNDCLASS wc = {};
	wc.style = p_style;
	wc.lpfnWndProc = p_wndproc;
	wc.cbClsExtra = p_clsextra;
	wc.cbWndExtra = p_wndextra;
	wc.hInstance = core_api::get_my_instance();
	wc.hIcon = p_icon;
	wc.hCursor = p_cursor;
	wc.hbrBackground = p_background;
	wc.lpszMenuName = p_menu_name;
	wc.lpszClassName = p_class_name;
	WIN32_OP( (m_class = RegisterClass(&wc)) != 0);
}

void registerclass_scope_delayed::toggle_off() {
	if (m_class != 0) {
		UnregisterClass((LPCTSTR)m_class,core_api::get_my_instance());
		m_class = 0;
	}
}


unsigned QueryScreenDPI() {
	HDC dc = GetDC(0);
	unsigned ret = GetDeviceCaps(dc,LOGPIXELSY);
	ReleaseDC(0,dc);
	return ret;
}


SIZE QueryScreenDPIEx() {
	HDC dc = GetDC(0);
	SIZE ret = { GetDeviceCaps(dc,LOGPIXELSY), GetDeviceCaps(dc,LOGPIXELSY) };
	ReleaseDC(0,dc);
	return ret;
}


bool IsMenuNonEmpty(HMENU menu) {
	unsigned n,m=GetMenuItemCount(menu);
	for(n=0;n<m;n++) {
		if (GetSubMenu(menu,n)) return true;
		if (!(GetMenuState(menu,n,MF_BYPOSITION)&MF_SEPARATOR)) return true;
	}
	return false;
}

void WIN32_OP_FAIL() {
	PFC_ASSERT( GetLastError() != NO_ERROR );
	throw exception_win32(GetLastError());
}

#ifdef _DEBUG
void WIN32_OP_D_FAIL(const wchar_t * _Message, const wchar_t *_File, unsigned _Line) {
	const DWORD code = GetLastError();
	pfc::array_t<wchar_t> msgFormatted; msgFormatted.set_size(pfc::strlen_t(_Message) + 64);
	wsprintfW(msgFormatted.get_ptr(), L"%s (code: %u)", _Message, code);
	if (IsDebuggerPresent()) {
		OutputDebugString(TEXT("WIN32_OP_D() failure:\n"));
		OutputDebugString(msgFormatted.get_ptr());
		OutputDebugString(TEXT("\n"));
		pfc::crash();
	}
	_wassert(msgFormatted.get_ptr(),_File,_Line);
}
#endif