#include "foobar2000.h"


double playback_control::playback_get_length()
{
	double rv = 0;
	metadb_handle_ptr ptr;
	if (get_now_playing(ptr))
	{
		rv = ptr->get_length();
	}
	return rv;
}
