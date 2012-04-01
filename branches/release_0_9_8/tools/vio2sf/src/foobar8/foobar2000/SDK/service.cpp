#include "foobar2000.h"
#include "component.h"

foobar2000_client g_client;
foobar2000_api * g_api;

service_base * service_factory_base::enum_create(const GUID &g,int n)
{
	assert(core_api::are_services_available());
	return g_api->service_enum_create(g,n);
}

int service_factory_base::enum_get_count(const GUID &g)
{
	assert(core_api::are_services_available());
	return g_api->service_enum_get_count(g);
}

service_factory_base * service_factory_base::list=0;

service_enum::service_enum(const GUID &g)
{
	guid = g;
	reset();
}

void service_enum::reset()
{
	service_ptr=0;
}

service_base * service_enum::next()
{
	return service_factory_base::enum_create(guid,service_ptr++);
}

/*
void service_base::service_release_safe()
{
	try {
		if (this) service_release();
	}
	catch(...)
	{
	}
}
*/
#ifdef WIN32

long interlocked_increment(long * var)
{
	assert(!((unsigned)var&3));
	return InterlockedIncrement(var);
}

long interlocked_decrement(long * var)
{
	assert(!((unsigned)var&3));
	return InterlockedDecrement(var);
}

#else

#error portme

#endif