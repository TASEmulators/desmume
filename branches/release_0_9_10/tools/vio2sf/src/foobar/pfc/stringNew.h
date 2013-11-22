namespace pfc {
	//helper, const methods only
	class __stringEmpty : public string_base {
	public:
		const char * get_ptr() const {return "";}
		void add_string(const char * p_string,t_size p_length = ~0) {throw exception_not_implemented();}
		void set_string(const char * p_string,t_size p_length = ~0) {throw exception_not_implemented();}
		void truncate(t_size len) {throw exception_not_implemented();}
		t_size get_length() const {return 0;}
		char * lock_buffer(t_size p_requested_length) {throw exception_not_implemented();}
		void unlock_buffer() {throw exception_not_implemented();}
	};

	class stringp;

	//! New EXPERIMENTAL string class, allowing efficient copies and returning from functions. \n
	//! Does not implement the string_base interface so you still need string8 in many cases. \n
	//! Safe to pass between DLLs, but since a reference is used, objects possibly created by other DLLs must be released before owning DLLs are unloaded.
	class string {
	public:
		typedef rcptr_t<string_base const> t_data;
		typedef rcptr_t<pfc::string8> t_dataImpl;
		
		string() : m_content(rcnew_t<__stringEmpty>()) {}
		string(const char * p_source) : m_content(rcnew_t<string8>(p_source)) {}
		string(const char * p_source, t_size p_sourceLen) : m_content(rcnew_t<pfc::string8>(p_source,p_sourceLen)) {}
		string(t_data const & p_source) : m_content(p_source) {}
		template<typename TSource> string(const TSource & p_source);

		string const & toString() const {return *this;}

		//warning, not length-checked anymore!
		static string g_concatenateRaw(const char * item1, t_size len1, const char * item2, t_size len2) {
			t_dataImpl impl; impl.new_t();
			char * buffer = impl->lock_buffer(len1+len2);
			memcpy_t(buffer,item1,len1);
			memcpy_t(buffer+len1,item2,len2);
			impl->unlock_buffer();
			return string(t_data(impl));
		}

		string operator+(const string& p_item2) const {
			return g_concatenateRaw(ptr(),length(),p_item2.ptr(),p_item2.length());
		}
		string operator+(const char * p_item2) const {
			return g_concatenateRaw(ptr(),length(),p_item2,strlen(p_item2));
		}

		template<typename TSource> string operator+(const TSource & p_item2) const;

		template<typename TSource>
		const string & operator+=(const TSource & p_item) {
			*this = *this + p_item;
			return *this;
		}

		string subString(t_size base) const {
			if (base > length()) throw exception_overflow();
			return string(ptr() + base);
		}
		string subString(t_size base, t_size count) const {
			return string(ptr() + base,count);
		}

		string toLower() const {
			pfc::string8_fastalloc temp; temp.prealloc(128);
			stringToLowerAppend(temp,ptr(),~0);
			return string(temp.get_ptr());
		}
		string toUpper() const {
			pfc::string8_fastalloc temp; temp.prealloc(128);
			stringToUpperAppend(temp,ptr(),~0);
			return string(temp.get_ptr());
		}
		
		string clone() const {return string(ptr());}
		
		//! @returns ~0 if not found.
		t_size indexOf(char c,t_size base = 0) const;
		//! @returns ~0 if not found.
		t_size lastIndexOf(char c,t_size base = ~0) const;
		//! @returns ~0 if not found.
		t_size indexOf(stringp s,t_size base = 0) const;
		//! @returns ~0 if not found.
		t_size lastIndexOf(stringp s,t_size base = ~0) const;
		//! @returns ~0 if not found.
		t_size indexOfAnyChar(stringp s,t_size base = 0) const;
		//! @returns ~0 if not found.
		t_size lastIndexOfAnyChar(stringp s,t_size base = ~0) const;

		bool contains(char c) const;
		bool contains(stringp s) const;

		bool containsAnyChar(stringp s) const;

		bool startsWith(char c) const;
		bool startsWith(string s) const;
		bool endsWith(char c) const;
		bool endsWith(string s) const;

		char firstChar() const;
		char lastChar() const;

		string replace(stringp strOld, stringp strNew) const;

		static int g_compare(const string & p_item1, const string & p_item2) {return strcmp(p_item1.ptr(),p_item2.ptr());}
		bool operator==(const string& p_other) const {return g_compare(*this,p_other) == 0;}
		bool operator!=(const string& p_other) const {return g_compare(*this,p_other) != 0;}
		bool operator<(const string& p_other) const {return g_compare(*this,p_other) < 0;}
		bool operator>(const string& p_other) const {return g_compare(*this,p_other) > 0;}
		bool operator<=(const string& p_other) const {return g_compare(*this,p_other) <= 0;}
		bool operator>=(const string& p_other) const {return g_compare(*this,p_other) >= 0;}

		const char * ptr() const {return m_content->get_ptr();}
		const char * get_ptr() const {return m_content->get_ptr();}
		t_size length() const {return m_content->get_length();}
		t_size get_length() const {return m_content->get_length();}

		void set_string(const char * ptr, t_size len = ~0) {
			*this = string(ptr, len);
		}

		static bool isNonTextChar(char c) {return c >= 0 && c < 32;}

		char operator[](t_size p_index) const {
			PFC_ASSERT(p_index < length());
			return ptr()[p_index];
		}
		bool isEmpty() const {return length() == 0;}

		class comparatorCaseSensitive {
		public:
			template<typename T1,typename T2>
			static int compare(T1 const& v1, T2 const& v2) {
				return strcmp(stringToPtr(v1),stringToPtr(v2));
			}
			static int compare_ex(const char * v1, t_size l1, const char * v2, t_size l2) {
				return strcmp_ex(v1, l1, v2, l2);
			}

		};
		class comparatorCaseInsensitive {
		public:
			template<typename T1,typename T2>
			static int compare(T1 const& v1, T2 const& v2) {
				return stringCompareCaseInsensitive(stringToPtr(v1),stringToPtr(v2));
			}
		};
		class comparatorCaseInsensitiveASCII {
		public:
			template<typename T1,typename T2>
			static int compare(T1 const& v1, T2 const& v2) {
				return stricmp_ascii(stringToPtr(v1),stringToPtr(v2));
			}

			static int compare_ex(const char * v1, t_size l1, const char * v2, t_size l2) {
				return stricmp_ascii_ex(v1, l1, v2, l2);
			}
		};
		
		static bool g_equals(const string & p_item1, const string & p_item2) {return p_item1 == p_item2;}
		static bool g_equalsCaseInsensitive(const string & p_item1, const string & p_item2) {return comparatorCaseInsensitive::compare(p_item1,p_item2) == 0;}

		t_data _content() const {return m_content;}
	private:
		t_data m_content;
	};

	template<typename T> inline string toString(T const& val) {return val.toString();}
	template<> inline string toString(t_int64 const& val) {return format_int(val).get_ptr();}
	template<> inline string toString(t_int32 const& val) {return format_int(val).get_ptr();}
	template<> inline string toString(t_int16 const& val) {return format_int(val).get_ptr();}
	template<> inline string toString(t_uint64 const& val) {return format_uint(val).get_ptr();}
	template<> inline string toString(t_uint32 const& val) {return format_uint(val).get_ptr();}
	template<> inline string toString(t_uint16 const& val) {return format_uint(val).get_ptr();}
	template<> inline string toString(float const& val) {return format_float(val).get_ptr();}
	template<> inline string toString(double const& val) {return format_float(val).get_ptr();}
	template<> inline string toString(char const& val) {return string(&val,1);}
	inline const char * toString(std::exception const& val) {return val.what();}

	template<typename TSource> string::string(const TSource & p_source) {
		*this = pfc::toString(p_source);
	}
	template<typename TSource> string string::operator+(const TSource & p_item2) const {
		return *this + pfc::toString(p_item2);
	}
	
	//! "String parameter" helper class, to use in function parameters, allowing functions to take any type of string as a parameter (const char*, string_base, string).
	class stringp {
	public:
		stringp(const char * ptr) : m_ptr(ptr) {}
		stringp(pfc::string const &s) : m_ptr(s.ptr()), m_s(s._content()) {}
		stringp(pfc::string_base const &s) : m_ptr(s.get_ptr()) {}
		template<typename TWhat> stringp(const TWhat& in) : m_ptr(in.toString()) {}

		operator const char*() const {return m_ptr;}
		const char * ptr() const {return m_ptr;}
		const char * get_ptr() const {return m_ptr;}
		pfc::string str() const {return m_s.is_valid() ? pfc::string(m_s) : pfc::string(m_ptr);}
		operator pfc::string() const {return str();}
		pfc::string toString() const {return str();}
		t_size length() const {return m_s.is_valid() ? m_s->length() : strlen(m_ptr);}
	private:
		const char * const m_ptr;
		pfc::string::t_data m_s;
	};

	template<typename TList>
	string stringCombineList(const TList & list, stringp separator) {
		typename TList::const_iterator iter = list.first();
		string acc;
		if (iter.is_valid()) {
			acc = *iter;
			for(++iter; iter.is_valid(); ++iter) {
				acc = acc + separator + *iter;
			}
		}
		return acc;
	}
}
