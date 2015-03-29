#ifndef _PLAYBACK_CORE_H_
#define _PLAYBACK_CORE_H_

#include "service.h"
#include "metadb_handle.h"
#include "replaygain.h"
#include "output_manager.h"

struct playback_config
{
	enum dsp_mode_t
	{
		DSP_DEFAULT,//DSP enabled, use core settings
		DSP_DISABLED,
		DSP_CUSTOM,//DSP enabled, use custom settings
	};

	replaygain::mode_t replaygain_mode;
	dsp_mode_t dsp_mode;
	int process_total_time_counter;//boolean
	int low_priority;//boolean
	int process_dynamic_info;//boolean
	


	struct
	{
		unsigned int count;
		const GUID * guids;
	} dsp_custom;//valid only if dsp_mode = DSP_CUSTOM
	
	output_config output;
	
	playback_config()
	{
		replaygain_mode = replaygain::MODE_DEFAULT;
		dsp_mode = DSP_DEFAULT;
		dsp_custom.count = 0;
		dsp_custom.guids = 0;
		process_total_time_counter = 0;
		low_priority = 0;
		process_dynamic_info = 0;
	}
};


class NOVTABLE playback_core : public service_base
{
public:

	struct track//reminder: if you pass this around, functions you call will not free the metadb_handle; if you return this, make sure you handle_add_ref(), because the code calling you will be responsible for freeing
	{
		metadb_handle * handle;
		int user;
	};


	class NOVTABLE callback
	{
	public:
		enum file_error_handler
		{
			file_error_abort,
			file_error_continue,
			file_error_replace
		};
		virtual bool get_next_track(track * out)=0;//return false if end-of-playlist, true if out struct is filled with data
		virtual void on_file_open(const track * src) {}//called before attempting file open; use only for logging etc
		virtual void on_file_open_success(const track * ptr) {}
		virtual file_error_handler on_file_error(const track * src,track * replace) {return file_error_abort;}
		virtual void on_track_change(const track * ptr) {};//called when currently played track has changed, some time after get_next_track
		virtual void on_dynamic_info(const file_info * info,bool b_track_change) {}
		virtual void on_exiting(bool error) {};//called when terminating (either end-of-last-track or some kind of error or stop())
		
		//DO NOT call any playback_core methods from inside callback methods !
		//all callback methods are called from playback thread, you are responsible for taking proper anti-deadlock measures (eg. dont call SendMessage() directly)
		//safest way to handle end-of-playback: create a window from main thread, PostMessage to it when you get callbacks, execute callback handlers (and safely call playback_core api) from message handlers
	};

	class NOVTABLE vis_callback
	{
	public:
		virtual void on_data(class audio_chunk * chunk,double timestamp)=0;//timestamp == timestamp of first sample in the chunk, according to get_cur_time_fromstart() timing
		virtual void on_flush()=0;
	};

	struct stats
	{
		double decoding_position,playback_position,dsp_latency,output_latency;
	};


	//MUST be called first, immediately after creation (hint: create() does this for you)
	virtual void set_callback(callback * ptr)=0;

	virtual void set_vis_callback(vis_callback * ptr)=0;//optional

	virtual void set_config(const playback_config * data)=0;//call before starting playback

	virtual void start()=0;
	virtual void stop()=0;
	virtual bool is_playing()=0;
	virtual bool is_finished()=0;
	virtual bool is_paused()=0;
	virtual void pause(int paused)=0;

	virtual bool can_seek()=0;
	virtual void seek(double time)=0;
	virtual void seek_delta(double time_delta)=0;

	virtual double get_cur_time()=0;
	virtual double get_cur_time_fromstart()=0;
	virtual bool get_cur_track(track * out)=0;
	virtual bool get_stats(stats * out)=0;


	metadb_handle * get_cur_track_handle()
	{
		track t;
		if (get_cur_track(&t)) return t.handle;
		else return 0;
	}

	double get_total_time()
	{
		double rv = 0;
		metadb_handle * ptr = get_cur_track_handle();
		if (ptr)
		{
			rv = ptr->handle_get_length();
			ptr->handle_release();
		}
		return rv;
	}

	static playback_core * create(callback * cb)
	{
		playback_core * ptr = service_enum_create_t(playback_core,0);
		if (ptr) ptr->set_callback(cb);
		return ptr;		
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

class NOVTABLE play_sound : public service_base
{
public:
	enum
	{
		USE_REPLAYGAIN = 1,
		USE_DSP = 2,
	};

	virtual void play(metadb_handle * file,const playback_config * settings=0)=0;//settings are optional; if non-zero, they will be passed to playback_core being used
	virtual void stop_all()=0;
	
	static play_sound * get() {return service_enum_create_t(play_sound,0);}//safe not to release

	static void g_play(metadb_handle * file,const playback_config * settings)
	{
		play_sound * ptr = get();
		if (ptr) ptr->play(file,settings);
	}

	static void g_stop_all()
	{
		play_sound * ptr = get();
		if (ptr) ptr->stop_all();
	}

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

#endif