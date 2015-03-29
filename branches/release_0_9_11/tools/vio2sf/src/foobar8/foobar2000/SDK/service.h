#ifndef _SERVICE_H_
#define _SERVICE_H_

#include "../../pfc/pfc.h"

#include "utf8api.h"

#include "core_api.h"

long interlocked_increment(long * var);//note: win32 sucks, return values arent reliable, they can be used only to determine if new value is <0, 0 or >0
long interlocked_decrement(long * var);

class NOVTABLE service_base		//ALL service classes MUST inherit from this
{
public:	
	virtual int service_release() = 0;
	virtual int service_add_ref() = 0;
	virtual service_base * service_query(const GUID & guid) {return 0;}
};

#include "service_impl.h"
/*
//msvc6 sucks

template<class T>
	static T* service_query_t(service_base * ptr) {return static_cast<T*>(ptr->service_query(T::get_class_guid()));}

*/
#define service_query_t(T,ptr) (static_cast<T*>(ptr->service_query(T::get_class_guid())))
//user service_release on returned object when youre done

class NOVTABLE service_factory_base
{
private:
	static service_factory_base *list;
	service_factory_base * next;	
	GUID guid;
protected:
	service_factory_base(const GUID & p_guid) {assert(!core_api::are_services_available());guid=p_guid;next=list;list=this;}
public:

	inline const GUID & get_class_guid() const {return guid;}

	inline static service_factory_base * list_get_pointer() {return list;}
	inline service_factory_base * list_get_next() {return next;}

	static service_base * enum_create(const GUID &g,int n);
	static int enum_get_count(const GUID &g);
	inline static bool is_service_present(const GUID & g) {return enum_get_count(g)>0;}

#ifdef FOOBAR2000_EXE
	static void process_components_directory(const char * path,service_factory_base ** & blah);
	static void on_app_init(const char * exe_path);
	static void on_app_shutdown();
	static void config_reset(const char * name = 0);
	static void on_app_post_init();
	static void on_saveconfig(bool b_reset=false);
#endif

	//obsolete, use core_api
	inline static HINSTANCE get_my_instance() {return core_api::get_my_instance();}
	inline static const char * get_my_file_name() {return core_api::get_my_file_name();}
	inline static const char * get_my_full_path() {return core_api::get_my_full_path();}
	inline static HWND get_main_window() {return core_api::get_main_window();}

	virtual service_base * instance_create() = 0;

};


/*

//msvc6 sucks

template<class T>
	static T * service_enum_create_t(int n) {return (T*)service_factory_base::enum_create(T::get_class_guid(),n);}
template<class T>
	static int service_enum_get_count_t() {return service_factory_base::enum_get_count(T::get_class_guid());}
*/

#define service_enum_create_t(T,n) (static_cast<T*>(service_factory_base::enum_create(T::get_class_guid(),n)))
#define service_enum_get_count_t(T) (service_factory_base::enum_get_count(T::get_class_guid()))
#define service_enum_is_present(g) (service_factory_base::is_service_present(g))
#define service_enum_is_present_t(T) (service_factory_base::is_service_present(T::get_class_guid()))


class service_enum
{
private:
	int service_ptr;
	GUID guid;
public:
	service_enum(const GUID &);
	void reset();
	service_base * first() {reset();return next();}
	service_base * next();
};

template<class B>
class service_enum_t : private service_enum
{
public:
	service_enum_t() : service_enum(B::get_class_guid()) {}
	void reset() {service_enum::reset();}
	B * first() {return (B*)(service_enum::first());}
	B * next() {return (B*)(service_enum::next());}
};


template<class B,class T>
class service_factory_t : public service_factory_base
{
public:
	service_factory_t() : service_factory_base(B::get_class_guid())
	{
	}

	~service_factory_t()
	{
	}

	virtual service_base * instance_create()
	{
		return (service_base*)(B*)(T*)new service_impl_t<T>;
	}
};

template<class B,class T>
class service_factory_single_t : public service_factory_base
{
	service_impl_single_t<T> g_instance;
public:
	service_factory_single_t() : service_factory_base(B::get_class_guid()) {}

	~service_factory_single_t() {}

	virtual service_base * instance_create()
	{
		g_instance.service_add_ref();
		return (service_base*)(B*)(T*)&g_instance;
	}

	inline T& get_static_instance() const {return (T&)g_instance;}
};

template<class B,class T>
class service_factory_single_ref_t : public service_factory_base
{
private:
	T & instance;
public:
	service_factory_single_ref_t(T& param) : instance(param), service_factory_base(B::get_class_guid()) {}

	virtual service_base * instance_create()
	{
		service_base * ret = dynamic_cast<service_base*>(&instance);
		ret->service_add_ref();
		return ret;
	}

	inline T& get_static_instance() {return instance;}

	virtual void instance_release(service_base * ptr) {assert(0);}
};


template<class B,class T>
class service_factory_single_transparent_t : public service_factory_base, public service_impl_single_t<T>
{	
public:
	service_factory_single_transparent_t() : service_factory_base(B::get_class_guid()) {}

	virtual service_base * instance_create()
	{
		service_add_ref();
		return (service_base*)(B*)(T*)this;
	}

	inline T& get_static_instance() const {return *(T*)this;}
};

template<class B,class T,class P1>
class service_factory_single_transparent_p1_t : public service_factory_base, public service_impl_single_p1_t<T,P1>
{	
public:
	service_factory_single_transparent_p1_t(P1 p1) : service_factory_base(B::get_class_guid()), service_impl_single_p1_t<T,P1>(p1) {}


	virtual service_base * instance_create()
	{
		service_add_ref();
		return (service_base*)(B*)(T*)this;
	}

	inline T& get_static_instance() const {return *(T*)this;}
};

template<class B,class T,class P1,class P2>
class service_factory_single_transparent_p2_t : public service_factory_base, public service_impl_single_p2_t<T,P1,P2>
{	
public:
	service_factory_single_transparent_p2_t(P1 p1,P2 p2) : service_factory_base(B::get_class_guid()), service_impl_single_p2_t<T,P1,P2>(p1,p2) {}


	virtual service_base * instance_create()
	{
		service_add_ref();
		return (service_base*)(B*)(T*)this;
	}

	inline T& get_static_instance() const {return *(T*)this;}
};

template<class B,class T,class P1,class P2,class P3>
class service_factory_single_transparent_p3_t : public service_factory_base, public service_impl_single_p3_t<T,P1,P2,P3>
{	
public:
	service_factory_single_transparent_p3_t(P1 p1,P2 p2,P3 p3) : service_factory_base(B::get_class_guid()), service_impl_single_p3_t<T,P1,P2,P3>(p1,p2,p3) {}


	virtual service_base * instance_create()
	{
		service_add_ref();
		return (service_base*)(B*)(T*)this;
	}

	inline T& get_static_instance() const {return *(T*)this;}
};

template<class B,class T,class P1,class P2,class P3,class P4>
class service_factory_single_transparent_p4_t : public service_factory_base, public service_impl_single_p4_t<T,P1,P2,P3,P4>
{	
public:
	service_factory_single_transparent_p4_t(P1 p1,P2 p2,P3 p3,P4 p4) : service_factory_base(B::get_class_guid()), service_impl_single_p4_t<T,P1,P2,P3,P4>(p1,p2,p3,p4) {}


	virtual service_base * instance_create()
	{
		service_add_ref();
		return (service_base*)(B*)(T*)this;
	}

	inline T& get_static_instance() const {return *(T*)this;}
};

#endif