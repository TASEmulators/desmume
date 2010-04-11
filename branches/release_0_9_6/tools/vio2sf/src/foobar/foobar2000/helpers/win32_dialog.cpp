#include "stdafx.h"


namespace dialog_helper {


	INT_PTR CALLBACK dialog::DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		dialog * p_this;
		BOOL rv;
		if (msg==WM_INITDIALOG)
		{
			p_this = reinterpret_cast<dialog*>(lp);
			p_this->wnd = wnd;
			SetWindowLongPtr(wnd,DWLP_USER,lp);

			if (p_this->m_is_modal) p_this->m_modal_scope.initialize(wnd);
		}
		else p_this = reinterpret_cast<dialog*>(GetWindowLongPtr(wnd,DWLP_USER));

		rv = p_this ? p_this->on_message(msg,wp,lp) : FALSE;

		if (msg==WM_DESTROY && p_this)
		{
			SetWindowLongPtr(wnd,DWLP_USER,0);
//			p_this->wnd = 0;
		}

		return rv;
	}


	int dialog::run_modal(unsigned id,HWND parent)
	{
		assert(wnd == 0);
		if (wnd != 0) return -1;
		m_is_modal = true; 
		return uDialogBox(id,parent,DlgProc,reinterpret_cast<LPARAM>(this));
	}
	HWND dialog::run_modeless(unsigned id,HWND parent)
	{
		assert(wnd == 0);
		if (wnd != 0) return 0;
		m_is_modal = false; 
		return uCreateDialog(id,parent,DlgProc,reinterpret_cast<LPARAM>(this));
	}

	void dialog::end_dialog(int code)
	{
		assert(m_is_modal); 
		if (m_is_modal) uEndDialog(wnd,code);
	}










	int dialog_modal::run(unsigned p_id,HWND p_parent,HINSTANCE p_instance)
	{
		int status;

		// note: uDialogBox() has its own modal scope, we don't want that to trigger
		// if this is ever changed, move deinit to WM_DESTROY handler in DlgProc

		status = (int)DialogBoxParam(p_instance,MAKEINTRESOURCE(p_id),p_parent,DlgProc,reinterpret_cast<LPARAM>(this));

		m_modal_scope.deinitialize();

		return status;
	}
		
	void dialog_modal::end_dialog(int p_code)
	{
		EndDialog(m_wnd,p_code);
	}

		
	INT_PTR CALLBACK dialog_modal::DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		dialog_modal * _this;
		if (msg==WM_INITDIALOG)
		{
			_this = reinterpret_cast<dialog_modal*>(lp);
			_this->m_wnd = wnd;
			SetWindowLongPtr(wnd,DWLP_USER,lp);

			_this->m_modal_scope.initialize(wnd);
		}
		else _this = reinterpret_cast<dialog_modal*>(GetWindowLongPtr(wnd,DWLP_USER));

		assert(_this == 0 || _this->m_wnd == wnd);

		return _this ? _this->on_message(msg,wp,lp) : FALSE;
	}


	bool dialog_modeless::create(unsigned p_id,HWND p_parent,HINSTANCE p_instance) {
		assert(!m_is_in_create);
		if (m_is_in_create) return false;
		pfc::vartoggle_t<bool> scope(m_is_in_create,true);
		if (CreateDialogParam(p_instance,MAKEINTRESOURCE(p_id),p_parent,DlgProc,reinterpret_cast<LPARAM>(this)) == 0) return false;
		return m_wnd != 0;
	}

	dialog_modeless::~dialog_modeless() {
		assert(!m_is_in_create);
		switch(m_destructor_status)
		{
		case destructor_none:
			m_destructor_status = destructor_normal;
			if (m_wnd != 0)
			{
				DestroyWindow(m_wnd);
				m_wnd = 0;
			}
			break;
		case destructor_fromwindow:
			if (m_wnd != 0) SetWindowLongPtr(m_wnd,DWLP_USER,0);
			break;
		default:
			//should never trigger
			pfc::crash();
			break;
		}
	}

	void dialog_modeless::on_window_destruction()
	{
		if (m_is_in_create)
		{
			m_wnd = 0;
		}
		else
		switch(m_destructor_status)
		{
		case destructor_none:
			m_destructor_status = destructor_fromwindow;
			delete this;
			break;
		case destructor_fromwindow:
			pfc::crash();
			break;
		default:
			break;
		}
	}

	BOOL dialog_modeless::on_message_wrap(UINT msg,WPARAM wp,LPARAM lp)
	{
		if (m_destructor_status == destructor_none)
			return on_message(msg,wp,lp);
		else
			return FALSE;
	}

	INT_PTR CALLBACK dialog_modeless::DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		dialog_modeless * thisptr;
		BOOL rv;
		if (msg == WM_INITDIALOG)
		{
			thisptr = reinterpret_cast<dialog_modeless*>(lp);
			thisptr->m_wnd = wnd;
			SetWindowLongPtr(wnd,DWLP_USER,lp);
			modeless_dialog_manager::g_add(wnd);
		}
		else thisptr = reinterpret_cast<dialog_modeless*>(GetWindowLongPtr(wnd,DWLP_USER));

		rv = thisptr ? thisptr->on_message_wrap(msg,wp,lp) : FALSE;

		if (msg == WM_DESTROY)
			modeless_dialog_manager::g_remove(wnd);

		if (msg == WM_DESTROY && thisptr != 0)
			thisptr->on_window_destruction();

		return rv;
	}
















	dialog_modeless_v2::dialog_modeless_v2(unsigned p_id,HWND p_parent,HINSTANCE p_instance,bool p_stealfocus) : m_wnd(0), m_status(status_construction), m_stealfocus(p_stealfocus)
	{
		WIN32_OP( CreateDialogParam(p_instance,MAKEINTRESOURCE(p_id),p_parent,DlgProc,reinterpret_cast<LPARAM>(this)) != NULL );
		m_status = status_lifetime;
	}

	dialog_modeless_v2::~dialog_modeless_v2()
	{
		bool is_window_being_destroyed = (m_status == status_destruction_requested);
		m_status = status_destruction;

		if (m_wnd != 0)
		{
			if (is_window_being_destroyed)
				detach_window();
			else
				DestroyWindow(m_wnd);
		}
	}
	
	INT_PTR CALLBACK dialog_modeless_v2::DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp)
	{
		dialog_modeless_v2 * thisptr;
		BOOL rv = FALSE;
		if (msg == WM_INITDIALOG)
		{
			thisptr = reinterpret_cast<dialog_modeless_v2*>(lp);
			assert(thisptr->m_status == status_construction);
			thisptr->m_wnd = wnd;
			SetWindowLongPtr(wnd,DWLP_USER,lp);
			if (GetWindowLong(wnd,GWL_STYLE) & WS_POPUP) {
				modeless_dialog_manager::g_add(wnd);
			}
		}
		else thisptr = reinterpret_cast<dialog_modeless_v2*>(GetWindowLongPtr(wnd,DWLP_USER));

		if (thisptr != NULL) rv = thisptr->on_message_internal(msg,wp,lp);

		if (msg == WM_DESTROY)
		{
			modeless_dialog_manager::g_remove(wnd);
		}

		return rv;
	}


	void dialog_modeless_v2::detach_window()
	{
		if (m_wnd != 0)
		{
			SetWindowLongPtr(m_wnd,DWLP_USER,0);
			m_wnd = 0;
		}
	}

	
	BOOL dialog_modeless_v2::on_message_internal(UINT msg,WPARAM wp,LPARAM lp)
	{
		if (m_status == status_lifetime || m_status == status_destruction_requested)
		{
			if (msg == WM_DESTROY)
			{
				assert(m_status == status_lifetime);
				m_status = status_destruction_requested;
				delete this;
				return TRUE;
			}
			else 
				return on_message(msg,wp,lp);
		}
		else if (m_status == status_construction)
		{
			if (msg == WM_INITDIALOG) return m_stealfocus ? TRUE : FALSE;
			else return FALSE;
		}
		else return FALSE;
	}
}

HWND uCreateDialog(UINT id,HWND parent,DLGPROC proc,LPARAM param)
{
	return CreateDialogParam(core_api::get_my_instance(),MAKEINTRESOURCE(id),parent,proc,param);
}

int uDialogBox(UINT id,HWND parent,DLGPROC proc,LPARAM param)
{
	return (int)DialogBoxParam(core_api::get_my_instance(),MAKEINTRESOURCE(id),parent,proc,param);
}
