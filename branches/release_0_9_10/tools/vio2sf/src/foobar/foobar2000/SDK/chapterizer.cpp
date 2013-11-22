#include "foobar2000.h"

void chapter_list::copy(const chapter_list & p_source)
{
	t_size n, count = p_source.get_chapter_count();
	set_chapter_count(count);
	for(n=0;n<count;n++)
		set_info(n,p_source.get_info(n));
}

bool chapterizer::g_find(service_ptr_t<chapterizer> & p_out,const char * p_path,abort_callback & p_abort)
{
	service_ptr_t<chapterizer> ptr;
	service_enum_t<chapterizer> e;
	while(e.next(ptr))
	{
		if (ptr->is_our_file(p_path,p_abort))
		{
			p_out = ptr;
			return true;
		}
	}
	return false;
}