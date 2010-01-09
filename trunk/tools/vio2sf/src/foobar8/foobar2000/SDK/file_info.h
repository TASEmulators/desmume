#ifndef _FILE_INFO_H_
#define _FILE_INFO_H_

#include "playable_location.h"
#include "playlist_entry.h"//for compatibility with old code

//file_info == playable_location + length + metadata + tech infos
//also see: metadb.h

//all char* strings are UTF-8, including filenames, unless comments state otherwise

class NOVTABLE file_info	//interface (cross-dll-safe)
{
public:
	virtual ~file_info() {}
	//interface

	virtual void copy(const file_info * src);//can be overridden

	//multiple fields of the same name ARE allowed.
	virtual int meta_get_count() const = 0;
	virtual const char * meta_enum_name(int n) const = 0;
	virtual const char * meta_enum_value(int n) const = 0;
	virtual void meta_modify_value(int n,const char * new_value) = 0;
	virtual void meta_insert(int index,const char * name,const char * value) = 0;
	virtual void meta_add(const char * name,const char * value) = 0;
	virtual void meta_remove(int n) = 0;
	virtual void meta_remove_all() = 0;
	virtual int meta_get_idx(const char * name,int num = 0) const;	
	//tech infos (bitrate, replaygain, etc), not user-editable
	//multiple fields of the same are NOT allowed.
	virtual void info_set(const char * name,const char * value) = 0;//replaces existing field if found
	virtual int info_get_count() const = 0;
	virtual const char * info_enum_name(int n) const = 0;
	virtual const char * info_enum_value(int n) const = 0;	
	virtual void info_remove(int n) = 0;
	virtual void info_remove_all() = 0;
	virtual int info_get_idx(const char * name) const;

	virtual const playable_location * get_location() const = 0;
	virtual void set_location(const playable_location *)=0;

	virtual void set_length(double) = 0;//length in seconds
	virtual double get_length() const = 0;


	inline int get_subsong_index() const {return get_location()->get_number();}
	inline const char * get_file_path() const {return get_location()->get_path();}

	
	inline void reset() {meta_remove_all();info_remove_all();set_length(0);}

//helper stuff for setting meta
	void meta_add_wide(const WCHAR * name,const WCHAR* value);
	void meta_add_ansi(const char * name,const char * value);
	void meta_set_wide(const WCHAR * name,const WCHAR* value);
	void meta_set_ansi(const char * name,const char * value);
	void meta_add_n(const char * name,int name_len,const char * value,int value_len);
	void meta_remove_field(const char * name);//removes ALL fields of given name
	void meta_set(const char * name,const char * value);	//deletes all fields of given name (if any), then adds new one
	const char * meta_get(const char * name,int num = 0) const;
	int meta_get_count_by_name(const char* name) const;
	const char * info_get(const char * name) const;
	void info_remove_field(const char * name);

	__int64 info_get_int(const char * name) const;
	__int64 info_get_length_samples() const;
	double info_get_float(const char * name);
	void info_set_int(const char * name,__int64 value);
	void info_set_float(const char * name,double value,unsigned precision,bool force_sign = false,const char * unit = 0);
	void info_set_replaygain_track_gain(double value);
	void info_set_replaygain_album_gain(double value);
	void info_set_replaygain_track_peak(double value);
	void info_set_replaygain_album_peak(double value);

	inline __int64 info_get_bitrate_vbr() {return info_get_int("bitrate_dynamic");}
	inline void info_set_bitrate_vbr(__int64 val) {info_set_int("bitrate_dynamic",val);}
	inline __int64 info_get_bitrate() {return info_get_int("bitrate");}
	inline void info_set_bitrate(__int64 val) {info_set_int("bitrate",val);}

	unsigned info_get_decoded_bps();//what bps the stream originally was (before converting to audio_sample), 0 if unknown
};

/*
recommended meta types:
TITLE
ARTIST
ALBUM
TRACKNUMBER  (not TRACK)
DATE   (not YEAR)
DISC   (for disc number in a multidisc album)
GENRE
COMMENT
*/

#endif //_FILE_INFO_H_