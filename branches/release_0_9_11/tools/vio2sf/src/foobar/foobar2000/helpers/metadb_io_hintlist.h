
class metadb_io_hintlist {
public:
	void hint_reader(service_ptr_t<input_info_reader> p_reader, const char * p_path,abort_callback & p_abort);
	void add(metadb_handle_ptr const & p_handle,const file_info & p_info,t_filestats const & p_stats,bool p_fresh);
	void run();
	t_size get_pending_count() const {return m_entries.get_count();}
private:
	struct t_entry {
		metadb_handle_ptr m_handle;
		pfc::rcptr_t<file_info_const_impl> m_info;
		t_filestats m_stats;
		bool m_fresh;
	};
	class metadb_io_hintlist_wrapper_part1 : public pfc::list_base_const_t<metadb_handle_ptr> {
	public:
		metadb_io_hintlist_wrapper_part1(const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & p_list) : m_list(p_list) {}
		t_size get_count() const {return m_list.get_count();}
		void get_item_ex(metadb_handle_ptr & p_out, t_size n) const {p_out = m_list[n].m_handle;}

	private:
		const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & m_list;
	};
	class metadb_io_hintlist_wrapper_part2 : public pfc::list_base_const_t<const file_info*> {
	public:
		metadb_io_hintlist_wrapper_part2(const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & p_list) : m_list(p_list) {}
		t_size get_count() const {return m_list.get_count();}
		void get_item_ex(const file_info* & p_out, t_size n) const {p_out = &*m_list[n].m_info;}
	private:
		const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & m_list;
	};
	class metadb_io_hintlist_wrapper_part3 : public pfc::list_base_const_t<t_filestats> {
	public:
		metadb_io_hintlist_wrapper_part3(const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & p_list) : m_list(p_list) {}
		t_size get_count() const {return m_list.get_count();}
		void get_item_ex(t_filestats & p_out, t_size n) const {p_out = m_list[n].m_stats;}
	private:
		const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & m_list;
	};
	class metadb_io_hintlist_wrapper_part4 : public bit_array {
	public:
		metadb_io_hintlist_wrapper_part4(const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & p_list) : m_list(p_list) {}
		bool get(t_size n) const {return m_list[n].m_fresh;}
	private:
		const pfc::list_base_const_t<metadb_io_hintlist::t_entry> & m_list;
	};

	pfc::list_t<t_entry,pfc::alloc_fast> m_entries;
};

