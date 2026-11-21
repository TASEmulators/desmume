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

#include "utils/task.h"
#include "../../SPU.h"

#define VIDEO_STREAM				0
#define AUDIO_STREAM				1

#define AUDIO_STREAM_BUFFER_SIZE	((DESMUME_SAMPLE_RATE * sizeof(int16_t) * 2) / 30)	// 16-bit samples, 2 channels, 2 frames (need only 1 frame's worth, but have 2 frame's worth for safety)
#define MAX_AVI_FILE_SIZE			(2ULL * 1024ULL * 1024ULL * 1024ULL)				// Max file size should be 2 GB due to the Video for Windows AVI limit.

// Depth of the write queue.  Frames are CVPixelBufferRefs handed to the
// encoder by reference, so this only bounds latency, not memory: the real
// memory ceiling is the capture pixel buffer pool (a handful of frames).
// Keeping it at 3 seconds is still useful, since a slow encoder should stall
// the display thread rather than make the recording silently drop frames.
#define MAX_PENDING_FRAME_COUNT		180


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
	AVFileTypeID_wav_PCMs16LE				= (AVFileTypeContainerID_wav | AVFileTypeVideoCodecID_None | AVFileTypeAudioCodecID_PCMs16LE) // dec = 8650512
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

struct AVStreamWriteParam
{
	u8 *srcAudio;
	size_t audioBufferSize;
	
	// The finished frame.  When non-NULL this is a retained CVPixelBufferRef
	// that already holds the frame (BGRA, produced on the GPU); NULL means the
	// entry carries audio only (audio-only recording).  Ownership transfers to
	// the file stream, which releases it after encoding.
	void *srcPixelBuffer;
	
	// Presentation index for the GPU path: counts capture ticks, not accepted
	// frames, so a dropped frame leaves a hole in the timeline (the previous
	// frame simply displays longer) instead of shifting video against audio.
	// -1 means "use the stream's own accepted-frame counter".
	int64_t videoFrameIndex;
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
	
	// Queues one frame.  The video payload is an already-finished GPU frame:
	// the stream takes ownership of the (retained) pixel buffer.  Passing NULL
	// queues an audio-only entry.  frameIndex is the capture tick this frame
	// belongs to (see AVStreamWriteParam).
	void QueueAddFrame(void *srcPixelBuffer, int64_t frameIndex, uint8_t *srcAudio, const size_t audioBufferSize);
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
	size_t _audioBlockSize;
	size_t _audioFrameSize;
	
	// Audio-only slots.  Video never touches these: a finished frame lives in
	// its CVPixelBuffer and is handed over by reference.
	uint8_t *_pendingAudioBuffer;
	size_t *_pendingAudioWriteSize;
	size_t _pendingBufferCount;
	size_t _currentBufferIndex;
	
	// Moves the audio collected since the last queued frame into dstBuffer and
	// returns the byte count.  outRemaining optionally receives what is still
	// left in the ring (for the diagnostic log).
	size_t __DrainAudioRing(uint8_t *dstBuffer, size_t *outRemaining = NULL);
	
	// Continuous audio capture ring.  Audio is produced on the emulation core
	// thread at the DS frame rate while video frames are queued from the
	// display thread, so the two are not in phase.  Feeding audio through
	// per-frame slots lost every chunk whose slot had already advanced -- about
	// 20% of the track in practice, which is heard as sped-up audio with
	// crackle.  Audio now lands in this ring and is drained into whichever
	// frame is queued next, so nothing is lost and the timeline stays exact.
	uint8_t *_audioRing;
	size_t _audioRingCapacity;
	size_t _audioRingReadPos;
	size_t _audioRingCount;
	size_t _audioRingDroppedBytes;
	pthread_mutex_t _mutexAudioRing;
	
	Task *_fileWriteThread;
	
	slock_t *_mutexCaptureFlags;
	
	// Guards _fs and its lifetime.  _fs is written by the UI thread
	// (SetOutputFileStream) while the emulation core thread and the display
	// output thread read and use it every frame.  Without this, the core can
	// pass the "stream is valid" check and then find _fs already NULL on the
	// very next access -- which is exactly the crash at _semQueue (+0x88).
	// Readers take the read lock for the whole duration of their use; the
	// writer takes the write lock, so it also waits for in-flight frames.
	pthread_rwlock_t _rwlockStream;
	
private:
	bool __IsFileStreamValidLocked();
	
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
	
	// Queues an audio-only entry.  Only used when the recording has no video
	// track; the emulation core thread calls this every frame in that case
	// (see cocoa_core.mm), because no display frame is consumed.
	void StreamWriteStart();
	void StreamWriteFinish();
	
	// Stops capturing without touching _fs.  This is the only teardown step
	// that is safe to run while the core thread may still hold this object:
	// it just clears the flags under their own lock, so the next frame takes
	// the "not capturing" branch instead of reaching into a closing stream.
	void StopCapture();
	
	// Queues a finished frame that is already in a pixel buffer.  Takes
	// ownership of the (retained) buffer; returns false if it was rejected, in
	// which case the caller must release it itself.
	bool QueuePixelBufferFrame(void *pixelBuffer, int64_t frameIndex);
	
	void CaptureAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount);
};

#endif // _CLIENT_AV_CAPTURE_OBJECT_H_
