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

#include "ClientAVCaptureObject.h"

#include "../../common.h"
#include "../../NDSSystem.h"
#include "../../GPU.h"


static void* RunConvertVideoSlice555XTo888(void *arg)
{
	VideoConvertParam *convertParam = (VideoConvertParam *)arg;
	convertParam->captureObj->ConvertVideoSlice555Xto888(*convertParam);
	
	return NULL;
}

static void* RunConvertVideoSlice888XTo888(void *arg)
{
	VideoConvertParam *convertParam = (VideoConvertParam *)arg;
	convertParam->captureObj->ConvertVideoSlice888Xto888(*convertParam);
	
	return NULL;
}

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
	
	const size_t videoFrameSize = videoWidth * videoHeight * 3; // The video frame will always be in the RGB888 colorspace.
	const size_t audioBlockSize = sizeof(int16_t) * 2;
	const size_t audioFrameSize = ((DESMUME_SAMPLE_RATE * audioBlockSize) / 30);
	
	this->_expectedMaxFrameSize = videoFrameSize + audioFrameSize;
	
	_semQueue = ssem_new(pendingFrameCount);
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

void ClientAVCaptureFileStream::QueueAdd(uint8_t *srcVideo, const size_t videoBufferSize, uint8_t *srcAudio, const size_t audioBufferSize)
{
	AVStreamWriteParam newParam;
	newParam.srcVideo = srcVideo;
	newParam.videoBufferSize = videoBufferSize;
	newParam.srcAudio = srcAudio;
	newParam.audioBufferSize = audioBufferSize;
	
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

ClientAVCaptureObject::ClientAVCaptureObject(size_t videoFrameWidth, size_t videoFrameHeight)
{
	__InstanceInit(videoFrameWidth, videoFrameHeight);
	
	if (_videoFrameSize > 0)
	{
		_pendingVideoBuffer = (u8 *)malloc_alignedCacheLine(_videoFrameSize * _pendingBufferCount);
	}
	
	_pendingAudioBuffer = (u8 *)malloc_alignedCacheLine(_audioFrameSize * _pendingBufferCount);
	_pendingAudioWriteSize = (size_t *)calloc(_pendingBufferCount, sizeof(size_t));
	
	const size_t linesPerThread = (_numThreads > 0) ? videoFrameHeight / _numThreads : videoFrameHeight;
	
	if (_numThreads == 0)
	{
		for (size_t i = 0; i < MAX_CONVERT_THREADS; i++)
		{
			_convertParam[i].firstLineIndex = 0;
			_convertParam[i].lastLineIndex = linesPerThread - 1;
			_convertParam[i].srcOffset = 0;
			_convertParam[i].dstOffset = 0;
			_convertParam[i].frameWidth = videoFrameWidth;
		}
	}
	else
	{
		for (size_t i = 0; i < _numThreads; i++)
		{
			if (i == 0)
			{
				_convertParam[i].firstLineIndex = 0;
				_convertParam[i].lastLineIndex = linesPerThread - 1;
			}
			else if (i == (_numThreads - 1))
			{
				_convertParam[i].firstLineIndex = _convertParam[i - 1].lastLineIndex + 1;
				_convertParam[i].lastLineIndex = videoFrameHeight - 1;
			}
			else
			{
				_convertParam[i].firstLineIndex = _convertParam[i - 1].lastLineIndex + 1;
				_convertParam[i].lastLineIndex = _convertParam[i].firstLineIndex + linesPerThread - 1;
			}
			
			_convertParam[i].srcOffset = (videoFrameWidth * _convertParam[i].firstLineIndex);
			_convertParam[i].dstOffset = (videoFrameWidth * _convertParam[i].firstLineIndex * 3);
			_convertParam[i].frameWidth = videoFrameWidth;
		}
	}
}

void ClientAVCaptureObject::__InstanceInit(size_t videoFrameWidth, size_t videoFrameHeight)
{
	_fs = NULL;
	_isCapturingVideo = false;
	_isCapturingAudio = false;
	
	_mutexCaptureFlags = slock_new();
	
	if ( (videoFrameWidth == 0) || (videoFrameHeight == 0) )
	{
		// Audio-only capture.
		_videoFrameWidth = 0;
		_videoFrameHeight = 0;
		_videoFrameSize = 0;
	}
	else
	{
		_videoFrameWidth = videoFrameWidth;
		_videoFrameHeight = videoFrameHeight;
		_videoFrameSize = videoFrameWidth * videoFrameHeight * 3; // The video frame will always be in the RGB888 colorspace.
	}
	
	_audioBlockSize = sizeof(int16_t) * 2;
	_audioFrameSize = ((DESMUME_SAMPLE_RATE * _audioBlockSize) / 30);
	
	_pendingVideoBuffer = NULL;
	_pendingAudioBuffer = NULL;
	_pendingAudioWriteSize = NULL;
	_currentBufferIndex = 0;
	
	_pendingBufferCount = MAX_PENDING_BUFFER_SIZE / (_videoFrameSize + _audioFrameSize);
	if (_pendingBufferCount > MAX_PENDING_FRAME_COUNT)
	{
		_pendingBufferCount = MAX_PENDING_FRAME_COUNT;
	}
	
	// Create the colorspace conversion threads.
	_numThreads = CommonSettings.num_cores;
	
	if (_numThreads > MAX_CONVERT_THREADS)
	{
		_numThreads = MAX_CONVERT_THREADS;
	}
	else if (_numThreads < 2)
	{
		_numThreads = 0;
	}
	
	for (size_t i = 0; i < _numThreads; i++)
	{
		memset(&_convertParam[i], 0, sizeof(VideoConvertParam));
		_convertParam[i].captureObj = this;
		
		_convertThread[i] = new Task();
		_convertThread[i]->start(false);
	}
	
	_fileWriteThread = new Task();
	_fileWriteThread->start(false);
}

ClientAVCaptureObject::~ClientAVCaptureObject()
{
	this->_fileWriteThread->finish();
	delete this->_fileWriteThread;
	
	for (size_t i = 0; i < this->_numThreads; i++)
	{
		this->_convertThread[i]->finish();
		delete this->_convertThread[i];
	}
	
	free_aligned(this->_pendingVideoBuffer);
	free_aligned(this->_pendingAudioBuffer);
	free(this->_pendingAudioWriteSize);
	
	slock_free(this->_mutexCaptureFlags);
}

ClientAVCaptureFileStream* ClientAVCaptureObject::GetOutputFileStream()
{
	return this->_fs;
}

void ClientAVCaptureObject::SetOutputFileStream(ClientAVCaptureFileStream *fs)
{
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
}

bool ClientAVCaptureObject::IsFileStreamValid()
{
	if (this->_fs == NULL)
	{
		return false;
	}
	
	return this->_fs->IsValid();
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
	if (!this->IsFileStreamValid())
	{
		return;
	}
	
	// If the queue is full, then we need to wait for some frames to finish writing
	// before we continue adding new frames to the queue.
	this->_fs->QueueWait();
	
	this->_currentBufferIndex = ((this->_currentBufferIndex + 1) % this->_pendingBufferCount);
	this->_pendingAudioWriteSize[this->_currentBufferIndex] = 0;
}

void ClientAVCaptureObject::StreamWriteStart()
{
	if (!this->IsFileStreamValid())
	{
		return;
	}
	
	// Force video conversion to finish before putting the frame on the queue.
	if ( (this->_videoFrameSize > 0) && (this->_numThreads > 0) )
	{
		for (size_t i = 0; i < this->_numThreads; i++)
		{
			this->_convertThread[i]->finish();
		}
	}
	
	const size_t bufferIndex = this->_currentBufferIndex;
	const size_t queueSize = this->_fs->GetQueueSize();
	const bool isQueueEmpty = (queueSize == 0);
	
	this->_fs->QueueAdd(this->_pendingVideoBuffer + (this->_videoFrameSize * bufferIndex), this->_videoFrameSize,
						this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex), this->_pendingAudioWriteSize[bufferIndex]);
	
	// If the queue was initially empty, then we need to wake up the file write
	// thread at this time.
	if (isQueueEmpty)
	{
		this->_fileWriteThread->execute(&RunAviFileWrite, this->_fs);
	}
}

void ClientAVCaptureObject::StreamWriteFinish()
{
	this->_fileWriteThread->finish();
}

//converts 16bpp to 24bpp and flips
void ClientAVCaptureObject::ConvertVideoSlice555Xto888(const VideoConvertParam &param)
{
	const size_t lineCount = param.lastLineIndex - param.firstLineIndex + 1;
	const u16 *__restrict src = (const u16 *__restrict)param.src;
	u8 *__restrict dst = param.dst;
	
	ColorspaceConvertBuffer555XTo888<false, false>(src, dst, param.frameWidth * lineCount);
}

//converts 32bpp to 24bpp and flips
void ClientAVCaptureObject::ConvertVideoSlice888Xto888(const VideoConvertParam &param)
{
	const size_t lineCount = param.lastLineIndex - param.firstLineIndex + 1;
	const u32 *__restrict src = (const u32 *__restrict)param.src;
	u8 *__restrict dst = param.dst;
	
	ColorspaceConvertBuffer888XTo888<false, false>(src, dst, param.frameWidth * lineCount);
}

void ClientAVCaptureObject::CaptureVideoFrame(const void *srcVideoFrame, const size_t inFrameWidth, const size_t inFrameHeight, const NDSColorFormat colorFormat)
{
	//dont do anything if prescale has changed, it's just going to be garbage
	if (!this->_isCapturingVideo ||
		(srcVideoFrame == NULL) ||
		(this->_videoFrameSize == 0) ||
		(this->_videoFrameWidth != inFrameWidth) ||
		(this->_videoFrameHeight != inFrameHeight))
	{
		return;
	}
	
	const size_t bufferIndex = this->_currentBufferIndex;
	u8 *convertBufferHead = this->_pendingVideoBuffer + (this->_videoFrameSize * bufferIndex);
	
	if (colorFormat == NDSColorFormat_BGR555_Rev)
	{
		if (this->_numThreads == 0)
		{
			this->_convertParam[0].src = (u16 *)srcVideoFrame;
			this->_convertParam[0].dst = convertBufferHead + this->_convertParam[0].dstOffset;
			this->ConvertVideoSlice555Xto888(this->_convertParam[0]);
		}
		else
		{
			for (size_t i = 0; i < this->_numThreads; i++)
			{
				this->_convertThread[i]->finish();
				this->_convertParam[i].src = (u16 *)srcVideoFrame + this->_convertParam[i].srcOffset;
				this->_convertParam[i].dst = convertBufferHead + this->_convertParam[i].dstOffset;
				this->_convertThread[i]->execute(&RunConvertVideoSlice555XTo888, &this->_convertParam[i]);
			}
		}
	}
	else
	{
		if (this->_numThreads == 0)
		{
			this->_convertParam[0].src = (u32 *)srcVideoFrame;
			this->_convertParam[0].dst = convertBufferHead + this->_convertParam[0].dstOffset;
			this->ConvertVideoSlice888Xto888(this->_convertParam[0]);
		}
		else
		{
			for (size_t i = 0; i < this->_numThreads; i++)
			{
				this->_convertThread[i]->finish();
				this->_convertParam[i].src = (u32 *)srcVideoFrame + this->_convertParam[i].srcOffset;
				this->_convertParam[i].dst = convertBufferHead + this->_convertParam[i].dstOffset;
				this->_convertThread[i]->execute(&RunConvertVideoSlice888XTo888, &this->_convertParam[i]);
			}
		}
	}
}

void ClientAVCaptureObject::CaptureAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount)
{
	if (!this->_isCapturingAudio || (srcAudioBuffer == NULL))
	{
		return;
	}
	
	const size_t bufferIndex = this->_currentBufferIndex;
	const size_t soundSize = inSampleCount * this->_audioBlockSize;
	
	memcpy(this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex) + this->_pendingAudioWriteSize[bufferIndex], srcAudioBuffer, soundSize);
	this->_pendingAudioWriteSize[bufferIndex] += soundSize;
}
