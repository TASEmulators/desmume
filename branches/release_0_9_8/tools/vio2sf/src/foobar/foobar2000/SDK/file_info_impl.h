#ifndef _FOOBAR2000_SDK_FILE_INFO_IMPL_H_
#define _FOOBAR2000_SDK_FILE_INFO_IMPL_H_

namespace file_info_impl_utils {

	struct info_entry {
		void init(const char * p_name,t_size p_name_len,const char * p_value,t_size p_value_len) {
			m_name.set_string(p_name,p_name_len);
			m_value.set_string(p_value,p_value_len);
		}
		
		inline const char * get_name() const {return m_name;}
		inline const char * get_value() const {return m_value;}

		pfc::string_simple m_name,m_value;
	};

	typedef pfc::array_t<info_entry,pfc::alloc_fast> info_entry_array;

}

namespace pfc {
	template<> class traits_t<file_info_impl_utils::info_entry> : public traits_t<pfc::string_simple> {};
};


namespace file_info_impl_utils {
	class info_storage
	{
	public:
		t_size add_item(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length);
		void remove_mask(const bit_array & p_mask);	
		inline t_size get_count() const {return m_info.get_count();}
		inline const char * get_name(t_size p_index) const {return m_info[p_index].get_name();}
		inline const char * get_value(t_size p_index) const {return m_info[p_index].get_value();}
		void copy_from(const file_info & p_info);
	private:
		info_entry_array m_info;
	};
}


namespace file_info_impl_utils {
	typedef pfc::array_hybrid_t<pfc::string_simple,1,pfc::alloc_fast > meta_value_array;
	struct meta_entry {
		meta_entry() {}
		meta_entry(const char * p_name,t_size p_name_len,const char * p_value,t_size p_value_len);

		void remove_values(const bit_array & p_mask);
		void insert_value(t_size p_value_index,const char * p_value,t_size p_value_length);
		void modify_value(t_size p_value_index,const char * p_value,t_size p_value_length);

		inline const char * get_name() const {return m_name;}
		inline const char * get_value(t_size p_index) const {return m_values[p_index];}
		inline t_size get_value_count() const {return m_values.get_size();}
		

		pfc::string_simple m_name;
		meta_value_array m_values;
	};
	typedef pfc::array_hybrid_t<meta_entry,10, pfc::alloc_fast> meta_entry_array;
}
namespace pfc {
	template<> class traits_t<file_info_impl_utils::meta_entry> : public pfc::traits_combined<pfc::string_simple,file_info_impl_utils::meta_value_array> {};
}


namespace file_info_impl_utils {
	class meta_storage
	{
	public:
		t_size add_entry(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length);
		void insert_value(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length);
		void modify_value(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length);
		void remove_values(t_size p_index,const bit_array & p_mask);
		void remove_mask(const bit_array & p_mask);
		void copy_from(const file_info & p_info);

		inline void reorder(const t_size * p_order);

		inline t_size get_count() const {return m_data.get_size();}
		
		inline const char * get_name(t_size p_index) const {assert(p_index < m_data.get_size()); return m_data[p_index].get_name();}
		inline const char * get_value(t_size p_index,t_size p_value_index) const {assert(p_index < m_data.get_size()); return m_data[p_index].get_value(p_value_index);}
		inline t_size get_value_count(t_size p_index) const {assert(p_index < m_data.get_size()); return m_data[p_index].get_value_count();}

	private:
		meta_entry_array m_data;
	};
}

//! Implements file_info.
class file_info_impl : public file_info
{
public:
	file_info_impl(const file_info_impl & p_source);
	file_info_impl(const file_info & p_source);
	file_info_impl();
	~file_info_impl();

	double		get_length() const;
	void		set_length(double p_length);

	void		copy_meta(const file_info & p_source);//virtualized for performance reasons, can be faster in two-pass
	void		copy_info(const file_info & p_source);//virtualized for performance reasons, can be faster in two-pass
	
	t_size	meta_get_count() const;
	const char*	meta_enum_name(t_size p_index) const;
	t_size	meta_enum_value_count(t_size p_index) const;
	const char*	meta_enum_value(t_size p_index,t_size p_value_number) const;
	t_size	meta_set_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length);
	void		meta_insert_value_ex(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length);
	void		meta_remove_mask(const bit_array & p_mask);
	void		meta_reorder(const t_size * p_order);
	void		meta_remove_values(t_size p_index,const bit_array & p_mask);
	void		meta_modify_value_ex(t_size p_index,t_size p_value_index,const char * p_value,t_size p_value_length);

	t_size	info_get_count() const;
	const char*	info_enum_name(t_size p_index) const;
	const char*	info_enum_value(t_size p_index) const;
	t_size	info_set_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length);
	void		info_remove_mask(const bit_array & p_mask);

	const file_info_impl & operator=(const file_info_impl & p_source);

	replaygain_info	get_replaygain() const;
	void			set_replaygain(const replaygain_info & p_info);

protected:
	t_size	meta_set_nocheck_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length);
	t_size	info_set_nocheck_ex(const char * p_name,t_size p_name_length,const char * p_value,t_size p_value_length);
private:


	file_info_impl_utils::meta_storage m_meta;
	file_info_impl_utils::info_storage m_info;
	

	double m_length;

	replaygain_info m_replaygain;
};

#endif
