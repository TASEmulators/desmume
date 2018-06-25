/*
	Copyright (C) 2009-2018 DeSmuME team

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

#ifndef _AVIOUT_H_
#define _AVIOUT_H_

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Commdlg.h>
#include <Shellapi.h>

#include <vfw.h>

#include <queue>

#include <rthreads/rthreads.h>
#include <rthreads/rsemaphore.h>

#include "GPU.h"
#include "SPU.h"
#include "utils/task.h"

#define VIDEO_STREAM				0
#define AUDIO_STREAM				1
#define MAX_CONVERT_THREADS			32

#define AUDIO_STREAM_BUFFER_SIZE	((DESMUME_SAMPLE_RATE * sizeof(u16) * 2) / 30)		// 16-bit samples, 2 channels, 2 frames (need only 1 frame's worth, but have 2 frame's worth for safety)
#define MAX_AVI_FILE_SIZE			(2ULL * 1024ULL * 1024ULL * 1024ULL)				// Max file size should be 2 GB due to the Video for Windows AVI limit.
#define MAX_PENDING_BUFFER_SIZE		(1536ULL * 1024ULL * 1024ULL)						// Max pending buffer size should not exceed 1.5 GB.
#define MAX_PENDING_FRAME_COUNT		180													// Maintain up to 180 frames in memory for current and future file writes. This is equivalent to 3 seconds worth of frames.


enum FileStreamCloseAction
{
	FSCA_DoNothing				= 0,
	FSCA_PurgeQueue				= 1,
	FSCA_WriteRemainingInQueue	= 2
};

class NDSCaptureObject;

struct VideoConvertParam
{
	NDSCaptureObject *captureObj;

	const void *src;
	u8 *dst;

	size_t srcOffset;
	size_t dstOffset;

	size_t firstLineIndex;
	size_t lastLineIndex;
	size_t frameWidth;
};
typedef struct VideoConvertParam VideoConvertParam;

struct AVIFileWriteParam
{
	u8 *srcVideo;
	u8 *srcAudio;
	size_t videoBufferSize;
	size_t audioBufferSize;
};
typedef struct AVIFileWriteParam AVIFileWriteParam;

typedef std::queue<AVIFileWriteParam> AVIWriteQueue;

class AVIFileStream
{
private:
	static bool __needAviLibraryInit;

protected:
	PAVIFILE _file;
	PAVISTREAM _stream[2];
	PAVISTREAM _compressedStream[2];
	AVISTREAMINFO _streamInfo[2];

	AVICOMPRESSOPTIONS _compressionOptions[2];
	BITMAPINFOHEADER _bmpFormat;
	WAVEFORMATEX _wavFormat;

	char _baseFileName[MAX_PATH];
	char _baseFileNameExt[MAX_PATH];
	size_t _segmentNumber;

	size_t _expectedFrameSize;
	size_t _writtenBytes;
	LONG _writtenVideoFrameCount;
	LONG _writtenAudioSampleCount;

	ssem_t *_semQueue;
	slock_t *_mutexQueue;
	AVIWriteQueue _writeQueue;

	void _CloseStreams();

public:
	AVIFileStream();
	~AVIFileStream();

	HRESULT InitBaseProperties(const char *fileName, BITMAPINFOHEADER *bmpFormat, WAVEFORMATEX *wavFormat, size_t pendingFrameCount);
	HRESULT Open();
	void Close(FileStreamCloseAction theAction);
	bool IsValid();

	void QueueAdd(u8 *srcVideo, const size_t videoBufferSize, u8 *srcAudio, const size_t audioBufferSize);
	void QueueWait();
	size_t GetQueueSize();

	HRESULT FlushVideo(u8 *srcBuffer, const LONG bufferSize);
	HRESULT FlushAudio(u8 *srcBuffer, const LONG bufferSize);
	HRESULT WriteOneFrame(const AVIFileWriteParam &param);
	HRESULT WriteAllFrames();
};

class NDSCaptureObject
{
protected:
	AVIFileStream *_fs;

	BITMAPINFOHEADER _bmpFormat;
	WAVEFORMATEX _wavFormat;

	u8 *_pendingVideoBuffer;
	u8 *_pendingAudioBuffer;
	size_t *_pendingAudioWriteSize;
	size_t _pendingBufferCount;
	size_t _currentBufferIndex;

	size_t _numThreads;
	Task *_fileWriteThread;
	Task *_convertThread[MAX_CONVERT_THREADS];
	VideoConvertParam _convertParam[MAX_CONVERT_THREADS];

public:
	NDSCaptureObject();
	NDSCaptureObject(size_t videoWidth, size_t videoHeight, const WAVEFORMATEX *wfex);
	~NDSCaptureObject();

	static size_t GetExpectedFrameSize(const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat);

	HRESULT OpenFileStream(const char *fileName);
	void CloseFileStream();
	bool IsFileStreamValid();

	void StartFrame();
	void StreamWriteStart();
	void StreamWriteFinish();

	void ConvertVideoSlice555Xto888(const VideoConvertParam &param);
	void ConvertVideoSlice888Xto888(const VideoConvertParam &param);

	void ReadVideoFrame(const void *srcVideoFrame, const size_t inFrameWidth, const size_t inFrameHeight, const NDSColorFormat colorFormat);
	void ReadAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount);
};

bool DRV_AviBegin(const char *fileName);
void DRV_AviEnd();
bool AVI_IsRecording();

void DRV_AviFrameStart();
void DRV_AviVideoUpdate(const NDSDisplayInfo &displayInfo);
void DRV_AviSoundUpdate(void *soundData, const int soundLen);
void DRV_AviFileWriteStart();
void DRV_AviFileWriteFinish();

#endif
