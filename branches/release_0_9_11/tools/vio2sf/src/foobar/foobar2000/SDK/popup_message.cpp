#include "foobar2000.h"

void popup_message::g_show_ex(const char * p_msg,unsigned p_msg_length,const char * p_title,unsigned p_title_length,t_icon p_icon)
{
	static_api_ptr_t<popup_message>()->show_ex(p_msg,p_msg_length,p_title,p_title_length,p_icon);
}
