//! Helper class for running audio data through a DSP chain.
class dsp_manager {
public:
	dsp_manager() : m_config_changed(false) {}

	//! Alters the DSP chain configuration. Should be called before the first run() to set the configuration but can be also called anytime later between run() calls.
	void set_config( const dsp_chain_config & p_data );
	//! Runs DSP on the specified chunk list.
	//! @returns Current DSP latency in seconds.
	double run(dsp_chunk_list * p_list,const metadb_handle_ptr & p_cur_file,unsigned p_flags,abort_callback & p_abort);
	//! Flushes the DSP (e.g. when seeking).
	void flush();

	//! Equivalent to set_config() with empty configuration.
	void close();

	//! Returns whether there's at least one active DSP in the configuration.
	bool is_active() const;

private:
	struct t_dsp_chain_entry {
		service_ptr_t<dsp> m_dsp;
		dsp_preset_impl m_preset;
		bool m_recycle_flag;
	};
	typedef pfc::chain_list_v2_t<t_dsp_chain_entry> t_dsp_chain;

	t_dsp_chain m_chain;
	dsp_chain_config_impl m_config;
	bool m_config_changed;
	
	void dsp_run(t_dsp_chain::const_iterator p_iter,dsp_chunk_list * list,const metadb_handle_ptr & cur_file,unsigned flags,double & latency,abort_callback&);

	dsp_manager(const dsp_manager &) {throw pfc::exception_not_implemented();}
	const dsp_manager & operator=(const dsp_manager&) {throw pfc::exception_not_implemented();}
};

//! Core API for accessing core playback DSP settings as well as spawning DSP configuration dialogs. \n
//! Use static_api_ptr_t<dsp_config_manager>() to instantiate.
class dsp_config_manager : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(dsp_config_manager);
public:
	//! Retrieves current core playback DSP settings.
	virtual void get_core_settings(dsp_chain_config & p_out) = 0;
	//! Changes current core playback DSP settings.
	virtual void set_core_settings(const dsp_chain_config & p_data) = 0;
	
	//! Runs a modal DSP settings dialog.
	//! @param p_data DSP chain configuration to edit - contains initial configuration to put in the dialog when called, receives the new configuration on successful edit.
	//! @returns True when user approved DSP configuration changes (pressed the "OK" button), false when the user cancelled them ("Cancel" button).
	virtual bool configure_popup(dsp_chain_config & p_data,HWND p_parent,const char * p_title) = 0;

	//! Spawns an embedded DSP settings dialog. 
	//! @param p_initdata Initial DSP chain configuration to put in the dialog.
	//! @param p_parent Parent window to contain the embedded dialog.
	//! @param p_id Control ID of the embedded dialog. The parent window will receive a WM_COMMAND with BN_CLICKED and this identifier when user changes settings in the embedded dialog.
	//! @param p_from_modal Must be set to true when the parent window is a modal dialog, false otherwise.
	virtual HWND configure_embedded(const dsp_chain_config & p_initdata,HWND p_parent,unsigned p_id,bool p_from_modal) = 0;
	//! Retrieves current settings from an embedded DSP settings dialog. See also: configure_embedded().
	virtual void configure_embedded_retrieve(HWND wnd,dsp_chain_config & p_data) = 0;
	//! Changes current settings in an embedded DSP settings dialog. See also: configure_embedded().
	virtual void configure_embedded_change(HWND wnd,const dsp_chain_config & p_data) = 0;


	//! Helper - enables a DSP in core playback settings.
	void core_enable_dsp(const dsp_preset & preset);
	//! Helper - disables a DSP in core playback settings.
	void core_disable_dsp(const GUID & id);
	//! Helper - if a DSP with the specified identifier is present in playback settings, retrieves its configuration and returns true, otherwise returns false.
	bool core_query_dsp(const GUID & id, dsp_preset & out);
};

//! Callback class for getting notified about core playback DSP settings getting altered. \n
//! Register your implementations with static service_factory_single_t<myclass> g_myclass_factory;
class NOVTABLE dsp_config_callback : public service_base {
	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(dsp_config_callback);
public:
	//! Called when core playback DSP settings change. \n
	//! Note: you must not try to alter core playback DSP settings inside this callback, or call anything else that possibly alters core playback DSP settings.
	virtual void on_core_settings_change(const dsp_chain_config & p_newdata) = 0;
};
