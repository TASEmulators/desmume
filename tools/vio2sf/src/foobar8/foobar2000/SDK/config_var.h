#ifndef _FOOBAR2000_CONFIG_VAR_H_
#define _FOOBAR2000_CONFIG_VAR_H_

#include "service.h"

#include "interface_helper.h"

#include "initquit.h"

// "externally visible" config vars that can be accessed by other components

//note: callbacks from different threads are allowed, keep your stuff mt-safe


class config_var;

class config_var_callback
{
public:
	virtual void on_changed(const config_var * ptr)=0;
};


class callback_list
{
	ptr_list_simple< config_var_callback > list;
public:
	void add(config_var_callback * ptr) {list.add_item(ptr);}
	void remove(config_var_callback * ptr) {list.remove_item(ptr);}
	void call(const config_var * ptr) const
	{
		int n,m=list.get_count();
		for(n=0;n<m;n++)
			list[n]->on_changed(ptr);
	}
};



class config_var : public service_base
{
public:
	class NOVTABLE write_config_callback
	{
	public:
		virtual void write(const void * ptr,int bytes)=0;
	};

	class write_config_callback_i : public write_config_callback
	{
	public:
		mem_block data;
		write_config_callback_i() {data.set_mem_logic(mem_block::ALLOC_FAST_DONTGODOWN);}
		virtual void write(const void * ptr,int bytes) {data.append(ptr,bytes);}
	};

	virtual const char * get_name() const = 0;
	virtual bool is_public() const {return true;}
	virtual void get_raw_data(config_var::write_config_callback * out) const = 0;
	virtual void set_raw_data(const void * data,int size) = 0;
	virtual void reset()=0;
	virtual GUID get_type() const = 0;
	virtual void add_callback(config_var_callback * ptr,bool calloninit=false)=0;
	virtual void remove_callback(config_var_callback * ptr)=0;
	virtual const char * get_library() {return core_api::get_my_file_name();}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static config_var * g_find(GUID type,const char * name)
	{
		service_enum_t<config_var> e;
		config_var * ptr;
		for(ptr=e.first();ptr;ptr=e.next())
		{
			if (ptr->get_type()==type && ptr->is_public() && !stricmp_utf8(name,ptr->get_name())) return ptr;
			ptr->service_release();
		}
		return 0;
	}

	static void g_add_callback(GUID type,const char * name,config_var_callback * cb,bool calloninit=false)
	{
		config_var * ptr = g_find(type,name);
		if (ptr) {ptr->add_callback(cb,calloninit);ptr->service_release();}
	}
	
	static void g_remove_callback(GUID type,const char * name,config_var_callback * cb)
	{
		config_var * ptr = g_find(type,name);
		if (ptr) {ptr->remove_callback(cb);ptr->service_release();}
	}

};

class NOVTABLE config_var_int : public config_var
{
public:
	static const GUID var_type;
	inline static const GUID & get_var_type() {return var_type;}

	virtual const char * get_name() const = 0;
	virtual void get_raw_data(config_var::write_config_callback * out) const = 0;
	virtual void set_raw_data(const void * data,int size) = 0;
	virtual void reset()=0;
	virtual GUID get_type() const {return get_var_type();}

	virtual int get_value() const = 0;
	virtual void set_value(int val) = 0;

	static config_var_int * g_find(const char * name)
	{
		return static_cast<config_var_int*>(config_var::g_find(get_var_type(),name));
	}
	
	static int g_get_value(const char * name)
	{
		int rv = 0;
		config_var_int * ptr = g_find(name);
		if (ptr) {rv = ptr->get_value();ptr->service_release();}
		return rv;
	}

	static void g_set_value(const char * name,int val)
	{
		config_var_int * ptr = g_find(name);
		if (ptr) {ptr->set_value(val);ptr->service_release();}
	}

	static void g_add_callback(const char * name,config_var_callback * cb,bool calloninit=false)
	{
		config_var_int * ptr = g_find(name);
		if (ptr) {ptr->add_callback(cb,calloninit);ptr->service_release();}
	}
	
	static void g_remove_callback(const char * name,config_var_callback * cb)
	{
		config_var_int * ptr = g_find(name);
		if (ptr) {ptr->remove_callback(cb);ptr->service_release();}
	}
};

extern const GUID config_var_struct_var_type;

template<class T>
class NOVTABLE config_var_struct : public config_var
{
public:
	inline static const GUID & get_var_type() {return config_var_struct_var_type;}

	virtual const char * get_name() const = 0;
	virtual void get_raw_data(config_var::write_config_callback * out) const = 0;
	virtual void set_raw_data(const void * data,int size) = 0;
	virtual void reset()=0;
	virtual GUID get_type() const {return get_var_type();}
	virtual unsigned get_struct_size() {return sizeof(T);}

	virtual void get_value(T & out) const = 0;
	virtual void set_value(const T& src) = 0;

	static config_var_struct<T> * g_find(const char * name)
	{
		config_var_struct<T> * temp = static_cast<config_var_struct<T>*>(config_var::g_find(get_var_type(),name));
		assert(temp==0 || temp->get_struct_size() == sizeof(T) );
		return temp;
	}
	
	static bool g_get_value(const char * name,T& out)
	{
		bool rv = false;
		config_var_struct<T> * ptr = g_find(name);
		if (ptr) {ptr->get_value(out);ptr->service_release();rv=true;}
		return rv;
	}

	static void g_set_value(const char * name,const T& val)
	{
		config_var_struct<T> * ptr = g_find(name);
		if (ptr) {ptr->set_value(val);ptr->service_release();}
	}

	static void g_add_callback(const char * name,config_var_callback * cb,bool calloninit=false)
	{
		config_var_struct<T> * ptr = g_find(name);
		if (ptr) {ptr->add_callback(cb,calloninit);ptr->service_release();}
	}
	
	static void g_remove_callback(const char * name,config_var_callback * cb)
	{
		config_var_struct<T> * ptr = g_find(name);
		if (ptr) {ptr->remove_callback(cb);ptr->service_release();}
	}
};

template<class T>
class config_var_struct_i : public config_var_struct<T>
{
	string_simple name;
	T value,def;
	mutable critical_section sync;
	callback_list callbacks;
public:
	virtual const char * get_name() const {return name;}
	virtual void get_raw_data(config_var::write_config_callback * out) const {insync(sync);out->write(&value,sizeof(value));}
	virtual void set_raw_data(const void * data,int size)
	{
		insync(sync);
		if (size==sizeof(value)) value = *(T*)data;
		callbacks.call(this);
	}
	virtual void reset()
	{
		insync(sync);
		value = def;
		callbacks.call(this);
	}

	virtual void add_callback(config_var_callback * ptr,bool calloninit=false)
	{
		insync(sync);
		callbacks.add(ptr);
		if (calloninit) ptr->on_changed(this);
	}
	virtual void remove_callback(config_var_callback * ptr)
	{
		insync(sync);
		callbacks.remove(ptr);
	}

	virtual void get_value(T & out) const {insync(sync);out = value;}
	virtual void set_value(const T & p_val)
	{
		insync(sync);
		if (memcmp(&value,&p_val,sizeof(value)))
		{
			value = p_val;
			callbacks.call(this);
		}
	}

	config_var_struct_i(const char * p_name,T p_value) : name(p_name), value(p_value), def(p_value) {}

	void operator=(const T& v) {set_value(v);}
	inline operator T() const {T temp;get_value(temp);return temp;}
};


class NOVTABLE config_var_string : public config_var
{
public:
	static const GUID var_type;
	inline static const GUID & get_var_type() {return var_type;}

	virtual const char * get_name() const = 0;
	virtual void get_raw_data(config_var::write_config_callback * out) const = 0;
	virtual void set_raw_data(const void * data,int size) = 0;
	virtual void reset()=0;
	virtual GUID get_type() const {return get_var_type();}

	virtual void get_value(string_base & out) const = 0;
	virtual void set_value(const char * val) = 0;

	static config_var_string * g_find(const char * name)
	{
		return static_cast<config_var_string*>(config_var::g_find(get_var_type(),name));
	}
	
	static void g_get_value(const char * name,string_base & out)
	{
		config_var_string * ptr = g_find(name);
		if (ptr) {ptr->get_value(out);ptr->service_release();}
	}

	static void g_set_value(const char * name,const char * val)
	{
		config_var_string * ptr = g_find(name);
		if (ptr) {ptr->set_value(val);ptr->service_release();}
	}

	static void g_add_callback(const char * name,config_var_callback * cb,bool calloninit=false)
	{
		config_var_string * ptr = g_find(name);
		if (ptr) {ptr->add_callback(cb,calloninit);ptr->service_release();}
	}
	
	static void g_remove_callback(const char * name,config_var_callback * cb)
	{
		config_var_string * ptr = g_find(name);
		if (ptr) {ptr->remove_callback(cb);ptr->service_release();}
	}
};

class config_var_int_i : public config_var_int
{
	string_simple name;
	int value,def;
	mutable critical_section sync;
	callback_list callbacks;
public:
	virtual const char * get_name() const {return name;}
	virtual void get_raw_data(config_var::write_config_callback * out) const {insync(sync);out->write(&value,sizeof(int));}
	virtual void set_raw_data(const void * data,int size)//bah not endian safe
	{
		insync(sync);
		if (size==sizeof(int)) value = *(int*)data;
		callbacks.call(this);
	}
	virtual void reset()
	{
		insync(sync);
		value = def;
		callbacks.call(this);
	}

	virtual void add_callback(config_var_callback * ptr,bool calloninit=false)
	{
		insync(sync);
		callbacks.add(ptr);
		if (calloninit) ptr->on_changed(this);
	}
	virtual void remove_callback(config_var_callback * ptr)
	{
		insync(sync);
		callbacks.remove(ptr);
	}

	virtual int get_value() const {insync(sync);return value;}
	virtual void set_value(int p_val)
	{
		insync(sync);
		if (value!=p_val)
		{
			value = p_val;
			callbacks.call(this);
		}
	}

	config_var_int_i(const char * p_name,int p_value) : name(p_name), value(p_value), def(p_value) {}

	int operator=(int v) {set_value(v);return v;}
	operator int() const {return get_value();}
};

class config_var_string_i : public config_var_string
{
	string_simple name,value,def;
	mutable critical_section sync;
	callback_list callbacks;
public:
	virtual const char * get_name() const {return name;}
	virtual void get_raw_data(config_var::write_config_callback * out) const 
	{
		insync(sync);
		out->write(value.get_ptr(),strlen(value));
	}
	virtual void set_raw_data(const void * data,int size)
	{
		insync(sync);
		value.set_string_n((const char*)data,size);
		callbacks.call(this);
	}
	virtual void reset()
	{
		insync(sync);
		value = def;
		callbacks.call(this);
	}

	virtual GUID get_type() const {return get_var_type();}

	virtual void get_value(string_base & out) const
	{
		insync(sync);
		out.set_string(value);
	}

	virtual void set_value(const char * val)
	{
		insync(sync);
		if (strcmp(value,val))
		{
			value.set_string(val);
			callbacks.call(this);
		}
	}

	virtual void add_callback(config_var_callback * ptr,bool calloninit=false)
	{
		insync(sync);
		callbacks.add(ptr);
		if (calloninit) ptr->on_changed(this);
	}
	virtual void remove_callback(config_var_callback * ptr)
	{
		insync(sync);
		callbacks.remove(ptr);
	}
	config_var_string_i(const char * p_name,const char * p_value) : name(p_name), value(p_value), def(p_value) {}
};

//only static creation!
template<class vartype>
class config_var_callback_autoreg : public config_var_callback
{
	class callback_autoreg_initquit : public initquit_autoreg
	{
		config_var_callback_autoreg * cb;
	public:
		callback_autoreg_initquit(config_var_callback_autoreg * p_cb) {cb = p_cb;}
		virtual void on_init() {cb->on_init();}
		virtual void on_quit() {cb->on_quit();}
	};
	
	vartype * var;
	string_simple varname;
	virtual void on_changed(const config_var * ptr) {on_changed(static_cast<const vartype*>(ptr));}
	callback_autoreg_initquit * p_initquit;
	bool calloninit;
protected:
	virtual void on_changed(const vartype * ptr)=0;
public:
	void on_init()
	{
		if (!var && !varname.is_empty())
			var = vartype::g_find(varname);
		if (var) var->add_callback(this,calloninit);
	}
	void on_quit()
	{
		if (var) var->remove_callback(this);
	}
	config_var_callback_autoreg(vartype * p_var,bool p_calloninit = false)
	{
		calloninit = p_calloninit;
		var = p_var;
		p_initquit = new callback_autoreg_initquit(this);
	}
	config_var_callback_autoreg(const char * name,bool p_calloninit = false)
	{
		calloninit = p_calloninit;
		var = 0;
		varname = name;
		p_initquit = new callback_autoreg_initquit(this);
	}
	~config_var_callback_autoreg()
	{
		delete p_initquit;
	}
};

template<class vartype>
class config_var_callback_simple : public config_var_callback_autoreg<vartype>
{
	void (*func)(const vartype *);
	virtual void on_changed(const vartype * ptr) {func(ptr);}
public:
	config_var_callback_simple(vartype * p_var,void (*p_func)(const vartype *),bool p_calloninit = false)
		: config_var_callback_autoreg<vartype>(p_var,p_calloninit) {func = p_func;}

	config_var_callback_simple(const char * name,void (*p_func)(const vartype *),bool p_calloninit = false)
		: config_var_callback_autoreg<vartype>(name,p_calloninit) {func = p_func;}
};

#define DECLARE_CONFIG_VAR_INT(VARNAME,NAME,VALUE) \
	static service_factory_single_transparent_p2_t<config_var,config_var_int_i,const char*,int> VARNAME(NAME,VALUE);


#define DECLARE_CONFIG_VAR_STRING(VARNAME,NAME,VALUE) \
	static service_factory_single_transparent_p2_t<config_var,config_var_string_i,const char*,const char*> VARNAME(NAME,VALUE);

#define DECLARE_CONFIG_VAR_STRUCT(TYPE,VARNAME,NAME,VALUE) \
	static service_factory_single_transparent_p2_t<config_var,config_var_struct_i<TYPE>,const char*,TYPE> VARNAME(NAME,VALUE);


#endif