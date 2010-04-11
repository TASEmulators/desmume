#ifndef _FOOBAR2000_METADB_HANDLE_H_
#define _FOOBAR2000_METADB_HANDLE_H_

#include "file_info.h"

class reader;

//metadb_handle is a reference to file_info stored in database (see: file_info.h)
//as long as handle is active, info about particular is kept in database and immediately available (unless file i/o error occurs or the file gets removed)
//metadb_handle is the most efficient way of passing around playable resource locations and info about them, since all you have to do is to copy a pointer (and possibly call handle_add_ref)
//metadb_handle also lets you efficiently store locations of playable resources without having to waste memory on file path strings allocated by playable_location etc
//metadb_handle lets you pull file_info immediately without searching database, unlike metadb::query and other metadb api calls
//metadb_handle objects keep their reference counts, eg. you need to make a matching handle_release() call for every handle_add_ref()

/*
common n00b FAQ:

getting file location from metadb_handle -> use handle_get_location(), it never fails, returned pointer valid until you release the handle
handle_query_locked MAY fail and return null when file io error occurs or something, so CHECK the result BEFORE using it
make sure you call handle_lock/handle_unlock when you use handle_query_locked, otherwise some other thread might modify database data while you are working with result and thing will be fucked

unless commented otherwise, functions taking metadb_handle as parameter don't release the handle and add a reference if they store it; when function returns a metadb_handle, calling code is responsible for releasing it
EXCEPTION: all ptr_list-style objects dont touch reference counts when they receive/return metadb_handle; that includes ptr_list_interface<metadb_handle>, ptr_list_t<metadb_handle> and metadb_handle_list

*/


class NOVTABLE metadb_handle
{
public:
	virtual void handle_add_ref()=0;
	virtual void handle_release()=0;
	virtual int handle_query(file_info * dest,bool dbonly = false)=0;//dest can be null if you're only checking if info is available
	virtual const file_info * handle_query_locked(bool dbonly = false)=0;//must be inside database_lock()-protected section, may return 0 if info is not available for some reason
	virtual const playable_location * handle_get_location() const = 0;//never fails, returned pointer valid till you call release
	virtual void handle_hint(const file_info * src)=0;
	virtual int handle_format_title(string_base & out,const char * spec,const char * extra_items)=0;//reminder: see titleformat.h for descriptions of titleformatting parameters
	virtual int handle_flush_info()=0;//tries to reload info from file
	virtual int handle_update_info(const file_info * src,bool dbonly=false)=0;//return value - see metadb::update_info(); src may be null if you want to update file with info which is currently in database
	virtual void handle_entry_dead()=0;//for reporting dead files; need to call handle_release() separately
	virtual int handle_precache(reader * r=0)=0;//makes sure that info is in database; reader is optional
	virtual void handle_user_requested_remove()=0;//user requests this object to be removed from database (but file isn't being physically removed)
	virtual int handle_lock()=0;//same as metadb::database_lock()
	virtual int handle_try_lock()=0;
	virtual int handle_unlock()=0;//same as metadb::database_unlock()	
	virtual int handle_update_and_unlock(const file_info * info)=0;//new in 0.7.1; will unlock even if update fails

	//new in 0.8
	//time is a 64-bit value representing the number of 100-nanosecond intervals since January 1, 1601; 0 for invalid/unknown time
	virtual __int64 handle_get_modification_time() const = 0;
	virtual void handle_set_modification_time(__int64)=0;
	virtual __int64 handle_get_file_size() const = 0;
	virtual void handle_set_file_size(__int64)=0;
	virtual void handle_hint_ex(const file_info * src,__int64 time,__int64 size,bool b_fresh)=0;
	virtual int handle_check_if_changed()=0;//compares known modification time with file system and reloads info if needed
	virtual int handle_is_permcached() const = 0;//returns if this object is in "database" or not
	virtual int handle_get_relative_path(string_base & out) const = 0;


	inline int handle_query_dbonly(file_info * dest) {return handle_query(dest,true);}
	inline const file_info * handle_query_locked_dbonly() {return handle_query_locked(true);}


	inline metadb_handle * handle_duplicate() {handle_add_ref();return this;}

	bool handle_query_meta_field(const char * name,int num,string_base & out);
	int handle_query_meta_field_int(const char * name,int num);//returns 0 if not found
	double handle_get_length();
	__int64 handle_get_length_samples();

	inline const char * handle_get_path() const//never fails
	{
		return handle_get_location()->get_path();
	}
	inline int handle_get_subsong_index() const
	{
		return handle_get_location()->get_subsong_index();
	}



protected:
	metadb_handle() {}
	~metadb_handle() {}//verboten, avoid ptr_list_t stupidity
};

//important: standard conventions with addref'ing before returning / functions addref'ing their params DO NOT apply to pointer lists !!!

class metadb_handle_list : public ptr_list_t<metadb_handle>
{
public:
	void delete_item(metadb_handle * ptr);
	void delete_by_idx(int idx);
	void delete_all();
	void add_ref_all();
	void delete_mask(const bit_array & mask);
	void filter_mask_del(const bit_array & mask);
	void duplicate_list(const ptr_list_base<metadb_handle> & source);

	void sort_by_format_get_order(int * order,const char * spec,const char * extra_items) const; //only outputs order, doesnt modify list
	void sort_by_format(const char * spec,const char * extra_items);
	void remove_duplicates(bool b_release = true);
	void remove_duplicates_quick(bool b_release = true);//sorts by pointer values internally
	void sort_by_path_quick();
	
};

class metadb_handle_lock
{
	metadb_handle * ptr;
public:
	inline metadb_handle_lock(metadb_handle * param)
	{
		ptr = param->handle_duplicate();
		ptr->handle_lock();
	}	
	inline ~metadb_handle_lock() {ptr->handle_unlock();ptr->handle_release();}
};


#endif //_FOOBAR2000_METADB_HANDLE_H_