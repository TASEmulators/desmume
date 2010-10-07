#ifndef _WINDOWS
#error PORTME
#endif

//! Entrypoint service for user interface modules. Implement when registering an UI module. Do not call existing implementations; only core enumerates / dispatches calls. To control UI behaviors from other components, use ui_control API. \n
//! Use user_interface_factory_t<> to register, e.g static user_interface_factory_t<myclass> g_myclass_factory;
class NOVTABLE user_interface : public service_base {
public:
	//!HookProc usage: \n
	//! in your windowproc, call HookProc first, and if it returns true, return LRESULT value it passed to you
	typedef BOOL (WINAPI * HookProc_t)(HWND wnd,UINT msg,WPARAM wp,LPARAM lp,LRESULT * ret);

	//! Retrieves name (UTF-8 null-terminated string) of the UI module.
	virtual const char * get_name()=0;
	//! Initializes the UI module - creates the main app window, etc. Failure should be signaled by appropriate exception (std::exception or a derivative).
	virtual HWND init(HookProc_t hook)=0;
	//! Deinitializes the UI module - destroys the main app window, etc.
	virtual void shutdown()=0;
	//! Activates main app window.
	virtual void activate()=0;
	//! Minimizes/hides main app window.
	virtual void hide()=0;
	//! Returns whether main window is visible / not minimized. Used for activate/hide command.
	virtual bool is_visible() = 0;
	//! Retrieves GUID of your implementation, to be stored in configuration file etc.
	virtual GUID get_guid() = 0;

	//! Overrides statusbar text with specified string. The parameter is a null terminated UTF-8 string. The override is valid until another override_statusbar_text() call or revert_statusbar_text() call.
	virtual void override_statusbar_text(const char * p_text) = 0;
	//! Disables statusbar text override.
	virtual void revert_statusbar_text() = 0;

	//! Shows now-playing item somehow (e.g. system notification area popup).
	virtual void show_now_playing() = 0;

	static bool g_find(service_ptr_t<user_interface> & p_out,const GUID & p_guid);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(user_interface);
};

template<typename T>
class user_interface_factory : public service_factory_single_t<T> {};

//! Interface class allowing you to override UI statusbar text. There may be multiple callers trying to override statusbar text; backend decides which one succeeds so you will not always get what you want. Statusbar text override is automatically cancelled when the object is released.\n
//! Use ui_control::override_status_text_create() to instantiate.
//! Implemented by core. Do not reimplement.
class NOVTABLE ui_status_text_override : public service_base
{
public:
	//! Sets statusbar text to specified UTF-8 null-terminated string.
	virtual void override_text(const char * p_message) = 0;
	//! Cancels statusbar text override.
	virtual void revert_text() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(ui_status_text_override,service_base);
};

//! Serivce providing various UI-related commands. Implemented by core; do not reimplement.
//! Instantiation: use static_api_ptr_t<ui_control>.
class NOVTABLE ui_control : public service_base {
public:
	//! Returns whether primary UI is visible/unminimized.
	virtual bool is_visible()=0;
	//! Activates/unminimizes main UI.
	virtual void activate()=0;
	//! Hides/minimizese main UI.
	virtual void hide()=0;
	//! Retrieves main GUI icon, to use as window icon etc. Returned handle does not need to be freed.
	virtual HICON get_main_icon()=0;
	//! Loads main GUI icon, version with specified width/height. Returned handle needs to be freed with DestroyIcon when you are done using it.
	virtual HICON load_main_icon(unsigned width,unsigned height) = 0;

	//! Activates preferences dialog and navigates to specified page. See also: preference_page API.
	virtual void show_preferences(const GUID & p_page) = 0;

	//! Instantiates ui_status_text_override service, that can be used to display status messages.
	//! @param p_out receives new ui_status_text_override instance.
	//! @returns true on success, false on failure (out of memory / no GUI loaded / etc)
	virtual bool override_status_text_create(service_ptr_t<ui_status_text_override> & p_out) = 0;

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(ui_control);
};

//! Service called from the UI when some object is dropped into the UI. Usable for modifying drag&drop behaviors such as adding custom handlers for object types other than supported media files.\n
//! Implement where needed; use ui_drop_item_callback_factory_t<> template to register, e.g. static ui_drop_item_callback_factory_t<myclass> g_myclass_factory.
class NOVTABLE ui_drop_item_callback : public service_base {
public:
	//! Called when an object was dropped; returns true if the object was processed and false if not.
	virtual bool on_drop(interface IDataObject * pDataObject) = 0;
	//! Tests whether specified object type is supported by this ui_drop_item_callback implementation. Returns true and sets p_effect when it's supported; returns false otherwise. \n
	//! See IDropTarget::DragEnter() documentation for more information about p_effect values.
	virtual bool is_accepted_type(interface IDataObject * pDataObject, DWORD * p_effect)=0;

	//! Static helper, calls all existing implementations appropriately. See on_drop().
	static bool g_on_drop(interface IDataObject * pDataObject);
	//! Static helper, calls all existing implementations appropriately. See is_accepted_type().
	static bool g_is_accepted_type(interface IDataObject * pDataObject, DWORD * p_effect);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(ui_drop_item_callback);
};

template<class T>
class ui_drop_item_callback_factory_t : public service_factory_single_t<T> {};


class ui_selection_callback;

//! Write interface and reference counter for the shared selection.
//! The ui_selection_manager stores the selected items as a list.
//! The ui_selection_holder service allows components to modify this list.
//! It also serves as a reference count: the ui_selection_manager clears the stored
//! selection when no component holds a reference to a ui_selection_holder.
//! 
//! When a window that uses the shared selection gets the focus, it should acquire
//! a ui_selection_holder from the ui_selection_manager. If it contains selectable items,
//! it should use the appropriate method to store its selected items as the shared selection.
//! If it just wants to preserve the selection - for example so it can display it - it should
//! merely store the acquired ui_selection_holder.
//!
//! When the window loses the focus, it should release its ui_selection_holder.
//! It should not use a set method to clear the selection
class NOVTABLE ui_selection_holder : public service_base {
public:
	//! Sets selected items.
	virtual void set_selection(metadb_handle_list_cref data) = 0;
	//! Sets selected items to playlist selection and enables tracking.
	//! When the playlist selection changes, the stored selection is automatically updated.
	//! Tracking ends when a set method is called on any ui_selection_holder or when
	//! the last reference to this ui_selection_holder is released.
	virtual void set_playlist_selection_tracking() = 0;
	//! Sets selected items to the contents of the active playlist and enables tracking.
	//! When the active playlist or its contents changes, the stored selection is automatically updated.
	//! Tracking ends when a set method is called on any ui_selection_holder or when
	//! the last reference to this ui_selection_holder is released.
	virtual void set_playlist_tracking() = 0;

	//! Sets selected items and type of selection holder.
	//! @param type Specifies type of selection. Values same as contextmenu_item caller IDs.
	virtual void set_selection_ex(metadb_handle_list_cref data, const GUID & type) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(ui_selection_holder,service_base);
};

class NOVTABLE ui_selection_manager : public service_base {
public:
	//! Retrieves current selection.
	virtual void get_selection(pfc::list_base_t<metadb_handle_ptr> & p_selection) = 0;
	//! Registers a callback. It is recommended to use ui_selection_callback_impl_base class instead of calling this directly.
	virtual void register_callback(ui_selection_callback * p_callback) = 0;
	//! Unregisters a callback. It is recommended to use ui_selection_callback_impl_base class instead of calling this directly.
	virtual void unregister_callback(ui_selection_callback * p_callback) = 0;

	virtual ui_selection_holder::ptr acquire() = 0;

	//! Retrieves type of the active selection holder. Values same as contextmenu_item caller IDs.
	virtual GUID get_selection_type() = 0;
	
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(ui_selection_manager);
};

class ui_selection_callback {
public:
	virtual void on_selection_changed(const pfc::list_base_const_t<metadb_handle_ptr> & p_selection) = 0;
protected:
	ui_selection_callback() {}
	~ui_selection_callback() {}
};

//! ui_selection_callback implementation helper with autoregistration - do not instantiate statically
class ui_selection_callback_impl_base : public ui_selection_callback {
protected:
	ui_selection_callback_impl_base(bool activate = true) : m_active() {ui_selection_callback_activate(activate);}
	~ui_selection_callback_impl_base() {ui_selection_callback_activate(false);}

	void ui_selection_callback_activate(bool state = true) {
		if (state != m_active) {
			m_active = state;
			static_api_ptr_t<ui_selection_manager> api;
			if (state) api->register_callback(this);
			else api->unregister_callback(this);
		}
	}

	//avoid pure virtual function calls in rare cases - provide a dummy implementation
	void on_selection_changed(const pfc::list_base_const_t<metadb_handle_ptr> & p_selection) {}

	PFC_CLASS_NOT_COPYABLE_EX(ui_selection_callback_impl_base);
private:
	bool m_active;
};
