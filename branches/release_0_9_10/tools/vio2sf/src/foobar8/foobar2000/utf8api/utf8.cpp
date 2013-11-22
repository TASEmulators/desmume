#include "utf8api.h"
#include <locale.h>

extern "C" {

static unsigned q_tolower(unsigned c)
{
	if (c>='A' && c<='Z') c += 'a' - 'A';
	return c;
}


#ifdef WIN32
static UINT char_convert_win9x(UINT param,bool b_upper)
{
	assert(param && param<0x10000);
	char temp[16];
	WCHAR temp_w[16];
	temp_w[0] = (WCHAR)param;
	temp_w[1] = 0;
	if (WideCharToMultiByte(CP_ACP,0,temp_w,-1,temp,16,0,0)==0) return param;
	temp[15] = 0;
	if (b_upper) CharUpperA(temp); else CharLowerA(temp);
	if (MultiByteToWideChar(CP_ACP,0,temp,-1,temp_w,16)==0) return param;
	if (temp_w[0]==0 || temp_w[1]!=0) return param;
	return temp_w[0];
}

#endif

UTF8API_EXPORT UINT uCharLower(UINT param)
{
	if (param<128)
	{
		if (param>='A' && param<='Z') param += 'a' - 'A';
		return param;
	}
#ifdef WIN32
	else if (param<0x10000)
	{
		unsigned ret;
#ifdef UNICODE
		ret = (unsigned)CharLowerW((WCHAR*)param);
#else
		ret = char_convert_win9x(param,false);
#endif
		return ret;
	}
	else return param;
#else
	else
	{
		setlocale(LC_CTYPE,"");
		return towlower(param);
	}
#endif
}

UTF8API_EXPORT UINT uCharUpper(UINT param)
{
	if (param<128)
	{
		if (param>='a' && param<='z') param += 'A' - 'a';
		return param;
	}
#ifdef WIN32
	else if (param<0x10000)
	{
		unsigned ret;
#ifdef UNICODE
		ret = (unsigned)CharUpperW((WCHAR*)param);
#else
		ret = char_convert_win9x(param,true);
#endif
		return ret;
	}
	else return param;
#else
	else
	{
		setlocale(LC_CTYPE,"");
		return towupper(param);
	}
#endif
}

static inline int compare_wchar(unsigned c1,unsigned c2)
{
	if (c1==c2) return 0;
	c1 = char_lower(c1);
	c2 = char_lower(c2);
	if (c1<c2) return -1;
	else if (c1>c2) return 1;
	else return 0;
}


UTF8API_EXPORT int stricmp_utf8(const char * p1,const char * p2)
{
	for(;;)
	{
		if (*p1==0 && *p2==0) return 0;
		else if (*p1>0 && *p2>0)//signed char
		{
			unsigned c1 = q_tolower(*p1), c2 = q_tolower(*p2);
			if (c1<c2) return -1;
			else if (c1>c2) return 1;
			else
			{
				p1++;
				p2++;
			}
		}
		else
		{
			unsigned int w1,w2,d1,d2;
			d1 = utf8_decode_char(p1,&w1);
			d2 = utf8_decode_char(p2,&w2);
			if (w1==0 && w2==0) return 0;
			int rv = compare_wchar(w1,w2);
			if (rv) return rv;
			p1 += d1;
			p2 += d2;
		}
	}
}

UTF8API_EXPORT int stricmp_utf8_stringtoblock(const char * p1,const char * p2,unsigned p2_bytes)
{
	return stricmp_utf8_ex(p1,-1,p2,p2_bytes);
}

UTF8API_EXPORT int stricmp_utf8_partial(const char * p1,const char * p2,unsigned num)
{
	for(;num;)
	{
		unsigned int w1,w2,d1,d2;
		d1 = utf8_decode_char(p1,&w1);
		d2 = utf8_decode_char(p2,&w2);
		if (w2==0 || d2==0) return 0;
		int rv = compare_wchar(w1,w2);
		if (rv) return rv;
		p1 += d1;
		p2 += d2;
		num--;
	}
	return 0;
}

UTF8API_EXPORT int stricmp_utf8_max(const char * p1,const char * p2,unsigned p1_bytes)
{
	return stricmp_utf8_ex(p1,p1_bytes,p2,-1);
}

namespace {
	typedef bool (*t_replace_test)(const char * src,const char * test,unsigned len);

	static bool replace_test_i(const char * src,const char * test,unsigned len)
	{
		return stricmp_utf8_max(src,test,len)==0;
	}

	static bool replace_test(const char * src,const char * test,unsigned len)
	{
		unsigned ptr;
		bool rv = true;
		for(ptr=0;ptr<len;ptr++)
		{
			if (src[ptr]!=test[ptr]) {rv = false; break;}
		}
		return rv;
	}
}

UTF8API_EXPORT unsigned uReplaceStringAdd(string_base & out,const char * src,unsigned src_len,const char * s1,unsigned len1,const char * s2,unsigned len2,bool casesens)
{
	t_replace_test testfunc = casesens ? replace_test : replace_test_i;

	len1 = strlen_max(s1,len1), len2 = strlen_max(s2,len2);

	unsigned len = strlen_max(src,src_len);
	
	unsigned count = 0;

	if (len1>0)
	{
		unsigned ptr = 0;
		while(ptr+len1<=len)
		{
			if (testfunc(src+ptr,s1,len1))
			{
				count++;
				out.add_string(s2,len2);
				ptr += len1;
			}
			else out.add_byte(src[ptr++]);
		}
		if (ptr<len) out.add_string(src+ptr,len-ptr);
	}
	return count;
}

UTF8API_EXPORT unsigned uReplaceCharAdd(string_base & out,const char * src,unsigned src_len,unsigned c1,unsigned c2,bool casesens)
{
	assert(c1>0);
	assert(c2>0);
	char s1[8],s2[8];
	unsigned len1,len2;
	len1 = utf8_encode_char(c1,s1);
	len2 = utf8_encode_char(c2,s2);
	return uReplaceString(out,src,src_len,s1,len1,s2,len2,casesens);
}


UTF8API_EXPORT void uAddStringLower(string_base & out,const char * src,unsigned len)
{
	while(*src && len)
	{
		unsigned int c,d;
		d = utf8_decode_char(src,&c);
		if (d==0 || d>len) break;
		out.add_char(char_lower(c));
		src+=d;
		len-=d;
	}
}

UTF8API_EXPORT void uAddStringUpper(string_base & out,const char * src,unsigned len)
{
	while(*src && len)
	{
		unsigned int c,d;
		d = utf8_decode_char(src,&c);
		if (d==0 || d>len) break;
		out.add_char(char_upper(c));
		src+=d;
		len-=d;
	}
}

UTF8API_EXPORT int stricmp_utf8_ex(const char * p1,unsigned p1_bytes,const char * p2,unsigned p2_bytes)
{
	p1_bytes = strlen_max(p1,p1_bytes);
	p2_bytes = strlen_max(p2,p2_bytes);
	for(;;)
	{
		if (p1_bytes == 0 && p2_bytes == 0) return 0;
		else if (p1_bytes == 0) return 1;
		else if (p2_bytes == 0) return -1;
		else if (*p1>0 && *p2>0)//signed char
		{
			unsigned c1 = q_tolower(*p1), c2 = q_tolower(*p2);
			if (c1<c2) return -1;
			else if (c1>c2) return 1;
			else
			{
				p1++;
				p2++;
				p1_bytes--;
				p2_bytes--;				
			}
		}
		else
		{
			unsigned w1,w2,d1,d2;
			d1 = utf8_decode_char(p1,&w1,p1_bytes);
			d2 = utf8_decode_char(p2,&w2,p2_bytes);
			if (d1==0) return -1;
			if (d2==0) return 1;
			int rv = compare_wchar(w1,w2);
			if (rv) return rv;
			p1 += d1;
			p2 += d2;
			p1_bytes -= d1;
			p2_bytes -= d2;
		}
	}
}

}
