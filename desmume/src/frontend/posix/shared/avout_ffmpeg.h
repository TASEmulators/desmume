/*
	Copyright (C) 2026 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef _AVOUT_FFMPEG_H_
#define _AVOUT_FFMPEG_H_

#include <string>
#include <vector>

#include "types.h"

// Native dual-screen buffer size (main screen above touch screen).
#define AVOUT_FFMPEG_WIDTH   256
#define AVOUT_FFMPEG_HEIGHT  384

// Input audio is interleaved s16 stereo at DESMUME_SAMPLE_RATE (SPU.h).
// The recorder converts it to the rate required by the chosen encoder.

struct FFmpegEncoderInfo
{
	std::string name;      // avcodec encoder name, e.g. "h264_vaapi"
	std::string longName;  // codec->long_name
	bool hardware;         // AV_CODEC_CAP_HARDWARE / AV_CODEC_CAP_HYBRID
};

// Pixel layout of the buffer passed to updateVideo().
enum FFmpegCaptureFormat
{
	// 16-bit packed BGR555: R in bits 0-4, G in 5-9, B in 10-14.
	// This is the GPU's native NDSColorFormat_BGR555_Rev layout.
	FFmpegCaptureFormat_BGR555LE,

	// 32-bit packed R,G,B,A bytes in memory. This is what the GPU writes
	// into masterCustomBuffer for the 18-bit and 24-bit color formats.
	FFmpegCaptureFormat_RGBA8888,

	// 32-bit packed B,G,R,A bytes in memory. This is the layout of the
	// video filter source and destination buffers (and the displayed frame).
	FFmpegCaptureFormat_BGRA8888
};

// Arrangement of the two screens in the recorded frame. The captured buffer
// always contains the screens stacked vertically; the recorder rearranges it
// before scaling to the output size.
enum FFmpegScreenLayout
{
	FFmpegScreenLayout_Vertical   = 0, // main above touch (or the reverse when swapped)
	FFmpegScreenLayout_Horizontal = 1, // main beside touch (or the reverse when swapped)
	FFmpegScreenLayout_Single     = 2  // one screen only; swap selects the touch screen
};

struct FFmpegRecordConfig
{
	std::string videoEncoder;    // empty = no video (audio-only recording)
	std::string audioEncoder;    // empty = no audio
	std::string container;       // "mp4" or "matroska"
	int videoBitrateKbps;
	int audioBitrateKbps;

	// Output dimensions. Zero means the native 256x384 dual-screen size.
	// Filtered recording sets these to the filter destination size so the
	// recording keeps the filter's scale (for example 2x = 512x768).
	int width;
	int height;

	// Screen arrangement. screenGap is in source pixels and rotation is one
	// of 0, 90, 180 or 270 degrees (clockwise).
	int layout;
	bool swapScreens;
	int screenGap;
	int rotation;

	FFmpegRecordConfig()
		: videoEncoder("libx264")
		, container("mp4")
		, videoBitrateKbps(8000)
		, audioBitrateKbps(192)
		, width(0)
		, height(0)
		, layout(FFmpegScreenLayout_Vertical)
		, swapScreens(false)
		, screenGap(0)
		, rotation(0)
	{
	}
};

// FFmpeg-based video/audio recorder.
//
// The public header deliberately does not include any FFmpeg header; all
// libav* state lives in Impl so the GTK code only sees this interface.
class AVOutFFmpeg
{
public:
	AVOutFFmpeg();
	~AVOutFFmpeg();

	// Opens the encoders and the output file, then starts the worker thread.
	// Returns false on failure; lastError() describes the problem.
	bool begin(const char* filename, const FFmpegRecordConfig& config);
	void end();

	bool isRecording() const;
	// False for an audio-only recording (config.videoEncoder was empty).
	bool hasVideo() const;
	bool hasFailed() const;
	// Clears the failure state after the UI has shown lastError().
	void clearFailure();
	std::string lastError() const;

	// Called once per emulated frame. `buffer` is a packed image of `height`
	// rows of `width` pixels in the given format. The data is copied before
	// this returns, so the caller may reuse the buffer immediately.
	void updateVideo(const void* buffer, int width, int height, FFmpegCaptureFormat format);

	// Called once per emulated audio block. Data is interleaved s16 stereo;
	// sampleCount is the number of samples per channel.
	void updateAudio(const void* data, int sampleCount);

private:
	struct Impl;
	Impl* impl;

	AVOutFFmpeg(const AVOutFFmpeg&);            // DO NOT IMPLEMENT
	AVOutFFmpeg& operator=(const AVOutFFmpeg&); // DO NOT IMPLEMENT
};

namespace FFmpegRecorder
{
	// Encoders that can actually be used for the given container. Encoders
	// that the muxer rejects, experimental codecs, the pseudo encoders
	// (wrapped_avframe, anull) and hardware backends whose device nodes are
	// absent are omitted. Hardware first, then by name.
	std::vector<FFmpegEncoderInfo> videoEncoders(const std::string& container);
	std::vector<FFmpegEncoderInfo> audioEncoders(const std::string& container);

	// Size of the frame produced by arranging the two stacked screens.
	// sourceWidth/sourceHeight describe the full captured frame (both screens
	// stacked), so one screen is sourceWidth x sourceHeight / 2. swapScreens
	// does not change the size, but it is part of the same geometry.
	void composedSize(int sourceWidth, int sourceHeight,
	                  int layout, bool swapScreens, int screenGap, int rotation,
	                  int* outWidth, int* outHeight);

	// "<name> - <long name> [Hardware|Software]" for combo boxes.
	std::string encoderLabel(const FFmpegEncoderInfo& info);
}

#endif
