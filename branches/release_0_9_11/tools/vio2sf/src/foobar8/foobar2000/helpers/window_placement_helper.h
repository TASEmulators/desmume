#ifndef _WINDOW_PLACEMENT_HELPER_H_
#define _WINDOW_PLACEMENT_HELPER_H_

void read_window_placement(cfg_struct_t<WINDOWPLACEMENT> & dst,HWND wnd);
bool apply_window_placement(const cfg_struct_t<WINDOWPLACEMENT> & src,HWND wnd);

#endif //_WINDOW_PLACEMENT_HELPER_H_