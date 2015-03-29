//! This class handles conversion of audio data (audio_chunk) to various linear PCM types, with optional dithering.

class NOVTABLE audio_postprocessor : public service_base
{
public:
	//! Processes one chunk of audio data.
	//! @param p_chunk Chunk of audio data to process.
	//! @param p_output Receives output linear signed PCM data.
	//! @param p_out_bps Desired bit depth of output.
	//! @param p_out_bps_physical Desired physical word width of output. Must be either 8, 16, 24 or 32, greater or equal to p_out_bps. This is typically set to same value as p_out_bps.
	//! @param p_dither Indicates whether dithering should be used. Note that dithering is CPU-heavy.
	//! @param p_prescale Value to scale all audio samples by when converting. Set to 1.0 to do nothing.

	virtual void run(const audio_chunk & p_chunk,
		mem_block_container & p_output,
		t_uint32 p_out_bps,
		t_uint32 p_out_bps_physical,
		bool p_dither,
		audio_sample p_prescale
		) = 0;



	FB2K_MAKE_SERVICE_INTERFACE_ENTRYPOINT(audio_postprocessor);
};
