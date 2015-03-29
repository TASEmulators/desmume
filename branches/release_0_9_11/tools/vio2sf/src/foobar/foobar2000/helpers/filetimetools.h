namespace foobar2000_io {
	t_filetimestamp filetimestamp_from_string(const char * date);

	//! Warning: this formats according to system timezone settings, created strings should be used for display only, never for storage.
	class format_filetimestamp {
	public:
		format_filetimestamp(t_filetimestamp p_timestamp);
		operator const char*() const {return m_buffer;}
		const char * get_ptr() const {return m_buffer;}
	private:
		pfc::string_fixed_t<32> m_buffer;
	};

}
