#include "foobar2000.h"
#include "component.h"

foobar2000_api * g_api;

service_class_ref service_factory_base::enum_find_class(const GUID & p_guid)
{
	PFC_ASSERT(core_api::are_services_available() && g_api);
	return g_api->service_enum_find_class(p_guid);
}

bool service_factory_base::enum_create(service_ptr_t<service_base> & p_out,service_class_ref p_class,t_size p_index)
{
	PFC_ASSERT(core_api::are_services_available() && g_api);
	return g_api->service_enum_create(p_out,p_class,p_index);
}

t_size service_factory_base::enum_get_count(service_class_ref p_class)
{
	PFC_ASSERT(core_api::are_services_available() && g_api);
	return g_api->service_enum_get_count(p_class);
}

service_factory_base * service_factory_base::__internal__list = NULL;





namespace {
	class main_thread_callback_release_object : public main_thread_callback {
	public:
		main_thread_callback_release_object(service_ptr obj) : m_object(obj) {}
		void callback_run() {
			try { m_object.release(); } catch(...) {}
		}
	private:
		service_ptr m_object;
	};
}
namespace service_impl_helper {
	void release_object_delayed(service_ptr obj) {
		static_api_ptr_t<main_thread_callback_manager>()->add_callback(new service_impl_t<main_thread_callback_release_object>(obj));
	}
};


void _standard_api_create_internal(service_ptr & out, const GUID & classID) {
	service_class_ref c = service_factory_base::enum_find_class(classID);
	switch(service_factory_base::enum_get_count(c)) {
		case 0:
			throw exception_service_not_found();
		case 1:
			PFC_ASSERT_SUCCESS( service_factory_base::enum_create(out, c, 0) );
			break;
		default:
			throw exception_service_duplicated();
	}
}
