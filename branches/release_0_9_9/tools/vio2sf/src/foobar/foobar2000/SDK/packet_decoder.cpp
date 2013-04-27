#include "foobar2000.h"

void packet_decoder::g_open(service_ptr_t<packet_decoder> & p_out,bool p_decode,const GUID & p_owner,t_size p_param1,const void * p_param2,t_size p_param2size,abort_callback & p_abort)
{
	service_enum_t<packet_decoder_entry> e;
	service_ptr_t<packet_decoder_entry> ptr;
	while(e.next(ptr)) {
		if (ptr->is_our_setup(p_owner,p_param1,p_param2,p_param2size)) {
			ptr->open(p_out,p_decode,p_owner,p_param1,p_param2,p_param2size,p_abort);
			return;
		}
	}
	throw exception_io_data();
}