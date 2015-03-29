#ifndef _READER_H_
#define _READER_H_

#include "service.h"
#include "interface_helper.h"

class playlist_loader_callback;

//file paths are all UTF-8

class NOVTABLE reader : public service_base
{
protected:
	virtual ~reader() {};
	reader() {};
public:
	enum MODE {
		MODE_READ = 1,//open with READ access and READ-only sharing
		MODE_WRITE_EXISTING,//open existing file with full R/W access and NO sharing
		MODE_WRITE_NEW,//create new file with full R/W access and NO sharing
	};

	virtual bool open(const char *path,MODE mode)=0;
	//open should be called ONLY ONCE on a reader instance, with the same path as passed to file::get_reader
	//you MAY get deleted before open()

	virtual unsigned read(void *buffer, unsigned length)=0;	//0 on error/eof, number of bytes read otherwise (<length if eof)
	virtual unsigned write(const void *buffer, unsigned length)=0;	//returns amount of bytes written
	virtual __int64 get_length()=0; //-1 if nonseekable/unknown
	virtual __int64 get_position()=0;	//-1 if nonseekable/unknown
	virtual bool set_eof() {return false;}
	virtual bool seek(__int64 position)=0;//returns 1 on success, 0 on failure
	virtual bool seek2(__int64 position,int mode);//helper, implemented in reader.cpp, no need to override
	virtual bool can_seek() {return true;}//if false, all seek calls will fail
	virtual void abort() {} //aborts current operation (eg. when stopping a http stream), async call, ensure multithread safety on your side
	virtual bool get_content_type(string_base & out) {return 0;}//mime type, return 1 on success, 0 if not supported
	virtual bool is_in_memory() {return 0;}//return 1 if your file is fully buffered in memory
	virtual void on_idle() {}
	virtual bool error() {return false;}

	virtual void reader_release() {service_release();}
	void reader_release_safe();

	__int64 get_modification_time();

	//helper
	static __int64 transfer(reader * src,reader * dst,__int64 bytes);

	
	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}//only for service_query compatibility

	template<class T>
	bool read_x_le(T & val)
	{
		if (read(&val,sizeof(val))!=sizeof(val)) return false;
		byte_order_helper::order_le_to_native(&val,sizeof(val));
		return true;
	}

	template<class T>
	bool read_x_be(T & val)
	{
		if (read(&val,sizeof(val))!=sizeof(val)) return false;
		byte_order_helper::order_be_to_native(&val,sizeof(val));
		return true;
	}

	template<class T>
	bool write_x_le(T val)
	{
		byte_order_helper::order_native_to_le(&val,sizeof(val));
		return write(&val,sizeof(val))==sizeof(val);
	}

	template<class T>
	bool write_x_be(T val)
	{
		byte_order_helper::order_native_to_be(&val,sizeof(val));
		return write(&val,sizeof(val))==sizeof(val);
	}

	//endian helpers
	inline bool read_word_be(WORD & val) {return read_x_be(val);}
	inline bool read_word_le(WORD & val) {return read_x_le(val);}
	inline bool read_dword_be(DWORD & val) {return read_x_be(val);}
	inline bool read_dword_le(DWORD & val) {return read_x_le(val);}
	inline bool read_qword_be(QWORD & val) {return read_x_be(val);}
	inline bool read_qword_le(QWORD & val) {return read_x_le(val);}
	inline bool read_float_be(float & val) {return read_x_be(val);}
	inline bool read_float_le(float & val) {return read_x_le(val);}
	inline bool read_double_be(double & val) {return read_x_be(val);}
	inline bool read_double_le(double & val) {return read_x_le(val);}
	inline bool write_word_be(WORD val) {return write_x_be(val);}
	inline bool write_word_le(WORD val) {return write_x_le(val);}
	inline bool write_dword_be(DWORD val) {return write_x_be(val);}
	inline bool write_dword_le(DWORD val) {return write_x_le(val);}
	inline bool write_qword_be(QWORD val) {return write_x_be(val);}
	inline bool write_qword_le(QWORD val) {return write_x_le(val);}
	inline bool write_float_be(float val) {return write_x_be(val);}
	inline bool write_float_le(float val) {return write_x_le(val);}
	inline bool write_double_be(double val) {return write_x_be(val);}
	inline bool write_double_le(double val) {return write_x_le(val);}
	bool read_guid_le(GUID & g);
	bool read_guid_be(GUID & g);
	bool write_guid_le(const GUID & g);
	bool write_guid_be(const GUID & g);
};

class reader_filetime : public reader
{
public:
	//time is a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601; 0 for invalid/unknown time
	virtual __int64 get_modification_time()=0;

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return reader::service_query(guid);
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}//only for service_query

};

class reader_dynamicinfo : public reader//for shoutcast metadata BS
{
public:

	virtual bool is_dynamic_info_enabled()=0;//checks if currently open stream gets dynamic metadata

	virtual bool get_dynamic_info(class file_info * out, bool * b_track_change)=0;//see input::get_dynamic_info

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return reader::service_query(guid);
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}//only for service_query
};

class NOVTABLE file : public service_base
{
public:
	virtual int get_canonical_path(const char * path,string_base & out)=0;//return 1 on success, 0 on failure / not our file
	virtual int is_our_path(const char * path)=0;

	virtual int get_display_path(const char * path,string_base & out)=0;
	//converts canonical path to "human-readable" path (eg. removes file:// )
	//return 1 on success, 0 on failure
	//generally OK to leave it returning 0

	//in in all subsequent calls other than get_canonical_path, you should expect file path that has been accepted by your is_our_path

	//everything else than get_canonical_path() should expect strings that have already been processed by get_canonical_path()
	//get_canonical_path() is called WITHOUT calling is_our_file first
	virtual reader * get_reader(const char * path)=0;//note: reader will usually stay alive after you are service_release'd; do not open the file at this point, just create a reader capable of opening this file and return it
	virtual int exists(const char * path)=0;//for return values, see FILE_EXISTS* below
	virtual int remove(const char * path)=0;//return 1 on success and 0 on failure; also removes directories
	virtual int move(const char * src,const char * dst)=0;
	virtual int dont_read_infos(const char * src) {return 0;}//return 1 if infos shouldn't be precached from you (eg. HTTP)

	virtual int relative_path_create(const char * file_path,const char * playlist_path,string_base & out) {return 0;}
	//eg. file_path : "file://z:\dir\subdir\blah.mpc", playlist_path: "file://z:\dir\playlist.fpl", out: "file://subdir\blah.mpc"
	//file_path == canonical path accepted by your class, playlist_path == canonical path possibly belonging to some other file class, set out to a string your relative_path_parse will recognize (use url-style notation to prevent confusion)
	//dont worry about output string being possibly passed to anything else than relative_path_parse
	//return 1 on success, 0 if relative path can't be created (will store as full path)
	virtual int relative_path_parse(const char * relative_path,const char * playlist_path,string_base & out) {return 0;}
	//eg. relative_path: "file://subdir\blah.mpc", playlist_path: "file://z:\dir\playlist.fpl", out: "file://z:\dir\subdir\blah.mpc"
	//relative_path == relative path produced by some file class (possibly you), check it before processing)
	//output canonical path to the file on success
	//return: 1 on success, 0 on failure / not-our-file

	virtual int create_directory(const char * path) {return 0;}
	
	//helpers
	static void g_get_canonical_path(const char * path,string_base & out);
	static void g_get_display_path(const char * path,string_base & out);

	static file * g_get_interface(const char * path);//path is AFTER get_canonical_path
	static reader * g_get_reader(const char * path);//path is AFTER get_canonical_path (you're gonna need canonical version for reader::open() anyway)
	static int g_dont_read_infos(const char * path);//path is AFTER get_canonical_path
	//these below do get_canonical_path internally
	static reader * g_open(const char * path,reader::MODE mode);//get_canonical_path + get_reader + reader->open; slow with http etc (you cant abort it)
	static reader * g_open_read(const char * path) {return g_open(path,reader::MODE_READ);}
	static reader * g_open_precache(const char * path);//open only for precaching data (eg. will fail on http etc)
	static int g_exists(const char * path);
	static int g_remove(const char * path);
	static int g_move(const char * src,const char * dst);//needs canonical path
	static int g_move_ex(const char * src,const char * dst);//converts to canonical path first
	static int g_relative_path_create(const char * file_path,const char * playlist_path,string_base & out);
	static int g_relative_path_parse(const char * relative_path,const char * playlist_path,string_base & out);
	
	static int g_create_directory(const char * path);

	static FILE * streamio_open(const char * path,const char * flags); // if for some bloody reason you ever need stream io compatibility, use this, INSTEAD of calling fopen() on the path string you've got; will only work with file:// (and not with http://, unpack:// or whatever)

	inline static reader * g_open_temp() {return g_open("tempfile://",reader::MODE_WRITE_NEW);}
	inline static reader * g_open_tempmem()
	{
		reader * r = g_open("tempmem://",reader::MODE_WRITE_NEW);
		if (r==0) r = g_open_temp();
		return r;
	}

	enum {
		FILE_EXISTS = 1,//use as flags, FILE_EXISTS_WRITEABLE implies FILE_EXISTS
		FILE_EXISTS_WRITEABLE = 2
	};
	

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE directory_callback
{
public:
	virtual void on_file(const char * url)=0;
};

class directory_callback_i : public directory_callback
{
	ptr_list_t<char> data;
	static int sortfunc(const char * & p1, const char * & p2) {return stricmp_utf8(p1,p2);}
public:
	virtual void on_file(const char * url) {data.add_item(strdup(url));}
	~directory_callback_i() {data.free_all();}
	unsigned get_count() {return data.get_count();}
	const char * operator[](unsigned n) {return data[n];}
	const char * get_item(unsigned n) {return data[n];}
	void sort() {data.sort(sortfunc);}
};

class NOVTABLE directory : public service_base
{
public:
	virtual int list(const char * path,directory_callback * out,playlist_loader_callback * callback,bool b_recur = true)=0;

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
	//helper
	static int g_list(const char * path,directory_callback * out,playlist_loader_callback * callback,bool b_recur = true);
	//playlist_loader_callback pointer is optional and used only for querying if enumeration should be aborted

	static bool g_is_valid_dir(const char * path);
	static bool g_is_empty(const char * path);
};

class NOVTABLE archive : public file//dont derive from this, use archive_i class below
{
public:
	virtual int list(const char * path,directory_callback * out,playlist_loader_callback * callback)=0;
	virtual int precache(const char * path,playlist_loader_callback * callback) {return 0;} //optional - if you are slow (eg. rar), metadb::precache() your files in optimal way using supplied reader pointer
	//playlist_loader_callback ONLY for on_progress calls

	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE archive_i : public archive // derive from this
{
private:
	//do not override these
	virtual int get_canonical_path(const char * path,string_base & out);
	virtual int is_our_path(const char * path);
	virtual int get_display_path(const char * path,string_base & out);
	virtual int exists(const char * path);
	virtual int remove(const char * path);
	virtual int move(const char * src,const char * dst);
	virtual int dont_read_infos(const char * src);
	virtual int relative_path_create(const char * file_path,const char * playlist_path,string_base & out);
	virtual int relative_path_parse(const char * relative_path,const char * playlist_path,string_base & out);
protected:
	//override these
	virtual const char * get_archive_type()=0;//eg. "zip", must be lowercase
	virtual int exists_in_archive(const char * archive,const char * file) {return 1;}

public:
	//override these
	virtual reader * get_reader(const char * path)=0;
	virtual int list(const char * path,directory_callback * out,playlist_loader_callback * callback)=0;
	virtual int precache(const char * path,playlist_loader_callback * callback) {return 0;}//optional - if you are slow (eg. rar), metadb::precache() your files in optimal way using supplied reader pointer
	//playlist_loader_callback ONLY for on_progress calls


	static bool parse_unpack_path(const char * path,string8 & archive,string8 & file);
	static void make_unpack_path(string8 & path,const char * archive,const char * file,const char * name);
	void make_unpack_path(string8 & path,const char * archive,const char * file);

};

template<class T>
class archive_factory
{
	service_factory_single_t<file,T> factory1;
	service_factory_single_t<archive,T> factory2;
public:
	archive_factory() {}	
	T& get_static_instance() {return factory1.get_static_instance();}
};

//register as:
// static archive_factory<archive_foo> foo;


#include "reader_helper.h"

#endif