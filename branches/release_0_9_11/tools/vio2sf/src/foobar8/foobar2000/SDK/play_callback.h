#ifndef _PLAY_CALLBACK_H_
#define _PLAY_CALLBACK_H_

#include "service.h"

#include "metadb_handle.h"
//callbacks for getting notified about playback status
//multithread safety: all play_callback api calls come from main thread.

#include "play_control.h"

class NOVTABLE play_callback : public service_base
{
public:

	enum {
		MASK_on_playback_starting = 1<<0, MASK_on_playback_new_track = 1<<1, MASK_on_playback_stop = 1<<2,
		MASK_on_playback_seek = 1<<3,  MASK_on_playback_pause = 1<<4, MASK_on_playback_edited = 1<<5,
		MASK_on_playback_dynamic_info = 1<<6, MASK_on_playback_time = 1<<7, MASK_on_volume_change = 1<<8,
	};

	virtual unsigned get_callback_mask() {return (unsigned)(-1);}
	//returns combination of MASK_* above, return value must be constant, called only once
	//system uses it as a hint to avoid calling you when not needed, to reduce working code size (and "memory usage")

	virtual void on_playback_starting()=0;
	virtual void on_playback_new_track(metadb_handle * track)=0;
	virtual void on_playback_stop(play_control::stop_reason reason)=0;
	virtual void on_playback_seek(double time)=0;
	virtual void on_playback_pause(int state)=0;
	virtual void on_playback_edited(metadb_handle * track)=0;//currently played file got edited
	virtual void on_playback_dynamic_info(const file_info * info,bool b_track_change)=0;
	virtual void on_playback_time(metadb_handle * track,double val) {}//called every second
	virtual void on_volume_change(int new_val) {};//may also happen when not playing	

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}
};

template<class T>
class play_callback_factory : public service_factory_single_t<play_callback,T> {};


#endif