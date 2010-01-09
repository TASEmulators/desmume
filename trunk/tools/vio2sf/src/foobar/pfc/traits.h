namespace pfc {

	class traits_default {
	public:
		enum { 
			realloc_safe = false, 
			needs_destructor = true, 
			needs_constructor = true, 
			constructor_may_fail = true
		};
	};
	
	class traits_default_movable {
	public:
		enum { 
			realloc_safe = true, 
			needs_destructor = true, 
			needs_constructor = true, 
			constructor_may_fail = true
		};
	};

	class traits_rawobject : public traits_default {
	public:
		enum { 
			realloc_safe = true, 
			needs_destructor = false, 
			needs_constructor = false, 
			constructor_may_fail = false
		};
	};

	class traits_vtable {
	public:
		enum {
			realloc_safe = true,
			needs_destructor = true,
			needs_constructor = true,
			constructor_may_fail = false
		};
	};

	template<typename T> class traits_t : public traits_default {};

	template<typename traits1,typename traits2>
	class combine_traits {
	public:
		enum {
			realloc_safe = (traits1::realloc_safe && traits2::realloc_safe),
			needs_destructor = (traits1::needs_destructor || traits2::needs_destructor),
			needs_constructor = (traits1::needs_constructor || traits2::needs_constructor),
			constructor_may_fail = (traits1::constructor_may_fail || traits2::constructor_may_fail),
		};
	};

	template<typename type1, typename type2>
	class traits_combined : public combine_traits<pfc::traits_t<type1>,pfc::traits_t<type2> > {};

	template<typename T> class traits_t<T*> : public traits_rawobject {};

	template<> class traits_t<char> : public traits_rawobject {};
	template<> class traits_t<unsigned char> : public traits_rawobject {};
	template<> class traits_t<wchar_t> : public traits_rawobject {};
	template<> class traits_t<short> : public traits_rawobject {};
	template<> class traits_t<unsigned short> : public traits_rawobject {};
	template<> class traits_t<int> : public traits_rawobject {};
	template<> class traits_t<unsigned int> : public traits_rawobject {};
	template<> class traits_t<long> : public traits_rawobject {};
	template<> class traits_t<unsigned long> : public traits_rawobject {};
	template<> class traits_t<long long> : public traits_rawobject {};
	template<> class traits_t<unsigned long long> : public traits_rawobject {};
	template<> class traits_t<bool> : public traits_rawobject {};

	template<> class traits_t<float> : public traits_rawobject {};
	template<> class traits_t<double> : public traits_rawobject {};
	
	template<> class traits_t<GUID> : public traits_rawobject {};

	template<typename T,t_size p_count>
	class traits_t<T[p_count]> : public traits_t<T> {};
}