#include "pfc.h"

#ifdef WIN32
#define STRICT
#include <windows.h>
#include <stdio.h>
#endif


void string_base::add_char(unsigned c)
{
	char temp[8];
	unsigned len = utf8_encode_char(c,temp);
	if (len>0) add_string(temp,len);
}

void string_base::skip_trailing_char(unsigned skip)
{
	const char * str = get_ptr();
	unsigned ptr,trunc;
	bool need_trunc = false;
	for(ptr=0;str[ptr];)
	{
		unsigned c;
		unsigned delta = utf8_decode_char(str+ptr,&c);
		if (delta==0) break;
		if (c==skip)
		{
			need_trunc = true;
			trunc = ptr;
		}
		else
		{
			need_trunc = false;
		}
		ptr += delta;
	}
	if (need_trunc) truncate(trunc);
}

string_print_time::string_print_time(__int64 length)
{
	if (length<0) length=0;
	char * out = buffer;
	int weeks,days,hours,minutes,seconds;
	
	weeks = (int)( ( length / (60*60*24*7) ) );
	days = (int)( ( length / (60*60*24) ) % 7 );
	hours = (int) ( ( length / (60 * 60) ) % 24);
	minutes = (int) ( ( length / (60 ) ) % 60 );
	seconds = (int) ( ( length ) % 60 );

	if (weeks)
	{
		out += sprintf(out,"%uwk ",weeks);
	}
	if (days || weeks)
	{
		out += sprintf(out,"%ud ",days);
	}
	if (hours || days || weeks)
	{
		out += sprintf(out,"%u:%02u:%02u",hours,minutes,seconds);
	}
	else out += sprintf(out,"%u:%02u",minutes,seconds);
}

int strcmp_partial(const char * s1,const char * s2)
{
	while(*s2)
	{
		if (*s1<*s2) return -1;
		else if (*s1>*s2) return 1;
		s1++;
		s2++;
	}
	return 0;
}

void string_base::add_float(double val,unsigned digits)
{
	char temp[64];
	_gcvt(val,digits,temp);
	add_string(temp);
}

void string_base::add_int(signed __int64 val,unsigned base)
{
	char temp[64];
	_i64toa(val,temp,base);
	add_string(temp);
}

void string_base::add_uint(unsigned __int64 val,unsigned base)
{
	char temp[64];
	_ui64toa(val,temp,base);
	add_string(temp);
}

bool is_path_separator(unsigned c)
{
	return c=='\\' || c=='/' || c=='|' || c==':';
}

bool is_path_bad_char(unsigned c)
{
#ifdef WIN32
	return c=='\\' || c=='/' || c=='|' || c==':' || c=='*' || c=='?' || c=='\"' || c=='>' || c=='<';
#else
#error portme
#endif
}


void string_printf::g_run(string_base & out,const char * fmt,va_list list)
{
	out.reset();
	while(*fmt)
	{
		if (*fmt=='%')
		{
			fmt++;
			if (*fmt=='%')
			{
				out.add_char('%');
				fmt++;
			}
			else
			{
				bool force_sign = false;
				if (*fmt=='+')
				{
					force_sign = true;
					fmt++;
				}
				char padchar = (*fmt == '0') ? '0' : ' ';
				int pad = 0;
				while(*fmt>='0' && *fmt<='9')
				{
					pad = pad * 10 + (*fmt - '0');
					fmt++;
				}

				if (*fmt=='s' || *fmt=='S')
				{
					const char * ptr = va_arg(list,const char*);
					int len = strlen(ptr);
					if (pad>len) out.add_chars(padchar,pad-len);
					out.add_string(ptr);
					fmt++;

				}
				else if (*fmt=='i' || *fmt=='I' || *fmt=='d' || *fmt=='D')
				{
					char temp[8*sizeof(int)];
					int val = va_arg(list,int);
					if (force_sign && val>0) out.add_char('+');
					itoa(val,temp,10);
					int len = strlen(temp);
					if (pad>len) out.add_chars(padchar,pad-len);
					out.add_string(temp);
					fmt++;
				}
				else if (*fmt=='u' || *fmt=='U')
				{
					char temp[8*sizeof(int)];
					int val = va_arg(list,int);
					if (force_sign && val>0) out.add_char('+');
					_ultoa(val,temp,10);
					int len = strlen(temp);
					if (pad>len) out.add_chars(padchar,pad-len);
					out.add_string(temp);
					fmt++;
				}
				else if (*fmt=='x' || *fmt=='X')
				{
					char temp[8*sizeof(int)];
					int val = va_arg(list,int);
					if (force_sign && val>0) out.add_char('+');
					_ultoa(val,temp,16);
					if (*fmt=='X')
					{
						char * t = temp;
						while(*t)
						{
							if (*t>='a' && *t<='z')
								*t += 'A' - 'a';
							t++;
						}
					}
					int len = strlen(temp);
					if (pad>len) out.add_chars(padchar,pad-len);
					out.add_string(temp);
					fmt++;
				}
				else if (*fmt=='c' || *fmt=='C')
				{
					out.add_char(va_arg(list,char));
					fmt++;
				}
			}
		}
		else
		{
			out.add_char(*(fmt++));
		}
	}
}

string_printf::string_printf(const char * fmt,...)
{
	va_list list;
	va_start(list,fmt);
	run(fmt,list);
	va_end(list);
}



unsigned strlen_max(const char * ptr,unsigned max)
{
	if (ptr==0) return 0;
	unsigned int n = 0;
	while(ptr[n] && n<max) n++;
	return n;
}

unsigned wcslen_max(const WCHAR * ptr,unsigned max)
{
	if (ptr==0) return 0;
	unsigned int n = 0;
	while(ptr[n] && n<max) n++;
	return n;
}

char * strdup_n(const char * src,unsigned len)
{
	char * ret = (char*)malloc(len+1);
	if (ret)
	{
		memcpy(ret,src,len);
		ret[len]=0;
	}
	return ret;
}

void string8::add_string(const char * ptr,unsigned len)
{
	len = strlen_max(ptr,len);
	makespace(used+len+1);
	data.copy(ptr,len,used);
	used+=len;
	data[used]=0;
}

void string8::set_string(const char * ptr,unsigned len)
{
	len = strlen_max(ptr,len);
	makespace(len+1);
	data.copy(ptr,len);
	used=len;
	data[used]=0;
}




void string8::set_char(unsigned offset,char c)
{
	if (!c) truncate(offset);
	else if (offset<used) data[offset]=c;
}

int string8::find_first(char c,int start)	//return -1 if not found
{
	unsigned n;
	if (start<0) start = 0;
	for(n=start;n<used;n++)
	{
		if (data[n]==c) return n;
	}
	return -1;
}

int string8::find_last(char c,int start)
{
	int n;
	if (start<0) start = used-1;
	for(n=start;n>=0;n--)
	{
		if (data[n]==c) return n;
	}
	return -1;
}

int string8::find_first(const char * str,int start)
{
	unsigned n;
	if (start<0) start=0;
	for(n=start;n<used;n++)
	{
		if (!strcmp_partial(data.get_ptr()+n,str)) return n;
	}
	return -1;
}

int string8::find_last(const char * str,int start)
{
	int n;
	if (start<0) start = used-1;
	for(n=start;n>=0;n--)
	{
		if (!strcmp_partial(data.get_ptr()+n,str)) return n;
	}
	return -1;
}

unsigned string8::g_scan_filename(const char * ptr)
{
	int n;
	int _used = strlen(ptr);
	for(n=_used-1;n>=0;n--)
	{
		if (is_path_separator(ptr[n])) return n+1;
	}
	return 0;
}

unsigned string8::scan_filename()
{
	int n;
	for(n=used-1;n>=0;n--)
	{
		if (is_path_separator(data[n])) return n+1;
	}
	return 0;
}

void string8::fix_filename_chars(char def,char leave)//replace "bad" characters, leave can be used to keep eg. path separators
{
	unsigned n;
	for(n=0;n<used;n++)
		if (data[n]!=leave && is_path_bad_char(data[n])) data[n]=def;
}

void string8::fix_dir_separator(char c)
{
	if (used==0 || data[used-1]!=c) add_char(c);
}

void string8::_xor(char x)//renamed from "xor" to keep weird compilers happy
{
	unsigned n;
	for(n=0;n<used;n++)
		data[n]^=x;
}


//slow
void string8::remove_chars(unsigned first,unsigned count)
{
	if (first>used) first = used;
	if (first+count>used) count = used-first;
	if (count>0)
	{
		unsigned n;
		for(n=first+count;n<=used;n++)
			data[n-count]=data[n];
		used -= count;
		makespace(used+1);
	}
}

//slow
void string8::insert_chars(unsigned first,const char * src, unsigned count)
{
	if (first > used) first = used;

	makespace(used+count+1);
	unsigned n;
	for(n=used;(int)n>=(int)first;n--)
		data[n+count] = data[n];
	for(n=0;n<count;n++)
		data[first+n] = src[n];

	used+=count;
}

void string8::insert_chars(unsigned first,const char * src) {insert_chars(first,src,strlen(src));}

bool string8::truncate_eol(unsigned start)
{
	unsigned n;
	const char * ptr = data + start;
	for(n=start;n<used;n++)
	{
		if (*ptr==10 || *ptr==13)
		{
			truncate(n);
			return true;
		}
		ptr++;
	}
	return false;
}

bool string8::fix_eol(const char * append,unsigned start)
{
	bool rv = truncate_eol(start);
	if (rv) add_string(append);
	return rv;
}

string_filename::string_filename(const char * fn)
{
	fn += string8::g_scan_filename(fn);
	const char * ptr=fn,*dot=0;
	while(*ptr && *ptr!='?')
	{
		if (*ptr=='.') dot=ptr;
		ptr++;
	}

	if (dot && dot>fn) set_string_n(fn,dot-fn);
	else set_string(fn);
}

string_filename_ext::string_filename_ext(const char * fn)
{
	fn += string8::g_scan_filename(fn);
	const char * ptr = fn;
	while(*ptr && *ptr!='?') ptr++;
	set_string_n(fn,ptr-fn);
}

string_extension::string_extension(const char * src)
{
	buffer[0]=0;
	const char * start = src + string8::g_scan_filename(src);
	const char * end = start + strlen(start);
	const char * ptr = end-1;
	while(ptr>start && *ptr!='.')
	{
		if (*ptr=='?') end=ptr;
		ptr--;
	}

	if (ptr>=start && *ptr=='.')
	{
		ptr++;
		unsigned len = end-ptr;
		if (len<tabsize(buffer))
		{
			memcpy(buffer,ptr,len*sizeof(char));
			buffer[len]=0;
		}
	}
}


bool has_path_bad_chars(const char * param)
{
	while(*param)
	{
		if (is_path_bad_char(*param)) return true;
		param++;
	}
	return false;
}


void pfc_float_to_string(char * out,double val,unsigned precision,bool b_sign)
{
	char temp[64];
	if (val<0) {*(out++) = '-'; val = -val;}
	else if (b_sign) {*(out++) = '+';}
	_i64toa((__int64)(val * pow(10,precision)),temp,10);
	unsigned len = strlen(temp);
	if (len <= precision)
	{
		*(out++) = '0';
		*(out++) = '.';
		unsigned d;
		for(d=precision-len;d;d--) *(out++) = '0';
		for(d=0;d<len;d++) *(out++) = temp[d];
	}
	else
	{
		unsigned d = len;
		const char * src = temp;
		while(*src)
		{
			if (d-- == precision) *(out++) = '.';
			*(out++) = *(src++);
		}
	}
	*out = 0;
}

double pfc_string_to_float(const char * src)
{
	bool neg = false;
	__int64 val = 0;
	int div = 0;
	bool got_dot = false;

	while(*src==' ') src++;

	if (*src=='-') {neg = true;src++;}
	else if (*src=='+') src++;
	
	while(*src)
	{
		if (*src>='0' && *src<='9')
		{
			int d = *src - '0';
			val = val * 10 + d;
			if (got_dot) div--;
			src++;
		}
		else if (*src=='.' || *src==',')
		{
			if (got_dot) break;
			got_dot = true;
			src++;
		}
		else if (*src=='E' || *src=='e')
		{
			src++;
			div += atoi(src);
			break;
		}
		else break;
	}
	if (neg) val = -val;
	return (double) val * pow(10.0,(double)div);
}

bool string8::limit_length(unsigned length_in_chars,const char * append)
{
	bool rv = false;
	const char * base = get_ptr(), * ptr = base;
	while(length_in_chars && utf8_advance(ptr)) length_in_chars--;
	if (length_in_chars==0)
	{
		truncate(ptr-base);
		add_string(append);
		rv = true;
	}
	return rv;
}

void string_base::convert_to_lower_ascii(const char * src,char replace)
{
	reset();
	assert(replace>0);
	while(*src)
	{
		unsigned c;
		unsigned delta = utf8_decode_char(src,&c);
		if (delta==0) {c = replace; delta = 1;}
		else if (c>=0x80) c = replace;
		add_byte((char)c);
		src += delta;
	}
}

void convert_to_lower_ascii(const char * src,unsigned max,char * out,char replace)
{
	unsigned ptr = 0;
	assert(replace>0);
	while(src[ptr] && ptr<max)
	{
		unsigned c;
		unsigned delta = utf8_decode_char(src+ptr,&c,max-ptr);
		if (delta==0) {c = replace; delta = 1;}
		else if (c>=0x80) c = replace;
		*(out++) = (char)c;
		ptr += delta;
	}
	*out = 0;
}

string_list_nulldelimited::string_list_nulldelimited()
{
	reset();
}

void string_list_nulldelimited::add_string(const char * src)
{
	unsigned src_len = strlen(src) + 1;
	data.copy(src,src_len,len);
	len += src_len;
	data.copy("",1,len);
}

void string_list_nulldelimited::add_string_multi(const char * src)
{
	unsigned src_len = 0;
	while(src[src_len])
	{
		src_len += strlen(src+src_len) + 1;
	}
	data.copy(src,src_len,len);
	len += src_len;
	data.copy("",1,len);
}


void string_list_nulldelimited::reset()
{
	len = 0;
	data.copy("\0",2,len);
}