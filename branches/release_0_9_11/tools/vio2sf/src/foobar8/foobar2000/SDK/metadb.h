#ifndef _METADB_H_
#define _METADB_H_

#include "service.h"
#include "interface_helper.h"
#include "file_info.h"

class input;
class reader;


#include "metadb_handle.h"

//only one implementation in main exe, DO NOT DERIVE FROM THIS
class NOVTABLE metadb : public service_base
{
public:
	virtual int update_info_flush()=0;//tries to perform any pending tag update operations, may take a few good seconds to execute

	virtual void file_moved(const char * src,const char * dst)=0;//will transfer existing database data to new path (and notify callbacks about file being moved)
	virtual void file_copied(const char * src,const char * dst)=0;
	virtual void file_removed(const char * src)=0;//use this to tell database that url [src] is dead
	virtual void entry_dead(const playable_location * entry)=0;//same as file_removed but taking playable_location

	virtual int database_lock()=0;//enters database's internal critical section, prevents other threads from accessing database as long as it's locked
	virtual int database_try_lock()=0;//returns immediately without waiting, may fail, returns 0 on failure
	virtual int database_unlock()=0;

	
	virtual metadb_handle * handle_create(const playable_location * src)=0;//should never fail
	virtual metadb_handle * handle_create_hint(const file_info * src)=0;//should never fail
	
	virtual void get_all_entries(ptr_list_base<metadb_handle> & out)=0;//you get duplicate handles of all database items, you are responsible for releasing them

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static metadb * get();//safe not to release it; don't call from DLL startup / static object constructor

	//helper
	static int g_format_title(const playable_location * source,string_base & out,const char * spec,const char * extra_items=0)
	{
		return get()->format_title(source,out,spec,extra_items);
	}

	int query(file_info * out);
	int query_flush(file_info * out);
	int query_dbonly(file_info * out);
	int query_meta_field(const playable_location * src,const char * name,int num,string8 & out);
	int update_info(const file_info * info,bool dbonly=false);//using playable_location from file_info, return -1 on failure, 0 if pending, 1 on immediate success; may take a few good seconds to execute
	void user_requested_remove(const playable_location * entry);//use this when user requests files to be removed from database (but not deleted physically)
	int query(const playable_location * entry,file_info * out);
	int query_flush(const playable_location * entry,file_info * out);
	int query_dbonly(const playable_location * entry,file_info * out);
	int precache(const playable_location * entry,reader * r);
	void precache(const char * filename,reader * r);
	void hint(const file_info * src);
	
	int format_title(const playable_location * source,string_base & out,const char * spec,const char * extra_items);
	//reminder: see titleformat.h for descriptions of titleformatting parameters
	//return 1 on success, 0 on failure (cant get info, used empty fields and filename)

	inline void update_info_dbonly(const file_info * info) {update_info(info,true);} //updates database, doesnt modify files

#ifdef _DEBUG
	int database_lock_count();
	void database_delockify(int wanted_count);
	inline void assert_not_locked()
	{
		assert(database_lock_count()==0);
	}
	inline void assert_locked()
	{
		assert(database_lock_count()>0);
	}
#endif

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	virtual void handle_release_multi(metadb_handle * const * list,unsigned count)=0;
	virtual void handle_add_ref_multi(metadb_handle * const * list,unsigned count)=0;
};

class NOVTABLE metadb_callback : public service_base//use service_factory_single_t to instantiate, you will get called with database notifications
{//IMPORTANT: metadb_callback calls may happen from different threads ! watch out for deadlocks / calling things that work only from main thread / etc
public:
	virtual void on_info_edited(metadb_handle * handle) {}
	virtual void on_info_reloaded(metadb_handle * handle) {}
	virtual void on_file_removed(metadb_handle * handle) {}
	virtual void on_file_moved(metadb_handle * src,metadb_handle * dst) {}
	virtual void on_file_new(metadb_handle * handle) {}//reserved for future use
	virtual void on_file_removed_user(metadb_handle * handle) {}//user requested file to be removed from db, file didn't get physically removed
//warning: all callback methods are called inside database_lock section ! watch out for deadlock conditions if you use SendMessage() or critical sections
	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE metadb_callback_simple : public service_base
{//this is always properly sync'd to main thread, unlike metadb_callback above
public:
	virtual void on_info_change(metadb_handle * handle)=0;
	virtual void on_file_moved(metadb_handle * src,metadb_handle * dst)=0;

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};


class in_metadb_sync
{
public:
	in_metadb_sync() {metadb::get()->database_lock();}
	~in_metadb_sync() {metadb::get()->database_unlock();}	
};


#endif