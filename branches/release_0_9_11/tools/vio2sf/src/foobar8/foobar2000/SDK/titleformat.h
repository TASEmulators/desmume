#ifndef _FOOBAR2000_TITLEFORMAT_H_
#define _FOOBAR2000_TITLEFORMAT_H_

#include "service.h"

#include "file_info.h"
#include "config_var.h"

//use this to call titleformatting directly (bypassing database)
//implemented in the core, dont override

class titleformat : public service_base
{
public:
	virtual void run(const file_info * source,string_base & out,const char * spec,const char * extra_items)=0;
	//source - file info to use
	//out - string receiving results
	//spec - formatting specification string to use, e.g. "%ARTIST% - %TITLE%"
	//extra_items - %_name% variables, name=value pairs, null-separated, terminated with two nulls, e.g. "name=value\0name=value\0"
	//extra_items can be null

	//helpers
	static void g_run(const file_info * source,string_base & out,const char * spec,const char * extra_items);

	static void remove_color_marks(const char * src,string8 & out);//helper

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};


#endif //_FOOBAR2000_TITLEFORMAT_H_