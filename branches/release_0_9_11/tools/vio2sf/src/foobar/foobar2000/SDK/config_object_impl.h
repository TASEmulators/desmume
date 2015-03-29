#ifndef _CONFIG_OBJECT_IMPL_H_
#define _CONFIG_OBJECT_IMPL_H_

//template function bodies from config_object class

template<class T>
void config_object::get_data_struct_t(T& p_out) {
	if (get_data_raw(&p_out,sizeof(T)) != sizeof(T)) throw exception_io_data_truncation();
}

template<class T>
void config_object::set_data_struct_t(const T& p_in) {
	return set_data_raw(&p_in,sizeof(T));
}

template<class T>
void config_object::g_get_data_struct_t(const GUID & p_guid,T & p_out) {
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	return ptr->get_data_struct_t<T>(p_out);
}

template<class T>
void config_object::g_set_data_struct_t(const GUID & p_guid,const T & p_in) {
	service_ptr_t<config_object> ptr;
	if (!g_find(ptr,p_guid)) throw exception_service_not_found();
	return ptr->set_data_struct_t<T>(p_in);
}


class config_object_impl : public config_object, private cfg_var
{
public:
	GUID get_guid() const {return cfg_var::get_guid();}
	void get_data(stream_writer * p_stream,abort_callback & p_abort) const ;
	void set_data(stream_reader * p_stream,abort_callback & p_abort,bool p_notify);

	config_object_impl(const GUID & p_guid,const void * p_data,t_size p_bytes);
private:

	//cfg_var methods
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {get_data(p_stream,p_abort);}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {set_data(p_stream,p_abort,false);}

	mutable critical_section m_sync;
	pfc::array_t<t_uint8> m_data;	
};

typedef service_factory_single_transparent_t<config_object_impl> config_object_factory;

template<t_size p_size>
class config_object_fixed_const_impl_t : public config_object {
public:
	config_object_fixed_const_impl_t(const GUID & p_guid, const void * p_data) : m_guid(p_guid) {memcpy(m_data,p_data,p_size);}
	GUID get_guid() const {return m_guid;}

	void get_data(stream_writer * p_stream, abort_callback & p_abort) const { p_stream->write_object(m_data,p_size,p_abort); }
	void set_data(stream_reader * p_stream, abort_callback & p_abort, bool p_notify) { PFC_ASSERT(!"Should not get here."); }

private:
	t_uint8 m_data[p_size];
	const GUID m_guid;
};

template<t_size p_size>
class config_object_fixed_impl_t : public config_object, private cfg_var {
public:
	GUID get_guid() const {return cfg_var::get_guid();}
	
	void get_data(stream_writer * p_stream,abort_callback & p_abort) const {
		insync(m_sync);
		p_stream->write_object(m_data,p_size,p_abort);
	}

	void set_data(stream_reader * p_stream,abort_callback & p_abort,bool p_notify) {
		core_api::ensure_main_thread();
		
		{
			t_uint8 temp[p_size];
			p_stream->read_object(temp,p_size,p_abort);
			insync(m_sync);
			memcpy(m_data,temp,p_size);
		}

		if (p_notify) config_object_notify_manager::g_on_changed(this);
	}

	config_object_fixed_impl_t (const GUID & p_guid,const void * p_data)
		: cfg_var(p_guid)
	{
		memcpy(m_data,p_data,p_size);
	}

private:
	//cfg_var methods
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {get_data(p_stream,p_abort);}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {set_data(p_stream,p_abort,false);}

	mutable critical_section m_sync;
	t_uint8 m_data[p_size];
	
};

template<t_size p_size, bool isConst> class _config_object_fixed_impl_switch;
template<t_size p_size> class _config_object_fixed_impl_switch<p_size,false> { public: typedef config_object_fixed_impl_t<p_size> type; };
template<t_size p_size> class _config_object_fixed_impl_switch<p_size,true> { public: typedef config_object_fixed_const_impl_t<p_size> type; };

template<t_size p_size, bool isConst = false>
class config_object_fixed_factory_t : public service_factory_single_transparent_t< typename _config_object_fixed_impl_switch<p_size,isConst>::type >
{
public:
	config_object_fixed_factory_t(const GUID & p_guid,const void * p_initval)
		:
		service_factory_single_transparent_t< typename _config_object_fixed_impl_switch<p_size,isConst>::type >
		(p_guid,p_initval)
	{}
};


class config_object_string_factory : public config_object_factory
{
public:
	config_object_string_factory(const GUID & p_guid,const char * p_string,t_size p_string_length = infinite)
		: config_object_factory(p_guid,p_string,pfc::strlen_max(p_string,infinite)) {}

};

template<bool isConst = false>
class config_object_bool_factory_t : public config_object_fixed_factory_t<1,isConst> {
public:
	config_object_bool_factory_t(const GUID & p_guid,bool p_initval)
		: config_object_fixed_factory_t<1,isConst>(p_guid,&p_initval) {}
};
typedef config_object_bool_factory_t<> config_object_bool_factory;

template<class T,bool isConst = false>
class config_object_int_factory_t : public config_object_fixed_factory_t<sizeof(T),isConst>
{
private:
	template<class T>
	struct t_initval
	{
		T m_initval;
		t_initval(T p_initval) : m_initval(p_initval) {byte_order::order_native_to_le_t(m_initval);}
		T * get_ptr() {return &m_initval;}
	};
public:
	config_object_int_factory_t(const GUID & p_guid,T p_initval)
		: config_object_fixed_factory_t<sizeof(T)>(p_guid,t_initval<T>(p_initval).get_ptr() )
	{}
};

typedef config_object_int_factory_t<t_int32> config_object_int32_factory;



class config_object_notify_impl_simple : public config_object_notify
{
public:
	t_size get_watched_object_count() {return 1;}
	GUID get_watched_object(t_size p_index) {return m_guid;}
	void on_watched_object_changed(const service_ptr_t<config_object> & p_object) {m_func(p_object);}
	
	typedef void (*t_func)(const service_ptr_t<config_object> &);

	config_object_notify_impl_simple(const GUID & p_guid,t_func p_func) : m_guid(p_guid), m_func(p_func) {}
private:
	GUID m_guid;
	t_func m_func;	
};

typedef service_factory_single_transparent_t<config_object_notify_impl_simple> config_object_notify_simple_factory;

#endif _CONFIG_OBJECT_IMPL_H_
