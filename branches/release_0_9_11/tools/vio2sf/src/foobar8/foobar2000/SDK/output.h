#ifndef _OUTPUT_H_
#define _OUTPUT_H_

#include "service.h"
#include "audio_chunk.h"
#include "interface_helper.h"


class NOVTABLE output : public service_base
{
public:
	virtual double get_latency()=0;//in seconds
	virtual int process_samples(const output_chunk * chunk,int format_code)=0;//1 on success, 0 on error, no sleeping, copies data to its own buffer immediately; format_code - see enums below
	virtual int update()=0;//return 1 if ready for next write, 0 if still working with last process_samples(), -1 on error
	virtual void pause(int state)=0;
	virtual void flush()=0;
	virtual void force_play()=0;
	virtual GUID get_guid()=0;//for storing config
	virtual const char * get_name()=0;//for preferences
	virtual const char * get_config_page_name() {return 0;}//for "go to settings" button, return 0 if you dont have a config page

	enum
	{
		DATA_FORMAT_LINEAR_PCM,
		DATA_FORMAT_IEEE_FLOAT,//32bit float
	};


	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	static output * create(GUID g);

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return service_base::service_query(guid);
	}

};

//implementation helper, you can derive from this instead of deriving directly from output
class NOVTABLE output_i_base : public output
{
	mem_block_t<char> chunk;
	int chunk_size,chunk_ptr;
	int new_bps,new_srate,new_nch;
	int cur_bps,cur_srate,cur_nch;
	int is_open;
	int cur_format_code,new_format_code;


protected:
	output_i_base()
	{
		is_open = 0;
		chunk_size = chunk_ptr = 0;
	}

	void set_error(const char * msg);

	//override me
	virtual int open_ex(int srate,int bps,int nch,int format_code)//return 1 on success, 0 on failure, might get called when you are open already (eg. new PCM format)
	{
		if (format_code==DATA_FORMAT_LINEAR_PCM)
		{
			return open(srate,bps,nch);//call old version if not overridden
		}
		else
		{
			set_error("Unsupported output data format.");
			return 0;
		}
	}
	virtual int open(int srate,int bps,int nch) {return 0;}//old version
	virtual int can_write()=0;//in bytes
	virtual int write(const char * data,int bytes)=0;//return 1 on success, 0 on error, bytes always <= last can_write() return value
	virtual int get_latency_bytes()=0;//amount of data buffered (write_offset-playback_offset), in bytes	
	virtual void pause(int state)=0;
	virtual void force_play()=0;
	virtual int do_flush() {return 1;} //return 1 if you need reopen, 0 if successfully flushed
	virtual int is_playing() {return get_latency_bytes()>0;}
	virtual GUID get_guid()=0;
	virtual const char * get_name()=0;

public:
	virtual double get_latency()
	{
		double rv = 0;
		if (is_open)
		{
			int delta = get_latency_bytes();
			if (delta<0) delta=0;
			rv += (double) (delta / (cur_nch * cur_bps/8)) / (double) cur_srate;
		}
		if (chunk_size>0 && chunk_ptr<chunk_size)
		{
			rv += (double)( (chunk_size-chunk_ptr) / (new_nch * new_bps / 8) ) / (double) new_srate;
		}
		return rv;
	}

	virtual int process_samples(const output_chunk * p_chunk,int format_code)
	{
		chunk.copy((const char*)p_chunk->data,p_chunk->size);
		chunk_size = p_chunk->size;
		chunk_ptr = 0;
		new_bps = p_chunk->bps;
		new_srate = p_chunk->srate;
		new_nch = p_chunk->nch;
		new_format_code = format_code;
		return 1;
	}
	virtual int update()
	{
		if (chunk_size<=0 || chunk_ptr>=chunk_size) return 1;
		if (is_open && (cur_bps!=new_bps || cur_srate!=new_srate || cur_nch!=new_nch || cur_format_code!=new_format_code))
		{
			force_play();
			if (is_playing()) return 0;
			is_open = 0;
		}
		if (!is_open)
		{
			if (!open_ex(new_srate,new_bps,new_nch,new_format_code))
				return -1;//error
			cur_srate = new_srate;
			cur_nch = new_nch;
			cur_bps = new_bps;
			cur_format_code = new_format_code;
			is_open = 1;
		}

		int cw;
		while((cw = can_write())>0 && chunk_ptr<chunk_size)
		{
			int d = chunk_size - chunk_ptr;
			if (d>cw) d=cw;
			if (!write(chunk+chunk_ptr,d)) return -1;
			chunk_ptr+=d;
		}
		if (chunk_ptr<chunk_size)
		{
			force_play();
			return 0;
		}
		else
		{
			return 1;
		}
	}

	virtual void flush()
	{
		if (is_open)
		{
			if (do_flush()) is_open = 0;
		}
	}

};

template<class T>
class output_factory : public service_factory_t<output,T> {};

#endif