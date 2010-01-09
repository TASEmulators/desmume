//! Entrypoint class for adding items to Advanced Preferences page. \n
//! Implementations must derive from one of subclasses: advconfig_branch, advconfig_entry_checkbox, advconfig_entry_string. \n
//! Implementations are typically registered using static service_factory_single_t<myclass>, or using provided helper classes in case of standard implementations declared in this header.
class NOVTABLE advconfig_entry : public service_base {
public:
	virtual void get_name(pfc::string_base & p_out) = 0;
	virtual GUID get_guid() = 0;
	virtual GUID get_parent() = 0;
	virtual void reset() = 0;
	virtual double get_sort_priority() = 0;

	static bool g_find(service_ptr_t<advconfig_entry>& out, const GUID & id) {
		service_enum_t<advconfig_entry> e; service_ptr_t<advconfig_entry> ptr; while(e.next(ptr)) { if (ptr->get_guid() == id) {out = ptr; return true;} } return false;
	}

	static const GUID guid_root;
	static const GUID guid_branch_tagging,guid_branch_decoding,guid_branch_tools,guid_branch_playback,guid_branch_display;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(advconfig_entry);
};

//! Creates a new branch in Advanced Preferences. \n
//! Implementation: see advconfig_branch_impl / advconfig_branch_factory.
class NOVTABLE advconfig_branch : public advconfig_entry {
public:
	FB2K_MAKE_SERVICE_INTERFACE(advconfig_branch,advconfig_entry);
};

//! Creates a checkbox/radiocheckbox entry in Advanced Preferences. \n
//! The difference between checkboxes and radiocheckboxes is different icon (obviously) and that checking a radiocheckbox unchecks all other radiocheckboxes in the same branch. \n
//! Implementation: see advconfig_entry_checkbox_impl / advconfig_checkbox_factory_t.
class NOVTABLE advconfig_entry_checkbox : public advconfig_entry {
public:
	virtual bool get_state() = 0;
	virtual void set_state(bool p_state) = 0;
	virtual bool is_radio() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(advconfig_entry_checkbox,advconfig_entry);
};

//! Creates a string/integer editbox entry in Advanced Preferences.\n
//! Implementation: see advconfig_entry_string_impl / advconfig_string_factory.
class NOVTABLE advconfig_entry_string : public advconfig_entry {
public:
	virtual void get_state(pfc::string_base & p_out) = 0;
	virtual void set_state(const char * p_string,t_size p_length = infinite) = 0;
	virtual t_uint32 get_flags() = 0;

	enum {
		flag_is_integer		= 1 << 0, 
		flag_is_signed		= 1 << 1,
	};

	FB2K_MAKE_SERVICE_INTERFACE(advconfig_entry_string,advconfig_entry);
};


//! Standard implementation of advconfig_branch. \n
//! Usage: no need to use this class directly - use advconfig_branch_factory instead.
class advconfig_branch_impl : public advconfig_branch {
public:
	advconfig_branch_impl(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority) : m_name(p_name), m_guid(p_guid), m_parent(p_parent), m_priority(p_priority) {}
	void get_name(pfc::string_base & p_out) {p_out = m_name;}
	GUID get_guid() {return m_guid;}
	GUID get_parent() {return m_parent;}
	void reset() {}
	double get_sort_priority() {return m_priority;}
private:
	pfc::string8 m_name;
	GUID m_guid,m_parent;
	const double m_priority;
};

//! Standard implementation of advconfig_entry_checkbox. \n
//! p_is_radio parameter controls whether we're implementing a checkbox or a radiocheckbox (see advconfig_entry_checkbox description for more details).
template<bool p_is_radio = false>
class advconfig_entry_checkbox_impl : public advconfig_entry_checkbox {
public:
	advconfig_entry_checkbox_impl(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,bool p_initialstate)
		: m_name(p_name), m_initialstate(p_initialstate), m_state(p_guid,p_initialstate), m_parent(p_parent), m_priority(p_priority) {}
	
	void get_name(pfc::string_base & p_out) {p_out = m_name;}
	GUID get_guid() {return m_state.get_guid();}
	GUID get_parent() {return m_parent;}
	void reset() {m_state = m_initialstate;}
	bool get_state() {return m_state;}
	void set_state(bool p_state) {m_state = p_state;}
	bool is_radio() {return p_is_radio;}
	double get_sort_priority() {return m_priority;}
	bool get_state_() const {return m_state;}
private:
	pfc::string8 m_name;
	const bool m_initialstate;
	cfg_bool m_state;
	GUID m_parent;
	const double m_priority;
};

//! Service factory helper around standard advconfig_branch implementation. Use this class to register your own Advanced Preferences branches. \n
//! Usage: static advconfig_branch_factory mybranch(name, branchID, parentBranchID, priority);
class advconfig_branch_factory : public service_factory_single_t<advconfig_branch_impl> {
public:
	advconfig_branch_factory(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority)
		: service_factory_single_t<advconfig_branch_impl>(p_name,p_guid,p_parent,p_priority) {}
};

//! Service factory helper around standard advconfig_entry_checkbox implementation. Use this class to register your own Advanced Preferences checkbox/radiocheckbox entries. \n
//! Usage: static advconfig_entry_checkbox<isRadioBox> mybox(name, itemID, parentID, priority, initialstate);
template<bool p_is_radio>
class advconfig_checkbox_factory_t : public service_factory_single_t<advconfig_entry_checkbox_impl<p_is_radio> > {
public:
	advconfig_checkbox_factory_t(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,bool p_initialstate) 
		: service_factory_single_t<advconfig_entry_checkbox_impl<p_is_radio> >(p_name,p_guid,p_parent,p_priority,p_initialstate) {}

	bool get() const {return get_static_instance().get_state_();}
	void set(bool val) {get_static_instance().set_state(val);}
	operator bool() const {return get();}
	bool operator=(bool val) {set(val); return val;}
};

//! Service factory helper around standard advconfig_entry_checkbox implementation, specialized for checkboxes (rather than radiocheckboxes). See advconfig_checkbox_factory_t<> for more details.
typedef advconfig_checkbox_factory_t<false> advconfig_checkbox_factory;
//! Service factory helper around standard advconfig_entry_checkbox implementation, specialized for radiocheckboxes (rather than standard checkboxes). See advconfig_checkbox_factory_t<> for more details.
typedef advconfig_checkbox_factory_t<true> advconfig_radio_factory;


//! Standard advconfig_entry_string implementation. Use advconfig_string_factory to register your own string entries in Advanced Preferences instead of using this class directly.
class advconfig_entry_string_impl : public advconfig_entry_string {
public:
	advconfig_entry_string_impl(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,const char * p_initialstate)
		: m_name(p_name), m_parent(p_parent), m_priority(p_priority), m_initialstate(p_initialstate), m_state(p_guid,p_initialstate) {}
	void get_name(pfc::string_base & p_out) {p_out = m_name;}
	GUID get_guid() {return m_state.get_guid();}
	GUID get_parent() {return m_parent;}
	void reset() {core_api::ensure_main_thread();m_state = m_initialstate;}
	double get_sort_priority() {return m_priority;}
	void get_state(pfc::string_base & p_out) {core_api::ensure_main_thread();p_out = m_state;}
	void set_state(const char * p_string,t_size p_length = infinite) {core_api::ensure_main_thread();m_state.set_string(p_string,p_length);}
	t_uint32 get_flags() {return 0;}
private:
	const pfc::string8 m_initialstate, m_name;
	cfg_string m_state;
	const double m_priority;
	const GUID m_parent;
};

//! Service factory helper around standard advconfig_entry_string implementation. Use this class to register your own string entries in Advanced Preferences. \n
//! Usage: static advconfig_string_factory mystring(name, itemID, branchID, priority, initialValue);
class advconfig_string_factory : public service_factory_single_t<advconfig_entry_string_impl> {
public:
	advconfig_string_factory(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,const char * p_initialstate) 
		: service_factory_single_t<advconfig_entry_string_impl>(p_name,p_guid,p_parent,p_priority,p_initialstate) {}

	void get(pfc::string_base & out) {get_static_instance().get_state(out);}
	void set(const char * in) {get_static_instance().set_state(in);}
};


//! Special advconfig_entry_string implementation - implements integer entries. Use advconfig_integer_factory to register your own integer entries in Advanced Preferences instead of using this class directly.
class advconfig_entry_integer_impl : public advconfig_entry_string {
public:
	advconfig_entry_integer_impl(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,t_uint64 p_initialstate,t_uint64 p_min,t_uint64 p_max)
		: m_name(p_name), m_parent(p_parent), m_priority(p_priority), m_initval(p_initialstate), m_min(p_min), m_max(p_max), m_state(p_guid,p_initialstate) {}
	void get_name(pfc::string_base & p_out) {p_out = m_name;}
	GUID get_guid() {return m_state.get_guid();}
	GUID get_parent() {return m_parent;}
	void reset() {m_state = m_initval;}
	double get_sort_priority() {return m_priority;}
	void get_state(pfc::string_base & p_out) {p_out = pfc::format_uint(m_state.get_value());}
	void set_state(const char * p_string,t_size p_length) {set_state_int(pfc::atoui64_ex(p_string,p_length));}
	t_uint32 get_flags() {return advconfig_entry_string::flag_is_integer;}

	t_uint64 get_state_int() const {return m_state;}
	void set_state_int(t_uint64 val) {m_state = pfc::clip_t<t_uint64>(val,m_min,m_max);}
private:
	cfg_int_t<t_uint64> m_state;
	const double m_priority;
	const t_uint64 m_initval, m_min, m_max;
	const GUID m_parent;
	const pfc::string8 m_name;
};

//! Service factory helper around integer-specialized advconfig_entry_string implementation. Use this class to register your own integer entries in Advanced Preferences. \n
//! Usage: static advconfig_integer_factory myint(name, itemID, parentID, priority, initialValue, minValue, maxValue);
class advconfig_integer_factory : public service_factory_single_t<advconfig_entry_integer_impl> {
public:
	advconfig_integer_factory(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,t_uint64 p_initialstate,t_uint64 p_min,t_uint64 p_max) 
		: service_factory_single_t<advconfig_entry_integer_impl>(p_name,p_guid,p_parent,p_priority,p_initialstate,p_min,p_max) {}

	t_uint64 get() const {return get_static_instance().get_state_int();}
	void set(t_uint64 val) {get_static_instance().set_state_int(val);}

	operator t_uint64() const {return get();}
	t_uint64 operator=(t_uint64 val) {set(val); return val;}
};


//! Not currently used, reserved for future use.
class NOVTABLE advconfig_entry_enum : public advconfig_entry {
public:
	virtual t_size get_value_count() = 0;
	virtual void enum_value(pfc::string_base & p_out,t_size p_index) = 0;
	virtual t_size get_state() = 0;
	virtual void set_state(t_size p_value) = 0;
	
	FB2K_MAKE_SERVICE_INTERFACE(advconfig_entry_enum,advconfig_entry);
};




//! Special version if advconfig_entry_string_impl that allows the value to be retrieved from worker threads.
class advconfig_entry_string_impl_MT : public advconfig_entry_string {
public:
	advconfig_entry_string_impl_MT(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,const char * p_initialstate)
		: m_name(p_name), m_parent(p_parent), m_priority(p_priority), m_initialstate(p_initialstate), m_state(p_guid,p_initialstate) {}
	void get_name(pfc::string_base & p_out) {p_out = m_name;}
	GUID get_guid() {return m_state.get_guid();}
	GUID get_parent() {return m_parent;}
	void reset() {
		insync(m_sync);
		m_state = m_initialstate;
	}
	double get_sort_priority() {return m_priority;}
	void get_state(pfc::string_base & p_out) {
		insync(m_sync);
		p_out = m_state;
	}
	void set_state(const char * p_string,t_size p_length = infinite) {
		insync(m_sync);
		m_state.set_string(p_string,p_length);
	}
	t_uint32 get_flags() {return 0;}
private:
	const pfc::string8 m_initialstate, m_name;
	cfg_string m_state;
	critical_section m_sync;
	const double m_priority;
	const GUID m_parent;
};

//! Special version if advconfig_string_factory that allows the value to be retrieved from worker threads.
class advconfig_string_factory_MT : public service_factory_single_t<advconfig_entry_string_impl_MT> {
public:
	advconfig_string_factory_MT(const char * p_name,const GUID & p_guid,const GUID & p_parent,double p_priority,const char * p_initialstate) 
		: service_factory_single_t<advconfig_entry_string_impl_MT>(p_name,p_guid,p_parent,p_priority,p_initialstate) {}

	void get(pfc::string_base & out) {get_static_instance().get_state(out);}
	void set(const char * in) {get_static_instance().set_state(in);}
};




/* 
  Advanced Preferences variable declaration examples 
	
	static advconfig_string_factory mystring("name goes here",myguid,parentguid,0,"asdf");
	to retrieve state: pfc::string8 val; mystring.get(val);

	static advconfig_checkbox_factory mycheckbox("name goes here",myguid,parentguid,0,false);
	to retrieve state: mycheckbox.get();

	static advconfig_integer_factory myint("name goes here",myguid,parentguid,0,initialValue,minimumValue,maximumValue);
	to retrieve state: myint.get();
*/
