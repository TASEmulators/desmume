#ifndef _FOOBAR2000_SDK_CFG_VAR_H_
#define _FOOBAR2000_SDK_CFG_VAR_H_

#define CFG_VAR_ASSERT_SAFEINIT PFC_ASSERT(!core_api::are_services_available());/*imperfect check for nonstatic instantiation*/


//! Reader part of cfg_var object. In most cases, you should use cfg_var instead of using cfg_var_reader directly.
class NOVTABLE cfg_var_reader {
public:
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	cfg_var_reader(const GUID & guid) : m_guid(guid) { CFG_VAR_ASSERT_SAFEINIT; m_next = g_list; g_list = this; }
	~cfg_var_reader() { CFG_VAR_ASSERT_SAFEINIT; }
	
	//! Sets state of the variable. Called only from main thread, when reading configuration file.
	//! @param p_stream Stream containing new state of the variable.
	//! @param p_sizehint Number of bytes contained in the stream; reading past p_sizehint bytes will fail (EOF).
	virtual void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) = 0;

	//! For internal use only, do not call.
	static void config_read_file(stream_reader * p_stream,abort_callback & p_abort);

	const GUID m_guid;
private:
	static cfg_var_reader * g_list;
	cfg_var_reader * m_next;

	PFC_CLASS_NOT_COPYABLE_EX(cfg_var_reader)
};

//! Writer part of cfg_var object. In most cases, you should use cfg_var instead of using cfg_var_writer directly.
class NOVTABLE cfg_var_writer {
public:
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	cfg_var_writer(const GUID & guid) : m_guid(guid) { CFG_VAR_ASSERT_SAFEINIT; m_next = g_list; g_list = this;}
	~cfg_var_writer() { CFG_VAR_ASSERT_SAFEINIT; }

	//! Retrieves state of the variable. Called only from main thread, when writing configuration file.
	//! @param p_stream Stream receiving state of the variable.
	virtual void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) = 0;
	
	//! For internal use only, do not call.
	static void config_write_file(stream_writer * p_stream,abort_callback & p_abort);

	const GUID m_guid;
private:
	static cfg_var_writer * g_list;
	cfg_var_writer * m_next;

	PFC_CLASS_NOT_COPYABLE_EX(cfg_var_writer)
};

//! Base class for configuration variable classes; provides self-registration mechaisms and methods to set/retrieve configuration data; those methods are automatically called for all registered instances by backend when configuration file is being read or written.\n
//! Note that cfg_var class and its derivatives may be only instantiated statically (as static objects or members of other static objects), NEVER dynamically (operator new, local variables, members of objects instantiated as such).
class NOVTABLE cfg_var : public cfg_var_reader, public cfg_var_writer {
protected:
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	cfg_var(const GUID & p_guid) : cfg_var_reader(p_guid), cfg_var_writer(p_guid) {}
public:
	GUID get_guid() const {return cfg_var_reader::m_guid;}
};

//! Generic integer config variable class. Template parameter can be used to specify integer type to use.\n
//! Note that cfg_var class and its derivatives may be only instantiated statically (as static objects or members of other static objects), NEVER dynamically (operator new, local variables, members of objects instantiated as such).
template<typename t_inttype>
class cfg_int_t : public cfg_var {
private:
	t_inttype m_val;
protected:
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {p_stream->write_lendian_t(m_val,p_abort);}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		t_inttype temp;
		p_stream->read_lendian_t(temp,p_abort);//alter member data only on success, this will throw an exception when something isn't right
		m_val = temp;
	}

public:
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	//! @param p_default Default value of the variable.
	explicit inline cfg_int_t(const GUID & p_guid,t_inttype p_default) : cfg_var(p_guid), m_val(p_default) {}
	
	inline const cfg_int_t<t_inttype> & operator=(const cfg_int_t<t_inttype> & p_val) {m_val=p_val.m_val;return *this;}
	inline t_inttype operator=(t_inttype p_val) {m_val=p_val;return m_val;}

	inline operator t_inttype() const {return m_val;}

	inline t_inttype get_value() const {return m_val;}
};

typedef cfg_int_t<t_int32> cfg_int;
typedef cfg_int_t<t_uint32> cfg_uint;
//! Since relevant byteswapping functions also understand GUIDs, this can be abused to declare a cfg_guid.
typedef cfg_int_t<GUID> cfg_guid;
typedef cfg_int_t<bool> cfg_bool;

//! String config variable. Stored in the stream with int32 header containing size in bytes, followed by non-null-terminated UTF-8 data.\n
//! Note that cfg_var class and its derivatives may be only instantiated statically (as static objects or members of other static objects), NEVER dynamically (operator new, local variables, members of objects instantiated as such).
class cfg_string : public cfg_var, public pfc::string8
{
protected:
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort);
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort);

public:
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	//! @param p_defaultval Default/initial value of the variable.
	explicit inline cfg_string(const GUID & p_guid,const char * p_defaultval) : cfg_var(p_guid), pfc::string8(p_defaultval) {}

	inline const cfg_string& operator=(const cfg_string & p_val) {set_string(p_val);return *this;}
	inline const cfg_string& operator=(const char* p_val) {set_string(p_val);return *this;}

	inline operator const char * () const {return get_ptr();}

};

//! Struct config variable template. Warning: not endian safe, should be used only for nonportable code.\n
//! Note that cfg_var class and its derivatives may be only instantiated statically (as static objects or members of other static objects), NEVER dynamically (operator new, local variables, members of objects instantiated as such).
template<typename t_struct>
class cfg_struct_t : public cfg_var {
private:
	t_struct m_val;
protected:

	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {p_stream->write_object(&m_val,sizeof(m_val),p_abort);}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		t_struct temp;
		p_stream->read_object(&temp,sizeof(temp),p_abort);
		m_val = temp;
	}
public:
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	inline cfg_struct_t(const GUID & p_guid,const t_struct & p_val) : cfg_var(p_guid), m_val(p_val) {}
	//! @param p_guid GUID of the variable, used to identify variable implementations owning specific configuration file entries when reading the configuration file back. You must generate a new GUID every time you declare a new cfg_var.
	inline cfg_struct_t(const GUID & p_guid,int filler) : cfg_var(p_guid) {memset(&m_val,filler,sizeof(t_struct));}
	
	inline const cfg_struct_t<t_struct> & operator=(const cfg_struct_t<t_struct> & p_val) {m_val = p_val.get_value();return *this;}
	inline const cfg_struct_t<t_struct> & operator=(const t_struct & p_val) {m_val = p_val;return *this;}

	inline const t_struct& get_value() const {return m_val;}
	inline t_struct& get_value() {return m_val;}
	inline operator t_struct() const {return m_val;}
};


template<typename TObj>
class cfg_objList : public cfg_var, public pfc::list_t<TObj> {
public:
	cfg_objList(const GUID& guid) : cfg_var(guid) {}
	template<typename TSource, unsigned Count> cfg_objList(const GUID& guid, const TSource (& source)[Count]) : cfg_var(guid) {
		reset(source);
	}
	template<typename TSource, unsigned Count> void reset(const TSource (& source)[Count]) {
		set_size(Count); for(t_size walk = 0; walk < Count; ++walk) (*this)[walk] = source[walk];
	}
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {
		stream_writer_formatter<> out(*p_stream,p_abort);
		out << pfc::downcast_guarded<t_uint32>(get_size());
		for(t_size walk = 0; walk < get_size(); ++walk) out << (*this)[walk];
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		try {
			stream_reader_formatter<> in(*p_stream,p_abort);
			t_uint32 count; in >> count;
			set_count(count);
			for(t_uint32 walk = 0; walk < count; ++walk) in >> (*this)[walk];
		} catch(...) {
			remove_all();
			throw;
		}
	}
};
template<typename TList>
class cfg_objListEx : public cfg_var, public TList {
public:
	cfg_objListEx(const GUID & guid) : cfg_var(guid) {}
	void get_data_raw(stream_writer * p_stream, abort_callback & p_abort) {
		stream_writer_formatter<> out(*p_stream,p_abort);
		out << pfc::downcast_guarded<t_uint32>(this->get_count());
		for(typename TList::const_iterator walk = this->first(); walk.is_valid(); ++walk) out << *walk;
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		remove_all();
		stream_reader_formatter<> in(*p_stream,p_abort);
		t_uint32 count; in >> count;
		for(t_uint32 walk = 0; walk < count; ++walk) {
			typename TList::t_item item; in >> item; this->add_item(item);
		}
	}
};

template<typename TObj>
class cfg_obj : public cfg_var, public TObj {
public:
	cfg_obj(const GUID& guid) : cfg_var(guid), TObj() {}
	template<typename TInitData> cfg_obj(const GUID& guid,const TInitData& initData) : cfg_var(guid), TObj(initData) {}

	TObj & val() {return *this;}
	TObj const & val() const {return *this;}

	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {
		stream_writer_formatter<> out(*p_stream,p_abort);
		const TObj * ptr = this;
		out << *ptr;
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		stream_reader_formatter<> in(*p_stream,p_abort);
		TObj * ptr = this;
		in >> *ptr;
	}
};

template<typename TObj, typename TImport> class cfg_objListImporter : private cfg_var_reader {
public:
	typedef cfg_objList<TObj> TMasterVar;
	cfg_objListImporter(TMasterVar & var, const GUID & guid) : m_var(var), cfg_var_reader(guid) {}

private:
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		TImport temp;
		try {
			stream_reader_formatter<> in(*p_stream,p_abort);
			t_uint32 count; in >> count;
			m_var.set_count(count);
			for(t_uint32 walk = 0; walk < count; ++walk) {
				in >> temp;
				m_var[walk] = temp;
			}
		} catch(...) {
			m_var.remove_all();
			throw;
		}
	}
	TMasterVar & m_var;
};

#endif
