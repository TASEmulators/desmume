namespace VisUtils {
	//! Turns an arbitrary audio_chunk into a valid chunk to run FFT on, with proper sample count etc.
	//! @param centerOffset Time offset (in seconds) inside the source chunk to center the output on, in case the FFT window is smaller than input data.
	void PrepareFFTChunk(audio_chunk const & source, audio_chunk & out, double centerOffset);

	bool IsValidFFTSize(t_size size);
	t_size MatchFFTSize(t_size samples);
};
