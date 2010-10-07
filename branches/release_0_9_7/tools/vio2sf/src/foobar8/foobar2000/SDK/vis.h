#ifndef _FOOBAR2000_VIS_H_
#define _FOOBAR2000_VIS_H_

#include "service.h"
#include "audio_chunk.h"

typedef float vis_sample;

struct vis_chunk
{
	enum
	{
		FLAG_LAGGED = 1,	//unless you need to process all samples in the stream for some reason (eg. scanning for peak), you should ignore chunks marked as "lagged"
	};

	vis_sample * data;
	unsigned samples;// sizeof(data) == sizeof(vis_sample) * samples * nch;
	unsigned srate,nch;
	vis_sample * spectrum;
	unsigned spectrum_size;// sizeof(spectrum) == sizeof(vis_sample) * spectrum_size * nch;
	unsigned flags;//see FLAG_* above

	inline double get_duration() const {return srate>0 ? (double)samples / (double)srate : 0;}

};//both data and spectrum are channel-interleaved

#define visualisation visualization
#define visualisation_factory visualization_factory

class visualization : public service_base
{
public://all calls are from main thread
	virtual void on_data(const vis_chunk * data)=0;
	virtual void on_flush()=0;
	virtual double get_expected_latency() {return 0;}//return time in seconds; allowed to change when running
	
	//allowed to change in runtime
	virtual bool is_active()=0;
	virtual bool need_spectrum()=0;

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static bool is_vis_manager_present()
	{
		static const GUID guid = 
		{ 0x5ca94fe1, 0x7593, 0x47de, { 0x8a, 0xdf, 0x8e, 0x36, 0xb4, 0x93, 0xa6, 0xd0 } };
		return service_enum_is_present(guid);
	}
};

template<class T>
class visualization_factory : public service_factory_single_t<visualization,T> {};

//usage: static visualisation_factory<myvisualisation> blah;

#endif //_FOOBAR2000_VIS_H_