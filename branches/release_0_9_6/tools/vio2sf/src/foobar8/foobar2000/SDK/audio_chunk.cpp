#include "foobar2000.h"

bool audio_chunk::set_data(const audio_sample * src,unsigned samples,unsigned nch,unsigned srate)
{
	bool rv = false;
	unsigned size = samples * nch;
	audio_sample * out = check_data_size(size);
	if (out)
	{
		if (src)
			mem_ops<audio_sample>::copy(out,src,size);
		else
			mem_ops<audio_sample>::set(out,0,size);
		set_sample_count(samples);
		set_channels(nch);
		set_srate(srate);
		rv = true;
	}
	else reset();
	return rv;
}

static bool check_exclusive(unsigned val, unsigned mask)
{
	return (val&mask)!=0 && (val&mask)!=mask;
}

//leet superfast fixedpoint->floatingpoint convertah
//somewhat big though due to excess templating

namespace {

	template<class T,bool b_swap,bool b_signed,bool b_pad> class msvc6_sucks_v2 { public:
		inline static void do_fixedpoint_convert(const void * source,unsigned bps,unsigned count,audio_sample* buffer)
		{
			const char * src = (const char *) source;
			unsigned bytes = bps>>3;
			unsigned n;
			T max = ((T)1)<<(bps-1);
			
			T negmask = - max;

			ASSUME(bytes<=sizeof(T));

			double div = 1.0 / (double)(1<<(bps-1));
			for(n=0;n<count;n++)
			{
				T temp;
				if (b_pad)
				{
					temp = 0;
					memcpy(&temp,src,bytes);
				}
				else
				{
					temp = * reinterpret_cast<const T*>(src);
				}

				if (b_swap) byte_order_helper::swap_order(&temp,bytes);
				
				if (!b_signed) temp ^= max;

				if (b_pad)
				{
					if (temp & max) temp |= negmask;
				}

				if (b_pad)
					src += bytes;
				else
					src += sizeof(T);


				buffer[n] = (audio_sample) ( (double)temp * div );
			}
		}
	};

	template <class T,bool b_pad> class msvc6_sucks { public:
		inline static void do_fixedpoint_convert(bool b_swap,bool b_signed,const void * source,unsigned bps,unsigned count,audio_sample* buffer)
		{
			if (sizeof(T)==1)
			{
				if (b_signed)
				{
					msvc6_sucks_v2<T,false,true,b_pad>::do_fixedpoint_convert(source,bps,count,buffer);
				}
				else
				{
					msvc6_sucks_v2<T,false,false,b_pad>::do_fixedpoint_convert(source,bps,count,buffer);
				}
			}
			else if (b_swap)
			{
				if (b_signed)
				{
					msvc6_sucks_v2<T,true,true,b_pad>::do_fixedpoint_convert(source,bps,count,buffer);
				}
				else
				{
					msvc6_sucks_v2<T,true,false,b_pad>::do_fixedpoint_convert(source,bps,count,buffer);
				}
			}
			else
			{
				if (b_signed)
				{
					msvc6_sucks_v2<T,false,true,b_pad>::do_fixedpoint_convert(source,bps,count,buffer);
				}
				else
				{
					msvc6_sucks_v2<T,false,false,b_pad>::do_fixedpoint_convert(source,bps,count,buffer);
				}
			}
		}
	};


};


bool audio_chunk::set_data_fixedpoint_ex(const void * source,unsigned size,unsigned srate,unsigned nch,unsigned bps,unsigned flags)
{
	assert( check_exclusive(flags,FLAG_SIGNED|FLAG_UNSIGNED) );
	assert( check_exclusive(flags,FLAG_LITTLE_ENDIAN|FLAG_BIG_ENDIAN) );

	bool need_swap = !!(flags & FLAG_BIG_ENDIAN);
	if (byte_order_helper::machine_is_big_endian()) need_swap = !need_swap;

	unsigned count = size / (bps/8);
	audio_sample * buffer = check_data_size(count);
	if (buffer==0) {reset();return false;}
	bool b_signed = !!(flags & FLAG_SIGNED);

	switch(bps)
	{
	case 8:
		msvc6_sucks<char,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 16:
		msvc6_sucks<short,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 24:
		msvc6_sucks<long,true>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 32:
		msvc6_sucks<long,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 40:
	case 48:
	case 56:
	case 64:
		msvc6_sucks<__int64,true>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
/*
		additional template would bump size while i doubt if anyone playing PCM above 32bit would care about performance gain
		msvc6_sucks<__int64,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;*/
	default:
		//unknown size, cant convert
		mem_ops<audio_sample>::setval(buffer,0,count);
		break;
	}
	set_sample_count(count/nch);
	set_srate(srate);
	set_channels(nch);
	return true;
}

bool audio_chunk::set_data_floatingpoint_ex(const void * ptr,unsigned size,unsigned srate,unsigned nch,unsigned bps,unsigned flags)
{
	assert(bps==32 || bps==64);
	assert( check_exclusive(flags,FLAG_LITTLE_ENDIAN|FLAG_BIG_ENDIAN) );
	assert( ! (flags & (FLAG_SIGNED|FLAG_UNSIGNED) ) );

	void (* p_orderfunc)(void * ptr,unsigned bytes) = (flags & FLAG_BIG_ENDIAN) ? byte_order_helper::order_be_to_native : byte_order_helper::order_le_to_native;

	unsigned bytes_per_sample = bps>>3;

	unsigned count = size / bytes_per_sample;
	audio_sample * out = check_data_size(count);
	if (out==0) {reset();return false;}
	unsigned char temp[64/8];
	const unsigned char * src = (const unsigned char *) ptr;
	while(count)
	{
		audio_sample val;
		memcpy(temp,src,bytes_per_sample);
		p_orderfunc(temp,bytes_per_sample);
		if (bps==32) val = (audio_sample) *(float*)&temp;
		else val = (audio_sample) *(double*)&temp;
		*(out++) = val;
		src += bytes_per_sample;
		count--;
	}
	return true;
}

#if audio_sample_size == 64
bool audio_chunk::set_data_32(const float * src,UINT samples,UINT nch,UINT srate)
{
	unsigned size = samples * nch;
	audio_sample * out = check_data_size(size);
	if (out==0) {reset();return false;}
	dsp_util::convert_32_to_64(src,out,size);
	set_sample_count(samples);
	set_channels(nch);
	set_srate(srate);
	return true;
}
#else
bool audio_chunk::set_data_64(const double * src,UINT samples,UINT nch,UINT srate)
{
	unsigned size = samples * nch;
	audio_sample * out = check_data_size(size);
	if (out==0) {reset();return false;}
	dsp_util::convert_64_to_32(src,out,size);
	set_sample_count(samples);
	set_channels(nch);
	set_srate(srate);
	return true;
}
#endif

bool audio_chunk::is_valid()
{
	unsigned nch = get_channels();
	if (nch==0 || nch>256) return false;
	unsigned srate = get_srate();
	if (srate<1000 || srate>1000000) return false;
	unsigned samples = get_sample_count();
	if (samples==0 || samples >= 0x80000000 / (sizeof(audio_sample) * nch) ) return false;
	unsigned size = get_data_size();
	if (samples * nch > size) return false;
	if (!get_data()) return false;
	return true;
}


bool audio_chunk::pad_with_silence_ex(unsigned samples,unsigned hint_nch,unsigned hint_srate)
{
	if (is_empty())
	{
		if (hint_srate && hint_nch)
		{
			return set_data(0,samples,hint_nch,hint_srate);
		}
		else return false;
	}
	else
	{
		if (hint_srate && hint_srate != get_srate()) samples = MulDiv(samples,get_srate(),hint_srate);
		if (samples > get_sample_count())
		{
			unsigned old_size = get_sample_count() * get_channels();
			unsigned new_size = samples * get_channels();
			audio_sample * ptr = check_data_size(new_size);
			if (ptr)
			{
				mem_ops<audio_sample>::set(ptr + old_size,0,new_size - old_size);
				set_sample_count(samples);
				return true;
			}
			else return false;			
		}
		else return true;
	}
}

bool audio_chunk::pad_with_silence(unsigned samples)
{
	if (samples > get_sample_count())
	{
		unsigned old_size = get_sample_count() * get_channels();
		unsigned new_size = samples * get_channels();
		audio_sample * ptr = check_data_size(new_size);
		if (ptr)
		{
			mem_ops<audio_sample>::set(ptr + old_size,0,new_size - old_size);
			set_sample_count(samples);
			return true;
		}
		else return false;			
	}
	else return true;
}

bool audio_chunk::insert_silence_fromstart(unsigned samples)
{
	unsigned old_size = get_sample_count() * get_channels();
	unsigned delta = samples * get_channels();
	unsigned new_size = old_size + delta;
	audio_sample * ptr = check_data_size(new_size);
	if (ptr)
	{
		mem_ops<audio_sample>::move(ptr+delta,ptr,old_size);
		mem_ops<audio_sample>::set(ptr,0,delta);
		set_sample_count(get_sample_count() + samples);
		return true;
	}
	else return false;			
}

unsigned audio_chunk::skip_first_samples(unsigned samples_delta)
{
	unsigned samples_old = get_sample_count();
	if (samples_delta >= samples_old)
	{
		set_sample_count(0);
		set_data_size(0);
		return samples_old;
	}
	else
	{
		unsigned samples_new = samples_old - samples_delta;
		unsigned nch = get_channels();
		audio_sample * ptr = get_data();
		mem_ops<audio_sample>::move(ptr,ptr+nch*samples_delta,nch*samples_new);
		set_sample_count(samples_new);
		set_data_size(nch*samples_new);
		return samples_delta;
	}
}