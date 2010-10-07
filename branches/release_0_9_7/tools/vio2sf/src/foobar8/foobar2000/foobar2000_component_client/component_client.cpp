#include "../SDK/foobar2000.h"
#include "../SDK/component.h"

static HINSTANCE g_hIns;

static string_simple g_name,g_full_path;

static bool g_services_available = false;

#ifdef _DEBUG
static void selftest()
{
	assert(sizeof(bool)==1);
	assert(sizeof(int)==4);
	assert(sizeof(__int64)==8);
}
#endif

extern "C"
{
	__declspec(dllexport) foobar2000_client * _cdecl foobar2000_get_interface(foobar2000_api * p_api,HINSTANCE hIns)
	{
#ifdef _DEBUG
		selftest();
#endif
		cfg_var::config_on_app_init();
		g_hIns = hIns;
		g_api = p_api;

		return &g_client;
	}
}


namespace core_api
{

	HINSTANCE get_my_instance()
	{
		return g_hIns;
	}

	HWND get_main_window()
	{
		return g_api->get_main_window();
	}
	const char * get_my_file_name()
	{
		return g_name;
	}

	const char * get_my_full_path()
	{
		return g_full_path;
	}

	bool are_services_available()
	{
		return g_services_available;
	}
	bool assert_main_thread()
	{
		return (g_services_available && g_api) ? g_api->assert_main_thread() : true;
	}

	bool is_main_thread()
	{
		return (g_services_available && g_api) ? g_api->is_main_thread() : true;
	}
	const char * get_profile_path()
	{
		return (g_services_available && g_api) ? g_api->get_profile_path() : 0;
	}
}

void foobar2000_client::set_library_path(const char * path,const char * name)
{
	g_full_path = path;
	g_name = name;
}

void foobar2000_client::services_init(bool val)
{
	g_services_available = val;
}

#ifndef _DEBUG
#ifdef _MSC_VER
#if _MSC_VER==1200
#pragma comment(linker,"/opt:nowin98")
#endif
#endif
#endif