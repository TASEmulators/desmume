#ifndef _DIALOG_RESIZE_HELPER_H_
#define _DIALOG_RESIZE_HELPER_H_

BOOL GetChildWindowRect(HWND wnd,UINT id,RECT* child);

class dialog_resize_helper
{
	pfc::array_t<RECT> rects;
	RECT orig_client;
	HWND parent;
	HWND sizegrip;
	unsigned min_x,min_y,max_x,max_y;

public:
	struct param {
		unsigned short id;
		unsigned short flags;
	};
private:
	pfc::array_t<param> m_table;

	void set_parent(HWND wnd);
	void reset();
	void on_wm_size();
public:
	inline void set_min_size(unsigned x,unsigned y) {min_x = x; min_y = y;}
	inline void set_max_size(unsigned x,unsigned y) {max_x = x; max_y = y;}
	void add_sizegrip();
	
	enum {
		X_MOVE = 1, X_SIZE = 2, Y_MOVE = 4, Y_SIZE = 8,
		XY_MOVE = X_MOVE|Y_MOVE, XY_SIZE = X_SIZE|Y_SIZE,
		X_MOVE_Y_SIZE = X_MOVE|Y_SIZE, X_SIZE_Y_MOVE = X_SIZE|Y_MOVE,
	};
	//the old way
	bool process_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);

	//ATL-compatible
	BOOL ProcessWindowMessage(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT& lResult);

	dialog_resize_helper(const param * src,unsigned count,unsigned p_min_x,unsigned p_min_y,unsigned p_max_x,unsigned p_max_y);

	~dialog_resize_helper();

	PFC_CLASS_NOT_COPYABLE_EX(dialog_resize_helper);
};

#endif
