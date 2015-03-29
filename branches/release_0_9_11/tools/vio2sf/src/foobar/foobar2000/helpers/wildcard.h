#ifndef __FOOBAR2000_HELPER_WILDCARD_H__
#define __FOOBAR2000_HELPER_WILDCARD_H__

namespace wildcard_helper
{
	bool test_path(const char * path,const char * pattern,bool b_separate_by_semicolon = false);//will extract filename from path first
	bool test(const char * str,const char * pattern,bool b_separate_by_semicolon = false);//tests if str matches pattern
	bool has_wildcards(const char * str);
};



#endif //__FOOBAR2000_HELPER_WILDCARD_H__