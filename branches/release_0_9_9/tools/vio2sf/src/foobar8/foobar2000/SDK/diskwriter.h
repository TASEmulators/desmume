#ifndef _DISKWRITER_H_
#define _DISKWRITER_H_

#include "service.h"
#include "interface_helper.h"
#include "audio_chunk.h"
#include "file_info.h"

//one instance of object will be called *only* with one output file
class NOVTABLE diskwriter : public service_base
{
public:

	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	
	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

	virtual void get_name(string_base & out)=0;//name of your diskwriter service. never supposed to change
	virtual bool get_extension(string_base & out)=0;//return false if you don't actually write files; result is allowed to depend on preset
	
	virtual bool open(const char * out_path,unsigned expected_sample_rate,unsigned expected_bps,double expected_length,bool should_dither)=0;
	//expected_sample_rate may be null in rare cases, rely on it only as much as you must
	//expected_sample_rate/expected_bps are pulled from first file on to-encode list (there may be more), if you actually need them and start getting chunks with different specs, return error
	//expected_length is only as reliable as inputs reporting length, may not be strictly accurate

	virtual bool on_source_track(metadb_handle * ptr) {return true;}//return false to abort
		
	virtual bool process_samples(const audio_chunk * src)=0;//return true on success, false on failure

	virtual void flush()=0;
	//close output file etc
	
	virtual GUID get_guid()=0;//GUID of your diskwriter subclass, for storing config

	virtual void preset_set_data(const void * ptr,unsigned bytes)=0;
	virtual void preset_get_data(cfg_var::write_config_callback * out)=0;
	virtual bool preset_configurable()=0;//return if you support config dialog or not
	virtual bool preset_configure(HWND parent)=0;//return false if user aborted the operation

	virtual unsigned preset_default_get_count()=0;
	virtual void preset_default_enumerate(unsigned index,cfg_var::write_config_callback * out)=0;
	virtual bool preset_get_description(string_base & out)=0;
	

	static diskwriter * instantiate(const GUID & guid);
	static bool instantiate_test(const GUID & guid);
};

class NOVTABLE diskwriter_presetless : public diskwriter //helper, preset-less
{
public:
	virtual void preset_set_data(const void * ptr,unsigned bytes) {}
	virtual void preset_get_data(cfg_var::write_config_callback * out) {}
	virtual bool preset_configurable() {return false;}
	virtual bool preset_configure(HWND parent) {return false;}
	

	virtual unsigned preset_default_get_count() {return 1;}
	virtual void preset_default_enumerate(unsigned index,cfg_var::write_config_callback * out) {}
	virtual bool preset_get_description(string_base & out) {return false;}
	
};


template<class T>
class diskwriter_factory : public service_factory_t<diskwriter,T> {};

#endif