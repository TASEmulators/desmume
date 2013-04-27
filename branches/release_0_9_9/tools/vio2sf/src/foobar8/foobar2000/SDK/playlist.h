#ifndef _PLAYLIST_H_
#define _PLAYLIST_H_

#include "service.h"
#include "metadb_handle.h"
#include "play_control.h"
#include "interface_helper.h"

//important: playlist engine is SINGLE-THREADED. call any APIs not from main thread and things will either blow up or refuse to work. all callbacks can be assumed to come from main thread.


//notes:
//all methods returning metadb_handle return add_ref'd handles, you have to handle_release them
//all methods taking metadb_handle will add_ref them if they store them, so it's your responsibility to later release handles you pass

class NOVTABLE playlist_oper : public service_base
{
public:
	//retrieving info
	virtual int get_count()=0;
	virtual bool get_sel(int idx)=0;//returns reference to a static object that holds state of playlist selection
	virtual bool get_sel_mask(bool * out,unsigned count)=0;
	virtual void get_sel_mask(bit_array_var & out);
	virtual metadb_handle * get_item(int index)=0;
	virtual int get_now_playing()=0;//will return -1 when not playing or currently played track isn't on playlist
	virtual int get_focus()=0;
	virtual int get_playback_cursor()=0;
	virtual void format_title(int idx,string_base & out,const char * spec,const char * extra_items)=0;//spec may be null, will use core settings; extra items are optional
	virtual void get_format_spec(string_base & out)=0;

	//modifying playlist
	virtual void set_sel_mask(const bit_array & sel,const bit_array & mask)=0;
	virtual void remove_mask(const bit_array & items)=0;
	virtual void replace_item(int idx,metadb_handle * handle)=0;
	virtual void set_focus(int idx)=0;
	virtual void add_items(const ptr_list_base<metadb_handle> & data,bool replace = false,int insert_idx = -1,bool select = false,bool filter = true)=0;
	//data - entries to insert, replace = replace-current-playlist flag, insert_idx - where to insert (-1 will append them), select - should new items be selected or not
	virtual void add_locations(const ptr_list_base<const char> & urls,bool replace = false,int insert_idx = -1,bool select = false,bool filter = true)=0;

	virtual void apply_order(const int * order,int count)=0;
	virtual void sort_by_format(const char * spec,int sel_only)=0;//spec==0 => randomizes
	virtual void set_playback_cursor(int idx)=0;
	virtual metadb_handle * playback_advance(play_control::track_command cmd)=0;//deprecated in 0.8
	virtual void undo_restore()=0;
	virtual void undo_reset()=0;
	virtual void ensure_visible(int idx)=0;

	//other
	virtual void play_item(int idx)=0;
	

	//new (0.8)
	virtual bool process_locations(const ptr_list_base<const char> & urls,ptr_list_base<metadb_handle> & out,bool filter = true,const char * mask = 0)=0;
	virtual bool process_dropped_files(interface IDataObject * pDataObject,ptr_list_base<metadb_handle> & out,bool filter = true)=0;
	virtual bool process_dropped_files_check(interface IDataObject * pDataObject)=0;
	virtual interface IDataObject * create_dataobject(const ptr_list_base<metadb_handle> & data)=0;
	virtual void get_sel_items(ptr_list_base<metadb_handle> & out)=0;
	virtual int get_sel_count()=0;
	virtual void get_items_mask(ptr_list_base<metadb_handle> & out, const bit_array & mask)=0;

	//helper

	inline void add_location(const char * url,bool replace = false,int insert_idx = -1,bool select = false,bool filter = true)
	{
		ptr_list_t<const char> temp;
		temp.add_item(url);
		add_locations(temp,replace,insert_idx,select,filter);
	}

	inline void set_sel_mask(const bit_array & sel) {set_sel_mask(sel,bit_array_true());}
	inline void set_sel(int idx,bool state) {set_sel_mask(bit_array_val(state),bit_array_one(idx));}
	inline void remove_item(int idx) {remove_mask(bit_array_one(idx));}
	inline void set_focus_sel(int idx) {set_focus(idx);set_sel_mask(bit_array_one(idx));}
	inline void reset() {remove_mask(bit_array_true());}
	inline void remove_all() {remove_mask(bit_array_true());}
	inline void set_sel_one(int idx) {set_sel_mask(bit_array_one(idx));}

	inline bool is_sel_count_greater(int v)
	{
		return get_sel_count()>v;
	}



	void remove_sel(bool crop);

	void move_items(int delta);

	static playlist_oper * get();

	inline metadb_handle * get_focus_item()
	{
		return get_item(get_focus());//may return null
	}

	void get_data(ptr_list_interface<metadb_handle> & out)
	{
		get_items_mask(out,bit_array_true());
	}


	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};


//note: certain methods may get called outside initquit !
//playlist_callback methods are called by playlist engine.
class NOVTABLE playlist_callback : public service_base
{
public:
	virtual void on_items_added(int start, int count)=0;//inside any of these methods, you can call playlist_oper APIs to get exact info about what happened (but only methods that read playlist state, not those that modify it)
	virtual void on_order(const int * order,int count)=0;//changes selection too; doesnt actually change set of items that are selected or item having focus, just changes their order
	virtual void on_items_removing(const bit_array & mask)=0;//called before actually removing them
	virtual void on_items_removed(const bit_array & mask)=0;//note: items being removed are de-selected first always
	virtual void on_sel_change(int idx,bool state)=0;
	virtual void on_sel_change_multi(const bit_array & before,const bit_array & after,int count)=0;
	virtual void on_focus_change(int from,int to)=0;//focus may be -1 when no item has focus; reminder: focus may also change on other callbacks
	virtual void on_modified(int idx)=0;
	virtual void on_replaced(int idx) {on_modified(idx);}
	virtual void on_ensure_visible(int idx) {}
	virtual void on_default_format_changed() {}
	//note: if you cache strings or whatever, make sure to flush cache when total item count changes, because stuff like %_playlist_total% may change as well (not to mention %_playlist_number% everywhere)
	
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE playback_flow_control : public service_base
{
public:
	virtual const char * get_name()=0;
	
	virtual int get_next(int previous_index,int focus_item,int total,int advance,bool follow_focus,bool user_advance) {return -1;}//deprecated

	int get_next(int previous_index,int focus_item,int total,int advance,bool follow_focus,bool user_advance,unsigned playlist);

	//int advance : -1 for prev, 0 for play, 1 for next

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}
};

class NOVTABLE playback_flow_control_v2 : public playback_flow_control
{
public:
	virtual int get_next(int previous_index,int focus_item,int total,int advance,bool follow_focus,bool user_advance,unsigned playlist)=0;
	//previous_index and focus_item may be -1; return -1 to stop
	//IMPORTANT (changed in 0.8): you are in playlist indicated by <unsigned playlist>, use playlist_switcher methods to access data

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return playback_flow_control::service_query(guid);
	}

};

template<class T>
class playback_flow_control_factory : public service_factory_single_t<playback_flow_control,T> {};

//helper code for manipulating configuration of playback flow control (repeat/shuffle/whatever) => see ../helpers/playback_order_helper.h

#endif