#ifndef _FOOBAR2000_MENU_ITEM_H_
#define _FOOBAR2000_MENU_ITEM_H_

#include "service.h"
#include "interface_helper.h"
#include "metadb_handle.h"

class NOVTABLE menu_item : public service_base
{
public:
	enum type
	{
		TYPE_MAIN,
		TYPE_CONTEXT,
	};

	enum enabled_state
	{
		DEFAULT_OFF,
		DEFAULT_ON,
		FORCE_ON,
		FORCE_OFF,
	};

	enum
	{
		FLAG_CHECKED = 1,
		FLAG_DISABLED = 2,
		FLAG_GRAYED = 4,
		FLAG_DISABLED_GRAYED = FLAG_DISABLED|FLAG_GRAYED,
	};

	virtual enabled_state get_enabled_state(unsigned idx) {return DEFAULT_ON;}//return if menu item should be enabled by default or not

	virtual type get_type()=0;
	virtual unsigned get_num_items()=0;
	virtual void enum_item(unsigned n,string_base & out)=0;//return full formal name, like "playback/play", 0<=n<get_num_items(), no error return
	
	virtual bool get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags)=0;//displayflags - see FLAG_* above; displayflags are set to 0 before calling
	virtual void perform_command(unsigned n,const ptr_list_base<metadb_handle> & data)=0;

	virtual bool get_description(unsigned n,string_base & out) {return false;}

	virtual void * get_glyph(unsigned n) {return 0;}//RESERVED, do not override
	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	static const GUID caller_now_playing;
	static const GUID caller_playlist;
	static const GUID caller_undefined;

	bool get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags,const GUID & caller);
	void perform_command(unsigned n,const ptr_list_base<metadb_handle> & data,const GUID & caller);
	bool enum_item_guid(unsigned n,GUID & out);
};

class NOVTABLE menu_item_v2 : public menu_item
{
	virtual bool get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags)
	{return get_display_data(n,data,out,displayflags,caller_undefined);}
	virtual void perform_command(unsigned n,const ptr_list_base<metadb_handle> & data)
	{perform_command(n,data,caller_undefined);}
public:
	virtual bool get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags,const GUID & caller)=0;
	virtual void perform_command(unsigned n,const ptr_list_base<metadb_handle> & data,const GUID & caller)=0;
	virtual bool enum_item_guid(unsigned n,GUID & out) {return false;}//returns false if item doesnt have a GUID

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return menu_item::service_query(guid);
	}
};

class NOVTABLE menu_item_main : public menu_item_v2
{
	virtual type get_type() {return TYPE_MAIN;}

	virtual bool get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags,const GUID & caller)
	{
		assert(n>=0 && n<get_num_items());
		if (!is_available(n)) return false;
		string8 temp;
		enum_item(n,temp);
		const char * ptr = strrchr(temp,'/');
		if (ptr) ptr++;
		else ptr = temp;
		out.set_string(ptr);
		displayflags = (is_checked(n) ? FLAG_CHECKED : 0) | (is_disabled(n) ? (FLAG_DISABLED|FLAG_GRAYED) : 0);
		return true;
	}

	virtual void perform_command(unsigned n,const ptr_list_base<metadb_handle> & data,const GUID & caller) {perform_command(n);}
protected:
	//override these
	virtual unsigned get_num_items()=0;
	virtual void enum_item(unsigned n,string_base & out)=0;
	virtual void perform_command(unsigned n)=0;
	virtual bool is_checked(unsigned n) {return false;}
	virtual bool is_disabled(unsigned n) {return false;}
	virtual bool is_available(unsigned n) {return true;}
	virtual bool get_description(unsigned n,string_base & out) {return false;}
};

class NOVTABLE menu_item_main_single : public menu_item_main //helper
{
	virtual unsigned get_num_items() {return 1;}
	virtual void enum_item(unsigned n,string_base & out)
	{
		if (n==0) get_name(out);
	}
	virtual void perform_command(unsigned n)
	{
		if (n==0) run();
	}
	virtual bool is_checked(unsigned n) {assert(n==0); return is_checked();};
	virtual bool is_disabled(unsigned n) {assert(n==0); return is_disabled();}
	virtual bool get_description(unsigned n,string_base & out) {assert(n==0); return get_description(out);}
protected://override these
	virtual void get_name(string_base & out)=0;
	virtual void run()=0;
	virtual bool is_checked() {return false;}
	virtual bool is_disabled() {return false;}
	virtual bool get_description(string_base & out) {return false;}
};

class NOVTABLE menu_item_context : public menu_item_v2
{
	virtual type get_type() {return TYPE_CONTEXT;}
	virtual bool get_display_data(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags,const GUID & caller)
	{
		bool rv = false;
		assert(n>=0 && n<get_num_items());
		if (data.get_count()>0)
		{
			rv = context_get_display(n,data,out,displayflags,caller);
		}
		return rv;
	}
	virtual void perform_command(unsigned n,const ptr_list_base<metadb_handle> & data,const GUID & caller)
	{
		if (data.get_count()>0) context_command(n,data,caller);
	}
	virtual bool is_checked(unsigned n,const ptr_list_base<metadb_handle> & data) {return false;}
protected:
	//override these
	virtual unsigned get_num_items()=0;
	virtual void enum_item(unsigned n,string_base & out)=0;
	virtual void context_command(unsigned n,const ptr_list_base<metadb_handle> & data,const GUID& caller)=0;
	virtual bool context_get_display(unsigned n,const ptr_list_base<metadb_handle> & data,string_base & out,unsigned & displayflags,const GUID &)
	{
		assert(n>=0 && n<get_num_items());
		string8 temp;
		enum_item(n,temp);
		const char * ptr = strrchr(temp,'/');
		if (ptr) ptr++;
		else ptr = temp;
		out.set_string(ptr);
		return true;
	}
};


template<class T>
class menu_item_factory : public service_factory_single_t<menu_item,T> {};


#endif //_FOOBAR2000_MENU_ITEM_H_