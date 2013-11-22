#include "stdafx.h"

static void file_list_remove_duplicates(pfc::ptr_list_t<char> & out)
{
	t_size n, m = out.get_count();
	out.sort_t(metadb::path_compare);
	bit_array_bittable mask(m);
	t_size duplicates = 0;
	for(n=1;n<m;n++) {
		if (!metadb::path_compare(out[n-1],out[n])) {duplicates++;mask.set(n,true);}
	}
	if (duplicates>0) {
		out.free_mask(mask);
	}
}


namespace file_list_helper
{
	t_size file_list_from_metadb_handle_list::g_get_count(metadb_handle_list_cref data, t_size max) {
		pfc::avltree_t<const char*, metadb::path_comparator> content;
		const t_size inCount = data.get_size();
		for(t_size walk = 0; walk < inCount && content.get_count() < max; ++walk) {
			content += data[walk]->get_path();
		}
		return content.get_count();
	}
	void file_list_from_metadb_handle_list::__add(const char * p_what) {
		char * temp = _strdup(p_what);
		if (temp == NULL) throw std::bad_alloc();
		try {m_data.add_item(temp); } catch(...) {free(temp); throw;}
	}

	void file_list_from_metadb_handle_list::init_from_list(const list_base_const_t<metadb_handle_ptr> & p_list)
	{
		m_data.free_all();

		t_size n, m = p_list.get_count();
		for(n=0;n<m;n++) {
			__add( p_list.get_item(n)->get_path() );
		}
		file_list_remove_duplicates(m_data);
	}

	void file_list_from_metadb_handle_list::init_from_list_display(const list_base_const_t<metadb_handle_ptr> & p_list)
	{
		m_data.free_all();
		
		pfc::string8_fastalloc temp;

		t_size n, m = p_list.get_count();
		for(n=0;n<m;n++)
		{
			filesystem::g_get_display_path(p_list.get_item(n)->get_path(),temp);
			__add(temp);
		}
		file_list_remove_duplicates(m_data);


	}

	t_size file_list_from_metadb_handle_list::get_count() const {return m_data.get_count();}
	void file_list_from_metadb_handle_list::get_item_ex(const char * & p_out,t_size n) const {p_out = m_data.get_item(n);}

	file_list_from_metadb_handle_list::~file_list_from_metadb_handle_list()
	{
		m_data.free_all();
	}

}