/*!
This service implements methods allowing you to interact with the Media Library.\n
All methods are valid from main thread only, unless noted otherwise.\n
To avoid race conditions, methods that alter library contents should not be called from inside global callbacks.\n
Usage: Use static_api_ptr_t<library_manager> to instantiate. \n

Future compatibility notes: \n
In 0.9.6, the Media Library backend will be entirely reimplemented to perform tracking of folder content changes on its own. This API will still be provided for backwards compatibility, though most of methods will become stubs as their original purpose will be no longer valid. \n
To keep your component working sanely in future foobar2000 releases, do not depend on functions flagged as scheduled to be dropped - you can still call them, but keep in mind that they will become meaningless in the next major release.
*/

class NOVTABLE library_manager : public service_base {
public:
	//! Interface for use with library_manager::enum_items().
	class NOVTABLE enum_callback {
	public:
		//! Return true to continue enumeration, false to abort.
		virtual bool on_item(const metadb_handle_ptr & p_item) = 0;
	};

	//! Returns whether the specified item is in the Media Library or not.
	virtual bool is_item_in_library(const metadb_handle_ptr & p_item) = 0;
	//! Returns whether current user settings allow the specified item to be added to the Media Library or not.
	virtual bool is_item_addable(const metadb_handle_ptr & p_item) = 0;
	//! Returns whether current user settings allow the specified item path to be added to the Media Library or not.
	virtual bool is_path_addable(const char * p_path) = 0;
	//! Retrieves path of the specified item relative to the Media Library folder it is in. Returns true on success, false when the item is not in the Media Library.
	//! SPECIAL WARNING: to allow multi-CPU optimizations to parse relative track paths, this API works in threads other than the main app thread. Main thread MUST be blocked while working in such scenarios, it's NOT safe to call from worker threads while the Media Library content/configuration might be getting altered.
	virtual bool get_relative_path(const metadb_handle_ptr & p_item,pfc::string_base & p_out) = 0;
	//! Calls callback method for every item in the Media Library. Note that order of items in Media Library is undefined.
	virtual void enum_items(enum_callback & p_callback) = 0;
	//! Scheduled to be dropped in 0.9.6 (will do nothing). \n
	//! Adds specified items to the Media Library (items actually added will be filtered according to user settings).
	virtual void add_items(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
	//! Scheduled to be dropped in 0.9.6 (will do nothing). \n
	//! Removes specified items from the Media Library (does nothing if specific item is not in the Media Library).
	virtual void remove_items(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
	//! Scheduled to be dropped in 0.9.6 (will do nothing). \n
	//! Adds specified items to the Media Library (items actually added will be filtered according to user settings). The difference between this and add_items() is that items are not added immediately; the operation is queued and executed later, so it is safe to call from e.g. global callbacks.
	virtual void add_items_async(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;

	//! Scheduled to be dropped in 0.9.6 (will do nothing). \n
	//! For internal use only; p_data must be sorted by metadb::path_compare; use file_operation_callback static methods instead of calling this directly.
	virtual void on_files_deleted_sorted(const pfc::list_base_const_t<const char *> & p_data) = 0;

	//! Retrieves the entire Media Library content.
	virtual void get_all_items(pfc::list_base_t<metadb_handle_ptr> & p_out) = 0;

	//! Returns whether Media Library functionality is enabled or not (to be exact: whether there's at least one Media Library folder present in settings), for e.g. notifying the user to change settings when trying to use a Media Library viewer without having configured the Media Library first.
	virtual bool is_library_enabled() = 0;
	//! Pops up the Media Library preferences page.
	virtual void show_preferences() = 0;

	//! Scheduled to be dropped in 0.9.6. \n
	//! Deprecated; use library_manager_v2::rescan_async() when possible.\n
	//! Rescans user-specified Media Library directories for new files and removes references to files that no longer exist from the Media Library.\n
	//! Note that this function creates modal dialog and does not return until the operation has completed.\n
	virtual void rescan() = 0;
	
	//! Scheduled to be dropped in 0.9.6. \n
	//! Deprecated; use library_manager_v2::check_dead_entries_async() when possible.\n
	//! Hints Media Library about possible dead items, typically used for "remove dead entries" context action in ML viewers. The implementation will verify whether the items are actually dead before ML contents are altered.\n
	//! Note that this function creates modal dialog and does not return until the operation has completed.\n
	virtual void check_dead_entries(const pfc::list_base_t<metadb_handle_ptr> & p_list) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(library_manager);
};

//! \since 0.9.3
class NOVTABLE library_manager_v2 : public library_manager {
public:
	//! Scheduled to be dropped in 0.9.6 (will always return false). \n
	//! Returns whether a rescan process is currently running.
	virtual bool is_rescan_running() = 0;

	//! Scheduled to be dropped in 0.9.6 (will do nothing and instantly signal completion). \n
	//! Starts an async rescan process. Note that if another process is already running, the process is silently aborted.
	//! @param p_parent Parent window for displayed progress dialog.
	//! @param p_notify Allows caller to receive notifications about the process finishing. Status code: 1 on success, 0 on user abort. Pass NULL if caller doesn't care.
	virtual void rescan_async(HWND p_parent,completion_notify_ptr p_notify) = 0;

	//! Scheduled to be dropped in 0.9.6 (will do nothing and instantly signal completion). \n
	//! Hints Media Library about possible dead items, typically used for "remove dead entries" context action in ML viewers. The implementation will verify whether the items are actually dead before ML contents are altered.\n
	//! @param p_list List of items to process.
	//! @param p_parent Parent window for displayed progress dialog.
	//! @param p_notify Allows caller to receive notifications about the process finishing. Status code: 1 on success, 0 on user abort. Pass NULL if caller doesn't care.
	virtual void check_dead_entries_async(const pfc::list_base_const_t<metadb_handle_ptr> & p_list,HWND p_parent,completion_notify_ptr p_notify) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(library_manager_v2,library_manager);
};


class NOVTABLE library_callback_dynamic {
public:
	//! Called when new items are added to the Media Library.
	virtual void on_items_added(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
	//! Called when some items have been removed from the Media Library.
	virtual void on_items_removed(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
	//! Called when some items in the Media Library have been modified.
	virtual void on_items_modified(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
};

//! \since 0.9.5
class NOVTABLE library_manager_v3 : public library_manager_v2 {
public:
	//! Retrieves directory path and subdirectory/filename formatting scheme for newly encoded/copied/moved tracks.
	//! @returns True on success, false when the feature has not been configured.
	virtual bool get_new_file_pattern_tracks(pfc::string_base & p_directory,pfc::string_base & p_format) = 0;
	//! Retrieves directory path and subdirectory/filename formatting scheme for newly encoded/copied/moved full album images.
	//! @returns True on success, false when the feature has not been configured.
	virtual bool get_new_file_pattern_images(pfc::string_base & p_directory,pfc::string_base & p_format) = 0;

	virtual void register_callback(library_callback_dynamic * p_callback) = 0;
	virtual void unregister_callback(library_callback_dynamic * p_callback) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(library_manager_v3,library_manager_v2);
};

class library_callback_dynamic_impl_base : public library_callback_dynamic {
public:
	library_callback_dynamic_impl_base() {static_api_ptr_t<library_manager_v3>()->register_callback(this);}
	~library_callback_dynamic_impl_base() {static_api_ptr_t<library_manager_v3>()->unregister_callback(this);}

	//stub implementations - avoid pure virtual function call issues
	void on_items_added(metadb_handle_list_cref p_data) {}
	void on_items_removed(metadb_handle_list_cref p_data) {}
	void on_items_modified(metadb_handle_list_cref p_data) {}

	PFC_CLASS_NOT_COPYABLE_EX(library_callback_dynamic_impl_base);
};

//! Callback service receiving notifications about Media Library content changes. Methods called only from main thread.\n
//! Use library_callback_factory_t template to register.
class NOVTABLE library_callback : public service_base {
public:
	//! Called when new items are added to the Media Library.
	virtual void on_items_added(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
	//! Called when some items have been removed from the Media Library.
	virtual void on_items_removed(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;
	//! Called when some items in the Media Library have been modified.
	virtual void on_items_modified(const pfc::list_base_const_t<metadb_handle_ptr> & p_data) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(library_callback);
};

template<typename T>
class library_callback_factory_t : public service_factory_single_t<T> {};

//! Implement this service to appear on "library viewers" list in Media Library preferences page.\n
//! Use library_viewer_factory_t to register.
class NOVTABLE library_viewer : public service_base {
public:
	//! Retrieves GUID of your preferences page (pfc::guid_null if you don't have one).
	virtual GUID get_preferences_page() = 0;
	//! Queries whether "activate" action is supported (relevant button will be disabled if it's not).
	virtual bool have_activate() = 0;
	//! Activates your Media Library viewer component (e.g. shows its window).
	virtual void activate() = 0;
	//! Retrieves GUID of your library_viewer implementation, for internal identification. Note that this not the same as preferences page GUID.
	virtual GUID get_guid() = 0;
	//! Retrieves name of your Media Library viewer, a null-terminated UTF-8 encoded string.
	virtual const char * get_name() = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(library_viewer);
};

template<typename T>
class library_viewer_factory_t : public service_factory_single_t<T> {};




//! \since 0.9.5.4
//! Allows you to spawn a popup Media Library Search window with any query string that you specify. \n
//! Usage: static_api_ptr_t<library_search_ui>()->show("querygoeshere");
class NOVTABLE library_search_ui : public service_base {
public:
	virtual void show(const char * query) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(library_search_ui)
};
