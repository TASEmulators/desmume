#include "stdafx.h"


namespace dialog_helper {


	BOOL CALLBACK dialog::DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		dialog * p_this;
		BOOL rv;
		if (msg==WM_INITDIALOG)
		{
			p_this = reinterpret_cast<dialog*>(lp);
			p_this->wnd = wnd;
			uSetWindowLong(wnd,DWL_USER,lp);
		}
		else p_this = reinterpret_cast<dialog*>(uGetWindowLong(wnd,DWL_USER));

		rv = p_this ? p_this->on_message(msg,wp,lp) : FALSE;

		if (msg==WM_DESTROY && p_this)
		{
			uSetWindowLong(wnd,DWL_USER,0);
			wnd = 0;
		}

		return rv;
	}

	void set_item_text_multi(HWND wnd,const set_item_text_multi_param * param,unsigned count)
	{
		unsigned n;
		for(n=0;n<count;n++)
		{
			if (param[n].id)
				uSetDlgItemText(wnd,param[n].id,param[n].text);
			else
				uSetWindowText(wnd,param[n].text);
		}
	}
}