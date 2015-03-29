#ifndef _FOOBAR2000_PLAYABLE_LOCATION_H_
#define _FOOBAR2000_PLAYABLE_LOCATION_H_

#include "service.h"

#include "interface_helper.h"

//playable_location stores location of a playable resource, currently implemented as file path and integer for indicating multiple playable "subsongs" per file
//also see: file_info.h
//for getting more info about resource referenced by a playable_location, see metadb.h

//char* strings are all UTF-8

class playable_location//interface (for passing around between DLLs)
{
public:
	virtual const char * get_path() const =0;
	virtual void set_path(const char*)=0;
	virtual int get_number() const =0;
	virtual void set_number(int)=0;
	virtual ~playable_location() {};

	void copy(const playable_location * src)
	{
		set_path(src->get_path());
		set_number(src->get_number());
	}
#if 0
	int compare_sort(const playable_location * src) const
	{
		int ret = stricmp_utf8_full(file_path_display(get_path()),file_path_display(src->get_path()));
		if (ret!=0) return ret;
		else return compare(src);
	};
#endif

	int compare(const playable_location * src) const;

	const playable_location & operator=(const playable_location & src)
	{
		copy(&src);
		return *this;
	}	

	inline bool is_empty() {return get_path()[0]==0 && get_number()==0;}
	inline void reset() {set_path("");set_number(0);}
	inline int get_subsong_index() const {return get_number();}
	inline void set_subsong_index(int v) {set_number(v);}
};

class playable_location_i : public playable_location//implementation
{
	string_simple path;
	int number;
public:

	virtual const char * get_path() const {return path;}
	virtual void set_path(const char* z) {path=z;}
	virtual int get_number() const {return number;}
	virtual void set_number(int z) {number=z;}

	playable_location_i() {number=0;}

	const playable_location_i & operator=(const playable_location & src)
	{
		copy(&src);
		return *this;
	}

	playable_location_i(const char * p_path,int p_number) : path(p_path), number(p_number) {}

	playable_location_i(const playable_location & src) {copy(&src);}
};


// usage: something( make_playable_location("file://c:\blah.ogg",0) );
// only for use as a parameter to a function taking const playable_location*
class make_playable_location : private playable_location
{
	const char * path;
	int num;
	virtual const char * get_path() const {return path;}
	virtual void set_path(const char*) {}
	virtual int get_number() const {return num;}
	virtual void set_number(int) {}

public:
	make_playable_location(const char * p_path,int p_num) : path(p_path), num(p_num) {}
	operator const playable_location * () {return this;}
};


#endif //_FOOBAR2000_PLAYABLE_LOCATION_H_