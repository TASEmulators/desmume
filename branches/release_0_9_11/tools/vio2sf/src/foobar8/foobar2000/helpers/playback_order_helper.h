#ifndef _FOOBAR2000_PLAYBACK_ORDER_HELPER_H_
#define _FOOBAR2000_PLAYBACK_ORDER_HELPER_H_

namespace playback_order_helper {
	enum { order_invalid = (unsigned)(-1) };
	unsigned get_count();
	const char * get_name_from_index(unsigned idx);	//index is 0 ... get_count()-1
	unsigned get_index_from_name(const char * name);//returns order_invalid if not found
	unsigned get_config_by_index();//order_invalid on error
	const char * get_config_by_name();//reverts to "default" on error
	void get_config_by_name(string_base & out);//may return name of order that doesn't exist anymore (e.g. user removing dlls)
	void set_config_by_name(const char * name);
	void set_config_by_index(unsigned idx);

	class modify_callback : public config_var_callback_autoreg<config_var_string>
	{
		void (*func)(const char * newval,unsigned newidx);
		virtual void on_changed(const config_var_string * ptr);
	public:
		modify_callback(void (*p_func)(const char * newval,unsigned newidx), bool calloninit = false);
	};


	inline order_exists(const char * name) {return get_index_from_name(name)!=order_invalid;}
};


#endif