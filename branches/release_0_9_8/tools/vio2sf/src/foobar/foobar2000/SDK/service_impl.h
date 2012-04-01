//! Template implementing reference-counting features of service_base. Intended for dynamic instantiation: "new service_impl_t<someclass>(param1,param2);"; should not be instantiated otherwise (no local/static/member objects) because it does a "delete this;" when reference count reaches zero.\n
//! Note that some constructor parameters such as NULL will need explicit typecasts to ensure correctly guessed types for constructor function template (null string needs to be (const char*)NULL rather than just NULL, etc).
template<typename T>
class service_impl_t : public T
{
public:
	int FB2KAPI service_release() throw() {
		int ret = --m_counter; 
		if (ret == 0) {
			PFC_ASSERT_NO_EXCEPTION( delete this );
		}
		return ret;
	}
	int FB2KAPI service_add_ref() throw() {return ++m_counter;}

	TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(service_impl_t,T)
private:
	pfc::refcounter m_counter;
};

//! Template implementing dummy version of reference-counting features of service_base. Intended for static/local/member instantiation: "static service_impl_single_t<someclass> myvar(params);". Because reference counting features are disabled (dummy reference counter), code instantiating it is responsible for deleting it as well as ensuring that no references are active when the object gets deleted.\n
//! Note that some constructor parameters such as NULL will need explicit typecasts to ensure correctly guessed types for constructor function template (null string needs to be (const char*)NULL rather than just NULL, etc).
template<typename T>
class service_impl_single_t : public T
{
public:
	int FB2KAPI service_release() throw() {return 1;}
	int FB2KAPI service_add_ref() throw() {return 1;}

	TEMPLATE_CONSTRUCTOR_FORWARD_FLOOD(service_impl_single_t,T)
};


namespace service_impl_helper {
	void release_object_delayed(service_ptr obj);
};

