#ifndef _WINDOW_PLACEMENT_HELPER_H_
#define _WINDOW_PLACEMENT_HELPER_H_

class cfg_window_placement : public cfg_var
{
public:
	bool on_window_creation(HWND window);//returns true if window position has been changed, false if not
	void on_window_creation_silent(HWND window);
	void on_window_destruction(HWND window);
	bool read_from_window(HWND window);
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort);
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort);
	cfg_window_placement(const GUID & p_guid);
private:
	pfc::list_hybrid_t<HWND,2> m_windows;
	WINDOWPLACEMENT m_data;
};

class cfg_window_size : public cfg_var
{
public:
	bool on_window_creation(HWND window);//returns true if window position has been changed, false if not
	void on_window_destruction(HWND window);
	bool read_from_window(HWND window);
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort);
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort);
	cfg_window_size(const GUID & p_guid);
private:
	pfc::list_hybrid_t<HWND,2> m_windows;
	t_uint32 m_width,m_height;
};


#endif //_WINDOW_PLACEMENT_HELPER_H_