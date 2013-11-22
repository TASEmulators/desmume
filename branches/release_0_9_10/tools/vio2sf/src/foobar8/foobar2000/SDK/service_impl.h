//only meant to be included from service.h

class service_reference_counter
{
private:
	long refcount;
public:
	inline service_reference_counter() : refcount(1) {}
	inline long increment() {return interlocked_increment(&refcount);}
	inline long decrement() {return interlocked_decrement(&refcount);}
};

#define __implement_service_base \
	private:	\
		service_reference_counter m_reference_counter;	\
	public:	\
		virtual int service_release() {long ret = m_reference_counter.decrement(); if (ret == 0) delete this; return ret;} \
		virtual int service_add_ref() {return m_reference_counter.increment();}

#define __implement_service_base_single \
	public:	\
		virtual int service_release() {return 1;}	\
		virtual int service_add_ref() {return 1;}

template<class T>
class service_impl_t : public T
{
	__implement_service_base;	
};

template<class T,class P1>
class service_impl_p1_t : public T
{
	__implement_service_base;
public:	
	service_impl_p1_t(P1 p1) : T(p1) {}
};

template<class T,class P1,class P2>
class service_impl_p2_t : public T
{
	__implement_service_base;
public:	
	service_impl_p2_t(P1 p1,P2 p2) : T(p1,p2) {}
};

template<class T,class P1,class P2,class P3>
class service_impl_p3_t : public T
{
	__implement_service_base;
public:	
	service_impl_p3_t(P1 p1,P2 p2,P3 p3) : T(p1,p2,p3) {}
};

template<class T,class P1,class P2,class P3,class P4>
class service_impl_p4_t : public T
{
	__implement_service_base;
public:	
	service_impl_p4_t(P1 p1,P2 p2,P3 p3,P4 p4) : T(p1,p2,p3,p4) {}
};


template<class T>
class service_impl_single_t : public T
{
	__implement_service_base_single;
};

template<class T,class P1>
class service_impl_single_p1_t : public T
{
	__implement_service_base_single;
public:
	service_impl_single_p1_t(P1 p1) : T(p1) {}
};

template<class T,class P1,class P2>
class service_impl_single_p2_t : public T
{
	__implement_service_base_single;
public:	
	service_impl_single_p2_t(P1 p1,P2 p2) : T(p1,p2) {}
};

template<class T,class P1,class P2,class P3>
class service_impl_single_p3_t : public T
{
	__implement_service_base_single;
public:	
	service_impl_single_p3_t(P1 p1,P2 p2,P3 p3) : T(p1,p2,p3) {}
};

template<class T,class P1,class P2,class P3,class P4>
class service_impl_single_p4_t : public T
{
	__implement_service_base_single;
public:
	service_impl_single_p4_t(P1 p1,P2 p2,P3 p3,P4 p4) : T(p1,p2,p3,p4) {}
};


#undef __implement_service_base
#undef __implement_service_base_single
