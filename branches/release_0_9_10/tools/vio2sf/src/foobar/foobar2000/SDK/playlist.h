//! This interface allows filtering of playlist modification operations.\n
//! Implemented by components "locking" playlists; use playlist_manager::playlist_lock_install() etc to takeover specific playlist with your instance of playlist_lock.
class NOVTABLE playlist_lock : public service_base {
public:
	enum {
		filter_add		= 1 << 0,
		filter_remove	= 1 << 1,
		filter_reorder  = 1 << 2,
		filter_replace  = 1 << 3,
		filter_rename	= 1 << 4,
		filter_remove_playlist = 1 << 5,
		filter_default_action = 1 << 6,
	};

	//! Queries whether specified item insertiion operation is allowed in the locked playlist.
	//! @param p_base Index from which the items are being inserted.
	//! @param p_data Items being inserted.
	//! @param p_selection Caller-requested selection state of items being inserted.
	//! @returns True to allow the operation, false to block it.
	virtual bool query_items_add(t_size p_base, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection) = 0;
	//! Queries whether specified item reorder operation is allowed in the locked playlist.
	//! @param p_order Pointer to array containing permutation defining requested reorder operation.
	//! @param p_count Number of items in array pointed to by p_order. This should always be equal to number of items on the locked playlist.
	//! @returns True to allow the operation, false to block it.
	virtual bool query_items_reorder(const t_size * p_order,t_size p_count) = 0;
	//! Queries whether specified item removal operation is allowed in the locked playlist.
	//! @param p_mask Specifies which items from locked playlist are being removed.
	//! @param p_force If set to true, the call is made only for notification purpose and items are getting removed regardless (after e.g. they have been physically removed).
	//! @returns True to allow the operation, false to block it. Note that return value is ignored if p_force is set to true.
	virtual bool query_items_remove(const bit_array & p_mask,bool p_force) = 0;
	//! Queries whether specified item replacement operation is allowed in the locked playlist.
	//! @param p_index Index of the item being replaced.
	//! @param p_old Old value of the item being replaced.
	//! @param p_new New value of the item being replaced.
	//! @returns True to allow the operation, false to block it.
	virtual bool query_item_replace(t_size p_index,const metadb_handle_ptr & p_old,const metadb_handle_ptr & p_new)=0;
	//! Queries whether renaming the locked playlist is allowed.
	//! @param p_new_name Requested new name of the playlist; a UTF-8 encoded string.
	//! @param p_new_name_len Length limit of the name string, in bytes (actual string may be shorter if null terminator is encountered before). Set this to infinite to use plain null-terminated strings.
	//! @returns True to allow the operation, false to block it.
	virtual bool query_playlist_rename(const char * p_new_name,t_size p_new_name_len) = 0;
	//! Queries whether removal of the locked playlist is allowed. Note that the lock will be released when the playlist is removed.
	//! @returns True to allow the operation, false to block it.
	virtual bool query_playlist_remove() = 0;
	//! Executes "default action" (doubleclick etc) for specified playlist item. When the playlist is not locked, default action starts playback of the item.
	//! @returns True if custom default action was executed, false to fall-through to default one for non-locked playlists (start playback).
	virtual bool execute_default_action(t_size p_item) = 0;
	//! Notifies lock about changed index of the playlist, in result of user reordering playlists or removing other playlists.
	virtual void on_playlist_index_change(t_size p_new_index) = 0;
	//! Notifies lock about the locked playlist getting removed.
	virtual void on_playlist_remove() = 0;
	//! Retrieves human-readable name of playlist lock to display.
	virtual void get_lock_name(pfc::string_base & p_out) = 0;
	//! Requests user interface of component controlling the playlist lock to be shown.
	virtual void show_ui() = 0;
	//! Queries which actions the lock filters. The return value must not change while the lock is registered with playlist_manager. The return value is a combination of one or more filter_* constants.
	virtual t_uint32 get_filter_mask() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(playlist_lock,service_base);
};

struct t_playback_queue_item {
	metadb_handle_ptr m_handle;
	t_size m_playlist,m_item;

	bool operator==(const t_playback_queue_item & p_item) const;
	bool operator!=(const t_playback_queue_item & p_item) const;
};


//! This service provides methods for all sorts of playlist interaction.\n
//! All playlist_manager methods are valid only from main app thread.\n
//! Usage: static_api_ptr_t<playlist_manager>.
class NOVTABLE playlist_manager : public service_base
{
public:

	//! Callback interface for playlist enumeration methods.
	class NOVTABLE enum_items_callback {
	public:
		//! @returns True to continue enumeration, false to abort.
		virtual bool on_item(t_size p_index,const metadb_handle_ptr & p_location,bool b_selected) = 0;//return false to stop
	};

	//! Retrieves number of playlists.
	virtual t_size get_playlist_count() = 0;
	//! Retrieves index of active playlist; infinite if no playlist is active.
	virtual t_size get_active_playlist() = 0;
	//! Sets active playlist (infinite to set no active playlist).
	virtual void set_active_playlist(t_size p_index) = 0;
	//! Retrieves playlist from which items to be played are taken from.
	virtual t_size get_playing_playlist() = 0;
	//! Sets playlist from which items to be played are taken from.
	virtual void set_playing_playlist(t_size p_index) = 0;
	//! Removes playlists according to specified mask. See also: bit_array.
	virtual bool remove_playlists(const bit_array & p_mask) = 0;
	//! Creates a new playlist.
	//! @param p_name Name of playlist to create; a UTF-8 encoded string.
	//! @param p_name_length Length limit of playlist name string, in bytes (actual string may be shorter if null terminator is encountered before). Set this to infinite to use plain null-terminated strings.
	//! @param p_index Index at which to insert new playlist; set to infinite to put it at the end of playlist list.
	//! @returns Actual index of newly inserted playlist, infinite on failure (call from invalid context).
	virtual t_size create_playlist(const char * p_name,t_size p_name_length,t_size p_index) = 0;
	//! Reorders the playlist list according to specified permutation.
	//! @returns True on success, false on failure (call from invalid context).
	virtual bool reorder(const t_size * p_order,t_size p_count) = 0;
	
	
	//! Retrieves number of items on specified playlist.
	virtual t_size playlist_get_item_count(t_size p_playlist) = 0;
	//! Enumerates contents of specified playlist.
	virtual void playlist_enum_items(t_size p_playlist,enum_items_callback & p_callback,const bit_array & p_mask) = 0;
	//! Retrieves index of focus item on specified playlist; returns infinite when no item has focus.
	virtual t_size playlist_get_focus_item(t_size p_playlist) = 0;
	//! Retrieves name of specified playlist. Should never fail unless the parameters are invalid.
	virtual bool playlist_get_name(t_size p_playlist,pfc::string_base & p_out) = 0;
	
	//! Reorders items in specified playlist according to specified permutation.
	virtual bool playlist_reorder_items(t_size p_playlist,const t_size * p_order,t_size p_count) = 0;
	//! Selects/deselects items on specified playlist.
	//! @param p_playlist Index of playlist to alter.
	//! @param p_affected Mask of items to alter.
	//! @param p_status Mask of selected/deselected state to apply to items specified by p_affected.
	virtual void playlist_set_selection(t_size p_playlist,const bit_array & p_affected,const bit_array & p_status) = 0;
	//! Removes specified items from specified playlist. Returns true on success or false on failure (playlist locked).
	virtual bool playlist_remove_items(t_size p_playlist,const bit_array & mask)=0;
	//! Replaces specified item on specified playlist. Returns true on success or false on failure (playlist locked).
	virtual bool playlist_replace_item(t_size p_playlist,t_size p_item,const metadb_handle_ptr & p_new_item) = 0;
	//! Sets index of focus item on specified playlist; use infinite to set no focus item.
	virtual void playlist_set_focus_item(t_size p_playlist,t_size p_item) = 0;
	//! Inserts new items into specified playlist, at specified position.
	virtual t_size playlist_insert_items(t_size p_playlist,t_size p_base,const pfc::list_base_const_t<metadb_handle_ptr> & data,const bit_array & p_selection) = 0;
	//! Tells playlist renderers to make sure that specified item is visible.
	virtual void playlist_ensure_visible(t_size p_playlist,t_size p_item) = 0;
	//! Renames specified playlist.
	//! @param p_name New name of playlist; a UTF-8 encoded string.
	//! @param p_name_length Length limit of playlist name string, in bytes (actual string may be shorter if null terminator is encountered before). Set this to infinite to use plain null-terminated strings.
	//! @returns True on success, false on failure (playlist locked).
	virtual bool playlist_rename(t_size p_index,const char * p_name,t_size p_name_length) = 0;


	//! Creates an undo restore point for specified playlist.
	virtual void playlist_undo_backup(t_size p_playlist) = 0;
	//! Reverts specified playlist to last undo restore point and generates a redo restore point.
	//! @returns True on success, false on failure (playlist locked or no restore point available).
	virtual bool playlist_undo_restore(t_size p_playlist) = 0;
	//! Reverts specified playlist to next redo restore point and generates an undo restore point.
	//! @returns True on success, false on failure (playlist locked or no restore point available).
	virtual bool playlist_redo_restore(t_size p_playlist) = 0;
	//! Returns whether an undo restore point is available for specified playlist.
	virtual bool playlist_is_undo_available(t_size p_playlist) = 0;
	//! Returns whether a redo restore point is available for specified playlist.
	virtual bool playlist_is_redo_available(t_size p_playlist) = 0;

	//! Renders information about specified playlist item, using specified titleformatting script parameters.
	//! @param p_playlist Index of playlist containing item being processed.
	//! @param p_item Index of item being processed in the playlist containing it.
	//! @param p_hook Titleformatting script hook to use; see titleformat_hook documentation for more info. Set to NULL when hook functionality is not needed.
	//! @param p_out String object receiving results.
	//! @param p_script Compiled titleformatting script to use; see titleformat_object cocumentation for more info.
	//! @param p_filter Text filter to use; see titleformat_text_filter documentation for more info. Set to NULL when text filter functionality is not needed.
	//! @param p_playback_info_level Level of playback related information requested. See playback_control::t_display_level documentation for more info.
	virtual void playlist_item_format_title(t_size p_playlist,t_size p_item,titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<titleformat_object> & p_script,titleformat_text_filter * p_filter,playback_control::t_display_level p_playback_info_level)=0;


	//! Retrieves playlist position of currently playing item.
	//! @param p_playlist Receives index of playlist containing currently playing item on success.
	//! @param p_index Receives index of currently playing item in the playlist that contains it on success.
	//! @returns True on success, false on failure (not playing or currently played item has been removed from the playlist it was on when starting).
	virtual bool get_playing_item_location(t_size * p_playlist,t_size * p_index) = 0;
	
	//! Sorts specified playlist - entire playlist or selection only - by specified title formatting pattern, or randomizes the order.
	//! @param p_playlist Index of playlist to alter.
	//! @param p_pattern Title formatting pattern to sort by (an UTF-8 encoded null-termindated string). Set to NULL to randomize the order of items.
	//! @param p_sel_only Set to false to sort/randomize whole playlist, or to true to sort/randomize only selection on the playlist.
	//! @returns True on success, false on failure (playlist locked etc).
	virtual bool playlist_sort_by_format(t_size p_playlist,const char * p_pattern,bool p_sel_only) = 0;

	//! For internal use only; p_items must be sorted by metadb::path_compare; use file_operation_callback static methods instead of calling this directly.
	virtual void on_files_deleted_sorted(const pfc::list_base_const_t<const char *> & p_items) = 0;
	//! For internal use only; p_from must be sorted by metadb::path_compare; use file_operation_callback static methods instead of calling this directly.
	virtual void on_files_moved_sorted(const pfc::list_base_const_t<const char *> & p_from,const pfc::list_base_const_t<const char *> & p_to) = 0;

	virtual bool playlist_lock_install(t_size p_playlist,const service_ptr_t<playlist_lock> & p_lock) = 0;//returns false when invalid playlist or already locked
	virtual bool playlist_lock_uninstall(t_size p_playlist,const service_ptr_t<playlist_lock> & p_lock) = 0;
	virtual bool playlist_lock_is_present(t_size p_playlist) = 0;
	virtual bool playlist_lock_query_name(t_size p_playlist,pfc::string_base & p_out) = 0;
	virtual bool playlist_lock_show_ui(t_size p_playlist) = 0;
	virtual t_uint32 playlist_lock_get_filter_mask(t_size p_playlist) = 0;


	//! Retrieves number of available playback order modes.
	virtual t_size playback_order_get_count() = 0;
	//! Retrieves name of specified playback order move.
	//! @param p_index Index of playback order mode to query, from 0 to playback_order_get_count() return value - 1.
	//! @returns Null-terminated UTF-8 encoded string containing name of the playback order mode. Returned pointer points to statically allocated string and can be safely stored without having to free it later.
	virtual const char * playback_order_get_name(t_size p_index) = 0;
	//! Retrieves GUID of specified playback order mode. Used for managing playback modes without relying on names.
	//! @param p_index Index of playback order mode to query, from 0 to playback_order_get_count() return value - 1.
	virtual GUID playback_order_get_guid(t_size p_index) = 0;
	//! Retrieves index of active playback order mode.
	virtual t_size playback_order_get_active() = 0;
	//! Sets index of active playback order mode.
	virtual void playback_order_set_active(t_size p_index) = 0;
	
	virtual void queue_remove_mask(bit_array const & p_mask) = 0;
	virtual void queue_add_item_playlist(t_size p_playlist,t_size p_item) = 0;
	virtual void queue_add_item(metadb_handle_ptr p_item) = 0;
	virtual t_size queue_get_count() = 0;
	virtual void queue_get_contents(pfc::list_base_t<t_playback_queue_item> & p_out) = 0;
	//! Returns index (0-based) on success, infinite on failure (item not in queue).
	virtual t_size queue_find_index(t_playback_queue_item const & p_item) = 0;

	//! Registers a playlist callback; registered object receives notifications about any modifications of any of loaded playlists.
	//! @param p_callback Callback interface to register.
	//! @param p_flags Flags indicating which callback methods are requested. See playlist_callback::flag_* constants for more info. The main purpose of flags parameter is working set optimization by not calling methods that do nothing.
	virtual void register_callback(class playlist_callback * p_callback,unsigned p_flags) = 0;
	//! Registers a playlist callback; registered object receives notifications about any modifications of active playlist.
	//! @param p_callback Callback interface to register.
	//! @param p_flags Flags indicating which callback methods are requested. See playlist_callback_single::flag_* constants for more info. The main purpose of flags parameter is working set optimization by not calling methods that do nothing.
	virtual void register_callback(class playlist_callback_single * p_callback,unsigned p_flags) = 0;
	//! Unregisters a playlist callback (playlist_callback version).
	virtual void unregister_callback(class playlist_callback * p_callback) = 0;
	//! Unregisters a playlist callback (playlist_callback_single version).
	virtual void unregister_callback(class playlist_callback_single * p_callback) = 0;
	//! Modifies flags indicating which calback methods are requested (playlist_callback version).
	virtual void modify_callback(class playlist_callback * p_callback,unsigned p_flags) = 0;
	//! Modifies flags indicating which calback methods are requested (playlist_callback_single version).
	virtual void modify_callback(class playlist_callback_single * p_callback,unsigned p_flags) = 0;
	
	//! Executes default doubleclick/enter action for specified item on specified playlist (starts playing the item unless overridden by a lock to do something else).
	virtual bool playlist_execute_default_action(t_size p_playlist,t_size p_item) = 0;

	
	//! Helper; removes all items from the playback queue.
	void queue_flush() {queue_remove_mask(bit_array_true());}
	//! Helper; returns whether there are items in the playback queue.
	bool queue_is_active() {return queue_get_count() > 0;}

	//! Helper; highlights currently playing item; returns true on success or false on failure (not playing or currently played item has been removed from playlist since playback started).
	bool highlight_playing_item();
	//! Helper; removes single playlist of specified index.
	bool remove_playlist(t_size p_playlist);
	//! Helper; removes single playlist of specified index, and switches to another playlist when possible.
	bool remove_playlist_switch(t_size p_playlist);

	//! Helper; returns whether specified item on specified playlist is selected or not.
	bool playlist_is_item_selected(t_size p_playlist,t_size p_item);
	//! Helper; retrieves metadb_handle of the specified playlist item. Returns true on success, false on failure (invalid parameters).
	bool playlist_get_item_handle(metadb_handle_ptr & p_out,t_size p_playlist,t_size p_item);
	//! Helper; retrieves metadb_handle of the specified playlist item; throws pfc::exception_invalid_params() on failure.
	metadb_handle_ptr playlist_get_item_handle(t_size playlist, t_size item);

	//! Moves selected items up/down in the playlist by specified offset.
	//! @param p_playlist Index of playlist to alter.
	//! @param p_delta Offset to move items by. Set it to a negative valuye to move up, or to a positive value to move down.
	//! @returns True on success, false on failure (e.g. playlist locked).
	bool playlist_move_selection(t_size p_playlist,int p_delta);
	//! Retrieves selection map of specific playlist, using bit_array_var interface.
	void playlist_get_selection_mask(t_size p_playlist,bit_array_var & out);
	void playlist_get_items(t_size p_playlist,pfc::list_base_t<metadb_handle_ptr> & out,const bit_array & p_mask);
	void playlist_get_all_items(t_size p_playlist,pfc::list_base_t<metadb_handle_ptr> & out);
	void playlist_get_selected_items(t_size p_playlist,pfc::list_base_t<metadb_handle_ptr> & out);
	
	//! Clears contents of specified playlist (removes all items from it).
	void playlist_clear(t_size p_playlist);
	bool playlist_add_items(t_size playlist,const pfc::list_base_const_t<metadb_handle_ptr> & data,const bit_array & p_selection);
	void playlist_clear_selection(t_size p_playlist);
	void playlist_remove_selection(t_size p_playlist,bool p_crop = false);
	

	//! Changes contents of the specified playlist to the specified items, trying to reuse existing playlist content as much as possible (preserving selection/focus/etc). Order of items in playlist not guaranteed to be the same as in the specified item list.
	//! @returns true if the playlist has been altered, false if there was nothing to update.
	bool playlist_update_content(t_size playlist, metadb_handle_list_cref content, bool bUndoBackup);
	
	//retrieving status
	t_size activeplaylist_get_item_count();
	void activeplaylist_enum_items(enum_items_callback & p_callback,const bit_array & p_mask);
	t_size activeplaylist_get_focus_item();//focus may be infinite if no item is focused
	bool activeplaylist_get_name(pfc::string_base & p_out);

	//modifying playlist
	bool activeplaylist_reorder_items(const t_size * order,t_size count);
	void activeplaylist_set_selection(const bit_array & affected,const bit_array & status);
	bool activeplaylist_remove_items(const bit_array & mask);
	bool activeplaylist_replace_item(t_size p_item,const metadb_handle_ptr & p_new_item);
	void activeplaylist_set_focus_item(t_size p_item);
	t_size activeplaylist_insert_items(t_size p_base,const pfc::list_base_const_t<metadb_handle_ptr> & data,const bit_array & p_selection);
	void activeplaylist_ensure_visible(t_size p_item);
	bool activeplaylist_rename(const char * p_name,t_size p_name_len);

	void activeplaylist_undo_backup();
	bool activeplaylist_undo_restore();
	bool activeplaylist_redo_restore();

	bool activeplaylist_is_item_selected(t_size p_item);
	bool activeplaylist_get_item_handle(metadb_handle_ptr & item,t_size p_item);
	metadb_handle_ptr activeplaylist_get_item_handle(t_size p_item);
	void activeplaylist_move_selection(int p_delta);
	void activeplaylist_get_selection_mask(bit_array_var & out);
	void activeplaylist_get_items(pfc::list_base_t<metadb_handle_ptr> & out,const bit_array & p_mask);
	void activeplaylist_get_all_items(pfc::list_base_t<metadb_handle_ptr> & out);
	void activeplaylist_get_selected_items(pfc::list_base_t<metadb_handle_ptr> & out);
	void activeplaylist_clear();

	bool activeplaylist_add_items(const pfc::list_base_const_t<metadb_handle_ptr> & data,const bit_array & p_selection);

	bool playlist_insert_items_filter(t_size p_playlist,t_size p_base,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,bool p_select);
	bool activeplaylist_insert_items_filter(t_size p_base,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,bool p_select);

	//! \deprecated (since 0.9.3) Use playlist_incoming_item_filter_v2::process_locations_async whenever possible
	bool playlist_insert_locations(t_size p_playlist,t_size p_base,const pfc::list_base_const_t<const char*> & p_urls,bool p_select,HWND p_parentwnd);
	//! \deprecated (since 0.9.3) Use playlist_incoming_item_filter_v2::process_locations_async whenever possible
	bool activeplaylist_insert_locations(t_size p_base,const pfc::list_base_const_t<const char*> & p_urls,bool p_select,HWND p_parentwnd);

	bool playlist_add_items_filter(t_size p_playlist,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,bool p_select);
	bool activeplaylist_add_items_filter(const pfc::list_base_const_t<metadb_handle_ptr> & p_data,bool p_select);

	bool playlist_add_locations(t_size p_playlist,const pfc::list_base_const_t<const char*> & p_urls,bool p_select,HWND p_parentwnd);
	bool activeplaylist_add_locations(const pfc::list_base_const_t<const char*> & p_urls,bool p_select,HWND p_parentwnd);

	void reset_playing_playlist();

	void activeplaylist_clear_selection();
	void activeplaylist_remove_selection(bool p_crop = false);

	void activeplaylist_item_format_title(t_size p_item,titleformat_hook * p_hook,pfc::string_base & out,const service_ptr_t<titleformat_object> & p_script,titleformat_text_filter * p_filter,play_control::t_display_level p_playback_info_level);

	void playlist_set_selection_single(t_size p_playlist,t_size p_item,bool p_state);
	void activeplaylist_set_selection_single(t_size p_item,bool p_state);

	t_size playlist_get_selection_count(t_size p_playlist,t_size p_max);
	t_size activeplaylist_get_selection_count(t_size p_max);

	bool playlist_get_focus_item_handle(metadb_handle_ptr & p_item,t_size p_playlist);
	bool activeplaylist_get_focus_item_handle(metadb_handle_ptr & item);

	t_size find_playlist(const char * p_name,t_size p_name_length = ~0);
	t_size find_or_create_playlist(const char * p_name,t_size p_name_length = ~0);
	t_size find_or_create_playlist_unlocked(const char * p_name,t_size p_name_length = ~0);

	t_size create_playlist_autoname(t_size p_index = infinite);

	bool activeplaylist_sort_by_format(const char * spec,bool p_sel_only);

	t_uint32 activeplaylist_lock_get_filter_mask();
	bool activeplaylist_is_undo_available();
	bool activeplaylist_is_redo_available();

	bool activeplaylist_execute_default_action(t_size p_item);

	void remove_items_from_all_playlists(const pfc::list_base_const_t<metadb_handle_ptr> & p_data);

	void active_playlist_fix();

	bool get_all_items(pfc::list_base_t<metadb_handle_ptr> & out);

	void playlist_activate_delta(int p_delta);
	void playlist_activate_next() {playlist_activate_delta(1);}
	void playlist_activate_previous() {playlist_activate_delta(-1);}


	t_size playlist_get_selected_count(t_size p_playlist,bit_array const & p_mask);
	t_size activeplaylist_get_selected_count(bit_array const & p_mask) {return playlist_get_selected_count(get_active_playlist(),p_mask);}

	bool playlist_find_item(t_size p_playlist,metadb_handle_ptr p_item,t_size & p_result);//inefficient, walks entire playlist
	bool playlist_find_item_selected(t_size p_playlist,metadb_handle_ptr p_item,t_size & p_result);//inefficient, walks entire playlist
	t_size playlist_set_focus_by_handle(t_size p_playlist,metadb_handle_ptr p_item);
	bool activeplaylist_find_item(metadb_handle_ptr p_item,t_size & p_result);//inefficient, walks entire playlist
	t_size activeplaylist_set_focus_by_handle(metadb_handle_ptr p_item);

	static void g_make_selection_move_permutation(t_size * p_output,t_size p_count,const bit_array & p_selection,int p_delta);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playlist_manager);
};

//! Extension of the playlist_manager service that manages playlist properties.
//! Playlist properties come in two flavors: persistent and runtime.
//! Persistent properties are blocks of binary that that will be preserved when the application is exited and restarted.
//! Runtime properties are service pointers that will be lost when the application exits.
//! \since 0.9.5
class NOVTABLE playlist_manager_v2 : public playlist_manager {
public:
	//! Write a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_stream stream that contains the data that will be associated with the property
	//! \param p_abort abort_callback that will be used when reading from p_stream
	virtual void playlist_set_property(t_size p_playlist,const GUID & p_property,stream_reader * p_stream,t_size p_size_hint,abort_callback & p_abort) = 0;
	//! Read a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_stream stream that will receive the stored data
	//! \param p_abort abort_callback that will be used when writing to p_stream
	//! \return true if the property exists, false otherwise
	virtual bool playlist_get_property(t_size p_playlist,const GUID & p_property,stream_writer * p_stream,abort_callback & p_abort) = 0;
	//! Test existence of a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \return true if the property exists, false otherwise
	virtual bool playlist_have_property(t_size p_playlist,const GUID & p_property) = 0;
	//! Remove a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \return true if the property existed, false otherwise
	virtual bool playlist_remove_property(t_size p_playlist,const GUID & p_property) = 0;

	//! Write a runtime playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_data service pointer that will be associated with the property
	virtual void playlist_set_runtime_property(t_size p_playlist,const GUID & p_property,service_ptr_t<service_base> p_data) = 0;
	//! Read a runtime playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_data base service pointer reference that will receive the stored servive pointer
	//! \return true if the property exists, false otherwise
	virtual bool playlist_get_runtime_property(t_size p_playlist,const GUID & p_property,service_ptr_t<service_base> & p_data) = 0;
	//! Test existence of a runtime playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \return true if the property exists, false otherwise
	virtual bool playlist_have_runtime_property(t_size p_playlist,const GUID & p_property) = 0;
	//! Remove a runtime playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \return true if the property existed, false otherwise
	virtual bool playlist_remove_runtime_property(t_size p_playlist,const GUID & p_property) = 0;

	//! Write a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_data array that contains the data that will be associated with the property
	template<typename t_array> void playlist_set_property(t_size p_playlist,const GUID & p_property,const t_array & p_data) {
		pfc::static_assert<sizeof(p_data[0]) == 1>();
		playlist_set_property(p_playlist,p_property,&stream_reader_memblock_ref(p_data),p_data.get_size(),abort_callback_impl());
	}
	//! Read a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_data array that will receive the stored data
	//! \return true if the property exists, false otherwise
	template<typename t_array> bool playlist_get_property(t_size p_playlist,const GUID & p_property,t_array & p_data) {
		pfc::static_assert<sizeof(p_data[0]) == 1>();
		typedef pfc::array_t<t_uint8,pfc::alloc_fast_aggressive> t_temp;
		t_temp temp;
		if (!playlist_get_property(p_playlist,p_property,&stream_writer_buffer_append_ref_t<t_temp>(temp),abort_callback_impl())) return false;
		p_data = temp;
		return true;
	}
	//! Read a runtime playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_data specific service pointer reference that will receive the stored servive pointer
	//! \return true if the property exists and can be converted to the type of p_data, false otherwise
	template<typename _t_interface> bool playlist_get_runtime_property(t_size p_playlist,const GUID & p_property,service_ptr_t<_t_interface> & p_data) {
		service_ptr_t<service_base> ptr;
		if (!playlist_get_runtime_property(p_playlist,p_property,ptr)) return false;
		return ptr->service_query_t(p_data);
	}
	
	//! Write a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_value integer that will be associated with the property
	template<typename _t_int>
	void playlist_set_property_int(t_size p_playlist,const GUID & p_property,_t_int p_value) {
		pfc::array_t<t_uint8> temp; temp.set_size(sizeof(_t_int));
		pfc::encode_little_endian(temp.get_ptr(),p_value);
		playlist_set_property(p_playlist,p_property,temp);
	}
	//! Read a persistent playlist property.
	//! \param p_playlist Index of the playlist
	//! \param p_property GUID that identifies the property
	//! \param p_value integer reference that will receive the stored data
	//! \return true if the property exists and if the data is compatible with p_value, false otherwise
	template<typename _t_int>
	bool playlist_get_property_int(t_size p_playlist,const GUID & p_property,_t_int & p_value) {
		pfc::array_t<t_uint8> temp;
		if (!playlist_get_property(p_playlist,p_property,temp)) return false;
		if (temp.get_size() != sizeof(_t_int)) return false;
		pfc::decode_little_endian(p_value,temp.get_ptr());
		return true;
	}

	FB2K_MAKE_SERVICE_INTERFACE(playlist_manager_v2,playlist_manager)
};

//! \since 0.9.5
class NOVTABLE playlist_manager_v3 : public playlist_manager_v2 {
	FB2K_MAKE_SERVICE_INTERFACE(playlist_manager_v3,playlist_manager_v2)
public:
	virtual t_size recycler_get_count() = 0;
	virtual void recycler_get_content(t_size which, metadb_handle_list_ref out) = 0;
	virtual void recycler_get_name(t_size which, pfc::string_base & out) = 0;
	virtual t_uint32 recycler_get_id(t_size which) = 0;
	virtual void recycler_purge(const bit_array & mask) = 0;
	virtual void recycler_restore(t_size which) = 0;

	void recycler_restore_by_id(t_uint32 id);
	t_size recycler_find_by_id(t_uint32 id);
};

//! \since 0.9.5.4
class NOVTABLE playlist_manager_v4 : public playlist_manager_v3 {
	FB2K_MAKE_SERVICE_INTERFACE(playlist_manager_v4, playlist_manager_v3)
public:
	virtual void playlist_get_sideinfo(t_size which, stream_writer * stream, abort_callback & abort) = 0;
	virtual t_size create_playlist_ex(const char * p_name,t_size p_name_length,t_size p_index, metadb_handle_list_cref content, stream_reader * sideInfo, abort_callback & abort) = 0;
};

class NOVTABLE playlist_callback
{
public:
	virtual void on_items_added(t_size p_playlist,t_size p_start, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection)=0;//inside any of these methods, you can call playlist APIs to get exact info about what happened (but only methods that read playlist state, not those that modify it)
	virtual void on_items_reordered(t_size p_playlist,const t_size * p_order,t_size p_count)=0;//changes selection too; doesnt actually change set of items that are selected or item having focus, just changes their order
	virtual void on_items_removing(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count)=0;//called before actually removing them
	virtual void on_items_removed(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count)=0;
	virtual void on_items_selection_change(t_size p_playlist,const bit_array & p_affected,const bit_array & p_state) = 0;
	virtual void on_item_focus_change(t_size p_playlist,t_size p_from,t_size p_to)=0;//focus may be -1 when no item has focus; reminder: focus may also change on other callbacks
	
	virtual void on_items_modified(t_size p_playlist,const bit_array & p_mask)=0;
	virtual void on_items_modified_fromplayback(t_size p_playlist,const bit_array & p_mask,play_control::t_display_level p_level)=0;

	struct t_on_items_replaced_entry
	{
		t_size m_index;
		metadb_handle_ptr m_old,m_new;
	};

	virtual void on_items_replaced(t_size p_playlist,const bit_array & p_mask,const pfc::list_base_const_t<t_on_items_replaced_entry> & p_data)=0;

	virtual void on_item_ensure_visible(t_size p_playlist,t_size p_idx)=0;

	virtual void on_playlist_activate(t_size p_old,t_size p_new) = 0;
	virtual void on_playlist_created(t_size p_index,const char * p_name,t_size p_name_len) = 0;
	virtual void on_playlists_reorder(const t_size * p_order,t_size p_count) = 0;
	virtual void on_playlists_removing(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) = 0;
	virtual void on_playlists_removed(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) = 0;
	virtual void on_playlist_renamed(t_size p_index,const char * p_new_name,t_size p_new_name_len) = 0;

	virtual void on_default_format_changed() = 0;
	virtual void on_playback_order_changed(t_size p_new_index) = 0;
	virtual void on_playlist_locked(t_size p_playlist,bool p_locked) = 0;

	enum {
		flag_on_items_added					= 1 << 0,
		flag_on_items_reordered				= 1 << 1,
		flag_on_items_removing				= 1 << 2,
		flag_on_items_removed				= 1 << 3,
		flag_on_items_selection_change		= 1 << 4,
		flag_on_item_focus_change			= 1 << 5,
		flag_on_items_modified				= 1 << 6,
		flag_on_items_modified_fromplayback	= 1 << 7,
		flag_on_items_replaced				= 1 << 8,
		flag_on_item_ensure_visible			= 1 << 9,
		flag_on_playlist_activate			= 1 << 10,
		flag_on_playlist_created			= 1 << 11,
		flag_on_playlists_reorder			= 1 << 12,
		flag_on_playlists_removing			= 1 << 13,
		flag_on_playlists_removed			= 1 << 14,
		flag_on_playlist_renamed			= 1 << 15,
		flag_on_default_format_changed		= 1 << 16,
		flag_on_playback_order_changed		= 1 << 17,
		flag_on_playlist_locked				= 1 << 18,

		flag_all							= ~0,
		flag_item_ops						= flag_on_items_added | flag_on_items_reordered | flag_on_items_removing | flag_on_items_removed | flag_on_items_selection_change | flag_on_item_focus_change | flag_on_items_modified | flag_on_items_modified_fromplayback | flag_on_items_replaced | flag_on_item_ensure_visible,
		flag_playlist_ops					= flag_on_playlist_activate | flag_on_playlist_created | flag_on_playlists_reorder | flag_on_playlists_removing | flag_on_playlists_removed | flag_on_playlist_renamed | flag_on_playlist_locked,
	};
protected:
	playlist_callback() {}
	~playlist_callback() {}
};

class NOVTABLE playlist_callback_static : public service_base, public playlist_callback 
{
public:
	virtual unsigned get_flags() = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playlist_callback_static);
};

class NOVTABLE playlist_callback_single
{
public:
	virtual void on_items_added(t_size p_base, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection)=0;//inside any of these methods, you can call playlist APIs to get exact info about what happened (but only methods that read playlist state, not those that modify it)
	virtual void on_items_reordered(const t_size * p_order,t_size p_count)=0;//changes selection too; doesnt actually change set of items that are selected or item having focus, just changes their order
	virtual void on_items_removing(const bit_array & p_mask,t_size p_old_count,t_size p_new_count)=0;//called before actually removing them
	virtual void on_items_removed(const bit_array & p_mask,t_size p_old_count,t_size p_new_count)=0;
	virtual void on_items_selection_change(const bit_array & p_affected,const bit_array & p_state) = 0;
	virtual void on_item_focus_change(t_size p_from,t_size p_to)=0;//focus may be -1 when no item has focus; reminder: focus may also change on other callbacks
	virtual void on_items_modified(const bit_array & p_mask)=0;
	virtual void on_items_modified_fromplayback(const bit_array & p_mask,play_control::t_display_level p_level)=0;
	virtual void on_items_replaced(const bit_array & p_mask,const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry> & p_data)=0;
	virtual void on_item_ensure_visible(t_size p_idx)=0;

	virtual void on_playlist_switch() = 0;
	virtual void on_playlist_renamed(const char * p_new_name,t_size p_new_name_len) = 0;
	virtual void on_playlist_locked(bool p_locked) = 0;

	virtual void on_default_format_changed() = 0;
	virtual void on_playback_order_changed(t_size p_new_index) = 0;

	enum {
		flag_on_items_added					= 1 << 0,
		flag_on_items_reordered				= 1 << 1,
		flag_on_items_removing				= 1 << 2,
		flag_on_items_removed				= 1 << 3,
		flag_on_items_selection_change		= 1 << 4,
		flag_on_item_focus_change			= 1 << 5,
		flag_on_items_modified				= 1 << 6,
		flag_on_items_modified_fromplayback = 1 << 7,
		flag_on_items_replaced				= 1 << 8,
		flag_on_item_ensure_visible			= 1 << 9,
		flag_on_playlist_switch				= 1 << 10,
		flag_on_playlist_renamed			= 1 << 11,
		flag_on_playlist_locked				= 1 << 12,
		flag_on_default_format_changed		= 1 << 13,
		flag_on_playback_order_changed		= 1 << 14,
		flag_all							= ~0,
	};
protected:
	playlist_callback_single() {}
	~playlist_callback_single() {}
};

//! playlist_callback implementation helper - registers itself on creation / unregisters on destruction. Must not be instantiated statically!
class playlist_callback_impl_base : public playlist_callback {
public:
	playlist_callback_impl_base(t_uint32 p_flags = 0) {
		static_api_ptr_t<playlist_manager>()->register_callback(this,p_flags);
	}
	~playlist_callback_impl_base() {
		static_api_ptr_t<playlist_manager>()->unregister_callback(this);
	}
	void set_callback_flags(t_uint32 p_flags) {
		static_api_ptr_t<playlist_manager>()->modify_callback(this,p_flags);
	}
	//dummy implementations - avoid possible pure virtual function calls!
	void on_items_added(t_size p_playlist,t_size p_start, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection) {}
	void on_items_reordered(t_size p_playlist,const t_size * p_order,t_size p_count) {}
	void on_items_removing(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
	void on_items_removed(t_size p_playlist,const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
	void on_items_selection_change(t_size p_playlist,const bit_array & p_affected,const bit_array & p_state) {}
	void on_item_focus_change(t_size p_playlist,t_size p_from,t_size p_to) {}
	
	void on_items_modified(t_size p_playlist,const bit_array & p_mask) {}
	void on_items_modified_fromplayback(t_size p_playlist,const bit_array & p_mask,play_control::t_display_level p_level) {}

	void on_items_replaced(t_size p_playlist,const bit_array & p_mask,const pfc::list_base_const_t<t_on_items_replaced_entry> & p_data) {}

	void on_item_ensure_visible(t_size p_playlist,t_size p_idx) {}

	void on_playlist_activate(t_size p_old,t_size p_new) {}
	void on_playlist_created(t_size p_index,const char * p_name,t_size p_name_len) {}
	void on_playlists_reorder(const t_size * p_order,t_size p_count) {}
	void on_playlists_removing(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
	void on_playlists_removed(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
	void on_playlist_renamed(t_size p_index,const char * p_new_name,t_size p_new_name_len) {}

	void on_default_format_changed() {}
	void on_playback_order_changed(t_size p_new_index) {}
	void on_playlist_locked(t_size p_playlist,bool p_locked) {}
};

//! playlist_callback_single implementation helper - registers itself on creation / unregisters on destruction. Must not be instantiated statically!
class playlist_callback_single_impl_base : public playlist_callback_single {
protected:
	playlist_callback_single_impl_base(t_uint32 p_flags = 0) {
		static_api_ptr_t<playlist_manager>()->register_callback(this,p_flags);
	}
	void set_callback_flags(t_uint32 p_flags) {
		static_api_ptr_t<playlist_manager>()->modify_callback(this,p_flags);
	}
	~playlist_callback_single_impl_base() {
		static_api_ptr_t<playlist_manager>()->unregister_callback(this);
	}

	//dummy implementations - avoid possible pure virtual function calls!
	void on_items_added(t_size p_base, const pfc::list_base_const_t<metadb_handle_ptr> & p_data,const bit_array & p_selection) {}
	void on_items_reordered(const t_size * p_order,t_size p_count) {}
	void on_items_removing(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
	void on_items_removed(const bit_array & p_mask,t_size p_old_count,t_size p_new_count) {}
	void on_items_selection_change(const bit_array & p_affected,const bit_array & p_state) {}
	void on_item_focus_change(t_size p_from,t_size p_to) {}
	void on_items_modified(const bit_array & p_mask) {}
	void on_items_modified_fromplayback(const bit_array & p_mask,play_control::t_display_level p_level) {}
	void on_items_replaced(const bit_array & p_mask,const pfc::list_base_const_t<playlist_callback::t_on_items_replaced_entry> & p_data) {}
	void on_item_ensure_visible(t_size p_idx) {}

	void on_playlist_switch() {}
	void on_playlist_renamed(const char * p_new_name,t_size p_new_name_len) {}
	void on_playlist_locked(bool p_locked) {}

	void on_default_format_changed() {}
	void on_playback_order_changed(t_size p_new_index) {}

	PFC_CLASS_NOT_COPYABLE(playlist_callback_single_impl_base,playlist_callback_single_impl_base);
};

class playlist_callback_single_static : public service_base, public playlist_callback_single
{
public:
	virtual unsigned get_flags() = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playlist_callback_single_static);
};


//! Class used for async processing of IDataObject. Content of IDataObject can be dumped into dropped_files_data without any time-consuming operations - won't block calling app when used inside drag&drop handler - and actual time-consuming processing (listing directories and reading infos) can be done later.\n
//! \deprecated In 0.9.3 and up, instead of going thru dropped_files_data, you can use playlist_incoming_item_filter_v2::process_dropped_files_async().
class NOVTABLE dropped_files_data {
public:
	virtual void set_paths(pfc::string_list_const const & p_paths) = 0;
	virtual void set_handles(const pfc::list_base_const_t<metadb_handle_ptr> & p_handles) = 0;
protected:
	dropped_files_data() {}
	~dropped_files_data() {}
};


class NOVTABLE playlist_incoming_item_filter : public service_base {
public:
	//! Pre-sorts incoming items according to user-configured settings, removes duplicates. \n
	//! @param in Items to process.
	//! @param out Receives processed item list. \n
	//! NOTE: because of an implementation bug in pre-0.9.5, the output list should be emptied before calling filter_items(), otherwise the results will be inconsistent/unpredictable.
	//! @returns True when there's one or more item in the output list, false when the output list is empty.
	virtual bool filter_items(metadb_handle_list_cref in,metadb_handle_list_ref out) = 0;
	
	//! Converts one or more paths to a list of metadb_handles; displays a progress dialog.\n
	//! Note that this function creates modal dialog and does not return until the operation has completed.
	//! @returns True on success, false on user abort.
	//! \deprecated Use playlist_incoming_item_filter_v2::process_locations_async() when possible.
	virtual bool process_locations(const pfc::list_base_const_t<const char*> & p_urls,pfc::list_base_t<metadb_handle_ptr> & p_out,bool p_filter,const char * p_restrict_mask_override, const char * p_exclude_mask_override,HWND p_parentwnd) = 0;
	
	//! Converts an IDataObject to a list of metadb_handles.
	//! Using this function is strongly disrecommended as it implies blocking the drag&drop source app (as well as our app).\n
	//! @returns True on success, false on user abort or unknown data format.
	//! \deprecated Use playlist_incoming_item_filter_v2::process_dropped_files_async() when possible.
	virtual bool process_dropped_files(interface IDataObject * pDataObject,pfc::list_base_t<metadb_handle_ptr> & p_out,bool p_filter,HWND p_parentwnd) = 0;

	//! Checks whether IDataObject contains one of known data formats that can be translated to a list of metadb_handles.
	virtual bool process_dropped_files_check(interface IDataObject * pDataObject) = 0;
	
	//! Checks whether IDataObject contains our own private data format (drag&drop within the app etc).
	virtual bool process_dropped_files_check_if_native(interface IDataObject * pDataObject) = 0;
	
	//! Creates an IDataObject from specified metadb_handle list. The caller is responsible for releasing the returned object. It is recommended that you use create_dataobject_ex() to get an autopointer that ensures proper deletion.
	virtual interface IDataObject * create_dataobject(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;

	//! Checks whether IDataObject contains one of known data formats that can be translated to a list of metadb_handles.\n
	//! This function also returns drop effects to use (see: IDropTarget::DragEnter(), IDropTarget::DragOver() ). In certain cases, drag effects are necessary for drag&drop to work at all (such as dragging links from IE).\n
	virtual bool process_dropped_files_check_ex(interface IDataObject * pDataObject, DWORD * p_effect) = 0;

	//! Dumps IDataObject content to specified dropped_files_data object, without any time-consuming processing.\n
	//! Using this function instead of process_dropped_files() and processing dropped_files_data outside drop handler allows you to avoid blocking drop source app when processing large directories etc.\n
	//! Note: since 0.9.3, it is recommended to use playlist_incoming_item_filter_v2::process_dropped_files_async() instead.
	//! @returns True on success, false when IDataObject does not contain any of known data formats.
	virtual bool process_dropped_files_delayed(dropped_files_data & p_out,interface IDataObject * pDataObject) = 0;

	//! Helper - calls process_locations() with a single URL. See process_locations() for more info.
	bool process_location(const char * url,pfc::list_base_t<metadb_handle_ptr> & out,bool filter,const char * p_mask,const char * p_exclude,HWND p_parentwnd);
	//! Helper - returns a pfc::com_ptr_t<> rather than a raw pointer.
	pfc::com_ptr_t<interface IDataObject> create_dataobject_ex(metadb_handle_list_cref data);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playlist_incoming_item_filter);
};

//! For use with playlist_incoming_item_filter_v2::process_locations_async().
//! \since 0.9.3
class NOVTABLE process_locations_notify : public service_base {
public:
	virtual void on_completion(const pfc::list_base_const_t<metadb_handle_ptr> & p_items) = 0;
	virtual void on_aborted() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(process_locations_notify,service_base);
};

typedef service_ptr_t<process_locations_notify> process_locations_notify_ptr;

//! \since 0.9.3
class NOVTABLE playlist_incoming_item_filter_v2 : public playlist_incoming_item_filter {
public:
	enum {
		//! Set this to disable presorting (according to user settings) and duplicate removal in output list. Should be unset in most cases.
		op_flag_no_filter		= 1 << 0,
		//! Set this flag to make the progress dialog not steal focus on creation.
		op_flag_background		= 1 << 1,
		//! Set this flag to delay the progress dialog becoming visible, so it does not appear at all during short operations. Also implies op_flag_background effect.
		op_flag_delay_ui		= 1 << 2,
	};

	//! Converts one or more paths to a list of metadb_handles. The function returns immediately; specified callback object receives results when the operation has completed.
	//! @param p_urls List of paths to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_restrict_mask_override Override of "restrict incoming items to" setting. Pass NULL to use the value from preferences.
	//! @param p_exclude_mask_override Override of "exclude file types" setting. Pass NULL to use value from preferences.
	//! @param p_parentwnd Parent window for spawned progress dialogs.
	//! @param p_notify Callback receiving notifications about success/abort of the operation as well as output item list.
	virtual void process_locations_async(const pfc::list_base_const_t<const char*> & p_urls,t_uint32 p_op_flags,const char * p_restrict_mask_override, const char * p_exclude_mask_override,HWND p_parentwnd,process_locations_notify_ptr p_notify) = 0;

	//! Converts an IDataObject to a list of metadb_handles. The function returns immediately; specified callback object receives results when the operation has completed.
	//! @param p_dataobject IDataObject to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_parentwnd Parent window for spawned progress dialogs.
	//! @param p_notify Callback receiving notifications about success/abort of the operation as well as output item list.
	virtual void process_dropped_files_async(interface IDataObject * p_dataobject,t_uint32 p_op_flags,HWND p_parentwnd,process_locations_notify_ptr p_notify) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(playlist_incoming_item_filter_v2,playlist_incoming_item_filter);
};

//! \since 0.9.5
class playlist_incoming_item_filter_v3 : public playlist_incoming_item_filter_v2 {
public:
	virtual bool auto_playlist_name(metadb_handle_list_cref data,pfc::string_base & out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(playlist_incoming_item_filter_v3,playlist_incoming_item_filter_v2)
};

//! Implementation of dropped_files_data.
class dropped_files_data_impl : public dropped_files_data {
public:
	dropped_files_data_impl() : m_is_paths(false) {}
	void set_paths(pfc::string_list_const const & p_paths) {
		m_is_paths = true;
		m_paths = p_paths;
	}
	void set_handles(const pfc::list_base_const_t<metadb_handle_ptr> & p_handles) {
		m_is_paths = false;
		m_handles = p_handles;
	}

	void to_handles_async(bool p_filter,HWND p_parentwnd,service_ptr_t<process_locations_notify> p_notify);
	//! @param p_op_flags Can be null, or one or more of playlist_incoming_item_filter_v2::op_flag_* enum values combined, altering behaviors of the operation.
	void to_handles_async_ex(t_uint32 p_op_flags,HWND p_parentwnd,service_ptr_t<process_locations_notify> p_notify);
	bool to_handles(pfc::list_base_t<metadb_handle_ptr> & p_out,bool p_filter,HWND p_parentwnd);
private:
	pfc::string_list_impl m_paths;
	metadb_handle_list m_handles;
	bool m_is_paths;
};


class NOVTABLE playback_queue_callback : public service_base
{
public:
	enum t_change_origin {
		changed_user_added,
		changed_user_removed,
		changed_playback_advance,
	};
	virtual void on_changed(t_change_origin p_origin) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playback_queue_callback);
};


class playlist_lock_change_notify : private playlist_callback_single_impl_base {
public:
	playlist_lock_change_notify() : playlist_callback_single_impl_base(flag_on_playlist_switch|flag_on_playlist_locked) {}
protected:
	virtual void on_lock_state_change() {}
	bool is_playlist_command_available(t_uint32 what) const {
		static_api_ptr_t<playlist_manager> api;
		const t_size active = api->get_active_playlist();
		if (active == ~0) return false;
		return (api->playlist_lock_get_filter_mask(active) & what) == 0;
	}
private:
	void on_playlist_switch() {on_lock_state_change();}
	void on_playlist_locked(bool p_locked) {on_lock_state_change();}
};
