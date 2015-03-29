//! Callback object class for main_thread_callback_manager service.
class NOVTABLE main_thread_callback : public service_base {
public:
	//! Gets called from main app thread. See main_thread_callback_manager description for more info.
	virtual void callback_run() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(main_thread_callback,service_base);
};

/*!
Allows you to queue a callback object to be called from main app thread. This is commonly used to trigger main-thread-only API calls from worker threads.\n
This can be also used from main app thread, to avoid race conditions when trying to use APIs that dispatch global callbacks from inside some other global callback, or using message loops / modal dialogs inside global callbacks.
*/
class NOVTABLE main_thread_callback_manager : public service_base {
public:
	//! Queues a callback object. This can be called from any thread, implementation ensures multithread safety. Implementation will call p_callback->callback_run() once later. To get it called repeatedly, you would need to add your callback again.
	virtual void add_callback(service_ptr_t<main_thread_callback> p_callback) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(main_thread_callback_manager);
};
