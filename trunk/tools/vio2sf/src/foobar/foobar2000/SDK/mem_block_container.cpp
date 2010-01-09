#include "foobar2000.h"

void mem_block_container::from_stream(stream_reader * p_stream,t_size p_bytes,abort_callback & p_abort) {
	if (p_bytes == 0) {set_size(0);}
	set_size(p_bytes);
	p_stream->read_object(get_ptr(),p_bytes,p_abort);
}

void mem_block_container::set(const void * p_buffer,t_size p_size) {
	set_size(p_size);
	memcpy(get_ptr(),p_buffer,p_size);
}
