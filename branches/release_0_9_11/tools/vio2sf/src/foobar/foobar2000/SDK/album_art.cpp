#include "foobar2000.h"

bool album_art_editor::g_get_interface(service_ptr_t<album_art_editor> & out,const char * path) {
	service_enum_t<album_art_editor> e; ptr ptr;
	pfc::string_extension ext(path);
	while(e.next(ptr)) {
		if (ptr->is_our_path(path,ext)) {
			out = ptr; return true;
		}
	}
	return false;
}

bool album_art_editor::g_is_supported_path(const char * path) {
	ptr ptr;
	return g_get_interface(ptr,path);
}