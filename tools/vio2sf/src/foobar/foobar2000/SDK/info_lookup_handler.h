//! Service used to access various external (online) track info lookup services, such as freedb, to update file tags with info retrieved from those services.
class NOVTABLE info_lookup_handler : public service_base {
public:
	enum {
		flag_album_lookup = 1 << 0,
		flag_track_lookup = 1 << 1,
	};

	//! Retrieves human-readable name of the lookup handler to display in user interface.
	virtual void get_name(pfc::string_base & p_out) = 0;

	//! Returns one or more of flag_track_lookup, and flag_album_lookup.
	virtual t_uint32 get_flags() = 0; 

	virtual HICON get_icon(int p_width, int p_height) = 0;

	//! Performs a lookup. Creates a modeless dialog and returns immediately.
	//! @param p_items Items to look up.
	//! @param p_notify Callback to notify caller when the operation has completed. Call on_completion with status code 0 to signal failure/abort, or with code 1 to signal success / new infos in metadb.
	//! @param p_parent Parent window for the lookup dialog. Caller will typically disable the window while lookup is in progress and enable it back when completion is signaled.
	virtual void lookup(const pfc::list_base_const_t<metadb_handle_ptr> & p_items,completion_notify_ptr p_notify,HWND p_parent) = 0;
 
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(info_lookup_handler);
};
