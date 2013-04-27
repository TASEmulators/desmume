//! (DEPRECATED) This service is used to signal whether something is currently preventing main window from being closed and app from being shut down.
class NOVTABLE app_close_blocker : public service_base
{
public:
	//! Checks whether this service is currently preventing main window from being closed and app from being shut down.
	virtual bool query() = 0;

	//! Static helper function, checks whether any of registered app_close_blocker services is currently preventing main window from being closed and app from being shut down.
	static bool g_query();

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(app_close_blocker);
};

//! An interface encapsulating a task preventing the foobar2000 application from being closed. Instances of this class need to be registered using app_close_blocking_task_manager methods. \n
//! Implementation: it's recommended that you derive from app_close_blocking_task_impl class instead of deriving from app_close_blocking_task directly, it manages registration/unregistration behind-the-scenes.
class NOVTABLE app_close_blocking_task {
public:
	virtual void query_task_name(pfc::string_base & out) = 0;

protected:
	app_close_blocking_task() {}
	~app_close_blocking_task() {}

	PFC_CLASS_NOT_COPYABLE_EX(app_close_blocking_task);
};

//! Entrypoint class for registering app_close_blocking_task instances. Introduced in 0.9.5.1. \n
//! Usage: static_api_ptr_t<app_close_blocking_task_manager>(). May fail if user runs pre-0.9.5.1. It's recommended that you use app_close_blocking_task_impl class instead of calling app_close_blocking_task_manager directly.
class NOVTABLE app_close_blocking_task_manager : public service_base {
public:
	virtual void register_task(app_close_blocking_task * task) = 0;
	virtual void unregister_task(app_close_blocking_task * task) = 0;
	

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(app_close_blocking_task_manager);
};

//! Helper; implements standard functionality required by app_close_blocking_task implementations - registers/unregisters the task on construction/destruction.
class app_close_blocking_task_impl : public app_close_blocking_task {
public:
	app_close_blocking_task_impl() { try { static_api_ptr_t<app_close_blocking_task_manager>()->register_task(this); } catch(exception_service_not_found) {/*user runs pre-0.9.5.1*/}}
	~app_close_blocking_task_impl() { try { static_api_ptr_t<app_close_blocking_task_manager>()->unregister_task(this); } catch(exception_service_not_found) {/*user runs pre-0.9.5.1*/}}

	void query_task_name(pfc::string_base & out) { out = "<unnamed task>"; }
};
