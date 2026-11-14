/*
	Copyright (C) 2026 DeSmuME team

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

// MFVideoWriter.cpp : the Media Foundation side of recording.
//
// One IMFSinkWriter does the whole job: it picks the encoder MFT, inserts a
// colour converter when the encoder wants NV12 and the input is RGB24 or RGB32,
// interleaves the audio, and muxes the container.  The file extension passed
// to MFCreateSinkWriterFromURL is what decides the container.
//
// The frame arrives as the 24bpp bottom-up BGR buffer the capture path already
// produces for the AVI writer.  It goes straight to the sink writer as RGB24
// when its stride meets MF's alignment requirement; otherwise it is converted
// to top-down RGB32 here so the capture path need not know which writer it
// feeds.
//
// Two ways to run it.  Synchronously, the frame is converted and handed to
// WriteSample on the caller's thread -- which is the emulation thread, so any
// time the encoder makes it wait is time the emulator is not running.  That
// wait is real even on a GPU encoder: WriteSample blocks until the encoder
// accepts the sample, and its surface pool is small.  Asynchronously the
// caller only converts into a free slot and signals a worker thread, and that
// thread is the one that waits.
//
// DeSmuME always takes the asynchronous path: the writer has to be created on
// the thread that calls it (COM apartment affinity), and in this frontend the
// only thread that touches frames is the capture writer thread, which must not
// stop for an encoder.  aviout.cpp forces params.async for that reason; the
// synchronous path is kept so the class stays the same shape as its source.

// Declarations only: _WIN32_WINNT decides which SDK prototypes this file can
// see, not what the exe links against.
#define _WIN32_WINNT 0x0A00
#define WINVER 0x0A00

#include <vector>

#include "mflite.h"
#include "mfvideowriter.h"
#include "videoencoders.h"

namespace
{
	const UINT32 kAudioChannels	  = 2;
	const UINT32 kAudioBits		  = 16;
	const UINT32 kAudioOutputRate = 44100;

	// The rate the DS LCD refreshes at, and the one the AVI writer stamps its
	// stream with: 33513982 / (6 * 355 * 263).  Keeping the same numbers here
	// means a frame recorded through either writer has the same duration.
	const UINT32 kFrameRateNumerator   = 33513982;
	const UINT32 kFrameRateDenominator = 560190;

	LONGLONG framesToTime(int frames, UINT32 numerator, UINT32 denominator)
	{
		return (LONGLONG)frames * 10000000LL * denominator / numerator;
	}

	// The frame is 8-bit RGB, so it is full range by definition and there is
	// nothing to gain by squeezing it into 16..235 first.
	//
	// Left alone, the colour converter the sink writer inserts picks BT.601
	// *limited* range for an RGB32 input, and neither the converter nor the
	// encoders then describe the result: no VUI colour description in the
	// bitstream, no colr box in the MP4.  Every player is left to guess, and
	// they do not guess alike -- PotPlayer's D3D11 renderer, for one, reads
	// the file as something other than what was written, which shows up as
	// blown-out colour.
	//
	// So state it: full range, and the matrix the converter actually used.
	//
	// The matrix is BT.601 because that is what Media Foundation's colour
	// converter does for an RGB input; MF_MT_YUV_MATRIX is ignored on both
	// types (measured with 8 flat RGB bars: BT.601 fits the result to within
	// 0.3 of a code value, BT.709 is off by 9).  Claiming BT.709 here would be
	// worse than saying nothing, because a player that believes the tag would
	// then apply the wrong matrix.  The primaries and transfer stay BT.709:
	// the frame comes from an sRGB-ish RGB buffer, only the YCbCr matrix is the
	// 601 one -- the usual combination for this content.
	//
	// The same values go on the input type (where they tell the converter what
	// the RGB means) and on the output type (where they tell the encoder what
	// to produce and what to write into the VUI).
	void setColorAttributes(IMFMediaType *type, bool fullRange)
	{
		type->SetUINT32(MF_MT_YUV_MATRIX, MFVideoTransferMatrix_BT601);
		type->SetUINT32(MF_MT_VIDEO_PRIMARIES, MFVideoPrimaries_BT709);
		type->SetUINT32(MF_MT_TRANSFER_FUNCTION, MFVideoTransFunc_709);
		type->SetUINT32(MF_MT_VIDEO_NOMINAL_RANGE,
		                fullRange ? MFNominalRange_0_255 : MFNominalRange_16_235);
	}

	// A transform that will not take the colour description is not a reason to
	// refuse to record: an untagged file is exactly what this writer produced
	// before, and it still plays.
	void clearColorAttributes(IMFMediaType *type)
	{
		type->DeleteItem(MF_MT_YUV_MATRIX);
		type->DeleteItem(MF_MT_VIDEO_PRIMARIES);
		type->DeleteItem(MF_MT_TRANSFER_FUNCTION);
		type->DeleteItem(MF_MT_VIDEO_NOMINAL_RANGE);
	}
}

struct MFVideoWriter::Impl
{
	// What to record.
	RecordParams	params;
	BITMAPINFOHEADER bih;
	WAVEFORMATEX	 wfx;

	bool hasVideo = false;
	bool hasAudio = false;
	bool paused	  = false;

	// Which input type the sink writer took.  BGR24 is the frame as the
	// emulator produces it, no conversion; RGB32 is the fallback and costs
	// one pass over every pixel.
	bool inputBgr24 = false;

	size_t frameBytes() const
	{
		return (size_t)bih.biWidth * (size_t)bih.biHeight * (inputBgr24 ? 3 : 4);
	}

	// The two pieces of state that cross a thread boundary without the queue
	// behind them.  Both are written by one side and read by the other, so they
	// go through Interlocked instead of the critical section -- a lock around a
	// single bool is not worth the two system calls.
	LONG failed = 0;	// a Media Foundation call went wrong
	LONG initOk = 0;	// the writer is up; the streams can no longer change

	// Media Foundation, owned by whichever thread created it.
	IMFSinkWriter *writer		   = NULL;
	DWORD		   videoStream	   = 0;
	DWORD		   audioStream	   = 0;
	UINT32		   fpsNumerator	   = 1;
	UINT32		   fpsDenominator  = 1;
	UINT32		   frameStep	   = 1;
	UINT32		   inputFrames	   = 0;
	bool		   platformUp	   = false;
	mf::ComApartment *com		   = NULL;
	std::wstring   filename;

	// Timestamps are computed by the caller, so that dropping a frame still
	// moves the clock and the duration of the file stays right.
	int		 submittedFrames = 0;
	LONGLONG audioSamples	 = 0;

	// Synchronous path: one conversion buffer.
	u8 *   stage	 = NULL;
	size_t stageSize = 0;

	// Asynchronous path.
	static const int kSlots			 = 3;
	static const int kMaxAudioBlocks = 512;

	HANDLE			 thread	 = NULL;
	HANDLE			 wake	 = NULL;	// auto reset: "there is work"
	HANDLE			 ready	 = NULL;	// manual reset: "the writer is up"
	CRITICAL_SECTION lock;
	bool			 lockInit = false;
	bool			 stopping = false;	// guarded by lock: it is read together with the queues

	u8 *	 slotPixels[kSlots];
	size_t	 slotCapacity[kSlots];
	LONGLONG slotTime[kSlots];
	bool	 slotBusy[kSlots];
	std::vector<int> slotQueue;	   // the order the frames arrived in

	struct AudioBlock
	{
		u8 *	 data;
		u32		 len;
		LONGLONG time;
		LONGLONG duration;
	};
	std::vector<AudioBlock> audioQueue;

	Impl();
	~Impl();

	bool flagSet(LONG *flag) { return InterlockedCompareExchange(flag, 0, 0) != 0; }
	void flagSetTo(LONG *flag, bool value) { InterlockedExchange(flag, value ? 1 : 0); }

	bool hasFailed() { return flagSet(&failed); }
	void fail() { flagSetTo(&failed, true); }
	bool opened() { return flagSet(&initOk); }

	static DWORD WINAPI threadEntry(LPVOID param);
	DWORD				run();

	bool createWriter();
	void releaseWriter();
	bool createVideoStream();
	bool createAudioStream();
	bool setInputType(const GUID &subtype, LONG stride);

	bool openSync(const wchar_t *name);
	bool openAsync(const wchar_t *name);
	void stopAsync();
	void release();

	void convertFrame(const u8 *bgr24BottomUp, u8 *destination);
	void writeVideoSample(const u8 *pixels, LONGLONG time);
	void writeAudioSample(const u8 *data, u32 len, LONGLONG time, LONGLONG duration);
};

MFVideoWriter::Impl::Impl()
{
	ZeroMemory(&bih, sizeof(bih));
	ZeroMemory(&wfx, sizeof(wfx));
	ZeroMemory(slotPixels, sizeof(slotPixels));
	ZeroMemory(slotCapacity, sizeof(slotCapacity));
	ZeroMemory(slotTime, sizeof(slotTime));
	ZeroMemory(slotBusy, sizeof(slotBusy));
}

MFVideoWriter::Impl::~Impl()
{
	if (params.async)
		stopAsync();
	else
		release();
}

// --- building the sink writer ----------------------------------------------

bool MFVideoWriter::Impl::createWriter()
{
	fpsNumerator   = kFrameRateNumerator;
	fpsDenominator = kFrameRateDenominator;

	// The dialog already decided how many frames to skip; the stream keeps the
	// DS rate as its base and stretches each frame's duration by the step, so
	// the file lasts exactly as long as the recording did.
	frameStep = (params.frameStep > 0) ? (UINT32)params.frameStep : 1;
	fpsDenominator *= frameStep;

	IMFAttributes *attributes = NULL;
	if (FAILED(mf::createAttributes(&attributes, 4)))
		return false;

	// This is the only thing that reaches the sink writer about hardware:
	// there is no way to ask it for a particular MFT.
	attributes->SetUINT32(MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS,
	                      params.videoHardware ? TRUE : FALSE);

	HRESULT hr = mf::createSinkWriterFromURL(filename.c_str(), NULL, attributes, &writer);
	attributes->Release();

	if (FAILED(hr))
		return false;

	if (hasVideo && !createVideoStream())
		return false;

	if (hasAudio && !createAudioStream())
		return false;

	return SUCCEEDED(writer->BeginWriting());
}

void MFVideoWriter::Impl::releaseWriter()
{
	if (writer != NULL)
	{
		// Finalize() is what writes the file index; without it the file is
		// unplayable.  It can take a while, but there is nothing to do about
		// that here -- the caller sees it as the stop command taking a moment.
		writer->Finalize();
		writer->Release();
		writer = NULL;
	}
}

// --- streams ---------------------------------------------------------------

bool MFVideoWriter::Impl::createVideoStream()
{
	IMFMediaType *output = NULL;
	if (FAILED(mf::createMediaType(&output)))
		return false;

	output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	output->SetGUID(MF_MT_SUBTYPE, params.videoSubtype);
	output->SetUINT32(MF_MT_AVG_BITRATE, (UINT32)params.videoBitrate * 1000);
	output->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	MFSetAttributeSize(output, MF_MT_FRAME_SIZE, (UINT32)bih.biWidth, (UINT32)bih.biHeight);
	MFSetAttributeRatio(output, MF_MT_FRAME_RATE, fpsNumerator, fpsDenominator);
	MFSetAttributeRatio(output, MF_MT_PIXEL_ASPECT_RATIO, 1, 1);
	setColorAttributes(output, true);

	// MF_MT_MPEG2_PROFILE is the generic attribute; the value is the codec's
	// own profile number, which is why the dialog keeps one list per codec.
	if (params.profile != VIDEO_PROFILE_AUTO)
		output->SetUINT32(MF_MT_MPEG2_PROFILE, (UINT32)params.profile);

	HRESULT hr = writer->AddStream(output, &videoStream);
	if (FAILED(hr))
	{
		clearColorAttributes(output);
		hr = writer->AddStream(output, &videoStream);
	}
	output->Release();
	if (FAILED(hr))
		return false;

	// The capture path hands over 24bpp bottom-up BGR, which is exactly what
	// MFVideoFormat_RGB24 means: the name counts channels, the bytes in memory
	// are B, G, R.  With a negative stride saying the rows run bottom-up, the
	// frame goes to the sink writer the way it arrives, and Media Foundation
	// does the 4:2:0 conversion in a single pass.  Expanding every pixel to
	// 32 bits first (4.3 ms at 2400x1600) and then copying that buffer again
	// (1.0 ms) is work nobody needs.
	//
	// RGB24 is the one input format with a constraint the 32-bit ones do not
	// have: the stride must be a multiple of 4.  A buffer that ignores this
	// still passes SetInputMediaType and only fails later, in WriteSample,
	// when it is too late to fall back -- so the check is made here, and a
	// width that does not fit takes the RGB32 path instead.
	const LONG bgr24Stride = (LONG)bih.biWidth * 3;
	if ((bgr24Stride & 3) == 0 && setInputType(MFVideoFormat_RGB24, -bgr24Stride))
	{
		inputBgr24 = true;
		return true;
	}

	inputBgr24 = false;
	return setInputType(MFVideoFormat_RGB32, (LONG)bih.biWidth * 4);
}

// Hands one input type to the sink writer.  The colour description is dropped
// and the type tried once more if it is refused: an untagged file is what this
// writer produced before and it still plays, while a refusal is a recording
// that never happened.
bool MFVideoWriter::Impl::setInputType(const GUID &subtype, LONG stride)
{
	IMFMediaType *input = NULL;
	if (FAILED(mf::createMediaType(&input)))
		return false;

	input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video);
	input->SetGUID(MF_MT_SUBTYPE, subtype);
	input->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_Progressive);
	input->SetUINT32(MF_MT_DEFAULT_STRIDE, (UINT32)stride);
	MFSetAttributeSize(input, MF_MT_FRAME_SIZE, (UINT32)bih.biWidth, (UINT32)bih.biHeight);
	MFSetAttributeRatio(input, MF_MT_FRAME_RATE, fpsNumerator, fpsDenominator);
	setColorAttributes(input, true);

	HRESULT hr = writer->SetInputMediaType(videoStream, input, NULL);
	if (FAILED(hr))
	{
		clearColorAttributes(input);
		hr = writer->SetInputMediaType(videoStream, input, NULL);
	}
	input->Release();
	return SUCCEEDED(hr);
}

bool MFVideoWriter::Impl::createAudioStream()
{
	const bool aac		= IsEqualGUID(params.audioSubtype, MFAudioFormat_AAC) != FALSE;
	const bool lossless = aac || IsEqualGUID(params.audioSubtype, MFAudioFormat_FLAC) != FALSE ||
	                      IsEqualGUID(params.audioSubtype, MFAudioFormat_ALAC) != FALSE;

	IMFMediaType *output = NULL;
	if (FAILED(mf::createMediaType(&output)))
		return false;

	output->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	output->SetGUID(MF_MT_SUBTYPE, params.audioSubtype);
	output->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, kAudioChannels);
	output->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, kAudioOutputRate);
	output->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, (UINT32)params.audioBitrate * 1000 / 8);

	// The lossless encoders describe their output in bits per sample; MP3,
	// Opus and AC-3 have no such notion and reject an output type that
	// carries one.
	if (lossless)
		output->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, kAudioBits);

	if (aac)
	{
		output->SetUINT32(MF_MT_AAC_PAYLOAD_TYPE, 0);
		output->SetUINT32(MF_MT_AAC_AUDIO_PROFILE_LEVEL_INDICATION, 0x29);
	}

	HRESULT hr = writer->AddStream(output, &audioStream);
	output->Release();
	if (FAILED(hr))
		return false;

	// The emulator hands over the samples at the SPU's rate; the sink writer
	// inserts its own resampler when that differs from the encoder's rate.
	IMFMediaType *input = NULL;
	if (FAILED(mf::createMediaType(&input)))
		return false;

	input->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
	input->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
	input->SetUINT32(MF_MT_AUDIO_NUM_CHANNELS, wfx.nChannels);
	input->SetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, wfx.nSamplesPerSec);
	input->SetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, wfx.wBitsPerSample);
	input->SetUINT32(MF_MT_AUDIO_BLOCK_ALIGNMENT, wfx.nBlockAlign);
	input->SetUINT32(MF_MT_AUDIO_AVG_BYTES_PER_SECOND, wfx.nAvgBytesPerSec);
	input->SetUINT32(MF_MT_ALL_SAMPLES_INDEPENDENT, TRUE);

	hr = writer->SetInputMediaType(audioStream, input, NULL);
	input->Release();
	return SUCCEEDED(hr);
}

// --- opening ---------------------------------------------------------------

bool MFVideoWriter::Impl::openSync(const wchar_t *name)
{
	filename = name;

	com = new mf::ComApartment(false);
	if (!com->ok())
		return false;

	if (!mf::startup())
		return false;
	platformUp = true;

	const bool ok = createWriter();
	flagSetTo(&initOk, ok);
	return ok;
}

bool MFVideoWriter::Impl::openAsync(const wchar_t *name)
{
	filename = name;

	InitializeCriticalSection(&lock);
	lockInit = true;

	wake  = CreateEvent(NULL, FALSE, FALSE, NULL);
	ready = CreateEvent(NULL, TRUE, FALSE, NULL);
	if (wake == NULL || ready == NULL)
		return false;

	thread = CreateThread(NULL, 0, threadEntry, this, 0, NULL);
	if (thread == NULL)
		return false;

	// The writer cannot be built on this thread: it is created on the thread
	// that will call it, and that thread needs its own apartment.
	if (WaitForSingleObject(ready, 15000) != WAIT_OBJECT_0)
		return false;

	// The ready event already ordered the two threads; the flag only has to be
	// read atomically, not under the lock.
	return opened();
}

DWORD WINAPI MFVideoWriter::Impl::threadEntry(LPVOID param)
{
	return ((Impl *)param)->run();
}

DWORD MFVideoWriter::Impl::run()
{
	mf::ComApartment apartment(true);	// an MTA, so no message pump is needed

	bool ok = apartment.ok();
	if (ok)
	{
		ok		   = mf::startup();
		platformUp = ok;
	}
	if (ok)
		ok = createWriter();

	flagSetTo(&initOk, ok);
	SetEvent(ready);

	if (!ok)
	{
		if (platformUp)
		{
			mf::shutdown();
			platformUp = false;
		}
		return 1;
	}

	bool finished = false;
	while (!finished)
	{
		WaitForSingleObject(wake, INFINITE);

		for (;;)
		{
			int		  slot	   = -1;
			bool	  hasAudio = false;
			bool	  stop	   = false;
			AudioBlock block;
			ZeroMemory(&block, sizeof(block));

			EnterCriticalSection(&lock);

			// Let the sound catch up when the picture has been keeping the
			// thread busy; the queues are small, so this is rare.
			const bool audioFirst = audioQueue.size() > 8;

			if (audioFirst && !audioQueue.empty())
			{
				block	 = audioQueue.front();
				audioQueue.erase(audioQueue.begin());
				hasAudio = true;
			}
			else if (!slotQueue.empty())
			{
				slot = slotQueue.front();
				slotQueue.erase(slotQueue.begin());
			}
			else if (!audioQueue.empty())
			{
				block	 = audioQueue.front();
				audioQueue.erase(audioQueue.begin());
				hasAudio = true;
			}
			else if (stopping)
			{
				stop = true;
			}

			LeaveCriticalSection(&lock);

			if (stop)
			{
				finished = true;
				break;
			}

			if (slot >= 0)
			{
				writeVideoSample(slotPixels[slot], slotTime[slot]);

				EnterCriticalSection(&lock);
				slotBusy[slot] = false;
				LeaveCriticalSection(&lock);
				continue;
			}

			if (hasAudio)
			{
				writeAudioSample(block.data, block.len, block.time, block.duration);
				free(block.data);
				continue;
			}

			break;
		}
	}

	releaseWriter();

	if (platformUp)
	{
		mf::shutdown();
		platformUp = false;
	}

	return 0;
}

void MFVideoWriter::Impl::stopAsync()
{
	if (thread != NULL)
	{
		// A writer whose Open() never reached CreateThread() has no lock yet.
		if (lockInit)
		{
			EnterCriticalSection(&lock);
			stopping = true;
			LeaveCriticalSection(&lock);
		}

		SetEvent(wake);
		WaitForSingleObject(thread, 30000);
		CloseHandle(thread);
		thread = NULL;
	}

	if (wake != NULL)
	{
		CloseHandle(wake);
		wake = NULL;
	}
	if (ready != NULL)
	{
		CloseHandle(ready);
		ready = NULL;
	}

	// Whatever is still queued was never written.
	for (int i = 0; i < kSlots; ++i)
	{
		delete[] slotPixels[i];
		slotPixels[i]	= NULL;
		slotCapacity[i] = 0;
		slotBusy[i]		= false;
	}
	slotQueue.clear();

	for (size_t i = 0; i < audioQueue.size(); ++i)
		free(audioQueue[i].data);
	audioQueue.clear();

	if (lockInit)
	{
		DeleteCriticalSection(&lock);
		lockInit = false;
	}
}

void MFVideoWriter::Impl::release()
{
	releaseWriter();

	delete[] stage;
	stage	  = NULL;
	stageSize = 0;

	if (platformUp)
	{
		mf::shutdown();
		platformUp = false;
	}

	delete com;
	com = NULL;
}

// --- writing ---------------------------------------------------------------

void MFVideoWriter::Impl::convertFrame(const u8 *bgr24BottomUp, u8 *destination)
{
	// The sink writer already knows the rows are bottom-up (negative stride)
	// and that the pixels are B, G, R, so nothing has to be rearranged.
	if (inputBgr24)
	{
		memcpy(destination, bgr24BottomUp, frameBytes());
		return;
	}

	const int	 width	  = bih.biWidth;
	const int	 height	  = bih.biHeight;
	const size_t srcPitch = (size_t)width * 3;
	const size_t dstPitch = (size_t)width * 4;

	for (int y = 0; y < height; ++y)
	{
		const u8 *src = bgr24BottomUp + srcPitch * (size_t)(height - 1 - y);
		u8 *	  dst = destination + dstPitch * (size_t)y;

		for (int x = 0; x < width; ++x)
		{
			dst[0] = src[0];
			dst[1] = src[1];
			dst[2] = src[2];
			dst[3] = 255;
			src += 3;
			dst += 4;
		}
	}
}

void MFVideoWriter::Impl::writeVideoSample(const u8 *pixels, LONGLONG time)
{
	if (hasFailed() || writer == NULL)
		return;

	const size_t bytes = frameBytes();

	IMFMediaBuffer *buffer = NULL;
	if (FAILED(mf::createMemoryBuffer((DWORD)bytes, &buffer)))
	{
		fail();
		return;
	}

	BYTE *target = NULL;
	if (FAILED(buffer->Lock(&target, NULL, NULL)))
	{
		buffer->Release();
		fail();
		return;
	}

	memcpy(target, pixels, bytes);
	buffer->Unlock();
	buffer->SetCurrentLength((DWORD)bytes);

	IMFSample *sample = NULL;
	if (FAILED(mf::createSample(&sample)))
	{
		buffer->Release();
		fail();
		return;
	}

	sample->AddBuffer(buffer);
	buffer->Release();

	sample->SetSampleTime(time);
	sample->SetSampleDuration(framesToTime(1, fpsNumerator, fpsDenominator));

	if (FAILED(writer->WriteSample(videoStream, sample)))
		fail();

	sample->Release();
}

void MFVideoWriter::Impl::writeAudioSample(const u8 *data, u32 len, LONGLONG time,
                                           LONGLONG duration)
{
	if (hasFailed() || writer == NULL)
		return;

	IMFMediaBuffer *buffer = NULL;
	if (FAILED(mf::createMemoryBuffer(len, &buffer)))
	{
		fail();
		return;
	}

	BYTE *target = NULL;
	if (FAILED(buffer->Lock(&target, NULL, NULL)))
	{
		buffer->Release();
		fail();
		return;
	}

	memcpy(target, data, len);
	buffer->Unlock();
	buffer->SetCurrentLength(len);

	IMFSample *sample = NULL;
	if (FAILED(mf::createSample(&sample)))
	{
		buffer->Release();
		fail();
		return;
	}

	sample->AddBuffer(buffer);
	buffer->Release();

	sample->SetSampleTime(time);
	sample->SetSampleDuration(duration);

	if (FAILED(writer->WriteSample(audioStream, sample)))
		fail();

	sample->Release();
}

// --- the writer seen by the capture path ------------------------------------

MFVideoWriter::MFVideoWriter(const RecordParams &params)
{
	m_impl		   = new Impl();
	m_impl->params = params;
}

MFVideoWriter::~MFVideoWriter()
{
	Impl *impl = m_impl;
	if (impl != NULL)
	{
		delete impl;
		m_impl = NULL;
	}
}

void MFVideoWriter::SetVideoFormat(BITMAPINFOHEADER *bh)
{
	// Force size to 0x28 like the AVI writer does, so a caller that passes a
	// BITMAPV4HEADER does not smuggle extra fields in.
	memcpy(&m_impl->bih, bh, 0x28);
	m_impl->hasVideo = bh->biWidth > 0 && bh->biHeight > 0;
}

void MFVideoWriter::SetSoundFormat(WAVEFORMATEX *format)
{
	// Too late: the streams are fixed at Open().  See IsSoundAdded().
	if (m_impl->opened())
		return;

	if (format == NULL || format->nSamplesPerSec == 0 || format->nBlockAlign == 0)
	{
		m_impl->hasAudio = false;
		return;
	}

	memcpy(&m_impl->wfx, format, sizeof(WAVEFORMATEX));
	m_impl->hasAudio = true;
}

bool MFVideoWriter::Open(const wchar_t *filename)
{
	Impl &s = *m_impl;

	if (!s.hasVideo && !s.hasAudio)
		return false;

	const bool ok = s.params.async ? s.openAsync(filename) : s.openSync(filename);
	if (!ok)
		s.fail();

	return ok;
}

bool MFVideoWriter::AddFrame(const u8 *bgr24BottomUp)
{
	Impl &s = *m_impl;

	if (s.hasFailed())
		return false;
	if (!s.hasVideo || s.paused)
		return true;

	// Recording below the emulator's own rate: write every frameStep-th frame
	// and let the timestamps carry the shorter rate.
	const bool useFrame = (s.inputFrames % s.frameStep) == 0;
	++s.inputFrames;
	if (!useFrame)
		return true;

	// The clock moves on even when the frame below is dropped for want of a
	// free slot, so the duration of the file stays right.
	const LONGLONG time = framesToTime(s.submittedFrames, s.fpsNumerator, s.fpsDenominator);
	++s.submittedFrames;

	const size_t bytes = s.frameBytes();

	if (!s.params.async)
	{
		if (s.stageSize < bytes)
		{
			delete[] s.stage;
			s.stage		= new u8[bytes];
			s.stageSize = bytes;
		}

		s.convertFrame(bgr24BottomUp, s.stage);
		s.writeVideoSample(s.stage, time);
		return !s.hasFailed();
	}

	// Claim a slot, convert into it, then publish it.  The conversion happens
	// outside the lock, which is the whole point: the caller only pays for the
	// copy, never for the encoder.
	EnterCriticalSection(&s.lock);
	int slot = -1;
	for (int i = 0; i < Impl::kSlots; ++i)
	{
		if (!s.slotBusy[i])
		{
			slot		   = i;
			s.slotBusy[i]  = true;
			break;
		}
	}
	LeaveCriticalSection(&s.lock);

	if (slot < 0)
		return true;	// the encoder is behind; drop this frame

	if (s.slotCapacity[slot] < bytes)
	{
		delete[] s.slotPixels[slot];
		s.slotPixels[slot]	 = new u8[bytes];
		s.slotCapacity[slot] = bytes;
	}

	s.convertFrame(bgr24BottomUp, s.slotPixels[slot]);

	EnterCriticalSection(&s.lock);
	s.slotTime[slot] = time;
	s.slotQueue.push_back(slot);
	LeaveCriticalSection(&s.lock);

	SetEvent(s.wake);
	return true;
}

bool MFVideoWriter::AddSound(const u8 *sound, u32 len)
{
	Impl &s = *m_impl;

	if (s.hasFailed())
		return false;
	if (!s.hasAudio || s.paused || len == 0)
		return true;

	const UINT32   blockAlign = s.wfx.nBlockAlign ? s.wfx.nBlockAlign : 4;
	const UINT32   rate		  = s.wfx.nSamplesPerSec ? s.wfx.nSamplesPerSec : 44100;
	const LONGLONG time		  = s.audioSamples * 10000000LL / rate;
	const LONGLONG duration	  = (LONGLONG)(len / blockAlign) * 10000000LL / rate;

	s.audioSamples += len / blockAlign;

	if (!s.params.async)
	{
		s.writeAudioSample(sound, len, time, duration);
		return !s.hasFailed();
	}

	// The sound cannot be dropped the way a frame can: a missing block would
	// shorten the track and push the picture out of sync.  The queue is deep
	// enough that this only waits if the encoder has stopped entirely.
	for (;;)
	{
		EnterCriticalSection(&s.lock);
		const bool room = (int)s.audioQueue.size() < Impl::kMaxAudioBlocks;
		LeaveCriticalSection(&s.lock);

		if (room)
			break;

		// Never wait on a writer that is already dead.
		if (s.hasFailed())
			return false;

		Sleep(1);
	}

	u8 *copy = (u8 *)malloc(len);
	if (copy == NULL)
		return false;
	memcpy(copy, sound, len);

	EnterCriticalSection(&s.lock);
	Impl::AudioBlock block;
	block.data	   = copy;
	block.len	   = len;
	block.time	   = time;
	block.duration = duration;
	s.audioQueue.push_back(block);
	LeaveCriticalSection(&s.lock);

	SetEvent(s.wake);
	return true;
}

int MFVideoWriter::videoWidth() const
{
	return m_impl->bih.biWidth;
}

int MFVideoWriter::videoHeight() const
{
	return m_impl->bih.biHeight;
}

int MFVideoWriter::videoFrames()
{
	return m_impl->submittedFrames;
}

bool MFVideoWriter::IsSoundAdded()
{
	// Once the sink writer exists the audio stream either exists or it does
	// not, and the format can no longer change.  Reporting true here keeps the
	// capture path from handing over a format after Open(), which would
	// otherwise make AddSound() write into the video stream.
	return m_impl->opened();
}

void MFVideoWriter::Pause(bool pause)
{
	m_impl->paused = pause;
}

bool MFVideoWriter::IsPaused()
{
	return m_impl->paused;
}

bool MFVideoWriter::HasFailed()
{
	return m_impl->hasFailed();
}
