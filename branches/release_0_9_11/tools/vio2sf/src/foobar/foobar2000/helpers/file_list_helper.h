#ifndef _foobar2000_helpers_file_list_helper_
#define _foobar2000_helpers_file_list_helper_


namespace file_list_helper
{
	//list guaranteed to be sorted by metadb::path_compare
	class file_list_from_metadb_handle_list : public pfc::list_base_const_t<const char*> {
	public:

		static t_size g_get_count(const list_base_const_t<metadb_handle_ptr> & p_list, t_size max = ~0);

		void init_from_list(const list_base_const_t<metadb_handle_ptr> & p_list);
		void init_from_list_display(const list_base_const_t<metadb_handle_ptr> & p_list);

		t_size get_count() const;
		void get_item_ex(const char * & p_out,t_size n) const;

		~file_list_from_metadb_handle_list();

	private:
		void __add(const char * p_what);
		pfc::ptr_list_t<char> m_data;
	};


};


#endif //_foobar2000_helpers_file_list_helper_