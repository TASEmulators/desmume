#ifndef _DSP_H_
#define _DSP_H_

#include "service.h"
#include "audio_chunk.h"
#include "interface_helper.h"

namespace dsp_util
{
	__int64 duration_samples_from_time(double time,unsigned srate);

	void kill_denormal_32(float * ptr,unsigned count);
	void kill_denormal_64(double * ptr,unsigned count);
	void scale(audio_sample * ptr,double scale,unsigned count);
	inline void kill_denormal(audio_sample * ptr,unsigned count)
	{
#if audio_sample_size == 32
		kill_denormal_32(ptr,count);
#else
		kill_denormal_64(ptr,count);
#endif
	}
	inline void scale_chunk(audio_chunk * ptr,double scale) {dsp_util::scale(ptr->get_data(),scale,ptr->get_data_length());}
	
	void __fastcall convert_32_to_64(const float  * src,double * dest,unsigned count);
	void __fastcall convert_64_to_32(const double * src,float  * dest,unsigned count);
};

class NOVTABLE dsp_chunk_list//interface (cross-dll safe)
{
public:
	virtual unsigned get_count() const = 0;
	virtual audio_chunk * get_item(unsigned n) const = 0;
	virtual void remove_by_idx(unsigned idx) = 0;
	virtual void remove_mask(const bit_array & mask) = 0;
	virtual audio_chunk * insert_item(unsigned idx,unsigned hint_size=0) = 0;

	audio_chunk * add_item(unsigned hint_size=0) {return insert_item(get_count(),hint_size);}

	void remove_all() {remove_mask(bit_array_true());}

	double get_duration()
	{
		double rv = 0;
		unsigned n,m = get_count();
		for(n=0;n<m;n++) rv += get_item(n)->get_duration();
		return rv;
	}

	void add_chunk(const audio_chunk * chunk)
	{
		audio_chunk * dst = insert_item(get_count(),chunk->get_data_length());
		if (dst) dst->copy_from(chunk);
	}

	void remove_bad_chunks();
};

class dsp_chunk_list_i : public dsp_chunk_list//implementation
{
	ptr_list_simple<audio_chunk_i> data,recycled;
public:
	virtual unsigned get_count() const;
	virtual audio_chunk * get_item(unsigned n) const;
	virtual void remove_by_idx(unsigned idx);
	virtual void remove_mask(const bit_array & mask);
	virtual audio_chunk * insert_item(unsigned idx,unsigned hint_size=0);
	~dsp_chunk_list_i();
};

class NOVTABLE dsp : public service_base
{
public:
	enum
	{
		END_OF_TRACK = 1,	//flush whatever you need to when tracks change
		FLUSH = 2	//flush everything
	};

	virtual GUID get_guid()=0;
	virtual const char * get_name()=0;
	virtual void run(dsp_chunk_list * list,class metadb_handle * cur_file,int flags)=0;//int flags <= see flags above
	//cur_file is OPTIONAL and may be null
	virtual void flush() {}//after seek etc
	virtual double get_latency() {return 0;}//amount of data buffered (in seconds)
	virtual int need_track_change_mark() {return 0;}//return 1 if you need to know exact track change point (eg. for crossfading, removing silence), will force-flush any DSPs placed before you so when you get END_OF_TRACK, chunks you get contain last samples of the track; will often break regular gapless playback so don't use it unless you have reasons to


	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static GUID guid_from_name(const char * name);//may fail if DSP isn't present (eg. dll got removed)
	static const char * name_from_guid(GUID g);//may fail if DSP isn't present (eg. dll got removed)
	static dsp * instance_from_guid(GUID g);//may fail if DSP isn't present (eg. dll got removed)

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	void get_config(cfg_var::write_config_callback * out);
	void set_config(const void * src,unsigned bytes);
	bool popup_config(HWND parent);

};

class NOVTABLE dsp_v2 : public dsp
{
public:
	virtual void config_get(cfg_var::write_config_callback * out) {};
	virtual void config_set(const void * src,unsigned bytes) {};
	virtual bool config_popup(HWND parent) {return false;};//return false if you don't support config dialog
	virtual bool config_supported()=0;

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}


	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return dsp::service_query(guid);
	}

};

template<class T>
class dsp_i_base_t : public T
{
private:
	dsp_chunk_list * list;
	unsigned chunk_ptr;
	metadb_handle * cur_file;
	virtual void run(dsp_chunk_list * p_list,class metadb_handle * p_cur_file,int flags);
protected:
	inline metadb_handle * get_cur_file() {return cur_file;}// call only from on_chunk / on_endoftrack (on_endoftrack will give info on track being finished); may return null !!
	dsp_i_base_t() {list = 0;cur_file=0;chunk_ptr=0;}
	audio_chunk * insert_chunk(unsigned hint_size = 0)	//call only from on_endoftrack / on_endofplayback / on_chunk
	{//hint_size - optional, amout of buffer space you want to use
		return list ? list->insert_item(chunk_ptr++,hint_size) : 0;
	}
	//override these
	virtual void on_endoftrack()//use insert_chunk() if you have data you want to dump
	{
	}
	virtual void on_endofplayback()
	{//use insert_chunk() if you have data you want to dump

	}
	virtual bool on_chunk(audio_chunk * chunk)
	{//return true if your chunk, possibly modified needs to be put back into chain, false if you want it removed
		//use insert_chunk() if you want to insert pending data before current chunk
		return true;
	}

public:
	virtual GUID get_guid()=0;
	virtual const char * get_name()=0;
	
	virtual void flush() {}//after seek etc
	virtual double get_latency() {return 0;}//amount of data buffered (in seconds)
	virtual int need_track_change_mark() {return 0;}//return 1 if you need to know exact track change point (eg. for crossfading, removing silence), will force-flush any DSPs placed before you so when you get END_OF_TRACK, chunks you get contain last samples of the track; will often break regular gapless playback so don't use it unless you have reasons to

};

template<class T>
void dsp_i_base_t<T>::run(dsp_chunk_list * p_list,class metadb_handle * p_cur_file,int flags)
{
	list = p_list;
	cur_file = p_cur_file;
	for(chunk_ptr = 0;chunk_ptr<list->get_count();chunk_ptr++)
	{
		audio_chunk * c = list->get_item(chunk_ptr);
		if (c->is_empty() || !on_chunk(c))
			list->remove_by_idx(chunk_ptr--);
	}
	if (flags & FLUSH)
		on_endofplayback();
	else if (flags & END_OF_TRACK)
		on_endoftrack();

	list = 0;
	cur_file = 0;
}


class dsp_i_base : public dsp_i_base_t<dsp> {};

template<class T>
class dsp_factory : public service_factory_t<dsp,T> {};


#include "dsp_manager.h"

#endif