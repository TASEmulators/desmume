#ifndef _AUDIO_CHUNK_H_
#define _AUDIO_CHUNK_H_

#define audio_sample_size 64

#if audio_sample_size == 32
typedef float audio_sample;
#define audio_sample_asm dword
#elif audio_sample_size == 64
typedef double audio_sample;
#define audio_sample_asm qword
#else
#error wrong audio_sample_size
#endif

#define audio_sample_bytes (audio_sample_size/8)


//channel order same as in windows APIs - for 4ch: L/R/BL/BR, for 5.1: L/R/C/LF/BL/BR

class NOVTABLE audio_chunk
{
protected:
	audio_chunk() {}
	~audio_chunk() {}	
public:
	

	virtual audio_sample * get_data()=0;
	virtual const audio_sample * get_data() const = 0;
	virtual unsigned get_data_size() const = 0;//buffer size in audio_samples; must be at least samples * nch;
	virtual audio_sample * set_data_size(unsigned new_size)=0;
	
	virtual unsigned get_srate() const = 0;
	virtual void set_srate(unsigned val)=0;
	virtual unsigned get_channels() const = 0;
	virtual void set_channels(unsigned val)=0;

	virtual unsigned get_sample_count() const = 0;
	virtual void set_sample_count(unsigned val)=0;

	inline audio_sample * check_data_size(unsigned new_size) {if (new_size > get_data_size()) return set_data_size(new_size); else return get_data();}
	
	inline bool copy_from(const audio_chunk * src)
	{
		return set_data(src->get_data(),src->get_sample_count(),src->get_channels(),src->get_srate());
	}

	inline double get_duration() const
	{
		double rv = 0;
		unsigned srate = get_srate (), samples = get_sample_count();
		if (srate>0 && samples>0) rv = (double)samples/(double)srate;
		return rv;
	}
	
	inline bool is_empty() const {return get_channels()==0 || get_srate()==0 || get_sample_count()==0;}
	
	bool is_valid();

	inline unsigned get_data_length() const {return get_sample_count() * get_channels();}//actual amount of audio_samples in buffer

	inline void reset()
	{
		set_sample_count(0);
		set_srate(0);
		set_channels(0);
	}
	inline void reset_data()
	{
		reset();
		set_data_size(0);
	}
	
	bool set_data(const audio_sample * src,unsigned samples,unsigned nch,unsigned srate);//returns false on failure (eg. memory error)
	

	//helper routines for converting different input data formats
	inline bool set_data_fixedpoint(const void * ptr,unsigned bytes,unsigned srate,unsigned nch,unsigned bps)
	{
		return set_data_fixedpoint_ex(ptr,bytes,srate,nch,bps,(bps==8 ? FLAG_UNSIGNED : FLAG_SIGNED) | flags_autoendian());
	}

	inline bool set_data_fixedpoint_unsigned(const void * ptr,unsigned bytes,unsigned srate,unsigned nch,unsigned bps)
	{
		return set_data_fixedpoint_ex(ptr,bytes,srate,nch,bps,FLAG_UNSIGNED | flags_autoendian());
	}

	inline bool set_data_fixedpoint_signed(const void * ptr,unsigned bytes,unsigned srate,unsigned nch,unsigned bps)
	{
		return set_data_fixedpoint_ex(ptr,bytes,srate,nch,bps,FLAG_SIGNED | flags_autoendian());
	}

	enum
	{
		FLAG_LITTLE_ENDIAN = 1,
		FLAG_BIG_ENDIAN = 2,
		FLAG_SIGNED = 4,
		FLAG_UNSIGNED = 8,
	};

	inline static unsigned flags_autoendian()
	{
		return byte_order_helper::machine_is_big_endian() ? FLAG_BIG_ENDIAN : FLAG_LITTLE_ENDIAN;
	}

	bool set_data_fixedpoint_ex(const void * ptr,unsigned bytes,unsigned srate,unsigned nch,unsigned bps,unsigned flags);//flags - see FLAG_* above

	bool set_data_floatingpoint_ex(const void * ptr,unsigned bytes,unsigned srate,unsigned nch,unsigned bps,unsigned flags);//signed/unsigned flags dont apply

#if audio_sample_size == 64
	bool set_data_32(const float * src,unsigned samples,unsigned nch,unsigned srate);
	inline bool set_data_64(const double * src,unsigned samples,unsigned nch,unsigned srate) {return set_data(src,samples,nch,srate);}
#else
	inline bool set_data_32(const float * src,unsigned samples,unsigned nch,unsigned srate) {return set_data(src,samples,nch,srate);}
	bool set_data_64(const double * src,unsigned samples,unsigned nch,unsigned srate);
#endif

	bool pad_with_silence_ex(unsigned samples,unsigned hint_nch,unsigned hint_srate);
	bool pad_with_silence(unsigned samples);
	bool insert_silence_fromstart(unsigned samples);
	unsigned skip_first_samples(unsigned samples);
};

class audio_chunk_i : public audio_chunk
{
	mem_block_aligned_t<audio_sample> m_data;
	unsigned m_srate,m_nch,m_samples;
public:
	audio_chunk_i() : m_srate(0), m_nch(0), m_samples(0) {}
	audio_chunk_i(const audio_sample * src,unsigned samples,unsigned nch,unsigned srate) : m_srate(0), m_nch(0), m_samples(0)
	{set_data(src,samples,nch,srate);}
	
	virtual audio_sample * get_data() {return m_data;}
	virtual const audio_sample * get_data() const {return m_data;}
	virtual unsigned get_data_size() const {return m_data.get_size();}
	virtual audio_sample * set_data_size(unsigned new_size) {return m_data.set_size(new_size);}
	
	virtual unsigned get_srate() const {return m_srate;}
	virtual void set_srate(unsigned val) {m_srate=val;}
	virtual unsigned get_channels() const {return m_nch;}
	virtual void set_channels(unsigned val) {m_nch = val;}

	virtual unsigned get_sample_count() const {return m_samples;}
	virtual void set_sample_count(unsigned val) {m_samples = val;}

};

struct output_chunk	//used by float->pcm and output
{
	void * data;
	int size;//in bytes
	int srate;
	int nch;//number of interleaved channels
	int bps;//bits-per-sample
};

#endif //_AUDIO_CHUNK_H_