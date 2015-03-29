#include "StdAfx.h"

static const unsigned char utf8_header[3] = {0xEF,0xBB,0xBF};

namespace text_file_loader
{
	void write(const service_ptr_t<file> & p_file,abort_callback & p_abort,const char * p_string,bool is_utf8)
	{
		p_file->seek(0,p_abort);
		p_file->set_eof(p_abort);
		if (is_utf8)
		{
			p_file->write_object(utf8_header,sizeof(utf8_header),p_abort);
			p_file->write_object(p_string,strlen(p_string),p_abort);
		}
		else
		{
			pfc::stringcvt::string_ansi_from_utf8 bah(p_string);
			p_file->write_object(bah,bah.length(),p_abort);
		}
	}

	void read(const service_ptr_t<file> & p_file,abort_callback & p_abort,pfc::string_base & p_out,bool & is_utf8) {
		p_out.reset();
		if (p_file->can_seek())
		{
			p_file->seek(0,p_abort);
		}
		
		pfc::array_t<char> mem;
		t_filesize size64;
		size64 = p_file->get_size(p_abort);
		if (size64 == filesize_invalid)//typically HTTP
		{
			pfc::string8 ansitemp;
			is_utf8 = false;
			enum {delta = 1024*64, max = 1024*512};
			char temp[3];
			t_size done;
			done = p_file->read(temp,3,p_abort);
			if (done != 3)
			{
				if (done > 0) p_out = pfc::stringcvt::string_utf8_from_ansi(temp,done);
				return;
			}
			if (!memcmp(utf8_header,temp,3)) is_utf8 = true;
			else ansitemp.add_string(temp,3);

			mem.set_size(delta);
			
			for(;;)
			{
				done = p_file->read(mem.get_ptr(),delta,p_abort);
				if (done > 0)
				{
					if (is_utf8) p_out.add_string(mem.get_ptr(),done);
					else ansitemp.add_string(mem.get_ptr(),done);
				}
				if (done < delta) break;
			}

			if (!is_utf8)
			{
				p_out = pfc::stringcvt::string_utf8_from_ansi(ansitemp);
			}

			return;
		}
		else
		{
			if (size64>1024*1024*128) throw exception_io_data();//hard limit
			t_size size = pfc::downcast_guarded<t_size>(size64);
			mem.set_size(size+1);
			char * asdf = mem.get_ptr();
			p_file->read_object(asdf,size,p_abort);
			asdf[size]=0;
			if (size>3 && !memcmp(utf8_header,asdf,3)) {is_utf8 = true; p_out.add_string(asdf+3); }
			else {
				is_utf8 = false;
				p_out = pfc::stringcvt::string_utf8_from_ansi(asdf);
			}
			return;
		}
	}

	void write(const char * p_path,abort_callback & p_abort,const char * p_string,bool is_utf8)
	{
		service_ptr_t<file> f;
		filesystem::g_open_write_new(f,p_path,p_abort);
		write(f,p_abort,p_string,is_utf8);
	}

	void read(const char * p_path,abort_callback & p_abort,pfc::string_base & p_out,bool & is_utf8)
	{
		service_ptr_t<file> f;
		filesystem::g_open_read(f,p_path,p_abort);
		read(f,p_abort,p_out,is_utf8);
	}

}