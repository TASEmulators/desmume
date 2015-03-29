#ifndef _INPUT_H_
#define _INPUT_H_

#include "service.h"

#include "reader.h"
#include "file_info.h"

#include "audio_chunk.h"

/***************************
how input class is used

  case 0: checking filename
  create => test_filename() => destroy

  case 1: info reading
  create => test_filename() => open(r,info,OPEN_FLAG_GET_INFO) => destroy

  case 2: playback
  create => testfilename() => open(r,info,OPEN_FLAG_DECODE | [blah blah blah]) => run()  .... => destroy
  note: you must fill file_info stuff too if you get OPEN_FLAG_GET_INFO

  case 3: info writing
  create => testfilename() => set_info(r,info) => destroy


  there's no possibility of opening different files from the same input instance.

  when you get set_info() call, you have exclusive write access to the file; if user attempts to update currently played file, fb2k will wait until the file is free

  NOTE: when trying to decode unseekable source (reader->can_seek() returns 0), you should expect the reader you get to be able to seek back up to 64k


  REMINDER: ALL strings are UTF-8


***************************/

class NOVTABLE input : public service_base
{
public:
	enum
	{
		OPEN_FLAG_GET_INFO = 1,//if specified, you must pass all metadata/etc to file_info, otherwise you dont need to pass any data to file_info (but it wont blow up if you do, it will be just ignored)
		OPEN_FLAG_DECODE = 2,//open for decoding
		OPEN_FLAG_NO_SEEKING = 4,//when combined with OPEN_FLAG_DECODE, informs you that you don't need to be able to seek (still need to extract track length though)
		OPEN_FLAG_NO_LOOPING = 8,//when combined with OPEN_FLAG_DECODE, you're being replaygainscanned or something, if your input can decode indefinitely, disable that
	};

	//you should expect either OPEN_FLAG_GET_INFO alone, or OPEN_FLAG_DECODE (possibly with modifiers), or both OPEN_FLAG_GET_INFO and OPEN_FLAG_DECODE (extract info and start decoding)

	enum set_info_t
	{
		SET_INFO_SUCCESS,
		SET_INFO_FAILURE,
		SET_INFO_BUSY,
	};

	virtual bool open(reader * r,file_info * info,unsigned flags)=0;
	//caller is responsible for deleting the reader; reader pointer is valid until you get deleted; pass all your metadata/etc to info
	//flags => see OPEN_FLAG_* above
	//lazy solution: ignore flags, fill info and set up decoder
	//somewhat more efficient solution: avoid either setting up decoder or filling info depending on flags

	virtual bool is_our_content_type(const char * url,const char * type) {return 0;}//for mime type checks, before test_filename
	virtual bool test_filename(const char * full_path,const char * extension)=0; //perform extension/filename tests, return 1 if the file might be one of our types; do ONLY file extension checks etc, no opening; doesn't hurt if you return 1 for a file that doesn't actually belong to you
	

	virtual set_info_t set_info(reader *r,const file_info * info)=0;//reader with exclusive write access

	virtual bool needs_reader() {return true;}//return if you read files or not (if you return 0, reader pointer in open/set_info will be null
	virtual int run(audio_chunk * chunk)=0;
	// return 1 on success, -1 on failure, 0 on EOF

	virtual bool seek(double seconds)=0;//return 1 on success, 0 on failure; if EOF, return 1 and return 0 in next run() pass

	virtual bool can_seek() {return true;}//return 0 if the file you're playing can't be seeked

	virtual void abort() {} //async call, abort current seek()/run(), ensure multithread safety on your side

	virtual bool get_dynamic_info(file_info * out, double * timestamp_delta,bool * b_track_change) {return false;}
	//for dynamic song titles / VBR bitrate / etc
	//out initially contains currently displayed info (either last get_dynamic_info result or current database info)
	//timestamp_delta - you can use it to tell the core when this info should be displayed (in seconds, relative to first sample of last decoded chunk), initially set to 0
	//get_dynamic_info is called after each run() (or not called at all if caller doesn't care about dynamic info)
	//return false to keep old info, or true to modify it
	//set b_track_change to true if you want to indicate new song
	//please keep in mind that updating dynamic info (returning true from this func) generates some overhead (mallocing to pass new info around, play_callback calls, GUI updates and titleformatting), ; if you implement useless features like realtime vbr bitrate display, make sure that they are optional and disabled by default

	static input* g_open(reader * r,file_info * info,unsigned flags,__int64 * modtime = 0,__int64 * filesize=0,bool * new_info=0);
	static bool g_test_filename(const char * fn);
	static bool g_get_info(file_info * info,reader * r = 0,__int64 * modtime = 0,__int64 * filesize=0);
	static bool g_check_info(file_info * info,reader * r = 0,__int64 * modtime = 0,__int64 * filesize=0);//loads info if newer
	static set_info_t g_set_info_readerless(const file_info * info,__int64 * modtime = 0,__int64 * filesize=0);
	static set_info_t g_set_info_reader(const file_info * info,reader * r, __int64 * modtime = 0,__int64 * filesize=0);
	static set_info_t g_set_info(const file_info * info,reader * r = 0,__int64 * modtime = 0,__int64 * filesize=0);
	static bool is_entry_dead(const playable_location * entry);//tests if playlist_entry appears to be alive or not

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	bool open_ex(reader * r,file_info * info,unsigned flags,__int64 * modification_time,__int64 * file_size,bool * new_info);
	set_info_t set_info_ex(reader *r,const file_info * info,__int64 * modtime,__int64 * filesize);//reader with exclusive write access
	bool is_reopen_safe();
};

class input_v2 : public input//extension for readerless operations in 0.8, to allow modification_time processing
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return input::service_query(guid);
	}


	//time is a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601; 0 for invalid/unknown time
	virtual bool open_ex(reader * r,file_info * info,unsigned flags,__int64 * modification_time,__int64 * file_size,bool * new_info);
	//if modification_time pointer is null, open as usual
	//otherwise, if file time is different than *modification_time, set *modification_time to modification time and proceed as if flags had OPEN_FLAG_GET_INFO
	//if new_info pointer isnt null, set *new_info to true/false depending if you used OPEN_FLAG_GET_INFO

	virtual set_info_t set_info_ex(reader *r,const file_info * info,__int64 * modtime,__int64 * filesize);//reader with exclusive write access

	virtual bool get_album_art(const playable_location * src,reader * in,reader * out) {return false;};//reserved

	virtual bool is_reopen_safe() {return false;}

};

template<class T>
class input_factory : public service_factory_t<input,T> {};


class input_file_type : public service_base
{
public:
	virtual unsigned get_count()=0;
	virtual bool get_name(unsigned idx,string_base & out)=0;//e.g. "MPEG file"
	virtual bool get_mask(unsigned idx,string_base & out)=0;//e.g. "*.MP3;*.MP2"; separate with semicolons
	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	static void build_openfile_mask(string_base & out,bool b_include_playlists=true);
};

class input_file_type_i : public service_impl_single_t<input_file_type>
{
	const char * name, * mask;
public:
	virtual unsigned get_count() {return 1;}
	input_file_type_i(const char * p_name, const char * p_mask) : name(p_name), mask(p_mask) {}
	virtual bool get_name(unsigned idx,string_base & out) {if (idx==0) {out = name; return true;} else return false;}
	virtual bool get_mask(unsigned idx,string_base & out) {if (idx==0) {out = mask; return true;} else return false;}
};

template<class T>
class input_file_type_factory : public service_factory_t<input_file_type,T> {};

#define DECLARE_FILE_TYPE(NAME,MASK) \
	namespace { static input_file_type_i g_filetype_instance(NAME,MASK); \
	static service_factory_single_ref_t<input_file_type,input_file_type_i> g_filetype_service(g_filetype_instance); }


//USAGE: DECLARE_FILE_TYPE("Blah file","*.blah;*.bleh");

#include "input_helpers.h"

#endif