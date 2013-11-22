#include "foobar2000.h"

bool mainmenu_commands::g_execute(const GUID & p_guid,service_ptr_t<service_base> p_callback) {
	service_enum_t<mainmenu_commands> e;
	service_ptr_t<mainmenu_commands> ptr;
	while(e.next(ptr)) {
		const t_uint32 count = ptr->get_command_count();
		for(t_uint32 n=0;n<count;n++) {
			if (ptr->get_command(n) == p_guid) {
				ptr->execute(n,p_callback);
				return true;
			}
		}
	}
	return false;
}

bool mainmenu_commands::g_find_by_name(const char * p_name,GUID & p_guid) {
	service_enum_t<mainmenu_commands> e;
	service_ptr_t<mainmenu_commands> ptr;
	pfc::string8_fastalloc temp;
	while(e.next(ptr)) {
		const t_uint32 count = ptr->get_command_count();
		for(t_uint32 n=0;n<count;n++) {
			ptr->get_name(n,temp);
			if (stricmp_utf8(temp,p_name) == 0) {
				p_guid = ptr->get_command(n);
				return true;
			}
		}
	}
	return false;

}