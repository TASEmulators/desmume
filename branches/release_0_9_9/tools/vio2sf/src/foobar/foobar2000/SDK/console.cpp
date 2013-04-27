#include "foobar2000.h"


void console::info(const char * p_message) {print(p_message);}
void console::error(const char * p_message) {complain("Error", p_message);}
void console::warning(const char * p_message) {complain("Warning", p_message);}

void console::info_location(const playable_location & src) {print_location(src);}
void console::info_location(const metadb_handle_ptr & src) {print_location(src);}

void console::print_location(const metadb_handle_ptr & src)
{
	print_location(src->get_location());
}

void console::print_location(const playable_location & src)
{
	formatter() << src;
}

void console::complain(const char * what, const char * msg) {
	formatter() << what << ": " << msg;
}
void console::complain(const char * what, std::exception const & e) {
	complain(what, e.what());
}

void console::print(const char* p_message)
{
	if (core_api::are_services_available()) {
		service_ptr_t<console_receiver> ptr;
		service_enum_t<console_receiver> e;
		while(e.next(ptr)) ptr->print(p_message,infinite);
	}
}

void console::printf(const char* p_format,...)
{
	va_list list;
	va_start(list,p_format);
	printfv(p_format,list);
	va_end(list);
}

void console::printfv(const char* p_format,va_list p_arglist)
{
	pfc::string8_fastalloc temp;
	uPrintfV(temp,p_format,p_arglist);
	print(temp);
}