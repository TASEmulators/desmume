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

#include <windows.h>
#include <vfw.h>

#include "GPU.h"
#include "SPU.h"
#include "utils/task.h"

#define VIDEO_STREAM				0
#define AUDIO_STREAM				1
#define MAX_CONVERT_THREADS			32

#define AUDIO_STREAM_BUFFER_SIZE	((DESMUME_SAMPLE_RATE * sizeof(u16) * 2) / 30)		// 16-bit samples, 2 channels, 2 frames (need only 1 frame's worth, but have 2 frame's worth for safety)
#define MAX_AVI_FILE_SIZE			(2ULL * 1024ULL * 1024ULL * 1024ULL)				// Max file size should be 2 GB due to the Video for Windows AVI limit.
#define PENDING_BUFFER_COUNT		2													// Number of pending buffers to maintain for file writes. Each pending buffer will store 1 frame's worth of the audio/video streams.

class NDSCaptureObject;
class AVIFileStream;

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
	AVIFileStream *fs;
	u8 *srcVideo;
	u8 *srcAudio;
	size_t videoBufferSize;
	size_t audioBufferSize;
};
typedef struct AVIFileWriteParam AVIFileWriteParam;

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

public:
	AVIFileStream();
	~AVIFileStream();

	HRESULT Open(const char *fileName, BITMAPINFOHEADER *bmpFormat, WAVEFORMATEX *wavFormat);
	void Close();
	bool IsValid();

	HRESULT FlushVideo(u8 *srcBuffer, const LONG bufferSize);
	HRESULT FlushAudio(u8 *srcBuffer, const LONG bufferSize);
	HRESULT WriteOneFrame(u8 *srcVideo, const LONG videoBufferSize, u8 *srcAudio, const LONG audioBufferSize);
};

class NDSCaptureObject
{
protected:
	AVIFileStream *_fs;

	BITMAPINFOHEADER _bmpFormat;
	WAVEFORMATEX _wavFormat;

	u8 *_pendingVideoBuffer;
	u8 _pendingAudioBuffer[AUDIO_STREAM_BUFFER_SIZE * PENDING_BUFFER_COUNT];
	size_t _pendingAudioWriteSize[2];
	size_t _currentBufferIndex;

	size_t _numThreads;
	Task *_fileWriteThread;
	Task *_convertThread[MAX_CONVERT_THREADS];
	VideoConvertParam _convertParam[MAX_CONVERT_THREADS];
	AVIFileWriteParam _fileWriteParam;

public:
	NDSCaptureObject();
	NDSCaptureObject(size_t videoWidth, size_t videoHeight, const WAVEFORMATEX *wfex);
	~NDSCaptureObject();

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
