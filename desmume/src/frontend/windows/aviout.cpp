/*
	Copyright (C) 2006-2018 DeSmuME team

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

#include "aviout.h"

#include <assert.h>
#include <stdio.h>

#include <string>

#include "debug.h"
#include "common.h"

#include "windriver.h"
#include "driver.h"
#include "NDSSystem.h"


NDSCaptureObject *captureObject = NULL;
bool AVIFileStream::__needAviLibraryInit = true;

static void EMU_PrintError(const char* msg)
{
	LOG(msg);
}

static void EMU_PrintMessage(const char* msg)
{
	LOG(msg);
}

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
	AVIFileStream *fs = (AVIFileStream *)arg;
	fs->WriteAllFrames();

	return NULL;
}

AVIFileStream::AVIFileStream()
{
	_file = NULL;
	_stream[0] = NULL;
	_stream[1] = NULL;
	_compressedStream[0] = NULL;
	_compressedStream[1] = NULL;

	memset(&_compressionOptions[0], 0, sizeof(AVICOMPRESSOPTIONS));
	memset(&_compressionOptions[1], 0, sizeof(AVICOMPRESSOPTIONS));
	memset(&_bmpFormat, 0, sizeof(BITMAPINFOHEADER));
	memset(&_wavFormat, 0, sizeof(WAVEFORMATEX));

	memset(_baseFileName, '\0', MAX_PATH * sizeof(char));
	memset(_baseFileNameExt, '\0', MAX_PATH * sizeof(char));
	_segmentNumber = 0;

	memset(&_streamInfo[VIDEO_STREAM], 0, sizeof(AVISTREAMINFO));
	_streamInfo[VIDEO_STREAM].fccType = streamtypeVIDEO;
	_streamInfo[VIDEO_STREAM].dwScale = 6 * 355 * 263;
	_streamInfo[VIDEO_STREAM].dwRate = 33513982;

	memset(&_streamInfo[AUDIO_STREAM], 0, sizeof(AVISTREAMINFO));
	_streamInfo[AUDIO_STREAM].fccType = streamtypeAUDIO;
	_streamInfo[AUDIO_STREAM].dwQuality = (DWORD)-1;
	_streamInfo[AUDIO_STREAM].dwInitialFrames = 1;

	_expectedFrameSize = 0;
	_writtenVideoFrameCount = 0;
	_writtenAudioSampleCount = 0;
	_writtenBytes = 0;

	_semQueue = NULL;
	_mutexQueue = slock_new();
}

AVIFileStream::~AVIFileStream()
{
	this->Close(FSCA_WriteRemainingInQueue);

	slock_free(this->_mutexQueue);
	ssem_free(this->_semQueue);
}

HRESULT AVIFileStream::InitBaseProperties(const char *fileName, BITMAPINFOHEADER *bmpFormat, WAVEFORMATEX *wavFormat, size_t pendingFrameCount)
{
	HRESULT error = S_OK;

	// Generate the base file name. This will be used for the first segment.
	const char *dot = strrchr(fileName, '.');
	if (dot && dot > strrchr(fileName, '/') && dot > strrchr(fileName, '\\'))
	{
		const size_t baseNameSize = dot - fileName;

		strcpy(this->_baseFileNameExt, dot);
		strncpy(this->_baseFileName, fileName, baseNameSize);
		this->_baseFileName[baseNameSize] = '\0'; // Even though the string should already be filled with \0, manually terminate again for safety's sake.
	}
	else
	{
		error = E_INVALIDARG;
		return error;
	}

	// Set up the stream formats that will be used for all AVI segments.
	if (bmpFormat != NULL)
	{
		this->_bmpFormat = *bmpFormat;
		this->_streamInfo[VIDEO_STREAM].dwSuggestedBufferSize = this->_bmpFormat.biSizeImage;
	}

	if (wavFormat != NULL)
	{
		this->_wavFormat = *wavFormat;
		this->_streamInfo[AUDIO_STREAM].dwScale = this->_wavFormat.nBlockAlign;
		this->_streamInfo[AUDIO_STREAM].dwRate = this->_wavFormat.nAvgBytesPerSec;
		this->_streamInfo[AUDIO_STREAM].dwSampleSize = this->_wavFormat.nBlockAlign;
	}

	this->_expectedFrameSize = NDSCaptureObject::GetExpectedFrameSize(bmpFormat, wavFormat);

	_semQueue = ssem_new(pendingFrameCount);

	return error;
}

HRESULT AVIFileStream::Open()
{
	HRESULT error = S_OK;

	// Initialize the AVI library if it hasn't already been initialized.
	if (AVIFileStream::__needAviLibraryInit)
	{
		AVIFileInit();
		AVIFileStream::__needAviLibraryInit = false;
	}

	char workingFileName[MAX_PATH];
	memset(workingFileName, '\0', sizeof(workingFileName));

	if (this->_segmentNumber == 0)
	{
		sprintf(workingFileName, "%s%s", this->_baseFileName, this->_baseFileNameExt);
	}
	else
	{
		// Tack on a suffix to the base file name in order to generate a new file name.
		sprintf(workingFileName, "%s_part%d%s", this->_baseFileName, (int)(this->_segmentNumber + 1), this->_baseFileNameExt);
	}

	// this is only here because AVIFileOpen doesn't seem to do it for us
	FILE *fd = fopen(workingFileName, "wb");
	if (fd != NULL)
	{
		fclose(fd);
	}
	else
	{
		error = E_ACCESSDENIED;
		return error;
	}

	// open the file
	error = AVIFileOpen(&this->_file, workingFileName, OF_CREATE | OF_WRITE, NULL);
	if (FAILED(error))
	{
		this->_file = NULL;
		return error;
	}

	this->_writtenVideoFrameCount = 0;
	this->_writtenAudioSampleCount = 0;
	this->_writtenBytes = 0;

	if (this->_streamInfo[VIDEO_STREAM].dwSuggestedBufferSize > 0)
	{
		error = AVIFileCreateStream(this->_file, &this->_stream[VIDEO_STREAM], &this->_streamInfo[VIDEO_STREAM]);
		if (FAILED(error))
		{
			return error;
		}

		if (this->_segmentNumber == 0)
		{
			AVICOMPRESSOPTIONS *videoCompressionOptionsPtr = &this->_compressionOptions[VIDEO_STREAM];

			// get compression options
			INT_PTR optionsID = AVISaveOptions(MainWindow->getHWnd(), 0, 1, &this->_stream[VIDEO_STREAM], &videoCompressionOptionsPtr);
			if (optionsID == 0) // User clicked "Cancel"
			{
				error = E_ABORT;
				return error;
			}
		}

		// create compressed stream
		error = AVIMakeCompressedStream(&this->_compressedStream[VIDEO_STREAM], this->_stream[VIDEO_STREAM], &this->_compressionOptions[VIDEO_STREAM], NULL);
		if (FAILED(error))
		{
			return error;
		}

		// set the stream format
		error = AVIStreamSetFormat(this->_compressedStream[VIDEO_STREAM], 0, &this->_bmpFormat, this->_bmpFormat.biSize);
		if (FAILED(error))
		{
			return error;
		}
	}

	if (this->_streamInfo[AUDIO_STREAM].dwSampleSize > 0)
	{
		// create the audio stream
		error = AVIFileCreateStream(this->_file, &this->_stream[AUDIO_STREAM], &this->_streamInfo[AUDIO_STREAM]);
		if (FAILED(error))
		{
			return error;
		}

		// AVISaveOptions doesn't seem to work for audio streams
		// so here we just copy the pointer for the compressed stream
		this->_compressedStream[AUDIO_STREAM] = this->_stream[AUDIO_STREAM];

		// set the stream format
		error = AVIStreamSetFormat(this->_compressedStream[AUDIO_STREAM], 0, &this->_wavFormat, sizeof(WAVEFORMATEX));
		if (FAILED(error))
		{
			return error;
		}
	}

	return error;
}

void AVIFileStream::_CloseStreams()
{
	// _compressedStream[AUDIO_STREAM] is just a copy of _stream[AUDIO_STREAM]
	if (this->_compressedStream[AUDIO_STREAM])
	{
		AVIStreamClose(this->_compressedStream[AUDIO_STREAM]);
		this->_compressedStream[AUDIO_STREAM] = NULL;
		this->_stream[AUDIO_STREAM] = NULL;
	}

	if (this->_compressedStream[VIDEO_STREAM])
	{
		AVIStreamClose(this->_compressedStream[VIDEO_STREAM]);
		this->_compressedStream[VIDEO_STREAM] = NULL;
	}

	if (this->_stream[VIDEO_STREAM])
	{
		AVIStreamClose(this->_stream[VIDEO_STREAM]);
		this->_stream[VIDEO_STREAM] = NULL;
	}
}

void AVIFileStream::Close(FileStreamCloseAction theAction)
{
	switch (theAction)
	{
		case FSCA_PurgeQueue:
		{
			AVIWriteQueue clearQueue;

			slock_lock(this->_mutexQueue);
			this->_writeQueue.swap(clearQueue);
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
			if (this->_file != NULL)
			{
				do
				{
					slock_lock(this->_mutexQueue);
					if (this->_writeQueue.empty())
					{
						slock_unlock(this->_mutexQueue);
						break;
					}

					const AVIFileWriteParam param = this->_writeQueue.front();
					slock_unlock(this->_mutexQueue);

					this->WriteOneFrame(param);

					slock_lock(this->_mutexQueue);
					this->_writeQueue.pop();
					slock_unlock(this->_mutexQueue);

					ssem_signal(this->_semQueue);

				} while (true);
			}
			break;
		}

		case FSCA_DoNothing:
		default:
			break;
	}

	this->_CloseStreams();

	if (this->_file != NULL)
	{
		AVIFileClose(this->_file);
		this->_file = NULL;
	}
}

bool AVIFileStream::IsValid()
{
	return (this->_file != NULL);
}

void AVIFileStream::QueueAdd(u8 *srcVideo, const size_t videoBufferSize, u8 *srcAudio, const size_t audioBufferSize)
{
	AVIFileWriteParam newParam;
	newParam.srcVideo = srcVideo;
	newParam.videoBufferSize = videoBufferSize;
	newParam.srcAudio = srcAudio;
	newParam.audioBufferSize = audioBufferSize;

	ssem_wait(this->_semQueue);

	slock_lock(this->_mutexQueue);
	this->_writeQueue.push(newParam);
	slock_unlock(this->_mutexQueue);
}

void AVIFileStream::QueueWait()
{
	// If the queue if full, the ssem_wait() will force a wait until the current frame is finished writing.
	ssem_wait(this->_semQueue);
	ssem_signal(this->_semQueue);
}

size_t AVIFileStream::GetQueueSize()
{
	slock_lock(this->_mutexQueue);
	const size_t queueSize = this->_writeQueue.size();
	slock_unlock(this->_mutexQueue);

	return queueSize;
}

HRESULT AVIFileStream::FlushVideo(u8 *srcBuffer, const LONG bufferSize)
{
	HRESULT error = S_OK;

	if (this->_compressedStream[VIDEO_STREAM] == NULL)
	{
		return error;
	}

	LONG writtenBytesOut = 0;

	error = AVIStreamWrite(this->_compressedStream[VIDEO_STREAM],
		this->_writtenVideoFrameCount, 1,
		srcBuffer, bufferSize,
		AVIIF_KEYFRAME,
		NULL, &writtenBytesOut);

	if (FAILED(error))
	{
		return error;
	}

	this->_writtenVideoFrameCount++;
	this->_writtenBytes += writtenBytesOut;

	return error;
}

HRESULT AVIFileStream::FlushAudio(u8 *srcBuffer, const LONG bufferSize)
{
	HRESULT error = S_OK;

	if ( (this->_compressedStream[AUDIO_STREAM] == NULL) || (bufferSize == 0) )
	{
		return error;
	}

	const LONG inSampleCount = bufferSize / this->_streamInfo[AUDIO_STREAM].dwSampleSize;
	LONG writtenBytesOut = 0;

	error = AVIStreamWrite(this->_compressedStream[AUDIO_STREAM],
		this->_writtenAudioSampleCount, inSampleCount,
		srcBuffer, bufferSize,
		0,
		NULL, &writtenBytesOut);

	if (FAILED(error))
	{
		return error;
	}

	this->_writtenAudioSampleCount += inSampleCount;
	this->_writtenBytes += writtenBytesOut;

	return error;
}

HRESULT AVIFileStream::WriteOneFrame(const AVIFileWriteParam &param)
{
	HRESULT error = S_OK;

	error = this->FlushVideo(param.srcVideo, param.videoBufferSize);
	if (FAILED(error))
	{
		this->Close(FSCA_PurgeQueue);
		return error;
	}

	error = this->FlushAudio(param.srcAudio, param.audioBufferSize);
	if (FAILED(error))
	{
		this->Close(FSCA_PurgeQueue);
		return error;
	}

	// Create a new AVI segment if the next written frame would exceed the maximum AVI file size.
	const size_t futureFrameSize = this->_writtenBytes + this->_expectedFrameSize;
	if (futureFrameSize >= MAX_AVI_FILE_SIZE)
	{
		PAVIFILE oldFile = this->_file;

		this->_CloseStreams();
		this->_segmentNumber++;

		error = this->Open();
		AVIFileClose(oldFile);

		if (FAILED(error))
		{
			EMU_PrintError("Error creating new AVI segment.");
			this->Close(FSCA_PurgeQueue);
			return error;
		}
	}

	return error;
}

HRESULT AVIFileStream::WriteAllFrames()
{
	HRESULT error = S_OK;

	do
	{
		slock_lock(this->_mutexQueue);
		if (this->_writeQueue.empty())
		{
			slock_unlock(this->_mutexQueue);
			break;
		}

		const AVIFileWriteParam param = this->_writeQueue.front();
		slock_unlock(this->_mutexQueue);

		error = this->WriteOneFrame(param);

		slock_lock(this->_mutexQueue);
		this->_writeQueue.pop();
		slock_unlock(this->_mutexQueue);

		ssem_signal(this->_semQueue);

		if (FAILED(error))
		{
			return error;
		}

	} while (true);

	return error;
}

NDSCaptureObject::NDSCaptureObject()
{
	// Create the format structs.
	memset(&_bmpFormat, 0, sizeof(BITMAPINFOHEADER));
	_bmpFormat.biSize = 0x28;
	_bmpFormat.biPlanes = 1;
	_bmpFormat.biBitCount = 24;

	memset(&_wavFormat, 0, sizeof(WAVEFORMATEX));

	_pendingVideoBuffer = NULL;
	_pendingAudioBuffer = NULL;
	_pendingAudioWriteSize = NULL;
	_pendingBufferCount = 0;
	_currentBufferIndex = 0;

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
	
	// Generate the AVI file streams.
	_fs = new AVIFileStream;

	_fileWriteThread = new Task();
	_fileWriteThread->start(false);
}

NDSCaptureObject::NDSCaptureObject(size_t frameWidth, size_t frameHeight, const WAVEFORMATEX *wfex)
{
	this->NDSCaptureObject::NDSCaptureObject();

	_bmpFormat.biWidth = frameWidth;
	_bmpFormat.biHeight = frameHeight * 2;
	_bmpFormat.biSizeImage = _bmpFormat.biWidth * _bmpFormat.biHeight * 3;

	if (wfex != NULL)
	{
		_wavFormat = *wfex;
	}

	const size_t expectedFrameSize = NDSCaptureObject::GetExpectedFrameSize(&_bmpFormat, wfex);
	_pendingBufferCount = MAX_PENDING_BUFFER_SIZE / expectedFrameSize;

	if (_pendingBufferCount > MAX_PENDING_FRAME_COUNT)
	{
		_pendingBufferCount = MAX_PENDING_FRAME_COUNT;
	}

	_pendingVideoBuffer = (u8 *)malloc_alignedCacheLine(_bmpFormat.biSizeImage * _pendingBufferCount * sizeof(u8));
	_pendingAudioBuffer = (u8 *)malloc_alignedCacheLine(AUDIO_STREAM_BUFFER_SIZE * _pendingBufferCount * sizeof(u8));
	_pendingAudioWriteSize = (size_t *)calloc(_pendingBufferCount, sizeof(size_t));

	const size_t linesPerThread = (_numThreads > 0) ? _bmpFormat.biHeight / _numThreads : _bmpFormat.biHeight;

	if (_numThreads == 0)
	{
		for (size_t i = 0; i < MAX_CONVERT_THREADS; i++)
		{
			_convertParam[i].firstLineIndex = 0;
			_convertParam[i].lastLineIndex = linesPerThread - 1;
			_convertParam[i].srcOffset = 0;
			_convertParam[i].dstOffset = (_bmpFormat.biWidth * (_bmpFormat.biHeight - 1) * 3);
			_convertParam[i].frameWidth = _bmpFormat.biWidth;
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
				_convertParam[i].lastLineIndex = _bmpFormat.biHeight - 1;
			}
			else
			{
				_convertParam[i].firstLineIndex = _convertParam[i - 1].lastLineIndex + 1;
				_convertParam[i].lastLineIndex = _convertParam[i].firstLineIndex + linesPerThread - 1;
			}

			_convertParam[i].srcOffset = (_bmpFormat.biWidth * _convertParam[i].firstLineIndex);
			_convertParam[i].dstOffset = (_bmpFormat.biWidth * (_bmpFormat.biHeight - (_convertParam[i].firstLineIndex + 1)) * 3);
			_convertParam[i].frameWidth = _bmpFormat.biWidth;
		}
	}
}

NDSCaptureObject::~NDSCaptureObject()
{
	this->_fileWriteThread->finish();
	delete this->_fileWriteThread;

	for (size_t i = 0; i < this->_numThreads; i++)
	{
		this->_convertThread[i]->finish();
		delete this->_convertThread[i];
	}

	delete this->_fs;

	free_aligned(this->_pendingVideoBuffer);
	free_aligned(this->_pendingAudioBuffer);
	free(this->_pendingAudioWriteSize);
}

size_t NDSCaptureObject::GetExpectedFrameSize(const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat)
{
	size_t expectedFrameSize = 0; // The expected frame size is the video frame size plus the sum of the audio samples.

	if (bmpFormat != NULL)
	{
		expectedFrameSize += bmpFormat->biSizeImage;
	}

	if (wavFormat != NULL)
	{
		// Since the number of audio samples may not be exactly the same for each video frame,
		// we double the expected size of the audio buffer for safety.
		expectedFrameSize += ((wavFormat->nAvgBytesPerSec / 60) * 2);
	}

	return expectedFrameSize;
}

HRESULT NDSCaptureObject::OpenFileStream(const char *fileName)
{
	HRESULT error = S_OK;

	error = this->_fs->InitBaseProperties(fileName, &this->_bmpFormat, &this->_wavFormat, this->_pendingBufferCount);
	if (FAILED(error))
	{
		return error;
	}

	return this->_fs->Open();
}

void NDSCaptureObject::CloseFileStream()
{
	this->_fs->Close(FSCA_WriteRemainingInQueue);
}

bool NDSCaptureObject::IsFileStreamValid()
{
	return this->_fs->IsValid();
}

void NDSCaptureObject::StartFrame()
{
	// If the queue is full, then we need to wait for some frames to finish writing
	// before we continue adding new frames to the queue.
	this->_fs->QueueWait();

	this->_currentBufferIndex = ((this->_currentBufferIndex + 1) % this->_pendingBufferCount);
	this->_pendingAudioWriteSize[this->_currentBufferIndex] = 0;
}

void NDSCaptureObject::StreamWriteStart()
{
	const size_t bufferIndex = this->_currentBufferIndex;
	const size_t queueSize = this->_fs->GetQueueSize();
	const bool isQueueEmpty = (queueSize == 0);

	// If there are no frames in the current write queue, then we know that the current
	// pending video frame will be written immediately. If this is the case, then we
	// need to force the video conversion to finish so that we can write out the frame.
	if (isQueueEmpty)
	{
		if (this->_bmpFormat.biSizeImage > 0)
		{
			for (size_t i = 0; i < this->_numThreads; i++)
			{
				this->_convertThread[i]->finish();
			}
		}
	}

	this->_fs->QueueAdd(this->_pendingVideoBuffer + (this->_bmpFormat.biSizeImage * bufferIndex), this->_bmpFormat.biSizeImage,
	                    this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex), this->_pendingAudioWriteSize[bufferIndex]);

	if (isQueueEmpty)
	{
		this->_fileWriteThread->execute(&RunAviFileWrite, this->_fs);
	}
}

void NDSCaptureObject::StreamWriteFinish()
{
	this->_fileWriteThread->finish();
}

//converts 16bpp to 24bpp and flips
void NDSCaptureObject::ConvertVideoSlice555Xto888(const VideoConvertParam &param)
{
	const u16 *__restrict src = (const u16 *__restrict)param.src;
	u8 *__restrict dst = param.dst;

	for (size_t y = param.firstLineIndex; y <= param.lastLineIndex; y++)
	{
		ColorspaceConvertBuffer555XTo888<true, false>(src, dst, param.frameWidth);
		src += param.frameWidth;
		dst -= param.frameWidth * 3;
	}
}

//converts 32bpp to 24bpp and flips
void NDSCaptureObject::ConvertVideoSlice888Xto888(const VideoConvertParam &param)
{
	const u32 *__restrict src = (const u32 *__restrict)param.src;
	u8 *__restrict dst = param.dst;

	for (size_t y = param.firstLineIndex; y <= param.lastLineIndex; y++)
	{
		ColorspaceConvertBuffer888XTo888<true, false>(src, dst, param.frameWidth);
		src += param.frameWidth;
		dst -= param.frameWidth * 3;
	}
}

void NDSCaptureObject::ReadVideoFrame(const void *srcVideoFrame, const size_t inFrameWidth, const size_t inFrameHeight, const NDSColorFormat colorFormat)
{
	//dont do anything if prescale has changed, it's just going to be garbage
	if ((this->_bmpFormat.biSizeImage == 0) ||
		(this->_bmpFormat.biWidth != inFrameWidth) ||
		(this->_bmpFormat.biHeight != (inFrameHeight * 2)))
	{
		return;
	}

	const size_t bufferIndex = this->_currentBufferIndex;
	u8 *convertBufferHead = this->_pendingVideoBuffer + (this->_bmpFormat.biSizeImage * bufferIndex);

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

void NDSCaptureObject::ReadAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount)
{
	if (this->_wavFormat.nBlockAlign == 0)
	{
		return;
	}

	const size_t bufferIndex = this->_currentBufferIndex;
	const size_t soundSize = inSampleCount * this->_wavFormat.nBlockAlign;

	memcpy(this->_pendingAudioBuffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex) + this->_pendingAudioWriteSize[bufferIndex], srcAudioBuffer, soundSize);
	this->_pendingAudioWriteSize[bufferIndex] += soundSize;
}

bool DRV_AviBegin(const char *fileName)
{
	HRESULT error = S_OK;
	bool result = true;

	WAVEFORMATEX wf;
	wf.cbSize = sizeof(WAVEFORMATEX);
	wf.nAvgBytesPerSec = DESMUME_SAMPLE_RATE * sizeof(u16) * 2;
	wf.nBlockAlign = 4;
	wf.nChannels = 2;
	wf.nSamplesPerSec = DESMUME_SAMPLE_RATE;
	wf.wBitsPerSample = 16;
	wf.wFormatTag = WAVE_FORMAT_PCM;
	 
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	NDSCaptureObject *newCaptureObject = new NDSCaptureObject(dispInfo.customWidth, dispInfo.customHeight, &wf);

	error = newCaptureObject->OpenFileStream(fileName);
	if (FAILED(error))
	{
		if (error != E_ABORT)
		{
			EMU_PrintError("Error starting AVI file.");
			driver->AddLine("Error starting AVI file.");
		}

		delete newCaptureObject;

		result = false;
		return result;
	}

	captureObject = newCaptureObject;
	return result;
}

void DRV_AviFrameStart()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	captureObject->StartFrame();
}

void DRV_AviVideoUpdate(const NDSDisplayInfo &displayInfo)
{
	if (!AVI_IsRecording())
	{
		return;
	}

	captureObject->ReadVideoFrame(displayInfo.masterCustomBuffer, displayInfo.customWidth, displayInfo.customHeight, displayInfo.colorFormat);
}

void DRV_AviSoundUpdate(void *soundData, const int soundLen)
{
	if (!AVI_IsRecording())
	{
		return;
	}

	captureObject->ReadAudioFrames(soundData, soundLen);
}

void DRV_AviFileWriteStart()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	// Clean up stale capture objects at this time if the underlying file stream is invalid.
	// An invalid file stream may occur if there was an error in the file stream creation or
	// write.
	if (!captureObject->IsFileStreamValid())
	{
		delete captureObject;
		captureObject = NULL;

		EMU_PrintMessage("AVI recording ended.");
		driver->AddLine("AVI recording ended.");

		return;
	}

	captureObject->StreamWriteStart();
}

void DRV_AviFileWriteFinish()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	captureObject->StreamWriteFinish();
}

bool AVI_IsRecording()
{
	return (captureObject != NULL);
}

void DRV_AviEnd()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	delete captureObject;
	captureObject = NULL;
}
