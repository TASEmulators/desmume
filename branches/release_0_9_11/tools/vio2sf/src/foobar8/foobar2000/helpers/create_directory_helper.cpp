#include "stdafx.h"
#include "create_directory_helper.h"

namespace create_directory_helper
{
	void create_directory_structure(const char * path)
	{
		mem_block_t<char> temp(strlen(path)+1);
		strcpy(temp,path);
		char * ptr = strstr(temp,":\\");
		if (ptr)
		{
			ptr+=2;
			while(*ptr)
			{
				if (*ptr == '\\')
				{
					*ptr = 0;
					file::g_create_directory(temp);
					*ptr = '\\';
				}
				ptr++;
			}
		}
	}

	static bool is_bad_dirchar(char c)
	{
		return c==' ' || c=='.';
	}

	void make_path(const char * parent,const char * filename,const char * extension,bool allow_new_dirs,string8 & out)
	{
		out.reset();
		if (parent && *parent)
		{
			out = parent;
			out.fix_dir_separator('\\');
		}
		bool last_char_is_dir_sep = true;
		while(*filename)
		{
#ifdef WIN32
			if (allow_new_dirs && is_bad_dirchar(*filename))
			{
				const char * ptr = filename+1;
				while(is_bad_dirchar(*ptr)) ptr++;
				if (*ptr!='\\' && *ptr!='/') out.add_string_n(filename,ptr-filename);
				filename = ptr;
				if (*filename==0) break;
			}
#endif
			if (is_path_bad_char(*filename))
			{
				if (allow_new_dirs && (*filename=='\\' || *filename=='/'))
				{
					if (!last_char_is_dir_sep)
					{
						out.add_char('\\');
						last_char_is_dir_sep = true;
					}
				}
				else
					out.add_char('_');
			}
			else
			{
				out.add_byte(*filename);
				last_char_is_dir_sep = false;
			}
			filename++;
		}
		if (out.length()>0 && out[out.length()-1]=='\\')
		{
			out.add_string("noname");
		}
		if (extension && *extension)
		{
			out.add_char('.');
			out.add_string(extension);
		}
	}
}

void create_directory_helper::format_filename(metadb_handle * handle,const char * spec,string8 & out,const char * extra)
{
	out = "";
	string8 temp;
	for(;;)
	{
		const char * ptr1 = strchr(spec,'\\'), *ptr2 = strchr(spec,'/');
		if ((!ptr1 && ptr2) || (ptr1 && ptr2 && ptr2<ptr1)) ptr1 = ptr2;
		if (ptr1)
		{
			handle->handle_format_title(temp,string_simple(spec,ptr1-spec),extra);
			temp.fix_filename_chars();
			spec = ptr1+1;
			out += temp;
			out += "\\";
		}
		else
		{
			handle->handle_format_title(temp,spec,extra);
			temp.fix_filename_chars();
			out += temp;
			break;
		}
	}
}
