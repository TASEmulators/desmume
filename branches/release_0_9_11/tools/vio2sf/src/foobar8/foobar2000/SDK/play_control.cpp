#include "foobar2000.h"


play_control * play_control::get()
{
	static play_control * ptr;
	if (!ptr) ptr = service_enum_create_t(play_control,0);
	return ptr;
}