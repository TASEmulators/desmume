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

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

extern "C"
{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/hwcontext.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libavutil/pixdesc.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <libswscale/swscale.h>
}

#include "avout_ffmpeg.h"
#include "SPU.h"

// Native DS frame rate: 33513982 / 560190 ~= 59.8261 Hz.
static const AVRational kVideoTimeBase = { 560190, 33513982 };
static const AVRational kVideoFrameRate = { 33513982, 560190 };

// One second of buffering at 60 fps / DESMUME_SAMPLE_RATE, and a byte cap so a
// large custom GPU resolution cannot queue hundreds of megabytes.
static const size_t kMaxVideoQueue = 60;
static const size_t kMaxVideoQueueBytes = 64 * 1024 * 1024;
static const size_t kMaxAudioQueueSamples = DESMUME_SAMPLE_RATE;

static std::string AvErrorString(int err)
{
	char buffer[AV_ERROR_MAX_STRING_SIZE] = { 0 };
	av_strerror(err, buffer, sizeof(buffer));
	return std::string(buffer);
}

static const char* ContainerDisplayName(const char* name)
{
	if (strcmp(name, "mp4") == 0 || strcmp(name, "mov") == 0)
	{
		return "MP4";
	}
	if (strcmp(name, "matroska") == 0)
	{
		return "Matroska";
	}
	if (strcmp(name, "webm") == 0)
	{
		return "WebM";
	}
	return name;
}

static enum AVPixelFormat CaptureFormatToAvPixelFormat(FFmpegCaptureFormat format)
{
	switch (format)
	{
		case FFmpegCaptureFormat_BGR555LE: return AV_PIX_FMT_BGR555LE;
		case FFmpegCaptureFormat_RGBA8888: return AV_PIX_FMT_RGBA;
		case FFmpegCaptureFormat_BGRA8888: return AV_PIX_FMT_BGRA;
		default: return AV_PIX_FMT_NONE;
	}
}

static int CaptureFormatBytesPerPixel(FFmpegCaptureFormat format)
{
	return (format == FFmpegCaptureFormat_BGR555LE) ? 2 : 4;
}

static int AvPixFmtBytesPerPixel(enum AVPixelFormat format)
{
	const AVPixFmtDescriptor* descriptor = av_pix_fmt_desc_get(format);
	return (descriptor != NULL) ? av_get_bits_per_pixel(descriptor) / 8 : 0;
}

static bool PixFmtIsHardware(enum AVPixelFormat format)
{
	const AVPixFmtDescriptor* desc = av_pix_fmt_desc_get(format);
	return (desc != NULL) && ((desc->flags & AV_PIX_FMT_FLAG_HWACCEL) != 0);
}

static bool PixFmtToHwDeviceType(enum AVPixelFormat format, enum AVHWDeviceType& type)
{
	switch (format)
	{
		case AV_PIX_FMT_VAAPI:
			type = AV_HWDEVICE_TYPE_VAAPI;
			return true;
		case AV_PIX_FMT_QSV:
			type = AV_HWDEVICE_TYPE_QSV;
			return true;
		case AV_PIX_FMT_VULKAN:
			type = AV_HWDEVICE_TYPE_VULKAN;
			return true;
		case AV_PIX_FMT_CUDA:
			type = AV_HWDEVICE_TYPE_CUDA;
			return true;
		default:
			return false;
	}
}

static std::vector<enum AVPixelFormat> SupportedVideoPixelFormats(const AVCodec* codec)
{
	std::vector<enum AVPixelFormat> result;

#if LIBAVCODEC_VERSION_MAJOR >= 62
	const void* configs = NULL;
	int count = 0;
	if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_PIX_FORMAT, 0, &configs, &count) >= 0 && configs != NULL && count > 0)
	{
		const enum AVPixelFormat* formats = (const enum AVPixelFormat*)configs;
		result.assign(formats, formats + count);
	}
#else
	if (codec->pix_fmts != NULL)
	{
		for (const enum AVPixelFormat* format = codec->pix_fmts; *format != AV_PIX_FMT_NONE; format++)
		{
			result.push_back(*format);
		}
	}
#endif

	return result;
}

static std::vector<enum AVSampleFormat> SupportedAudioSampleFormats(const AVCodec* codec)
{
	std::vector<enum AVSampleFormat> result;

#if LIBAVCODEC_VERSION_MAJOR >= 62
	const void* configs = NULL;
	int count = 0;
	if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_FORMAT, 0, &configs, &count) >= 0 && configs != NULL && count > 0)
	{
		const enum AVSampleFormat* formats = (const enum AVSampleFormat*)configs;
		result.assign(formats, formats + count);
	}
#else
	if (codec->sample_fmts != NULL)
	{
		for (const enum AVSampleFormat* format = codec->sample_fmts; *format != AV_SAMPLE_FMT_NONE; format++)
		{
			result.push_back(*format);
		}
	}
#endif

	return result;
}

static std::vector<int> SupportedAudioSampleRates(const AVCodec* codec)
{
	std::vector<int> result;

#if LIBAVCODEC_VERSION_MAJOR >= 62
	const void* configs = NULL;
	int count = 0;
	if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_SAMPLE_RATE, 0, &configs, &count) >= 0 && configs != NULL && count > 0)
	{
		const int* rates = (const int*)configs;
		result.assign(rates, rates + count);
	}
#else
	if (codec->supported_samplerates != NULL)
	{
		for (const int* rate = codec->supported_samplerates; *rate != 0; rate++)
		{
			result.push_back(*rate);
		}
	}
#endif

	return result;
}

static std::vector<AVChannelLayout> SupportedAudioChannelLayouts(const AVCodec* codec)
{
	std::vector<AVChannelLayout> result;

#if LIBAVCODEC_VERSION_MAJOR >= 62
	const void* configs = NULL;
	int count = 0;
	if (avcodec_get_supported_config(NULL, codec, AV_CODEC_CONFIG_CHANNEL_LAYOUT, 0, &configs, &count) >= 0 && configs != NULL && count > 0)
	{
		const AVChannelLayout* layouts = (const AVChannelLayout*)configs;
		result.assign(layouts, layouts + count);
	}
#else
	if (codec->ch_layouts != NULL)
	{
		for (const AVChannelLayout* layout = codec->ch_layouts; layout->nb_channels != 0; layout++)
		{
			result.push_back(*layout);
		}
	}
#endif

	return result;
}

struct VideoFormatChoice
{
	enum AVPixelFormat encoderFormat;
	enum AVPixelFormat softwareFormat;
	bool uploadToHardware;

	VideoFormatChoice()
		: encoderFormat(AV_PIX_FMT_NONE)
		, softwareFormat(AV_PIX_FMT_NONE)
		, uploadToHardware(false)
	{
	}
};

static bool ChooseVideoFormat(const AVCodec* codec, VideoFormatChoice& choice, std::string& error)
{
	static const enum AVPixelFormat preferred[] = {
		AV_PIX_FMT_NV12,
		AV_PIX_FMT_YUV420P,
		AV_PIX_FMT_YUVJ420P,
		AV_PIX_FMT_YUV422P,
		AV_PIX_FMT_YUV444P,
		AV_PIX_FMT_BGR0,
		AV_PIX_FMT_BGRA,
		AV_PIX_FMT_RGB24,
		AV_PIX_FMT_GBRP,
		AV_PIX_FMT_GRAY8
	};

	const std::vector<enum AVPixelFormat> supported = SupportedVideoPixelFormats(codec);

	// An empty list means "no restriction"; NV12 is the best default.
	if (supported.empty())
	{
		choice.encoderFormat = AV_PIX_FMT_NV12;
		choice.softwareFormat = AV_PIX_FMT_NV12;
		return true;
	}

	// Prefer a software format. Many hardware encoders (NVENC, V4L2-M2M)
	// accept software frames and upload internally, which is more portable.
	for (size_t i = 0; i < sizeof(preferred) / sizeof(preferred[0]); i++)
	{
		if (std::find(supported.begin(), supported.end(), preferred[i]) != supported.end())
		{
			choice.encoderFormat = preferred[i];
			choice.softwareFormat = preferred[i];
			return true;
		}
	}

	for (size_t i = 0; i < supported.size(); i++)
	{
		if (!PixFmtIsHardware(supported[i]))
		{
			choice.encoderFormat = supported[i];
			choice.softwareFormat = supported[i];
			return true;
		}
	}

	// No software format at all: the encoder needs a hardware frame.
	for (size_t i = 0; i < supported.size(); i++)
	{
		enum AVHWDeviceType type;
		if (PixFmtToHwDeviceType(supported[i], type))
		{
			choice.encoderFormat = supported[i];
			choice.softwareFormat = AV_PIX_FMT_NV12;
			choice.uploadToHardware = true;
			return true;
		}
	}

	error = "the encoder offers no usable pixel format";
	return false;
}

struct AudioFormatChoice
{
	int sampleRate;
	enum AVSampleFormat sampleFormat;
	AVChannelLayout layout;

	AudioFormatChoice()
		: sampleRate(48000)
		, sampleFormat(AV_SAMPLE_FMT_S16)
	{
		av_channel_layout_default(&layout, 2);
	}

	~AudioFormatChoice()
	{
		av_channel_layout_uninit(&layout);
	}
};

static AudioFormatChoice ChooseAudioFormat(const AVCodec* codec)
{
	static const enum AVSampleFormat preferredFormats[] = {
		AV_SAMPLE_FMT_S16,
		AV_SAMPLE_FMT_FLTP,
		AV_SAMPLE_FMT_FLT,
		AV_SAMPLE_FMT_S32,
		AV_SAMPLE_FMT_S16P,
		AV_SAMPLE_FMT_S32P
	};

	AudioFormatChoice choice;

	const std::vector<enum AVSampleFormat> formats = SupportedAudioSampleFormats(codec);
	if (!formats.empty())
	{
		choice.sampleFormat = formats[0];
		for (size_t i = 0; i < sizeof(preferredFormats) / sizeof(preferredFormats[0]); i++)
		{
			if (std::find(formats.begin(), formats.end(), preferredFormats[i]) != formats.end())
			{
				choice.sampleFormat = preferredFormats[i];
				break;
			}
		}
	}

	const std::vector<int> rates = SupportedAudioSampleRates(codec);
	if (!rates.empty())
	{
		choice.sampleRate = rates[0];
		if (std::find(rates.begin(), rates.end(), 48000) != rates.end())
		{
			choice.sampleRate = 48000;
		}
	}

	const std::vector<AVChannelLayout> layouts = SupportedAudioChannelLayouts(codec);
	if (!layouts.empty())
	{
		bool foundStereo = false;
		for (size_t i = 0; i < layouts.size(); i++)
		{
			if (layouts[i].nb_channels == 2)
			{
				av_channel_layout_copy(&choice.layout, &layouts[i]);
				foundStereo = true;
				break;
			}
		}
		if (!foundStereo)
		{
			av_channel_layout_copy(&choice.layout, &layouts[0]);
		}
	}

	return choice;
}

static std::vector<FFmpegEncoderInfo> EnumerateEncoders(enum AVMediaType type)
{
	std::vector<FFmpegEncoderInfo> result;

	void* opaque = NULL;
	const AVCodec* codec = NULL;
	while ((codec = av_codec_iterate(&opaque)) != NULL)
	{
		if (!av_codec_is_encoder(codec) || codec->type != type)
		{
			continue;
		}

		FFmpegEncoderInfo info;
		info.name = (codec->name != NULL) ? codec->name : "";
		info.longName = (codec->long_name != NULL) ? codec->long_name : "";
		info.hardware = (codec->capabilities & (AV_CODEC_CAP_HARDWARE | AV_CODEC_CAP_HYBRID)) != 0;
		result.push_back(info);
	}

	std::sort(result.begin(), result.end(), [](const FFmpegEncoderInfo& a, const FFmpegEncoderInfo& b)
	{
		if (a.hardware != b.hardware)
		{
			return a.hardware; // hardware first
		}
		return a.name < b.name;
	});

	return result;
}

struct AVOutFFmpeg::Impl
{
	struct VideoJob
	{
		std::vector<uint8_t> pixels;
		int width;
		int height;
		enum AVPixelFormat format;
		int64_t pts;
	};

	struct AudioJob
	{
		std::vector<int16_t> samples;
		int samplesPerChannel;
	};

	// Output configuration.
	int width;
	int height;
	int64_t videoFrameIndex;
	int64_t audioSampleCount;
	int audioFrameSize;

	// FFmpeg state.
	const AVCodec* videoCodec;
	const AVCodec* audioCodec;
	AVCodecContext* videoContext;
	AVCodecContext* audioContext;
	AVFormatContext* formatContext;
	AVStream* videoStream;
	AVStream* audioStream;
	AVBufferRef* hwDevice;
	AVBufferRef* hwFrames;
	SwsContext* swsContext;
	SwrContext* swrContext;
	AVAudioFifo* audioFifo;
	enum AVPixelFormat softwarePixelFormat;
	bool uploadToHardware;
	bool headerWritten;
	std::string outputFilename;
	bool outputFileCreated;

	// Worker thread and bounded capture queues.
	std::thread worker;
	std::mutex mutex;
	std::condition_variable condition;
	std::deque<VideoJob> videoQueue;
	std::deque<AudioJob> audioQueue;
	size_t queuedVideoBytes;
	size_t queuedAudioSamples;
	std::atomic<bool> stopRequested;
	std::atomic<bool> running;
	std::atomic<bool> failed;
	uint64_t droppedVideoFrames;
	uint64_t droppedAudioBlocks;

	std::mutex errorMutex;
	std::string errorText;

	Impl()
		: width(AVOUT_FFMPEG_WIDTH)
		, height(AVOUT_FFMPEG_HEIGHT)
		, videoFrameIndex(0)
		, audioSampleCount(0)
		, audioFrameSize(0)
		, videoCodec(NULL)
		, audioCodec(NULL)
		, videoContext(NULL)
		, audioContext(NULL)
		, formatContext(NULL)
		, videoStream(NULL)
		, audioStream(NULL)
		, hwDevice(NULL)
		, hwFrames(NULL)
		, swsContext(NULL)
		, swrContext(NULL)
		, audioFifo(NULL)
		, softwarePixelFormat(AV_PIX_FMT_NONE)
		, uploadToHardware(false)
		, headerWritten(false)
		, outputFileCreated(false)
		, queuedVideoBytes(0)
		, queuedAudioSamples(0)
		, stopRequested(false)
		, running(false)
		, failed(false)
		, droppedVideoFrames(0)
		, droppedAudioBlocks(0)
	{
	}

	~Impl()
	{
		Finish();
	}

	void StoreError(const std::string& message)
	{
		std::lock_guard<std::mutex> lock(errorMutex);
		errorText = message;
	}

	void SetRuntimeError(const std::string& message)
	{
		StoreError(message);
		failed = true;
	}

	void SetRuntimeAvError(const std::string& what, int errorCode)
	{
		SetRuntimeError(what + ": " + AvErrorString(errorCode));
	}

	std::string LastError()
	{
		std::lock_guard<std::mutex> lock(errorMutex);
		return errorText;
	}

	// Called by the UI after it has consumed (and displayed) a failure, so a
	// watchdog poll does not report the same error on every tick.
	void ClearFailure()
	{
		failed = false;
		StoreError(std::string());
	}

	void ResetState()
	{
		{
			std::lock_guard<std::mutex> lock(mutex);
			videoQueue.clear();
			audioQueue.clear();
			queuedVideoBytes = 0;
			queuedAudioSamples = 0;
		}

		videoFrameIndex = 0;
		audioSampleCount = 0;
		audioFrameSize = 0;
		stopRequested = false;
		failed = false;
		droppedVideoFrames = 0;
		droppedAudioBlocks = 0;
		StoreError(std::string());
	}

	bool Begin(const char* filename, const FFmpegRecordConfig& config)
	{
		if (filename == NULL || filename[0] == '\0')
		{
			StoreError("No output file name was provided.");
			return false;
		}

		if (running)
		{
			StoreError("A recording is already in progress.");
			return false;
		}

		ReleaseResources();
		ResetState();

		// Filtered capture keeps the filter's destination size; unfiltered
		// capture and callers that leave the fields at zero use the native
		// dual-screen size.
		width = (config.width > 0) ? config.width : AVOUT_FFMPEG_WIDTH;
		height = (config.height > 0) ? config.height : AVOUT_FFMPEG_HEIGHT;

		videoCodec = avcodec_find_encoder_by_name(config.videoEncoder.c_str());
		if (videoCodec == NULL || videoCodec->type != AVMEDIA_TYPE_VIDEO)
		{
			return BeginFail("Video encoder not found: " + config.videoEncoder);
		}

		VideoFormatChoice videoFormat;
		std::string formatError;
		if (!ChooseVideoFormat(videoCodec, videoFormat, formatError))
		{
			return BeginFail("Video encoder " + config.videoEncoder + ": " + formatError);
		}

		videoContext = avcodec_alloc_context3(videoCodec);
		if (videoContext == NULL)
		{
			return BeginFail("Unable to allocate the video encoder context.");
		}

		videoContext->width = width;
		videoContext->height = height;
		videoContext->time_base = kVideoTimeBase;
		videoContext->framerate = kVideoFrameRate;
		videoContext->pix_fmt = videoFormat.encoderFormat;
		videoContext->bit_rate = (int64_t)std::max(1, config.videoBitrateKbps) * 1000;
		videoContext->gop_size = 60;
		videoContext->max_b_frames = 0;
		videoContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

		if (videoFormat.uploadToHardware)
		{
			if (!SetUpHardwareFrames(videoFormat, formatError))
			{
				return BeginFail(formatError);
			}
		}

		int result = avcodec_open2(videoContext, videoCodec, NULL);
		if (result < 0)
		{
			return BeginFail("Unable to open video encoder " + config.videoEncoder + ": " + AvErrorString(result));
		}

		swsContext = NULL;
		softwarePixelFormat = videoFormat.softwareFormat;
		uploadToHardware = videoFormat.uploadToHardware;

		if (!config.audioEncoder.empty())
		{
			audioCodec = avcodec_find_encoder_by_name(config.audioEncoder.c_str());
			if (audioCodec == NULL || audioCodec->type != AVMEDIA_TYPE_AUDIO)
			{
				return BeginFail("Audio encoder not found: " + config.audioEncoder);
			}

			AudioFormatChoice audioFormat = ChooseAudioFormat(audioCodec);

			audioContext = avcodec_alloc_context3(audioCodec);
			if (audioContext == NULL)
			{
				return BeginFail("Unable to allocate the audio encoder context.");
			}

			audioContext->sample_rate = audioFormat.sampleRate;
			audioContext->sample_fmt = audioFormat.sampleFormat;
			av_channel_layout_copy(&audioContext->ch_layout, &audioFormat.layout);
			audioContext->bit_rate = (int64_t)std::max(1, config.audioBitrateKbps) * 1000;
			audioContext->time_base = AVRational{ 1, audioFormat.sampleRate };
			audioContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;

			result = avcodec_open2(audioContext, audioCodec, NULL);
			if (result < 0)
			{
				return BeginFail("Unable to open audio encoder " + config.audioEncoder + ": " + AvErrorString(result));
			}

			audioFrameSize = (audioContext->frame_size > 0) ? audioContext->frame_size : 1024;

			AVChannelLayout inputLayout;
			av_channel_layout_default(&inputLayout, 2);
			result = swr_alloc_set_opts2(&swrContext,
			                             &audioContext->ch_layout, audioContext->sample_fmt, audioContext->sample_rate,
			                             &inputLayout, AV_SAMPLE_FMT_S16, DESMUME_SAMPLE_RATE,
			                             0, NULL);
			av_channel_layout_uninit(&inputLayout);
			if (result < 0 || swrContext == NULL)
			{
				return BeginFail("Unable to create the audio resampler: " + AvErrorString(result));
			}

			result = swr_init(swrContext);
			if (result < 0)
			{
				return BeginFail("Unable to initialize the audio resampler: " + AvErrorString(result));
			}

			audioFifo = av_audio_fifo_alloc(audioContext->sample_fmt, audioContext->ch_layout.nb_channels, 1);
			if (audioFifo == NULL)
			{
				return BeginFail("Unable to allocate the audio sample queue.");
			}
		}

		const char* containerName = config.container.empty() ? "matroska" : config.container.c_str();
		result = avformat_alloc_output_context2(&formatContext, NULL, containerName, filename);
		if (formatContext == NULL)
		{
			return BeginFail("Unable to create the " + std::string(containerName) + " muxer.");
		}

		// Reject combinations the muxer refuses before the output file is
		// created. avformat_query_codec() returns 0 for known-bad pairs such
		// as rawvideo + mp4 (and 1 when the muxer accepts the codec); a
		// negative value means "cannot tell", so let the header write decide.
		const char* containerLabel = ContainerDisplayName(formatContext->oformat->name);
		if (avformat_query_codec(formatContext->oformat, videoCodec->id, FF_COMPLIANCE_NORMAL) == 0)
		{
			return BeginFail(std::string("The ") + containerLabel + " container does not support the " +
			                 config.videoEncoder + " video encoder. Choose a different container or encoder.");
		}
		if (audioCodec != NULL &&
		    avformat_query_codec(formatContext->oformat, audioCodec->id, FF_COMPLIANCE_NORMAL) == 0)
		{
			return BeginFail(std::string("The ") + containerLabel + " container does not support the " +
			                 config.audioEncoder + " audio encoder. Choose a different container or encoder.");
		}

		videoStream = avformat_new_stream(formatContext, videoCodec);
		if (videoStream == NULL)
		{
			return BeginFail("Unable to create the video stream.");
		}
		videoStream->time_base = videoContext->time_base;
		videoStream->avg_frame_rate = kVideoFrameRate;
		result = avcodec_parameters_from_context(videoStream->codecpar, videoContext);
		if (result < 0)
		{
			return BeginFail("Unable to copy the video stream parameters: " + AvErrorString(result));
		}

		if (audioContext != NULL)
		{
			audioStream = avformat_new_stream(formatContext, audioCodec);
			if (audioStream == NULL)
			{
				return BeginFail("Unable to create the audio stream.");
			}
			audioStream->time_base = audioContext->time_base;
			result = avcodec_parameters_from_context(audioStream->codecpar, audioContext);
			if (result < 0)
			{
				return BeginFail("Unable to copy the audio stream parameters: " + AvErrorString(result));
			}
		}

		if ((formatContext->oformat->flags & AVFMT_NOFILE) == 0)
		{
			result = avio_open(&formatContext->pb, filename, AVIO_FLAG_WRITE);
			if (result < 0)
			{
				return BeginFail("Unable to open the output file: " + AvErrorString(result));
			}
			outputFilename = filename;
			outputFileCreated = true;
		}

		result = avformat_write_header(formatContext, NULL);
		if (result < 0)
		{
			return BeginFail("Unable to write the output file header: " + AvErrorString(result));
		}
		headerWritten = true;

		running = true;
		try
		{
			worker = std::thread(&Impl::WorkerLoop, this);
		}
		catch (const std::exception& exception)
		{
			running = false;
			return BeginFail(std::string("Unable to start the recording thread: ") + exception.what());
		}

		return true;
	}

	bool BeginFail(const std::string& message)
	{
		StoreError(message);

		// begin() never returns false after recording has started, so any file
		// opened here contains nothing worth keeping. Remove it instead of
		// leaving a 0-byte or header-only artifact behind.
		const bool removeFile = outputFileCreated;
		const std::string path = outputFilename;

		ReleaseResources();
		if (removeFile && !path.empty())
		{
			remove(path.c_str());
		}
		outputFileCreated = false;
		outputFilename.clear();

		return false;
	}

	bool SetUpHardwareFrames(const VideoFormatChoice& choice, std::string& error)
	{
		enum AVHWDeviceType deviceType;
		if (!PixFmtToHwDeviceType(choice.encoderFormat, deviceType))
		{
			error = "Unsupported hardware pixel format.";
			return false;
		}

		const char* deviceName = NULL;
		switch (deviceType)
		{
			case AV_HWDEVICE_TYPE_QSV:
				deviceName = "auto";
				break;
			case AV_HWDEVICE_TYPE_CUDA:
				deviceName = "0";
				break;
			case AV_HWDEVICE_TYPE_VAAPI:
				deviceName = getenv("DESMUME_VAAPI_DEVICE");
				break;
			default:
				break;
		}

		int result = av_hwdevice_ctx_create(&hwDevice, deviceType, deviceName, NULL, 0);
		if (result < 0)
		{
			error = "Unable to create the hardware device: " + AvErrorString(result);
			return false;
		}

		// Different backends accept different software upload formats. Try the
		// common ones before giving up.
		static const enum AVPixelFormat uploadFormats[] = {
			AV_PIX_FMT_NV12,
			AV_PIX_FMT_YUV420P,
			AV_PIX_FMT_YUV444P
		};

		result = AVERROR(EINVAL);
		for (size_t i = 0; i < sizeof(uploadFormats) / sizeof(uploadFormats[0]); i++)
		{
			hwFrames = av_hwframe_ctx_alloc(hwDevice);
			if (hwFrames == NULL)
			{
				continue;
			}

			AVHWFramesContext* frames = (AVHWFramesContext*)hwFrames->data;
			frames->format = choice.encoderFormat;
			frames->sw_format = uploadFormats[i];
			frames->width = width;
			frames->height = height;
			frames->initial_pool_size = 20;

			result = av_hwframe_ctx_init(hwFrames);
			if (result >= 0)
			{
				videoContext->hw_frames_ctx = av_buffer_ref(hwFrames);
				if (videoContext->hw_frames_ctx != NULL)
				{
					return true;
				}
				result = AVERROR(ENOMEM);
			}

			av_buffer_unref(&hwFrames);
		}

		error = "Unable to create hardware frame pool: " + AvErrorString(result);
		return false;
	}

	void Finish()
	{
		running = false;

		{
			std::lock_guard<std::mutex> lock(mutex);
			stopRequested = true;
		}
		condition.notify_all();

		if (worker.joinable())
		{
			worker.join();
		}

		ReleaseResources();
		stopRequested = false;
		// Keep `failed` set: a trailer failure or a runtime error must remain
		// visible to the caller, which clears it with clearFailure() after
		// showing the message. ResetState() clears it for the next recording.
	}

	void ReleaseResources()
	{
		if (formatContext != NULL)
		{
			if (headerWritten)
			{
				const int result = av_write_trailer(formatContext);
				if (result < 0 && LastError().empty())
				{
					SetRuntimeAvError("Unable to finish the output file", result);
				}
			}
			if (formatContext->pb != NULL)
			{
				avio_closep(&formatContext->pb);
			}
			avformat_free_context(formatContext);
			formatContext = NULL;
		}

		if (audioFifo != NULL)
		{
			av_audio_fifo_free(audioFifo);
			audioFifo = NULL;
		}
		if (swrContext != NULL)
		{
			swr_free(&swrContext);
		}
		if (swsContext != NULL)
		{
			sws_freeContext(swsContext);
			swsContext = NULL;
		}
		if (audioContext != NULL)
		{
			avcodec_free_context(&audioContext);
		}
		if (videoContext != NULL)
		{
			avcodec_free_context(&videoContext);
		}
		if (hwFrames != NULL)
		{
			av_buffer_unref(&hwFrames);
		}
		if (hwDevice != NULL)
		{
			av_buffer_unref(&hwDevice);
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			videoQueue.clear();
			audioQueue.clear();
			queuedVideoBytes = 0;
			queuedAudioSamples = 0;
		}

		videoCodec = NULL;
		audioCodec = NULL;
		videoStream = NULL;
		audioStream = NULL;
		softwarePixelFormat = AV_PIX_FMT_NONE;
		uploadToHardware = false;
		headerWritten = false;
		outputFileCreated = false;
		outputFilename.clear();
		audioFrameSize = 0;
	}

	void PushVideo(const void* buffer, int srcWidth, int srcHeight, FFmpegCaptureFormat captureFormat)
	{
		if (!running || failed || buffer == NULL || srcWidth <= 0 || srcHeight <= 0)
		{
			return;
		}

		const int bytesPerPixel = CaptureFormatBytesPerPixel(captureFormat);
		const size_t frameBytes = (size_t)srcWidth * (size_t)srcHeight * (size_t)bytesPerPixel;
		const int64_t pts = videoFrameIndex++;

		{
			std::lock_guard<std::mutex> lock(mutex);
			if (videoQueue.size() >= kMaxVideoQueue ||
			    queuedVideoBytes + frameBytes > kMaxVideoQueueBytes)
			{
				droppedVideoFrames++;
				return;
			}

			VideoJob job;
			job.width = srcWidth;
			job.height = srcHeight;
			job.format = CaptureFormatToAvPixelFormat(captureFormat);
			const uint8_t* bytes = (const uint8_t*)buffer;
			job.pixels.assign(bytes, bytes + frameBytes);
			job.pts = pts;
			videoQueue.push_back(std::move(job));
			queuedVideoBytes += frameBytes;
		}

		condition.notify_one();
	}

	void PushAudio(const void* data, int sampleCount)
	{
		if (!running || failed || audioContext == NULL || data == NULL || sampleCount <= 0)
		{
			return;
		}

		{
			std::lock_guard<std::mutex> lock(mutex);
			if (queuedAudioSamples + (size_t)sampleCount > kMaxAudioQueueSamples)
			{
				droppedAudioBlocks++;
				return;
			}

			const int16_t* samples = (const int16_t*)data;
			AudioJob job;
			job.samplesPerChannel = sampleCount;
			job.samples.assign(samples, samples + (size_t)sampleCount * 2);
			audioQueue.push_back(std::move(job));
			queuedAudioSamples += (size_t)sampleCount;
		}

		condition.notify_one();
	}

	void WorkerLoop()
	{
		for (;;)
		{
			VideoJob videoJob;
			AudioJob audioJob;
			bool haveVideo = false;
			bool haveAudio = false;

			{
				std::unique_lock<std::mutex> lock(mutex);
				condition.wait(lock, [this]
				{
					return stopRequested || failed || !videoQueue.empty() || !audioQueue.empty();
				});

				if (failed)
				{
					break;
				}
				if (stopRequested && videoQueue.empty() && audioQueue.empty())
				{
					break;
				}

				if (!videoQueue.empty())
				{
					videoJob = std::move(videoQueue.front());
					videoQueue.pop_front();
					queuedVideoBytes -= videoJob.pixels.size();
					haveVideo = true;
				}
				if (!audioQueue.empty())
				{
					audioJob = std::move(audioQueue.front());
					audioQueue.pop_front();
					queuedAudioSamples -= (size_t)audioJob.samplesPerChannel;
					haveAudio = true;
				}
			}

			if (haveVideo && !EncodeVideo(videoJob))
			{
				break;
			}
			if (haveAudio && !EncodeAudio(audioJob))
			{
				break;
			}
		}

		if (!failed)
		{
			FlushEncoders();
		}
	}

	void FlushEncoders()
	{
		if (audioContext != NULL)
		{
			FlushResampler();
			DrainAudioFrames(true);
			FlushCodec(audioContext, audioStream, "audio");
		}

		FlushCodec(videoContext, videoStream, "video");
	}

	bool EncodeVideo(const VideoJob& job)
	{
		if (videoContext == NULL)
		{
			return false;
		}

		const int sourceBytesPerPixel = AvPixFmtBytesPerPixel(job.format);
		if (sourceBytesPerPixel <= 0)
		{
			SetRuntimeError("Unsupported video capture format.");
			return false;
		}

		// The capture comes from the frontend's final custom buffer, whose size
		// follows the GPU scaling factor. The encoder always uses the fixed
		// native output size, so reconfigure the scaler when the two differ.
		swsContext = sws_getCachedContext(swsContext,
		                                  job.width, job.height, job.format,
		                                  width, height, softwarePixelFormat,
		                                  SWS_BILINEAR, NULL, NULL, NULL);
		if (swsContext == NULL)
		{
			SetRuntimeError("Unable to create the video converter.");
			return false;
		}

		AVFrame* softwareFrame = av_frame_alloc();
		if (softwareFrame == NULL)
		{
			SetRuntimeError("Unable to allocate a video frame.");
			return false;
		}

		softwareFrame->format = softwarePixelFormat;
		softwareFrame->width = width;
		softwareFrame->height = height;

		int result = av_frame_get_buffer(softwareFrame, 0);
		if (result < 0)
		{
			av_frame_free(&softwareFrame);
			SetRuntimeAvError("Unable to allocate the video frame buffer", result);
			return false;
		}

		const uint8_t* sourceData[4] = { (const uint8_t*)job.pixels.data(), NULL, NULL, NULL };
		const int sourceStride[4] = { job.width * sourceBytesPerPixel, 0, 0, 0 };
		sws_scale(swsContext, sourceData, sourceStride, 0, job.height, softwareFrame->data, softwareFrame->linesize);

		bool success = false;

		if (uploadToHardware)
		{
			AVFrame* hardwareFrame = av_frame_alloc();
			if (hardwareFrame == NULL)
			{
				SetRuntimeError("Unable to allocate a hardware video frame.");
			}
			else
			{
				result = av_hwframe_get_buffer(videoContext->hw_frames_ctx, hardwareFrame, 0);
				if (result < 0)
				{
					SetRuntimeAvError("Unable to allocate a hardware frame buffer", result);
				}
				else
				{
					result = av_hwframe_transfer_data(hardwareFrame, softwareFrame, 0);
					if (result < 0)
					{
						SetRuntimeAvError("Unable to upload the video frame to the GPU", result);
					}
					else
					{
						hardwareFrame->pts = av_rescale_q(job.pts, kVideoTimeBase, videoContext->time_base);
						success = EncodeFrame(videoContext, videoStream, hardwareFrame, "video");
					}
				}
				av_frame_free(&hardwareFrame);
			}
		}
		else
		{
			softwareFrame->pts = av_rescale_q(job.pts, kVideoTimeBase, videoContext->time_base);
			success = EncodeFrame(videoContext, videoStream, softwareFrame, "video");
		}

		av_frame_free(&softwareFrame);
		return success;
	}

	bool EncodeAudio(const AudioJob& job)
	{
		if (swrContext == NULL || audioContext == NULL || audioFifo == NULL)
		{
			return false;
		}

		const uint8_t* inputData[1] = { (const uint8_t*)job.samples.data() };
		const int inputSamples = job.samplesPerChannel;

		int outputCapacity = (int)av_rescale_rnd(
			swr_get_delay(swrContext, DESMUME_SAMPLE_RATE) + inputSamples,
			audioContext->sample_rate, DESMUME_SAMPLE_RATE, AV_ROUND_UP);
		if (outputCapacity <= 0)
		{
			outputCapacity = inputSamples;
		}

		uint8_t* outputData[AV_NUM_DATA_POINTERS] = { NULL };
		int outputLineSize = 0;
		int result = av_samples_alloc(outputData, &outputLineSize,
		                              audioContext->ch_layout.nb_channels, outputCapacity,
		                              audioContext->sample_fmt, 0);
		if (result < 0)
		{
			SetRuntimeAvError("Unable to allocate the resampled audio buffer", result);
			return false;
		}

		result = swr_convert(swrContext, outputData, outputCapacity, inputData, inputSamples);
		if (result < 0)
		{
			av_freep(&outputData[0]);
			SetRuntimeAvError("Unable to resample audio", result);
			return false;
		}

		const int converted = result;
		result = av_audio_fifo_write(audioFifo, (void**)outputData, converted);
		av_freep(&outputData[0]);
		if (result < converted)
		{
			SetRuntimeAvError("Unable to queue resampled audio", result < 0 ? result : AVERROR(ENOMEM));
			return false;
		}

		return DrainAudioFrames(false);
	}

	bool DrainAudioFrames(bool flush)
	{
		if (audioFifo == NULL || audioContext == NULL)
		{
			return true;
		}

		while (av_audio_fifo_size(audioFifo) >= audioFrameSize)
		{
			if (!EncodeOneAudioFrame(audioFrameSize, false))
			{
				return false;
			}
		}

		if (flush)
		{
			const int remaining = av_audio_fifo_size(audioFifo);
			if (remaining > 0)
			{
				return EncodeOneAudioFrame(remaining, true);
			}
		}

		return true;
	}

	bool EncodeOneAudioFrame(int availableSamples, bool padToFrameSize)
	{
		AVFrame* frame = av_frame_alloc();
		if (frame == NULL)
		{
			SetRuntimeError("Unable to allocate an audio frame.");
			return false;
		}

		frame->nb_samples = audioFrameSize;
		frame->format = audioContext->sample_fmt;
		frame->sample_rate = audioContext->sample_rate;
		av_channel_layout_copy(&frame->ch_layout, &audioContext->ch_layout);

		int result = av_frame_get_buffer(frame, 0);
		if (result < 0)
		{
			av_frame_free(&frame);
			SetRuntimeAvError("Unable to allocate the audio frame buffer", result);
			return false;
		}

		const int samplesToRead = std::min(availableSamples, audioFrameSize);
		result = av_audio_fifo_read(audioFifo, (void**)frame->extended_data, samplesToRead);
		if (result < samplesToRead)
		{
			av_frame_free(&frame);
			SetRuntimeAvError("Unable to read the audio sample queue", result < 0 ? result : AVERROR(EIO));
			return false;
		}

		if (padToFrameSize && samplesToRead < audioFrameSize)
		{
			av_samples_set_silence(frame->extended_data, samplesToRead, audioFrameSize - samplesToRead,
			                       audioContext->ch_layout.nb_channels, audioContext->sample_fmt);
		}

		frame->pts = av_rescale_q(audioSampleCount, AVRational{ 1, audioContext->sample_rate }, audioContext->time_base);
		audioSampleCount += audioFrameSize;

		const bool success = EncodeFrame(audioContext, audioStream, frame, "audio");
		av_frame_free(&frame);
		return success;
	}

	void FlushResampler()
	{
		if (swrContext == NULL || audioContext == NULL || audioFifo == NULL)
		{
			return;
		}

		for (;;)
		{
			const int delay = (int)swr_get_delay(swrContext, audioContext->sample_rate);
			if (delay <= 0)
			{
				break;
			}

			uint8_t* outputData[AV_NUM_DATA_POINTERS] = { NULL };
			int outputLineSize = 0;
			int result = av_samples_alloc(outputData, &outputLineSize,
			                              audioContext->ch_layout.nb_channels, delay,
			                              audioContext->sample_fmt, 0);
			if (result < 0)
			{
				SetRuntimeAvError("Unable to allocate the audio flush buffer", result);
				return;
			}

			result = swr_convert(swrContext, outputData, delay, NULL, 0);
			if (result < 0)
			{
				av_freep(&outputData[0]);
				SetRuntimeAvError("Unable to flush the audio resampler", result);
				return;
			}
			if (result == 0)
			{
				av_freep(&outputData[0]);
				break;
			}

			const int converted = result;
			result = av_audio_fifo_write(audioFifo, (void**)outputData, converted);
			av_freep(&outputData[0]);
			if (result < converted)
			{
				SetRuntimeAvError("Unable to queue the remaining audio", result < 0 ? result : AVERROR(ENOMEM));
				return;
			}
			if (converted < delay)
			{
				break;
			}
		}
	}

	bool EncodeFrame(AVCodecContext* context, AVStream* stream, AVFrame* frame, const char* what)
	{
		int result = avcodec_send_frame(context, frame);
		if (result < 0)
		{
			SetRuntimeAvError(std::string("Unable to submit the ") + what + " frame", result);
			return false;
		}

		return ReceivePackets(context, stream, what);
	}

	bool ReceivePackets(AVCodecContext* context, AVStream* stream, const char* what)
	{
		AVPacket* packet = av_packet_alloc();
		if (packet == NULL)
		{
			SetRuntimeError("Unable to allocate a media packet.");
			return false;
		}

		for (;;)
		{
			int result = avcodec_receive_packet(context, packet);
			if (result == AVERROR(EAGAIN) || result == AVERROR_EOF)
			{
				break;
			}
			if (result < 0)
			{
				av_packet_free(&packet);
				SetRuntimeAvError(std::string("Unable to encode the ") + what + " packet", result);
				return false;
			}

			packet->stream_index = stream->index;
			av_packet_rescale_ts(packet, context->time_base, stream->time_base);

			result = av_interleaved_write_frame(formatContext, packet);
			av_packet_unref(packet);
			if (result < 0)
			{
				av_packet_free(&packet);
				SetRuntimeAvError(std::string("Unable to write the ") + what + " packet", result);
				return false;
			}
		}

		av_packet_free(&packet);
		return true;
	}

	void FlushCodec(AVCodecContext* context, AVStream* stream, const char* what)
	{
		if (context == NULL)
		{
			return;
		}

		int result = avcodec_send_frame(context, NULL);
		if (result < 0 && result != AVERROR_EOF)
		{
			SetRuntimeAvError(std::string("Unable to flush the ") + what + " encoder", result);
			return;
		}
		ReceivePackets(context, stream, what);
	}
};

AVOutFFmpeg::AVOutFFmpeg()
	: impl(new Impl())
{
}

AVOutFFmpeg::~AVOutFFmpeg()
{
	delete impl;
	impl = NULL;
}

bool AVOutFFmpeg::begin(const char* filename, const FFmpegRecordConfig& config)
{
	return impl->Begin(filename, config);
}

void AVOutFFmpeg::end()
{
	impl->Finish();
}

bool AVOutFFmpeg::isRecording() const
{
	return impl->running;
}

bool AVOutFFmpeg::hasFailed() const
{
	return impl->failed;
}

void AVOutFFmpeg::clearFailure()
{
	impl->ClearFailure();
}

std::string AVOutFFmpeg::lastError() const
{
	return impl->LastError();
}

void AVOutFFmpeg::updateVideo(const void* buffer, int width, int height, FFmpegCaptureFormat format)
{
	if (buffer != NULL)
	{
		impl->PushVideo(buffer, width, height, format);
	}
}

void AVOutFFmpeg::updateAudio(const void* data, int sampleCount)
{
	impl->PushAudio(data, sampleCount);
}

namespace FFmpegRecorder
{
	std::vector<FFmpegEncoderInfo> videoEncoders()
	{
		return EnumerateEncoders(AVMEDIA_TYPE_VIDEO);
	}

	std::vector<FFmpegEncoderInfo> audioEncoders()
	{
		return EnumerateEncoders(AVMEDIA_TYPE_AUDIO);
	}

	std::string encoderLabel(const FFmpegEncoderInfo& info)
	{
		std::string label = info.name;
		if (!info.longName.empty())
		{
			label += " - ";
			label += info.longName;
		}
		label += info.hardware ? " [Hardware]" : " [Software]";
		return label;
	}
}
