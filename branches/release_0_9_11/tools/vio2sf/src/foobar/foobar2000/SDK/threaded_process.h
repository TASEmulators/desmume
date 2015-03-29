#ifndef _foobar2000_sdk_threaded_process_h_
#define _foobar2000_sdk_threaded_process_h_

class NOVTABLE threaded_process_status
{
public:
	enum {progress_min = 0, progress_max = 5000};
	
	virtual void set_progress(t_size p_state) = 0;
	virtual void set_progress_secondary(t_size p_state) = 0;
	virtual void set_item(const char * p_item,t_size p_item_len = ~0) = 0;
	virtual void set_item_path(const char * p_item,t_size p_item_len = ~0) = 0;
	virtual void set_title(const char * p_title,t_size p_title_len = ~0) = 0;
	virtual void force_update() = 0;
	virtual bool is_paused() = 0;
	virtual bool process_pause() = 0;//checks if process is paused and sleeps if needed; returns false when process should be aborted, true on success

	void set_progress(t_size p_state,t_size p_max);
	void set_progress_secondary(t_size p_state,t_size p_max);
	void set_progress_float(double p_state);
	void set_progress_secondary_float(double p_state);
protected:
	threaded_process_status() {}
	~threaded_process_status() {}
};


class NOVTABLE threaded_process_callback : public service_base
{
public:
	virtual void on_init(HWND p_wnd) {}
	virtual void run(threaded_process_status & p_status,abort_callback & p_abort) = 0;
	virtual void on_done(HWND p_wnd,bool p_was_aborted) {}

	FB2K_MAKE_SERVICE_INTERFACE(threaded_process_callback,service_base);
};

class NOVTABLE threaded_process : public service_base {
public:
	enum {
		flag_show_abort			= 1,
		flag_show_minimize		= 1 << 1,
		flag_show_progress		= 1 << 2,
		flag_show_progress_dual	= 1 << 3,//implies flag_show_progress
		flag_show_item			= 1 << 4,
		flag_show_pause			= 1 << 5,
		flag_high_priority		= 1 << 6,
		flag_show_delayed		= 1 << 7,//modeless-only
		flag_no_focus			= 1 << 8,//new (0.9.3)
	};

	virtual bool run_modal(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len) = 0;
	virtual bool run_modeless(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len) = 0;

	static bool g_run_modal(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len = infinite);
	static bool g_run_modeless(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len = infinite);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(threaded_process);
};


#endif //_foobar2000_sdk_threaded_process_h_
