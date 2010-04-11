class string_filter_noncasesensitive {
public:
	string_filter_noncasesensitive(const char * p_string,t_size p_string_len = infinite) {
		uStringLower(m_pattern,p_string,p_string_len);
	}

	bool test(const char * p_string,t_size p_string_len = infinite) const {
		::uStringLower(m_lowercasebuffer,p_string,p_string_len);
		t_size walk = 0;
		while(m_pattern[walk] != 0) {
			while(m_pattern[walk] == ' ') walk++;
			t_size delta = 0;
			while(m_pattern[walk+delta] != 0 && m_pattern[walk+delta] != ' ') delta++;
			if (delta > 0) {
				if (pfc::string_find_first_ex(m_lowercasebuffer,infinite,m_pattern+walk,delta) == infinite) return false;
			}
			walk += delta;
		}
		return true;
	}
private:
	mutable pfc::string8_fastalloc m_lowercasebuffer;
	pfc::string8 m_pattern;
};
