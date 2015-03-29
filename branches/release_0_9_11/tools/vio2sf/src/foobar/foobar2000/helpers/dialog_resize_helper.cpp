#include "stdafx.h"
#include "dialog_resize_helper.h"

BOOL GetChildWindowRect(HWND wnd,UINT id,RECT* child)
{
	RECT temp;
	HWND wndChild = GetDlgItem(wnd, id);
	if (wndChild == NULL) return FALSE;
	if (!GetWindowRect(wndChild,&temp)) return FALSE;
	if (!MapWindowPoints(0,wnd,(POINT*)&temp,2)) return FALSE;
	*child = temp;
	return TRUE;
}

void dialog_resize_helper::set_parent(HWND wnd)
{
	reset();
	parent = wnd;
	GetClientRect(parent,&orig_client);
}

void dialog_resize_helper::reset()
{
	parent = 0;
	sizegrip = 0;
}

void dialog_resize_helper::on_wm_size()
{
	if (parent)
	{
		unsigned count = m_table.get_size();
		if (sizegrip != 0) count++;
		HDWP hWinPosInfo = BeginDeferWindowPos(count);
		for(unsigned n=0;n<m_table.get_size();n++)
		{
			param & e = m_table[n];
			const RECT & orig_rect = rects[n];
			RECT cur_client;
			GetClientRect(parent,&cur_client);
			HWND wnd = GetDlgItem(parent,e.id);
			if (wnd != NULL) {
				unsigned dest_x = orig_rect.left, dest_y = orig_rect.top, 
					dest_cx = orig_rect.right - orig_rect.left, dest_cy = orig_rect.bottom - orig_rect.top;

				int delta_x = cur_client.right - orig_client.right,
					delta_y = cur_client.bottom - orig_client.bottom;

				if (e.flags & X_MOVE)
					dest_x += delta_x;
				else if (e.flags & X_SIZE)
					dest_cx += delta_x;

				if (e.flags & Y_MOVE)
					dest_y += delta_y;
				else if (e.flags & Y_SIZE)
					dest_cy += delta_y;
				
				DeferWindowPos(hWinPosInfo, wnd,0,dest_x,dest_y,dest_cx,dest_cy,SWP_NOZORDER);
			}
		}
		if (sizegrip != 0)
		{
			RECT rc, rc_grip;
			GetClientRect(parent, &rc);
			GetWindowRect(sizegrip, &rc_grip);
			DeferWindowPos(hWinPosInfo, sizegrip, NULL, rc.right - (rc_grip.right - rc_grip.left), rc.bottom - (rc_grip.bottom - rc_grip.top), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}
		EndDeferWindowPos(hWinPosInfo);
		//RedrawWindow(parent,0,0,RDW_INVALIDATE);
	}
}

bool dialog_resize_helper::process_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp) {
	LRESULT result = 0;
	if (!ProcessWindowMessage(wnd,msg,wp,lp,result)) return false;
	SetWindowLongPtr(wnd,DWLP_MSGRESULT,result);
	return true;
}

BOOL dialog_resize_helper::ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult) {
	switch(uMsg) {
	case WM_SIZE:
		on_wm_size();
		return FALSE;
	case WM_GETMINMAXINFO:
		{
			RECT r;
			LPMINMAXINFO info = (LPMINMAXINFO) lParam;
			DWORD dwStyle = GetWindowLong(hWnd, GWL_STYLE);
			DWORD dwExStyle = GetWindowLong(hWnd, GWL_EXSTYLE);
			if (max_x && max_y)
			{
				r.left = 0; r.right = max_x;
				r.top = 0; r.bottom = max_y;
				MapDialogRect(hWnd,&r);
				AdjustWindowRectEx(&r, dwStyle, FALSE, dwExStyle);
				info->ptMaxTrackSize.x = r.right - r.left;
				info->ptMaxTrackSize.y = r.bottom - r.top;
			}
			if (min_x && min_y)
			{
				r.left = 0; r.right = min_x;
				r.top = 0; r.bottom = min_y;
				MapDialogRect(hWnd,&r);
				AdjustWindowRectEx(&r, dwStyle, FALSE, dwExStyle);
				info->ptMinTrackSize.x = r.right - r.left;
				info->ptMinTrackSize.y = r.bottom - r.top;
			}
		}
		lResult = 0;
		return TRUE;
	case WM_INITDIALOG:
		set_parent(hWnd);
		{
			t_size n;
			for(n=0;n<m_table.get_size();n++) {
				GetChildWindowRect(parent,m_table[n].id,&rects[n]);
			}
		}
		return FALSE;
	case WM_DESTROY:
		reset();
		return FALSE;
	default:
		return FALSE;
	}
}

void dialog_resize_helper::add_sizegrip()
{
	if (parent != 0 && sizegrip == 0)
	{
		sizegrip = CreateWindowEx(0, WC_SCROLLBAR, _T(""), WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN,
			0, 0, CW_USEDEFAULT, CW_USEDEFAULT,
			parent, (HMENU)0, NULL, NULL);
		if (sizegrip != 0)
		{
			RECT rc, rc_grip;
			GetClientRect(parent, &rc);
			GetWindowRect(sizegrip, &rc_grip);
			SetWindowPos(sizegrip, NULL, rc.right - (rc_grip.right - rc_grip.left), rc.bottom - (rc_grip.bottom - rc_grip.top), 0, 0, SWP_NOZORDER | SWP_NOSIZE);
		}
	}
}


dialog_resize_helper::dialog_resize_helper(const param * src,unsigned count,unsigned p_min_x,unsigned p_min_y,unsigned p_max_x,unsigned p_max_y) 
	: min_x(p_min_x), min_y(p_min_y), max_x(p_max_x), max_y(p_max_y), parent(0), sizegrip(0)
{
	m_table.set_size(count);
	unsigned n;
	for(n=0;n<count;n++)
		m_table[n] = src[n];
	rects.set_size(count);
}

dialog_resize_helper::~dialog_resize_helper()
{
}
