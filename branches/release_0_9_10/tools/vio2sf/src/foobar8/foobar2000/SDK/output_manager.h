#ifndef _OUTPUT_MANAGER_H_
#define _OUTPUT_MANAGER_H_

#include "output.h"

struct output_config
{
	GUID output;//null guid for default user-configured device

	int b_override_format;

	struct format_t
	{
		int bps,bps_pad;
		int b_float;
		int b_dither;

	};

	format_t format;//format valid only if b_override_format is set to non-zero, otherwise user settings from core preferences are used
	
	output_config()
	{
		memset(&output,0,sizeof(output));
		b_override_format = 0;

		format.bps = 16;
		format.bps_pad = 16;
		format.b_float = 0;
		format.b_dither = 1;
	}
};

class NOVTABLE output_manager : public service_base
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static output_manager * create() {return service_enum_create_t(output_manager,0);}//may fail on pre-0.63

	virtual void get_default_config(output_config * out)=0;//retrieves current user settings from core preferences; do not pass returned data to set_config (same effect as not calling set_config at all or passing uninitialized struct)

	virtual void set_config(const output_config * param)=0;//optionally call before first use
	
	virtual double get_latency()=0;//in seconds
	virtual int process_samples(const audio_chunk* chunk)=0;//1 on success, 0 on error, no sleeping, copies data to its own buffer immediately; format_code - see enums below
	virtual int update()=0;//return 1 if ready for next write, 0 if still working with last process_samples(), -1 on error
	virtual void pause(int state)=0;
	virtual void flush()=0;
	virtual void force_play()=0;
};

#endif //_OUTPUT_MANAGER_H_