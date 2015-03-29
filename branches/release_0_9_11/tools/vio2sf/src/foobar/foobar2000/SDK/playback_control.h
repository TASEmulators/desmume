//! Provides control for various playback-related operations.
//! All methods provided by this interface work from main app thread only. Calling from another thread will do nothing or trigger an exception. If you need to trigger one of playback_control methods from another thread, see main_thread_callback.
//! Do not call playback_control methods from inside any kind of global callback (e.g. playlist callback), otherwise race conditions may occur.
//! Use static_api_ptr_t to instantiate. See static_api_ptr_t documentation for more info.
class NOVTABLE playback_control : public service_base
{
public:

	enum t_stop_reason {
		stop_reason_user = 0,
		stop_reason_eof,
		stop_reason_starting_another,
		stop_reason_shutting_down,
	};


	enum t_track_command {
		track_command_default = 0,
		track_command_play,
		track_command_next,
		track_command_prev,
		track_command_settrack,
		track_command_rand,
		track_command_resume,
	};

	//! Retrieves now playing item handle.
	//! @returns true on success, false on failure (not playing).
	virtual bool get_now_playing(metadb_handle_ptr & p_out) = 0;
	//! Starts playback. If playback is already active, existing process is stopped first.
	//! @param p_command Specifies what track to start playback from. See t_track_Command enum for more info.
	//! @param p_paused Specifies whether playback should be started as paused.
	virtual void start(t_track_command p_command = track_command_play,bool p_paused = false) = 0;
	//! Stops playback.
	virtual void stop() = 0;
	//! Returns whether playback is active.
	virtual bool is_playing() = 0;
	//! Returns whether playback is active and in paused state.
	virtual bool is_paused() = 0;
	//! Toggles pause state if playback is active.
	//! @param p_state set to true when pausing or to false when unpausing.
	virtual void pause(bool p_state) = 0;

	//! Retrieves stop-after-current-track option state.
	virtual bool get_stop_after_current() = 0;
	//! Alters stop-after-current-track option state.
	virtual void set_stop_after_current(bool p_state) = 0;
	
	//! Alters playback volume level.
	//! @param p_value volume in dB; 0 for full volume.
	virtual void set_volume(float p_value) = 0;
	//! Retrieves playback volume level.
	//! @returns current playback volume level, in dB; 0 for full volume.
	virtual float get_volume() = 0;
	//! Alters playback volume level one step up.
	virtual void volume_up() = 0;
	//! Alters playback volume level one step down.
	virtual void volume_down() = 0;
	//! Toggles playback mute state.
	virtual void volume_mute_toggle() = 0;
	//! Seeks in currenly played track to specified time.
	//! @param p_time target time in seconds.
	virtual void playback_seek(double p_time) = 0;
	//! Seeks in currently played track by specified time forward or back.
	//! @param p_delta time in seconds to seek by; can be positive to seek forward or negative to seek back.
	virtual void playback_seek_delta(double p_delta) = 0;
	//! Returns whether currently played track is seekable. If it's not, playback_seek/playback_seek_delta calls will be ignored.
	virtual bool playback_can_seek() = 0;
	//! Returns current playback position within currently played track, in seconds.
	virtual double playback_get_position() = 0;

	//! Type used to indicate level of dynamic playback-related info displayed. Safe to use with <> opereators, e.g. level above N always includes information rendered by level N.
	enum t_display_level {
		//! No playback-related info
		display_level_none,
		//! Static info and is_playing/is_paused stats
		display_level_basic,
		//! display_level_basic + dynamic track titles on e.g. live streams
		display_level_titles,
		//! display_level_titles + timing + VBR bitrate display etc
		display_level_all,
	};

	//! Renders information about currently playing item.
	//! @param p_hook Optional callback object overriding fields and functions; set to NULL if not used.
	//! @param p_out String receiving the output on success.
	//! @param p_script Titleformat script to use. Use titleformat_compiler service to create one.
	//! @param p_filter Optional callback object allowing input to be filtered according to context (i.e. removal of linebreak characters present in tags when rendering playlist lines). Set to NULL when not used.
	//! @param p_level Indicates level of dynamic playback-related info displayed. See t_display_level enum for more details.
	//! @returns true on success, false when no item is currently being played.
	virtual bool playback_format_title(titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<class titleformat_object> & p_script,titleformat_text_filter * p_filter,t_display_level p_level) = 0;
	


	//! Helper; renders info about any item, including currently playing item info if the item is currently played.
	bool playback_format_title_ex(metadb_handle_ptr p_item,titleformat_hook * p_hook,pfc::string_base & p_out,const service_ptr_t<class titleformat_object> & p_script,titleformat_text_filter * p_filter,t_display_level p_level) {
		if (p_item.is_empty()) return playback_format_title(p_hook,p_out,p_script,p_filter,p_level);
		metadb_handle_ptr temp;
		if (get_now_playing(temp)) {
			if (temp == p_item) {
				return playback_format_title(p_hook,p_out,p_script,p_filter,p_level);
			}
		}
		p_item->format_title(p_hook,p_out,p_script,p_filter);
		return true;
	}
	
	//! Helper; retrieves length of currently playing item.
	double playback_get_length();

	//! Toggles stop-after-current state.
	void toggle_stop_after_current() {set_stop_after_current(!get_stop_after_current());}
	//! Toggles pause state.
	void toggle_pause() {pause(!is_paused());}

	//! Starts playback if playback is inactive, otherwise toggles pause.
	void play_or_pause() {if (is_playing()) toggle_pause(); else start();}

	//deprecated
	inline void play_start(t_track_command p_command = track_command_play,bool p_paused = false) {start(p_command,p_paused);}
	//deprecated
	inline void play_stop() {stop();}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(playback_control);
};

class playback_control_v2 : public playback_control {
public:
	virtual float get_volume_step() = 0;

	FB2K_MAKE_SERVICE_INTERFACE(playback_control_v2,playback_control);
};

//for compatibility with old code
typedef playback_control play_control;
