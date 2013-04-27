#include "foobar2000.h"

GUID hasher_md5::guid_from_result(const hasher_md5_result & param)
{
	assert(sizeof(GUID) == sizeof(hasher_md5_result));
	GUID temp = * reinterpret_cast<const GUID*>(&param);
	byte_order::order_le_to_native_t(temp);
	return temp;
}

hasher_md5_result hasher_md5::process_single(const void * p_buffer,t_size p_bytes)
{
	hasher_md5_state state;
	initialize(state);
	process(state,p_buffer,p_bytes);
	return get_result(state);
}

GUID hasher_md5::process_single_guid(const void * p_buffer,t_size p_bytes)
{
	return guid_from_result(process_single(p_buffer,p_bytes));
}