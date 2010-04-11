#include "foobar2000.h"

void threaded_process_status::set_progress(t_size p_state,t_size p_max)
{
	set_progress( progress_min + MulDiv_Size(p_state,progress_max-progress_min,p_max) );
}

void threaded_process_status::set_progress_secondary(t_size p_state,t_size p_max)
{
	set_progress_secondary( progress_min + MulDiv_Size(p_state,progress_max-progress_min,p_max) );
}

void threaded_process_status::set_progress_float(double p_state)
{
	if (p_state < 0.0) set_progress(progress_min);
	else if (p_state < 1.0) set_progress( progress_min + (t_size)(p_state * (progress_max - progress_min)));
	else set_progress(progress_max);
}

void threaded_process_status::set_progress_secondary_float(double p_state)
{
	if (p_state < 0.0) set_progress_secondary(progress_min);
	else if (p_state < 1.0) set_progress_secondary( progress_min + (t_size)(p_state * (progress_max - progress_min)));
	else set_progress_secondary(progress_max);
}


bool threaded_process::g_run_modal(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len)
{
	return static_api_ptr_t<threaded_process>()->run_modal(p_callback,p_flags,p_parent,p_title,p_title_len);
}

bool threaded_process::g_run_modeless(service_ptr_t<threaded_process_callback> p_callback,unsigned p_flags,HWND p_parent,const char * p_title,t_size p_title_len)
{
	return static_api_ptr_t<threaded_process>()->run_modeless(p_callback,p_flags,p_parent,p_title,p_title_len);
}
