/*
	Copyright (C) 2018-2023 DeSmuME team
 
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

#include "ClientAVCaptureObject.h"

#include "../../common.h"
#include "../../NDSSystem.h"
#include "../../GPU.h"

#include <stdio.h>


static void* RunAviFileWrite(void *arg)
{
	ClientAVCaptureFileStream *fs = (ClientAVCaptureFileStream *)arg;
	fs->WriteAllFrames();
	
	return NULL;
}

ClientAVCaptureFileStream::ClientAVCaptureFileStream()
{
	_videoWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_videoHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_fileTypeID = AVFileTypeID_mp4_H264_AAC;
	_fileTypeContainerID = (AVFileTypeContainerID)(_fileTypeID & AVFILETYPEID_CONTAINERMASK);
	_fileTypeVideoCodecID = (AVFileTypeVideoCodecID)(_fileTypeID & AVFILETYPEID_VIDEOCODECMASK);
	_fileTypeAudioCodecID = (AVFileTypeAudioCodecID)(_fileTypeID & AVFILETYPEID_AUDIOCODECMASK);
	
	_baseFileName = "untitled";
	_baseFileNameExt = "mp4";
	_segmentNumber = 0;
	
	_maxSegmentSize = (4ULL * 1024ULL * 1024ULL * 1024ULL * 1024ULL); // Default to 4TB segments.
	_expectedMaxFrameSize = 0;
	_writtenVideoFrameCount = 0;
	_writtenAudioSampleCount = 0;
	_writtenBytes = 0;
	
	_semQueue = NULL;
	_mutexQueue = slock_new();
}

ClientAVCaptureFileStream::~ClientAVCaptureFileStream()
{
	slock_free(this->_mutexQueue);
	ssem_free(this->_semQueue);
}

void ClientAVCaptureFileStream::InitBaseProperties(const AVFileTypeID fileTypeID, const std::string &fileName,
												   const size_t videoWidth, const size_t videoHeight,
												   size_t pendingFrameCount)
{
	// Set up the stream formats that will be used for all AVI segments.
	this->_fileTypeID = fileTypeID;
	this->_fileTypeContainerID = (AVFileTypeContainerID)(fileTypeID & AVFILETYPEID_CONTAINERMASK);
	this->_fileTypeVideoCodecID = (AVFileTypeVideoCodecID)(fileTypeID & AVFILETYPEID_VIDEOCODECMASK);
	this->_fileTypeAudioCodecID = (AVFileTypeAudioCodecID)(fileTypeID & AVFILETYPEID_AUDIOCODECMASK);
	
	switch (this->_fileTypeContainerID)
	{
		case AVFileTypeContainerID_mp4:
			this->_baseFileNameExt = "mp4";
			break;
			
		case AVFileTypeContainerID_avi:
			this->_baseFileNameExt = "avi";
			break;
			
		case AVFileTypeContainerID_mkv:
			this->_baseFileNameExt = "mkv";
			break;
			
		case AVFileTypeContainerID_mov:
			this->_baseFileNameExt = "mov";
			break;
			
		case AVFileTypeContainerID_m4a:
			this->_baseFileNameExt = "m4a";
			break;
			
		case AVFileTypeContainerID_aiff:
			this->_baseFileNameExt = "aiff";
			break;
			
		case AVFileTypeContainerID_flac:
			this->_baseFileNameExt = "flac";
			break;
			
		case AVFileTypeContainerID_wav:
			this->_baseFileNameExt = "wav";
			break;
			
		default:
			break;
	}
	
	this->_baseFileName = fileName;
	this->_videoWidth  = videoWidth;
	this->_videoHeight = videoHeight;
	
	// Only a segment-size estimate: the encoder receives finished BGRA pixel
	// buffers, so this is the worst-case uncompressed size of one frame.
	const size_t videoFrameSize = videoWidth * videoHeight * 4;
	const size_t audioBlockSize = sizeof(int16_t) * 2;
	const size_t audioFrameSize = ((DESMUME_SAMPLE_RATE * audioBlockSize) / 30);
	
	this->_expectedMaxFrameSize = videoFrameSize + audioFrameSize;
	
	_semQueue = ssem_new((int)pendingFrameCount);
}

AVFileTypeVideoCodecID ClientAVCaptureFileStream::GetVideoCodecID()
{
	return this->_fileTypeVideoCodecID;
}

AVFileTypeAudioCodecID ClientAVCaptureFileStream::GetAudioCodecID()
{
	return this->_fileTypeAudioCodecID;
}

bool ClientAVCaptureFileStream::IsValid()
{
	// Implementations need to override this method.
	return false;
}

void ClientAVCaptureFileStream::QueueAddFrame(void *srcPixelBuffer, int64_t frameIndex, uint8_t *srcAudio, const size_t audioBufferSize)
{
	AVStreamWriteParam newParam;
	newParam.srcAudio = srcAudio;
	newParam.audioBufferSize = audioBufferSize;
	newParam.srcPixelBuffer = srcPixelBuffer;
	newParam.videoFrameIndex = frameIndex;
	
	ssem_wait(this->_semQueue);
	
	slock_lock(this->_mutexQueue);
	this->_writeQueue.push(newParam);
	slock_unlock(this->_mutexQueue);
}

void ClientAVCaptureFileStream::QueueWait()
{
	// If the queue if full, the ssem_wait() will force a wait until the current frame is finished writing.
	ssem_wait(this->_semQueue);
	ssem_signal(this->_semQueue);
}

size_t ClientAVCaptureFileStream::GetQueueSize()
{
	slock_lock(this->_mutexQueue);
	const size_t queueSize = this->_writeQueue.size();
	slock_unlock(this->_mutexQueue);
	
	return queueSize;
}

ClientAVCaptureError ClientAVCaptureFileStream::FlushVideo(uint8_t *srcBuffer, const size_t bufferSize)
{
	// Do nothing. This is implementation dependent.
	return ClientAVCaptureError_None;
}

ClientAVCaptureError ClientAVCaptureFileStream::FlushAudio(uint8_t *srcBuffer, const size_t bufferSize)
{
	// Do nothing. This is implementation dependent.
	return ClientAVCaptureError_None;
}

ClientAVCaptureError ClientAVCaptureFileStream::WriteOneFrame(const AVStreamWriteParam &param)
{
	// Do nothing. This is implementation dependent.
	return ClientAVCaptureError_None;
}

ClientAVCaptureError ClientAVCaptureFileStream::WriteAllFrames()
{
	ClientAVCaptureError error = ClientAVCaptureError_None;
	
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
		
		error = this->WriteOneFrame(param);
		
		slock_lock(this->_mutexQueue);
		this->_writeQueue.pop();
		slock_unlock(this->_mutexQueue);
		
		ssem_signal(this->_semQueue);
		
		if (error)
		{
			return error;
		}
		
	} while (true);
	
	return error;
}

ClientAVCaptureObject::ClientAVCaptureObject()
{
	__InstanceInit(0, 0);
}

// Video arrives as finished CVPixelBuffers, so this object owns no video
// storage at all: only the audio slots that have to be paired with each
// queued frame.
ClientAVCaptureObject::ClientAVCaptureObject(size_t videoFrameWidth, size_t videoFrameHeight)
{
	__InstanceInit(videoFrameWidth, videoFrameHeight);
	
	_pendingAudioBuffer = (u8 *)malloc_alignedCacheLine(_audioFrameSize * _pendingBufferCount);
	_pendingAudioWriteSize = (size_t *)calloc(_pendingBufferCount, sizeof(size_t));
}

void ClientAVCaptureObject::__InstanceInit(size_t videoFrameWidth, size_t videoFrameHeight)
{
	_fs = NULL;
	_isCapturingVideo = false;
	_isCapturingAudio = false;
	
	pthread_rwlock_init(&_rwlockStream, NULL);
	
	_mutexCaptureFlags = slock_new();
	
	if ( (videoFrameWidth == 0) || (videoFrameHeight == 0) )
	{
		// Audio-only capture.
		_videoFrameWidth = 0;
		_videoFrameHeight = 0;
	}
	else
	{
		_videoFrameWidth = videoFrameWidth;
		_videoFrameHeight = videoFrameHeight;
	}
	
	_audioBlockSize = sizeof(int16_t) * 2;
	_audioFrameSize = ((DESMUME_SAMPLE_RATE * _audioBlockSize) / 30);
	
	_pendingAudioBuffer = NULL;
	_pendingAudioWriteSize = NULL;
	_currentBufferIndex = 0;
	
	// Two seconds of 16-bit stereo audio at the DS sample rate.  The ring only
	// needs to absorb the phase difference between the core thread (producer)
	// and the display thread (consumer), which is well under one frame, but
	// extra headroom keeps a slow disk from ever truncating the track.
	_audioRingCapacity = (size_t)DESMUME_SAMPLE_RATE * sizeof(int16_t) * 2 * 2;
	_audioRing = (uint8_t *)malloc(_audioRingCapacity);
	_audioRingReadPos = 0;
	_audioRingCount = 0;
	_audioRingDroppedBytes = 0;
	pthread_mutex_init(&_mutexAudioRing, NULL);
	
	// One audio slot per queue entry.  The slot count is bounded by the queue
	// depth, not by frame size: video frames are passed to the write queue as
	// CVPixelBufferRefs and cost no extra memory here.
	_pendingBufferCount = MAX_PENDING_FRAME_COUNT;
	
	_fileWriteThread = new Task();
	_fileWriteThread->start(false);
}

ClientAVCaptureObject::~ClientAVCaptureObject()
{
	this->_fileWriteThread->finish();
	delete this->_fileWriteThread;
	
	free_aligned(this->_pendingAudioBuffer);
	free(this->_pendingAudioWriteSize);
	
	free(this->_audioRing);
	pthread_mutex_destroy(&this->_mutexAudioRing);
	
	pthread_rwlock_destroy(&this->_rwlockStream);
	
	slock_free(this->_mutexCaptureFlags);
}

ClientAVCaptureFileStream* ClientAVCaptureObject::GetOutputFileStream()
{
	return this->_fs;
}

void ClientAVCaptureObject::SetOutputFileStream(ClientAVCaptureFileStream *fs)
{
	// Write lock: this waits until every in-flight StartFrame() /
	// StreamWriteStart() / CaptureAudioFrames() has finished using the old
	// stream.  Only then is _fs replaced, so no reader can observe NULL (or a
	// stream that is about to be closed) halfway through its own use.
	pthread_rwlock_wrlock(&this->_rwlockStream);
	
	if (fs == NULL)
	{
		slock_lock(this->_mutexCaptureFlags);
		this->_isCapturingVideo = false;
		this->_isCapturingAudio = false;
		slock_unlock(this->_mutexCaptureFlags);
	}
	
	this->_fs = fs;
	
	if (fs != NULL)
	{
		slock_lock(this->_mutexCaptureFlags);
		this->_isCapturingVideo = (fs->GetVideoCodecID() != AVFileTypeVideoCodecID_None);
		this->_isCapturingAudio = (fs->GetAudioCodecID() != AVFileTypeAudioCodecID_None);
		slock_unlock(this->_mutexCaptureFlags);
	}
	
	pthread_rwlock_unlock(&this->_rwlockStream);
}

void ClientAVCaptureObject::StopCapture()
{
	slock_lock(this->_mutexCaptureFlags);
	this->_isCapturingVideo = false;
	this->_isCapturingAudio = false;
	slock_unlock(this->_mutexCaptureFlags);
}

// Caller must already hold _rwlockStream (read or write).
bool ClientAVCaptureObject::__IsFileStreamValidLocked()
{
	if (this->_fs == NULL)
	{
		return false;
	}
	
	return this->_fs->IsValid();
}

bool ClientAVCaptureObject::IsFileStreamValid()
{
	pthread_rwlock_rdlock(&this->_rwlockStream);
	const bool isValid = this->__IsFileStreamValidLocked();
	pthread_rwlock_unlock(&this->_rwlockStream);
	
	return isValid;
}

bool ClientAVCaptureObject::IsCapturingVideo()
{
	slock_lock(this->_mutexCaptureFlags);
	const bool isCapturingVideo = this->_isCapturingVideo;
	slock_unlock(this->_mutexCaptureFlags);
	
	return isCapturingVideo;
}

bool ClientAVCaptureObject::IsCapturingAudio()
{
	slock_lock(this->_mutexCaptureFlags);
	const bool isCapturingAudio = this->_isCapturingAudio;
	slock_unlock(this->_mutexCaptureFlags);
	
	return isCapturingAudio;
}

void ClientAVCaptureObject::StartFrame()
{
	// Backpressure only.  _currentBufferIndex used to advance here, on the
	// emulation core thread, while CaptureVideoFrame()/StreamWriteStart() ran
	// on the display thread: whenever the index moved between those two calls
	// the frame was queued from the wrong slot, and the display re-touching a
	// slot that was already in the write queue, which showed up as repeated
	// and out-of-order frames.  The index now advances in StreamWriteStart(),
	// on the same thread that fills the slot, exactly once per queued frame.
	pthread_rwlock_rdlock(&this->_rwlockStream);
	
	if (this->__IsFileStreamValidLocked())
	{
		this->_fs->QueueWait();
	}
	
	pthread_rwlock_unlock(&this->_rwlockStream);
}

// Audio-only path.  The emulation core thread calls this every frame when the
// recording has no video track, because in that case no display frame is
// consumed and nothing else would drain the audio ring.
//
// It never passes a video payload: video can only reach the encoder as a
// finished CVPixelBuffer (see QueuePixelBufferFrame), so there is no raw frame
// pointer here that could be stale, NULL or the wrong size.
void ClientAVCaptureObject::StreamWriteStart()
{
	pthread_rwlock_rdlock(&this->_rwlockStream);
	
	if (!this->__IsFileStreamValidLocked())
	{
		pthread_rwlock_unlock(&this->_rwlockStream);
		
		static unsigned int invalidStreamCount = 0;
		if ( (++invalidStreamCount <= 3) || ((invalidStreamCount % 120) == 0) )
		{
			printf("DeSmuME[AVCapture]: StreamWriteStart DROPPED frame: file stream invalid (count=%u)\n", invalidStreamCount);
			fflush(stdout);
		}
		return;
	}
	
	ClientAVCaptureFileStream * const fs = this->_fs;
	
	const size_t bufferIndex = this->_currentBufferIndex;
	
	size_t audioRingRemaining = 0;
	const size_t audioBytes = this->__DrainAudioRing(this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex),
													 &audioRingRemaining);
	
	this->_pendingAudioWriteSize[bufferIndex] = audioBytes;
	
	const bool isQueueEmpty = (fs->GetQueueSize() == 0);
	
	static unsigned int enqueueCount = 0;
	if ( (++enqueueCount <= 3) || ((enqueueCount % 120) == 0) )
	{
		printf("DeSmuME[AVCapture]: Audio-only enqueue #%u slot=%zu audio=%zu queue=%zu ring=%zu dropped=%zu\n",
			   enqueueCount, bufferIndex, this->_pendingAudioWriteSize[bufferIndex], fs->GetQueueSize(),
			   audioRingRemaining, this->_audioRingDroppedBytes);
		fflush(stdout);
	}
	
	fs->QueueAddFrame(NULL, -1,
					  this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex), this->_pendingAudioWriteSize[bufferIndex]);
	
	// If the queue was initially empty, then we need to wake up the file write
	// thread at this time.
	if (isQueueEmpty)
	{
		this->_fileWriteThread->execute(&RunAviFileWrite, fs);
	}
	
	// Advance only after the slot has been handed to the queue, so the next
	// captured frame always lands in a slot that is not already queued.
	if (this->_pendingBufferCount > 0)
	{
		this->_currentBufferIndex = ((this->_currentBufferIndex + 1) % this->_pendingBufferCount);
	}
	
	pthread_rwlock_unlock(&this->_rwlockStream);
}

// GPU capture path.  The frame is already finished (composited, filtered and
// scaled by the Metal presenter into an IOSurface-backed CVPixelBuffer), so
// there is nothing to convert and nothing to copy: this only pairs the frame
// with the audio that belongs to it and hands it to the write queue.
//
// Takes ownership of pixelBuffer: it is released by the file stream after
// encoding, or by the caller when this returns false.
bool ClientAVCaptureObject::QueuePixelBufferFrame(void *pixelBuffer, int64_t frameIndex)
{
	if ( (pixelBuffer == NULL) || !this->_isCapturingVideo )
	{
		return false;
	}
	
	pthread_rwlock_rdlock(&this->_rwlockStream);
	
	if (!this->__IsFileStreamValidLocked())
	{
		pthread_rwlock_unlock(&this->_rwlockStream);
		
		static unsigned int invalidStreamCount = 0;
		if ( (++invalidStreamCount <= 3) || ((invalidStreamCount % 120) == 0) )
		{
			printf("DeSmuME[AVCapture]: QueuePixelBufferFrame DROPPED frame: file stream invalid (count=%u)\n", invalidStreamCount);
			fflush(stdout);
		}
		return false;
	}
	
	ClientAVCaptureFileStream * const fs = this->_fs;
	const size_t bufferIndex = this->_currentBufferIndex;
	
	size_t audioRingRemaining = 0;
	const size_t audioBytes = this->__DrainAudioRing(this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex),
													 &audioRingRemaining);
	
	this->_pendingAudioWriteSize[bufferIndex] = audioBytes;
	
	const bool isQueueEmpty = (fs->GetQueueSize() == 0);
	
	static unsigned int gpuEnqueueCount = 0;
	if ( (++gpuEnqueueCount <= 3) || ((gpuEnqueueCount % 120) == 0) )
	{
		printf("DeSmuME[AVCapture]: GPU enqueue #%u slot=%zu audio=%zu queue=%zu ring=%zu dropped=%zu\n",
			   gpuEnqueueCount, bufferIndex, audioBytes,
			   fs->GetQueueSize(), audioRingRemaining, this->_audioRingDroppedBytes);
		fflush(stdout);
	}
	
	fs->QueueAddFrame(pixelBuffer,
					  frameIndex,
					  this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex),
					  this->_pendingAudioWriteSize[bufferIndex]);
	
	if (isQueueEmpty)
	{
		this->_fileWriteThread->execute(&RunAviFileWrite, fs);
	}
	
	if (this->_pendingBufferCount > 0)
	{
		this->_currentBufferIndex = ((this->_currentBufferIndex + 1) % this->_pendingBufferCount);
	}
	
	pthread_rwlock_unlock(&this->_rwlockStream);
	
	return true;
}

size_t ClientAVCaptureObject::__DrainAudioRing(uint8_t *dstBuffer, size_t *outRemaining)
{
	size_t audioBytes = 0;
	
	// Pull whatever audio has been captured since the last queued frame into
	// this frame's slot.  This is the only place audio leaves the ring, and it
	// runs on the same thread that just filled the video slot, so the audio
	// and video of one queued frame always belong together.
	pthread_mutex_lock(&this->_mutexAudioRing);
	
	if ( (this->_audioRingCount > 0) && (dstBuffer != NULL) )
	{
		audioBytes = (this->_audioRingCount < (size_t)AUDIO_STREAM_BUFFER_SIZE)
					 ? this->_audioRingCount : (size_t)AUDIO_STREAM_BUFFER_SIZE;
		
		const size_t firstPart = ((this->_audioRingCapacity - this->_audioRingReadPos) < audioBytes)
								 ? (this->_audioRingCapacity - this->_audioRingReadPos) : audioBytes;
		
		memcpy(dstBuffer, this->_audioRing + this->_audioRingReadPos, firstPart);
		if (audioBytes > firstPart)
		{
			memcpy(dstBuffer + firstPart, this->_audioRing, audioBytes - firstPart);
		}
		
		this->_audioRingReadPos = (this->_audioRingReadPos + audioBytes) % this->_audioRingCapacity;
		this->_audioRingCount -= audioBytes;
	}
	
	if (outRemaining != NULL)
	{
		*outRemaining = this->_audioRingCount;
	}
	
	pthread_mutex_unlock(&this->_mutexAudioRing);
	
	return audioBytes;
}

void ClientAVCaptureObject::StreamWriteFinish()
{
	this->_fileWriteThread->finish();
}

void ClientAVCaptureObject::CaptureAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount)
{
	if ( (srcAudioBuffer == NULL) || !this->_isCapturingAudio )
	{
		return;
	}
	
	const size_t soundSize = inSampleCount * this->_audioBlockSize;
	if (soundSize == 0)
	{
		return;
	}
	
	pthread_mutex_lock(&this->_mutexAudioRing);
	
	if ( (this->_audioRing == NULL) || (soundSize > (this->_audioRingCapacity - this->_audioRingCount)) )
	{
		this->_audioRingDroppedBytes += soundSize;
		if (this->_audioRingDroppedBytes == soundSize)
		{
			printf("DeSmuME[AVCapture]: Audio ring full, dropping samples (capacity=%zu).\n", this->_audioRingCapacity);
			fflush(stdout);
		}
		pthread_mutex_unlock(&this->_mutexAudioRing);
		return;
	}
	
	const size_t writePos = (this->_audioRingReadPos + this->_audioRingCount) % this->_audioRingCapacity;
	const size_t firstPart = ((this->_audioRingCapacity - writePos) < soundSize)
							 ? (this->_audioRingCapacity - writePos) : soundSize;
	
	memcpy(this->_audioRing + writePos, srcAudioBuffer, firstPart);
	if (soundSize > firstPart)
	{
		memcpy(this->_audioRing, (const uint8_t *)srcAudioBuffer + firstPart, soundSize - firstPart);
	}
	
	this->_audioRingCount += soundSize;
	
	pthread_mutex_unlock(&this->_mutexAudioRing);
}
