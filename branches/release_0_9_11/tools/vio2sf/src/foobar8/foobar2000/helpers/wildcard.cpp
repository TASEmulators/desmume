#include "stdafx.h"

static bool test_recur(const char * fn,const char * rm,bool b_sep)
{
	for(;;)
	{
		if ((b_sep && *rm==';') || *rm==0) return *fn==0;
		else if (*rm=='*')
		{
			rm++;
			do
			{
				if (test_recur(fn,rm,b_sep)) return true;
			} while(utf8_advance(fn));
			return false;
		}
		else if (*fn==0) return false;
		else if (*rm!='?' && char_lower(utf8_get_char(fn))!=char_lower(utf8_get_char(rm))) return false;
		
		fn = utf8_char_next(fn); rm = utf8_char_next(rm);
	}
}

bool wildcard_helper::test_path(const char * path,const char * pattern,bool b_sep) {return test(path + string8::g_scan_filename(path),pattern,b_sep);}

bool wildcard_helper::test(const char * fn,const char * pattern,bool b_sep)
{
	if (!b_sep) return test_recur(fn,pattern,false);
	const char * rm=pattern;
	while(*rm)
	{
		if (test_recur(fn,rm,true)) return true;
		while(*rm && *rm!=';') rm++;
		if (*rm==';')
		{
			while(*rm==';') rm++;
			while(*rm==' ') rm++;
		}
	};

	return false;
}

bool wildcard_helper::has_wildcards(const char * str) {return strchr(str,'*') || strchr(str,'?');}