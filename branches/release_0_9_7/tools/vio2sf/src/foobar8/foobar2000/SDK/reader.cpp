#include "foobar2000.h"


reader * unpacker::g_open(reader * p)
{
	service_enum_t<unpacker> e;
	unpacker * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		p->seek(0);
		reader * r = ptr->open(p);
		ptr->service_release();
		if (r) return r;
	}
	return 0;

}

bool reader::seek2(__int64 position,int mode)
{
	switch(mode)
	{
	case SEEK_CUR:
		{
			__int64 delta = get_position();
			if (delta<0) return false;
			position+=delta;
		}
		break;
	case SEEK_END:
		{
			__int64 delta = get_length();
			if (delta<0) return false;
			position+=delta;
		}
		break;
	}
	return seek(position);
}

__int64 reader::transfer(reader * src,reader * dst,__int64 bytes)
{
	enum{BUFSIZE = 1024*1024};
	mem_block temp;
	void* ptr = temp.set_size((int)(BUFSIZE<bytes ? BUFSIZE : bytes));
	__int64 done = 0;
	while(done<bytes)
	{
		__int64 delta = bytes-done;
		if (delta>BUFSIZE) delta = BUFSIZE;
		delta = src->read(ptr,(int)delta);
		if (delta<=0) break;
		delta = dst->write(ptr,(int)delta);
		if (delta<=0) break;
		done += delta;
		
	}
	return done;
}


void file::g_get_canonical_path(const char * path,string_base & out)
{
	service_enum_t<file> e;
	file * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		int rv = ptr->get_canonical_path(path,out);
		ptr->service_release();
		if (rv) return;
	}
	//fucko: noone wants to process this, lets copy over
	out.set_string(path);
}

void file::g_get_display_path(const char * path,string_base & out)
{
	file * ptr = g_get_interface(path);
	if (ptr==0)
	{
		//fucko: noone wants to process this, lets copy over
		out.set_string(path);
	}
	else
	{
		if (!ptr->get_display_path(path,out))
			out.set_string(path);
		ptr->service_release();
	}
}

file * file::g_get_interface(const char * path)
{
	service_enum_t<file> e;
	file * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->is_our_path(path))
			return ptr;
		ptr->service_release();
	}
	return 0;
}

reader * file::g_get_reader(const char * path)
{
	file * ptr = g_get_interface(path);
	if (ptr==0) return 0;
	reader * r = ptr->get_reader(path);
	ptr->service_release();
	if (r==0) return 0;
	return r;
}

reader * file::g_open(const char * path,reader::MODE mode)
{
	string8 path_c;
	g_get_canonical_path(path,path_c);
	file * ptr = g_get_interface(path_c);
	if (ptr==0) return 0;
	reader * r = ptr->get_reader(path_c);
	ptr->service_release();
	if (r!=0)
	{
		if (r->open(path_c,mode)==0)
		{
			r->reader_release();
			r=0;
		}
	}
	return r;
}

int file::g_exists(const char * path)
{
	string8 path_c;
	g_get_canonical_path(path,path_c);
	file * ptr = g_get_interface(path_c);
	if (ptr==0) return 0;
	int rv = ptr->exists(path_c);
	ptr->service_release();
	return rv;
}

int file::g_remove(const char * path)
{
	string8 path_c;
	g_get_canonical_path(path,path_c);
	file * ptr = g_get_interface(path_c);
	if (ptr==0) return 0;
	int rv = ptr->remove(path_c);
	ptr->service_release();
	return rv;
}

int file::g_create_directory(const char * path)
{
	string8 path_c;
	g_get_canonical_path(path,path_c);
	file * ptr = g_get_interface(path_c);
	if (ptr==0) return 0;
	int rv = ptr->create_directory(path_c);
	ptr->service_release();
	return rv;
}

int file::g_move(const char * src,const char * dst)
{
	service_enum_t<file> e;
	file * ptr;
	int rv = 0;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->is_our_path(src) && ptr->is_our_path(dst))
		{
			rv = ptr->move(src,dst);
		}
		ptr->service_release();
		if (rv) break;
	}
	return rv;
}

int file::g_move_ex(const char * _src,const char * _dst)
{
	string8 src,dst;
	g_get_canonical_path(_src,src);
	g_get_canonical_path(_dst,dst);

	service_enum_t<file> e;
	file * ptr;
	int rv = 0;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		if (ptr->is_our_path(src) && ptr->is_our_path(dst))
		{
			rv = ptr->move(src,dst);
		}
		ptr->service_release();
		if (rv) break;
	}
	return rv;
}

int directory::g_list(const char * path,directory_callback * out,playlist_loader_callback * callback,bool b_recur)
{
	service_enum_t<directory> e;
	directory * d;
	for(d=e.first();d;d=e.next())
	{
		int rv;
		rv = d->list(path,out,callback,b_recur);
		d->service_release();
		if (rv) return 1;
		if (callback && !callback->on_progress(0)) break;
	}
	return 0;
}


static void path_pack_string(string8 & out,const char * src)
{
	out.add_char('|');
	out.add_int(strlen(src));
	out.add_char('|');
	out.add_string(src);
	out.add_char('|');
}

static int path_unpack_string(string8 & out,const char * src)
{
	int ptr=0;
	if (src[ptr++]!='|') return -1;
	int len = atoi(src+ptr);
	if (len<=0) return -1;
	while(src[ptr]!=0 && src[ptr]!='|') ptr++;
	if (src[ptr]!='|') return -1;
	ptr++;
	int start = ptr;
	while(ptr-start<len)
	{
		if (src[ptr]==0) return -1;
		ptr++;
	}
	if (src[ptr]!='|') return -1;
	out.add_string_n(&src[start],len);
	ptr++;	
	return ptr;
}


reader * file::g_open_precache(const char * path)
{
	string8 path_c;
	g_get_canonical_path(path,path_c);
	file * ptr = g_get_interface(path_c);
	if (ptr==0) return 0;
	if (ptr->dont_read_infos(path))
	{
		ptr->service_release();
		return 0;
	}
	reader * r = ptr->get_reader(path_c);
	ptr->service_release();
	if (r!=0)
	{
		if (r->open(path_c,reader::MODE_READ)==0)
		{
			r->reader_release();
			r=0;
		}
	}
	return r;
}

int file::g_dont_read_infos(const char * path)
{
	int rv = -1;
	file * ptr = g_get_interface(path);
	if (ptr)
	{
		rv = ptr->dont_read_infos(path);
		ptr->service_release();
	}
	return rv;
}



bool reader_backbuffer_wrap::seek(__int64 position)
{
	if (aborting) return false;
	if (position>reader_pos)
	{
		if (position > reader_pos + buffer.get_size())
		{
			reader_pos = position - buffer.get_size();
			m_reader->seek(reader_pos);
			buffered = 0;
		}

		read_pos = reader_pos;
		__int64 skip = (position-reader_pos);
		char temp[256];
		while(skip>0)
		{
			__int64 delta = sizeof(temp);
			if (delta>skip) delta=skip;
			if (read(temp,(int)delta)!=delta) return false;
			skip-=delta;
		}
		return true;
	}
	else if (reader_pos-position>(__int64)buffered)
	{
		read_pos = reader_pos = position;
		m_reader->seek(reader_pos);
		buffered = 0;
		return true;
	}
	else
	{
		read_pos = position;
		return true;
	}
}

int file::g_relative_path_create(const char * file_path,const char * playlist_path,string_base & out)
{
	file * ptr = g_get_interface(file_path);
	int rv = 0;
	if (ptr)
	{
		rv = ptr->relative_path_create(file_path,playlist_path,out);
		ptr->service_release();
	}
	return rv;
}

int file::g_relative_path_parse(const char * relative_path,const char * playlist_path,string_base & out)
{
	service_enum_t<file> e;
	file * ptr;
	for(ptr=e.first();ptr;ptr=e.next())
	{
		int rv = ptr->relative_path_parse(relative_path,playlist_path,out);
		ptr->service_release();
		if (rv) return rv;
	}
	return 0;
}



int archive_i::get_canonical_path(const char * path,string_base & out)
{
	if (is_our_path(path))
	{
		out.set_string(path);
		return 1;
	}
	else return 0;
}

int archive_i::is_our_path(const char * path)
{
	if (strncmp(path,"unpack://",9)) return 0;
	const char * type = get_archive_type();
	path += 9;
	while(*type)
	{
		if (*type!=*path) return 0;
		type++;
		path++;
	}
	if (*path!='|') return 0;
	return 1;
}

int archive_i::get_display_path(const char * path,string_base & out)
{
	string8 archive,file;
	if (parse_unpack_path(path,archive,file))
	{
		g_get_display_path(archive,out);
		out.add_string("|");
		out.add_string(file);
		return 1;
	}
	else return 0;
}

int archive_i::exists(const char * path)
{
	string8 archive,file;
	if (parse_unpack_path(path,archive,file))
	{
		if (g_exists(archive))
		{
			if (g_dont_read_infos(archive)) return 1;
			else return exists_in_archive(archive,file);
		}
	}
	return 0;
}

int archive_i::remove(const char * path)
{
	return 0;
}

int archive_i::move(const char * src,const char * dst)
{
	return 0;
}

int archive_i::dont_read_infos(const char * src)
{
	string8 archive,file;
	if (parse_unpack_path(src,archive,file))
	{
		return g_dont_read_infos(archive);
	}
	else return 1;
}

int archive_i::relative_path_create(const char * file_path,const char * playlist_path,string_base & out)
{
	string8 archive,file;
	if (parse_unpack_path(file_path,archive,file))
	{
		string8 archive_rel;
		if (g_relative_path_create(archive,playlist_path,archive_rel))
		{
			string8 out_path;
			make_unpack_path(out_path,archive_rel,file);
			out.set_string(out_path);
			return 1;
		}
	}
	return 0;
}

int archive_i::relative_path_parse(const char * relative_path,const char * playlist_path,string_base & out)
{
	if (!is_our_path(relative_path)) return 0;
	string8 archive_rel,file;
	if (parse_unpack_path(relative_path,archive_rel,file))
	{
		string8 archive;
		if (g_relative_path_parse(archive_rel,playlist_path,archive))
		{
			string8 out_path;
			make_unpack_path(out_path,archive,file);
			out.set_string(out_path);
			return 1;
		}
	}
	return 0;
}

// "unpack://zip|17|file://c:\unf.rar|meh.mp3"

bool archive_i::parse_unpack_path(const char * path,string8 & archive,string8 & file)
{
	path  = strchr(path,'|');
	if (!path) return false;
	int delta = path_unpack_string(archive,path);
	if (delta<0) return false;
	path += delta;
	file = path;
	return true;
}

void archive_i::make_unpack_path(string8 & path,const char * archive,const char * file,const char * name)
{
	path = "unpack://";
	path += name;
	path_pack_string(path,archive);
	path += file;
}

void archive_i::make_unpack_path(string8 & path,const char * archive,const char * file) {make_unpack_path(path,archive,file,get_archive_type());}


FILE * file::streamio_open(const char * path,const char * flags)
{
	FILE * ret = 0;
	string8 temp;
	g_get_canonical_path(path,temp);
	if (!strncmp(temp,"file://",7))
	{
		ret = _wfopen(string_wide_from_utf8(path+7),string_wide_from_utf8(flags));
	}
	return ret;
}

void reader::reader_release_safe()
{
	reader_release();
/*	try {
		if (this) reader_release();
	} catch(...) {}*/
}
#if 0

bool reader::read_byte(BYTE & val)
{
	return read(&val,sizeof(val))==sizeof(val);
}

bool reader::read_word(WORD & val)
{
	return read(&val,sizeof(val))==sizeof(val);
}

bool reader::read_dword(DWORD & val)
{
	return read(&val,sizeof(val))==sizeof(val);
}

bool reader::read_qword(QWORD & val)
{
	return read(&val,sizeof(val))==sizeof(val);
}

bool  reader::write_byte(BYTE val)
{
	return write(&val,sizeof(val))==sizeof(val);
}

bool  reader::write_word(WORD val)
{
	return write(&val,sizeof(val))==sizeof(val);
}

bool  reader::write_dword(DWORD val)
{
	return write(&val,sizeof(val))==sizeof(val);
}

bool  reader::write_qword(QWORD val)
{
	return write(&val,sizeof(val))==sizeof(val);
}

//endian helpers
bool reader::read_word_be(WORD &val)
{
	WORD temp;
	if (!read_word(temp)) return false;
	val = byte_order_helper::word_be_to_native(temp);
	return true;
}

bool reader::read_word_le(WORD &val)
{
	WORD temp;
	if (!read_word(temp)) return false;
	val = byte_order_helper::word_le_to_native(temp);
	return true;
}

bool reader::read_dword_be(DWORD &val)
{
	DWORD temp;
	if (!read_dword(temp)) return false;
	val = byte_order_helper::dword_be_to_native(temp);
	return true;
}
bool reader::read_dword_le(DWORD &val)
{
	DWORD temp;
	if (!read_dword(temp)) return false;
	val = byte_order_helper::dword_le_to_native(temp);
	return true;

}
bool reader::read_qword_be(QWORD &val)
{
	QWORD temp;
	if (!read_qword(temp)) return false;
	val = byte_order_helper::qword_be_to_native(temp);
	return true;
}

bool reader::read_qword_le(QWORD &val)
{
	QWORD temp;
	if (!read_qword(temp)) return false;
	val = byte_order_helper::qword_le_to_native(temp);
	return true;
}

bool reader::write_word_be(WORD val)
{
	return write_word(byte_order_helper::word_native_to_be(val));
}
bool reader::write_word_le(WORD val)
{
	return write_word(byte_order_helper::word_native_to_le(val));
}
bool reader::write_dword_be(DWORD val)
{
	return write_dword(byte_order_helper::dword_native_to_be(val));
}
bool reader::write_dword_le(DWORD val)
{
	return write_dword(byte_order_helper::dword_native_to_le(val));
}
bool reader::write_qword_be(QWORD val)
{
	return write_qword(byte_order_helper::qword_native_to_be(val));
}
bool reader::write_qword_le(QWORD val)
{
	return write_qword(byte_order_helper::qword_native_to_le(val));
}

#endif

bool reader::read_guid_le(GUID & g)
{
	if (read(&g,sizeof(g))!=sizeof(g)) return false;
	byte_order_helper::guid_le_to_native(g);
	return true;
}

bool reader::read_guid_be(GUID & g)
{
	if (read(&g,sizeof(g))!=sizeof(g)) return false;
	byte_order_helper::guid_be_to_native(g);
	return true;
}

bool reader::write_guid_le(const GUID & g)
{
	GUID temp = g;
	byte_order_helper::guid_native_to_le(temp);
	return write(&temp,sizeof(temp))==sizeof(temp);
}

bool reader::write_guid_be(const GUID & g)
{
	GUID temp = g;
	byte_order_helper::guid_native_to_be(temp);
	return write(&temp,sizeof(temp))==sizeof(temp);
}

__int64 reader::get_modification_time()
{
	__int64 rv = 0;
	reader_filetime * ptr = service_query_t(reader_filetime,this);
	if (ptr)
	{
		rv = ptr->get_modification_time();
		ptr->service_release();
	}
	return rv;
}

namespace {

	class directory_callback_isempty : public directory_callback
	{
		bool m_isempty;
	public:
		directory_callback_isempty() : m_isempty(true) {}
		void on_file(const char * url) {m_isempty = false;}//api fixme: should signal enumeration abort
		bool isempty() {return m_isempty;}
	};

	class directory_callback_dummy : public directory_callback
	{
	public:
		void on_file(const char * url) {}	
	};

}

bool directory::g_is_empty(const char * path)
{
	service_enum_t<directory> e;
	directory * d;
	for(d=e.first();d;d=e.next())
	{
		int rv;
		directory_callback_isempty callback;
		rv = d->list(path,&callback,0,true);//blargh @ old API not reporting directories inside, need to run in recur mode, fixme
		d->service_release();
		if (rv) return callback.isempty();
	}
	return true;
}

bool directory::g_is_valid_dir(const char * path)
{
	service_enum_t<directory> e;
	directory * d;
	for(d=e.first();d;d=e.next())
	{
		int rv;
		directory_callback_isempty callback;
		rv = d->list(path,&directory_callback_dummy(),0,false);//another API fixme
		d->service_release();
		if (rv) return true;
	}
	return false;
}