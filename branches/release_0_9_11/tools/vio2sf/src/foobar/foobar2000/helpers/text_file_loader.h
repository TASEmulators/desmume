namespace text_file_loader
{
	void write(const service_ptr_t<file> & p_file,abort_callback & p_abort,const char * p_string,bool is_utf8);
	void read(const service_ptr_t<file> & p_file,abort_callback & p_abort,pfc::string_base & p_out,bool & is_utf8);

	void write(const char * p_path,abort_callback & p_abort,const char * p_string,bool is_utf8);
	void read(const char * p_path,abort_callback & p_abort,pfc::string_base & p_out,bool & is_utf8);

};