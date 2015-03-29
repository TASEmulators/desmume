#ifndef _FOOBAR2000_HELPERS_WIN32_DIALOG_H_
#define _FOOBAR2000_HELPERS_WIN32_DIALOG_H_

namespace dialog_helper
{
	class dialog
	{
		HWND wnd;
		static BOOL CALLBACK DlgProc(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);
	protected:
		
		dialog() : wnd(0) {}
		~dialog() { }

		virtual BOOL on_message(UINT msg,WPARAM wp,LPARAM lp)=0;

		void end_dialog(int code) {uEndDialog(wnd,code);}

	public:
		inline HWND get_wnd() {return wnd;}

		inline int run_modal(unsigned id,HWND parent) {return uDialogBox(id,parent,DlgProc,reinterpret_cast<long>(this));}

		inline HWND run_modeless(unsigned id,HWND parent) {return uCreateDialog(id,parent,DlgProc,reinterpret_cast<long>(this));}
	};

	struct set_item_text_multi_param
	{
		unsigned id;
		const char * text;
	};

	void set_item_text_multi(HWND wnd,const set_item_text_multi_param * param,unsigned count);

};

#endif