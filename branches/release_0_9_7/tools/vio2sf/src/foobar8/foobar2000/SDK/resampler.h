#ifndef _RESAMPLER_H_
#define _RESAMPLER_H_

#include "dsp.h"

//hint: use resampler_helper class declared below to resample

class NOVTABLE resampler : public dsp
{
public:
	static const GUID class_guid;
	static inline const GUID & get_class_guid() {return class_guid;}

	virtual void set_dest_sample_rate(unsigned srate)=0;
	virtual void set_config(unsigned flags)=0;//flags - see below
	virtual bool is_conversion_supported(unsigned src_srate,unsigned dst_srate)=0;

	virtual service_base * service_query(const GUID & guid)
	{
		if (guid == get_class_guid()) {service_add_ref();return this;}
		else return dsp::service_query(guid);
	}

	enum
	{
		FLAG_FAST = 0x0,
		FLAG_SLOW = 0x1,
	};

	//helper
	static bool get_interface(resampler ** out_resampler,dsp ** out_dsp)//may fail if no resampler DSP is installed
	{//you need to release both returned pointers
		service_enum_t<dsp> e;
		dsp * ptr;
		for(ptr=e.first();ptr;ptr=e.next())
		{
			resampler * ptr2 = service_query_t(resampler,ptr);
			if (ptr2)
			{
				*out_resampler = ptr2;
				*out_dsp = ptr;
				return true;
			}
			ptr->service_release();
		}
		return false;
	}

	static bool get_interface_ex(resampler ** out_resampler,dsp ** out_dsp,unsigned src_srate,unsigned dst_srate)//may fail if no resampler DSP is installed or installed ones cant handle this conversion
	{//you need to release both returned pointers
		service_enum_t<dsp> e;
		dsp * ptr;
		for(ptr=e.first();ptr;ptr=e.next())
		{
			resampler * ptr2 = service_query_t(resampler,ptr);
			if (ptr2)
			{
				if (ptr2->is_conversion_supported(src_srate,dst_srate))
				{
					*out_resampler = ptr2;
					*out_dsp = ptr;
					return true;
				}
				else
				{
					ptr2->service_release();
				}
			}
			ptr->service_release();
		}
		return false;
	}

	static dsp * create(unsigned srate,unsigned flags)
	{
		dsp * p_resampler_dsp;
		resampler * p_resampler;
		if (resampler::get_interface(&p_resampler,&p_resampler_dsp))
		{
			p_resampler->set_config(flags);
			p_resampler->set_dest_sample_rate(srate);
			p_resampler->service_release();
		}
		else
		{
			p_resampler_dsp = 0;
		}
		return p_resampler_dsp;
	}

	static dsp * create_ex(unsigned src_srate,unsigned dst_srate,unsigned flags)
	{
		dsp * p_resampler_dsp;
		resampler * p_resampler;
		if (resampler::get_interface_ex(&p_resampler,&p_resampler_dsp,src_srate,dst_srate))
		{
			p_resampler->set_config(flags);
			p_resampler->set_dest_sample_rate(dst_srate);
			p_resampler->service_release();
		}
		else
		{
			p_resampler_dsp = 0;
		}
		return p_resampler_dsp;
	}

};

class resampler_helper
{
	dsp * p_dsp;
	unsigned expected_dst_srate;
public:
	explicit resampler_helper()
	{
		p_dsp = 0;
		expected_dst_srate = 0;
	}

	void setup_ex(unsigned src_srate,unsigned dst_srate,unsigned flags)
	{
		if (!p_dsp) p_dsp = resampler::create_ex(src_srate,dst_srate,flags);
		expected_dst_srate = dst_srate;
	}

	void setup(unsigned flags,unsigned dst_srate)
	{
		if (!p_dsp) p_dsp = resampler::create(dst_srate,flags);
		expected_dst_srate = dst_srate;
	}

	bool run(dsp_chunk_list * list,unsigned flags)//returns false on failure; flags - see dsp.h
	{
		if (!p_dsp || expected_dst_srate<=0) return false;
		else
		{
			p_dsp->run(list,0,flags);
			bool rv = true;
			unsigned n,m=list->get_count();
			for(n=0;n<m;n++)
			{
				if (list->get_item(n)->get_srate() != expected_dst_srate) {rv = false;break;}
			}
			return rv;			
		}
	}

	void flush() {if (p_dsp) p_dsp->flush();}

	double get_latency()
	{
		return p_dsp ? p_dsp->get_latency() : 0;;
	}


	~resampler_helper()
	{
		if (p_dsp) p_dsp->service_release();
	}
};


#endif//_RESAMPLER_H_