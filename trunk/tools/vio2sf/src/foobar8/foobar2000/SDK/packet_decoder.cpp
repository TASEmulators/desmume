#include "foobar2000.h"

packet_decoder * packet_decoder::g_open(const GUID & owner,unsigned param1,const void * param2,unsigned param2size,file_info * info)
{
	service_enum_t<packet_decoder> e;
	packet_decoder * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->open_stream(owner,param1,param2,param2size,info)) return ptr;
		ptr->service_release();
	}
	return 0;
}
