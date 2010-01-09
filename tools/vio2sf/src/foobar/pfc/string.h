#ifndef _PFC_STRING_H_
#define _PFC_STRING_H_

namespace pfc {

	class NOVTABLE string_receiver {
	public:
		virtual void add_string(const char * p_string,t_size p_string_size = infinite) = 0;

		void add_char(t_uint32 c);//adds unicode char to the string
		void add_byte(char c) {add_string(&c,1);}
		void add_chars(t_uint32 p_char,t_size p_count) {for(;p_count;p_count--) add_char(p_char);}
	protected:
		string_receiver() {}
		~string_receiver() {}
	};

	t_size scan_filename(const char * ptr);

	bool is_path_separator(unsigned c);
	bool is_path_bad_char(unsigned c);
	bool is_valid_utf8(const char * param,t_size max = infinite);
	bool is_lower_ascii(const char * param);
	bool is_multiline(const char * p_string,t_size p_len = infinite);
	bool has_path_bad_chars(const char * param);
	void recover_invalid_utf8(const char * src,char * out,unsigned replace);//out must be enough to hold strlen(char) + 1, or appropiately bigger if replace needs multiple chars
	void convert_to_lower_ascii(const char * src,t_size max,char * out,char replace = '?');//out should be at least strlen(src)+1 long

	inline char ascii_tolower(char c) {if (c >= 'A' && c <= 'Z') c += 'a' - 'A'; return c;}
	inline char ascii_toupper(char c) {if (c >= 'a' && c <= 'z') c += 'A' - 'a'; return c;}

	t_size string_find_first(const char * p_string,char p_tofind,t_size p_start = 0);	//returns infinite if not found
	t_size string_find_last(const char * p_string,char p_tofind,t_size p_start = ~0);	//returns infinite if not found
	t_size string_find_first(const char * p_string,const char * p_tofind,t_size p_start = 0);	//returns infinite if not found
	t_size string_find_last(const char * p_string,const char * p_tofind,t_size p_start = ~0);	//returns infinite if not found

	t_size string_find_first_ex(const char * p_string,t_size p_string_length,char p_tofind,t_size p_start = 0);	//returns infinite if not found
	t_size string_find_last_ex(const char * p_string,t_size p_string_length,char p_tofind,t_size p_start = ~0);	//returns infinite if not found
	t_size string_find_first_ex(const char * p_string,t_size p_string_length,const char * p_tofind,t_size p_tofind_length,t_size p_start = 0);	//returns infinite if not found
	t_size string_find_last_ex(const char * p_string,t_size p_string_length,const char * p_tofind,t_size p_tofind_length,t_size p_start = ~0);	//returns infinite if not found

	
	template<typename t_char>
	t_size strlen_max_t(const t_char * ptr,t_size max) {
		if (ptr == NULL) return 0;
		t_size n = 0;
		while(n<max && ptr[n] != 0) n++;
		return n;
	}

	inline t_size strlen_max(const char * ptr,t_size max) throw() {return strlen_max_t(ptr,max);}
	inline t_size wcslen_max(const wchar_t * ptr,t_size max) throw() {return strlen_max_t(ptr,max);}

#ifdef _WINDOWS
	inline t_size tcslen_max(const TCHAR * ptr,t_size max) {return strlen_max_t(ptr,max);}
#endif
	
	bool string_is_numeric(const char * p_string,t_size p_length = infinite) throw();
	inline bool char_is_numeric(char p_char) throw() {return p_char >= '0' && p_char <= '9';}
	inline bool char_is_ascii_alpha_upper(char p_char) throw() {return p_char >= 'A' && p_char <= 'Z';}
	inline bool char_is_ascii_alpha_lower(char p_char) throw() {return p_char >= 'a' && p_char <= 'z';}
	inline bool char_is_ascii_alpha(char p_char) throw() {return char_is_ascii_alpha_lower(p_char) || char_is_ascii_alpha_upper(p_char);}
	inline bool char_is_ascii_alphanumeric(char p_char) throw() {return char_is_ascii_alpha(p_char) || char_is_numeric(p_char);}
	
	unsigned atoui_ex(const char * ptr,t_size max);
	t_int64 atoi64_ex(const char * ptr,t_size max);
	t_uint64 atoui64_ex(const char * ptr,t_size max);

	t_size strlen_utf8(const char * s,t_size num = ~0) throw();//returns number of characters in utf8 string; num - no. of bytes (optional)
	t_size utf8_char_len(const char * s,t_size max = ~0) throw();//returns size of utf8 character pointed by s, in bytes, 0 on error
	t_size utf8_char_len_from_header(char c) throw();
	t_size utf8_chars_to_bytes(const char * string,t_size count) throw();

	t_size strcpy_utf8_truncate(const char * src,char * out,t_size maxbytes);

	t_size utf8_decode_char(const char * src,unsigned & out,t_size src_bytes) throw();//returns length in bytes
	t_size utf8_decode_char(const char * src,unsigned & out) throw();//returns length in bytes

	t_size utf8_encode_char(unsigned c,char * out) throw();//returns used length in bytes, max 6
	t_size utf16_decode_char(const wchar_t * p_source,unsigned * p_out,t_size p_source_length = ~0) throw();
	t_size utf16_encode_char(unsigned c,wchar_t * out) throw();


	t_size strstr_ex(const char * p_string,t_size p_string_len,const char * p_substring,t_size p_substring_len) throw();


	t_size skip_utf8_chars(const char * ptr,t_size count) throw();
	char * strdup_n(const char * src,t_size len);
	int stricmp_ascii(const char * s1,const char * s2) throw();
	int stricmp_ascii_ex(const char * s1,t_size len1,const char * s2,t_size len2) throw();

	int strcmp_ex(const char* p1,t_size n1,const char* p2,t_size n2) throw();

	unsigned utf8_get_char(const char * src);

	inline bool utf8_advance(const char * & var) throw() {
		t_size delta = utf8_char_len(var);
		var += delta;
		return delta>0;
	}

	inline bool utf8_advance(char * & var) throw() {
		t_size delta = utf8_char_len(var);
		var += delta;
		return delta>0;
	}

	inline const char * utf8_char_next(const char * src) throw() {return src + utf8_char_len(src);}
	inline char * utf8_char_next(char * src) throw() {return src + utf8_char_len(src);}

	class NOVTABLE string_base : public pfc::string_receiver {
	public:
		virtual const char * get_ptr() const = 0;
		virtual void add_string(const char * p_string,t_size p_length = ~0) = 0;//same as string_receiver method
		virtual void set_string(const char * p_string,t_size p_length = ~0) {reset();add_string(p_string,p_length);}
		virtual void truncate(t_size len)=0;
		virtual t_size get_length() const {return strlen(get_ptr());}
		virtual char * lock_buffer(t_size p_requested_length) = 0;
		virtual void unlock_buffer() = 0;

		inline const char * toString() const {return get_ptr();}

		//! For compatibility with old conventions.
		inline t_size length() const {return get_length();}
		
		inline void reset() {truncate(0);}
		
		inline bool is_empty() const {return *get_ptr()==0;}
		
		void skip_trailing_char(unsigned c = ' ');

		bool is_valid_utf8() const {return pfc::is_valid_utf8(get_ptr());}

		void convert_to_lower_ascii(const char * src,char replace = '?');

		inline const string_base & operator= (const char * src) {set_string(src);return *this;}
		inline const string_base & operator+= (const char * src) {add_string(src);return *this;}
		inline const string_base & operator= (const string_base & src) {set_string(src);return *this;}
		inline const string_base & operator+= (const string_base & src) {add_string(src);return *this;}

		bool operator==(const string_base & p_other) const {return strcmp(*this,p_other) == 0;}
		bool operator!=(const string_base & p_other) const {return strcmp(*this,p_other) != 0;}
		bool operator>(const string_base & p_other) const {return strcmp(*this,p_other) > 0;}
		bool operator<(const string_base & p_other) const {return strcmp(*this,p_other) < 0;}
		bool operator>=(const string_base & p_other) const {return strcmp(*this,p_other) >= 0;}
		bool operator<=(const string_base & p_other) const {return strcmp(*this,p_other) <= 0;}

		inline operator const char * () const {return get_ptr();}

		t_size scan_filename() const {return pfc::scan_filename(get_ptr());}

		t_size find_first(char p_char,t_size p_start = 0) const {return pfc::string_find_first(get_ptr(),p_char,p_start);}
		t_size find_last(char p_char,t_size p_start = ~0) const {return pfc::string_find_last(get_ptr(),p_char,p_start);}
		t_size find_first(const char * p_string,t_size p_start = 0) const {return pfc::string_find_first(get_ptr(),p_string,p_start);}
		t_size find_last(const char * p_string,t_size p_start = ~0) const {return pfc::string_find_last(get_ptr(),p_string,p_start);}

		void fix_dir_separator(char p_char);

		bool truncate_eol(t_size start = 0);
		bool fix_eol(const char * append = " (...)",t_size start = 0);
		bool limit_length(t_size length_in_chars,const char * append = " (...)");

	protected:
		string_base() {}
		~string_base() {}
	};

	template<t_size max_length>
	class string_fixed_t : public pfc::string_base {
	public:
		inline string_fixed_t() {init();}
		inline string_fixed_t(const string_fixed_t<max_length> & p_source) {init(); *this = p_source;}
		inline string_fixed_t(const char * p_source) {init(); set_string(p_source);}
		
		inline const string_fixed_t<max_length> & operator=(const string_fixed_t<max_length> & p_source) {set_string(p_source);return *this;}
		inline const string_fixed_t<max_length> & operator=(const char * p_source) {set_string(p_source);return *this;}

		char * lock_buffer(t_size p_requested_length) {
			if (p_requested_length >= max_length) return NULL;
			memset(m_data,0,sizeof(m_data));
			return m_data;
		}
		void unlock_buffer() {
			m_length = strlen(m_data);
		}

		inline operator const char * () const {return m_data;}
		
		const char * get_ptr() const {return m_data;}

		void add_string(const char * ptr,t_size len) {
			len = strlen_max(ptr,len);
			if (m_length + len < m_length || m_length + len > max_length) throw pfc::exception_overflow();
			for(t_size n=0;n<len;n++) {
				m_data[m_length++] = ptr[n];
			}
			m_data[m_length] = 0;
		}
		void truncate(t_size len) {
			if (len > max_length) len = max_length;
			if (m_length > len) {
				m_length = len;
				m_data[len] = 0;
			}
		}
		t_size get_length() const {return m_length;}
	private:
		inline void init() {
			pfc::static_assert<(max_length>1)>();
			m_length = 0; m_data[0] = 0;
		}
		t_size m_length;
		char m_data[max_length+1];
	};

	template<template<typename> class t_alloc>
	class string8_t : public pfc::string_base {
	private:
		typedef string8_t<t_alloc> t_self;
	protected:
		pfc::array_t<char,t_alloc> m_data;
		t_size used;

		inline void makespace(t_size s) {
			if (t_alloc<char>::alloc_prioritizes_speed) {
				m_data.set_size(s);
			} else {
				const t_size old_size = m_data.get_size();
				if (old_size < s)
					m_data.set_size(s + 16);
				else if (old_size > s + 32)
					m_data.set_size(s);
			}
		}

		inline const char * _get_ptr() const throw() {return used > 0 ? m_data.get_ptr() : "";}

	public:
		inline const t_self & operator= (const char * src) {set_string(src);return *this;}
		inline const t_self & operator+= (const char * src) {add_string(src);return *this;}
		inline const t_self & operator= (const string_base & src) {set_string(src);return *this;}
		inline const t_self & operator+= (const string_base & src) {add_string(src);return *this;}
		inline const t_self & operator= (const t_self & src) {set_string(src);return *this;}
		inline const t_self & operator+= (const t_self & src) {add_string(src);return *this;}

		inline operator const char * () const throw() {return _get_ptr();}

		string8_t() : used(0) {}
		string8_t(const char * p_string) : used(0) {set_string(p_string);}
		string8_t(const char * p_string,t_size p_length) : used(0) {set_string(p_string,p_length);}
		string8_t(const t_self & p_string) : used(0) {set_string(p_string);}
		string8_t(const string_base & p_string) : used(0) {set_string(p_string);}

		void prealloc(t_size p_size) {m_data.prealloc(p_size+1);}

		const char * get_ptr() const throw() {return _get_ptr();}

		void add_string(const char * p_string,t_size p_length = ~0);
		void set_string(const char * p_string,t_size p_length = ~0);

		void truncate(t_size len)
		{
			if (used>len) {used=len;m_data[len]=0;makespace(used+1);}
		}

		t_size get_length() const throw() {return used;}


		void set_char(unsigned offset,char c);

		t_size replace_nontext_chars(char p_replace = '_');
		t_size replace_char(unsigned c1,unsigned c2,t_size start = 0);
		t_size replace_byte(char c1,char c2,t_size start = 0);
		void fix_filename_chars(char def = '_',char leave=0);//replace "bad" characters, leave parameter can be used to keep eg. path separators
		void remove_chars(t_size first,t_size count); //slow
		void insert_chars(t_size first,const char * src, t_size count);//slow
		void insert_chars(t_size first,const char * src);

		//for string_buffer class
		char * lock_buffer(t_size n)
		{
			if (n + 1 == 0) throw exception_overflow();
			makespace(n+1);
			pfc::memset_t(m_data,(char)0);
			return m_data.get_ptr();;
		}

		void unlock_buffer() {
			if (m_data.get_size() > 0) {
				used=strlen(m_data.get_ptr());
				makespace(used+1);
			}
		}

		void force_reset() {used=0;m_data.force_reset();}

		inline static void g_swap(t_self & p_item1,t_self & p_item2) {
			pfc::swap_t(p_item1.m_data,p_item2.m_data);
			pfc::swap_t(p_item1.used,p_item2.used);
		}
	};

	typedef string8_t<pfc::alloc_standard> string8;
	typedef string8_t<pfc::alloc_fast> string8_fast;
	typedef string8_t<pfc::alloc_fast_aggressive> string8_fast_aggressive;
	//for backwards compatibility
	typedef string8_t<pfc::alloc_fast_aggressive> string8_fastalloc;


	template<template<typename> class t_alloc> class traits_t<string8_t<t_alloc> > : public pfc::combine_traits<pfc::traits_vtable,pfc::traits_t<pfc::array_t<char,t_alloc> > > {
	public:
		enum {
			needs_constructor = true,
		};
	};
}



#include "string8_impl.h"

#define PFC_DEPRECATE_PRINTF PFC_DEPRECATE("Use string8/string_fixed_t with operator<< overloads instead.")

namespace pfc {

	class string_buffer {
	private:
		string_base & m_owner;
		char * m_buffer;
	public:
		explicit string_buffer(string_base & p_string,t_size p_requeted_length) : m_owner(p_string) {m_buffer = m_owner.lock_buffer(p_requeted_length);}
		~string_buffer() {m_owner.unlock_buffer();}
		operator char* () {return m_buffer;}
	};

	class PFC_DEPRECATE_PRINTF string_printf : public string8_fastalloc {
	public:
		static void g_run(string_base & out,const char * fmt,va_list list);
		void run(const char * fmt,va_list list);

		explicit string_printf(const char * fmt,...);
	};

	class PFC_DEPRECATE_PRINTF string_printf_va : public string8_fastalloc {
	public:
		string_printf_va(const char * fmt,va_list list);
	};

	class format_time {
	public:
		format_time(t_uint64 p_seconds);
		const char * get_ptr() const {return m_buffer;}
		operator const char * () const {return m_buffer;}
	protected:
		string_fixed_t<127> m_buffer;
	};


	class format_time_ex {
	public:
		format_time_ex(double p_seconds,unsigned p_extra = 3);
		const char * get_ptr() const {return m_buffer;}
		operator const char * () const {return m_buffer;}
	private:
		string_fixed_t<127> m_buffer;
	};




	class string_filename : public string8 {
	public:
		explicit string_filename(const char * fn);
	};

	class string_filename_ext : public string8 {
	public:
		explicit string_filename_ext(const char * fn);
	};

	class string_extension
	{
		char buffer[32];
	public:
		inline const char * get_ptr() const {return buffer;}
		inline t_size length() const {return strlen(buffer);}
		inline operator const char * () const {return buffer;}
		inline const char * toString() const {return buffer;}
		explicit string_extension(const char * src);
	};


	class string_replace_extension
	{
	public:
		string_replace_extension(const char * p_path,const char * p_ext);
		inline operator const char*() const {return m_data;}
	private:
		string8 m_data;
	};

	class string_directory
	{
	public:
		string_directory(const char * p_path);
		inline operator const char*() const {return m_data;}
	private:
		string8 m_data;
	};

	void float_to_string(char * out,t_size out_max,double val,unsigned precision,bool force_sign = false);//doesnt add E+X etc, has internal range limits, useful for storing float numbers as strings without having to bother with international coma/dot settings BS
	double string_to_float(const char * src,t_size len = infinite);

	template<>
	inline void swap_t(string8 & p_item1,string8 & p_item2)
	{
		string8::g_swap(p_item1,p_item2);
	}

	class format_float
	{
	public:
		format_float(double p_val,unsigned p_width = 0,unsigned p_prec = 7);
		format_float(const format_float & p_source) {*this = p_source;}

		inline const char * get_ptr() const {return m_buffer.get_ptr();}
		inline const char * toString() const {return m_buffer.get_ptr();}
		inline operator const char*() const {return m_buffer.get_ptr();}
	private:
		string8 m_buffer;
	};

	class format_int
	{
	public:
		format_int(t_int64 p_val,unsigned p_width = 0,unsigned p_base = 10);
		format_int(const format_int & p_source) {*this = p_source;}
		inline const char * get_ptr() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
	private:
		char m_buffer[64];
	};


	class format_uint {
	public:
		format_uint(t_uint64 p_val,unsigned p_width = 0,unsigned p_base = 10);
		format_uint(const format_uint & p_source) {*this = p_source;}
		inline const char * get_ptr() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
	private:
		char m_buffer[64];
	};

	class format_hex
	{
	public:
		format_hex(t_uint64 p_val,unsigned p_width = 0);
		format_hex(const format_hex & p_source) {*this = p_source;}
		inline const char * get_ptr() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
	private:
		char m_buffer[17];
	};

	class format_hex_lowercase
	{
	public:
		format_hex_lowercase(t_uint64 p_val,unsigned p_width = 0);
		format_hex_lowercase(const format_hex_lowercase & p_source) {*this = p_source;}
		inline const char * get_ptr() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
	private:
		char m_buffer[17];
	};


	typedef string8_fastalloc string_formatter;

	class format_hexdump_ex {
	public:
		template<typename TWord> format_hexdump_ex(const TWord * buffer, t_size bufLen, const char * spacing = " ") {
			for(t_size n = 0; n < bufLen; n++) {
				if (n > 0 && spacing != NULL) m_formatter << spacing;
				m_formatter << format_hex(buffer[n],sizeof(TWord) * 2);
			}
		}
		inline const char * get_ptr() const {return m_formatter;}
		inline operator const char * () const {return m_formatter;}
		inline const char * toString() const {return m_formatter;}
	private:
		string_formatter m_formatter;
	};

	class format_hexdump
	{
	public:
		format_hexdump(const void * p_buffer,t_size p_bytes,const char * p_spacing = " ");

		inline const char * get_ptr() const {return m_formatter;}
		inline operator const char * () const {return m_formatter;}
		inline const char * toString() const {return m_formatter;}
	private:
		string_formatter m_formatter;
	};

	class format_hexdump_lowercase
	{
	public:
		format_hexdump_lowercase(const void * p_buffer,t_size p_bytes,const char * p_spacing = " ");

		inline const char * get_ptr() const {return m_formatter;}
		inline operator const char * () const {return m_formatter;}
		inline const char * toString() const {return m_formatter;}

	private:
		string_formatter m_formatter;
	};

	class format_fixedpoint
	{
	public:
		format_fixedpoint(t_int64 p_val,unsigned p_point);
		inline const char * get_ptr() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
	private:
		string_formatter m_buffer;
	};

	class format_char {
	public:
		format_char(char p_char) {m_buffer[0] = p_char; m_buffer[1] = 0;}
		inline const char * get_ptr() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
	private:
		char m_buffer[2];
	};

	template<typename t_stringbuffer = pfc::string8_fastalloc>
	class format_pad_left {
	public:
		format_pad_left(t_size p_chars,t_uint32 p_padding /* = ' ' */,const char * p_string,t_size p_string_length = infinite) {
			t_size source_len = 0, source_walk = 0;
			
			while(source_walk < p_string_length && source_len < p_chars) {
				unsigned dummy;
				t_size delta = pfc::utf8_decode_char(p_string + source_walk, dummy, p_string_length - source_walk);
				if (delta == 0) break;
				source_len++;
				source_walk += delta;
			}

			m_buffer.add_string(p_string,source_walk);
			m_buffer.add_chars(p_padding,p_chars - source_len);
		}
		inline const char * get_ptr() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
	private:
		t_stringbuffer m_buffer;
	};

	template<typename t_stringbuffer = pfc::string8_fastalloc>
	class format_pad_right {
	public:
		format_pad_right(t_size p_chars,t_uint32 p_padding /* = ' ' */,const char * p_string,t_size p_string_length = infinite) {
			t_size source_len = 0, source_walk = 0;
			
			while(source_walk < p_string_length && source_len < p_chars) {
				unsigned dummy;
				t_size delta = pfc::utf8_decode_char(p_string + source_walk, dummy, p_string_length - source_walk);
				if (delta == 0) break;
				source_len++;
				source_walk += delta;
			}

			m_buffer.add_chars(p_padding,p_chars - source_len);
			m_buffer.add_string(p_string,source_walk);
		}
		inline const char * get_ptr() const {return m_buffer;}
		inline operator const char*() const {return m_buffer;}
		inline const char * toString() const {return m_buffer;}
	private:
		t_stringbuffer m_buffer;
	};

	class format_array : public string_formatter {
	public:
		template<typename t_source> format_array(t_source const & source, const char * separator = ", ") {
			const t_size count = array_size_t(source);
			if (count > 0) {
				*this << source[0];
				for(t_size walk = 1; walk < count; ++walk) *this << separator << source[walk];
			}
		}
	};


	class format_file_size_short : public string_formatter {
	public:
		format_file_size_short(t_uint64 size);
		t_uint64 get_used_scale() const {return m_scale;}
	private:
		t_uint64 m_scale;
	};

}

inline pfc::string_base & operator<<(pfc::string_base & p_fmt,const char * p_source) {p_fmt.add_string(p_source); return p_fmt;}
inline pfc::string_base & operator<<(pfc::string_base & p_fmt,t_int32 p_val) {return p_fmt << pfc::format_int(p_val);}
inline pfc::string_base & operator<<(pfc::string_base & p_fmt,t_uint32 p_val) {return p_fmt << pfc::format_uint(p_val);}
inline pfc::string_base & operator<<(pfc::string_base & p_fmt,t_int64 p_val) {return p_fmt << pfc::format_int(p_val);}
inline pfc::string_base & operator<<(pfc::string_base & p_fmt,t_uint64 p_val) {return p_fmt << pfc::format_uint(p_val);}
inline pfc::string_base & operator<<(pfc::string_base & p_fmt,double p_val) {return p_fmt << pfc::format_float(p_val);}

inline pfc::string_base & operator<<(pfc::string_base & p_fmt,std::exception const & p_exception) {return p_fmt << p_exception.what();}






namespace pfc {
	template<typename t_char>
	class string_simple_t {
	private:
		typedef string_simple_t<t_char> t_self;
	public:
		t_size length(t_size p_limit = infinite) const {return pfc::strlen_t(get_ptr(),p_limit);}
		bool is_empty() const {return length(1) == 0;}
		void set_string(const t_char * p_source,t_size p_length = infinite) {
			t_size length = pfc::strlen_t(p_source,p_length);
			m_buffer.set_size(length + 1);
			pfc::memcpy_t(m_buffer.get_ptr(),p_source,length);
			m_buffer[length] = 0;
		}
		string_simple_t() {}
		string_simple_t(const t_char * p_source,t_size p_length = infinite) {set_string(p_source,p_length);}
		const t_self & operator=(const t_char * p_source) {set_string(p_source);return *this;}
		operator const t_char* () const {return get_ptr();}
		const t_char * get_ptr() const {return m_buffer.get_size() > 0 ? m_buffer.get_ptr() : pfc::empty_string_t<t_char>();}
	private:
		pfc::array_t<t_char> m_buffer;
	};

	typedef string_simple_t<char> string_simple;

	template<typename t_char> class traits_t<string_simple_t<t_char> > : public traits_t<array_t<t_char> > {};
}


namespace pfc {
	class comparator_strcmp {
	public:
		inline static int compare(const char * p_item1,const char * p_item2) {return strcmp(p_item1,p_item2);}
		inline static int compare(const wchar_t * item1, const wchar_t * item2) {return wcscmp(item1, item2);}
	};

	class comparator_stricmp_ascii {
	public:
		inline static int compare(const char * p_item1,const char * p_item2) {return pfc::stricmp_ascii(p_item1,p_item2);}
	};




	template<typename t_output, typename t_splitCheck>
	void splitStringEx(t_output & p_output, const t_splitCheck & p_check, const char * p_string, t_size p_stringLen = infinite) {
		t_size walk = 0, splitBase = 0;
		const t_size max = strlen_max(p_string,p_stringLen);
		for(;walk < max;) {
			t_size delta = p_check(p_string + walk,p_stringLen - walk);
			if (delta > 0) {
				if (walk > splitBase) p_output(p_string + splitBase, walk - splitBase);
				splitBase = walk + delta;
			} else {
				delta = utf8_char_len(p_string + walk, p_stringLen - walk);
				if (delta == 0) break;
			}
			walk += delta;
		}
		if (walk > splitBase) p_output(p_string + splitBase, walk - splitBase);
	}

	class __splitStringSimple_calculateSubstringCount {
	public:
		__splitStringSimple_calculateSubstringCount() : m_count() {}
		void operator() (const char *, t_size) {++m_count;}
		t_size get() const {return m_count;}
	private:
		t_size m_count;
	};
	class __splitStringSimple_check {
	public:
		__splitStringSimple_check(const char * p_chars) {
			m_chars.set_size(strlen_utf8(p_chars));
			for(t_size walk = 0, ptr = 0; walk < m_chars.get_size(); ++walk) {
				ptr += utf8_decode_char(p_chars + ptr,m_chars[walk]);
			}
		}
		t_size operator()(const char * p_string, t_size p_stringLen) const {
			t_uint32 c;
			t_size delta = utf8_decode_char(p_string, c, p_stringLen);
			if (delta > 0) {
				for(t_size walk = 0; walk < m_chars.get_size(); ++walk) {
					if (m_chars[walk] == c) return delta;
				}
			}
			return 0;
		}
	private:
		array_t<t_uint32> m_chars;
	};
	template<typename t_array>
	class __splitStringSimple_arrayWrapper {
	public:
		__splitStringSimple_arrayWrapper(t_array & p_array) : m_walk(), m_array(p_array) {}
		void operator()(const char * p_string, t_size p_stringLen) {
			m_array[m_walk++].set_string(p_string,p_stringLen);
		}
	private:
		t_size m_walk;
		t_array & m_array;
	};
	template<typename t_list>
	class __splitStringSimple_listWrapper {
	public:
		__splitStringSimple_listWrapper(t_list & p_list) : m_list(p_list) {}
		void operator()(const char * p_string, t_size p_stringLen) {
			m_temp.set_string(p_string,p_stringLen);
			m_list.add_item(m_temp);
		}
	private:
		string8_fastalloc m_temp;
		t_list & m_list;
	};

	template<typename t_array>
	void splitStringSimple_toArray(t_array & p_output, const char * p_splitChars, const char * p_string, t_size p_stringLen = infinite) {
		__splitStringSimple_check check(p_splitChars);

		{
			__splitStringSimple_calculateSubstringCount wrapper;
			splitStringEx(wrapper,check,p_string,p_stringLen);
			p_output.set_size(wrapper.get());
		}

		{
			__splitStringSimple_arrayWrapper<t_array> wrapper(p_output);
			splitStringEx(wrapper,check,p_string,p_stringLen);
		}
	}
	template<typename t_list>
	void splitStringSimple_toList(t_list & p_output, const char * p_splitChars, const char * p_string, t_size p_stringLen = infinite) {
		__splitStringSimple_check check(p_splitChars);

		__splitStringSimple_listWrapper<t_list> wrapper(p_output);
		splitStringEx(wrapper,check,p_string,p_stringLen);
	}

	template<typename t_out> void splitStringByLines(t_out & out, const char * str) {
		for(;;) {
			const char * next = strchr(str, '\n');
			if (next == NULL) {
				out.insert_last()->set_string(str); break;
			}
			const char * walk = next;
			while(walk > str && walk[-1] == '\r') --walk;
			out.insert_last()->set_string(str, walk - str);
			str = next + 1;
		}
	}

	void stringToUpperAppend(string_base & p_out, const char * p_source, t_size p_sourceLen);
	void stringToLowerAppend(string_base & p_out, const char * p_source, t_size p_sourceLen);
	int stringCompareCaseInsensitive(const char * s1, const char * s2);
	t_uint32 charLower(t_uint32 param);
	t_uint32 charUpper(t_uint32 param);

	template<typename T> inline const char * stringToPtr(T const& val) {return val.get_ptr();}
	inline const char * stringToPtr(const char* val) {return val;}



	class string_base_ref : public string_base {
	public:
		string_base_ref(const char * ptr) : m_ptr(ptr), m_len(strlen(ptr)) {}
		const char * get_ptr() const {return m_ptr;}
		t_size get_length() const {return m_len;}
	private:
		void add_string(const char * p_string,t_size p_length = ~0) {throw pfc::exception_not_implemented();}
		void set_string(const char * p_string,t_size p_length = ~0) {throw pfc::exception_not_implemented();}
		void truncate(t_size len) {throw pfc::exception_not_implemented();}
		char * lock_buffer(t_size p_requested_length) {throw pfc::exception_not_implemented();}
		void unlock_buffer() {throw pfc::exception_not_implemented();}
	private:
		const char * const m_ptr;
		t_size const m_len;
	};

	//! Writes a string to a fixed-size buffer. Truncates the string if necessary. Always writes a null terminator.
	template<typename TChar, t_size len, typename TSource>
	void stringToBuffer(TChar (&buffer)[len], const TSource & source) {
		pfc::static_assert<(len>0)>();
		t_size walk;
		for(walk = 0; walk < len - 1 && source[walk] != 0; ++walk) {
			buffer[walk] = source[walk];
		}
		buffer[walk] = 0;
	}

	//! Same as stringToBuffer() but throws exception_overflow() if the string could not be fully written, including null terminator.
	template<typename TChar, t_size len, typename TSource>
	void stringToBufferGuarded(TChar (&buffer)[len], const TSource & source) {
		t_size walk;
		for(walk = 0; source[walk] != 0; ++walk) {
			if (walk >= len) throw exception_overflow();
			buffer[walk] = source[walk];
		}
		if (walk >= len) throw exception_overflow();
		buffer[walk] = 0;
	}


	template<typename t_char> int _strcmp_partial_ex(const t_char * p_string,t_size p_string_length,const t_char * p_substring,t_size p_substring_length) throw() {
		for(t_size walk=0;walk<p_substring_length;walk++) {
			t_char stringchar = (walk>=p_string_length ? 0 : p_string[walk]);
			t_char substringchar = p_substring[walk];
			int result = compare_t(stringchar,substringchar);
			if (result != 0) return result;
		}
		return 0;
	}

	template<typename t_char> int strcmp_partial_ex_t(const t_char * p_string,t_size p_string_length,const t_char * p_substring,t_size p_substring_length) throw() {
		p_string_length = strlen_max_t(p_string,p_string_length); p_substring_length = strlen_max_t(p_substring,p_substring_length);
		return _strcmp_partial_ex(p_string,p_string_length,p_substring,p_substring_length);
	}

	template<typename t_char>
	int strcmp_partial_t(const t_char * p_string,const t_char * p_substring) throw() {return strcmp_partial_ex_t(p_string,infinite,p_substring,infinite);}

	static int strcmp_partial_ex(const char * str, t_size strLen, const char * substr, t_size substrLen) throw() {return strcmp_partial_ex(str, strLen, substr, substrLen); }
	static int strcmp_partial(const char * str, const char * substr) throw() {return strcmp_partial_t(str, substr); }

}

#endif //_PFC_STRING_H_
