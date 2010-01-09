#ifndef _FOOBAR2000_PLAYABLE_LOCATION_H_
#define _FOOBAR2000_PLAYABLE_LOCATION_H_

//playable_location stores location of a playable resource, currently implemented as file path and integer for indicating multiple playable "subsongs" per file
//also see: file_info.h
//for getting more info about resource referenced by a playable_location, see metadb.h

//char* strings are all UTF-8

class NOVTABLE playable_location//interface (for passing around between DLLs)
{
public:
	virtual const char * get_path() const =0;
	virtual void set_path(const char*)=0;
	virtual t_uint32 get_subsong() const =0;
	virtual void set_subsong(t_uint32)=0;
	
	void copy(const playable_location & p_other) {
		set_path(p_other.get_path());
		set_subsong(p_other.get_subsong());
	}

	static int g_compare(const playable_location & p_item1,const playable_location & p_item2);

	const playable_location & operator=(const playable_location & src) {copy(src);return *this;}	

	bool operator==(const playable_location & p_other) const;
	bool operator!=(const playable_location & p_other) const;

	inline bool is_empty() {return get_path()[0]==0 && get_subsong()==0;}
	inline void reset() {set_path("");set_subsong(0);}
	inline t_uint32 get_subsong_index() const {return get_subsong();}
	inline void set_subsong_index(t_uint32 v) {set_subsong(v);}

	class comparator {
	public:
		static int compare(const playable_location & v1, const playable_location & v2) {return g_compare(v1,v2);}
	};

protected:
	playable_location() {}
	~playable_location() {}
};

typedef playable_location * pplayable_location;
typedef playable_location const * pcplayable_location;
typedef playable_location & rplayable_location;
typedef playable_location const & rcplayable_location;

class playable_location_impl : public playable_location//implementation
{
public:
	const char * get_path() const {return m_path;}
	void set_path(const char* p_path) {m_path=p_path;}
	t_uint32 get_subsong() const {return m_subsong;}
	void set_subsong(t_uint32 p_subsong) {m_subsong=p_subsong;}

	const playable_location_impl & operator=(const playable_location & src) {copy(src);return *this;}
	const playable_location_impl & operator=(const playable_location_impl & src) {copy(src);return *this;}

	playable_location_impl() : m_subsong(0) {}
	playable_location_impl(const char * p_path,t_uint32 p_subsong) : m_path(p_path), m_subsong(p_subsong) {}
	playable_location_impl(const playable_location & src) {copy(src);}
	playable_location_impl(const playable_location_impl & src) {copy(src);}

private:
	pfc::string_simple m_path;
	t_uint32 m_subsong;
};

// usage: something( make_playable_location("file://c:\blah.ogg",0) );
// only for use as a parameter to a function taking const playable_location &
class make_playable_location : public playable_location
{
	const char * path;
	t_uint32 num;
	
	void set_path(const char*) {throw pfc::exception_not_implemented();}
	void set_subsong(t_uint32) {throw pfc::exception_not_implemented();}

public:
	const char * get_path() const {return path;}
	t_uint32 get_subsong() const {return num;}

	make_playable_location(const char * p_path,t_uint32 p_num) : path(p_path), num(p_num) {}
};

pfc::string_base & operator<<(pfc::string_base & p_fmt,const playable_location & p_location);

#endif //_FOOBAR2000_PLAYABLE_LOCATION_H_
