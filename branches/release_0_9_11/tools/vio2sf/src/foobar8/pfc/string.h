#ifndef _PFC_STRING_H_
#define _PFC_STRING_H_

#ifndef WCHAR
typedef unsigned short WCHAR;
#endif

bool is_path_separator(unsigned c);
bool is_path_bad_char(unsigned c);
bool is_valid_utf8(const char * param);
bool is_lower_ascii(const char * param);
bool has_path_bad_chars(const char * param);
void recover_invalid_utf8(const char * src,char * out,unsigned replace);//out must be enough to hold strlen(char) + 1, or appropiately bigger if replace needs multiple chars
void convert_to_lower_ascii(const char * src,unsigned max,char * out,char replace = '?');//out should be at least strlen(src)+1 long

class NOVTABLE string_base	//cross-dll safe string for returing strings from api functions etc
{//all char* are UTF-8 unless local comments state otherwise
protected:
	string_base() {}
	~string_base() {}
public:
	virtual const char * get_ptr() const = 0;
	virtual void add_string(const char * ptr,unsigned len=-1)=0;//stops adding new chars if encounters null before len
	virtual void set_string(const char * ptr,unsigned len=-1) {reset();add_string(ptr,len);}
	virtual void truncate(unsigned len)=0;
	virtual unsigned length() const {return strlen(get_ptr());}

	inline operator const char * () const {return get_ptr();}

	
	inline void reset() {truncate(0);}
	
	//for compatibility
	inline void add_string_n(const char * ptr,unsigned len) {add_string(ptr,len);}	
	inline void set_string_n(const char * ptr,unsigned len) {set_string(ptr,len);}

	inline void add_byte(char c) {add_string_n(&c,1);}
	inline bool is_empty() const {return *get_ptr()==0;}
	
	void add_char(unsigned c);//adds unicode char to the string
	void skip_trailing_char(unsigned c = ' ');

	inline void add_chars(unsigned c,unsigned count) {for(;count;count--) add_char(c);}

	
	void add_int(signed __int64 val,unsigned base = 10);
	void add_uint(unsigned __int64 val,unsigned base = 10);
	void add_float(double val,unsigned digits);

/*	void add_string_lower(const char * src,unsigned len = -1);
	void add_string_upper(const char * src,unsigned len = -1);
	inline void set_string_lower(const char * src,unsigned len = -1) {reset();add_string_lower(src,len);}
	inline void set_string_upper(const char * src,unsigned len = -1) {reset();add_string_upper(src,len);}

	void to_lower();
	void to_upper();
*/
	void add_string_ansi(const char * src,unsigned len = -1); //converts from ansi to utf8
	void set_string_ansi(const char * src,unsigned len = -1) {reset();add_string_ansi(src,len);}
	void add_string_utf16(const WCHAR * src,unsigned len = -1);//converts from utf16 (widechar) to utf8
	void set_string_utf16(const WCHAR * src,unsigned len = -1) {reset();add_string_utf16(src,len);}

	bool is_valid_utf8() {return ::is_valid_utf8(get_ptr());}

	void convert_to_lower_ascii(const char * src,char replace = '?');



	inline const char * operator = (const char * src) {set_string(src);return get_ptr();}
	inline const char * operator += (const char * src) {add_string(src);return get_ptr();}
	inline const char * operator = (const string_base & src) {set_string(src);return get_ptr();}
};

class string8 : public string_base
{
protected:
	mem_block_t<char> data;
	unsigned used;

	inline void makespace(unsigned s)
	{
		unsigned old_size = data.get_size();
		if (old_size < s)
			data.set_size(s+16);
		else if (old_size > s + 32)
			data.set_size(s);
	}

public:
	inline const char * operator = (const char * src) {set_string(src);return get_ptr();}
	inline const char * operator += (const char * src) {add_string(src);return get_ptr();}
	inline operator const char * () const {return get_ptr();}
	inline const char * operator = (const string8 & src) {set_string(src);return get_ptr();}

	string8() {used=0;}
	string8(const char * src) {used=0;set_string(src);}
	string8(const char * src,unsigned num) {used=0;set_string_n(src,num);}
	string8(const string8 & src) {used=0;set_string(src);}

	void set_mem_logic(mem_block::mem_logic_t v) {data.set_mem_logic(v);}
	int get_mem_logic() const {return data.get_mem_logic();}
	void prealloc(unsigned s) {s++;if (s>used) makespace(s);}

	const char * get_ptr() const
	{
		return used > 0 ? data.get_ptr() : "";
	}

	virtual void add_string(const char * ptr,unsigned len = -1);
	virtual void set_string(const char * ptr,unsigned len = -1);

	virtual void truncate(unsigned len)
	{
		if (used>len) {used=len;data[len]=0;makespace(used+1);}
	}

	virtual unsigned length() const {return used;}

//	~string8() {}

	void set_char(unsigned offset,char c);
	int find_first(char c,int start=0);	//return -1 if not found
	int find_last(char c,int start = -1);
	int find_first(const char * str,int start = 0);
	int find_last(const char * str,int start = -1);
//	unsigned replace_string(const char * s1,unsigned len1,const char * s2,unsigned len2,bool casesens = true,unsigned start = 0);//len1/len2 can be -1
//	unsigned replace_byte(char c1,char c2,unsigned start = 0);
	unsigned replace_char(unsigned c1,unsigned c2,unsigned start = 0);
//	inline unsigned replace_byte_from(unsigned start,char c1,char c2) {return replace_byte(c1,c2,start);}
//	inline unsigned replace_char_from(unsigned start,unsigned c1,unsigned c2) {return replace_char(c1,c2,start);}
	static unsigned g_scan_filename(const char * ptr);
	unsigned scan_filename();
	void fix_filename_chars(char def = '_',char leave=0);//replace "bad" characters, leave can be used to keep eg. path separators
	void fix_dir_separator(char c);
	void _xor(char x);//renamed from "xor" to keep weird compilers happy	
	void remove_chars(unsigned first,unsigned count); //slow
	void insert_chars(unsigned first,const char * src, unsigned count);//slow
	void insert_chars(unsigned first,const char * src);
	bool truncate_eol(unsigned start = 0);
	bool fix_eol(const char * append = " (...)",unsigned start = 0);
	bool limit_length(unsigned length_in_chars,const char * append = " (...)");

	//for string_buffer class
	inline char * buffer_get(unsigned n)
	{
		makespace(n+1);
		data.zeromemory();
		return data;
	}

	inline void buffer_done()
	{
		used=strlen(data);
		makespace(used+1);
	}

	inline void force_reset() {used=0;data.force_reset();}

};

class string8_fastalloc : public string8
{
public:
	explicit string8_fastalloc(unsigned p_prealloc = 0) {set_mem_logic(mem_block::ALLOC_FAST_DONTGODOWN);if (p_prealloc) prealloc(p_prealloc);}
	inline const char * operator=(const char * src) {set_string(src);return get_ptr();}
	inline const char * operator+=(const char * src) {add_string(src);return get_ptr();}
};

class string_buffer
{
private:
	string8 * owner;
	char * data;
public:
	explicit string_buffer(string8 & s,unsigned siz) {owner=&s;data=s.buffer_get(siz);}
	~string_buffer() {owner->buffer_done();}
	operator char* () {return data;}
};

class string_printf : public string8_fastalloc
{
public:
	static void g_run(string_base & out,const char * fmt,va_list list);
	inline void run(const char * fmt,va_list list) {g_run(*this,fmt,list);}

	explicit string_printf(const char * fmt,...);
};

class string_printf_va : public string8_fastalloc
{
public:
	explicit string_printf_va(const char * fmt,va_list list) {string_printf::g_run(*this,fmt,list);}
};


class string_print_time
{
protected:
	char buffer[128];
public:
	string_print_time(__int64 s);
	operator const char * () const {return buffer;}
};


class string_print_time_ex : private string_print_time
{
public:
	string_print_time_ex(double s,unsigned extra = 3) : string_print_time((__int64)s)
	{
		if (extra>0)
		{
			unsigned mul = 1, n;
			for(n=0;n<extra;n++) mul *= 10;

			
			unsigned val = (unsigned)((__int64)(s*mul) % mul);
			char fmt[16];
			sprintf(fmt,".%%0%uu",extra);
			sprintf(buffer + strlen(buffer),fmt,val);
		}
	}
	operator const char * () const {return buffer;}
};

unsigned strlen_max(const char * ptr,unsigned max);
unsigned wcslen_max(const WCHAR * ptr,unsigned max);

unsigned strlen_utf8(const char * s,unsigned num=-1);//returns number of characters in utf8 string; num - no. of bytes (optional)
unsigned utf8_char_len(const char * s);//returns size of utf8 character pointed by s, in bytes, 0 on error
unsigned utf8_chars_to_bytes(const char * string,unsigned count);

unsigned strcpy_utf8_truncate(const char * src,char * out,unsigned maxbytes);

unsigned utf8_decode_char(const char * src,unsigned * out,unsigned src_bytes = -1);//returns length in bytes
unsigned utf8_encode_char(unsigned c,char * out);//returns used length in bytes, max 6
unsigned utf16_decode_char(const WCHAR * src,unsigned * out);
unsigned utf16_encode_char(unsigned c,WCHAR * out);

inline unsigned estimate_utf8_to_utf16(const char * src,unsigned len) {return len + 1;}//estimates amount of output buffer space that will be sufficient for conversion, including null
inline unsigned estimate_utf8_to_utf16(const char * src) {return estimate_utf8_to_utf16(src,strlen(src));}
inline unsigned estimate_utf16_to_utf8(const WCHAR * src,unsigned len) {return len * 3 + 1;}
inline unsigned estimate_utf16_to_utf8(const WCHAR * src) {return estimate_utf16_to_utf8(src,wcslen(src));}

inline unsigned estimate_utf16_to_ansi(const WCHAR * src,unsigned len) {return len * 2 + 1;}
inline unsigned estimate_utf16_to_ansi(const WCHAR * src) {return estimate_utf16_to_ansi(src,wcslen(src));}
inline unsigned estimate_ansi_to_utf16(const char * src,unsigned len) {return len + 1;}
inline unsigned estimate_ansi_to_utf16(const char * src) {return estimate_ansi_to_utf16(src,strlen(src));}

inline unsigned estimate_utf8_to_ansi(const char * src,unsigned len) {return len + 1;}
inline unsigned estimate_utf8_to_ansi(const char * src) {return estimate_utf8_to_ansi(src,strlen(src));}
inline unsigned estimate_ansi_to_utf8(const char * src,unsigned len) {return len * 3 + 1;}
inline unsigned estimate_ansi_to_utf8(const char * src) {return estimate_ansi_to_utf8(src,strlen(src));}

unsigned convert_utf8_to_utf16(const char * src,WCHAR * dst,unsigned len = -1);//len - amount of bytes/wchars to convert (will not go past null terminator)
unsigned convert_utf16_to_utf8(const WCHAR * src,char * dst,unsigned len = -1);
unsigned convert_utf8_to_ansi(const char * src,char * dst,unsigned len = -1);
unsigned convert_ansi_to_utf8(const char * src,char * dst,unsigned len = -1);
unsigned convert_ansi_to_utf16(const char * src,WCHAR * dst,unsigned len = -1);
unsigned convert_utf16_to_ansi(const WCHAR * src,char * dst,unsigned len = -1);

template<class T>
class string_convert_base
{
protected:
	T * ptr;
	inline void alloc(unsigned size) {ptr = mem_ops<T>::alloc(size);}
	inline ~string_convert_base() {mem_ops<T>::free(ptr);}
public:
	inline operator const T * () const {return ptr;}
	inline const T * get_ptr() const {return ptr;}
	
	inline unsigned length()
	{
		unsigned ret = 0;
		const T* p = ptr;
		while(*p) {ret++;p++;}
		return ret;
	}
};

class string_utf8_from_utf16 : public string_convert_base<char>
{
public:
	explicit string_utf8_from_utf16(const WCHAR * src,unsigned len = -1)
	{
		len = wcslen_max(src,len);
		alloc(estimate_utf16_to_utf8(src,len));
		convert_utf16_to_utf8(src,ptr,len);
	}
};

class string_utf16_from_utf8 : public string_convert_base<WCHAR>
{
public:
	explicit string_utf16_from_utf8(const char * src,unsigned len = -1)
	{
		len = strlen_max(src,len);
		alloc(estimate_utf8_to_utf16(src,len));
		convert_utf8_to_utf16(src,ptr,len);
	}
};

class string_ansi_from_utf16 : public string_convert_base<char>
{
public:
	explicit string_ansi_from_utf16(const WCHAR * src,unsigned len = -1)
	{
		len = wcslen_max(src,len);
		alloc(estimate_utf16_to_ansi(src,len));
		convert_utf16_to_ansi(src,ptr,len);
	}
};

class string_utf16_from_ansi : public string_convert_base<WCHAR>
{
public:
	explicit string_utf16_from_ansi(const char * src,unsigned len = -1)
	{
		len = strlen_max(src,len);
		alloc(estimate_ansi_to_utf16(src,len));
		convert_ansi_to_utf16(src,ptr,len);
	}
};

class string_utf8_from_ansi : public string_convert_base<char>
{
public:
	explicit string_utf8_from_ansi(const char * src,unsigned len = -1)
	{
		len = strlen_max(src,len);
		alloc(estimate_ansi_to_utf8(src,len));
		convert_ansi_to_utf8(src,ptr,len);
	}
};

class string_ansi_from_utf8 : public string_convert_base<char>
{
public:
	explicit string_ansi_from_utf8(const char * src,unsigned len = -1)
	{
		len = strlen_max(src,len);
		alloc(estimate_utf8_to_ansi(src,len));
		convert_utf8_to_ansi(src,ptr,len);
	}
};

class string_ascii_from_utf8 : public string_convert_base<char>
{
public:
	explicit string_ascii_from_utf8(const char * src,unsigned len = -1)
	{
		len = strlen_max(src,len);
		alloc(len+1);
		convert_to_lower_ascii(src,len,ptr,'?');
	}
};

#define string_wide_from_utf8 string_utf16_from_utf8
#define string_utf8_from_wide string_utf8_from_utf16
#define string_wide_from_ansi string_utf16_from_ansi
#define string_ansi_from_wide string_ansi_from_utf16

#ifdef UNICODE
#define string_os_from_utf8 string_wide_from_utf8
#define string_utf8_from_os string_utf8_from_wide
#define _TX(X) L##X
#define estimate_utf8_to_os estimate_utf8_to_utf16
#define estimate_os_to_utf8 estimate_utf16_to_utf8
#define convert_utf8_to_os convert_utf8_to_utf16
#define convert_os_to_utf8 convert_utf16_to_utf8
#define add_string_os add_string_utf16
#define set_string_os set_string_utf16
#else
#define string_os_from_utf8 string_ansi_from_utf8
#define string_utf8_from_os string_utf8_from_ansi
#define _TX(X) X
#define estimate_utf8_to_os estimate_utf8_to_ansi
#define estimate_os_to_utf8 estimate_ansi_to_utf8
#define convert_utf8_to_os convert_utf8_to_ansi
#define convert_os_to_utf8 convert_ansi_to_utf8
#define add_string_os add_string_ansi
#define set_string_os set_string_ansi
#endif


int strcmp_partial(const char * s1,const char * s2);
int skip_utf8_chars(const char * ptr,int count);
char * strdup_n(const char * src,unsigned len);

unsigned utf8_get_char(const char * src);

inline bool utf8_advance(const char * & var)
{
	UINT delta = utf8_char_len(var);
	var += delta;
	return delta>0;
}

inline bool utf8_advance(char * & var)
{
	UINT delta = utf8_char_len(var);
	var += delta;
	return delta>0;
}

inline const char * utf8_char_next(const char * src) {return src + utf8_char_len(src);}
inline char * utf8_char_next(char * src) {return src + utf8_char_len(src);}


#define filename_cmp(s1,s2) stricmp_utf8(s1,s2)	//deprecated

template<class T>
class string_simple_t//simplified string class, less efficient but smaller; could make it derive from string_base but it wouldn't be so light anymore (vtable)
{
private:
	T * ptr;

	void do_realloc(unsigned size)
	{
		ptr = mem_ops<T>::realloc(ptr,size);
	}

	static unsigned t_strlen(const T* src,unsigned max = -1)
	{
		unsigned ret;
		for(ret=0;src[ret] && ret<max;ret++);
		return ret;
	}

	static void t_strcpy(T* dst,const T* src,unsigned max=-1)
	{
		unsigned ptr;
		for(ptr=0;src[ptr] && ptr<max;ptr++)
			dst[ptr]=src[ptr];
		dst[ptr]=0;
	}

	static T* t_strdup(const T* src,unsigned max=-1)
	{
		T* ret = mem_ops<T>::alloc(t_strlen(src,max)+1);
		if (ret) t_strcpy(ret,src,max);
		return ret;
	}

	inline static void t_memcpy(T* dst,const T* src,unsigned len) {mem_ops<T>::copy(dst,src,len);}

public:
	inline const T * get_ptr() const {return ptr ? ptr : reinterpret_cast<const T*>("\0\0\0\0");}
	inline operator const T * () const {return get_ptr();}
	inline string_simple_t(const T * param) {ptr = t_strdup(param);}
	inline string_simple_t(const T * param,unsigned len) {ptr = t_strdup(param,len);}
	inline string_simple_t() {ptr=0;}
	inline string_simple_t(__int64 val)
	{
		T temp[64];
		if (sizeof(T)==1)
		{
			_i64toa(val,reinterpret_cast<char*>(&temp),10);
		}
		else if (sizeof(T)==2)
		{
			_i64tow(val,reinterpret_cast<WCHAR*>(&temp),10);
		}
		else
		{
			ASSUME(0);
		}
		ptr = t_strdup(temp);
	}
	inline ~string_simple_t() {if (ptr) mem_ops<T>::free(ptr);}
	inline const T * operator = (const T * param) {set_string(param);return get_ptr();}
	inline const T * operator = (const string_simple_t<T> & param) {set_string(param);return get_ptr();}
	inline const T * operator += (const T * src) {add_string(src);return get_ptr();}

	inline string_simple(const string_simple_t<T> & param) {ptr = t_strdup(param);}
	inline void reset() {if (ptr) {mem_ops<T>::free(ptr);ptr=0;}}
	inline bool is_empty() {return !ptr || !*ptr;}
	inline unsigned length() {return t_strlen(get_ptr());}

	void add_string(const T * param,unsigned len = -1)
	{
		len = t_strlen(param,len);
		if (len>0)
		{
			unsigned old_len = length();
			do_realloc(old_len + len + 1);
			t_memcpy(ptr+old_len,param,len);
			ptr[old_len+len]=0;
		}
	}
	
	void set_string(const T * param,unsigned len = -1)
	{
		len = t_strlen(param,len);
		if (len>0)
		{
			do_realloc(len + 1);//will malloc if old ptr was null
			t_memcpy(ptr,param,len);
			ptr[len]=0;
		}
		else reset();
	}

	//for compatibility
	inline void set_string_n(const T * param,unsigned len) {set_string(param,len);}
	inline void add_string_n(const T * param,unsigned len) {add_string(param,len);}

	void truncate(unsigned len)
	{
		if (len<length())
		{
			do_realloc(len+1);
			ptr[len]=0;
		}
	}
};

#define string_simple string_simple_t<char>
#define w_string_simple string_simple_t<WCHAR>
#define t_string_simple string_simple_t<TCHAR>

class string_filename : public string_simple
{
public:
	explicit string_filename(const char * fn);
};

class string_filename_ext : public string_simple
{
public:
	explicit string_filename_ext(const char * fn);
};

class string_extension
{
	char buffer[32];
public:
	inline const char * get_ptr() const {return buffer;}
	inline unsigned length() const {return strlen(buffer);}
	inline operator const char * () const {return buffer;}
	explicit string_extension(const char * src);
};

#define string_extension_8 string_extension

template<class T>
class string_buffer_t
{
private:
	string_simple_t<T> & owner;
	mem_block_t<T> data;
	unsigned size;
public:
	string_buffer_t(string_simple_t<T> & p_owner,unsigned p_size) : owner(p_owner), data(p_size+1), size(size) {data.zeromemory();}
	operator T* () {return data.get_ptr();}
	~string_buffer_t() {owner.set_string(data,size);}
};

#define a_string_buffer string_buffer_t<char>
#define t_string_buffer string_buffer_t<TCHAR>
#define w_string_buffer string_buffer_t<WCHAR>

class make_string_n	//trick for passing truncated char* to api that takes null-terminated strings, needs to temporarily modify string data
{
	char * ptr, *ptr0, old;
public:
	inline explicit make_string_n(char * src,unsigned len)
	{
		ptr = src; ptr0 = src+len;
		old = *ptr0;
		*ptr0 = 0;
	}
	inline ~make_string_n() {*ptr0 = old;}
	inline const char * get_ptr() const {return ptr;}
	inline operator const char * () const {return ptr;}	
};

void pfc_float_to_string(char * out,double val,unsigned precision,bool force_sign = false);//doesnt add E+X etc, has internal range limits, useful for storing float numbers as strings without having to bother with international coma/dot settings BS
double pfc_string_to_float(const char * src);

class string_list_nulldelimited
{
	mem_block_fastalloc<char> data;
	unsigned len;
public:
	string_list_nulldelimited();
	inline const char * get_ptr() const {return data;}
	inline operator const char * () const {return data;}
	void add_string(const char *);
	void add_string_multi(const char *);
	void reset();
};

#endif //_PFC_STRING_H_