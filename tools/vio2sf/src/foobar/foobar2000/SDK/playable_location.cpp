#include "foobar2000.h"

int playable_location::g_compare(const playable_location & p_item1,const playable_location & p_item2) {
	int ret = metadb::path_compare(p_item1.get_path(),p_item2.get_path());
	if (ret != 0) return ret;
	return pfc::compare_t(p_item1.get_subsong(),p_item2.get_subsong());
}

pfc::string_base & operator<<(pfc::string_base & p_fmt,const playable_location & p_location)
{
	p_fmt << "\"" << file_path_display(p_location.get_path()) << "\"";
	t_uint32 index = p_location.get_subsong_index();
	if (index != 0) p_fmt << " / index: " << p_location.get_subsong_index();
	return p_fmt;
}


bool playable_location::operator==(const playable_location & p_other) const {
	return metadb::path_compare(get_path(),p_other.get_path()) == 0 && get_subsong() == p_other.get_subsong();
}
bool playable_location::operator!=(const playable_location & p_other) const {
	return !(*this == p_other);
}
