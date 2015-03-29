#ifndef _FOOBAR2000_UI_H_
#define _FOOBAR2000_UI_H_

#include "service.h"

#ifndef WIN32
#error PORTME
#endif

class NOVTABLE user_interface : public service_base
{
public:
	typedef BOOL (WINAPI * HookProc_t)(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT * ret);
	//HookProc usage:
	//in your windowproc, call HookProc first, and if it returns true, return LRESULT value it passed to you

	virtual const char * get_name()=0;
	virtual HWND init(HookProc_t hook)=0;//create your window here
	virtual void shutdown(bool endsession)=0;//you need to destroy your window here
	virtual void activate()=0;
	virtual void hide()=0;
	virtual bool is_visible()=0;//for activate/hide command

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static user_interface * g_find(const char * name)
	{
		service_enum_t<user_interface> e;
		user_interface * ptr;
		for(ptr=e.first();ptr;ptr=e.next())
		{
			if (!stricmp_utf8(ptr->get_name(),name)) return ptr;
			ptr->service_release();
		}
		return 0;
	}
};

template<class T>
class user_interface_factory : public service_factory_single_t<user_interface,T> {};

//new (0.8)
class NOVTABLE ui_control : public service_base//implemented in the core, do not override
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual bool is_visible()=0;
	virtual void activate()=0;
	virtual void hide()=0;
	virtual HICON get_main_icon()=0;//no need to free returned handle
	virtual HICON load_main_icon(unsigned width,unsigned height)=0;//use DestroyIcon() to free it

	static ui_control * get() {return service_enum_create_t(ui_control,0);}//no need to service_release
};

class NOVTABLE ui_drop_item_callback : public service_base //called from UI when some object (ie. files from explorer) is dropped
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual bool on_drop(interface IDataObject * pDataObject)=0;//return true if you processed the data, false if not
	virtual bool is_accepted_type(interface IDataObject * pDataObject)=0;

	static bool g_on_drop(interface IDataObject * pDataObject);
	static bool g_is_accepted_type(interface IDataObject * pDataObject);
};

template<class T>
class ui_drop_item_callback_factory : public service_factory_single_t<ui_drop_item_callback,T> {};

#endif