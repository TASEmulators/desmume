class cfg_guidlist : public cfg_var, public pfc::list_t<GUID>
{
public:
	void get_data_raw(stream_writer * p_stream,abort_callback & p_abort) {
		t_uint32 n, m = pfc::downcast_guarded<t_uint32>(get_count());
		p_stream->write_lendian_t(m,p_abort);
		for(n=0;n<m;n++) p_stream->write_lendian_t(get_item(n),p_abort);
	}
	void set_data_raw(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) {
		t_uint32 n,count;
		p_stream->read_lendian_t(count,p_abort);
		m_buffer.set_size(count);
		for(n=0;n<count;n++) {
			try {
				p_stream->read_lendian_t(m_buffer[n],p_abort);
			} catch(...) {m_buffer.set_size(0); throw;}
		}
	}

	void sort() {sort_t(pfc::guid_compare);}

	bool have_item_bsearch(const GUID & p_item) {
		t_size dummy;
		return bsearch_t(pfc::guid_compare,p_item,dummy);
	}

public:
	cfg_guidlist(const GUID & p_guid) : cfg_var(p_guid) {}
};
