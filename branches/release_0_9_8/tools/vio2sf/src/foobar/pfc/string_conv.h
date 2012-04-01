namespace pfc {

	namespace stringcvt {

#ifdef _WINDOWS
		enum {
			codepage_system = CP_ACP,
			codepage_ascii = 20127,
			codepage_iso_8859_1 = 28591,
		};

		//! Converts UTF-8 characters to wide character.
		//! @param p_out Output buffer, receives converted string, with null terminator.
		//! @param p_out_size Size of output buffer, in characters. If converted string is too long, it will be truncated. Null terminator is always written, unless p_out_size is zero.
		//! @param p_source String to convert.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters written, not counting null terminator.
		t_size convert_utf8_to_wide(wchar_t * p_out,t_size p_out_size,const char * p_source,t_size p_source_size);
		

		//! Estimates buffer size required to convert specified UTF-8 string to widechar.
		//! @param p_source String to be converted.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters to allocate, including space for null terminator.
		t_size estimate_utf8_to_wide(const char * p_source,t_size p_source_size);
		
		t_size estimate_utf8_to_wide(const char * p_source);

		//! Converts wide character string to UTF-8.
		//! @param p_out Output buffer, receives converted string, with null terminator.
		//! @param p_out_size Size of output buffer, in characters. If converted string is too long, it will be truncated. Null terminator is always written, unless p_out_size is zero.
		//! @param p_source String to convert.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters written, not counting null terminator.
		t_size convert_wide_to_utf8(char * p_out,t_size p_out_size,const wchar_t * p_source,t_size p_source_size);

		//! Estimates buffer size required to convert specified wide character string to UTF-8.
		//! @param p_source String to be converted.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters to allocate, including space for null terminator.
		t_size estimate_wide_to_utf8(const wchar_t * p_source,t_size p_source_size);



		//! Converts string from specified codepage to wide character.
		//! @param p_out Output buffer, receives converted string, with null terminator.
		//! @param p_codepage Codepage ID of source string.
		//! @param p_source String to convert.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @param p_out_size Size of output buffer, in characters. If converted string is too long, it will be truncated. Null terminator is always written, unless p_out_size is zero.
		//! @returns Number of characters written, not counting null terminator.
		t_size convert_codepage_to_wide(unsigned p_codepage,wchar_t * p_out,t_size p_out_size,const char * p_source,t_size p_source_size);

		//! Estimates buffer size required to convert specified string from specified codepage to wide character.
		//! @param p_codepage Codepage ID of source string.
		//! @param p_source String to be converted.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters to allocate, including space for null terminator.
		t_size estimate_codepage_to_wide(unsigned p_codepage,const char * p_source,t_size p_source_size);

		//! Converts string from wide character to specified codepage.
		//! @param p_codepage Codepage ID of source string.
		//! @param p_out Output buffer, receives converted string, with null terminator.
		//! @param p_out_size Size of output buffer, in characters. If converted string is too long, it will be truncated. Null terminator is always written, unless p_out_size is zero.
		//! @param p_source String to convert.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters written, not counting null terminator.
		t_size convert_wide_to_codepage(unsigned p_codepage,char * p_out,t_size p_out_size,const wchar_t * p_source,t_size p_source_size);

		//! Estimates buffer size required to convert specified wide character string to specified codepage.
		//! @param p_codepage Codepage ID of source string.
		//! @param p_source String to be converted.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters to allocate, including space for null terminator.
		t_size estimate_wide_to_codepage(unsigned p_codepage,const wchar_t * p_source,t_size p_source_size);
		

		//! Converts string from system codepage to wide character.
		//! @param p_out Output buffer, receives converted string, with null terminator.
		//! @param p_source String to convert.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @param p_out_size Size of output buffer, in characters. If converted string is too long, it will be truncated. Null terminator is always written, unless p_out_size is zero.
		//! @returns Number of characters written, not counting null terminator.
		inline t_size convert_ansi_to_wide(wchar_t * p_out,t_size p_out_size,const char * p_source,t_size p_source_size) {
			return convert_codepage_to_wide(codepage_system,p_out,p_out_size,p_source,p_source_size);
		}

		//! Estimates buffer size required to convert specified system codepage string to wide character.
		//! @param p_source String to be converted.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters to allocate, including space for null terminator.
		inline t_size estimate_ansi_to_wide(const char * p_source,t_size p_source_size) {
			return estimate_codepage_to_wide(codepage_system,p_source,p_source_size);
		}

		//! Converts string from wide character to system codepage.
		//! @param p_out Output buffer, receives converted string, with null terminator.
		//! @param p_out_size Size of output buffer, in characters. If converted string is too long, it will be truncated. Null terminator is always written, unless p_out_size is zero.
		//! @param p_source String to convert.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters written, not counting null terminator.
		inline t_size convert_wide_to_ansi(char * p_out,t_size p_out_size,const wchar_t * p_source,t_size p_source_size) {
			return convert_wide_to_codepage(codepage_system,p_out,p_out_size,p_source,p_source_size);
		}

		//! Estimates buffer size required to convert specified wide character string to system codepage.
		//! @param p_source String to be converted.
		//! @param p_source_size Number of characters to read from p_source. If reading stops if null terminator is encountered earlier.
		//! @returns Number of characters to allocate, including space for null terminator.
		inline t_size estimate_wide_to_ansi(const wchar_t * p_source,t_size p_source_size) {
			return estimate_wide_to_codepage(codepage_system,p_source,p_source_size);
		}


		//! estimate_utf8_to_wide_quick() functions use simple math to determine buffer size required for the conversion. The result is not accurate length of output string - it's just a safe estimate of required buffer size, possibly bigger than what's really needed. \n
		//! These functions are meant for scenarios when speed is more important than memory usage.
		inline t_size estimate_utf8_to_wide_quick(t_size sourceLen) {
			return sourceLen + 1;
		}
		inline t_size estimate_utf8_to_wide_quick(const char * source) {
			return estimate_utf8_to_wide_quick(strlen(source));
		}
		inline t_size estimate_utf8_to_wide_quick(const char * source, t_size sourceLen) {
			return estimate_utf8_to_wide_quick(strlen_max(source, sourceLen));
		}
		t_size convert_utf8_to_wide_unchecked(wchar_t * p_out,const char * p_source);

		template<typename t_char> const t_char * null_string_t();
		template<> inline const char * null_string_t<char>() {return "";}
		template<> inline const wchar_t * null_string_t<wchar_t>() {return L"";}

		template<typename t_char> t_size strlen_t(const t_char * p_string,t_size p_string_size = ~0) {
			for(t_size n=0;n<p_string_size;n++) {
				if (p_string[n] == 0) return n;
			}
			return p_string_size;
		}

		template<typename t_char> bool string_is_empty_t(const t_char * p_string,t_size p_string_size = ~0) {
			if (p_string_size == 0) return true;
			return p_string[0] == 0;
		}

		template<typename t_char,template<typename t_allocitem> class t_alloc = pfc::alloc_standard> class char_buffer_t {
		public:
			char_buffer_t() {}
			char_buffer_t(const char_buffer_t & p_source) : m_buffer(p_source.m_buffer) {}
			void set_size(t_size p_count) {m_buffer.set_size(p_count);}
			t_char * get_ptr_var() {return m_buffer.get_ptr();}
			const t_char * get_ptr() const {
				return m_buffer.get_size() > 0 ? m_buffer.get_ptr() : null_string_t<t_char>();
			}
		private:
			pfc::array_t<t_char,t_alloc> m_buffer;
		};

		template<template<typename t_allocitem> class t_alloc = pfc::alloc_standard>
		class string_utf8_from_wide_t {
		public:
			string_utf8_from_wide_t() {}
			string_utf8_from_wide_t(const wchar_t * p_source,t_size p_source_size = ~0) {convert(p_source,p_source_size);}
		
			void convert(const wchar_t * p_source,t_size p_source_size = ~0) {
				t_size size = estimate_wide_to_utf8(p_source,p_source_size);
				m_buffer.set_size(size);
				convert_wide_to_utf8( m_buffer.get_ptr_var(),size,p_source,p_source_size);
			}

			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			const char * toString() const {return get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

		private:
			char_buffer_t<char,t_alloc> m_buffer;
		};
		typedef string_utf8_from_wide_t<> string_utf8_from_wide;
		
		template<template<typename t_allocitem> class t_alloc = pfc::alloc_standard>
		class string_wide_from_utf8_t {
		public:
			string_wide_from_utf8_t() {}
			string_wide_from_utf8_t(const char* p_source) {convert(p_source);}
			string_wide_from_utf8_t(const char* p_source,t_size p_source_size) {convert(p_source,p_source_size);}

			void convert(const char* p_source,t_size p_source_size) {
				const t_size size = estimate_size(p_source, p_source_size);
				m_buffer.set_size(size);
				convert_utf8_to_wide( m_buffer.get_ptr_var(),size,p_source,p_source_size );
			}
			void convert(const char * p_source) {
				m_buffer.set_size( estimate_size(p_source) );
				convert_utf8_to_wide_unchecked(m_buffer.get_ptr_var(), p_source);
			}

			operator const wchar_t * () const {return get_ptr();}
			const wchar_t * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

			enum { alloc_prioritizes_speed = t_alloc<wchar_t>::alloc_prioritizes_speed };
		private:
			
			inline t_size estimate_size(const char * source, t_size sourceLen) {
				return alloc_prioritizes_speed ? estimate_utf8_to_wide_quick(source, sourceLen) : estimate_utf8_to_wide(source,sourceLen);
			}
			inline t_size estimate_size(const char * source) {
				return alloc_prioritizes_speed ? estimate_utf8_to_wide_quick(source) : estimate_utf8_to_wide(source,~0);
			}
			char_buffer_t<wchar_t,t_alloc> m_buffer;
		};
		typedef string_wide_from_utf8_t<> string_wide_from_utf8;
		typedef string_wide_from_utf8_t<alloc_fast_aggressive> string_wide_from_utf8_fast;

		template<template<typename t_allocitem> class t_alloc = pfc::alloc_standard>
		class string_wide_from_codepage_t {
		public:
			string_wide_from_codepage_t() {}
			string_wide_from_codepage_t(const string_wide_from_codepage_t<t_alloc> & p_source) : m_buffer(p_source.m_buffer) {}
			string_wide_from_codepage_t(unsigned p_codepage,const char * p_source,t_size p_source_size = ~0) {convert(p_codepage,p_source,p_source_size);}

			void convert(unsigned p_codepage,const char * p_source,t_size p_source_size = ~0) {
				t_size size = estimate_codepage_to_wide(p_codepage,p_source,p_source_size);
				m_buffer.set_size(size);
				convert_codepage_to_wide(p_codepage, m_buffer.get_ptr_var(),size,p_source,p_source_size);
			}

			operator const wchar_t * () const {return get_ptr();}
			const wchar_t * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

		private:
			char_buffer_t<wchar_t,t_alloc> m_buffer;
		};
		typedef string_wide_from_codepage_t<> string_wide_from_codepage;

		
		template<template<typename t_allocitem> class t_alloc = pfc::alloc_standard>
		class string_codepage_from_wide_t {
		public:
			string_codepage_from_wide_t() {}
			string_codepage_from_wide_t(const string_codepage_from_wide_t<t_alloc> & p_source) : m_buffer(p_source.m_buffer) {}
			string_codepage_from_wide_t(unsigned p_codepage,const wchar_t * p_source,t_size p_source_size = ~0) {convert(p_codepage,p_source,p_source_size);}

			void convert(unsigned p_codepage,const wchar_t * p_source,t_size p_source_size = ~0) {
				t_size size = estimate_wide_to_codepage(p_codepage,p_source,p_source_size);
				m_buffer.set_size(size);
				convert_wide_to_codepage(p_codepage, m_buffer.get_ptr_var(),size,p_source,p_source_size);
			}

			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

		private:
			char_buffer_t<char,t_alloc> m_buffer;
		};
		typedef string_codepage_from_wide_t<> string_codepage_from_wide;

		class string_codepage_from_utf8 {
		public:
			string_codepage_from_utf8() {}
			string_codepage_from_utf8(const string_codepage_from_utf8 & p_source) : m_buffer(p_source.m_buffer) {}
			string_codepage_from_utf8(unsigned p_codepage,const char * p_source,t_size p_source_size = ~0) {convert(p_codepage,p_source,p_source_size);}
			
			void convert(unsigned p_codepage,const char * p_source,t_size p_source_size = ~0) {
				string_wide_from_utf8 temp;
				temp.convert(p_source,p_source_size);
				t_size size = estimate_wide_to_codepage(p_codepage,temp,~0);
				m_buffer.set_size(size);
				convert_wide_to_codepage(p_codepage,m_buffer.get_ptr_var(),size,temp,~0);
			}

			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

		private:
			char_buffer_t<char> m_buffer;
		};

		class string_utf8_from_codepage {
		public:
			string_utf8_from_codepage() {}
			string_utf8_from_codepage(const string_utf8_from_codepage & p_source) : m_buffer(p_source.m_buffer) {}
			string_utf8_from_codepage(unsigned p_codepage,const char * p_source,t_size p_source_size = ~0) {convert(p_codepage,p_source,p_source_size);}
			
			void convert(unsigned p_codepage,const char * p_source,t_size p_source_size = ~0) {
				string_wide_from_codepage temp;
				temp.convert(p_codepage,p_source,p_source_size);
				t_size size = estimate_wide_to_utf8(temp,~0);
				m_buffer.set_size(size);
				convert_wide_to_utf8( m_buffer.get_ptr_var(),size,temp,~0);
			}

			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

		private:
			char_buffer_t<char> m_buffer;
		};


		class string_utf8_from_ansi {
		public:
			string_utf8_from_ansi() {}
			string_utf8_from_ansi(const string_utf8_from_ansi & p_source) : m_buffer(p_source.m_buffer) {}
			string_utf8_from_ansi(const char * p_source,t_size p_source_size = ~0) : m_buffer(codepage_system,p_source,p_source_size) {}
			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			const char * toString() const {return get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}
			void convert(const char * p_source,t_size p_source_size = ~0) {m_buffer.convert(codepage_system,p_source,p_source_size);}

		private:
			string_utf8_from_codepage m_buffer;
		};

		class string_ansi_from_utf8 {
		public:
			string_ansi_from_utf8() {}
			string_ansi_from_utf8(const string_ansi_from_utf8 & p_source) : m_buffer(p_source.m_buffer) {}
			string_ansi_from_utf8(const char * p_source,t_size p_source_size = ~0) : m_buffer(codepage_system,p_source,p_source_size) {}
			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

			void convert(const char * p_source,t_size p_source_size = ~0) {m_buffer.convert(codepage_system,p_source,p_source_size);}

		private:
			string_codepage_from_utf8 m_buffer;
		};

		class string_wide_from_ansi {
		public:
			string_wide_from_ansi() {}
			string_wide_from_ansi(const string_wide_from_ansi & p_source) : m_buffer(p_source.m_buffer) {}
			string_wide_from_ansi(const char * p_source,t_size p_source_size = ~0) : m_buffer(codepage_system,p_source,p_source_size) {}
			operator const wchar_t * () const {return get_ptr();}
			const wchar_t * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

			void convert(const char * p_source,t_size p_source_size = ~0) {m_buffer.convert(codepage_system,p_source,p_source_size);}

		private:
			string_wide_from_codepage m_buffer;
		};

		class string_ansi_from_wide {
		public:
			string_ansi_from_wide() {}
			string_ansi_from_wide(const string_ansi_from_wide & p_source) : m_buffer(p_source.m_buffer) {}
			string_ansi_from_wide(const wchar_t * p_source,t_size p_source_size = ~0) : m_buffer(codepage_system,p_source,p_source_size) {}
			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return string_is_empty_t(get_ptr());}
			t_size length() const {return strlen_t(get_ptr());}

			void convert(const wchar_t * p_source,t_size p_source_size = ~0) {m_buffer.convert(codepage_system,p_source,p_source_size);}

		private:
			string_codepage_from_wide m_buffer;
		};

#ifdef UNICODE
		typedef string_wide_from_utf8 string_os_from_utf8;
		typedef string_utf8_from_wide string_utf8_from_os;
		typedef string_wide_from_utf8_fast string_os_from_utf8_fast;
#else
		typedef string_ansi_from_utf8 string_os_from_utf8;
		typedef string_utf8_from_ansi string_utf8_from_os;
		typedef string_ansi_from_utf8 string_os_from_utf8_fast;
#endif

		class string_utf8_from_os_ex {
		public:
			template<typename t_source> string_utf8_from_os_ex(const t_source * source, t_size sourceLen = ~0) {
				convert(source,sourceLen);
			}

			void convert(const char * source, t_size sourceLen = ~0) {
				m_buffer = string_utf8_from_ansi(source,sourceLen);
			}
			void convert(const wchar_t * source, t_size sourceLen = ~0) {
				m_buffer = string_utf8_from_wide(source,sourceLen);
			}

			operator const char * () const {return get_ptr();}
			const char * get_ptr() const {return m_buffer.get_ptr();}
			bool is_empty() const {return m_buffer.is_empty();}
			t_size length() const {return m_buffer.length();}
			const char * toString() const {return get_ptr();}
		private:
			string8 m_buffer;
		};
	}

#else
//PORTME
#endif
};
