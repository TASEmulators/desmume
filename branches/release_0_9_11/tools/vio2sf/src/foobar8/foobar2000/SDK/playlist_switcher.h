#ifndef _FOOBAR2000_PLAYLIST_SWITCHER_
#define _FOOBAR2000_PLAYLIST_SWITCHER_


//all calls from main app thread only !

class NOVTABLE playlist_switcher : public service_base
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual unsigned get_num_playlists()=0;
	virtual bool get_playlist_name(unsigned idx,string_base & out)=0;
	virtual bool get_playlist_data(unsigned idx,ptr_list_interface<metadb_handle> & out)=0;
	virtual unsigned get_playlist_size(unsigned idx)=0;//returns number of entries
	virtual bool get_playlist_selection_mask(unsigned idx,bit_array_var & out)=0;
	virtual unsigned get_active_playlist()=0;

	virtual bool set_active_playlist(unsigned idx)=0;
	virtual unsigned create_playlist(const char * name,const ptr_list_interface<metadb_handle> & data)=0;//returns index (or 0 on failure; index is always >0 because there is always at least one playlist present)
	virtual unsigned create_playlist_fixname(string_base & name,const ptr_list_interface<metadb_handle> & data)=0;//name may change when playlist of this name is already present or passed name contains invalid chars; returns index (or 0 on failure; index is always >0 because there is always at least one playlist present)
	virtual bool delete_playlist(unsigned idx)=0;//will fail if you are trying to delete only playlist, or currently active playlist
	virtual bool reorder(const int * order,unsigned count)=0;
	virtual bool rename_playlist(unsigned idx,const char * new_name)=0;//will fail if name exists or contains invalid chars
	virtual bool rename_playlist_fixname(unsigned idx,string_base & name)=0;//will attempt to correct the name before failing

	virtual metadb_handle * get_item(unsigned playlist,unsigned idx)=0;
	virtual bool format_title(unsigned playlist,unsigned idx,string_base & out,const char * spec,const char * extra_items)=0;//spec may be null, will use core settings; extra items are optional

	//new (0.8)
	virtual unsigned get_playing_playlist()=0;
	virtual metadb_handle * playback_advance(play_control::track_command cmd,unsigned * p_playlist,unsigned * p_item,bool * b_is_active_playlist)=0;//deprecated in 0.8
	virtual void reset_playing_playlist()=0;
	virtual void highlight_playing_item()=0;

	static playlist_switcher * get();

	unsigned find_playlist(const char * name);//returns ((unsigned)-1) when not found
	unsigned find_or_create_playlist(const char * name);
	bool find_or_create_playlist_activate(const char * name);
	bool delete_playlist_autoswitch(unsigned idx);
};

class playlist_switcher_callback : public service_base
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual void on_playlist_switch_before(unsigned from,unsigned to)=0;//called before switching 
	virtual void on_playlist_switch_after(unsigned from,unsigned to)=0;//called after switching

	virtual void on_reorder(const int * order,unsigned count)=0;
	virtual void on_new_playlist(const char * name,unsigned idx,const ptr_list_interface<metadb_handle> & data)=0;
	virtual void on_delete_playlist(unsigned idx)=0;
	virtual void on_rename_playlist(unsigned idx,const char * new_name)=0;

	virtual void on_item_replaced(unsigned pls,unsigned item,metadb_handle * from,metadb_handle * to)=0;//workaround for inactive-playlist-needs-modification-when-using-masstagger-autorename; does not work for active playlist !
};

template<class T>
class playlist_switcher_callback_factory : public service_factory_single_t<playlist_switcher_callback,T> {};


#endif//_FOOBAR2000_PLAYLIST_SWITCHER_
