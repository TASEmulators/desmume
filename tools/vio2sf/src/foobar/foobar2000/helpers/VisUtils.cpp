#include "stdafx.h"

namespace VisUtils {
	void PrepareFFTChunk(audio_chunk const & source, audio_chunk & out, double centerOffset) {
		const t_size channels = source.get_channel_count();
		const t_uint32 sampleRate = source.get_sample_rate();
		pfc::dynamic_assert( sampleRate > 0 );
		out.set_channels(channels, source.get_channel_config());
		out.set_sample_rate(sampleRate);
		const t_size inSize = source.get_sample_count();
		const t_size fftSize = MatchFFTSize(inSize);
		out.set_sample_count(fftSize);
		out.set_data_size(fftSize * channels);
		if (fftSize >= inSize) { //rare case with *REALLY* small input
			pfc::memcpy_t( out.get_data(), source.get_data(), inSize * channels );
			pfc::memset_null_t( out.get_data() + inSize * channels, (fftSize - inSize) * channels );
		} else { //inSize > fftSize, we're using a subset of source chunk for the job, pick a subset around centerOffset.
			const double baseOffset = pfc::max_t<double>(0, centerOffset - 0.5 * (double)fftSize / (double)sampleRate);
			const t_size baseSample = pfc::min_t<t_size>( (t_size) audio_math::time_to_samples(baseOffset, sampleRate), inSize - fftSize);
			pfc::memcpy_t( out.get_data(), source.get_data() + baseSample * channels, fftSize * channels);
		}
	}

	bool IsValidFFTSize(t_size p_size) {
		return p_size >= 2 && (p_size & (p_size - 1)) == 0;
	}

	t_size MatchFFTSize(t_size samples) {
		if (samples <= 2) return 2;
		t_size mask = 1;
		while(!IsValidFFTSize(samples)) {
			samples &= ~mask; mask <<= 1;
		}
		return samples;
	}
};
