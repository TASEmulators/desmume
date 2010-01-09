class uGetOpenFileNameMultiResult_impl : public uGetOpenFileNameMultiResult {
	pfc::list_t<pfc::string> m_data;
public:
	void AddItem(pfc::stringp param) {m_data.add_item(param);}
	t_size get_count() const {return m_data.get_count();}
	void get_item_ex(const char * & out,t_size n) const {out = m_data[n].ptr();}
};
