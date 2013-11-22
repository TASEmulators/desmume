//! Entrypoint interface for registering media file types that can be opened through "open file" dialogs or associated with foobar2000 application in Windows shell. \n
//! Instead of implementing this directly, use DECLARE_FILE_TYPE() / DECLARE_FILE_TYPE_EX() macros.
class input_file_type : public service_base {
public:
	virtual unsigned get_count()=0;
	virtual bool get_name(unsigned idx,pfc::string_base & out)=0;//eg. "MPEG files"
	virtual bool get_mask(unsigned idx,pfc::string_base & out)=0;//eg. "*.MP3;*.MP2"; separate with semicolons
	virtual bool is_associatable(unsigned idx) = 0;

	static void build_openfile_mask(pfc::string_base & out,bool b_include_playlists=true);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(input_file_type);
};

//! Extended interface for registering media file types that can be associated with foobar2000 application in Windows shell. \n
//! Instead of implementing this directly, use DECLARE_FILE_TYPE() / DECLARE_FILE_TYPE_EX() macros.
class input_file_type_v2 : public input_file_type {
public:
	virtual void get_format_name(unsigned idx, pfc::string_base & out, bool isPlural) = 0;
	virtual void get_extensions(unsigned idx, pfc::string_base & out) = 0;

	//Deprecated input_file_type method implementations:
	bool get_name(unsigned idx, pfc::string_base & out) {get_format_name(idx, out, true); return true;}
	bool get_mask(unsigned idx, pfc::string_base & out) {
		pfc::string_formatter temp; get_extensions(idx,temp);
		pfc::chain_list_v2_t<pfc::string> exts; pfc::splitStringSimple_toList(exts,";",temp);
		if (exts.get_count() == 0) return false;//should not happen
		temp.reset();
		for(pfc::const_iterator<pfc::string> walk = exts.first(); walk.is_valid(); ++walk) {
			if (!temp.is_empty()) temp << ";";
			temp << "*." << walk->get_ptr();
		}
		out = temp;
		return true;
	}

	FB2K_MAKE_SERVICE_INTERFACE(input_file_type_v2,input_file_type)
};


//! Implementation helper.
class input_file_type_impl : public service_impl_single_t<input_file_type>
{
	const char * name, * mask;
	bool m_associatable;
public:
	input_file_type_impl(const char * p_name, const char * p_mask,bool p_associatable) : name(p_name), mask(p_mask), m_associatable(p_associatable) {}
	unsigned get_count() {return 1;}
	bool get_name(unsigned idx,pfc::string_base & out) {if (idx==0) {out = name; return true;} else return false;}
	bool get_mask(unsigned idx,pfc::string_base & out) {if (idx==0) {out = mask; return true;} else return false;}
	bool is_associatable(unsigned idx) {return m_associatable;}
};


//! Helper macro for registering our media file types.
//! Usage: DECLARE_FILE_TYPE("Blah files","*.blah;*.bleh");
#define DECLARE_FILE_TYPE(NAME,MASK) \
	namespace { static input_file_type_impl g_filetype_instance(NAME,MASK,true); \
	static service_factory_single_ref_t<input_file_type_impl> g_filetype_service(g_filetype_instance); }




//! Implementation helper.
//! Usage: static input_file_type_factory mytype("blah type","*.bla;*.meh",true);
class input_file_type_factory : private service_factory_single_transparent_t<input_file_type_impl>
{
public:
	input_file_type_factory(const char * p_name,const char * p_mask,bool p_associatable)
		: service_factory_single_transparent_t<input_file_type_impl>(p_name,p_mask,p_associatable) {}
};



class input_file_type_v2_impl : public input_file_type_v2 {
public:
	input_file_type_v2_impl(const char * extensions,const char * name, const char * namePlural) : m_extensions(extensions), m_name(name), m_namePlural(namePlural) {}
	unsigned get_count() {return 1;}
	bool is_associatable(unsigned idx) {return true;}
	void get_format_name(unsigned idx, pfc::string_base & out, bool isPlural) {
		out = isPlural ? m_namePlural : m_name;
	}
	void get_extensions(unsigned idx, pfc::string_base & out) {
		out = m_extensions;
	}

private:
	const pfc::string8 m_name, m_namePlural, m_extensions;
};

//! Helper macro for registering our media file types, extended version providing separate singular/plural type names.
//! Usage: DECLARE_FILE_TYPE_EX("mp1;mp2;mp3","MPEG file","MPEG files")
#define DECLARE_FILE_TYPE_EX(extensions, name, namePlural) \
	namespace { static service_factory_single_t<input_file_type_v2_impl> g_myfiletype(extensions, name, namePlural); }
