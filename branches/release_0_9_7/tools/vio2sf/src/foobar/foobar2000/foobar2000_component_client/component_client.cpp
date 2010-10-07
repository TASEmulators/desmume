#include "../SDK/foobar2000.h"
#include "../SDK/component.h"

static HINSTANCE g_hIns;

static pfc::string_simple g_name,g_full_path;

static bool g_services_available = false, g_initialized = false;



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
	pcchar get_my_file_name()
	{
		return g_name;
	}

	pcchar get_my_full_path()
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

	void ensure_main_thread() {
		if (!assert_main_thread()) throw exception_wrong_thread();
	}

	bool is_main_thread()
	{
		return (g_services_available && g_api) ? g_api->is_main_thread() : true;
	}
	pcchar get_profile_path()
	{
		return (g_services_available && g_api) ? g_api->get_profile_path() : 0;
	}

	bool is_shutting_down()
	{
		return (g_services_available && g_api) ? g_api->is_shutting_down() : g_initialized;
	}
	bool is_initializing()
	{
		return (g_services_available && g_api) ? g_api->is_initializing() : !g_initialized;
	}
}

namespace {
	class foobar2000_client_impl : public foobar2000_client
	{
	public:
		t_uint32 get_version() {return FOOBAR2000_CLIENT_VERSION;}
		pservice_factory_base get_service_list() {return service_factory_base::__internal__list;}

		void get_config(stream_writer * p_stream,abort_callback & p_abort) {
			cfg_var::config_write_file(p_stream,p_abort);
		}

		void set_config(stream_reader * p_stream,abort_callback & p_abort) {
			cfg_var::config_read_file(p_stream,p_abort);
		}

		void set_library_path(const char * path,const char * name) {
			g_full_path = path;
			g_name = name;
		}

		void services_init(bool val) {
			if (val) g_initialized = true;
			g_services_available = val;
		}

		bool is_debug() {
#ifdef _DEBUG
			return true;
#else
			return false;
#endif
		}
	};
}

static foobar2000_client_impl g_client;

extern "C"
{
	__declspec(dllexport) foobar2000_client * _cdecl foobar2000_get_interface(foobar2000_api * p_api,HINSTANCE hIns)
	{
		g_hIns = hIns;
		g_api = p_api;

		return &g_client;
	}
}

#if 0
BOOLEAN WINAPI DllMain(IN HINSTANCE hDllHandle, IN DWORD     nReason,  IN LPVOID  Reserved )
{
    BOOLEAN bSuccess = TRUE;

    switch ( nReason ) {
        case DLL_PROCESS_ATTACH:

            DisableThreadLibraryCalls( hDllHandle );

            break;

        case DLL_PROCESS_DETACH:

            break;
    }
	return TRUE;
}
#endif