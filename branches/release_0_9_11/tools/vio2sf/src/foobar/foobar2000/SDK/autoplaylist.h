/*
	Autoplaylist APIs
	These APIs were introduced in foobar2000 0.9.5, to reduce amount of code required to create your own autoplaylists. Creation of autoplaylists is was also possible before through playlist lock APIs.
	In most cases, you'll want to turn regular playlists into autoplaylists using the following code:
	static_api_ptr_t<autoplaylist_manager>()->add_client_simple(querypattern, sortpattern, playlistindex, forceSort ? autoplaylist_flag_sort : 0);
	If you require more advanced functionality, such as using your own code to filter which part of user's Media Library should be placed in specific autoplaylist, you must implement autoplaylist_client (to let autoplaylist manager invoke your handlers when needed) / autoplaylist_client_factory (to re-instantiate your autoplaylist_client after a foobar2000 restart cycle).
*/

enum {
	//! When set, core will keep the autoplaylist sorted and prevent user from reordering it.
	autoplaylist_flag_sort	= 1 << 0,
};
//! Main class controlling autoplaylist behaviors. Implemented by autoplaylist client in scenarios where simple query/sort strings are not enough (core provides a standard implementation for simple queries).
class NOVTABLE autoplaylist_client : public service_base {
public:
	virtual GUID get_guid() = 0;
	//! Called only inside a metadb lock for performance reasons.
	virtual void filter(metadb_handle_list_cref data, bool * out) = 0;
	//! Return true when you have filled p_orderbuffer with a permutation to apply to p_items, false when you don't support sorting (core's own sort scheme will be applied).
	virtual bool sort(metadb_handle_list_cref p_items,t_size * p_orderbuffer) = 0;
	//! Retrieves your configuration data to be used later when re-instantiating your autoplaylist_client after a restart.
	virtual void get_configuration(stream_writer * p_stream,abort_callback & p_abort) = 0;

	virtual void show_ui(t_size p_source_playlist) = 0;

	//! Helper.
	template<typename t_array> void get_configuration(t_array & p_out) {
		pfc::static_assert<sizeof(p_out[0]) == 1>();
		typedef pfc::array_t<t_uint8,pfc::alloc_fast_aggressive> t_temp; t_temp temp;
		get_configuration(&stream_writer_buffer_append_ref_t<t_temp>(temp),abort_callback_impl());
		p_out = temp;
	}


	FB2K_MAKE_SERVICE_INTERFACE(autoplaylist_client,service_base)
};

typedef service_ptr_t<autoplaylist_client> autoplaylist_client_ptr;

//! Supported from 0.9.5.3 up.
class NOVTABLE autoplaylist_client_v2 : public autoplaylist_client {
public:
	//! Sets a completion_notify object that the autoplaylist_client implementation should call when its filtering behaviors have changed so the whole playlist needs to be rebuilt. \n
	//! completion_notify::on_completion() status parameter meaning: \n
	//! 0.9.5.3 : ignored. \n
	//! 0.9.5.4 and newer: set to 1 to indicate that your configuration has changed as well (for an example as a result of user edits) to get a get_configuration() call as well as cause the playlist to be rebuilt; set to zero otherwise - when the configuration hasn't changed but the playlist needs to be rebuilt as a result of some other event.
	virtual void set_full_refresh_notify(completion_notify::ptr notify) = 0;

	//! Returns whether the show_ui() method is available / does anything useful with out implementation (not everyone implements show_ui).
	virtual bool show_ui_available() = 0;

	//! Returns a human-readable autoplaylist implementer's label to display in playlist's context menu / description / etc.
	virtual void get_display_name(pfc::string_base & out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(autoplaylist_client_v2, autoplaylist_client);
};

//! Class needed to re-instantiate autoplaylist_client after a restart. Not directly needed to set up an autoplaylist_client, but without it, your autoplaylist will be lost after a restart.
class NOVTABLE autoplaylist_client_factory : public service_base {
public:
	//! Must return same GUID as your autoplaylist_client::get_guid()
	virtual GUID get_guid() = 0;
	//! Instantiates your autoplaylist_client with specified configuration.
	virtual autoplaylist_client_ptr instantiate(stream_reader * p_stream,t_size p_sizehint,abort_callback & p_abort) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(autoplaylist_client_factory)
};

PFC_DECLARE_EXCEPTION(exception_autoplaylist,pfc::exception,"Autoplaylist error")

PFC_DECLARE_EXCEPTION(exception_autoplaylist_already_owned,exception_autoplaylist,"This playlist is already an autoplaylist")
PFC_DECLARE_EXCEPTION(exception_autoplaylist_not_owned,exception_autoplaylist,"This playlist is not an autoplaylist")
PFC_DECLARE_EXCEPTION(exception_autoplaylist_lock_failure,exception_autoplaylist,"Playlist could not be locked")


//! Primary class for managing autoplaylists. Implemented by core, do not reimplement; instantiate using static_api_ptr_t<autoplaylist_manager>.
class NOVTABLE autoplaylist_manager : public service_base {
public:
	//! Throws exception_autoplaylist or one of its subclasses on failure.
	//! @param p_flags See autoplaylist_flag_* constants.
	virtual void add_client(autoplaylist_client_ptr p_client,t_size p_playlist,t_uint32 p_flags) = 0;
	virtual bool is_client_present(t_size p_playlist) = 0;
	//! Throws exception_autoplaylist or one of its subclasses on failure (eg. not an autoplaylist).
	virtual autoplaylist_client_ptr query_client(t_size p_playlist) = 0;
	virtual void remove_client(t_size p_playlist) = 0;
	//! Helper; sets up an autoplaylist using standard autoplaylist_client implementation based on simple query/sort strings. When using this, you don't need to maintain own autoplaylist_client/autoplaylist_client_factory implementations, and autoplaylists that you create will not be lost when your DLL is removed, as opposed to using add_client() directly.
	//! Throws exception_autoplaylist or one of its subclasses on failure.
	//! @param p_flags See autoplaylist_flag_* constants.
	virtual void add_client_simple(const char * p_query,const char * p_sort,t_size p_playlist,t_uint32 p_flags) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(autoplaylist_manager)
};

//! \since 0.9.5.4
//! Extended version of autoplaylist_manager, available from 0.9.5.4 up, with methods allowing modification of autoplaylist flags.
class NOVTABLE autoplaylist_manager_v2 : public autoplaylist_manager {
	FB2K_MAKE_SERVICE_INTERFACE(autoplaylist_manager_v2, autoplaylist_manager)
public:
	virtual t_uint32 get_client_flags(t_size playlist) = 0;
	virtual void set_client_flags(t_size playlist, t_uint32 newFlags) = 0;

	//! For use with autoplaylist client configuration dialogs. It's recommended not to call this from anything else.
	virtual t_uint32 get_client_flags(autoplaylist_client::ptr client) = 0;
	//! For use with autoplaylist client configuration dialogs. It's recommended not to call this from anything else.
	virtual void set_client_flags(autoplaylist_client::ptr client, t_uint32 newFlags) = 0;
};
