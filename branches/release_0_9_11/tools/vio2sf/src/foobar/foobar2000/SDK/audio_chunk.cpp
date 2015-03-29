#include "foobar2000.h"

void audio_chunk::set_data(const audio_sample * src,t_size samples,unsigned nch,unsigned srate,unsigned channel_config)
{
	t_size size = samples * nch;
	set_data_size(size);
	if (src)
		pfc::memcpy_t(get_data(),src,size);
	else
		pfc::memset_t(get_data(),(audio_sample)0,size);
	set_sample_count(samples);
	set_channels(nch,channel_config);
	set_srate(srate);
}

static bool check_exclusive(unsigned val, unsigned mask)
{
	return (val&mask)!=0 && (val&mask)!=mask;
}

namespace {

	template<class T,bool b_swap,bool b_signed,bool b_pad> class msvc6_sucks_v2 { public:
		inline static void do_fixedpoint_convert(const void * source,unsigned bps,t_size count,audio_sample* buffer)
		{
			const char * src = (const char *) source;
			unsigned bytes = bps>>3;
			t_size n;
			T max = ((T)1)<<(bps-1);
			
			T negmask = - max;

			ASSUME(bytes<=sizeof(T));

			const double div = 1.0 / (double)(1<<(bps-1));
			for(n=0;n<count;n++) {
				T temp;
				if (b_pad)
				{
					temp = 0;
					memcpy(&temp,src,bytes);
					if (b_swap) pfc::byteswap_raw(&temp,bytes);
				}
				else
				{
					temp = * reinterpret_cast<const T*>(src);
					if (b_swap) temp = pfc::byteswap_t(temp);
				}

				
				
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
		inline static void do_fixedpoint_convert(bool b_swap,bool b_signed,const void * source,unsigned bps,t_size count,audio_sample* buffer)
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


void audio_chunk::set_data_fixedpoint_ex(const void * source,t_size size,unsigned srate,unsigned nch,unsigned bps,unsigned flags,unsigned p_channel_config)
{
	assert( check_exclusive(flags,FLAG_SIGNED|FLAG_UNSIGNED) );
	assert( check_exclusive(flags,FLAG_LITTLE_ENDIAN|FLAG_BIG_ENDIAN) );

	bool need_swap = !!(flags & FLAG_BIG_ENDIAN);
	if (pfc::byte_order_is_big_endian) need_swap = !need_swap;

	t_size count = size / (bps/8);
	set_data_size(count);
	audio_sample * buffer = get_data();
	bool b_signed = !!(flags & FLAG_SIGNED);

	switch(bps)
	{
	case 8:
		msvc6_sucks<t_int8,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 16:
		if (!need_swap && b_signed) audio_math::convert_from_int16((const t_int16*)source,count,buffer,1.0);
		else msvc6_sucks<t_int16,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 24:
		msvc6_sucks<t_int32,true>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	case 32:
		if (!need_swap && b_signed) audio_math::convert_from_int32((const t_int32*)source,count,buffer,1.0);
		else msvc6_sucks<t_int32,false>::do_fixedpoint_convert(need_swap,b_signed,source,bps,count,buffer);
		break;
	default:
		//unknown size, cant convert
		pfc::memset_t(buffer,(audio_sample)0,count);
		break;
	}
	set_sample_count(count/nch);
	set_srate(srate);
	set_channels(nch,p_channel_config);
}

template<class t_float>
static void process_float_multi(audio_sample * p_out,const t_float * p_in,const t_size p_count)
{
	t_size n;
	for(n=0;n<p_count;n++)
		p_out[n] = (audio_sample)p_in[n];
}

template<class t_float>
static void process_float_multi_swap(audio_sample * p_out,const t_float * p_in,const t_size p_count)
{
	t_size n;
	for(n=0;n<p_count;n++) {
		p_out[n] = (audio_sample) pfc::byteswap_t(p_in[n]);
	}
}

void audio_chunk::set_data_floatingpoint_ex(const void * ptr,t_size size,unsigned srate,unsigned nch,unsigned bps,unsigned flags,unsigned p_channel_config)
{
	assert(bps==32 || bps==64);
	assert( check_exclusive(flags,FLAG_LITTLE_ENDIAN|FLAG_BIG_ENDIAN) );
	assert( ! (flags & (FLAG_SIGNED|FLAG_UNSIGNED) ) );

	bool use_swap = pfc::byte_order_is_big_endian ? !!(flags & FLAG_LITTLE_ENDIAN) : !!(flags & FLAG_BIG_ENDIAN);

	const t_size count = size / (bps/8);
	set_data_size(count);
	audio_sample * out = get_data();

	if (bps == 32)
	{
		if (use_swap)
			process_float_multi_swap(out,reinterpret_cast<const float*>(ptr),count);
		else
			process_float_multi(out,reinterpret_cast<const float*>(ptr),count);
	}
	else if (bps == 64)
	{
		if (use_swap)
			process_float_multi_swap(out,reinterpret_cast<const double*>(ptr),count);
		else
			process_float_multi(out,reinterpret_cast<const double*>(ptr),count);
	}
	else throw exception_io_data("invalid bit depth");

	set_sample_count(count/nch);
	set_srate(srate);
	set_channels(nch,p_channel_config);
}

bool audio_chunk::is_valid() const
{
	unsigned nch = get_channels();
	if (nch==0 || nch>256) return false;
	if (!g_is_valid_sample_rate(get_srate())) return false;
	t_size samples = get_sample_count();
	if (samples==0 || samples >= 0x80000000 / (sizeof(audio_sample) * nch) ) return false;
	t_size size = get_data_size();
	if (samples * nch > size) return false;
	if (!get_data()) return false;
	return true;
}


void audio_chunk::pad_with_silence_ex(t_size samples,unsigned hint_nch,unsigned hint_srate) {
	if (is_empty())
	{
		if (hint_srate && hint_nch) {
			return set_data(0,samples,hint_nch,hint_srate);
		} else throw exception_io_data();
	}
	else
	{
		if (hint_srate && hint_srate != get_srate()) samples = MulDiv_Size(samples,get_srate(),hint_srate);
		if (samples > get_sample_count())
		{
			t_size old_size = get_sample_count() * get_channels();
			t_size new_size = samples * get_channels();
			set_data_size(new_size);
			pfc::memset_t(get_data() + old_size,(audio_sample)0,new_size - old_size);
			set_sample_count(samples);
		}
	}
}

void audio_chunk::pad_with_silence(t_size samples) {
	if (samples > get_sample_count())
	{
		t_size old_size = get_sample_count() * get_channels();
		t_size new_size = pfc::multiply_guarded(samples,get_channels());
		set_data_size(new_size);
		pfc::memset_t(get_data() + old_size,(audio_sample)0,new_size - old_size);
		set_sample_count(samples);
	}
}

void audio_chunk::insert_silence_fromstart(t_size samples) {
	t_size old_size = get_sample_count() * get_channels();
	t_size delta = samples * get_channels();
	t_size new_size = old_size + delta;
	set_data_size(new_size);
	audio_sample * ptr = get_data();
	pfc::memmove_t(ptr+delta,ptr,old_size);
	pfc::memset_t(ptr,(audio_sample)0,delta);
	set_sample_count(get_sample_count() + samples);
}

t_size audio_chunk::skip_first_samples(t_size samples_delta)
{
	t_size samples_old = get_sample_count();
	if (samples_delta >= samples_old)
	{
		set_sample_count(0);
		set_data_size(0);
		return samples_old;
	}
	else
	{
		t_size samples_new = samples_old - samples_delta;
		unsigned nch = get_channels();
		audio_sample * ptr = get_data();
		pfc::memmove_t(ptr,ptr+nch*samples_delta,nch*samples_new);
		set_sample_count(samples_new);
		set_data_size(nch*samples_new);
		return samples_delta;
	}
}

audio_sample audio_chunk::get_peak(audio_sample p_peak) const {
	return pfc::max_t(p_peak, get_peak());
}

audio_sample audio_chunk::get_peak() const {
	return audio_math::calculate_peak(get_data(),get_sample_count() * get_channels());
}

void audio_chunk::scale(audio_sample p_value)
{
	audio_sample * ptr = get_data();
	audio_math::scale(ptr,get_sample_count() * get_channels(),ptr,p_value);
}


static void render_8bit(const audio_sample * in, t_size inLen, void * out) {
	t_int8 * outWalk = reinterpret_cast<t_int8*>(out);
	for(t_size walk = 0; walk < inLen; ++walk) {
		*outWalk++ = (t_int8)pfc::clip_t<t_int32>(audio_math::rint32( in[walk] * 0x80 ), -128, 127);
	}
}
static void render_24bit(const audio_sample * in, t_size inLen, void * out) {
	t_uint8 * outWalk = reinterpret_cast<t_uint8*>(out);
	for(t_size walk = 0; walk < inLen; ++walk) {
		const t_int32 v = pfc::clip_t<t_int32>(audio_math::rint32( in[walk] * 0x800000 ), -128 * 256 * 256, 128 * 256 * 256 - 1);
		*(outWalk ++) = (t_uint8) (v & 0xFF);
		*(outWalk ++) = (t_uint8) ((v >> 8) & 0xFF);
		*(outWalk ++) = (t_uint8) ((v >> 16) & 0xFF);
	}
}

bool audio_chunk::to_raw_data(mem_block_container & out, t_uint32 bps) const {
	const t_size samples = get_sample_count();
	const t_size dataLen = pfc::multiply_guarded(samples, (t_size)get_channel_count());
	switch(bps) {
		case 8:
			out.set_size(dataLen);
			render_8bit(get_data(), dataLen, out.get_ptr());
			break;
		case 16:
			out.set_size(dataLen * 2);
			audio_math::convert_to_int16(get_data(), dataLen, reinterpret_cast<t_int16*>(out.get_ptr()), 1.0);
			break;
		case 24:
			out.set_size(dataLen * 3);
			render_24bit(get_data(), dataLen, out.get_ptr());
			break;
		case 32:
			pfc::static_assert<sizeof(audio_sample) == 4>();
			out.set(get_data(), dataLen * sizeof(audio_sample));
			break;
		default:
			return false;
	}
	return true;
}
