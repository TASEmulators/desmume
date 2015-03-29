#ifndef _PLAYLIST_LOADER_H_
#define _PLAYLIST_LOADER_H_

#include "service.h"

#include "reader.h"

#include "metadb.h"

class NOVTABLE playlist_loader_callback	//receives new playlist entries from playlists/filesystem/whatever
{
public:
	enum entry_type
	{
		USER_REQUESTED,
		DIRECTORY_ENUMERATED,
		FROM_PLAYLIST,
	};
	virtual int on_progress(const char * path) {return 1;}
	//for monitoring progress only, return 1 to continue, 0 to abort; note that you will most likely get called a few times after first 0 return
	//path might be null if playlist loader is just checking if you are aborting or not
	
	virtual void on_entry(const playable_location * src,entry_type type)=0;
	virtual void on_entry_have_info(const file_info * src,entry_type type) {on_entry(src->get_location(),type);}
	virtual void on_entry_dbhandle(metadb_handle * ptr,entry_type type) {on_entry(ptr->handle_get_location(),type);}
};

class playlist_loader_callback_i : public playlist_loader_callback
{
	metadb_handle_list data;
public:

	~playlist_loader_callback_i() {data.delete_all();}

	metadb_handle * get_item(unsigned idx) {return data[idx];}
	metadb_handle * operator[](unsigned idx) {return data[idx];}
	unsigned get_count() {return data.get_count();}
	
	const metadb_handle_list & get_data() {return data;}

	virtual int on_progress(const char * path) {return 1;}
	virtual void on_entry(const playable_location * src,entry_type type)
	{data.add_item(metadb::get()->handle_create(src));}
	virtual void on_entry_have_info(const file_info * src,entry_type type)
	{data.add_item(metadb::get()->handle_create_hint(src));}
	virtual void on_entry_dbhandle(metadb_handle * ptr,entry_type type)
	{data.add_item(ptr->handle_duplicate());}
};

class NOVTABLE playlist_loader : public service_base
{
public:
	virtual int open(const char * filename, reader * r,playlist_loader_callback * callback)=0;	//send new entries to callback
	virtual int write(const char * filename, reader * r,const ptr_list_base<metadb_handle> & data)=0;
	virtual const char * get_extension()=0;
	virtual bool can_write()=0;
	bool is_our_content_type(const char*);

	static int load_playlist(const char * filename,playlist_loader_callback * callback);
	//attempts to load file as a playlist, return 0 on failure

	static int save_playlist(const char * filename,const ptr_list_base<metadb_handle> & data);
	//saves playlist into file

	static void process_path(const char * filename,playlist_loader_callback * callback,playlist_loader_callback::entry_type type = playlist_loader_callback::USER_REQUESTED);
	//adds contents of filename (can be directory) to playlist, doesnt load playlists
	
	//this helper also loads playlists
	//return 1 if loaded as playlist, 0 if loaded as files
	static int process_path_ex(const char * filename,playlist_loader_callback * callback,playlist_loader_callback::entry_type type = playlist_loader_callback::USER_REQUESTED)
	{
		if (!load_playlist(filename,callback))
		{
			process_path(filename,callback,type);
			return 0;
		}
		else return 1;
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE playlist_loader_v2 : public playlist_loader
{
public:
	virtual bool is_our_content_type(const char * param) = 0;

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE track_indexer : public service_base
{
public:

	class NOVTABLE callback
	{
	public:
		virtual void on_entry(const playable_location * src)=0;
	};

	class callback_i : public callback
	{
	public:
		ptr_list_t<playable_location_i> data;
		virtual void on_entry(const playable_location * src) {data.add_item(new playable_location_i(*src));}
		~callback_i() {data.delete_all();}
	};

	class callback_i_ref : public callback
	{
		ptr_list_t<playable_location_i> &data;
	public:		
		virtual void on_entry(const playable_location * src) {data.add_item(new playable_location_i(*src));}
		callback_i_ref(ptr_list_t<playable_location_i> &p_data) : data(p_data) {}
	};

	class callback_i_wrap : public callback
	{
		playlist_loader_callback * out;
		playlist_loader_callback::entry_type type;
	public:
		virtual void on_entry(const playable_location * src) {out->on_entry(src,type);}
		callback_i_wrap(playlist_loader_callback * p_out,playlist_loader_callback::entry_type p_type) : out(p_out), type(p_type) {}
	};

	class callback_i_metadb : public callback
	{
		ptr_list_base<metadb_handle> & out;
		metadb * p_metadb;
	public:
		virtual void on_entry(const playable_location * src) {out.add_item(p_metadb->handle_create(src));}
		callback_i_metadb(ptr_list_base<metadb_handle> & p_out) : out(p_out), p_metadb(metadb::get()) {}
	};

public:
	virtual int get_tracks(const char * filename,callback* out,reader * r)=0;//return 0 on failure / not our file, 1 on success; send your subsongs to out; reader CAN BE NULL, if so, create/destroy your own one
	//return either playlist entries with same filename, or playlist entries belonging to a readerless input (eg. cdda://)

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static int g_get_tracks_callback(const char * filename,callback * out,reader * r=0);
	static int g_get_tracks(const char * filename,ptr_list_t<playable_location_i> & out,reader * r=0)
	{
		return g_get_tracks_callback(filename,&callback_i_ref(out),r);
	}

	static int g_get_tracks_wrap(const char * filename,playlist_loader_callback * out,playlist_loader_callback::entry_type type,reader * r=0);
	int get_tracks_ex(const char * filename,ptr_list_base<metadb_handle> & out,reader * r = 0);
	static int g_get_tracks_ex(const char * filename,ptr_list_base<metadb_handle> & out,reader * r = 0);

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}
};

class NOVTABLE track_indexer_v2 : public track_indexer
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	//since you usually have to parse the damn file anyway, here's a method that lets you precache info to database already to avoid redundant file operations later
	virtual int get_tracks_ex(const char * filename,ptr_list_base<metadb_handle> & out,reader * r = 0)=0;//return 0 on failure / not our file, 1 on success; send your subsongs to out; reader CAN BE NULL, if so, create/destroy your own one

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return track_indexer::service_query(guid);
	}

};

template<class T>
class track_indexer_factory : public service_factory_t<track_indexer,T> {};


#endif //_PLAYLIST_LOADER_H_