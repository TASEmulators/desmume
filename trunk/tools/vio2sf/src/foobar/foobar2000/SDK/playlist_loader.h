//! Callback interface receiving item locations from playlist loader. Also inherits abort_callback methods and can be passed to functions that require an abort_callback. \n
//! See playlist_loader_callback_impl for a basic implementation of playlist_loader_callback. Typically, you call one of standard services such as playlist_incoming_item_filter instead of implementing this interface and calling playlist_loader methods directly.
class NOVTABLE playlist_loader_callback : public abort_callback {
public:
	//! Enumeration type representing origin of item passed to playlist_loader_callback.
	enum t_entry_type {
		//! User-requested (such as directly dropped to window or picked in openfiledialog).
		entry_user_requested,
		//! From directory content enumeration.
		entry_directory_enumerated,
		//! Referenced by playlist file.
		entry_from_playlist,
	};
	//! Indicates specified path being processed; provided for updating GUI. Note that optimally GUI should not be updated every time this is called because that could introduce a bottleneck.
	virtual void on_progress(const char * p_path) = 0;
	
	//! Receives an item from one of playlist_loader functions.
	//! @param p_item Item location, in form of metadb_handle pointer.
	//! @param p_type Origin of this item - see t_entry_type for more info.
	//! @param p_stats File stats of this item; set to filestats_invalid if not available.
	//! @param p_fresh Fresh flag; indicates whether stats are directly from filesystem (true) or as stored earlier in a playlist file (false).
	virtual void on_entry(const metadb_handle_ptr & p_item,t_entry_type p_type,const t_filestats & p_stats,bool p_fresh) = 0;
	//! Queries whether file_info for specified item is requested. In typical scenario, if want_info() returns false, on_entry() will be called with same parameters; otherwise caller will attempt to read info from the item and call on_entry_info() with same parameters and file_info read from the item.
	//! @param p_item Item location, in form of metadb_handle pointer.
	//! @param p_type Origin of this item - see t_entry_type for more info.
	//! @param p_stats File stats of this item; set to filestats_invalid if not available.
	//! @param p_fresh Fresh flag; indicates whether stats are directly from filesystem (true) or as stored earlier in a playlist file (false).
	virtual bool want_info(const metadb_handle_ptr & p_item,t_entry_type p_type,const t_filestats & p_stats,bool p_fresh) = 0;
	//! Receives an item from one of playlist_loader functions; including file_info data. Except for file_info to be typically used as hint for metadb backend, behavior of this method is same as on_entry().
	//! @param p_item Item location, in form of metadb_handle pointer.
	//! @param p_type Origin of this item - see t_entry_type for more info.
	//! @param p_stats File stats of this item; set to filestats_invalid if not available.
	//! @param p_info Information about the item, read from the file directly (if p_fresh is set to true) or from e.g. playlist file (if p_fresh is set to false).
	//! @param p_fresh Fresh flag; indicates whether stats are directly from filesystem (true) or as stored earlier in a playlist file (false).
	virtual void on_entry_info(const metadb_handle_ptr & p_item,t_entry_type p_type,const t_filestats & p_stats,const file_info & p_info,bool p_fresh) = 0;

	//! Same as metadb::handle_create(); provided here to avoid repeated metadb instantiation bottleneck since calling code will need this function often.
	virtual void handle_create(metadb_handle_ptr & p_out,const playable_location & p_location) = 0;

protected:
	playlist_loader_callback() {}
	~playlist_loader_callback() {}
};

class NOVTABLE playlist_loader_callback_v2 : public playlist_loader_callback {
public:
	//! Returns whether further on_entry() calls for this file are wanted. Typically always returns true, can be used to optimize cases when directories are searched for files matching specific pattern only so unwanted files aren't parsed unnecessarily.
	//! @param path Canonical path to the media file being processed.
	virtual bool is_path_wanted(const char * path, t_entry_type type) = 0;

protected:
	playlist_loader_callback_v2() {}
	~playlist_loader_callback_v2() {}
};

//! Basic implementation of playlist_loader_callback.
class playlist_loader_callback_impl : public playlist_loader_callback_v2 {
public:

	playlist_loader_callback_impl(abort_callback & p_abort) : m_abort(p_abort) {}

	bool is_aborting() const {return m_abort.is_aborting();}
	abort_callback_event get_abort_event() const {return m_abort.get_abort_event();}

	const metadb_handle_ptr & get_item(t_size idx) const {return m_data[idx];}
	const metadb_handle_ptr & operator[](t_size idx) const {return m_data[idx];}
	t_size get_count() const {return m_data.get_count();}
	
	const metadb_handle_list & get_data() const {return m_data;}

	void on_progress(const char * path) {}

	void on_entry(const metadb_handle_ptr & ptr,t_entry_type type,const t_filestats & p_stats,bool p_fresh) {m_data.add_item(ptr);}
	bool want_info(const metadb_handle_ptr & ptr,t_entry_type type,const t_filestats & p_stats,bool p_fresh) {return false;}
	void on_entry_info(const metadb_handle_ptr & ptr,t_entry_type type,const t_filestats & p_stats,const file_info & p_info,bool p_fresh) {m_data.add_item(ptr);}

	void handle_create(metadb_handle_ptr & p_out,const playable_location & p_location) {m_api->handle_create(p_out,p_location);}

	bool is_path_wanted(const char * path,t_entry_type type) {return true;}

private:
	metadb_handle_list m_data;
	abort_callback & m_abort;
	static_api_ptr_t<metadb> m_api;
};

//! Service handling playlist file operations. There are multiple implementations handling different playlist formats; you can add new implementations to allow new custom playlist file formats to be read or written.\n
//! Also provides static helper functions for turning a filesystem path into a list of playable item locations. \n
//! Note that you should typically call playlist_incoming_item_filter methods instead of calling playlist_loader methods directly to get a list of playable items from a specified path; this way you get a core-implemented threading and abortable dialog displaying progress.\n
//! To register your own implementation, use playlist_loader_factory_t template.\n
//! To call existing implementations, use static helper methods of playlist_loader class.
class NOVTABLE playlist_loader : public service_base {
public:
	//! Parses specified playlist file into list of playable locations. Throws exception_io or derivatives on failure, exception_aborted on abort. If specified file is not a recognized playlist file, exception_io_unsupported_format is thrown.
	//! @param p_path Path of playlist file to parse. Used for relative path handling purposes (p_file parameter is used for actual file access).
	//! @param p_file File interface to use for reading. Read/write pointer must be set to beginning by caller before calling.
	//! @param p_callback Callback object receiving enumerated playable item locations as well as signaling user aborting the operation.
	virtual void open(const char * p_path, const service_ptr_t<file> & p_file,playlist_loader_callback & p_callback) = 0;
	//! Writes a playlist file containing specific item list to specified file. Will fail (pfc::exception_not_implemented) if specified playlist_loader is read-only (can_write() returns false).
	//! @param p_path Path of playlist file to write. Used for relative path handling purposes (p_file parameter is used for actual file access).
	//! @param p_file File interface to use for writing. Caller should ensure that the file is empty (0 bytes long) before calling.
	//! @param p_data List of items to save to playlist file.
	//! @param p_abort abort_callback object signaling user aborting the operation. Note that aborting a save playlist operation will most likely leave user with corrupted/incomplete file.
	virtual void write(const char * p_path, const service_ptr_t<file> & p_file,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,abort_callback & p_abort) = 0;
	//! Returns extension of file format handled by this playlist_loader implementation (a UTF-8 encoded null-terminated string).
	virtual const char * get_extension() = 0;
	//! Returns whether this playlist_loader implementation supports writing. If can_write() returns false, all write() calls will fail.
	virtual bool can_write() = 0;
	//! Returns whether specified content type is one of playlist types supported by this playlist_loader implementation or not.
	//! @param p_content_type Content type to query, a UTF-8 encoded null-terminated string.
	virtual bool is_our_content_type(const char* p_content_type) = 0;
	//! Returns whether playlist format extension supported by this implementation should be listed on file types associations page.
	virtual bool is_associatable() = 0;

	//! Attempts to load a playlist file from specified filesystem path. Throws exception_io or derivatives on failure, exception_aborted on abort. If specified file is not a recognized playlist file, exception_io_unsupported_format is thrown. \n
	//! Equivalent to g_load_playlist_filehint(NULL,p_path,p_callback).
	//! @param p_path Filesystem path to load playlist from, a UTF-8 encoded null-terminated string.
	//! @param p_callback Callback object receiving enumerated playable item locations as well as signaling user aborting the operation.
	static void g_load_playlist(const char * p_path,playlist_loader_callback & p_callback);

	//! Attempts to load a playlist file from specified filesystem path. Throws exception_io or derivatives on failure, exception_aborted on abort. If specified file is not a recognized playlist file, exception_io_unsupported_format is thrown.
	//! @param p_path Filesystem path to load playlist from, a UTF-8 encoded null-terminated string.
	//! @param p_callback Callback object receiving enumerated playable item locations as well as signaling user aborting the operation.
	//! @param fileHint File object to read from, can be NULL if not available.
	static void g_load_playlist_filehint(file::ptr fileHint,const char * p_path,playlist_loader_callback & p_callback);

	//! Saves specified list of locations into a playlist file. Throws exception_io or derivatives on failure, exception_aborted on abort.
	//! @param p_path Filesystem path to save playlist to, a UTF-8 encoded null-terminated string.
	//! @param p_data List of items to save to playlist file.
	//! @param p_abort abort_callback object signaling user aborting the operation. Note that aborting a save playlist operation will most likely leave user with corrupted/incomplete file.
	static void g_save_playlist(const char * p_path,const pfc::list_base_const_t<metadb_handle_ptr> & p_data,abort_callback & p_abort);

	//! Processes specified path to generate list of playable items. Includes recursive directory/archive enumeration. \n
	//! Does not touch playlist files encountered - use g_process_path_ex() if specified path is possibly a playlist file; playlist files found inside directories or archives are ignored regardless.\n
	//! Warning: caller must handle exceptions which will occur in case of I/O failure.
	//! @param p_path Filesystem path to process; a UTF-8 encoded null-terminated string.
	//! @param p_callback Callback object receiving enumerated playable item locations as well as signaling user aborting the operation.
	//! @param p_type Origin of p_path string. Reserved for internal use in recursive calls, should be left at default value; it controls various internal behaviors.
	static void g_process_path(const char * p_path,playlist_loader_callback_v2 & p_callback,playlist_loader_callback::t_entry_type p_type = playlist_loader_callback::entry_user_requested);
	
	//! Calls attempts to process specified path as a playlist; if that fails (i.e. not a playlist), calls g_process_path with same parameters. See g_process_path for parameter descriptions. \n
	//! Warning: caller must handle exceptions which will occur in case of I/O failure or playlist parsing failure.
	//! @returns True if specified path was processed as a playlist file, false otherwise (relevant in some scenarios where output is sorted after loading, playlist file contents should not be sorted).
	static bool g_process_path_ex(const char * p_path,playlist_loader_callback_v2 & p_callback,playlist_loader_callback::t_entry_type p_type = playlist_loader_callback::entry_user_requested);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playlist_loader);
};

template<typename t_myclass>
class playlist_loader_factory_t : public service_factory_single_t<t_myclass> {};

