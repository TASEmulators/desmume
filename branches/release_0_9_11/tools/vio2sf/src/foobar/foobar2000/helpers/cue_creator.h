namespace cue_creator
{
	struct t_entry
	{
		file_info_impl m_infos;
		pfc::string8 m_file,m_flags;
		unsigned m_track_number;

		t_cuesheet_index_list m_index_list;

		void set_simple_index(double p_time);
	};
	
	typedef pfc::chain_list_v2_t<t_entry> t_entry_list;

	void create(pfc::string_formatter & p_out,const t_entry_list & p_list);
};