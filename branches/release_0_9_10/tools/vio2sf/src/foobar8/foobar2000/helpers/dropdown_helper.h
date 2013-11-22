#ifndef _DROPDOWN_HELPER_H_
#define _DROPDOWN_HELPER_H_


class cfg_dropdown_history
{
	enum {separator = '\n'};
	cfg_string data;
	unsigned max;
	void build_list(ptr_list_simple<char> & out);
	void parse_list(const ptr_list_simple<char> & src);
public:
	cfg_dropdown_history(const char * name,unsigned p_max = 10,const char * init_vals = "") : data(name,init_vals) {max = p_max;}
	void setup_dropdown(HWND wnd);
	void setup_dropdown(HWND wnd,UINT id) {setup_dropdown(GetDlgItem(wnd,id));}
	void add_item(const char * item);
	bool is_empty();
};


#endif //_DROPDOWN_HELPER_H_