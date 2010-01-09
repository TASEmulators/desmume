#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include "foobar2000.h"

class foobar2000_client
{
public:
	enum {FOOBAR2000_CLIENT_VERSION_COMPATIBLE = 27, FOOBAR2000_CLIENT_VERSION=35};	//changes everytime global compatibility is broken
	virtual int get_version() {return FOOBAR2000_CLIENT_VERSION;}
	virtual service_factory_base * get_service_list() {return service_factory_base::list_get_pointer();}

	virtual void get_config(cfg_var::write_config_callback * out)
	{
		cfg_var::config_write_file(out);
	}

	virtual void set_config(const void * data,int size)
	{
		cfg_var::config_read_file(data,size);
	}

	virtual void reset_config()
	{
		//0.8: deprecated
	}

	virtual void set_library_path(const char * path,const char * name);

	virtual void services_init(bool val);

};

class NOVTABLE foobar2000_api
{
public:
	virtual service_base * service_enum_create(const GUID &g,int n)=0;
	virtual int service_enum_get_count(const GUID &g)=0;
	virtual HWND get_main_window()=0;
	virtual bool assert_main_thread()=0;
	virtual bool is_main_thread()=0;
	virtual const char * get_profile_path()=0;
};

extern foobar2000_client g_client;
extern foobar2000_api * g_api;

#endif