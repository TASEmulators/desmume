#ifndef _CREATE_DIRECTORY_HELPER_H_
#define _CREATE_DIRECTORY_HELPER_H_

namespace create_directory_helper {
	void create_path(const char * p_path,abort_callback & p_abort);
	void make_path(const char * parent,const char * filename,const char * extension,bool allow_new_dirs,pfc::string8 & out,bool b_really_create_dirs,abort_callback & p_dir_create_abort);
	void format_filename(const metadb_handle_ptr & handle,titleformat_hook * p_hook,const char * spec,pfc::string8 & out);
};

#endif//_CREATE_DIRECTORY_HELPER_H_