//! New in 0.9.5; allows your file format to use another icon than <extension>.ico when registering the file type with Windows shell. \n
//! Implementation: use icon_remapping_impl, or simply: static service_factory_single_t<icon_remapping_impl> myicon("ext","iconname.ico");
class icon_remapping : public service_base {
public:
	//! @param p_extension File type extension being queried.
	//! @param p_iconname Receives the icon name to use, including the .ico extension.
	//! @returns True when p_iconname has been set, false if we don't recognize the specified extension.
	virtual bool query(const char * p_extension,pfc::string_base & p_iconname) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(icon_remapping);
};

//! Standard implementation of icon_remapping.
class icon_remapping_impl : public icon_remapping {
public:
	icon_remapping_impl(const char * p_extension,const char * p_iconname) : m_extension(p_extension), m_iconname(p_iconname) {}
	bool query(const char * p_extension,pfc::string_base & p_iconname) {
		if (stricmp_utf8(p_extension,m_extension) == 0) {
			p_iconname = m_iconname; return true;
		} else {
			return false;
		}
	}
private:
	pfc::string8 m_extension,m_iconname;
};
