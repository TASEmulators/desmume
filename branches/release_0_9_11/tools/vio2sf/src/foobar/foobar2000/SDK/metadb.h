//! API for tag read/write operations. Legal to call from main thread only, except for hint_multi_async() / hint_async() / hint_reader().\n
//! Implemented only by core, do not reimplement.\n
//! Use static_api_ptr_t template to access metadb_io methods.\n
//! WARNING: Methods that perform file access (tag reads/writes) run a modal message loop. They SHOULD NOT be called from global callbacks and such.
class NOVTABLE metadb_io : public service_base
{
public:
	enum t_load_info_type {
		load_info_default,
		load_info_force,
		load_info_check_if_changed
	};

	enum t_update_info_state {
		update_info_success,
		update_info_aborted,
		update_info_errors,		
	};
	
	enum t_load_info_state {
		load_info_success,
		load_info_aborted,
		load_info_errors,		
	};

	//! No longer use - returns false always.
	__declspec(deprecated) virtual bool is_busy() = 0;
	//! No longer used - returns false always.
	__declspec(deprecated) virtual bool is_updating_disabled() = 0;
	//! No longer used - returns false always.
	__declspec(deprecated) virtual bool is_file_updating_blocked() = 0;
	//! No longer used.
	__declspec(deprecated) virtual void highlight_running_process() = 0;
	//! Loads tags from multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_load_info_state load_info_multi(metadb_handle_list_cref p_list,t_load_info_type p_type,HWND p_parent_window,bool p_show_errors) = 0;
	//! Updates tags on multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_update_info_state update_info_multi(metadb_handle_list_cref p_list,const pfc::list_base_const_t<file_info*> & p_new_info,HWND p_parent_window,bool p_show_errors) = 0;
	//! Rewrites tags on multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_update_info_state rewrite_info_multi(metadb_handle_list_cref p_list,HWND p_parent_window,bool p_show_errors) = 0;
	//! Removes tags from multiple items. Use the async version in metadb_io_v2 instead if possible.
	__declspec(deprecated) virtual t_update_info_state remove_info_multi(metadb_handle_list_cref p_list,HWND p_parent_window,bool p_show_errors) = 0;

	virtual void hint_multi(metadb_handle_list_cref p_list,const pfc::list_base_const_t<const file_info*> & p_infos,const pfc::list_base_const_t<t_filestats> & p_stats,const bit_array & p_fresh_mask) = 0;

	virtual void hint_multi_async(metadb_handle_list_cref p_list,const pfc::list_base_const_t<const file_info*> & p_infos,const pfc::list_base_const_t<t_filestats> & p_stats,const bit_array & p_fresh_mask) = 0;

	virtual void hint_reader(service_ptr_t<class input_info_reader> p_reader,const char * p_path,abort_callback & p_abort) = 0;

	//! For internal use only.
	virtual void path_to_handles_simple(const char * p_path, metadb_handle_list_ref p_out) = 0;

	//! Dispatches metadb_io_callback calls with specified items. To be used with metadb_display_field_provider when your component needs specified items refreshed.
	virtual void dispatch_refresh(metadb_handle_list_cref p_list) = 0;

	void dispatch_refresh(metadb_handle_ptr const & handle) {dispatch_refresh(pfc::list_single_ref_t<metadb_handle_ptr>(handle));}

	void hint_async(metadb_handle_ptr p_item,const file_info & p_info,const t_filestats & p_stats,bool p_fresh);

	__declspec(deprecated) t_load_info_state load_info(metadb_handle_ptr p_item,t_load_info_type p_type,HWND p_parent_window,bool p_show_errors);
	__declspec(deprecated) t_update_info_state update_info(metadb_handle_ptr p_item,file_info & p_info,HWND p_parent_window,bool p_show_errors);
	
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_io);
};

//! Implementing this class gives you direct control over which part of file_info gets altered during a tag update uperation. To be used with metadb_io_v2::update_info_async().
class NOVTABLE file_info_filter : public service_base {
public:
	//! Alters specified file_info entry; called as a part of tag update process. Specified file_info has been read from a file, and will be written back.\n
	//! WARNING: This will be typically called from another thread than main app thread (precisely, from thread created by tag updater). You should copy all relevant data to members of your file_info_filter instance in constructor and reference only member data in apply_filter() implementation.
	//! @returns True when you have altered file_info and changes need to be written back to the file; false if no changes have been made.
	virtual bool apply_filter(metadb_handle_ptr p_location,t_filestats p_stats,file_info & p_info) = 0;
	
	FB2K_MAKE_SERVICE_INTERFACE(file_info_filter,service_base);
};

//! Advanced interface for passing infos read from files to metadb backend. Use metadb_io_v2::create_hint_list() to instantiate.
class NOVTABLE metadb_hint_list : public service_base {
public:
	//! Adds a hint to the list.
	//! @param p_location Location of the item the hint applies to.
	//! @param p_info file_info object describing the item.
	//! @param p_stats Information about the file containing item the hint applies to.
	//! @param p_freshflag Set to true if the info has been directly read from the file, false if it comes from another source such as a playlist file.
	virtual void add_hint(metadb_handle_ptr const & p_location,const file_info & p_info,const t_filestats & p_stats,bool p_freshflag) = 0;
	//! Reads info from specified info reader instance and adds hints. May throw an exception in case info read has failed.
	virtual void add_hint_reader(const char * p_path,service_ptr_t<input_info_reader> const & p_reader,abort_callback & p_abort) = 0;
	//! Call this when you're done working with this metadb_hint_list instance, to apply hints and dispatch callbacks. If you don't call this, all added hints will be ignored.
	virtual void on_done() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(metadb_hint_list,service_base);
};

//! New in 0.9.3. Extends metadb_io functionality with nonblocking versions of tag read/write functions, and some other utility features.
class NOVTABLE metadb_io_v2 : public metadb_io {
public:
	enum {
		//! By default, when some part of requested operation could not be performed for reasons other than user abort, a popup dialog with description of the problem is spawned.\n
		//! Set this flag to disable error notification.
		op_flag_no_errors		= 1 << 0,
		//! Set this flag to make the progress dialog not steal focus on creation.
		op_flag_background		= 1 << 1,
		//! Set this flag to delay the progress dialog becoming visible, so it does not appear at all during short operations. Also implies op_flag_background effect.
		op_flag_delay_ui		= 1 << 2,
	};

	//! Preloads information from the specified tracks.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_load_info_state values. Can be null if caller doesn't care.
	virtual void load_info_async(metadb_handle_list_cref p_list,t_load_info_type p_type,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;
	//! Updates tags of the specified tracks.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	//! @param p_filter Callback handling actual file_info alterations. Typically used to replace entire meta part of file_info, or to alter something else such as ReplayGain while leaving meta intact.
	virtual void update_info_async(metadb_handle_list_cref p_list,service_ptr_t<file_info_filter> p_filter,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;
	//! Rewrites tags of the specified tracks; similar to update_info_async() but using last known/cached file_info values rather than values passed by caller.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	virtual void rewrite_info_async(metadb_handle_list_cref p_list,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;
	//! Strips all tags / metadata fields from the specified tracks.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	virtual void remove_info_async(metadb_handle_list_cref p_list,HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify) = 0;

	//! Creates a metadb_hint_list object.
	virtual metadb_hint_list::ptr create_hint_list() = 0;

	//! Updates tags of the specified tracks. Helper; uses update_info_async internally.
	//! @param p_list List of items to process.
	//! @param p_op_flags Can be null, or one or more of op_flag_* enum values combined, altering behaviors of the operation.
	//! @param p_notify Called when the task is completed. Status code is one of t_update_info values. Can be null if caller doesn't care.
	//! @param p_new_info New infos to write to specified items.
	void update_info_async_simple(metadb_handle_list_cref p_list,const pfc::list_base_const_t<const file_info*> & p_new_info, HWND p_parent_window,t_uint32 p_op_flags,completion_notify_ptr p_notify);

	FB2K_MAKE_SERVICE_INTERFACE(metadb_io_v2,metadb_io);
};


//! Dynamically-registered version of metadb_io_callback. See metadb_io_callback for documentation, register instances using metadb_io_v3::register_callback(). It's recommended that you use the metadb_io_callback_dynamic_impl_base helper class to manage registration/unregistration.
class NOVTABLE metadb_io_callback_dynamic {
public:
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) = 0;
};

//! New (0.9.5)
class NOVTABLE metadb_io_v3 : public metadb_io_v2 {
public:
	virtual void register_callback(metadb_io_callback_dynamic * p_callback) = 0;
	virtual void unregister_callback(metadb_io_callback_dynamic * p_callback) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(metadb_io_v3,metadb_io_v2);
};

//! metadb_io_callback_dynamic implementation helper.
class metadb_io_callback_dynamic_impl_base : public metadb_io_callback_dynamic {
public:
	void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) {}
	
	metadb_io_callback_dynamic_impl_base() {static_api_ptr_t<metadb_io_v3>()->register_callback(this);}
	~metadb_io_callback_dynamic_impl_base() {static_api_ptr_t<metadb_io_v3>()->unregister_callback(this);}

	PFC_CLASS_NOT_COPYABLE_EX(metadb_io_callback_dynamic_impl_base)
};
//! Callback service receiving notifications about metadb contents changes.
class NOVTABLE metadb_io_callback : public service_base {
public:
	//! Called when metadb contents change. (Or, one of display hook component requests display update).
	//! @param p_items_sorted List of items that have been updated. The list is always sorted by pointer value, to allow fast bsearch to test whether specific item has changed.
	//! @param p_fromhook Set to true when actual file contents haven't changed but one of metadb_display_field_provider implementations requested an update so output of metadb_handle::format_title() etc has changed.
	virtual void on_changed_sorted(metadb_handle_list_cref p_items_sorted, bool p_fromhook) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_io_callback);
};

//! Entrypoint service for metadb_handle related operations.\n
//! Implemented only by core, do not reimplement.\n
//! Use static_api_ptr_t template to access it, e.g. static_api_ptr_t<metadb>()->handle_create(myhandle,mylocation);
class NOVTABLE metadb : public service_base
{
public:
	//! Locks metadb to prevent other threads from modifying it while you're working with some of its contents. Some functions (metadb_handle::get_info_locked(), metadb_handle::get_info_async_locked()) can be called only from inside metadb lock section.
	virtual void database_lock()=0;
	//! Unlocks metadb after database_lock(). Some functions (metadb_handle::get_info_locked(), metadb_handle::get_info_async_locked()) can be called only from inside metadb lock section.
	virtual void database_unlock()=0;
	
	//! Returns a metadb_handle object referencing the specified location. If one doesn't exist yet a new one is created. There can be only one metadb_handle object referencing specific location. \n
	//! This function should never fail unless there's something critically wrong (can't allocate memory for the new object, etc). \n
	//! Speed: O(log(n)) to total number of metadb_handles present. It's recommended to pass metadb_handles around whenever possible rather than pass playable_locations then retrieve metadb_handles on demand when needed.
	//! @param p_out Receives the metadb_handle pointer.
	//! @param p_location Location to create a metadb_handle for.
	virtual void handle_create(metadb_handle_ptr & p_out,const playable_location & p_location)=0;

	void handle_create_replace_path_canonical(metadb_handle_ptr & p_out,const metadb_handle_ptr & p_source,const char * p_new_path);
	void handle_replace_path_canonical(metadb_handle_ptr & p_out,const char * p_new_path);
	void handle_create_replace_path(metadb_handle_ptr & p_out,const metadb_handle_ptr & p_source,const char * p_new_path);

	//! Helper function; attempts to retrieve a handle to any known playable location to be used for e.g. titleformatting script preview.\n
	//! @returns True on success; false on failure (no known playable locations).
	static bool g_get_random_handle(metadb_handle_ptr & p_out);

	enum {case_sensitive = true};
	typedef pfc::comparator_strcmp path_comparator;

	inline static int path_compare_ex(const char * p1,t_size len1,const char * p2,t_size len2) {return case_sensitive ? pfc::strcmp_ex(p1,len1,p2,len2) : stricmp_utf8_ex(p1,len1,p2,len2);}
	inline static int path_compare(const char * p1,const char * p2) {return case_sensitive ? strcmp(p1,p2) : stricmp_utf8(p1,p2);}
	inline static int path_compare_metadb_handle(const metadb_handle_ptr & p1,const metadb_handle_ptr & p2) {return path_compare(p1->get_path(),p2->get_path());}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb);
};

//! Metadb lock sync helper. For use around metadb_handle "locked" methods.
class in_metadb_sync {
public:
	in_metadb_sync() {
		m_api->database_lock();
	}
	~in_metadb_sync() {
		m_api->database_unlock();
	}
private:
	static_api_ptr_t<metadb> m_api;
};

//! Metadb lock sync helper. For use around metadb_handle "locked" methods.
class in_metadb_sync_fromptr {
public:
	in_metadb_sync_fromptr(const service_ptr_t<metadb> & p_api) : m_api(p_api) {m_api->database_lock();}
	~in_metadb_sync_fromptr() {m_api->database_unlock();}
private:
	service_ptr_t<metadb> m_api;
};

//! Metadb lock sync helper. For use around metadb_handle "locked" methods.
class in_metadb_sync_fromhandle {
public:
	in_metadb_sync_fromhandle(const service_ptr_t<metadb_handle> & p_api) : m_api(p_api) {m_api->metadb_lock();}
	~in_metadb_sync_fromhandle() {m_api->metadb_unlock();}
private:
	service_ptr_t<metadb_handle> m_api;
};

class titleformat_text_out;
class titleformat_hook_function_params;


/*!
	Implementing this service lets you provide your own title-formatting fields that are parsed globally with each call to metadb_handle::format_title methods. \n
	This should be implemented only where absolutely necessary, for safety and performance reasons. Any expensive operations inside the process_field() method may severely damage performance of affected title-formatting calls. \n
	You must NEVER make any other foobar2000 API calls from inside process_field, other than possibly querying information from the passed metadb_handle pointer; you should read your own implementation-specific private data and return as soon as possible. You must not make any assumptions about calling context (threading etc). \n
	It is guaranteed that process_field() is called only inside a metadb lock scope so you can safely call "locked" metadb_handle methods on the metadb_handle pointer you get. You must not lock metadb by yourself inside process_field() - while it is always called from inside a metadb lock scope, it may be called from another thread than the one maintaining the lock because of multi-CPU optimizations active. \n
	If there are multiple metadb_display_field_provider services registered providing fields of the same name, which one gets called is undefined. \n
	IMPORTANT: Any components implementing metadb_display_field_provider MUST call metadb_io::dispatch_refresh() with affected metadb_handles whenever info that they present changes. Otherwise, anything rendering title-formatting strings that reference your data will not update properly, resulting in unreliable/broken output, repaint glitches, etc. \n
	Do not expect a process_field() call each time somebody uses title formatting, calling code might perform its own caching of strings that you return, getting new ones only after metadb_io::dispatch_refresh() with relevant items. \n
	If you can't reliably notify other components about changes of content of fields that you provide (such as when your fields provide some kind of global information and not information specific to item identified by passed metadb_handle), you should not be providing those fields in first place. You must not change returned values of your fields without dispatching appropriate notifications. \n
	Use static service_factory_single_t<myclass> to register your metadb_display_field_provider implementations. Do not call other people's metadb_display_field_provider services directly, they're meant to be called by backend only. \n
	List of fields that you provide is expected to be fixed at run-time. The backend will enumerate your fields only once and refer to them by indexes later. \n
*/

class NOVTABLE metadb_display_field_provider : public service_base {
public:
	//! Returns number of fields provided by this metadb_display_field_provider implementation.
	virtual t_uint32 get_field_count() = 0;
	//! Returns name of specified field provided by this metadb_display_field_provider implementation. Names are not case sensitive. It's strongly recommended that you keep your field names plain English / ASCII only.
	virtual void get_field_name(t_uint32 index, pfc::string_base & out) = 0;
	//! Evaluates the specified field.
	//! @param index Index of field being processed : 0 <= index < get_field_count().
	//! @param handle Handle to item being processed. You can safely call "locked" methods on this handle to retrieve track information and such.
	//! @param out Interface receiving your text output.
	//! @returns Return true to indicate that the field is present so if it's enclosed in square brackets, contents of those brackets should not be skipped, false otherwise.
	virtual bool process_field(t_uint32 index, metadb_handle * handle, titleformat_text_out * out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(metadb_display_field_provider);
};





//! Helper implementation of file_info_filter_impl.
class file_info_filter_impl : public file_info_filter {
public:
	file_info_filter_impl(const pfc::list_base_const_t<metadb_handle_ptr> & p_list,const pfc::list_base_const_t<const file_info*> & p_new_info) {
		pfc::dynamic_assert(p_list.get_count() == p_new_info.get_count());
		pfc::array_t<t_size> order;
		order.set_size(p_list.get_count());
		order_helper::g_fill(order.get_ptr(),order.get_size());
		p_list.sort_get_permutation_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,order.get_ptr());
		m_handles.set_count(order.get_size());
		m_infos.set_size(order.get_size());
		for(t_size n = 0; n < order.get_size(); n++) {
			m_handles[n] = p_list[order[n]];
			m_infos[n] = *p_new_info[order[n]];
		}
	}

	bool apply_filter(metadb_handle_ptr p_location,t_filestats p_stats,file_info & p_info) {
		t_size index;
		if (m_handles.bsearch_t(pfc::compare_t<metadb_handle_ptr,metadb_handle_ptr>,p_location,index)) {
			p_info = m_infos[index];
			return true;
		} else {
			return false;
		}
	}
private:
	metadb_handle_list m_handles;
	pfc::array_t<file_info_impl> m_infos;
};

