/*
	Copyright (C) 2018 DeSmuME team
 
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

#include <pthread.h>

#include <map>

#import "../cocoa_file.h"
#import "../cocoa_core.h"
#import "../cocoa_GPU.h"
#import "../cocoa_output.h"
#import "../cocoa_globals.h"
#import "MacAVCaptureTool.h"
#import "MacOGLDisplayView.h"

#ifdef ENABLE_APPLE_METAL
#include "MacMetalDisplayView.h"
#endif
/*
extern "C"
{
#include <libavutil/avassert.h>
#include <libavutil/channel_layout.h>
#include <libavutil/opt.h>
#include <libavutil/mathematics.h>
#include <libavutil/timestamp.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}
*/
static std::map<VideoSizeOptionID, VideoSize> _videoSizeMap;
bool FFmpegFileStream::__needFFmpegLibraryInit = true;
/*
static void fill_rgb_image(AVFrame *pict, size_t frameIndex, size_t width, size_t height)
{
	size_t x;
	size_t y;
	size_t i = frameIndex;
	
	for (y = 0; y < height; y++)
	{
		for (x = 0; x < width; x++)
		{
			pict->data[0][y * pict->linesize[0] + ((x * 3) + 0)] = x + y + i * 3;
			pict->data[0][y * pict->linesize[0] + ((x * 3) + 1)] = 128 + y + i * 2;
			pict->data[0][y * pict->linesize[0] + ((x * 3) + 2)] = 64 + x + i * 5;
		}
	}
}

static AVFrame *get_video_frame(OutputStream *ost)
{
	AVCodecContext *c = ost->enc;
	
	// when we pass a frame to the encoder, it may keep a reference to it
	// internally; make sure we do not overwrite it here
	if (av_frame_make_writable(ost->frame) < 0)
	{
		exit(1);
	}
	
	if (c->pix_fmt != AV_PIX_FMT_RGB24)
	{
		fill_rgb_image(ost->tmp_frame, ost->next_pts, c->width, c->height);
		sws_scale(ost->sws_ctx,
				  (const uint8_t * const *)ost->tmp_frame->data, ost->tmp_frame->linesize,
				  0, c->height, ost->frame->data, ost->frame->linesize);
	}
	else
	{
		fill_rgb_image(ost->frame, ost->next_pts, c->width, c->height);
	}
	
	ost->frame->pts = ost->next_pts++;
	
	return ost->frame;
}

FFmpegFileStream::FFmpegFileStream()
{
	memset(&_videoStream, 0, sizeof(OutputStream));
	memset(&_audioStream, 0, sizeof(OutputStream));
	
	_outputFormat = NULL;
	_outputCtx = NULL;
	_audioCodec = NULL;
	_videoCodec = NULL;
}

FFmpegFileStream::~FFmpegFileStream()
{
	this->Close(FSCA_WriteRemainingInQueue);
}

AVFrame* FFmpegFileStream::_InitVideoFrame(const int pixFormat, const int videoWidth, const int videoHeight)
{
	AVFrame *theFrame = av_frame_alloc();
	if (theFrame == NULL)
	{
		fprintf(stderr, "Error allocating an video frame.\n");
		return theFrame;
	}
	
	theFrame->format = (AVPixelFormat)pixFormat;
	theFrame->width  = videoWidth;
	theFrame->height = videoHeight;
	
	int result = av_frame_get_buffer(theFrame, 32);
	if (result < 0)
	{
		fprintf(stderr, "Could not allocate frame data.\n");
		return theFrame;
	}
	
	return theFrame;
}

AVFrame* FFmpegFileStream::_InitAudioFrame(const int sampleFormat, const uint64_t channelLayout, const uint32_t sampleRate, const uint32_t nbSamples)
{
	const AVSampleFormat avSampleFormat = (AVSampleFormat)sampleFormat;
	AVFrame *theFrame = av_frame_alloc();
	if (theFrame == NULL)
	{
		fprintf(stderr, "Error allocating an audio frame.\n");
		return theFrame;
	}
	
	theFrame->format = avSampleFormat;
	theFrame->channel_layout = channelLayout;
	theFrame->sample_rate = sampleRate;
	theFrame->nb_samples = nbSamples;
	theFrame->pts = 0;
	
	if (nbSamples > 0)
	{
		int result = av_frame_get_buffer(theFrame, 0);
		if (result < 0)
		{
			fprintf(stderr, "Error allocating an audio buffer.\n");
			return theFrame;
		}
	}
	
	return theFrame;
}

void FFmpegFileStream::_CloseStream(OutputStream *outStream)
{
	avcodec_free_context(&outStream->enc);
	outStream->enc = NULL;
	
	av_frame_free(&outStream->frame);
	outStream->frame = NULL;
	
	av_frame_free(&outStream->tmp_frame);
	outStream->tmp_frame = NULL;
	
	sws_freeContext(outStream->sws_ctx);
	outStream->sws_ctx = NULL;
	
	swr_free(&outStream->swr_ctx);
	outStream->swr_ctx = NULL;
}

void FFmpegFileStream::_CloseStreams()
{
	// Write the trailer, if any. The trailer must be written before you
	// close the CodecContexts open when you wrote the header; otherwise
	// av_write_trailer() may try to use memory that was freed on
	// av_codec_close().
	av_write_trailer(this->_outputCtx);
	
	// Close each stream.
	if (this->_videoStream.stream != NULL)
	{
		this->_CloseStream(&this->_videoStream);
	}
	
	if (this->_audioStream.stream != NULL)
	{
		this->_CloseStream(&this->_audioStream);
	}
}

ClientAVCaptureError FFmpegFileStream::_SendStreamFrame(OutputStream *outStream)
{
	ClientAVCaptureError error = ClientAVCaptureError_None;
	bool gotValidPacket = false;
	
	int result = 0;
	AVCodecContext *codecCtx = outStream->enc;
	AVPacket thePacket = { 0 };
	
	result = avcodec_send_frame(codecCtx, outStream->frame);
	if (result >= 0)
	{
		av_init_packet(&thePacket);
		thePacket.data = NULL;
		thePacket.size = 0;
		
		result = avcodec_receive_packet(codecCtx, &thePacket);
		if (result >= 0)
		{
			gotValidPacket = true;
		}
		else if ( (result != AVERROR(EAGAIN)) && (result != AVERROR_EOF) )
		{
			fprintf(stderr, "Error encoding frame: %s\n", av_err2str(result));
			error = ClientAVCaptureError_FrameEncodeError;
			return error;
		}
	}
	
	if (gotValidPacket)
	{
		// rescale output packet timestamp values from codec to stream timebase
		av_packet_rescale_ts(&thePacket, codecCtx->time_base, outStream->stream->time_base);
		thePacket.stream_index = outStream->stream->index;
		
		this->_LogPacket(&thePacket);
		
		result = av_interleaved_write_frame(this->_outputCtx, &thePacket);
		if (result < 0)
		{
			fprintf(stderr, "Error while writing frame: %s\n", av_err2str(result));
			error = ClientAVCaptureError_FrameWriteError;
			return error;
		}
		
		av_free(thePacket.data);
		av_packet_unref(&thePacket);
	}
	
	return error;
}

void FFmpegFileStream::_LogPacket(const AVPacket *thePacket)
{
	AVRational *time_base = &this->_outputCtx->streams[thePacket->stream_index]->time_base;
	
	printf("pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d\n",
		   av_ts2str(thePacket->pts), av_ts2timestr(thePacket->pts, time_base),
		   av_ts2str(thePacket->dts), av_ts2timestr(thePacket->dts, time_base),
		   av_ts2str(thePacket->duration), av_ts2timestr(thePacket->duration, time_base),
		   thePacket->stream_index);
}

ClientAVCaptureError FFmpegFileStream::Open()
{
	ClientAVCaptureError error = ClientAVCaptureError_None;
	int result = 0;
	AVDictionary *containerOptions = NULL;
	AVDictionary *videoOptions = NULL;
	AVDictionary *audioOptions = NULL;
	AVPixelFormat videoPixFormat = AV_PIX_FMT_RGB24;
	AVSampleFormat audioSampleFormat = AV_SAMPLE_FMT_S16;
	std::string fullFileName = "";
	
	if (this->_segmentNumber == 0)
	{
		fullFileName = this->_baseFileName + "." + this->_baseFileNameExt;
	}
	else
	{
		char workingFileName[MAX_PATH];
		memset(workingFileName, '\0', sizeof(workingFileName));
		// Tack on a suffix to the base file name in order to generate a new file name.
		sprintf(workingFileName, "%s_part%d", this->_baseFileName.c_str(), (int)(this->_segmentNumber + 1));
		
		fullFileName = std::string(workingFileName) + "." + this->_baseFileNameExt;
	}
	
	// Initialize the library if it hasn't already been initialized.
	if (FFmpegFileStream::__needFFmpegLibraryInit)
	{
		// Initialize libavcodec, and register all codecs and formats.
		avcodec_register_all();
		
		extern AVOutputFormat ff_aiff_muxer;
		extern AVOutputFormat ff_avi_muxer;
		extern AVOutputFormat ff_flac_muxer;
		extern AVOutputFormat ff_ipod_muxer;
		extern AVOutputFormat ff_matroska_muxer;
		extern AVOutputFormat ff_mov_muxer;
		extern AVOutputFormat ff_mp4_muxer;
		extern AVOutputFormat ff_wav_muxer;
		
		av_register_output_format(&ff_aiff_muxer);
		av_register_output_format(&ff_avi_muxer);
		av_register_output_format(&ff_flac_muxer);
		av_register_output_format(&ff_ipod_muxer);
		av_register_output_format(&ff_matroska_muxer);
		av_register_output_format(&ff_mov_muxer);
		av_register_output_format(&ff_mp4_muxer);
		av_register_output_format(&ff_wav_muxer);
		
		FFmpegFileStream::__needFFmpegLibraryInit = false;
	}
	
	// allocate the output media context
	if (this->_fileTypeContainerID == AVFileTypeContainerID_m4a)
	{
		avformat_alloc_output_context2(&this->_outputCtx, NULL, "ipod", fullFileName.c_str());
	}
	else
	{
		avformat_alloc_output_context2(&this->_outputCtx, NULL, NULL, fullFileName.c_str());
	}
	
	if (this->_outputCtx == NULL)
	{
		printf("Could not deduce output format from file extension. Cancelling AV capture.\n");
		error = ClientAVCaptureError_InvalidContainerFormat;
		return error;
	}
	
	this->_outputFormat = this->_outputCtx->oformat;
	this->_outputFormat->subtitle_codec = AV_CODEC_ID_NONE;
	
	switch (this->_fileTypeVideoCodecID)
	{
		case AVFileTypeVideoCodecID_H264:
			this->_outputFormat->video_codec = AV_CODEC_ID_H264;
			this->_videoCodec = avcodec_find_encoder_by_name("libx264rgb");
			videoPixFormat = AV_PIX_FMT_RGB24;
			
			av_dict_set(&videoOptions, "preset", "ultrafast", 0);
			av_dict_set_int(&videoOptions, "crf", 12, 0);
			av_dict_set(&videoOptions, "tune", "zerolatency", 0);
			av_dict_set(&videoOptions, "profile", "high444", 0);
			break;
			
		case AVFileTypeVideoCodecID_HVEC:
			this->_outputFormat->video_codec = AV_CODEC_ID_HEVC;
			this->_videoCodec = avcodec_find_encoder(AV_CODEC_ID_HEVC);
			videoPixFormat = AV_PIX_FMT_YUV444P;
			break;
			
		case AVFileTypeVideoCodecID_HuffYUV:
			this->_outputFormat->video_codec = AV_CODEC_ID_HUFFYUV;
			this->_videoCodec = avcodec_find_encoder(AV_CODEC_ID_HUFFYUV);
			videoPixFormat = AV_PIX_FMT_RGB24;
			break;
			
		case AVFileTypeVideoCodecID_FFV1:
			this->_outputFormat->video_codec = AV_CODEC_ID_FFV1;
			this->_videoCodec = avcodec_find_encoder(AV_CODEC_ID_FFV1);
			videoPixFormat = AV_PIX_FMT_YUV444P10LE;
			break;
			
		case AVFileTypeVideoCodecID_ProRes:
			this->_outputFormat->video_codec = AV_CODEC_ID_PRORES;
			this->_videoCodec = avcodec_find_encoder_by_name("prores_ks");
			videoPixFormat = AV_PIX_FMT_YUV422P10LE;
			break;
			
		default:
			this->_outputFormat->video_codec = AV_CODEC_ID_NONE;
			break;
	}
	
	switch (this->_fileTypeAudioCodecID)
	{
		case AVFileTypeAudioCodecID_AAC:
			this->_outputFormat->audio_codec = AV_CODEC_ID_AAC;
			this->_audioCodec = avcodec_find_encoder(AV_CODEC_ID_AAC);
			audioSampleFormat = AV_SAMPLE_FMT_FLTP;
			
			av_dict_set(&audioOptions, "aac_coder", "fast", 0);
			av_dict_set(&audioOptions, "profile", "aac_low", 0);
			av_dict_set_int(&audioOptions, "b", 320000, 0);
			break;
			
		case AVFileTypeAudioCodecID_PCMs16LE:
			this->_outputFormat->audio_codec = AV_CODEC_ID_PCM_S16LE;
			this->_audioCodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16LE);
			audioSampleFormat = AV_SAMPLE_FMT_S16;
			break;
			
		case AVFileTypeAudioCodecID_PCMs16BE:
			this->_outputFormat->audio_codec = AV_CODEC_ID_PCM_S16BE;
			this->_audioCodec = avcodec_find_encoder(AV_CODEC_ID_PCM_S16BE);
			audioSampleFormat = AV_SAMPLE_FMT_S16;
			break;
			
		case AVFileTypeAudioCodecID_ALAC:
			this->_outputFormat->audio_codec = AV_CODEC_ID_ALAC;
			this->_audioCodec = avcodec_find_encoder(AV_CODEC_ID_ALAC);
			audioSampleFormat = AV_SAMPLE_FMT_S16P;
			break;
			
		case AVFileTypeAudioCodecID_FLAC:
			this->_outputFormat->audio_codec = AV_CODEC_ID_FLAC;
			this->_audioCodec = avcodec_find_encoder(AV_CODEC_ID_FLAC);
			audioSampleFormat = AV_SAMPLE_FMT_S16;
			
			av_dict_set(&audioOptions, "compression_level", "0", 0);
			break;
			
		default:
			this->_outputFormat->audio_codec = AV_CODEC_ID_NONE;
			break;
	}
	
	// Add the audio and video streams using the default format codecs and initialize the codecs.
	if (this->_outputFormat->video_codec != AV_CODEC_ID_NONE)
	{
		if (this->_videoCodec != NULL)
		{
			AVStream *newStream = NULL;
			AVCodecContext *codecCtx = NULL;
			AVFrame *newFrame = NULL;
			AVFrame *newTempFrame = NULL;
			SwsContext *newSWScaler = NULL;
			
			newStream = avformat_new_stream(this->_outputCtx, NULL);
			if (newStream == NULL)
			{
				fprintf(stderr, "Could not allocate stream\n");
				error = ClientAVCaptureError_VideoStreamError;
				return error;
			}
			
			newStream->id = this->_outputCtx->nb_streams - 1;
			codecCtx = avcodec_alloc_context3(this->_videoCodec);
			
			if (codecCtx == NULL)
			{
				fprintf(stderr, "Could not alloc an encoding context\n");
				error = ClientAVCaptureError_InvalidVideoCodec;
				return error;
			}
			
			codecCtx->codec_id = this->_outputFormat->video_codec;
			codecCtx->bit_rate = 1000000;
			
			// Resolution must be a multiple of two.
			codecCtx->width  = this->_videoWidth;
			codecCtx->height = this->_videoHeight;
			
			// timebase: This is the fundamental unit of time (in seconds) in terms
			// of which frame timestamps are represented. For fixed-fps content,
			// timebase should be 1/framerate and timestamp increments should be
			// identical to 1.
			newStream->time_base = (AVRational){ 10000, 598261 };
			codecCtx->time_base = newStream->time_base;
			
			codecCtx->gop_size = 12; // emit one intra frame every twelve frames at most
			codecCtx->pix_fmt  = videoPixFormat;
			
			switch (codecCtx->codec_id)
			{
				case AV_CODEC_ID_H264:
					codecCtx->thread_count = 1;
					break;
					
				case AV_CODEC_ID_MPEG1VIDEO:
					// Needed to avoid using macroblocks in which some coeffs overflow.
					// This does not happen with normal video, it just happens here as
					// the motion of the chroma plane does not match the luma plane.
					codecCtx->mb_decision = FF_MB_DECISION_RD;
					break;
					
				case AV_CODEC_ID_MPEG2VIDEO:
					codecCtx->max_b_frames = 2; // just for testing, we also add B-frames
					break;
					
				default:
					break;
			}
			
			// Some formats want stream headers to be separate.
			if (this->_outputCtx->oformat->flags & AVFMT_GLOBALHEADER)
			{
				codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}
			
			// open the codec
			result = avcodec_open2(codecCtx, this->_videoCodec, &videoOptions);
			av_dict_free(&videoOptions);
			videoOptions = NULL;
			
			if (result < 0)
			{
				fprintf(stderr, "Could not open video codec: %s\n", av_err2str(result));
				avcodec_free_context(&codecCtx);
				error = ClientAVCaptureError_InvalidVideoCodec;
				return error;
			}
			
			newFrame = this->_InitVideoFrame(codecCtx->pix_fmt, codecCtx->width, codecCtx->height);
			if (newFrame == NULL)
			{
				fprintf(stderr, "Could not allocate video frame\n");
				avcodec_free_context(&codecCtx);
				error = ClientAVCaptureError_VideoFrameCreationError;
				return error;
			}
			else
			{
				if (codecCtx->pix_fmt != AV_PIX_FMT_RGB24)
				{
					newTempFrame = this->_InitVideoFrame(AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height);
					if (newTempFrame == NULL)
					{
						fprintf(stderr, "Could not allocate temporary picture\n");
						av_frame_free(&newFrame);
						avcodec_free_context(&codecCtx);
						error = ClientAVCaptureError_VideoFrameCreationError;
						return error;
					}
					
					newSWScaler = sws_getContext(codecCtx->width, codecCtx->height,
												 AV_PIX_FMT_RGB24,
												 codecCtx->width, codecCtx->height,
												 codecCtx->pix_fmt,
												 SWS_BICUBIC, NULL, NULL, NULL);
					
					if (newSWScaler == NULL)
					{
						fprintf(stderr, "Could not initialize the conversion context\n");
						av_frame_free(&newFrame);
						av_frame_free(&newTempFrame);
						avcodec_free_context(&codecCtx);
						error = ClientAVCaptureError_VideoFrameCreationError;
						return error;
					}
				}
			}
			
			// copy the stream parameters to the muxer
			result = avcodec_parameters_from_context(newStream->codecpar, codecCtx);
			if (result < 0)
			{
				fprintf(stderr, "Could not copy the stream parameters\n");
				av_frame_free(&newFrame);
				av_frame_free(&newTempFrame);
				avcodec_free_context(&codecCtx);
				error = ClientAVCaptureError_VideoStreamError;
				return error;
			}
			
			this->_videoStream.stream = newStream;
			this->_videoStream.enc = codecCtx;
			this->_videoStream.frame = newFrame;
			this->_videoStream.tmp_frame = newTempFrame;
			this->_videoStream.sws_ctx = newSWScaler;
		}
		else
		{
			fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(this->_outputFormat->video_codec));
			error = ClientAVCaptureError_InvalidVideoCodec;
			return error;
		}
	}
	
	if (this->_outputFormat->audio_codec != AV_CODEC_ID_NONE)
	{
		if (this->_audioCodec != NULL)
		{
			AVStream *newStream = NULL;
			AVCodecContext *codecCtx = NULL;
			AVFrame *newFrame = NULL;
			AVFrame *newTempFrame = NULL;
			SwrContext *newSWResampler = NULL;
			
			newStream = avformat_new_stream(this->_outputCtx, NULL);
			if (newStream == NULL)
			{
				fprintf(stderr, "Could not allocate stream\n");
				error = ClientAVCaptureError_AudioStreamError;
				return error;
			}
			
			newStream->id = this->_outputCtx->nb_streams - 1;
			codecCtx = avcodec_alloc_context3(this->_audioCodec);
			
			if (codecCtx == NULL)
			{
				fprintf(stderr, "Could not alloc an encoding context\n");
				error = ClientAVCaptureError_InvalidAudioCodec;
				return error;
			}
			
			codecCtx->bit_rate     = 320000;
			codecCtx->sample_fmt   = audioSampleFormat;
			codecCtx->sample_rate  = SPU_SAMPLE_RATE;
			
			if (this->_audioCodec->supported_samplerates)
			{
				codecCtx->sample_rate = this->_audioCodec->supported_samplerates[0];
				for (size_t i = 0; this->_audioCodec->supported_samplerates[i]; i++)
				{
					if (this->_audioCodec->supported_samplerates[i] == SPU_SAMPLE_RATE)
					{
						codecCtx->sample_rate = SPU_SAMPLE_RATE;
					}
				}
			}
			
			codecCtx->channels = av_get_channel_layout_nb_channels(codecCtx->channel_layout);
			codecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
			if (this->_audioCodec->channel_layouts)
			{
				codecCtx->channel_layout = this->_audioCodec->channel_layouts[0];
				for (size_t i = 0; this->_audioCodec->channel_layouts[i]; i++)
				{
					if (this->_audioCodec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
					{
						codecCtx->channel_layout = AV_CH_LAYOUT_STEREO;
					}
				}
			}
			
			codecCtx->channels = av_get_channel_layout_nb_channels(codecCtx->channel_layout);
			newStream->time_base = (AVRational){ 1, codecCtx->sample_rate };
			codecCtx->time_base = newStream->time_base;
			
			// Some formats want stream headers to be separate.
			if (this->_outputCtx->oformat->flags & AVFMT_GLOBALHEADER)
			{
				codecCtx->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
			}
			
			// open it
			result = avcodec_open2(codecCtx, this->_audioCodec, &audioOptions);
			av_dict_free(&audioOptions);
			if (result < 0)
			{
				fprintf(stderr, "Could not open audio codec: %s\n", av_err2str(result));
				error = ClientAVCaptureError_InvalidAudioCodec;
				return error;
			}
			
			int nb_samples = 0;
			if (codecCtx->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
			{
				nb_samples = SPU_SAMPLE_RATE / 60;
			}
			else
			{
				nb_samples = codecCtx->frame_size;
			}
			
			newFrame = this->_InitAudioFrame(codecCtx->sample_fmt, codecCtx->channel_layout, codecCtx->sample_rate, nb_samples);
			if (newFrame == NULL)
			{
				fprintf(stderr, "Could not allocate audio frame\n");
				avcodec_free_context(&codecCtx);
				error = ClientAVCaptureError_AudioFrameCreationError;
				return error;
			}
			else
			{
				if (codecCtx->sample_fmt != AV_SAMPLE_FMT_S16)
				{
					newTempFrame = this->_InitAudioFrame(AV_SAMPLE_FMT_S16, codecCtx->channel_layout, codecCtx->sample_rate, nb_samples);
					if (newTempFrame == NULL)
					{
						fprintf(stderr, "Could not allocate audio frame\n");
						av_frame_free(&newFrame);
						avcodec_free_context(&codecCtx);
						error = ClientAVCaptureError_AudioFrameCreationError;
						return error;
					}
					
					// create resampler context
					newSWResampler = swr_alloc();
					if (newSWResampler == NULL)
					{
						fprintf(stderr, "Could not allocate resampler context\n");
						error = ClientAVCaptureError_AudioFrameCreationError;
						return error;
					}
					
					// set options
					av_opt_set_int       (newSWResampler, "in_channel_count",   codecCtx->channels,       0);
					av_opt_set_int       (newSWResampler, "in_sample_rate",     codecCtx->sample_rate,    0);
					av_opt_set_sample_fmt(newSWResampler, "in_sample_fmt",      AV_SAMPLE_FMT_S16,        0);
					av_opt_set_int       (newSWResampler, "out_channel_count",  codecCtx->channels,       0);
					av_opt_set_int       (newSWResampler, "out_sample_rate",    codecCtx->sample_rate,    0);
					av_opt_set_sample_fmt(newSWResampler, "out_sample_fmt",     codecCtx->sample_fmt,     0);
					
					// initialize the resampling context
					result = swr_init(newSWResampler);
					if (result < 0)
					{
						fprintf(stderr, "Failed to initialize the resampling context\n");
						av_frame_free(&newFrame);
						av_frame_free(&newTempFrame);
						avcodec_free_context(&codecCtx);
						error = ClientAVCaptureError_AudioFrameCreationError;
						return error;
					}
				}
			}
			
			// copy the stream parameters to the muxer
			result = avcodec_parameters_from_context(newStream->codecpar, codecCtx);
			if (result < 0)
			{
				fprintf(stderr, "Could not copy the stream parameters\n");
				error = ClientAVCaptureError_AudioStreamError;
				return error;
			}
			
			this->_audioStream.stream = newStream;
			this->_audioStream.enc = codecCtx;
			this->_audioStream.frame = newFrame;
			this->_audioStream.tmp_frame = newTempFrame;
			this->_audioStream.swr_ctx = newSWResampler;
			this->_audioStream.next_pts = 0;
			this->_audioStream.samples_count = 0;
			this->_audioStream.inBufferOffset = 0;
		}
		else
		{
			fprintf(stderr, "Could not find encoder for '%s'\n", avcodec_get_name(this->_outputFormat->audio_codec));
			error = ClientAVCaptureError_InvalidAudioCodec;
			return error;
		}
	}
	
	if (this->_segmentNumber == 0)
	{
		av_dump_format(this->_outputCtx, 0, fullFileName.c_str(), 1);
	}
	
	// open the output file
	result = avio_open(&this->_outputCtx->pb, fullFileName.c_str(), AVIO_FLAG_WRITE);
	if (result < 0)
	{
		fprintf(stderr, "Could not open '%s': %s\n", fullFileName.c_str(), av_err2str(result));
		error = ClientAVCaptureError_FileOpenError;
		return error;
	}
	
	// Write the stream header, if any.
	result = avformat_write_header(this->_outputCtx, &containerOptions);
	if (result < 0)
	{
		fprintf(stderr, "Error occurred when opening output file: %s\n", av_err2str(result));
		error = ClientAVCaptureError_FileOpenError;
		return error;
	}
	
	return error;
}

void FFmpegFileStream::Close(FileStreamCloseAction theAction)
{
	if (!this->IsValid())
	{
		return;
	}
	
	switch (theAction)
	{
		case FSCA_PurgeQueue:
		{
			slock_lock(this->_mutexQueue);
			while (!this->_writeQueue.empty())
			{
				this->_writeQueue.pop();
			}
			slock_unlock(this->_mutexQueue);
			
			const size_t remainingFrameCount = (size_t)ssem_get(this->_semQueue);
			for (size_t i = 0; i < remainingFrameCount; i++)
			{
				ssem_signal(this->_semQueue);
			}
			break;
		}
			
		case FSCA_WriteRemainingInQueue:
		{
			if (this->_outputCtx != NULL)
			{
				/*
				do
				{
					slock_lock(this->_mutexQueue);
					if (this->_writeQueue.empty())
					{
						slock_unlock(this->_mutexQueue);
						break;
					}
					
					const AVStreamWriteParam param = this->_writeQueue.front();
					slock_unlock(this->_mutexQueue);
					
					this->WriteOneFrame(param);
					
					slock_lock(this->_mutexQueue);
					this->_writeQueue.pop();
					slock_unlock(this->_mutexQueue);
					
					ssem_signal(this->_semQueue);
					
				} while (true);
				*
				
				AVPacket thePacket = { 0 };
				int result = 0;
				
				if (this->_videoStream.stream != NULL)
				{
					result = 0;
					
					while (result != AVERROR_EOF)
					{
						av_init_packet(&thePacket);
						thePacket.data = NULL;
						thePacket.size = 0;
						
						result = avcodec_receive_packet(this->_videoStream.enc, &thePacket);
						if (result >= 0)
						{
							// rescale output packet timestamp values from codec to stream timebase
							av_packet_rescale_ts(&thePacket, this->_videoStream.enc->time_base, this->_videoStream.stream->time_base);
							thePacket.stream_index = this->_videoStream.stream->index;
							
							this->_LogPacket(&thePacket);
							
							av_interleaved_write_frame(this->_outputCtx, &thePacket);
							av_free(thePacket.data);
							av_packet_unref(&thePacket);
						}
						else if (result == AVERROR(EAGAIN))
						{
							avcodec_send_frame(this->_videoStream.enc, NULL);
						}
					}
				}
				
				if (this->_audioStream.stream != NULL)
				{
					result = 0;
					
					while (result != AVERROR_EOF)
					{
						av_init_packet(&thePacket);
						thePacket.data = NULL;
						thePacket.size = 0;
						
						result = avcodec_receive_packet(this->_audioStream.enc, &thePacket);
						if (result >= 0)
						{
							// rescale output packet timestamp values from codec to stream timebase
							av_packet_rescale_ts(&thePacket, this->_audioStream.enc->time_base, this->_audioStream.stream->time_base);
							thePacket.stream_index = this->_audioStream.stream->index;
							
							this->_LogPacket(&thePacket);
							
							av_interleaved_write_frame(this->_outputCtx, &thePacket);
							av_free(thePacket.data);
							av_packet_unref(&thePacket);
						}
						else if (result == AVERROR(EAGAIN))
						{
							avcodec_send_frame(this->_audioStream.enc, NULL);
						}
					}
				}
			}
			break;
		}
			
		case FSCA_DoNothing:
		default:
			break;
	}
	
	this->_CloseStreams();
	
	// Close the output file.
	if (!(this->_outputFormat->flags & AVFMT_NOFILE))
	{
		avio_closep(&this->_outputCtx->pb);
	}
	
	// free the stream
	avformat_free_context(this->_outputCtx);
	this->_outputCtx = NULL;
	
	printf("AV recording complete.\n");
}

bool FFmpegFileStream::IsValid()
{
	return (this->_outputCtx != NULL);
}

ClientAVCaptureError FFmpegFileStream::FlushVideo(uint8_t *srcBuffer, const size_t bufferSize)
{
	ClientAVCaptureError error = ClientAVCaptureError_None;
	
	int result = 0;
	AVCodecContext *codecCtx = this->_videoStream.enc;
	
	// when we pass a frame to the encoder, it may keep a reference to it internally;
	// make sure we do not overwrite it here
	result = av_frame_make_writable(this->_videoStream.frame);
	if (result < 0)
	{
		error = ClientAVCaptureError_VideoStreamError;
		return error;
	}
	
	if (codecCtx->pix_fmt != AV_PIX_FMT_RGB24)
	{
		fill_rgb_image(this->_videoStream.tmp_frame, this->_videoStream.next_pts, codecCtx->width, codecCtx->height);
		sws_scale(this->_videoStream.sws_ctx,
				  (const uint8_t * const *)this->_videoStream.tmp_frame->data, this->_videoStream.tmp_frame->linesize,
				  0, codecCtx->height, this->_videoStream.frame->data, this->_videoStream.frame->linesize);
	}
	else
	{
		fill_rgb_image(this->_videoStream.frame, this->_videoStream.next_pts, codecCtx->width, codecCtx->height);
	}
	
	this->_videoStream.frame->pts = this->_videoStream.next_pts++;
	
	error = this->_SendStreamFrame(&this->_videoStream);
	if (error)
	{
		return error;
	}
	
	return error;
}

ClientAVCaptureError FFmpegFileStream::FlushAudio(uint8_t *srcBuffer, const size_t bufferSize)
{
	ClientAVCaptureError error = ClientAVCaptureError_None;
	
	int result = 0;
	AVCodecContext *codecCtx = this->_audioStream.enc;
	const size_t sampleSize = sizeof(int16_t) * 2;
	const size_t sampleCount = bufferSize / sampleSize;
	int64_t currentAudioSamples = (this->_audioStream.inBufferOffset / sampleSize) + sampleCount;
	
	// when we pass a frame to the encoder, it may keep a reference to it internally;
	// make sure we do not overwrite it here
	result = av_frame_make_writable(this->_audioStream.frame);
	if (result < 0)
	{
		error = ClientAVCaptureError_AudioStreamError;
		return error;
	}
	
	uint8_t *srcBufferPtr = srcBuffer;
	int64_t copySize = 0;
	size_t copySampleCount = 0;
	
	for (int64_t srcBufferRemaining = bufferSize; srcBufferRemaining > 0;)
	{
		if (currentAudioSamples < this->_audioStream.frame->nb_samples)
		{
			copySize = srcBufferRemaining;
		}
		else
		{
			copySize = srcBufferRemaining - ((currentAudioSamples - this->_audioStream.frame->nb_samples) * sampleSize);
		}
		
		copySampleCount = copySize / sampleSize;
		
		if (codecCtx->sample_fmt != AV_SAMPLE_FMT_S16)
		{
			memcpy(this->_audioStream.tmp_frame->data[0] + this->_audioStream.inBufferOffset, srcBufferPtr, copySize);
			this->_audioStream.tmp_frame->pts += copySampleCount;
		}
		else
		{
			memcpy(this->_audioStream.frame->data[0] + this->_audioStream.inBufferOffset, srcBufferPtr, copySize);
			this->_audioStream.frame->pts += copySampleCount;
		}
		
		if (currentAudioSamples < this->_audioStream.frame->nb_samples)
		{
			this->_audioStream.inBufferOffset += copySize;
			break;
		}
		else
		{
			if (codecCtx->sample_fmt != AV_SAMPLE_FMT_S16)
			{
				// convert samples from native format to destination codec format, using the resampler
				// compute destination number of samples
				int dst_nb_samples = av_rescale_rnd(swr_get_delay(this->_audioStream.swr_ctx, codecCtx->sample_rate) + this->_audioStream.frame->nb_samples,
													codecCtx->sample_rate, codecCtx->sample_rate, AV_ROUND_UP);
				av_assert0(dst_nb_samples == this->_audioStream.frame->nb_samples);
				
				// convert to destination format
				swr_convert(this->_audioStream.swr_ctx,
							this->_audioStream.frame->data, dst_nb_samples,
							(const uint8_t **)this->_audioStream.tmp_frame->data, this->_audioStream.frame->nb_samples);
				
				this->_audioStream.frame->pts = this->_audioStream.next_pts;
				this->_audioStream.samples_count += dst_nb_samples;
				this->_audioStream.next_pts = av_rescale_q(this->_audioStream.samples_count, (AVRational){1, codecCtx->sample_rate}, codecCtx->time_base);
			}
			else
			{
				this->_audioStream.next_pts += this->_audioStream.frame->nb_samples;
				this->_audioStream.samples_count += this->_audioStream.frame->nb_samples;
			}
			
			error = this->_SendStreamFrame(&this->_audioStream);
			if (error)
			{
				return error;
			}
			
			srcBufferRemaining -= copySize;
			srcBufferPtr += copySize;
			currentAudioSamples = srcBufferRemaining / sampleSize;
			this->_audioStream.inBufferOffset = 0;
		}
	}
	
	return error;
}

ClientAVCaptureError FFmpegFileStream::WriteOneFrame(const AVStreamWriteParam &param)
{
	ClientAVCaptureError error = ClientAVCaptureError_None;
	
	error = this->FlushVideo(param.srcVideo, param.videoBufferSize);
	if (error)
	{
		this->Close(FSCA_PurgeQueue);
		return error;
	}
	
	error = this->FlushAudio(param.srcAudio, param.audioBufferSize);
	if (error)
	{
		this->Close(FSCA_PurgeQueue);
		return error;
	}
	
	// Create a new segment if the next written frame would exceed the maximum file size.
	const size_t futureFrameSize = this->_writtenBytes + this->_expectedMaxFrameSize;
	if (futureFrameSize >= this->_maxSegmentSize)
	{
		AVFormatContext *oldContext = this->_outputCtx;
		
		this->_CloseStreams();
		this->_segmentNumber++;
		
		error = this->Open();
		
		// Close the output file.
		if (!(oldContext->flags & AVFMT_NOFILE))
		{
			avio_closep(&oldContext->pb);
		}
		
		avformat_free_context(oldContext);
		
		if (error)
		{
			puts("Error creating new AVI segment.");
			this->Close(FSCA_PurgeQueue);
			return error;
		}
	}
	
	return error;
}
*/
@implementation MacAVCaptureToolDelegate

@synthesize recordButton;
@synthesize execControl;
@synthesize cdsCore;
@dynamic videoSizeOption;
@synthesize videoWidth;
@synthesize videoHeight;
@synthesize isUsingCustomSize;
@dynamic isRecording;
@synthesize isFileStreamClosing;

- (id)init
{
	self = [super init];
	if(self == nil)
	{
		return nil;
	}
	
	_videoSizeMap[VideoSizeOption_UseEmulatorSettings].width = GPU_FRAMEBUFFER_NATIVE_WIDTH;	_videoSizeMap[VideoSizeOption_UseEmulatorSettings].height = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	_videoSizeMap[VideoSizeOption_Custom].width = GPU_FRAMEBUFFER_NATIVE_WIDTH;					_videoSizeMap[VideoSizeOption_Custom].height = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	
	_videoSizeMap[VideoSizeOption_NTSC_480p].width = 720;			_videoSizeMap[VideoSizeOption_NTSC_480p].height = 480;
	_videoSizeMap[VideoSizeOption_PAL_576p].width = 720;			_videoSizeMap[VideoSizeOption_PAL_576p].height = 576;
	_videoSizeMap[VideoSizeOption_nHD].width = 640;					_videoSizeMap[VideoSizeOption_nHD].height = 360;
	_videoSizeMap[VideoSizeOption_qHD].width = 960;					_videoSizeMap[VideoSizeOption_qHD].height = 540;
	_videoSizeMap[VideoSizeOption_HD_720p].width = 1280;			_videoSizeMap[VideoSizeOption_HD_720p].height = 720;
	_videoSizeMap[VideoSizeOption_HDPlus].width = 1600;				_videoSizeMap[VideoSizeOption_HDPlus].height = 900;
	_videoSizeMap[VideoSizeOption_FHD_1080p].width = 1920;			_videoSizeMap[VideoSizeOption_FHD_1080p].height = 1080;
	_videoSizeMap[VideoSizeOption_QHD].width = 2560;				_videoSizeMap[VideoSizeOption_QHD].height = 1440;
	_videoSizeMap[VideoSizeOption_4K_UHD].width = 3840;				_videoSizeMap[VideoSizeOption_4K_UHD].height = 2160;
	
	_videoSizeMap[VideoSizeOption_QVGA].width = 320;				_videoSizeMap[VideoSizeOption_QVGA].height = 240;
	_videoSizeMap[VideoSizeOption_HVGA].width = 480;				_videoSizeMap[VideoSizeOption_HVGA].height = 360;
	_videoSizeMap[VideoSizeOption_VGA].width = 640;					_videoSizeMap[VideoSizeOption_VGA].height = 480;
	_videoSizeMap[VideoSizeOption_SVGA].width = 800;				_videoSizeMap[VideoSizeOption_SVGA].height = 600;
	_videoSizeMap[VideoSizeOption_XGA].width = 1024;				_videoSizeMap[VideoSizeOption_XGA].height = 768;
	_videoSizeMap[VideoSizeOption_SXGA].width = 1280;				_videoSizeMap[VideoSizeOption_SXGA].height = 960;
	_videoSizeMap[VideoSizeOption_UXGA].width = 1600;				_videoSizeMap[VideoSizeOption_UXGA].height = 1200;
	_videoSizeMap[VideoSizeOption_QXGA].width = 2048;				_videoSizeMap[VideoSizeOption_QXGA].height = 1536;
	_videoSizeMap[VideoSizeOption_HXGA].width = 4096;				_videoSizeMap[VideoSizeOption_HXGA].height = 3072;
	
	_captureObject = NULL;
	execControl = NULL;
	cdsCore = NULL;
	_videoCaptureOutput = NULL;
	
	formatID = AVFileTypeID_mp4_H264_AAC;
	videoSizeOption = VideoSizeOption_UseEmulatorSettings;
	videoWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	videoHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	isUsingCustomSize = NO;
	isFileStreamClosing = NO;
	
	[[NSNotificationCenter defaultCenter] addObserver:self
											 selector:@selector(avFileStreamCloseFinish:)
												 name:@"org.desmume.DeSmuME.avFileStreamCloseFinish"
											   object:nil];
	
	return self;
}

- (void) setDisplayMode:(NSInteger)displayModeID
{
	displayMode = displayModeID;
	
	if (videoSizeOption == VideoSizeOption_UseEmulatorSettings)
	{
		[self setVideoSizeUsingEmulatorSettings];
	}
}

- (void) setDisplayLayout:(NSInteger)displayLayoutID
{
	displayLayout = displayLayoutID;
	
	if (videoSizeOption == VideoSizeOption_UseEmulatorSettings)
	{
		[self setVideoSizeUsingEmulatorSettings];
	}
}

- (void) setDisplaySeparation:(NSInteger)displaySeparationInteger
{
	displaySeparation = displaySeparationInteger;
	
	if (videoSizeOption == VideoSizeOption_UseEmulatorSettings)
	{
		[self setVideoSizeUsingEmulatorSettings];
	}
}

- (void) setDisplayRotation:(NSInteger)displayRotationInteger
{
	displayRotation = displayRotationInteger;
	
	if (videoSizeOption == VideoSizeOption_UseEmulatorSettings)
	{
		[self setVideoSizeUsingEmulatorSettings];
	}
}

- (void) setVideoSizeOption:(NSInteger)optionID
{
	videoSizeOption = optionID;
	
	switch (optionID)
	{
		case VideoSizeOption_UseEmulatorSettings:
		{
			[self setIsUsingCustomSize:NO];
			[self setVideoSizeUsingEmulatorSettings];
			break;
		}
			
		case VideoSizeOption_Custom:
			[self setIsUsingCustomSize:YES];
			break;
			
		default:
			[self setIsUsingCustomSize:NO];
			[self setVideoWidth:_videoSizeMap[(VideoSizeOptionID)optionID].width];
			[self setVideoHeight:_videoSizeMap[(VideoSizeOptionID)optionID].height];
			break;
	}
}

- (NSInteger) videoSizeOption
{
	return videoSizeOption;
}

- (void) setIsRecording:(BOOL)theState
{
	// Do nothing. This is here for KVO-compliance only.
}

- (BOOL) isRecording
{
	return (_captureObject != NULL);
}

- (void) setVideoSizeUsingEmulatorSettings
{
	// Do a few sanity checks before proceeding.
	if ([self sharedData] == nil)
	{
		return;
	}
	
	GPUClientFetchObject *fetchObject = [[self sharedData] GPUFetchObject];
	if (fetchObject == NULL)
	{
		return;
	}
	
	const u8 lastBufferIndex = fetchObject->GetLastFetchIndex();
	const NDSDisplayInfo &displayInfo = fetchObject->GetFetchDisplayInfoForBufferIndex(lastBufferIndex);
	
	double normalWidth = 0.0;
	double normalHeight = 0.0;
	ClientDisplayPresenter::CalculateNormalSize((ClientDisplayMode)displayMode, (ClientDisplayLayout)displayLayout, (double)displaySeparation / 100.0, normalWidth, normalHeight);
	
	double viewScale = (double)displayInfo.customWidth / (double)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	double transformedWidth = normalWidth;
	double transformedHeight = normalHeight;
	ClientDisplayPresenter::ConvertNormalToTransformedBounds(viewScale, (double)displayRotation, transformedWidth, transformedHeight);
	
	[self setVideoWidth:(size_t)(transformedWidth + 0.5)];
	[self setVideoHeight:(size_t)(transformedHeight + 0.5)];
}

- (void) readUserDefaults
{
	[self setSaveDirectoryPath:[[NSUserDefaults standardUserDefaults] stringForKey:@"AVCaptureTool_DirectoryPath"]];
	[self setFormatID:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_FileFormat"]];
	[self setUseDeposterize:[[NSUserDefaults standardUserDefaults] boolForKey:@"AVCaptureTool_Deposterize"]];
	[self setOutputFilterID:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_OutputFilter"]];
	[self setPixelScalerID:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_PixelScaler"]];
	[self setDisplayMode:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_DisplayMode"]];
	[self setDisplayLayout:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_DisplayLayout"]];
	[self setDisplayOrder:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_DisplayOrder"]];
	[self setDisplaySeparation:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_DisplaySeparation"]];
	[self setDisplayRotation:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_DisplayRotation"]];
	
	[self setVideoSizeOption:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_VideoSizeOption"]];
	[self setVideoWidth:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_VideoWidth"]];
	[self setVideoHeight:[[NSUserDefaults standardUserDefaults] integerForKey:@"AVCaptureTool_VideoHeight"]];
}

- (void) writeUserDefaults
{
	[[NSUserDefaults standardUserDefaults] setObject:[self saveDirectoryPath] forKey:@"AVCaptureTool_DirectoryPath"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self formatID] forKey:@"AVCaptureTool_FileFormat"];
	[[NSUserDefaults standardUserDefaults] setBool:[self useDeposterize] forKey:@"AVCaptureTool_Deposterize"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self outputFilterID] forKey:@"AVCaptureTool_OutputFilter"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self pixelScalerID] forKey:@"AVCaptureTool_PixelScaler"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayMode] forKey:@"AVCaptureTool_DisplayMode"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayLayout] forKey:@"AVCaptureTool_DisplayLayout"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayOrder] forKey:@"AVCaptureTool_DisplayOrder"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displaySeparation] forKey:@"AVCaptureTool_DisplaySeparation"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self displayRotation] forKey:@"AVCaptureTool_DisplayRotation"];
	
	[[NSUserDefaults standardUserDefaults] setInteger:[self videoSizeOption] forKey:@"AVCaptureTool_VideoSizeOption"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self videoWidth] forKey:@"AVCaptureTool_VideoWidth"];
	[[NSUserDefaults standardUserDefaults] setInteger:[self videoHeight] forKey:@"AVCaptureTool_VideoHeight"];
}

- (void) openFileStream
{
	// One final check for the video size if we're using the emulator settings.
	if (videoSizeOption == VideoSizeOption_UseEmulatorSettings)
	{
		[self setVideoSizeUsingEmulatorSettings];
	}
	
	// Do a few sanity checks before proceeding.
	if ([self sharedData] == nil)
	{
		return;
	}
	
	GPUClientFetchObject *fetchObject = [[self sharedData] GPUFetchObject];
	if (fetchObject == NULL)
	{
		return;
	}
	
	// Check for the existence of the target writable directory. Cancel the recording operation
	// if the directory does not exist or is not writable.
	NSString *savePath = [self saveDirectoryPath];
	if ( (savePath != nil) && ([savePath length] > 0) )
	{
		savePath = [savePath stringByExpandingTildeInPath];
	}
	else
	{
		[self chooseDirectoryPath:self];
		return;
	}
	
	NSFileManager *fileManager = [[NSFileManager alloc] init];
	BOOL isDirectoryFound = [fileManager createDirectoryAtPath:savePath withIntermediateDirectories:YES attributes:nil error:nil];
	
	if (!isDirectoryFound)
	{
		[self chooseDirectoryPath:self];
		[fileManager release];
		return;
	}
	
	[fileManager release];
	
	ClientAVCaptureObject *newCaptureObject = new ClientAVCaptureObject([self videoWidth], [self videoHeight]);
	
	// Set up the rendering properties.
	MacCaptureToolParams param;
	param.refObject					= newCaptureObject;
	param.sharedData				= [self sharedData];
	param.formatID					= [self formatID];
	param.savePath					= std::string([savePath cStringUsingEncoding:NSUTF8StringEncoding]);
	param.romName					= std::string([romName cStringUsingEncoding:NSUTF8StringEncoding]);
	param.useDeposterize			= [self useDeposterize] ? true : false;
	param.outputFilterID			= (OutputFilterTypeID)[self outputFilterID];
	param.pixelScalerID				= (VideoFilterTypeID)[self pixelScalerID];
	
	param.cdpProperty.mode			= (ClientDisplayMode)[self displayMode];
	param.cdpProperty.layout		= (ClientDisplayLayout)[self displayLayout];
	param.cdpProperty.order			= (ClientDisplayOrder)[self displayOrder];
	param.cdpProperty.gapScale		= (double)[self displaySeparation] / 100.0;
	param.cdpProperty.rotation		= (double)[self displayRotation];
	param.cdpProperty.clientWidth	= [self videoWidth];
	param.cdpProperty.clientHeight	= [self videoHeight];
	
	const u8 lastBufferIndex = fetchObject->GetLastFetchIndex();
	const NDSDisplayInfo &displayInfo = fetchObject->GetFetchDisplayInfoForBufferIndex(lastBufferIndex);
	
	if ( (displayInfo.renderedWidth[NDSDisplayID_Main]  == 0) || (displayInfo.renderedHeight[NDSDisplayID_Main]  == 0) ||
	     (displayInfo.renderedWidth[NDSDisplayID_Touch] == 0) || (displayInfo.renderedHeight[NDSDisplayID_Touch] == 0) )
	{
		return;
	}
	
	ClientDisplayPresenter::CalculateNormalSize(param.cdpProperty.mode, param.cdpProperty.layout, param.cdpProperty.gapScale, param.cdpProperty.normalWidth, param.cdpProperty.normalHeight);
	param.cdpProperty.viewScale = ClientDisplayPresenter::GetMaxScalarWithinBounds(param.cdpProperty.normalWidth, param.cdpProperty.normalHeight, param.cdpProperty.clientWidth, param.cdpProperty.clientHeight);
	param.cdpProperty.gapDistance = (double)DS_DISPLAY_UNSCALED_GAP * param.cdpProperty.gapScale;
	
	// Generate the base file name.
	NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
	[dateFormatter setDateFormat:@"yyyyMMdd_HH-mm-ss.SSS "];
	NSString *fileNameNSString = [[dateFormatter stringFromDate:[NSDate date]] stringByAppendingString:[NSString stringWithCString:param.romName.c_str() encoding:NSUTF8StringEncoding]];
	[dateFormatter release];
	
	std::string fileName = param.savePath + "/" + std::string([fileNameNSString cStringUsingEncoding:NSUTF8StringEncoding]);
	
	// Create the output file stream.
	/*
	ClientAVCaptureError error = ClientAVCaptureError_None;
	FFmpegFileStream *ffmpegFS = new FFmpegFileStream;
	
	ffmpegFS->InitBaseProperties((AVFileTypeID)param.formatID, fileName,
								 param.cdpProperty.clientWidth, param.cdpProperty.clientHeight,
								 MAX_PENDING_FRAME_COUNT);
	
	error = ffmpegFS->Open();
	if (error)
	{
		puts("Error starting AV capture file stream.");
		delete newCaptureObject;
	}
	else
	{
		CocoaDSVideoCapture *newVideoCaptureObject = [[CocoaDSVideoCapture alloc] init];
		
		newCaptureObject->SetOutputFileStream(ffmpegFS);
		_captureObject = newCaptureObject;
		
		execControl->SetClientAVCaptureObject(newCaptureObject);
		[cdsCore addOutput:newVideoCaptureObject];
		
		_videoCaptureOutput = newVideoCaptureObject;
	}
	*/
}

- (void) closeFileStream
{
	if (_captureObject == NULL)
	{
		return;
	}
	
	[self setIsFileStreamClosing:YES];
	[cdsCore removeOutput:_videoCaptureOutput];
	_videoCaptureOutput = NULL;
	execControl->SetClientAVCaptureObject(NULL);
	_captureObject->SetOutputFileStream(NULL);
	
	pthread_t fileWriteThread;
	pthread_attr_t fileWriteThreadAttr;
	
	pthread_attr_init(&fileWriteThreadAttr);
	pthread_attr_setdetachstate(&fileWriteThreadAttr, PTHREAD_CREATE_DETACHED);
	pthread_create(&fileWriteThread, &fileWriteThreadAttr, &RunAVCaptureCloseThread, _captureObject);
	pthread_attr_destroy(&fileWriteThreadAttr);
	
	_captureObject = NULL;
}

- (void) avFileStreamCloseFinish:(NSNotification *)aNotification
{
	[self setIsFileStreamClosing:NO];
}

- (IBAction) toggleRecordingState:(id)sender
{
	const BOOL newState = ![self isRecording];
	
	if (newState)
	{
		[self openFileStream];
	}
	else
	{
		[self closeFileStream];
	}
	
	[recordButton setTitle:(newState) ? @"Stop Recording" : @"Start Recording"];
	[self setIsRecording:newState];
}

@end

static void* RunAVCaptureCloseThread(void *arg)
{
	/*
	ClientAVCaptureObject *captureObject = (ClientAVCaptureObject *)arg;
	FFmpegFileStream *ffmpegFS = (FFmpegFileStream *)captureObject->GetOutputFileStream();
	
	captureObject->StreamWriteFinish();
	
	delete ffmpegFS;
	delete captureObject;
	
	[[NSNotificationCenter defaultCenter] postNotificationOnMainThreadName:@"org.desmume.DeSmuME.avFileStreamCloseFinish" object:nil userInfo:nil];
	*/
	return NULL;
}
