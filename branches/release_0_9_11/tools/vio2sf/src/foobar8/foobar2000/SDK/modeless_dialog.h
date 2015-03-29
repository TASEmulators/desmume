#ifndef _MODELESS_DIALOG_H_
#define _MODELESS_DIALOG_H_

#include "service.h"

//use this to hook your modeless dialogs to main message loop and get IsDialogMessage() stuff
//not needed for config pages

class modeless_dialog_manager : public service_base
{
private:
	virtual void _add(HWND wnd)=0;
	virtual void _remove(HWND wnd)=0;
public:
	static void add(HWND wnd);
	static void remove(HWND wnd);

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

#endif //_MODELESS_DIALOG_H_