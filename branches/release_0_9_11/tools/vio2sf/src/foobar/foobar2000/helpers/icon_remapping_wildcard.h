class icon_remapping_wildcard_impl : public icon_remapping {
public:
	icon_remapping_wildcard_impl(const char * p_pattern,const char * p_iconname) : m_pattern(p_pattern), m_iconname(p_iconname) {}
	bool query(const char * p_extension,pfc::string_base & p_iconname) {
		if (wildcard_helper::test(p_extension,m_pattern,true)) {
			p_iconname = m_iconname; return true;
		} else {
			return false;
		}
	}
private:
	pfc::string8 m_pattern,m_iconname;
};
