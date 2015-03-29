class titleformat_hook;
class titleformat_text_filter;

//! A metadb_handle object represents interface to reference-counted file_info cache entry for the specified location.\n
//! To obtain a metadb_handle to specific location, use metadb::handle_create(). To obtain a list of metadb_handle objects corresponding to specific path (directory, playlist, multitrack file, etc), use relevant playlist_incoming_item_filter methods (recommended), or call playlist_loader methods directly.\n
//! A metadb_handle is also the most efficient way of passing playable object locations around because it provides fast access to both location and infos, and is reference counted so duplicating it is as fast as possible.\n
//! To retrieve a path of a file from a metadb_handle, use metadb_handle::get_path() function. Note that metadb_handle is NOT just file path, some formats support multiple subsongs per physical file, which are signaled using subsong indexes.

class NOVTABLE metadb_handle : public service_base
{
public:
	//! Retrieves location represented by this metadb_handle object. Returned reference is valid until calling context releases metadb_handle that returned it (metadb_handle_ptr is deallocated etc).
	virtual const playable_location & get_location() const = 0;//never fails, returned pointer valid till the object is released


	//! Renders information about item referenced by this metadb_handle object.
	//! @param p_hook Optional callback object overriding fields and functions; set to NULL if not used.
	//! @param p_out String receiving the output on success.
	//! @param p_script Titleformat script to use. Use titleformat_compiler service to create one.
	//! @param p_filter Optional callback object allowing input to be filtered according to context (i.e. removal of linebreak characters present in tags when rendering playlist lines). Set to NULL when not used.
	//! @returns true on success, false when dummy file_info instance was used because actual info is was not (yet) known.
	virtual bool format_title(titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<class titleformat_object> & p_script,titleformat_text_filter * p_filter) = 0;

	//! Locks metadb to prevent other threads from modifying it while you're working with some of its contents. Some functions (metadb_handle::get_info_locked(), metadb_handle::get_info_async_locked()) can be called only from inside metadb lock section.
	//! Same as metadb::database_lock().
	virtual void metadb_lock() = 0;
	//! Unlocks metadb after metadb_lock(). Some functions (metadb_handle::get_info_locked(), metadb_handle::get_info_async_locked()) can be called only from inside metadb lock section.
	//! Same as metadb::database_unlock().
	virtual void metadb_unlock() = 0;

	//! Returns last seen file stats, filestats_invalid if unknown.
	virtual t_filestats get_filestats() const = 0;

	//! Queries whether cached info about item referenced by this metadb_handle object is already available.\n
	//! Note that state of cached info changes only inside main thread, so you can safely assume that it doesn't change while some block of your code inside main thread is being executed.
	virtual bool is_info_loaded() const = 0;
	//! Queries cached info about item referenced by this metadb_handle object. Returns true on success, false when info is not yet known.\n
	//! Note that state of cached info changes only inside main thread, so you can safely assume that it doesn't change while some block of your code inside main thread is being executed.
	virtual bool get_info(file_info & p_info) const = 0;
	//! Queries cached info about item referenced by this metadb_handle object. Returns true on success, false when info is not yet known. This is more efficient than get_info() since no data is copied.\n
	//! You must lock the metadb before calling this function, and unlock it after you are done working with the returned pointer, to ensure multithread safety.\n
	//! Note that state of cached info changes only inside main thread, so you can safely assume that it doesn't change while some block of your code inside main thread is being executed.
	//! @param p_info On success, receives a pointer to metadb's file_info object. The pointer is for temporary use only, and becomes invalid when metadb is unlocked.
	virtual bool get_info_locked(const file_info * & p_info) const = 0;
	
	//! Queries whether cached info about item referenced by this metadb_handle object is already available.\n
	//! This is intended for use in special cases when you need to immediately retrieve info sent by metadb_io hint from another thread; state of returned data can be altered by any thread, as opposed to non-async methods.
	virtual bool is_info_loaded_async() const = 0;	
	//! Queries cached info about item referenced by this metadb_handle object. Returns true on success, false when info is not yet known.\n
	//! This is intended for use in special cases when you need to immediately retrieve info sent by metadb_io hint from another thread; state of returned data can be altered by any thread, as opposed to non-async methods.
	virtual bool get_info_async(file_info & p_info) const = 0;	
	//! Queries cached info about item referenced by this metadb_handle object. Returns true on success, false when info is not yet known. This is more efficient than get_info() since no data is copied.\n
	//! You must lock the metadb before calling this function, and unlock it after you are done working with the returned pointer, to ensure multithread safety.\n
	//! This is intended for use in special cases when you need to immediately retrieve info sent by metadb_io hint from another thread; state of returned data can be altered by any thread, as opposed to non-async methods.
	//! @param p_info On success, receives a pointer to metadb's file_info object. The pointer is for temporary use only, and becomes invalid when metadb is unlocked.
	virtual bool get_info_async_locked(const file_info * & p_info) const = 0;


	//! Renders information about item referenced by this metadb_handle object, using external file_info data.
	virtual void format_title_from_external_info(const file_info & p_info,titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<class titleformat_object> & p_script,titleformat_text_filter * p_filter) = 0;


	//! New in 0.9.5.
	virtual bool format_title_nonlocking(titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<class titleformat_object> & p_script,titleformat_text_filter * p_filter) = 0;

	//! New in 0.9.5.
	virtual void format_title_from_external_info_nonlocking(const file_info & p_info,titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<class titleformat_object> & p_script,titleformat_text_filter * p_filter) = 0;
	
	static bool g_should_reload(const t_filestats & p_old_stats,const t_filestats & p_new_stats,bool p_fresh);
	bool should_reload(const t_filestats & p_new_stats,bool p_fresh) const;
	

	//! Helper provided for backwards compatibility; takes formatting script as text string and calls relevant titleformat_compiler methods; returns false when the script could not be compiled.\n
	//! See format_title() for descriptions of parameters.\n
	//! Bottleneck warning: you should consider using precompiled titleformat script object and calling regular format_title() instead when processing large numbers of items.
	bool format_title_legacy(titleformat_hook * p_hook,pfc::string_base & out,const char * p_spec,titleformat_text_filter * p_filter);

	//! Retrieves path of item described by this metadb_handle instance. Returned string is valid until calling context releases metadb_handle that returned it (metadb_handle_ptr is deallocated etc).
	inline const char * get_path() const {return get_location().get_path();}
	//! Retrieves subsong index of item described by this metadb_handle instance (used for multiple playable tracks within single physical file).
	inline t_uint32 get_subsong_index() const {return get_location().get_subsong_index();}
	
	double get_length();//helper
	
	t_filetimestamp get_filetimestamp();
	t_filesize get_filesize();

	FB2K_MAKE_SERVICE_INTERFACE(metadb_handle,service_base);
};

typedef service_ptr_t<metadb_handle> metadb_handle_ptr;

typedef pfc::list_base_t<metadb_handle_ptr> & metadb_handle_list_ref;
typedef pfc::list_base_const_t<metadb_handle_ptr> const & metadb_handle_list_cref;

namespace metadb_handle_list_helper {
	void sort_by_format(metadb_handle_list_ref p_list,const char * spec,titleformat_hook * p_hook);
	void sort_by_format_get_order(metadb_handle_list_cref p_list,t_size* order,const char * spec,titleformat_hook * p_hook);
	void sort_by_format(metadb_handle_list_ref p_list,const service_ptr_t<titleformat_object> & p_script,titleformat_hook * p_hook, int direction = 1);
	void sort_by_format_get_order(metadb_handle_list_cref p_list,t_size* order,const service_ptr_t<titleformat_object> & p_script,titleformat_hook * p_hook,int p_direction = 1);

	void sort_by_relative_path(metadb_handle_list_ref p_list);
	void sort_by_relative_path_get_order(metadb_handle_list_cref p_list,t_size* order);
	
	void remove_duplicates(pfc::list_base_t<metadb_handle_ptr> & p_list);
	void sort_by_pointer_remove_duplicates(pfc::list_base_t<metadb_handle_ptr> & p_list);
	void sort_by_path_quick(pfc::list_base_t<metadb_handle_ptr> & p_list);

	void sort_by_pointer(pfc::list_base_t<metadb_handle_ptr> & p_list);
	t_size bsearch_by_pointer(const pfc::list_base_const_t<metadb_handle_ptr> & p_list,const metadb_handle_ptr & val);

	double calc_total_duration(const pfc::list_base_const_t<metadb_handle_ptr> & p_list);

	void sort_by_path(pfc::list_base_t<metadb_handle_ptr> & p_list);

	t_filesize calc_total_size(metadb_handle_list_cref list, bool skipUnknown = false);
	t_filesize calc_total_size_ex(metadb_handle_list_cref list, bool & foundUnknown);
};

template<template<typename> class t_alloc = pfc::alloc_fast >
class metadb_handle_list_t : public service_list_t<metadb_handle,t_alloc> {
private:
	typedef metadb_handle_list_t<t_alloc> t_self;
	typedef list_base_const_t<metadb_handle_ptr> t_interface;
public:
	inline void sort_by_format(const char * spec,titleformat_hook * p_hook) {
		return metadb_handle_list_helper::sort_by_format(*this, spec, p_hook);
	}
	inline void sort_by_format_get_order(t_size* order,const char * spec,titleformat_hook * p_hook) const {
		metadb_handle_list_helper::sort_by_format_get_order(*this, order, spec, p_hook);
	}

	inline void sort_by_format(const service_ptr_t<titleformat_object> & p_script,titleformat_hook * p_hook, int direction = 1) {
		metadb_handle_list_helper::sort_by_format(*this, p_script, p_hook, direction);
	}
	inline void sort_by_format_get_order(t_size* order,const service_ptr_t<titleformat_object> & p_script,titleformat_hook * p_hook) const {
		metadb_handle_list_helper::sort_by_format_get_order(*this, order, p_script, p_hook);
	}
	
	inline void sort_by_relative_path() {
		metadb_handle_list_helper::sort_by_relative_path(*this);
	}
	inline void sort_by_relative_path_get_order(t_size* order) const {
		metadb_handle_list_helper::sort_by_relative_path_get_order(*this,order);
	}
	
	inline void remove_duplicates() {metadb_handle_list_helper::remove_duplicates(*this);}
	inline void sort_by_pointer_remove_duplicates() {metadb_handle_list_helper::sort_by_pointer_remove_duplicates(*this);}
	inline void sort_by_path_quick() {metadb_handle_list_helper::sort_by_path_quick(*this);}

	inline void sort_by_pointer() {metadb_handle_list_helper::sort_by_pointer(*this);}
	inline t_size bsearch_by_pointer(const metadb_handle_ptr & val) const {return metadb_handle_list_helper::bsearch_by_pointer(*this,val);}

	inline double calc_total_duration() const {return metadb_handle_list_helper::calc_total_duration(*this);}

	inline void sort_by_path() {metadb_handle_list_helper::sort_by_path(*this);}

	const t_self & operator=(const t_self & p_source) {remove_all(); add_items(p_source);return *this;}
	const t_self & operator=(const t_interface & p_source) {remove_all(); add_items(p_source);return *this;}
	metadb_handle_list_t(const t_self & p_source) {add_items(p_source);}
	metadb_handle_list_t(const t_interface & p_source) {add_items(p_source);}
	metadb_handle_list_t() {}

	t_self & operator+=(const t_interface & source) {add_items(source); return *this;}
	t_self & operator+=(const metadb_handle_ptr & source) {add_item(source); return *this;}
};

typedef metadb_handle_list_t<> metadb_handle_list;

namespace metadb_handle_list_helper {
	void sorted_by_pointer_extract_difference(metadb_handle_list const & p_list_1,metadb_handle_list const & p_list_2,metadb_handle_list & p_list_1_specific,metadb_handle_list & p_list_2_specific);
};

class metadb_handle_lock
{
	metadb_handle_ptr m_ptr;
public:
	inline metadb_handle_lock(const metadb_handle_ptr & param)
	{
		m_ptr = param;
		m_ptr->metadb_lock();
	}	
	inline ~metadb_handle_lock() {m_ptr->metadb_unlock();}
};

inline pfc::string_base & operator<<(pfc::string_base & p_fmt,const metadb_handle_ptr & p_location) {
	if (p_location.is_valid()) 
		return p_fmt << p_location->get_location();
	else
		return p_fmt << "[invalid location]";
}


class string_format_title {
public:
	string_format_title(metadb_handle_ptr p_item,const char * p_script) {
		p_item->format_title_legacy(NULL,m_data,p_script,NULL);
	}
	string_format_title(metadb_handle_ptr p_item,service_ptr_t<class titleformat_object> p_script) {
		p_item->format_title(NULL,m_data,p_script,NULL);
	}

	const char * get_ptr() const {return m_data.get_ptr();}
	operator const char * () const {return m_data.get_ptr();}
private:
	pfc::string8_fastalloc m_data;
};
