//! This class provides abstraction for retrieving visualisation data. Instances of visualisation_stream being created/released serve as an indication for visualisation backend to process currently played audio data or shut down when there are no visualisation clients active.\n
//! Use visualisation_manager::create_stream to instantiate.
class NOVTABLE visualisation_stream : public service_base {
public:
	//! Retrieves absolute playback time since last playback start or seek. You typically pass value retrieved by this function - optionally with offset added - to other visualisation_stream methods.
	virtual bool get_absolute_time(double & p_value) = 0;
	
	//! Retrieves an audio chunk starting at specified offset (see get_absolute_time()), of specified length.
	//! @returns False when requested timestamp is out of available range, true on success.
	virtual bool get_chunk_absolute(audio_chunk & p_chunk,double p_offset,double p_requested_length) = 0;
	//! Retrieves spectrum for audio data at specified offset (see get_absolute_time()), with specified FFT size.
	//! @param p_chunk Receives spectrum data. audio_chunk type is used for consistency (since required functionality is identical to provided by audio_chunk), the data is *not* PCM. Returned sample count is equal to half of FFT size; channels and sample rate are as in audio stream the spectrum was generated from.
	//! @param p_offset Timestamp of spectrum to retrieve. See get_absolute_time().
	//! @param p_fft_size FFT size to use for spectrum generation. Must be a power of 2.	
	//! @returns False when requested timestamp is out of available range, true on success.
	virtual bool get_spectrum_absolute(audio_chunk & p_chunk,double p_offset,unsigned p_fft_size) = 0;
	
	//! Generates fake audio chunk to display when get_chunk_absolute() fails - e.g. shortly after visualisation_stream creation data for currently played audio might not be available yet.
	//! Throws std::exception derivatives on failure.
	virtual void make_fake_chunk_absolute(audio_chunk & p_chunk,double p_offset,double p_requested_length) = 0;
	//! Generates fake spectrum to display when get_spectrum_absolute() fails - e.g. shortly after visualisation_stream creation data for currently played audio might not be available yet.
	//! Throws std::exception derivatives on failure.
	virtual void make_fake_spectrum_absolute(audio_chunk & p_chunk,double p_offset,unsigned p_fft_size) = 0;

	FB2K_MAKE_SERVICE_INTERFACE(visualisation_stream,service_base);
};

//! New in 0.9.5.
class NOVTABLE visualisation_stream_v2 : public visualisation_stream {
public:
	virtual void request_backlog(double p_time) = 0;
	virtual void set_channel_mode(t_uint32 p_mode) = 0;

	enum {
		channel_mode_default = 0,
		channel_mode_mono,
		channel_mode_frontonly,
		channel_mode_backonly,
	};
	
	FB2K_MAKE_SERVICE_INTERFACE(visualisation_stream_v2,visualisation_stream);
};

//! New in 0.9.5.2.
class NOVTABLE visualisation_stream_v3 : public visualisation_stream_v2 {
public:
	virtual void chunk_to_spectrum(audio_chunk const & chunk, audio_chunk & spectrum, double centerOffset) = 0;
	
	FB2K_MAKE_SERVICE_INTERFACE(visualisation_stream_v3,visualisation_stream_v2);
};

//! Entrypoint service for visualisation processing; use this to create visualisation_stream objects that can be used to retrieve properties of currently played audio. \n
//! Implemented by core; do not reimplement.\n
//! Use static_api_ptr_t to access it, e.g. static_api_ptr_t<visualisation_manager>()->create_stream(mystream,0);
class NOVTABLE visualisation_manager : public service_base {
public:
	//! Creates a visualisation_stream object. See visualisation_stream for more info.
	//! @param p_out Receives newly created visualisation_stream instance.
	//! @param p_flags Combination of one or more KStreamFlag* values. Currently only KStreamFlagNewFFT is defined.
	//! It's recommended that you set p_flags to KStreamFlagNewFFT to get the new FFT behavior (better quality and result normalization), the old behavior for null flags is preserved for compatibility with old components that rely on it.
	virtual void create_stream(service_ptr_t<visualisation_stream> & p_out,unsigned p_flags) = 0;

	enum {
		//! New FFT behavior for spectrum-generating methods, available in 0.9.5.2 and newer: output normalized to 0..1, Gauss window used instead of rectangluar (better quality / less aliasing).
		//! It's recommended to always set this flag. The old behavior is preserved for backwards compatibility.
		KStreamFlagNewFFT = 1 << 0,
	};


	//! Wrapper around non-template create_stream(); retrieves one of newer visualisation_stream_* interfaces rather than base visualisation_stream interface. Throws exception_service_extension_not_found() when running too old foobar2000 version for the requested interface.
	template<typename t_streamptr>
	void create_stream(t_streamptr & out, unsigned flags) {
		visualisation_stream::ptr temp; create_stream(temp, flags);
		if (!temp->service_query_t(out)) throw exception_service_extension_not_found();
	}

	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(visualisation_manager);
};
