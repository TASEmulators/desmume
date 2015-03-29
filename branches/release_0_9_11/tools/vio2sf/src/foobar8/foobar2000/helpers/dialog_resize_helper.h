#ifndef _DIALOG_RESIZE_HELPER_H_
#define _DIALOG_RESIZE_HELPER_H_


//deprecated, use dialog_resize_helper class
namespace resize
{
	void calc_xy(HWND wnd,UINT id,RECT &r,RECT & o);
	void calc_move_xy(HWND wnd,UINT id,RECT &r,RECT & o);
	void calc_move_x(HWND wnd,UINT id,RECT &r,RECT & o);
	void calc_move_x_size_y(HWND wnd,UINT id,RECT &r,RECT & o);
	void calc_move_y(HWND wnd,UINT id,RECT &r,RECT & o);
	void calc_x(HWND wnd,UINT id,RECT &r,RECT & o);
};

void GetChildRect(HWND wnd,UINT id,RECT* child);


class dialog_resize_helper
{
	RECT * rects;
	RECT orig_client;
	HWND parent;
	unsigned min_x,min_y,max_x,max_y;

public:
	struct param {
		unsigned short id;
		unsigned short flags;
	};
private:
	param * m_table;
	unsigned m_table_size;

	void set_parent(HWND wnd);
	void add_item(UINT id,UINT flags);
	void reset();
	void on_wm_size();
	void add_items(const param* table,unsigned count);
public:
	inline void set_min_size(unsigned x,unsigned y) {min_x = x; min_y = y;}
	inline void set_max_size(unsigned x,unsigned y) {max_x = x; max_y = y;}
	
	enum {
		X_MOVE = 1, X_SIZE = 2, Y_MOVE = 4, Y_SIZE = 8,
		XY_MOVE = X_MOVE|Y_MOVE, XY_SIZE = X_SIZE|Y_SIZE,
		X_MOVE_Y_SIZE = X_MOVE|Y_SIZE, X_SIZE_Y_MOVE = X_SIZE|Y_MOVE,
	};
	bool process_message(HWND wnd,UINT msg,WPARAM wp,LPARAM lp);



	explicit dialog_resize_helper(const param * src,unsigned count,unsigned p_min_x,unsigned p_min_y,unsigned p_max_x,unsigned p_max_y);
	~dialog_resize_helper();
};

#endif