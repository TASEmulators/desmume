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

#ifndef _CLIENT_AV_CAPTURE_OBJECT_H_
#define _CLIENT_AV_CAPTURE_OBJECT_H_

#include <stdio.h>
#include <stdint.h>
#include <string>
#include <queue>

#include <rthreads/rthreads.h>
#include <rthreads/rsemaphore.h>

#include "utils/colorspacehandler/colorspacehandler.h"
#include "utils/task.h"
#include "../../SPU.h"

#define VIDEO_STREAM				0
#define AUDIO_STREAM				1
#define MAX_CONVERT_THREADS			32

#define AUDIO_STREAM_BUFFER_SIZE	((DESMUME_SAMPLE_RATE * sizeof(int16_t) * 2) / 30)	// 16-bit samples, 2 channels, 2 frames (need only 1 frame's worth, but have 2 frame's worth for safety)
#define MAX_AVI_FILE_SIZE			(2ULL * 1024ULL * 1024ULL * 1024ULL)				// Max file size should be 2 GB due to the Video for Windows AVI limit.
#define MAX_PENDING_BUFFER_SIZE		(1536ULL * 1024ULL * 1024ULL)						// Max pending buffer size should not exceed 1.5 GB.
#define MAX_PENDING_FRAME_COUNT		180													// Maintain up to 180 frames in memory for current and future file writes. This is equivalent to 3 seconds worth of frames.


enum AVFileTypeContainerID
{
	AVFileTypeContainerID_mp4				= 0x000000,
	AVFileTypeContainerID_avi				= 0x010000,
	AVFileTypeContainerID_mkv				= 0x020000,
	AVFileTypeContainerID_mov				= 0x030000,
	
	AVFileTypeContainerID_m4a				= 0x800000,
	AVFileTypeContainerID_aiff				= 0x810000,
	AVFileTypeContainerID_flac				= 0x820000,
	AVFileTypeContainerID_wav				= 0x830000
};

enum AVFileTypeVideoCodecID
{
	AVFileTypeVideoCodecID_H264				= 0x000000,
	AVFileTypeVideoCodecID_HVEC				= 0x001000,
	AVFileTypeVideoCodecID_HuffYUV			= 0x002000,
	AVFileTypeVideoCodecID_FFV1				= 0x003000,
	AVFileTypeVideoCodecID_ProRes			= 0x004000,
	
	AVFileTypeVideoCodecID_None				= 0x00FF00
};

enum AVFileTypeAudioCodecID
{
	AVFileTypeAudioCodecID_AAC				= 0x000000,
	AVFileTypeAudioCodecID_PCMs16LE			= 0x000010,
	AVFileTypeAudioCodecID_PCMs16BE			= 0x000020,
	AVFileTypeAudioCodecID_ALAC				= 0x000030,
	AVFileTypeAudioCodecID_FLAC				= 0x000040,
	
	AVFileTypeAudioCodecID_None				= 0x0000FF
};

enum AVFileTypeID
{
	AVFileTypeID_mp4_H264_AAC				= (AVFileTypeContainerID_mp4 | AVFileTypeVideoCodecID_H264 | AVFileTypeAudioCodecID_AAC), // dec = 0
	AVFileTypeID_mp4_HVEC_AAC				= (AVFileTypeContainerID_mp4 | AVFileTypeVideoCodecID_HVEC | AVFileTypeAudioCodecID_AAC), // dec = 4096
	AVFileTypeID_avi_HuffYUV_PCMs16LE		= (AVFileTypeContainerID_avi | AVFileTypeVideoCodecID_HuffYUV | AVFileTypeAudioCodecID_PCMs16LE), // dec = 73744
	AVFileTypeID_mkv_FFV1_FLAC				= (AVFileTypeContainerID_mkv | AVFileTypeVideoCodecID_FFV1 | AVFileTypeAudioCodecID_FLAC), // dec = 143424
	AVFileTypeID_mov_H264_ALAC				= (AVFileTypeContainerID_mov | AVFileTypeVideoCodecID_H264 | AVFileTypeAudioCodecID_ALAC), // dec = 196656
	AVFileTypeID_mov_ProRes_ALAC			= (AVFileTypeContainerID_mov | AVFileTypeVideoCodecID_ProRes | AVFileTypeAudioCodecID_ALAC), // dec = 213040
	
	AVFileTypeID_m4a_AAC					= (AVFileTypeContainerID_m4a | AVFileTypeVideoCodecID_None | AVFileTypeAudioCodecID_AAC), // dec = 8453888
	AVFileTypeID_m4a_ALAC					= (AVFileTypeContainerID_m4a | AVFileTypeVideoCodecID_None | AVFileTypeAudioCodecID_ALAC), // dec = 8453936
	AVFileTypeID_aiff_PCMs16BE				= (AVFileTypeContainerID_aiff | AVFileTypeVideoCodecID_None | AVFileTypeAudioCodecID_PCMs16BE), // dec = 8519456
	AVFileTypeID_flac_FLAC					= (AVFileTypeContainerID_flac | AVFileTypeVideoCodecID_None | AVFileTypeAudioCodecID_FLAC), // dec = 8585024
	AVFileTypeID_wav_PCMs16LE				= (AVFileTypeContainerID_wav | AVFileTypeVideoCodecID_None | AVFileTypeAudioCodecID_PCMs16LE), // dec = 8650512
};

enum
{
	AVFILETYPEID_CONTAINERMASK				= 0xFF0000,
	AVFILETYPEID_VIDEOCODECMASK				= 0x00F000,
	AVFILETYPEID_VIDEOCODECOPTMASK			= 0x000F00,
	AVFILETYPEID_AUDIOCODECMASK				= 0x0000F0,
	AVFILETYPEID_AUDIOCODECOPTMASK			= 0x00000F
};

enum FileStreamCloseAction
{
	FSCA_DoNothing				= 0,
	FSCA_PurgeQueue				= 1,
	FSCA_WriteRemainingInQueue	= 2
};

enum ClientAVCaptureError
{
	ClientAVCaptureError_None						= 0,
	ClientAVCaptureError_GenericError				= 1,
	ClientAVCaptureError_FileOpenError				= 1000,
	ClientAVCaptureError_GenericFormatCodecError	= 2000,
	ClientAVCaptureError_InvalidContainerFormat		= 2000,
	ClientAVCaptureError_InvalidVideoCodec			= 2000,
	ClientAVCaptureError_InvalidAudioCodec			= 2000,
	ClientAVCaptureError_VideoStreamError			= 2000,
	ClientAVCaptureError_VideoFrameCreationError	= 2000,
	ClientAVCaptureError_AudioStreamError			= 2000,
	ClientAVCaptureError_AudioFrameCreationError	= 2000,
	ClientAVCaptureError_FrameEncodeError			= 2000,
	ClientAVCaptureError_FrameWriteError			= 2000
};

class ClientAVCaptureObject;

struct VideoConvertParam
{
	ClientAVCaptureObject *captureObj;
	
	const void *src;
	uint8_t *dst;
	
	size_t srcOffset;
	size_t dstOffset;
	
	size_t firstLineIndex;
	size_t lastLineIndex;
	size_t frameWidth;
};
typedef struct VideoConvertParam VideoConvertParam;

struct AVStreamWriteParam
{
	u8 *srcVideo;
	u8 *srcAudio;
	size_t videoBufferSize;
	size_t audioBufferSize;
};
typedef struct AVStreamWriteParam AVStreamWriteParam;

typedef std::queue<AVStreamWriteParam> AVStreamWriteQueue;

class ClientAVCaptureFileStream
{
protected:
	size_t _videoWidth;
	size_t _videoHeight;
	
	std::string _baseFileName;
	std::string _baseFileNameExt;
	size_t _segmentNumber;
	
	AVFileTypeID _fileTypeID;
	AVFileTypeContainerID _fileTypeContainerID;
	AVFileTypeVideoCodecID _fileTypeVideoCodecID;
	AVFileTypeAudioCodecID _fileTypeAudioCodecID;
	
	uint64_t _maxSegmentSize;
	uint64_t _expectedMaxFrameSize;
	uint64_t _writtenBytes;
	size_t _writtenVideoFrameCount;
	size_t _writtenAudioSampleCount;
	
	ssem_t *_semQueue;
	slock_t *_mutexQueue;
	AVStreamWriteQueue _writeQueue;
	
public:
	ClientAVCaptureFileStream();
	virtual ~ClientAVCaptureFileStream();
	
	void InitBaseProperties(const AVFileTypeID fileTypeID, const std::string &fileName,
							const size_t videoWidth, const size_t videoHeight,
							size_t pendingFrameCount);
	
	AVFileTypeVideoCodecID GetVideoCodecID();
	AVFileTypeAudioCodecID GetAudioCodecID();
	
	virtual ClientAVCaptureError Open() = 0;
	virtual void Close(FileStreamCloseAction theAction) = 0;
	virtual bool IsValid();
	
	void QueueAdd(uint8_t *srcVideo, const size_t videoBufferSize, uint8_t *srcAudio, const size_t audioBufferSize);
	void QueueWait();
	size_t GetQueueSize();
	
	virtual ClientAVCaptureError FlushVideo(uint8_t *srcBuffer, const size_t bufferSize);
	virtual ClientAVCaptureError FlushAudio(uint8_t *srcBuffer, const size_t bufferSize);
	virtual ClientAVCaptureError WriteOneFrame(const AVStreamWriteParam &param);
	
	ClientAVCaptureError WriteAllFrames();
};

class ClientAVCaptureObject
{
private:
	void __InstanceInit(size_t videoFrameWidth, size_t videoFrameHeight);
	
protected:
	ClientAVCaptureFileStream *_fs;
	
	bool _isCapturingVideo;
	bool _isCapturingAudio;
	
	size_t _videoFrameWidth;
	size_t _videoFrameHeight;
	size_t _videoFrameSize;
	size_t _audioBlockSize;
	size_t _audioFrameSize;
	
	uint8_t *_pendingVideoBuffer;
	uint8_t *_pendingAudioBuffer;
	size_t *_pendingAudioWriteSize;
	size_t _pendingBufferCount;
	size_t _currentBufferIndex;
	
	size_t _numThreads;
	Task *_fileWriteThread;
	Task *_convertThread[MAX_CONVERT_THREADS];
	VideoConvertParam _convertParam[MAX_CONVERT_THREADS];
	
	slock_t *_mutexCaptureFlags;
	
public:
	ClientAVCaptureObject();
	ClientAVCaptureObject(size_t videoFrameWidth, size_t videoFrameHeight);
	~ClientAVCaptureObject();
	
	ClientAVCaptureFileStream* GetOutputFileStream();
	void SetOutputFileStream(ClientAVCaptureFileStream *fs);
	bool IsFileStreamValid();
	
	bool IsCapturingVideo();
	bool IsCapturingAudio();
	
	void StartFrame();
	void StreamWriteStart();
	void StreamWriteFinish();
	
	void ConvertVideoSlice555Xto888(const VideoConvertParam &param);
	void ConvertVideoSlice888Xto888(const VideoConvertParam &param);
	
	void CaptureVideoFrame(const void *srcVideoFrame, const size_t inFrameWidth, const size_t inFrameHeight, const NDSColorFormat colorFormat);
	void CaptureAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount);
};

#endif // _CLIENT_AV_CAPTURE_OBJECT_H_
