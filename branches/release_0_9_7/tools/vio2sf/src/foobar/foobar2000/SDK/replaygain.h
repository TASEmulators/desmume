//! Structure storing ReplayGain configuration: album/track source data modes, gain/peak processing modes and preamp values.
struct t_replaygain_config
{
	enum /*t_source_mode*/ {source_mode_none,source_mode_track,source_mode_album};
	enum /*t_processing_mode*/ {processing_mode_none,processing_mode_gain,processing_mode_gain_and_peak,processing_mode_peak};
	typedef t_uint32 t_source_mode; typedef t_uint32 t_processing_mode;

	t_replaygain_config() {reset();}
	t_replaygain_config(const t_replaygain_config & p_source) {*this = p_source;}
	t_replaygain_config(t_source_mode p_source_mode,t_processing_mode p_processing_mode,float p_preamp_without_rg, float p_preamp_with_rg)
		: m_source_mode(p_source_mode), m_processing_mode(p_processing_mode), m_preamp_without_rg(p_preamp_without_rg), m_preamp_with_rg(p_preamp_with_rg) {}

	
	t_source_mode m_source_mode;
	t_processing_mode m_processing_mode;
	float m_preamp_without_rg, m_preamp_with_rg;//preamp values in dB

	void reset();
	audio_sample query_scale(const file_info & info) const;
	audio_sample query_scale(const metadb_handle_ptr & info) const;

	void format_name(pfc::string_base & p_out) const;
	bool is_active() const;
};

FB2K_STREAM_READER_OVERLOAD(t_replaygain_config) {
	return stream >> value.m_source_mode >> value.m_processing_mode >> value.m_preamp_with_rg >> value.m_preamp_with_rg;
}
FB2K_STREAM_WRITER_OVERLOAD(t_replaygain_config) {
	return stream << value.m_source_mode << value.m_processing_mode << value.m_preamp_with_rg << value.m_preamp_with_rg;
}

//! Core service providing methods to retrieve/alter playback ReplayGain settings, as well as use ReplayGain configuration dialog.
class NOVTABLE replaygain_manager : public service_base {
public:
	//! Retrieves playback ReplayGain settings.
	virtual void get_core_settings(t_replaygain_config & p_out) = 0;

	//! Creates embedded version of ReplayGain settings dialog. Note that embedded dialog sends WM_COMMAND with id/BN_CLICKED to parent window when user makes changes to settings.
	virtual HWND configure_embedded(const t_replaygain_config & p_initdata,HWND p_parent,unsigned p_id,bool p_from_modal) = 0;
	//! Retrieves settings from embedded version of ReplayGain settings dialog.
	virtual void configure_embedded_retrieve(HWND wnd,t_replaygain_config & p_data) = 0;
	
	//! Shows popup/modal version of ReplayGain settings dialog. Returns true when user changed the settings, false when user cancelled the operation. Title parameter can be null to use default one.
	virtual bool configure_popup(t_replaygain_config & p_data,HWND p_parent,const char * p_title) = 0;

	//! Alters playback ReplayGain settings.
	virtual void set_core_settings(const t_replaygain_config & p_config) = 0;

	//! Helper; queries scale value for specified item according to core playback settings.
	audio_sample core_settings_query_scale(const file_info & p_info);
	//! Helper; queries scale value for specified item according to core playback settings.
	audio_sample core_settings_query_scale(const metadb_handle_ptr & info);

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(replaygain_manager);
};
