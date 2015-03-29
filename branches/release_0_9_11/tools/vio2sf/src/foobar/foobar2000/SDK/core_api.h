#ifndef _CORE_API_H_
#define _CORE_API_H_

namespace core_api {

	//! Exception thrown by APIs locked to main app thread when called from another thread.
	PFC_DECLARE_EXCEPTION(exception_wrong_thread,pfc::exception_bug_check,"This method can be called only from the main thread");

	//! Retrieves HINSTANCE of calling DLL.
	HINSTANCE get_my_instance();
	//! Retrieves filename of calling dll, excluding extension, e.g. "foo_asdf"
	const char * get_my_file_name();
	//! Retrieves full path of calling dll, e.g. file://c:\blah\foobar2000\foo_asdf.dll
	const char * get_my_full_path();
	//! Retrieves main app window. WARNING: this is provided for parent of dialog windows and such only; using it for anything else (such as hooking windowproc to alter app behaviors) is absolutely illegal.
	HWND get_main_window();
	//! Tests whether services are available at this time. They are not available only during DLL startup or shutdown (e.g. inside static object constructors or destructors).
	bool are_services_available();
	//! Tests whether calling thread is main app thread, and shows diagnostic message in debugger output if it's not.
	bool assert_main_thread();
	//! Throws exception_wrong_thread if calling thread is not main app thread.
	void ensure_main_thread();
	//! Returns true if calling thread is main app thread, false otherwise.
	bool is_main_thread();
	//! Returns whether the app is currently shutting down.
	bool is_shutting_down();
	//! Returns whether the app is currently initializing.
	bool is_initializing();
	//! Returns filesystem path to directory with user settings, e.g. file://c:\documents_and_settings\username\blah\foobar2000
	const char * get_profile_path();
};

#endif
