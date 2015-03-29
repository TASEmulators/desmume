class __meta_table_enum_wrapper {
public:
	__meta_table_enum_wrapper(file_info & p_info) : m_info(p_info) {}
	template<typename t_values>
	void operator() (const char * p_name,const t_values & p_values) {
		t_size index = infinite;
		for(t_values::const_iterator iter = p_values.first(); iter.is_valid(); ++iter) {
			if (index == infinite) index = m_info.__meta_add_unsafe(p_name,*iter);
			else m_info.meta_add_value(index,*iter);
		}
	}
private:
	file_info & m_info;
};

//! Purpose: building a file_info metadata table from loose input without search-for-existing-entry bottleneck
class meta_table_builder {
public:
	typedef pfc::chain_list_v2_t<pfc::string8> t_entry;
	typedef pfc::map_t<pfc::string8,t_entry,file_info::field_name_comparator> t_content;

	t_content & content() {return m_data;}
	t_content const & content() const {return m_data;}

	void add(const char * p_name,const char * p_value,t_size p_value_len = infinite) {
		if (file_info::g_is_valid_field_name(p_name)) {
			__add(p_name).insert_last()->set_string(p_value,p_value_len);
		}
	}

	void remove(const char * p_name) {
		m_data.remove(p_name);
	}
	void set(const char * p_name,const char * p_value,t_size p_value_len = infinite) {
		if (file_info::g_is_valid_field_name(p_name)) {
			t_entry & entry = __add(p_name);
			entry.remove_all();
			entry.insert_last()->set_string(p_value,p_value_len);
		}
	}
	t_entry & add(const char * p_name) {
		if (!file_info::g_is_valid_field_name(p_name)) throw pfc::exception_bug_check_v2();//we return a reference, nothing smarter to do
		return __add(p_name);
	}
	void finalize(file_info & p_info) const {
		p_info.meta_remove_all();
		m_data.enumerate(__meta_table_enum_wrapper(p_info));
	}

	void from_info(const file_info & p_info) {
		m_data.remove_all();
		from_info_overwrite(p_info);
	}
	void from_info_overwrite(const file_info & p_info) {
		for(t_size metawalk = 0, metacount = p_info.meta_get_count(); metawalk < metacount; ++metawalk ) {
			const t_size valuecount = p_info.meta_enum_value_count(metawalk);
			if (valuecount > 0) {
				t_entry & entry = add(p_info.meta_enum_name(metawalk));
				entry.remove_all();
				for(t_size valuewalk = 0; valuewalk < valuecount; ++valuewalk) {
					entry.insert_last(p_info.meta_enum_value(metawalk,valuewalk));
				}
			}
		}
	}
	void reset() {m_data.remove_all();}
private:

	t_entry & __add(const char * p_name) {
		return m_data.find_or_add(p_name);
	}

	t_content m_data;
};
