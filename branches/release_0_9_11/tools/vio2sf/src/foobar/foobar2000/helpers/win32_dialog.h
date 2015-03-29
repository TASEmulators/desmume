#ifndef _FOOBAR2000_HELPERS_WIN32_DIALOG_H_
#define _FOOBAR2000_HELPERS_WIN32_DIALOG_H_

//DEPRECATED dialog helpers - kept only for compatibility with old code - do not use in new code, use WTL instead.

namespace dialog_helper
{
	
	class dialog
	{
	protected:
		
		dialog() : wnd(0), m_is_modal(false) {}
		~dialog() { }

		virtual BOOL on_message(UINT msg,WPARAM wp,LPARAM lp)=0;

		void end_dialog(int code);

	public:
		inline HWND get_wnd() {return wnd;}

		__declspec(deprecated) int run_modal(unsigned id,HWND parent);

		__declspec(deprecated) HWND run_modeless(unsigned id,HWND parent);
	private:
		HWND wnd;
		static INT_PTR CALLBACK DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);

		bool m_is_modal;

		modal_dialog_scope m_modal_scope;
	};

	//! This class is meant to be instantiated on-stack, as a local variable. Using new/delete operators instead or even making this a member of another object works, but does not make much sense because of the way this works (single run() call).
	class dialog_modal
	{
	public:
		__declspec(deprecated) int run(unsigned p_id,HWND p_parent,HINSTANCE p_instance = core_api::get_my_instance());
	protected:
		virtual BOOL on_message(UINT msg,WPARAM wp,LPARAM lp)=0;

		inline dialog_modal() : m_wnd(0) {}
		void end_dialog(int p_code);
		inline HWND get_wnd() const {return m_wnd;}
	private:
		static INT_PTR CALLBACK DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);

		HWND m_wnd;
		modal_dialog_scope m_modal_scope;
	};

	//! This class is meant to be used with new/delete operators only. Destroying the window - outside create() / WM_INITDIALOG - will result in object calling delete this. If object is deleted directly using delete operator, WM_DESTROY handler may not be called so it should not be used (use destructor of derived class instead).
	//! Classes derived from dialog_modeless must not be instantiated in any other way than operator new().
	/*! Typical usage : \n
	class mydialog : public dialog_helper::dialog_modeless {...};
	(...)
	bool createmydialog()
	{
		mydialog * instance = new mydialog;
		if (instance == 0) return flase;
		if (!instance->create(...)) {delete instance; return false;}
		return true;
	}

	*/
	class dialog_modeless
	{
	public:
		//! Creates the dialog window. This will call on_message with WM_INITDIALOG. To abort creation, you can call DestroyWindow() on our window; it will not delete the object but make create() return false instead. You should not delete the object from inside WM_INITDIALOG handler or anything else possibly called from create().
		//! @returns true on success, false on failure.
		__declspec(deprecated) bool create(unsigned p_id,HWND p_parent,HINSTANCE p_instance = core_api::get_my_instance());
	protected:
		//! Standard windows message handler (DialogProc-style). Use get_wnd() to retrieve our dialog window handle.
		virtual BOOL on_message(UINT msg,WPARAM wp,LPARAM lp)=0;

		inline dialog_modeless() : m_wnd(0), m_destructor_status(destructor_none), m_is_in_create(false) {}
		inline HWND get_wnd() const {return m_wnd;}
		virtual ~dialog_modeless();
	private:
		static INT_PTR CALLBACK DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
		void on_window_destruction();
		
		BOOL on_message_wrap(UINT msg,WPARAM wp,LPARAM lp);

		HWND m_wnd;
		enum {destructor_none,destructor_normal,destructor_fromwindow} m_destructor_status;
		bool m_is_in_create;
	};


	class dialog_modeless_v2
	{
	protected:
		__declspec(deprecated) explicit dialog_modeless_v2(unsigned p_id,HWND p_parent,HINSTANCE p_instance = core_api::get_my_instance(),bool p_stealfocus = true);
		virtual ~dialog_modeless_v2();
		HWND get_wnd() const {return m_wnd;}
		virtual BOOL on_message(UINT msg,WPARAM wp,LPARAM lp) {return FALSE;}

		static dialog_modeless_v2 * __unsafe__instance_from_window(HWND p_wnd) {return reinterpret_cast<dialog_modeless_v2*>(GetWindowLongPtr(p_wnd,DWLP_USER));}
	private:
		static INT_PTR CALLBACK DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
		void detach_window();
		BOOL on_message_internal(UINT msg,WPARAM wp,LPARAM lp);
		enum {status_construction, status_lifetime, status_destruction_requested, status_destruction} m_status;
		HWND m_wnd;
		const bool m_stealfocus;

		const dialog_modeless_v2 & operator=(const dialog_modeless_v2 &);
		dialog_modeless_v2(const dialog_modeless_v2 &);
	};

};

//! Wrapper (provided mainly for old code), simplifies parameters compared to standard CreateDialog() by using core_api::get_my_instance().
HWND uCreateDialog(UINT id,HWND parent,DLGPROC proc,LPARAM param = 0);
//! Wrapper (provided mainly for old code), simplifies parameters compared to standard DialogBox() by using core_api::get_my_instance().
int uDialogBox(UINT id,HWND parent,DLGPROC proc,LPARAM param = 0);


#endif
