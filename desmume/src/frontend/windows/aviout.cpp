/*
	Copyright (C) 2006-2026 DeSmuME team

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
#include "video.h"
#include "record_compositor.h"

#include "windriver.h"
#include "driver.h"
#include "NDSSystem.h"

#if DESMUME_ENABLE_MF
#include "mfvideowriter.h"
#include "videoencoders.h"
#endif

// The display's own picture settings, read once when a recording starts so the
// composed mode follows the layout the user is looking at.  Declared here
// rather than through display.h, which drags in the whole window class for the
// sake of two variables.
extern VideoInfo video;
extern int ScreenGapColor;


NDSCaptureObject *captureObject = NULL;

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

static void* RunComposeVideoSlice(void *arg)
{
	VideoConvertParam *convertParam = (VideoConvertParam *)arg;
	convertParam->captureObj->ComposeVideoSlice(*convertParam);

	return NULL;
}

static void* RunAviFileWrite(void *arg)
{
	VideoSink *fs = (VideoSink *)arg;
	fs->WriteAllFrames();

	return NULL;
}

// The file name arrives from the ANSI save dialog; everything Media
// Foundation takes is UTF-16.
#if DESMUME_ENABLE_MF
static std::wstring WidenFileName(const char *name)
{
	wchar_t buffer[MAX_PATH];
	buffer[0] = L'\0';

	MultiByteToWideChar(CP_ACP, 0, name, -1, buffer, MAX_PATH - 1);

	return std::wstring(buffer);
}
#endif

// --- VideoSink -------------------------------------------------------------

VideoSink::VideoSink()
{
	this->_valid = false;

	this->_semQueue = NULL;
	this->_mutexQueue = slock_new();
}

VideoSink::~VideoSink()
{
	slock_free(this->_mutexQueue);
	ssem_free(this->_semQueue);
}

void VideoSink::Close(FileStreamCloseAction theAction)
{
	switch (theAction)
	{
		case FSCA_PurgeQueue:
		{
			AVIWriteQueue clearQueue;

			slock_lock(this->_mutexQueue);
			this->_writeQueue.swap(clearQueue);
			slock_unlock(this->_mutexQueue);

			if (this->_semQueue != NULL)
			{
				const size_t remainingFrameCount = (size_t)ssem_get(this->_semQueue);
				for (size_t i = 0; i < remainingFrameCount; i++)
				{
					ssem_signal(this->_semQueue);
				}
			}
			break;
		}

		case FSCA_WriteRemainingInQueue:
		{
			if (this->_valid)
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
					if (!this->_writeQueue.empty())
					{
						this->_writeQueue.pop();
					}
					slock_unlock(this->_mutexQueue);

					if (this->_semQueue != NULL)
					{
						ssem_signal(this->_semQueue);
					}

				} while (true);
			}
			break;
		}

		case FSCA_DoNothing:
		default:
			break;
	}

	this->CloseFile();
	this->_valid = false;
}

bool VideoSink::IsValid()
{
	return this->_valid;
}

void VideoSink::QueueAdd(u8 *srcVideo, const size_t videoBufferSize, u8 *srcAudio, const size_t audioBufferSize)
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

void VideoSink::QueueWait()
{
	// If the queue if full, the ssem_wait() will force a wait until the current frame is finished writing.
	ssem_wait(this->_semQueue);
	ssem_signal(this->_semQueue);
}

size_t VideoSink::GetQueueSize()
{
	slock_lock(this->_mutexQueue);
	const size_t queueSize = this->_writeQueue.size();
	slock_unlock(this->_mutexQueue);

	return queueSize;
}

HRESULT VideoSink::WriteAllFrames()
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
		if (!this->_writeQueue.empty())
		{
			this->_writeQueue.pop();
		}
		slock_unlock(this->_mutexQueue);

		if (this->_semQueue != NULL)
		{
			ssem_signal(this->_semQueue);
		}

		if (FAILED(error))
		{
			// Nothing else is going to succeed on this file, so drop what is
			// still queued instead of letting the emulation thread wait on a
			// semaphore nobody signals.
			this->Close(FSCA_PurgeQueue);
			return error;
		}

	} while (true);

	return error;
}

// --- AVIFileStream ---------------------------------------------------------

bool AVIFileStream::__needAviLibraryInit = true;

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
}

AVIFileStream::~AVIFileStream()
{
	this->Close(FSCA_WriteRemainingInQueue);
}

HRESULT AVIFileStream::Open(const char *fileName, const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat, size_t pendingFrameCount)
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

	this->_semQueue = ssem_new(pendingFrameCount);

	error = this->_OpenSegment();
	if (FAILED(error))
	{
		return error;
	}

	this->_valid = true;

	return error;
}

HRESULT AVIFileStream::_OpenSegment()
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

void AVIFileStream::CloseFile()
{
	this->_CloseStreams();

	if (this->_file != NULL)
	{
		AVIFileClose(this->_file);
		this->_file = NULL;
	}
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
		return error;
	}

	error = this->FlushAudio(param.srcAudio, param.audioBufferSize);
	if (FAILED(error))
	{
		return error;
	}

	// Create a new AVI segment if the next written frame would exceed the maximum AVI file size.
	const size_t futureFrameSize = this->_writtenBytes + this->_expectedFrameSize;
	if (futureFrameSize >= MAX_AVI_FILE_SIZE)
	{
		PAVIFILE oldFile = this->_file;

		this->_CloseStreams();
		this->_segmentNumber++;

		error = this->_OpenSegment();
		AVIFileClose(oldFile);

		if (FAILED(error))
		{
			EMU_PrintError("Error creating new AVI segment.");
			return error;
		}
	}

	return error;
}

// --- MFVideoSink -----------------------------------------------------------

#if DESMUME_ENABLE_MF

MFVideoSink::MFVideoSink(const RecordParams &params)
{
	this->_writer = NULL;
	this->_params = params;
	this->_hasVideo = false;
	this->_hasAudio = false;

	// The capture pipeline already owns the thread that calls into a sink, and
	// the sink writer has to be created on the thread that writes to it.  So
	// the encoder always gets a worker of its own: AddFrame() then only copies
	// into a free slot, and the capture writer thread never blocks on it.
	this->_params.async = true;
}

MFVideoSink::~MFVideoSink()
{
	this->Close(FSCA_WriteRemainingInQueue);

	delete this->_writer;
	this->_writer = NULL;
}

HRESULT MFVideoSink::Open(const char *fileName, const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat, size_t pendingFrameCount)
{
	if (fileName == NULL)
	{
		return E_INVALIDARG;
	}

	BITMAPINFOHEADER bi;
	memset(&bi, 0, sizeof(bi));
	if (bmpFormat != NULL)
	{
		bi = *bmpFormat;
	}

	WAVEFORMATEX wf;
	memset(&wf, 0, sizeof(wf));
	if (wavFormat != NULL)
	{
		wf = *wavFormat;
	}

	this->_hasVideo = (bi.biWidth > 0 && bi.biHeight > 0);
	this->_hasAudio = (wf.nBlockAlign > 0 && wf.nSamplesPerSec > 0);

	if (!this->_hasVideo && !this->_hasAudio)
	{
		return E_INVALIDARG;
	}

	this->_writer = new MFVideoWriter(this->_params);

	if (this->_hasVideo)
	{
		this->_writer->SetVideoFormat(&bi);
	}

	if (this->_hasAudio)
	{
		this->_writer->SetSoundFormat(&wf);
	}

	const std::wstring wideName = WidenFileName(fileName);
	if (wideName.empty() || !this->_writer->Open(wideName.c_str()))
	{
		delete this->_writer;
		this->_writer = NULL;
		return E_FAIL;
	}

	// The queue is only used to hand frames from the emulation thread to the
	// capture writer thread, so it needs no headroom beyond the ring buffer
	// NDSCaptureObject already keeps.
	this->_semQueue = ssem_new(pendingFrameCount);

	this->_valid = true;

	return S_OK;
}

void MFVideoSink::SetPaused(bool pause)
{
	if (this->_writer != NULL)
	{
		this->_writer->Pause(pause);
	}
}

bool MFVideoSink::IsPaused()
{
	return (this->_writer != NULL) && this->_writer->IsPaused();
}

HRESULT MFVideoSink::WriteOneFrame(const AVIFileWriteParam &param)
{
	if (this->_writer == NULL)
	{
		return E_UNEXPECTED;
	}

	if (this->_hasVideo && !this->_writer->AddFrame(param.srcVideo))
	{
		return E_FAIL;
	}

	if (this->_hasAudio && param.audioBufferSize > 0 &&
	    !this->_writer->AddSound(param.srcAudio, (u32)param.audioBufferSize))
	{
		return E_FAIL;
	}

	return S_OK;
}

void MFVideoSink::CloseFile()
{
	// The writer's destructor is what stops the worker thread, and that thread
	// is the one that calls Finalize().  Dropping it here also drops any frame
	// the encoder refused.
	delete this->_writer;
	this->_writer = NULL;
}

#endif // DESMUME_ENABLE_MF

// --- NDSCaptureObject ------------------------------------------------------

NDSCaptureObject::NDSCaptureObject(VideoSink *sink, size_t outputWidth, size_t outputHeight, const WAVEFORMATEX *wfex,
                                   RecordCompositor *compositor)
{
	_fs = sink;
	_compositor = compositor;

	__InstanceInit();

	_bmpFormat.biWidth = outputWidth;
	_bmpFormat.biHeight = outputHeight;
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

void NDSCaptureObject::__InstanceInit()
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

	_fileWriteThread = new Task();
	_fileWriteThread->start(false);
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
	delete this->_compositor;
	this->_compositor = NULL;

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
	return this->_fs->Open(fileName, &this->_bmpFormat, &this->_wavFormat, this->_pendingBufferCount);
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
		ColorspaceConvertBuffer555xTo888<true, false>(src, dst, param.frameWidth);
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
		ColorspaceConvertBuffer888xTo888<true, false>(src, dst, param.frameWidth);
		src += param.frameWidth;
		dst -= param.frameWidth * 3;
	}
}

void NDSCaptureObject::ComposeVideoSlice(const VideoConvertParam &param)
{
	if (this->_compositor != NULL)
	{
		this->_compositor->ComposeRows(param.dst, param.firstLineIndex, param.lastLineIndex);
	}
}

void NDSCaptureObject::ReadVideoFrame(const void *srcVideoFrame, const size_t inFrameWidth, const size_t inFrameHeight,
                                      const NDSColorFormat colorFormat, float mainIntensity, float touchIntensity)
{
	if (this->_bmpFormat.biSizeImage == 0)
	{
		return;
	}

	const size_t bufferIndex = this->_currentBufferIndex;
	u8 *convertBufferHead = this->_pendingVideoBuffer + (this->_bmpFormat.biSizeImage * bufferIndex);

	// Composed mode: the frame the writer gets is laid out, filtered and sized
	// by the compositor, so the row bands here are bands of the *output*, not
	// of the GPU buffer.
	if (this->_compositor != NULL)
	{
		// A GPU scale change mid-recording would change the source behind the
		// compositor's back; dropping the frame is what the native path does
		// with the same condition.
		if (inFrameWidth != this->_compositor->SourceWidth() ||
			inFrameHeight != this->_compositor->SourceHeight())
		{
			return;
		}

		this->_compositor->BeginFrame(srcVideoFrame, colorFormat, mainIntensity, touchIntensity);

		if (this->_numThreads == 0)
		{
			this->_convertParam[0].src = (void *)srcVideoFrame;
			this->_convertParam[0].dst = convertBufferHead;
			this->ComposeVideoSlice(this->_convertParam[0]);
		}
		else
		{
			for (size_t i = 0; i < this->_numThreads; i++)
			{
				this->_convertThread[i]->finish();
				this->_convertParam[i].src = (void *)srcVideoFrame;
				this->_convertParam[i].dst = convertBufferHead;
				this->_convertThread[i]->execute(&RunComposeVideoSlice, &this->_convertParam[i]);
			}
		}

		return;
	}

	//dont do anything if prescale has changed, it's just going to be garbage
	if ((this->_bmpFormat.biWidth != inFrameWidth) ||
		(this->_bmpFormat.biHeight != (inFrameHeight * 2)))
	{
		return;
	}

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

// --- driver entry points ---------------------------------------------------

static WAVEFORMATEX _RecordingSoundFormat()
{
	WAVEFORMATEX wf;
	memset(&wf, 0, sizeof(wf));
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nAvgBytesPerSec = DESMUME_SAMPLE_RATE * sizeof(u16) * 2;
	wf.nBlockAlign = 4;
	wf.nChannels = 2;
	wf.nSamplesPerSec = DESMUME_SAMPLE_RATE;
	wf.wBitsPerSample = 16;
	wf.cbSize = sizeof(WAVEFORMATEX);

	return wf;
}

static HRESULT _BeginCapture(VideoSink *sink, const char *fileName, size_t width, size_t height, bool hasAudio,
                             RecordCompositor *compositor = NULL)
{
	const WAVEFORMATEX wf = _RecordingSoundFormat();

	NDSCaptureObject *newCaptureObject = new NDSCaptureObject(sink, width, height, hasAudio ? &wf : NULL, compositor);

	HRESULT error = newCaptureObject->OpenFileStream(fileName);
	if (FAILED(error))
	{
		delete newCaptureObject;
		return error;
	}

	captureObject = newCaptureObject;
	return S_OK;
}

bool DRV_AviBegin(const char *fileName)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();

	const HRESULT error = _BeginCapture(new AVIFileStream, fileName, dispInfo.customWidth, dispInfo.customHeight * 2, true);
	if (FAILED(error))
	{
		// E_ABORT is the user closing the codec dialog, which is not a failure.
		if (error != E_ABORT)
		{
			EMU_PrintError("Error starting AVI file.");
			driver->AddLine("Error starting AVI file.");
		}

		return false;
	}

	return true;
}

// Fills in everything the compositor needs from the picture settings the user
// is looking at right now.  Called once, when the recording starts: a layout or
// a rotation changed mid-recording cannot be allowed to change the frame size,
// because the encoder has already been told what it is.
static void _FillPicture(RecordPicture *picture, const RecordParams &params, const NDSDisplayInfo &dispInfo)
{
	memset(picture, 0, sizeof(*picture));

	picture->mode   = params.pictureMode;
	picture->size   = params.pictureSize;
	picture->width  = params.pictureWidth;
	picture->height = params.pictureHeight;
	picture->layout	 = video.layout;
	picture->rotation = video.rotation;
	picture->screengap = video.screengap;
	picture->swap	 = video.swap;
	picture->mainScreenIsEngineMain = (dispInfo.engineID[NDSDisplayID_Main] == GPUEngineID_Main);
	picture->filter	  = params.pictureFilter;
	picture->gapColor = ScreenGapColor;
}

bool DRV_AviBeginEx(const char *fileName, const RecordParams &params)
{
#if DESMUME_ENABLE_MF
	const bool hasVideo = !videoEncoders::isNullGuid(params.videoSubtype);
	const bool hasAudio = !videoEncoders::isNullGuid(params.audioSubtype);

	if (fileName == NULL || (!hasVideo && !hasAudio))
	{
		return false;
	}

	// An audio-only recording passes a size of zero, which is what tells the
	// capture object and the sink that there is no video stream to set up.
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();

	size_t width  = hasVideo ? dispInfo.customWidth : 0;
	size_t height = hasVideo ? (dispInfo.customHeight * 2) : 0;

	RecordCompositor *compositor = NULL;

	if (hasVideo && params.pictureMode == RECORD_PICTURE_COMPOSED)
	{
		RecordPicture picture;
		_FillPicture(&picture, params, dispInfo);

		compositor = new RecordCompositor();

		if (compositor->Configure(picture, dispInfo.customWidth, dispInfo.customHeight))
		{
			width  = compositor->OutputWidth();
			height = compositor->OutputHeight();
		}
		else
		{
			// The picture could not be composed; the stacked frame is still
			// recordable, so the recording goes ahead in that shape instead of
			// failing outright.
			delete compositor;
			compositor = NULL;

			EMU_PrintMessage("Could not compose the window picture; recording the two screens stacked instead.");
		}
	}

	if (SUCCEEDED(_BeginCapture(new MFVideoSink(params), fileName, width, height, hasAudio, compositor)))
	{
		return true;
	}

	EMU_PrintError("Error starting the recording file.");
	driver->AddLine("Error starting the recording file.");

	return false;
#else
	// This build has no Media Foundation headers, so the settings the caller
	// prepared cannot be honoured; the VFW writer is what it has.
	(void)params;
	return DRV_AviBegin(fileName);
#endif
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

	captureObject->ReadVideoFrame(displayInfo.masterCustomBuffer, displayInfo.customWidth, displayInfo.customHeight,
	                              displayInfo.colorFormat,
	                              displayInfo.backlightIntensity[NDSDisplayID_Main],
	                              displayInfo.backlightIntensity[NDSDisplayID_Touch]);
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

		EMU_PrintMessage("Recording ended.");
		driver->AddLine("Recording ended.");

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

void DRV_AviPause(bool pause)
{
	if (!AVI_IsRecording())
	{
		return;
	}

	captureObject->SetPaused(pause);
}

bool DRV_AviIsPaused()
{
	return AVI_IsRecording() && captureObject->IsPaused();
}

bool DRV_AviCanPause()
{
	return AVI_IsRecording() && captureObject->CanPause();
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
