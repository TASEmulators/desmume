#ifndef _foobar2000_sdk_service_h_included_
#define _foobar2000_sdk_service_h_included_

typedef const void* service_class_ref;

PFC_DECLARE_EXCEPTION(exception_service_not_found,pfc::exception,"Service not found");
PFC_DECLARE_EXCEPTION(exception_service_extension_not_found,pfc::exception,"Service extension not found");
PFC_DECLARE_EXCEPTION(exception_service_duplicated,pfc::exception,"Service duplicated");

#ifdef _MSC_VER
#define FOOGUIDDECL __declspec(selectany)
#else
#define FOOGUIDDECL
#endif


#define DECLARE_GUID(NAME,A,S,D,F,G,H,J,K,L,Z,X) FOOGUIDDECL const GUID NAME = {A,S,D,{F,G,H,J,K,L,Z,X}};
#define DECLARE_CLASS_GUID(NAME,A,S,D,F,G,H,J,K,L,Z,X) FOOGUIDDECL const GUID NAME::class_guid = {A,S,D,{F,G,H,J,K,L,Z,X}};

//! Special hack to ensure errors when someone tries to ->service_add_ref()/->service_release() on a service_ptr_t
template<typename T> class service_obscure_refcounting : public T {
private:
	int service_add_ref() throw();
	int service_release() throw();
};

//! Converts a service interface pointer to a pointer that obscures service counter functionality.
template<typename T> static inline service_obscure_refcounting<T>* service_obscure_refcounting_cast(T * p_source) throw() {return static_cast<service_obscure_refcounting<T>*>(p_source);}

//Must be templated instead of taking service_base* because of multiple inheritance issues.
template<typename T> static void service_release_safe(T * p_ptr) throw() {
	if (p_ptr != NULL) PFC_ASSERT_NO_EXCEPTION( p_ptr->service_release() );
}

//Must be templated instead of taking service_base* because of multiple inheritance issues.
template<typename T> static void service_add_ref_safe(T * p_ptr) throw() {
	if (p_ptr != NULL) PFC_ASSERT_NO_EXCEPTION( p_ptr->service_add_ref() );
}

class service_base;

//! Autopointer class to be used with all services. Manages reference counter calls behind-the-scenes.
template<typename T>
class service_ptr_t {
private:
	typedef service_ptr_t<T> t_self;
public:
	inline service_ptr_t() throw() : m_ptr(NULL) {}
	inline service_ptr_t(T* p_ptr) throw() : m_ptr(NULL) {copy(p_ptr);}
	inline service_ptr_t(const t_self & p_source) throw() : m_ptr(NULL) {copy(p_source);}

	template<typename t_source>
	inline service_ptr_t(t_source * p_ptr) throw() : m_ptr(NULL) {copy(p_ptr);}

	template<typename t_source>
	inline service_ptr_t(const service_ptr_t<t_source> & p_source) throw() : m_ptr(NULL) {copy(p_source);}

	inline ~service_ptr_t() throw() {service_release_safe(m_ptr);}
	
	template<typename t_source>
	void copy(t_source * p_ptr) throw() {
		service_add_ref_safe(p_ptr);
		service_release_safe(m_ptr);
		m_ptr = pfc::safe_ptr_cast<T>(p_ptr);
		
	}

	template<typename t_source>
	inline void copy(const service_ptr_t<t_source> & p_source) throw() {copy(p_source.get_ptr());}


	inline const t_self & operator=(const t_self & p_source) throw() {copy(p_source); return *this;}
	inline const t_self & operator=(T * p_ptr) throw() {copy(p_ptr); return *this;}

	template<typename t_source> inline t_self & operator=(const service_ptr_t<t_source> & p_source) throw() {copy(p_source); return *this;}
	template<typename t_source> inline t_self & operator=(t_source * p_ptr) throw() {copy(p_ptr); return *this;}
	
	inline void release() throw() {
		service_release_safe(m_ptr);
		m_ptr = NULL;
	}


	inline service_obscure_refcounting<T>* operator->() const throw() {PFC_ASSERT(m_ptr != NULL);return service_obscure_refcounting_cast(m_ptr);}

	inline T* get_ptr() const throw() {return m_ptr;}
	
	inline bool is_valid() const throw() {return m_ptr != NULL;}
	inline bool is_empty() const throw() {return m_ptr == NULL;}

	inline bool operator==(const t_self & p_item) const throw() {return m_ptr == p_item.get_ptr();}
	inline bool operator!=(const t_self & p_item) const throw() {return m_ptr != p_item.get_ptr();}
	inline bool operator>(const t_self & p_item) const throw() {return m_ptr > p_item.get_ptr();}
	inline bool operator<(const t_self & p_item) const throw() {return m_ptr < p_item.get_ptr();}

	template<typename t_other>
	inline t_self & operator<<(service_ptr_t<t_other> & p_source) throw() {attach(p_source.detach());return *this;}
	template<typename t_other>
	inline t_self & operator>>(service_ptr_t<t_other> & p_dest) throw() {p_dest.attach(detach());return *this;}


	inline T* __unsafe_duplicate() const throw() {//should not be used ! temporary !
		service_add_ref_safe(m_ptr);
		return m_ptr;
	}

	inline T* detach() throw() {
		return pfc::replace_null_t(m_ptr);
	}

	template<typename t_source>
	inline void attach(t_source * p_ptr) throw() {
		service_release_safe(m_ptr);
		m_ptr = pfc::safe_ptr_cast<T>(p_ptr);
	}

	T & operator*() const throw() {return *m_ptr;}

	service_ptr_t<service_base> & _as_base_ptr() {
		PFC_ASSERT( _as_base_ptr_check() );
		return *reinterpret_cast<service_ptr_t<service_base>*>(this);
	}
	static bool _as_base_ptr_check() {
		return static_cast<service_base*>((T*)NULL) == reinterpret_cast<service_base*>((T*)NULL);
	}
private:
	T* m_ptr;
};

namespace pfc {
	template<typename T>
	class traits_t<service_ptr_t<T> > : public traits_default {
	public:
		enum { realloc_safe = true, constructor_may_fail = false};
	};
}


template<typename T, template<typename> class t_alloc = pfc::alloc_fast>
class service_list_t : public pfc::list_t<service_ptr_t<T>, t_alloc >
{
};

//! Helper macro for use when defining a service class. Generates standard features of a service, without ability to register using service_factory / enumerate using service_enum_t. \n
//! This is used for declaring services that are meant to be instantiated by means other than service_enum_t (or non-entrypoint services), or extensions of services (including extension of entrypoint services).	\n
//! Sample non-entrypoint declaration: class myclass : public service_base {...; FB2K_MAKE_SERVICE_INTERFACE(myclass, service_base); };	\n
//! Sample extension declaration: class myclass : public myotherclass {...; FB2K_MAKE_SERVICE_INTERFACE(myclass, myotherclass); };	\n
//! This macro is intended for use ONLY WITH INTERFACE CLASSES, not with implementation classes.
#define FB2K_MAKE_SERVICE_INTERFACE(THISCLASS,PARENTCLASS) \
	public:	\
		typedef THISCLASS t_interface;	\
		typedef PARENTCLASS t_interface_parent;	\
			\
		static const GUID class_guid;	\
			\
		virtual bool service_query(service_ptr_t<service_base> & p_out,const GUID & p_guid) {	\
			if (p_guid == class_guid) {p_out = this; return true;}	\
			else return PARENTCLASS::service_query(p_out,p_guid);	\
		}	\
		typedef service_ptr_t<t_interface> ptr;	\
	protected:	\
		THISCLASS() {}	\
		~THISCLASS() {}	\
	private:	\
		const THISCLASS & operator=(const THISCLASS &) {throw pfc::exception_not_implemented();}	\
		THISCLASS(const THISCLASS &) {throw pfc::exception_not_implemented();}	\
	private:	\
		void __private__service_declaration_selftest() {	\
			pfc::assert_same_type<PARENTCLASS,PARENTCLASS::t_interface>(); /*parentclass must be an interface*/	\
			__validate_service_class_helper<THISCLASS>(); /*service_base must be reachable by walking t_interface_parent*/	\
			pfc::safe_cast<service_base*>(this); /*this class must derive from service_base, directly or indirectly, and be implictly castable to it*/ \
		}

//! Helper macro for use when defining an entrypoint service class. Generates standard features of a service, including ability to register using service_factory and enumerate using service_enum.	\n
//! Sample declaration: class myclass : public service_base {...; FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(myclass); };	\n
//! Note that entrypoint service classes must directly derive from service_base, and not from another service class.
//! This macro is intended for use ONLY WITH INTERFACE CLASSES, not with implementation classes.
#define FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(THISCLASS)	\
	public:	\
		typedef THISCLASS t_interface_entrypoint;	\
	FB2K_MAKE_SERVICE_INTERFACE(THISCLASS,service_base)
		
	
#define FB2K_DECLARE_SERVICE_BEGIN(THISCLASS,BASECLASS) \
	class NOVTABLE THISCLASS : public BASECLASS	{	\
		FB2K_MAKE_SERVICE_INTERFACE(THISCLASS,BASECLASS);	\
	public:

#define FB2K_DECLARE_SERVICE_END() \
	};

#define FB2K_DECLARE_SERVICE_ENTRYPOINT_BEGIN(THISCLASS) \
	class NOVTABLE THISCLASS : public service_base {	\
		FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(THISCLASS)	\
	public:
		

//! Base class for all service classes.\n
//! Provides interfaces for reference counter and querying for different interfaces supported by the object.\n
class NOVTABLE service_base
{
public:	
	//! Decrements reference count; deletes the object if reference count reaches zero. This is normally not called directly but managed by service_ptr_t<> template.
	//! @returns New reference count. For debug purposes only, in certain conditions return values may be unreliable.
	virtual int service_release() throw() = 0;
	//! Increments reference count. This is normally not called directly but managed by service_ptr_t<> template.
	//! @returns New reference count. For debug purposes only, in certain conditions return values may be unreliable.
	virtual int service_add_ref() throw() = 0;
	//! Queries whether the object supports specific interface and retrieves a pointer to that interface. This is normally not called directly but managed by service_query_t<> function template.
	//! Typical implementation checks the parameter against GUIDs of interfaces supported by this object, if the GUID is one of supported interfaces, p_out is set to service_base pointer that can be static_cast<>'ed to queried interface and the method returns true; otherwise the method returns false.
	virtual bool service_query(service_ptr_t<service_base> & p_out,const GUID & p_guid) {return false;}

	//! Queries whether the object supports specific interface and retrieves a pointer to that interface.
	//! @param p_out Receives pointer to queried interface on success.
	//! returns true on success, false on failure (interface not supported by the object).
	template<class T>
	bool service_query_t(service_ptr_t<T> & p_out)
	{
		pfc::assert_same_type<T,T::t_interface>();
		return service_query( *reinterpret_cast<service_ptr_t<service_base>*>(&p_out),T::class_guid);
	}

	typedef service_base t_interface;
	
protected:
	service_base() {}
	~service_base() {}
private:
	service_base(const service_base&) {throw pfc::exception_not_implemented();}
	const service_base & operator=(const service_base&) {throw pfc::exception_not_implemented();}
};

typedef service_ptr_t<service_base> service_ptr;

template<typename T>
static void __validate_service_class_helper() {
	__validate_service_class_helper<T::t_interface_parent>();
}

template<>
static void __validate_service_class_helper<service_base>() {}


#include "service_impl.h"

class NOVTABLE service_factory_base {
protected:
	inline service_factory_base(const GUID & p_guid) : m_guid(p_guid) {PFC_ASSERT(!core_api::are_services_available());__internal__next=__internal__list;__internal__list=this;}
	inline ~service_factory_base() {PFC_ASSERT(!core_api::are_services_available());}
public:
	inline const GUID & get_class_guid() const {return m_guid;}

	static service_class_ref enum_find_class(const GUID & p_guid);
	static bool enum_create(service_ptr_t<service_base> & p_out,service_class_ref p_class,t_size p_index);
	static t_size enum_get_count(service_class_ref p_class);

	inline static bool is_service_present(const GUID & g) {return enum_get_count(enum_find_class(g))>0;}

	//! Throws std::bad_alloc or another exception on failure.
	virtual void instance_create(service_ptr_t<service_base> & p_out) = 0;

	//! FOR INTERNAL USE ONLY
	static service_factory_base *__internal__list;
	//! FOR INTERNAL USE ONLY
	service_factory_base * __internal__next;
private:
	const GUID & m_guid;
};


template<typename B>
class service_factory_base_t : public service_factory_base {
public:
	service_factory_base_t() : service_factory_base(B::class_guid) {
		pfc::assert_same_type<B,B::t_interface_entrypoint>();
	}

};


template<typename T> static void _validate_service_ptr(service_ptr_t<T> const & ptr) {
	PFC_ASSERT( ptr.is_valid() );
	service_ptr_t<T> test;
	PFC_ASSERT( ptr->service_query_t(test) );
}

#ifdef _DEBUG
#define FB2K_ASSERT_VALID_SERVICE_PTR(ptr) _validate_service_ptr(ptr)
#else
#define FB2K_ASSERT_VALID_SERVICE_PTR(ptr)
#endif

template<class T> static bool service_enum_create_t(service_ptr_t<T> & p_out,t_size p_index) {
	pfc::assert_same_type<T,T::t_interface_entrypoint>();
	service_ptr_t<service_base> ptr;
	if (service_factory_base::enum_create(ptr,service_factory_base::enum_find_class(T::class_guid),p_index)) {
		p_out = static_cast<T*>(ptr.get_ptr());
		return true;
	} else {
		p_out.release();
		return false;
	}
}

template<typename T> static service_class_ref _service_find_class() {
	pfc::assert_same_type<T,T::t_interface_entrypoint>();
	return service_factory_base::enum_find_class(T::class_guid);
}

template<typename what>
static bool _service_instantiate_helper(service_ptr_t<what> & out, service_class_ref servClass, t_size index) {
	/*if (out._as_base_ptr_check()) {
		const bool state = service_factory_base::enum_create(out._as_base_ptr(), servClass, index);
		if (state) { FB2K_ASSERT_VALID_SERVICE_PTR(out); }
		return state;
	} else */{
		service_ptr temp;
		const bool state = service_factory_base::enum_create(temp, servClass, index);
		if (state) {
			out.attach( static_cast<what*>( temp.detach() ) );
			FB2K_ASSERT_VALID_SERVICE_PTR( out );
		}
		return state;
	}
}

template<typename T> class service_class_helper_t {
public:
	service_class_helper_t() : m_class(service_factory_base::enum_find_class(T::class_guid)) {
		pfc::assert_same_type<T,T::t_interface_entrypoint>();
	}
	t_size get_count() const {
		return service_factory_base::enum_get_count(m_class);
	}

	bool create(service_ptr_t<T> & p_out,t_size p_index) const {
		return _service_instantiate_helper(p_out, m_class, p_index);
	}

	service_ptr_t<T> create(t_size p_index) const {
		service_ptr_t<T> temp;
		if (!create(temp,p_index)) throw pfc::exception_bug_check_v2();
		return temp;
	}
	service_class_ref get_class() const {return m_class;}
private:
	service_class_ref m_class;
};

void _standard_api_create_internal(service_ptr & out, const GUID & classID);

template<typename T> static void standard_api_create_t(service_ptr_t<T> & p_out) {
	if (pfc::is_same_type<T,T::t_interface_entrypoint>::value) {
		_standard_api_create_internal(p_out._as_base_ptr(), T::class_guid);
		FB2K_ASSERT_VALID_SERVICE_PTR(p_out);
	} else {
		service_ptr_t<T::t_interface_entrypoint> temp;
		standard_api_create_t(temp);
		if (!temp->service_query_t(p_out)) throw exception_service_extension_not_found();
	}
}

template<typename T> static service_ptr_t<T> standard_api_create_t() {
	service_ptr_t<T> temp;
	standard_api_create_t(temp);
	return temp;
}

template<typename T>
static bool static_api_test_t() {
	typedef T::t_interface_entrypoint EP;
	service_class_helper_t<EP> helper;
	if (helper.get_count() != 1) return false;
	if (!pfc::is_same_type<T,EP>::value) {
		service_ptr_t<T> t;
		if (!helper.create(0)->service_query_t(t)) return false;
	}
	return true;
}

#define FB2K_API_AVAILABLE(API) static_api_test_t<API>()

//! Helper template used to easily access core services. \n
//! Usage: static_api_ptr_t<myclass> api; api->dosomething();
//! Can be used at any point of code, WITH EXCEPTION of static objects that are initialized during the DLL loading process before the service system is initialized; such as static static_api_ptr_t objects or having static_api_ptr_t instances as members of statically created objects.
//! Throws exception_service_not_found if service could not be reached (which can be ignored for core APIs that are always present unless there is some kind of bug in the code).
template<typename t_interface>
class static_api_ptr_t {
public:
	static_api_ptr_t() {
		standard_api_create_t(m_ptr);
	}
	service_obscure_refcounting<t_interface>* operator->() const {return service_obscure_refcounting_cast(m_ptr.get_ptr());}
	t_interface* get_ptr() const {return m_ptr.get_ptr();}
private:
	service_ptr_t<t_interface> m_ptr;
};

//! Helper; simulates array with instance of each available implementation of given service class.
template<typename T> class service_instance_array_t {
public:
	typedef service_ptr_t<T> t_ptr;
	service_instance_array_t() {
		service_class_helper_t<T> helper;
		const t_size count = helper.get_count();
		m_data.set_size(count);
		for(t_size n=0;n<count;n++) m_data[n] = helper.create(n);
	}

	t_size get_size() const {return m_data.get_size();}
	const t_ptr & operator[](t_size p_index) const {return m_data[p_index];}

	//nonconst version to allow sorting/bsearching; do not abuse
	t_ptr & operator[](t_size p_index) {return m_data[p_index];}
private:
	pfc::array_t<t_ptr> m_data;
};

template<typename t_interface>
class service_enum_t {
public:
	service_enum_t() : m_index(0) {
		pfc::assert_same_type<t_interface,typename t_interface::t_interface_entrypoint>();
	}
	void reset() {m_index = 0;}

	template<typename t_query>
	bool first(service_ptr_t<t_query> & p_out) {
		reset();
		return next(p_out);
	}
	
	template<typename t_query>
	bool next(service_ptr_t<t_query> & p_out) {
		pfc::assert_same_type<typename t_query::t_interface_entrypoint,t_interface>();
		if (pfc::is_same_type<t_query,t_interface>::value) {
			return __next(reinterpret_cast<service_ptr_t<t_interface>&>(p_out));
		} else {
			service_ptr_t<t_interface> temp;
			while(__next(temp)) {
				if (temp->service_query_t(p_out)) return true;
			}
			return false;
		}
	}

private:
	bool __next(service_ptr_t<t_interface> & p_out) {
		return m_helper.create(p_out,m_index++);
	}
	unsigned m_index;
	service_class_helper_t<t_interface> m_helper;
};

template<typename T>
class service_factory_t : public service_factory_base_t<typename T::t_interface_entrypoint> {
public:
	void instance_create(service_ptr_t<service_base> & p_out) {
		p_out = pfc::safe_cast<service_base*>(pfc::safe_cast<typename T::t_interface_entrypoint*>(pfc::safe_cast<T*>(  new service_impl_t<T>  )));
	}
};

template<typename T>
class service_factory_single_t : public service_factory_base_t<typename T::t_interface_entrypoint> {
	service_impl_single_t<T> g_instance;
public:
	TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(service_factory_single_t,g_instance)

	void instance_create(service_ptr_t<service_base> & p_out) {
		p_out = pfc::safe_cast<service_base*>(pfc::safe_cast<typename T::t_interface_entrypoint*>(pfc::safe_cast<T*>(&g_instance)));
	}

	inline T& get_static_instance() {return g_instance;}
	inline const T& get_static_instance() const {return g_instance;}
};

template<typename T>
class service_factory_single_ref_t : public service_factory_base_t<typename T::t_interface_entrypoint>
{
private:
	T & instance;
public:
	service_factory_single_ref_t(T& param) : instance(param) {}

	void instance_create(service_ptr_t<service_base> & p_out) {
		p_out = pfc::safe_cast<service_base*>(pfc::safe_cast<typename T::t_interface_entrypoint*>(pfc::safe_cast<T*>(&instance)));
	}

	inline T& get_static_instance() {return instance;}
};


template<typename T>
class service_factory_single_transparent_t : public service_factory_base_t<typename T::t_interface_entrypoint>, public service_impl_single_t<T>
{	
public:
	TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(service_factory_single_transparent_t,service_impl_single_t<T>)

	void instance_create(service_ptr_t<service_base> & p_out) {
		p_out = pfc::safe_cast<service_base*>(pfc::safe_cast<typename T::t_interface_entrypoint*>(pfc::safe_cast<T*>(this)));
	}

	inline T& get_static_instance() {return *(T*)this;}
};















template<typename what>
static bool service_by_guid_fallback(service_ptr_t<what> & out, const GUID & id) {
	service_enum_t<what> e;
	service_ptr_t<what> ptr;
	while(e.next(ptr)) {
		if (ptr->get_guid() == id) {out = ptr; return true;}
	}
	return false;
}

template<typename what>
class service_by_guid_data {
public:
	service_by_guid_data() : m_servClass(), m_inited() {}

	bool ready() const {return m_inited;}

	void initialize() {
		if (m_inited) return;
		pfc::assert_same_type< what, typename what::t_interface_entrypoint >();
		m_servClass = service_factory_base::enum_find_class(what::class_guid);
		const t_size servCount = service_factory_base::enum_get_count(m_servClass);
		for(t_size walk = 0; walk < servCount; ++walk) {
			service_ptr_t<what> temp;
			if (_service_instantiate_helper(temp, m_servClass, walk)) {
				m_order.set(temp->get_guid(), walk);
			}
		}
		m_inited = true;
	}

	bool create(service_ptr_t<what> & out, const GUID & id) const {
		PFC_ASSERT(m_inited);
		t_size index;
		if (!m_order.query(id,index)) return false;
		return _service_instantiate_helper(out, m_servClass, index);
	}
	service_ptr_t<what> create(const GUID & id) const {
		service_ptr_t<what> temp; if (!crete(temp,id)) throw exception_service_not_found(); return temp;
	}

private:
	volatile bool m_inited;
	pfc::map_t<GUID,t_size> m_order;
	service_class_ref m_servClass;
};

template<typename what>
class _service_by_guid_data_container {
public:
	static service_by_guid_data<what> data;
};
template<typename what> service_by_guid_data<what> _service_by_guid_data_container<what>::data;


template<typename what>
static void service_by_guid_init() {
	service_by_guid_data<what> & data = _service_by_guid_data_container<what>::data;
	data.initialize();
}
template<typename what>
static bool service_by_guid(service_ptr_t<what> & out, const GUID & id) {
	pfc::assert_same_type< what, typename what::t_interface_entrypoint >();
	service_by_guid_data<what> & data = _service_by_guid_data_container<what>::data;
	if (data.ready()) {
		//fall-thru
	} else if (core_api::is_main_thread()) {
		data.initialize();
	} else {
#ifdef _DEBUG
		uDebugLog() << "Warning: service_by_guid() used in non-main thread without initialization, using fallback";
#endif
		return service_by_guid_fallback(out,id);
	}
	return data.create(out,id);
}
template<typename what>
static service_ptr_t<what> service_by_guid(const GUID & id) {
	service_ptr_t<what> temp;
	if (!service_by_guid(temp,id)) throw exception_service_not_found();
	return temp;
}

#define FB2K_FOR_EACH_SERVICE(type, call) {service_enum_t<type> e; service_ptr_t<type> ptr; while(e.next(ptr)) {ptr->call;} }

#endif //_foobar2000_sdk_service_h_included_
