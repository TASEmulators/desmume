#include "stdafx.h"


namespace playback_order_helper
{
	static const char variable_name[] = "CORE/Playback flow control";

	unsigned get_count()
	{
		return service_enum_get_count_t(playback_flow_control);
	}

	const char * get_name_from_index(unsigned idx)
	{
		const char * ret = 0;
		assert(idx>=0);
		assert(idx<get_count());
		playback_flow_control * ptr = service_enum_create_t(playback_flow_control,idx);
		assert(ptr);
		ret = ptr->get_name();
		ptr->service_release();
		return ret;
	}

	unsigned get_index_from_name(const char * name)
	{
		unsigned idx, max = get_count();
		for(idx=0;idx<max;idx++)
		{
			const char * ptr = get_name_from_index(idx);
			assert(ptr);
			if (!stricmp_utf8(ptr,name))
				return idx;
		}
		return order_invalid;
	}

	unsigned get_config_by_index()
	{
		string8 name;
		get_config_by_name(name);
		return get_index_from_name(name);
	}

	const char * get_config_by_name()
	{
		unsigned idx = get_config_by_index();
		if (idx==order_invalid) return "Default";
		else return get_name_from_index(idx);
	}

	void get_config_by_name(string_base & out)
	{
		config_var_string::g_get_value(variable_name,out);
	}

	void set_config_by_name(const char * name)
	{
		assert(core_api::is_main_thread());
		config_var_string::g_set_value(variable_name,name);
	}

	void set_config_by_index(unsigned idx)
	{
		assert(core_api::is_main_thread());
		const char * name = get_name_from_index(idx);
		assert(name);
		set_config_by_name(name);
	}


	modify_callback::modify_callback(void (*p_func)(const char * newval,unsigned newidx),bool calloninit)
		: config_var_callback_autoreg<config_var_string>(variable_name,calloninit), func(p_func) {}
	
	void modify_callback::on_changed(const config_var_string * ptr)
	{
		string8 name;
		ptr->get_value(name);
		func(name,get_index_from_name(name));
	}

}