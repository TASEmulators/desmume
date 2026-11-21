/*
	Copyright (C) 2009-2026 DeSmuME team

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
#include <commdlg.h>
#include <shellapi.h>

#include <vfw.h>

#include <queue>
#include <string>

#include <rthreads/rthreads.h>
#include <rthreads/rsemaphore.h>

#include "GPU.h"
#include "SPU.h"
#include "utils/task.h"

#include "video_writer.h"
#include "mf_build.h"

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
class MFVideoWriter;
class RecordCompositor;

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

// The part of a recording both writers share: the frame queue, the semaphore
// that pushes back on the emulation thread when the writer falls behind, and
// WriteAllFrames() as the body of the file write thread.
//
// A derived class only has to open whatever it writes into and turn one
// queued frame into one written frame.  The queue itself is writer agnostic,
// which is what lets the capture path in NDSCaptureObject stay untouched
// whichever backend the user picked.
class VideoSink
{
protected:
	bool _valid;

	ssem_t *_semQueue;
	slock_t *_mutexQueue;
	AVIWriteQueue _writeQueue;

	VideoSink();

public:
	virtual ~VideoSink();

	// Creates the file and everything the writer needs before the first frame
	// arrives.  bmpFormat is NULL for an audio-only recording, which is how
	// "no video stream" is expressed here.
	virtual HRESULT Open(const char *fileName, const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat, size_t pendingFrameCount) = 0;

	// Drains or discards the queue according to theAction, then closes the
	// file.  Safe to call twice.
	void Close(FileStreamCloseAction theAction);
	bool IsValid();

	void QueueAdd(u8 *srcVideo, const size_t videoBufferSize, u8 *srcAudio, const size_t audioBufferSize);
	void QueueWait();
	size_t GetQueueSize();
	HRESULT WriteAllFrames();

	// Only a sink that can stop mid-file has anything to do here.  The AVI
	// writer cannot: VFW has no notion of a paused stream.
	virtual bool CanPause() { return false; }
	virtual void SetPaused(bool pause) {}
	virtual bool IsPaused() { return false; }

protected:
	// Writes one queued frame, video first.  Returning a failure closes the
	// file and ends the recording, which is the behaviour the AVI writer has
	// always had.
	virtual HRESULT WriteOneFrame(const AVIFileWriteParam &param) = 0;

	// Releases the file itself.  Called once by Close().
	virtual void CloseFile() = 0;
};

// One AVI file, or one segment of it, written through Video for Windows.
// The codec is chosen by the AVISaveOptions dialog inside its own copy of
// Windows, so nothing above this class knows about VFW.
class AVIFileStream : public VideoSink
{
protected:
	static bool __needAviLibraryInit;

	PAVIFILE _file;
	PAVISTREAM _stream[2];
	PAVISTREAM _compressedStream[2];
	AVISTREAMINFO _streamInfo[2];

	AVICOMPRESSOPTIONS _compressionOptions[2];
	BITMAPINFOHEADER _bmpFormat;
	WAVEFORMATEX _wavFormat;

	size_t _expectedFrameSize;
	size_t _writtenBytes;
	LONG _writtenVideoFrameCount;
	LONG _writtenAudioSampleCount;

	// Name generation for this file and its segments.  Only the AVI writer
	// splits a recording, so only it needs these.
	char _baseFileName[MAX_PATH];
	char _baseFileNameExt[MAX_PATH];
	size_t _segmentNumber;

	HRESULT _OpenSegment();
	void _CloseStreams();

public:
	AVIFileStream();
	virtual ~AVIFileStream();

	virtual HRESULT Open(const char *fileName, const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat, size_t pendingFrameCount);

	HRESULT FlushVideo(u8 *srcBuffer, const LONG bufferSize);
	HRESULT FlushAudio(u8 *srcBuffer, const LONG bufferSize);

protected:
	virtual HRESULT WriteOneFrame(const AVIFileWriteParam &param);
	virtual void CloseFile();
};

// The Media Foundation side: whatever the recording dialog asked for, muxed by
// an IMFSinkWriter.  Unlike the AVI writer it never splits the file, and it
// can be paused, because the sink writer simply stops receiving samples.
class MFVideoSink : public VideoSink
{
protected:
	MFVideoWriter *_writer;
	RecordParams _params;
	bool _hasVideo;
	bool _hasAudio;

public:
	MFVideoSink(const RecordParams &params);
	virtual ~MFVideoSink();

	virtual HRESULT Open(const char *fileName, const BITMAPINFOHEADER *bmpFormat, const WAVEFORMATEX *wavFormat, size_t pendingFrameCount);

	virtual bool CanPause() { return true; }
	virtual void SetPaused(bool pause);
	virtual bool IsPaused();

protected:
	virtual HRESULT WriteOneFrame(const AVIFileWriteParam &param);
	virtual void CloseFile();
};

class NDSCaptureObject
{
private:
	void __InstanceInit();

protected:
	VideoSink *_fs;
	RecordCompositor *_compositor;

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
	// outputWidth/outputHeight are the finished frame, so the composed mode
	// passes the canvas size and the native mode passes one screen's size
	// doubled.  The compositor, when there is one, is owned by this object.
	NDSCaptureObject(VideoSink *sink, size_t outputWidth, size_t outputHeight, const WAVEFORMATEX *wfex,
	                 RecordCompositor *compositor = NULL);
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
	void ComposeVideoSlice(const VideoConvertParam &param);

	void ReadVideoFrame(const void *srcVideoFrame, const size_t inFrameWidth, const size_t inFrameHeight,
	                    const NDSColorFormat colorFormat, float mainIntensity, float touchIntensity);
	void ReadAudioFrames(const void *srcAudioBuffer, const size_t inSampleCount);

	bool CanPause() { return _fs != NULL && _fs->CanPause(); }
	void SetPaused(bool pause) { if (_fs != NULL) _fs->SetPaused(pause); }
	bool IsPaused() { return _fs != NULL && _fs->IsPaused(); }
};

bool DRV_AviBegin(const char *fileName);
bool DRV_AviBeginEx(const char *fileName, const RecordParams &params);
void DRV_AviEnd();
bool AVI_IsRecording();

void DRV_AviPause(bool pause);
bool DRV_AviIsPaused();
bool DRV_AviCanPause();

void DRV_AviFrameStart();
void DRV_AviVideoUpdate(const NDSDisplayInfo &displayInfo);
void DRV_AviSoundUpdate(void *soundData, const int soundLen);
void DRV_AviFileWriteStart();
void DRV_AviFileWriteFinish();

#endif
