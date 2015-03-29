#define __file_info_const_impl_have_hintmap__

//! Special implementation of file_info that implements only const and copy methods. The difference between this and regular file_info_impl is amount of resources used and speed of the copy operation.
class file_info_const_impl : public file_info
{
public:
	file_info_const_impl(const file_info & p_source) {copy(p_source);}
	file_info_const_impl(const file_info_const_impl & p_source) {copy(p_source);}
	file_info_const_impl() {m_meta_count = m_info_count = 0; m_length = 0; m_replaygain.reset();}

	double		get_length() const {return m_length;}
	
	t_size		meta_get_count() const {return m_meta_count;}
	const char*	meta_enum_name(t_size p_index) const {return m_meta[p_index].m_name;}
	t_size		meta_enum_value_count(t_size p_index) const;
	const char*	meta_enum_value(t_size p_index,t_size p_value_number) const;
	t_size		meta_find_ex(const char * p_name,t_size p_name_length) const;

	t_size		info_get_count() const {return m_info_count;}
	const char*	info_enum_name(t_size p_index) const {return m_info[p_index].m_name;}
	const char*	info_enum_value(t_size p_index) const {return m_info[p_index].m_value;}


	const file_info_const_impl & operator=(const file_info & p_source) {copy(p_source); return *this;}
	const file_info_const_impl & operator=(const file_info_const_impl & p_source) {copy(p_source); return *this;}
	void copy(const file_info & p_source);
	void reset();

	replaygain_info	get_replaygain() const {return m_replaygain;}

private:
	void		set_length(double p_length) {throw pfc::exception_bug_check_v2();}

	t_size		meta_set_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length) {throw pfc::exception_bug_check_v2();}
	void		meta_insert_value_ex(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length) {throw pfc::exception_bug_check_v2();}
	void		meta_remove_mask(const bit_array & p_mask) {throw pfc::exception_bug_check_v2();}
	void		meta_reorder(const t_size * p_order) {throw pfc::exception_bug_check_v2();}
	void		meta_remove_values(t_size p_index,const bit_array & p_mask) {throw pfc::exception_bug_check_v2();}
	void		meta_modify_value_ex(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length) {throw pfc::exception_bug_check_v2();}

	t_size		info_set_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length) {throw pfc::exception_bug_check_v2();}
	void		info_remove_mask(const bit_array & p_mask) {throw pfc::exception_bug_check_v2();}

	void			set_replaygain(const replaygain_info & p_info) {throw pfc::exception_bug_check_v2();}
	
	t_size		meta_set_nocheck_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length) {throw pfc::exception_bug_check_v2();}
	t_size		info_set_nocheck_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length) {throw pfc::exception_bug_check_v2();}
public:
	struct meta_entry {
		const char * m_name;
		t_size m_valuecount;
		const char * const * m_valuemap;
	};

	struct info_entry {
		const char * m_name;
		const char * m_value;
	};

#ifdef __file_info_const_impl_have_hintmap__
	typedef t_uint32 t_index;
	enum {hintmap_cutoff = 20};
#endif//__file_info_const_impl_have_hintmap__
private:
	pfc::array_t<char> m_buffer;
	t_index m_meta_count;
	t_index m_info_count;
	
	const meta_entry * m_meta;
	const info_entry * m_info;

#ifdef __file_info_const_impl_have_hintmap__
	const t_index * m_hintmap;
#endif

	double m_length;
	replaygain_info m_replaygain;
};
