#include "stdafx.h"
#include "window_placement_helper.h"

//introduced in win98/win2k
typedef HANDLE (WINAPI * t_MonitorFromRect)(const RECT*, DWORD);

#ifndef MONITOR_DEFAULTTONULL
#define MONITOR_DEFAULTTONULL       0x00000000
#endif

static bool test_rect(const RECT * rc)
{
	t_MonitorFromRect p_MonitorFromRect = (t_MonitorFromRect)GetProcAddress(GetModuleHandle("user32"),"MonitorFromRect");
	if (!p_MonitorFromRect)
	{
		//if (!SystemParametersInfo(SPI_GETWORKAREA,0,&workarea,0)) return true;
		int max_x = GetSystemMetrics(SM_CXSCREEN), max_y = GetSystemMetrics(SM_CYSCREEN);
		return rc->left < max_x && rc->right > 0 && rc->top < max_y && rc->bottom > 0;
	}
	return !!p_MonitorFromRect(rc,MONITOR_DEFAULTTONULL);
}


bool apply_window_placement(const cfg_struct_t<WINDOWPLACEMENT> & src,HWND wnd)
{
	bool rv = false;
	if (config_var_int::g_get_value("CORE/remember window positions"))
	{
		WINDOWPLACEMENT wp = src;
		if (wp.length==sizeof(wp) && test_rect(&wp.rcNormalPosition))
		{
			if (SetWindowPlacement(wnd,&wp))
				rv = true;
		}
	}
	return rv;
}

void read_window_placement(cfg_struct_t<WINDOWPLACEMENT> & dst,HWND wnd)
{
	WINDOWPLACEMENT wp;
	memset(&wp,0,sizeof(wp));
	if (config_var_int::g_get_value("CORE/remember window positions"))
	{
		wp.length = sizeof(wp);
		if (!GetWindowPlacement(wnd,&wp))
			memset(&wp,0,sizeof(wp));
	}
	dst = wp;
}

