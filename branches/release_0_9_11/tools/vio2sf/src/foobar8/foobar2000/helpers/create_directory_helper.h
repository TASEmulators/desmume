#ifndef _CREATE_DIRECTORY_HELPER_H_
#define _CREATE_DIRECTORY_HELPER_H_

namespace create_directory_helper {
	void create_directory_structure(const char * path);
	void make_path(const char * parent,const char * filename,const char * extension,bool allow_new_dirs,string8 & out);
	void format_filename(class metadb_handle * handle,const char * spec,string8 & out,const char * extra=0);
};

#endif//_CREATE_DIRECTORY_HELPER_H_