//! Thrown when audio_chunk sample rate or channel mapping changes in mid-stream and the code receiving audio_chunks can't deal with that scenario.
PFC_DECLARE_EXCEPTION(exception_unexpected_audio_format_change, exception_io_data, "Unexpected audio format change" );

//! Interface to container of a chunk of audio data. See audio_chunk_impl for an implementation.
class NOVTABLE audio_chunk {
public:

	enum {
		sample_rate_min = 1000, sample_rate_max = 1000000
	};
	static bool g_is_valid_sample_rate(t_uint32 p_val) {return p_val >= sample_rate_min && p_val <= sample_rate_max;}
	
	//! Channel map flag declarations. Note that order of interleaved channel data in the stream is same as order of these flags.
	enum
	{
		channel_front_left			= 1<<0,
		channel_front_right			= 1<<1,
		channel_front_center		= 1<<2,
		channel_lfe					= 1<<3,
		channel_back_left			= 1<<4,
		channel_back_right			= 1<<5,
		channel_front_center_left	= 1<<6,
		channel_front_center_right	= 1<<7,
		channel_back_center			= 1<<8,
		channel_side_left			= 1<<9,
		channel_side_right			= 1<<10,
		channel_top_center			= 1<<11,
		channel_top_front_left		= 1<<12,
		channel_top_front_center	= 1<<13,
		channel_top_front_right		= 1<<14,
		channel_top_back_left		= 1<<15,
		channel_top_back_center		= 1<<16,
		channel_top_back_right		= 1<<17,

		channel_config_mono = channel_front_center,
		channel_config_stereo = channel_front_left | channel_front_right,
		channel_config_5point1 = channel_front_left | channel_front_right | channel_back_left | channel_back_right | channel_front_center | channel_lfe,

		defined_channel_count = 18,
	};

	//! Helper function; guesses default channel map for specified channel count.
	static unsigned g_guess_channel_config(unsigned count);

#ifdef _WIN32
	//! Helper function; translates audio_chunk channel map to WAVEFORMATEXTENSIBLE channel map.
	static DWORD g_channel_config_to_wfx(unsigned p_config);
	//! Helper function; translates WAVEFORMATEXTENSIBLE channel map to audio_chunk channel map.
	static unsigned g_channel_config_from_wfx(DWORD p_wfx);
#endif

	//! Extracts flag describing Nth channel from specified map. Usable to figure what specific channel in a stream means.
	static unsigned g_extract_channel_flag(unsigned p_config,unsigned p_index);
	//! Counts channels specified by channel map.
	static unsigned g_count_channels(unsigned p_config);
	//! Calculates index of a channel specified by p_flag in a stream where channel map is described by p_config.
	static unsigned g_channel_index_from_flag(unsigned p_config,unsigned p_flag);

	

	//! Retrieves audio data buffer pointer (non-const version). Returned pointer is for temporary use only; it is valid until next set_data_size call, or until the object is destroyed. \n
	//! Size of returned buffer is equal to get_data_size() return value (in audio_samples). Amount of actual data may be smaller, depending on sample count and channel count. Conditions where sample count * channel count are greater than data size should not be possible.
	virtual audio_sample * get_data() = 0;
	//! Retrieves audio data buffer pointer (const version). Returned pointer is for temporary use only; it is valid until next set_data_size call, or until the object is destroyed. \n
	//! Size of returned buffer is equal to get_data_size() return value (in audio_samples). Amount of actual data may be smaller, depending on sample count and channel count. Conditions where sample count * channel count are greater than data size should not be possible.
	virtual const audio_sample * get_data() const = 0;
	//! Retrieves size of allocated buffer space, in audio_samples.
	virtual t_size get_data_size() const = 0;
	//! Resizes audio data buffer to specified size. Throws std::bad_alloc on failure.
	virtual void set_data_size(t_size p_new_size) = 0;
	
	//! Retrieves sample rate of contained audio data.
	virtual unsigned get_srate() const = 0;
	//! Sets sample rate of contained audio data.
	virtual void set_srate(unsigned val) = 0;
	//! Retrieves channel count of contained audio data.
	virtual unsigned get_channels() const = 0;
	//! Helper - for consistency - same as get_channels().
	inline unsigned get_channel_count() const {return get_channels();}
	//! Retrieves channel map of contained audio data. Conditions where number of channels specified by channel map don't match get_channels() return value should not be possible.
	virtual unsigned get_channel_config() const = 0;
	//! Sets channel count / channel map.
	virtual void set_channels(unsigned p_count,unsigned p_config) = 0;

	//! Retrieves number of valid samples in the buffer. \n
	//! Note that a "sample" means a unit of interleaved PCM data representing states of each channel at given point of time, not a single PCM value. \n
	//! For an example, duration of contained audio data is equal to sample count / sample rate, while actual size of contained data is equal to sample count * channel count.
	virtual t_size get_sample_count() const = 0;
	
	//! Sets number of valid samples in the buffer. WARNING: sample count * channel count should never be above allocated buffer size.
	virtual void set_sample_count(t_size val) = 0;

	//! Helper, same as get_srate().
	inline unsigned get_sample_rate() const {return get_srate();}
	//! Helper, same as set_srate().
	inline void set_sample_rate(unsigned val) {set_srate(val);}

	//! Helper; sets channel count to specified value and uses default channel map for this channel count.
	void set_channels(unsigned val) {set_channels(val,g_guess_channel_config(val));}


	//! Helper; resizes audio data buffer when it's current size is smaller than requested.
	inline void grow_data_size(t_size p_requested) {if (p_requested > get_data_size()) set_data_size(p_requested);}


	//! Retrieves duration of contained audio data, in seconds.
	inline double get_duration() const
	{
		double rv = 0;
		t_size srate = get_srate (), samples = get_sample_count();
		if (srate>0 && samples>0) rv = (double)samples/(double)srate;
		return rv;
	}
	
	//! Returns whether the chunk is empty (contains no audio data).
	inline bool is_empty() const {return get_channels()==0 || get_srate()==0 || get_sample_count()==0;}
	
	//! Returns whether the chunk contents are valid (for bug check purposes).
	bool is_valid() const;

	//! Returns actual amount of audio data contained in the buffer (sample count * channel count). Must not be greater than data size (see get_data_size()).
	inline t_size get_data_length() const {return get_sample_count() * get_channels();}

	//! Resets all audio_chunk data.
	inline void reset() {
		set_sample_count(0);
		set_srate(0);
		set_channels(0);
		set_data_size(0);
	}
	
	//! Helper, sets chunk data to contents of specified buffer, with specified number of channels / sample rate / channel map.
	void set_data(const audio_sample * src,t_size samples,unsigned nch,unsigned srate,unsigned channel_config);
	
	//! Helper, sets chunk data to contents of specified buffer, with specified number of channels / sample rate, using default channel map for specified channel count.
	inline void set_data(const audio_sample * src,t_size samples,unsigned nch,unsigned srate) {set_data(src,samples,nch,srate,g_guess_channel_config(nch));}
	
	//! Helper, sets chunk data to contents of specified buffer, using default win32/wav conventions for signed/unsigned switch.
	inline void set_data_fixedpoint(const void * ptr,t_size bytes,unsigned srate,unsigned nch,unsigned bps,unsigned channel_config) {
		set_data_fixedpoint_ex(ptr,bytes,srate,nch,bps,(bps==8 ? FLAG_UNSIGNED : FLAG_SIGNED) | flags_autoendian(), channel_config);
	}

	inline void set_data_fixedpoint_unsigned(const void * ptr,t_size bytes,unsigned srate,unsigned nch,unsigned bps,unsigned channel_config) {
		return set_data_fixedpoint_ex(ptr,bytes,srate,nch,bps,FLAG_UNSIGNED | flags_autoendian(), channel_config);
	}

	inline void set_data_fixedpoint_signed(const void * ptr,t_size bytes,unsigned srate,unsigned nch,unsigned bps,unsigned channel_config) {
		return set_data_fixedpoint_ex(ptr,bytes,srate,nch,bps,FLAG_SIGNED | flags_autoendian(), channel_config);
	}

	enum
	{
		FLAG_LITTLE_ENDIAN = 1,
		FLAG_BIG_ENDIAN = 2,
		FLAG_SIGNED = 4,
		FLAG_UNSIGNED = 8,
	};

	inline static unsigned flags_autoendian() {
		return pfc::byte_order_is_big_endian ? FLAG_BIG_ENDIAN : FLAG_LITTLE_ENDIAN;
	}

	void set_data_fixedpoint_ex(const void * ptr,t_size bytes,unsigned p_sample_rate,unsigned p_channels,unsigned p_bits_per_sample,unsigned p_flags,unsigned p_channel_config);//p_flags - see FLAG_* above

	void set_data_floatingpoint_ex(const void * ptr,t_size bytes,unsigned p_sample_rate,unsigned p_channels,unsigned p_bits_per_sample,unsigned p_flags,unsigned p_channel_config);//signed/unsigned flags dont apply

	inline void set_data_32(const float * src,t_size samples,unsigned nch,unsigned srate) {return set_data(src,samples,nch,srate);}

	void pad_with_silence_ex(t_size samples,unsigned hint_nch,unsigned hint_srate);
	void pad_with_silence(t_size samples);
	void insert_silence_fromstart(t_size samples);
	t_size skip_first_samples(t_size samples);

	//! Simple function to get original PCM stream back. Assumes host's endianness, integers are signed - including the 8bit mode; 32bit mode assumed to be float.
	//! @returns false when the conversion could not be performed because of unsupported bit depth etc.
	bool to_raw_data(class mem_block_container & out, t_uint32 bps) const;


	//! Helper, calculates peak value of data in the chunk. The optional parameter specifies initial peak value, to simplify calling code.
	audio_sample get_peak(audio_sample p_peak) const;
	audio_sample get_peak() const;

	//! Helper function; scales entire chunk content by specified value.
	void scale(audio_sample p_value);

	//! Helper; copies content of another audio chunk to this chunk.
	void copy(const audio_chunk & p_source) {
		set_data(p_source.get_data(),p_source.get_sample_count(),p_source.get_channels(),p_source.get_srate(),p_source.get_channel_config());
	}

	const audio_chunk & operator=(const audio_chunk & p_source) {
		copy(p_source);
		return *this;
	}
protected:
	audio_chunk() {}
	~audio_chunk() {}	
};

//! Implementation of audio_chunk. Takes pfc allocator template as template parameter.
template<template<typename> class t_alloc = pfc::alloc_standard>
class audio_chunk_impl_t : public audio_chunk {
	typedef audio_chunk_impl_t<t_alloc> t_self;
	pfc::array_t<audio_sample,t_alloc> m_data;
	unsigned m_srate,m_nch,m_setup;
	t_size m_samples;
public:
	audio_chunk_impl_t() : m_srate(0), m_nch(0), m_samples(0), m_setup(0) {}
	audio_chunk_impl_t(const audio_sample * src,unsigned samples,unsigned nch,unsigned srate) : m_srate(0), m_nch(0), m_samples(0)
	{set_data(src,samples,nch,srate);}
	audio_chunk_impl_t(const audio_chunk & p_source) : m_srate(0), m_nch(0), m_samples(0), m_setup(0) {copy(p_source);}
	audio_chunk_impl_t(const t_self & p_source) : m_srate(0), m_nch(0), m_samples(0), m_setup(0) {copy(p_source);}
	
	virtual audio_sample * get_data() {return m_data.get_ptr();}
	virtual const audio_sample * get_data() const {return m_data.get_ptr();}
	virtual t_size get_data_size() const {return m_data.get_size();}
	virtual void set_data_size(t_size new_size) {m_data.set_size(new_size);}
	
	virtual unsigned get_srate() const {return m_srate;}
	virtual void set_srate(unsigned val) {m_srate=val;}
	virtual unsigned get_channels() const {return m_nch;}
	virtual unsigned get_channel_config() const {return m_setup;}
	virtual void set_channels(unsigned val,unsigned setup) {m_nch = val;m_setup = setup;}
	void set_channels(unsigned val) {set_channels(val,g_guess_channel_config(val));}

	virtual t_size get_sample_count() const {return m_samples;}
	virtual void set_sample_count(t_size val) {m_samples = val;}

	const t_self & operator=(const audio_chunk & p_source) {copy(p_source);return *this;}
	const t_self & operator=(const t_self & p_source) {copy(p_source);return *this;}
};

typedef audio_chunk_impl_t<> audio_chunk_impl;
typedef audio_chunk_impl_t<pfc::alloc_fast_aggressive> audio_chunk_impl_temporary;
typedef audio_chunk_impl audio_chunk_i;//for compatibility

//! Implements const methods of audio_chunk only, referring to an external buffer. For temporary use only (does not maintain own storage), e.g.: somefunc( audio_chunk_temp_impl(mybuffer,....) );
class audio_chunk_temp_impl : public audio_chunk {
public:
	audio_chunk_temp_impl(const audio_sample * p_data,t_size p_samples,t_uint32 p_sample_rate,t_uint32 p_channels,t_uint32 p_channel_config) :
	m_data(p_data), m_samples(p_samples), m_sample_rate(p_sample_rate), m_channels(p_channels), m_channel_config(p_channel_config)
	{
		PFC_ASSERT(is_valid());
	}

	audio_sample * get_data() {throw pfc::exception_not_implemented();}
	const audio_sample * get_data() const {return m_data;}
	t_size get_data_size() const {return m_samples * m_channels;}
	void set_data_size(t_size p_new_size) {throw pfc::exception_not_implemented();}
	
	unsigned get_srate() const {return m_sample_rate;}
	void set_srate(unsigned val) {throw pfc::exception_not_implemented();}
	unsigned get_channels() const {return m_channels;}
	unsigned get_channel_config() const {return m_channel_config;}
	void set_channels(unsigned p_count,unsigned p_config) {throw pfc::exception_not_implemented();}

	t_size get_sample_count() const {return m_samples;}
	
	void set_sample_count(t_size val) {throw pfc::exception_not_implemented();}

private:
	t_size m_samples;
	t_uint32 m_sample_rate,m_channels,m_channel_config;
	const audio_sample * m_data;
};



//! Duration counter class - accumulates duration using sample values, without any kind of rounding error accumulation.
class duration_counter {
public:
	duration_counter() : m_offset() {
	}
	void set(double v) {
		m_sampleCounts.remove_all();
		m_offset = v;
	}
	void reset() {
		set(0);
	}

	void add(double v) {m_offset += v;}
	void subtract(double v) {m_offset -= v;}

	double query() const {
		double acc = m_offset;
		for(t_map::const_iterator walk = m_sampleCounts.first(); walk.is_valid(); ++walk) {
			acc += audio_math::samples_to_time(walk->m_value, walk->m_key);
		}
		return acc;
	}
	void add(const audio_chunk & c) {
		add(c.get_sample_count(), c.get_sample_rate());
	}
	void add(t_uint64 sampleCount, t_uint32 sampleRate) {
		PFC_ASSERT( sampleRate > 0 );
		if (sampleRate > 0 && sampleCount > 0) {
			m_sampleCounts.find_or_add(sampleRate) += sampleCount;
		}
	}
	void add(const duration_counter & other) {
		add(other.m_offset);
		for(t_map::const_iterator walk = other.m_sampleCounts.first(); walk.is_valid(); ++walk) {
			add(walk->m_value, walk->m_key);
		}
	}
	void subtract(const duration_counter & other) {
		subtract(other.m_offset);
		for(t_map::const_iterator walk = other.m_sampleCounts.first(); walk.is_valid(); ++walk) {
			subtract(walk->m_value, walk->m_key);
		}
	}
	void subtract(t_uint64 sampleCount, t_uint32 sampleRate) {
		PFC_ASSERT( sampleRate > 0 );
		if (sampleRate > 0 && sampleCount > 0) {
			t_uint64 * val = m_sampleCounts.query_ptr(sampleRate);
			if (val == NULL) throw pfc::exception_invalid_params();
			if (*val < sampleCount) throw pfc::exception_invalid_params();
			else if (*val == sampleCount) {
				m_sampleCounts.remove(sampleRate);
			} else {
				*val -= sampleCount;
			}
			
		}
	}
	void subtract(const audio_chunk & c) {
		subtract(c.get_sample_count(), c.get_sample_rate());
	}
	template<typename t_source> duration_counter & operator+=(const t_source & source) {add(source); return *this;}
	template<typename t_source> duration_counter & operator-=(const t_source & source) {subtract(source); return *this;}
	template<typename t_source> duration_counter & operator=(const t_source & source) {reset(); add(source); return *this;}
private:
	double m_offset;
	typedef pfc::map_t<t_uint32, t_uint64> t_map;
	t_map m_sampleCounts;
};

class audio_chunk_partial_ref : public audio_chunk_temp_impl {
public:
	audio_chunk_partial_ref(const audio_chunk & chunk, t_size base, t_size count) : audio_chunk_temp_impl(chunk.get_data() + base * chunk.get_channels(), count, chunk.get_sample_rate(), chunk.get_channels(), chunk.get_channel_config()) {}
};
