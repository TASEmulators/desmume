#ifndef _COMPONENT_H_
#define _COMPONENT_H_

#include "foobar2000.h"

class NOVTABLE foobar2000_client
{
public:
	typedef service_factory_base* pservice_factory_base;

	enum {FOOBAR2000_CLIENT_VERSION_COMPATIBLE = 70, FOOBAR2000_CLIENT_VERSION = 73};	//changes everytime global compatibility is broken
	virtual t_uint32 FB2KAPI get_version() = 0;
	virtual pservice_factory_base FB2KAPI get_service_list() = 0;

	virtual void FB2KAPI get_config(stream_writer * p_stream,abort_callback & p_abort) = 0;
	virtual void FB2KAPI set_config(stream_reader * p_stream,abort_callback & p_abort) = 0;
	virtual void FB2KAPI set_library_path(const char * path,const char * name) = 0;
	virtual void FB2KAPI services_init(bool val) = 0;
	virtual bool is_debug() = 0;
protected:
	foobar2000_client() {}
	~foobar2000_client() {}
};

class NOVTABLE foobar2000_api
{
public:
	virtual service_class_ref FB2KAPI service_enum_find_class(const GUID & p_guid) = 0;
	virtual bool FB2KAPI service_enum_create(service_ptr_t<service_base> & p_out,service_class_ref p_class,t_size p_index) = 0;
	virtual t_size FB2KAPI service_enum_get_count(service_class_ref p_class) = 0;
	virtual HWND FB2KAPI get_main_window()=0;
	virtual bool FB2KAPI assert_main_thread()=0;
	virtual bool FB2KAPI is_main_thread()=0;
	virtual bool FB2KAPI is_shutting_down()=0;
	virtual pcchar FB2KAPI get_profile_path()=0;
	virtual bool FB2KAPI is_initializing() = 0;
protected:
	foobar2000_api() {}
	~foobar2000_api() {}
};

extern foobar2000_api * g_api;

#endif
