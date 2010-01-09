#include "stdafx.h"
#include "dialog_resize_helper.h"

void resize::calc_xy(HWND wnd,UINT id,RECT &r,RECT & o)
{
	RECT c;
	GetClientRect(wnd,&c);
	SetWindowPos(GetDlgItem(wnd,id),0,0,0,
		r.right-r.left+c.right-o.right,
		r.bottom-r.top+c.bottom-o.bottom,
		SWP_NOMOVE|SWP_NOZORDER);
}

void resize::calc_move_xy(HWND wnd,UINT id,RECT &r,RECT & o)
{
	RECT c;
	GetClientRect(wnd,&c);
	SetWindowPos(GetDlgItem(wnd,id),0,
		c.right-o.right+r.left,
		c.bottom-o.bottom+r.top,
		0,0,SWP_NOSIZE|SWP_NOZORDER);
}

void resize::calc_move_x(HWND wnd,UINT id,RECT &r,RECT & o)
{
	RECT c;
	GetClientRect(wnd,&c);
	SetWindowPos(GetDlgItem(wnd,id),0,
		c.right-o.right+r.left,
		r.top,
		0,0,SWP_NOSIZE|SWP_NOZORDER);
}

void resize::calc_move_x_size_y(HWND wnd,UINT id,RECT &r,RECT & o)
{
	RECT c;
	GetClientRect(wnd,&c);
	SetWindowPos(GetDlgItem(wnd,id),0,
		c.right-o.right+r.left,
		r.top,
		r.right-r.left,
		r.bottom-r.top+c.bottom-o.bottom,
		SWP_NOZORDER);
}

void resize::calc_move_y(HWND wnd,UINT id,RECT &r,RECT & o)
{
	RECT c;
	GetClientRect(wnd,&c);
	SetWindowPos(GetDlgItem(wnd,id),0,
		r.left,
		c.bottom-o.bottom+r.top,
		0,0,SWP_NOSIZE|SWP_NOZORDER);
}

void resize::calc_x(HWND wnd,UINT id,RECT &r,RECT & o)
{
	RECT c;
	GetClientRect(wnd,&c);
	SetWindowPos(GetDlgItem(wnd,id),0,0,0,
		r.right-r.left+c.right-o.right,
		r.bottom-r.top,
		SWP_NOMOVE|SWP_NOZORDER);
}

void GetChildRect(HWND wnd,UINT id,RECT* child)
{
	RECT temp;
	GetWindowRect(GetDlgItem(wnd,id),&temp);
	MapWindowPoints(0,wnd,(POINT*)&temp,2);
	*child = temp;
}

/*
class dialog_resize_helper
{
	struct entry { RECT orig; UINT id; UINT flags };
	mem_block_list<entry> data;
	RECT orig_client;
	HWND parent;
public:
	enum {
		X_MOVE = 1, X_SIZE = 2, Y_MOVE = 4, Y_SIZE = 8
	};
	void set_parent(HWND wnd);
	void add_item(UINT id,UINT flags);
	void reset();
};
*/

void dialog_resize_helper::set_parent(HWND wnd)
{
	reset();
	parent = wnd;
	GetClientRect(parent,&orig_client);
}

void dialog_resize_helper::reset()
{
	parent = 0;
}

void dialog_resize_helper::on_wm_size()
{
	if (parent)
	{
		unsigned n;
		for(n=0;n<m_table_size;n++)
		{
			param & e = m_table[n];
			const RECT & orig_rect = rects[n];
			RECT cur_client;
			GetClientRect(parent,&cur_client);
			HWND wnd = GetDlgItem(parent,e.id);

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
			
			SetWindowPos(wnd,0,dest_x,dest_y,dest_cx,dest_cy,SWP_NOZORDER);
		}
		RedrawWindow(parent,0,0,RDW_INVALIDATE|RDW_UPDATENOW);
	}
}


bool dialog_resize_helper::process_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
{
	switch(msg)
	{
	case WM_SIZE:
		on_wm_size();
		return true;
	case WM_GETMINMAXINFO:
		{
			RECT r;
			LPMINMAXINFO info = (LPMINMAXINFO) lp;
			if (max_x && max_y)
			{
				r.left = 0; r.right = max_x;
				r.top = 0; r.bottom = max_y;
				MapDialogRect(wnd,&r);
				info->ptMaxTrackSize.x = r.right;
				info->ptMaxTrackSize.y = r.bottom;
			}
			if (min_x && min_y)
			{
				r.left = 0; r.right = min_x;
				r.top = 0; r.bottom = min_y;
				MapDialogRect(wnd,&r);
				info->ptMinTrackSize.x = r.right;
				info->ptMinTrackSize.y = r.bottom;
			}
		}
		return true;
	case WM_INITDIALOG:
		set_parent(wnd);
		{
			unsigned n;
			for(n=0;n<m_table_size;n++)
			{
				GetChildRect(parent,m_table[n].id,&rects[n]);
			}
		}
		break;
	case WM_DESTROY:
		reset();
		break;
	}
	return false;
}


dialog_resize_helper::dialog_resize_helper(const param * src,unsigned count,unsigned p_min_x,unsigned p_min_y,unsigned p_max_x,unsigned p_max_y) 
	: min_x(p_min_x), min_y(p_min_y), max_x(p_max_x), max_y(p_max_y), parent(0)
{
	m_table_size = count;
	m_table = new param[count];
	unsigned n;
	for(n=0;n<count;n++)
		m_table[n] = src[n];
	rects = new RECT[count];
}

dialog_resize_helper::~dialog_resize_helper()
{
	delete[] m_table;
	delete[] rects;
}