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
#include <windows.h>
#include <vfw.h>

#include "debug.h"
#include "console.h"
#include "gfx3d.h"
#include "SPU.h"

#include "video.h"
#include "windriver.h"
#include "main.h"
#include "driver.h"
#include "NDSSystem.h"
#include "utils/task.h"

extern VideoInfo video;

static void EMU_PrintError(const char* msg) {
	LOG(msg);
}

static void EMU_PrintMessage(const char* msg) {
	LOG(msg);
}

//extern PALETTEENTRY *color_palette;
//extern WAVEFORMATEX wf;
//extern int soundo;

#define VIDEO_STREAM				0
#define AUDIO_STREAM				1
#define MAX_CONVERT_THREADS			32

#define AUDIO_STREAM_BUFFER_SIZE	(DESMUME_SAMPLE_RATE * sizeof(u16) * 2) // 16-bit samples, 2 channels, 1 second duration

struct AVIFile;

struct AVIConversionParam
{
	AVIFile *avi;
	size_t bufferIndex;
	const void *src;
	size_t firstLineIndex;
	size_t lastLineIndex;
};
typedef struct AVIConversionParam AVIConversionParam;

struct AVIFileWriteParam
{
	AVIFile *avi;
	size_t bufferIndex;
};
typedef struct AVIFileWriteParam AVIFileWriteParam;

struct AVIFile
{
	int					fps;
	int					fps_scale;

	int					video_added;
	BITMAPINFOHEADER	bitmap_format;

	int					sound_added;
	WAVEFORMATEX		wave_format;

	AVISTREAMINFO		avi_video_header;
	AVISTREAMINFO		avi_sound_header;
	PAVIFILE			avi_file;
	PAVISTREAM			streams[2];
	PAVISTREAM			compressed_streams[2];

	AVICOMPRESSOPTIONS	compress_options[2];
	AVICOMPRESSOPTIONS*	compress_options_ptr[2];

	int					video_frames;
	int					sound_samples;

	u8					*convert_buffer;
	size_t				videoStreamBufferSize;
	int					prescaleLevel;
	size_t				frameWidth;
	size_t				frameHeight;
	int					start_scanline;
	int					end_scanline;
	
	long				tBytes, ByteBuffer;

	u8					audio_buffer[AUDIO_STREAM_BUFFER_SIZE * 2];
	int					audio_buffer_pos[2];

	size_t				currentBufferIndex;

	size_t				numThreads;
	Task				*fileWriteThread;
	Task				*convertThread[MAX_CONVERT_THREADS];
	AVIConversionParam	convertParam[MAX_CONVERT_THREADS];
	AVIFileWriteParam	fileWriteParam;
};
typedef AVIFile AVIFile;

AVIFile *avi_file = NULL;

struct VideoSystemInfo
{
	int					start_scanline;
	int					end_scanline;
	int					fps;
};


static char saved_cur_avi_fnameandext[MAX_PATH];
static char saved_avi_fname[MAX_PATH];
static char saved_avi_ext[MAX_PATH];
static int avi_segnum=0;
//static FILE* avi_check_file=0;
static struct AVIFile saved_avi_info;
static int use_prev_options=0;
static bool use_sound=false;



static bool truncate_existing(const char* filename)
{
	// this is only here because AVIFileOpen doesn't seem to do it for us
	FILE* fd = fopen(filename, "wb");
	if(fd)
	{
		fclose(fd);
		return 1;
	}

	return 0;
}

static int avi_audiosegment_size(struct AVIFile* avi_out)
{
	if (!AVI_IsRecording() || !avi_out->sound_added)
		return 0;

	assert(avi_out->wave_format.nAvgBytesPerSec <= sizeof(avi_out->audio_buffer));
	return avi_out->wave_format.nAvgBytesPerSec;
}

static void avi_destroy(struct AVIFile** avi_out)
{
	if (!(*avi_out))
		return;

	const size_t bufferIndex = (*avi_out)->currentBufferIndex;
	HRESULT error = S_OK;

	if ((*avi_out)->sound_added)
	{
		if ((*avi_out)->compressed_streams[AUDIO_STREAM])
		{
			if ((*avi_out)->audio_buffer_pos[bufferIndex] > 0)
			{
				const int frameSampleCount = (*avi_out)->audio_buffer_pos[bufferIndex] / (*avi_out)->wave_format.nBlockAlign;

				error = AVIStreamWrite(avi_file->compressed_streams[AUDIO_STREAM],
					avi_file->sound_samples, frameSampleCount,
					(*avi_out)->audio_buffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex), (*avi_out)->audio_buffer_pos[bufferIndex], 0, NULL, &avi_file->ByteBuffer);

				(*avi_out)->sound_samples += frameSampleCount;
				(*avi_out)->tBytes += avi_file->ByteBuffer;
				(*avi_out)->audio_buffer_pos[bufferIndex] = 0;
			}

			LONG test = AVIStreamClose((*avi_out)->compressed_streams[AUDIO_STREAM]);
			(*avi_out)->compressed_streams[AUDIO_STREAM] = NULL;
			(*avi_out)->streams[AUDIO_STREAM] = NULL;				// compressed_streams[AUDIO_STREAM] is just a copy of streams[AUDIO_STREAM]
		}
	}

	if ((*avi_out)->video_added)
	{
		if((*avi_out)->compressed_streams[VIDEO_STREAM])
		{
			AVIStreamClose((*avi_out)->compressed_streams[VIDEO_STREAM]);
			(*avi_out)->compressed_streams[VIDEO_STREAM] = NULL;
		}

		if((*avi_out)->streams[VIDEO_STREAM])
		{
			AVIStreamClose((*avi_out)->streams[VIDEO_STREAM]);
			(*avi_out)->streams[VIDEO_STREAM] = NULL;
		}
	}

	if ((*avi_out)->avi_file)
	{
		AVIFileClose((*avi_out)->avi_file);
		(*avi_out)->avi_file = NULL;
	}
}

static void set_video_format(const BITMAPINFOHEADER* bitmap_format, struct AVIFile* avi_out)
{
	memcpy(&((*avi_out).bitmap_format), bitmap_format, sizeof(BITMAPINFOHEADER));
	(*avi_out).video_added = 1;
}

static void set_sound_format(const WAVEFORMATEX* wave_format, struct AVIFile* avi_out)
{
	memcpy(&((*avi_out).wave_format), wave_format, sizeof(WAVEFORMATEX));
	(*avi_out).sound_added = 1;
}

static bool avi_open(const char* filename, const BITMAPINFOHEADER* pbmih, const WAVEFORMATEX* pwfex, bool isNewSegment)
{
	bool isErrorInFileWrite = false;
	bool result = false;

	do
	{
		if (!truncate_existing(filename))
			break;

		if (!pbmih)
			break;

		if (!isNewSegment)
		{
			avi_file = (struct AVIFile*)malloc(sizeof(struct AVIFile));
			memset(avi_file, 0, sizeof(struct AVIFile));

			avi_file->prescaleLevel = video.prescaleHD;
			avi_file->frameWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH * video.prescaleHD;
			avi_file->frameHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT * video.prescaleHD * 2;
			avi_file->videoStreamBufferSize = avi_file->frameWidth * avi_file->frameHeight * 3;
			avi_file->convert_buffer = (u8*)malloc_alignedCacheLine(avi_file->videoStreamBufferSize * 2);

			// create the video stream
			set_video_format(pbmih, avi_file);

			memset(&avi_file->avi_video_header, 0, sizeof(AVISTREAMINFO));
			avi_file->avi_video_header.fccType = streamtypeVIDEO;
			avi_file->avi_video_header.dwScale = 6 * 355 * 263;
			avi_file->avi_video_header.dwRate = 33513982;
			avi_file->avi_video_header.dwSuggestedBufferSize = avi_file->bitmap_format.biSizeImage;

			// add audio format
			if (pwfex)
			{
				set_sound_format(pwfex, avi_file);

				memset(&avi_file->avi_sound_header, 0, sizeof(AVISTREAMINFO));
				avi_file->avi_sound_header.fccType = streamtypeAUDIO;
				avi_file->avi_sound_header.dwQuality = (DWORD)-1;
				avi_file->avi_sound_header.dwScale = avi_file->wave_format.nBlockAlign;
				avi_file->avi_sound_header.dwRate = avi_file->wave_format.nAvgBytesPerSec;
				avi_file->avi_sound_header.dwSampleSize = avi_file->wave_format.nBlockAlign;
				avi_file->avi_sound_header.dwInitialFrames = 1;
			}

			avi_file->currentBufferIndex = 0;
			avi_file->audio_buffer_pos[0] = 0;
			avi_file->audio_buffer_pos[1] = 0;
		}

		AVIFileInit();

		// open the file
		if (FAILED(AVIFileOpen(&avi_file->avi_file, filename, OF_CREATE | OF_WRITE, NULL)))
			break;
		
		if (FAILED(AVIFileCreateStream(avi_file->avi_file, &avi_file->streams[VIDEO_STREAM], &avi_file->avi_video_header)))
			break;

		if (use_prev_options)
		{
			avi_file->compress_options[VIDEO_STREAM] = saved_avi_info.compress_options[VIDEO_STREAM];
			avi_file->compress_options_ptr[VIDEO_STREAM] = &avi_file->compress_options[0];
		}
		else
		{
			// get compression options
			memset(&avi_file->compress_options[VIDEO_STREAM], 0, sizeof(AVICOMPRESSOPTIONS));
			avi_file->compress_options_ptr[VIDEO_STREAM] = &avi_file->compress_options[0];
//retryAviSaveOptions: //mbg merge 7/17/06 removed
			if (!AVISaveOptions(MainWindow->getHWnd(), 0, 1, &avi_file->streams[VIDEO_STREAM], &avi_file->compress_options_ptr[VIDEO_STREAM]))
				break;
			isErrorInFileWrite = true;
		}

		// create compressed stream
		if (FAILED(AVIMakeCompressedStream(&avi_file->compressed_streams[VIDEO_STREAM], avi_file->streams[VIDEO_STREAM], &avi_file->compress_options[VIDEO_STREAM], NULL)))
			break;

		// set the stream format
		if (FAILED(AVIStreamSetFormat(avi_file->compressed_streams[VIDEO_STREAM], 0, (void*)&avi_file->bitmap_format, avi_file->bitmap_format.biSize)))
			break;

		// add sound (if requested)
		if (pwfex)
		{
			// create the audio stream
			if (FAILED(AVIFileCreateStream(avi_file->avi_file, &avi_file->streams[AUDIO_STREAM], &avi_file->avi_sound_header)))
				break;

			// AVISaveOptions doesn't seem to work for audio streams
			// so here we just copy the pointer for the compressed stream
			avi_file->compressed_streams[AUDIO_STREAM] = avi_file->streams[AUDIO_STREAM];

			// set the stream format
			if (FAILED(AVIStreamSetFormat(avi_file->compressed_streams[AUDIO_STREAM], 0, (void*)&avi_file->wave_format, sizeof(WAVEFORMATEX))))
				break;
		}

		// initialize counters
		avi_file->video_frames = 0;
		avi_file->sound_samples = 0;
		avi_file->tBytes = 0;
		avi_file->ByteBuffer = 0;

		// success
		result = true;

	} while(0);

	if (!result)
	{
		avi_destroy(&avi_file);

		free_aligned(avi_file->convert_buffer);
		free(avi_file);
		avi_file = NULL;

		if (isErrorInFileWrite)
			EMU_PrintError("Error writing AVI file");
	}

	return result;
}

static bool AviNextSegment()
{
	char avi_fname[MAX_PATH];
	strcpy(avi_fname, saved_avi_fname);
	char avi_fname_temp[MAX_PATH];
	sprintf(avi_fname_temp, "%s_part%d%s", avi_fname, avi_segnum + 2, saved_avi_ext);
	saved_avi_info = *avi_file;
	use_prev_options = 1;
	avi_segnum++;
	bool ret = DRV_AviBegin(avi_fname_temp, true);
	use_prev_options = 0;
	strcpy(saved_avi_fname, avi_fname);
	return ret;
}

//converts 16bpp to 24bpp and flips
static void do_video_conversion555(AVIFile* avi, const size_t bufferIndex, const u16* srcHead, const size_t firstLineIndex, const size_t lastLineIndex)
{
	const u16* src = srcHead + (avi->frameWidth * firstLineIndex);
	u8* dst = avi->convert_buffer + (avi->videoStreamBufferSize * bufferIndex) + (avi->frameWidth * (avi->frameHeight - (firstLineIndex + 1)) * 3);

	for (size_t y = firstLineIndex; y <= lastLineIndex; y++)
	{
		ColorspaceConvertBuffer555XTo888<true, false>(src, dst, avi->frameWidth);
		src += avi->frameWidth;
		dst -= avi->frameWidth * 3;
	}
}

//converts 32bpp to 24bpp and flips
static void do_video_conversion(AVIFile* avi, const size_t bufferIndex, const u32* srcHead, const size_t firstLineIndex, const size_t lastLineIndex)
{
	const u32* src = srcHead + (avi->frameWidth * firstLineIndex);
	u8* dst = avi->convert_buffer + (avi->videoStreamBufferSize * bufferIndex) + (avi->frameWidth * (avi->frameHeight - (firstLineIndex + 1)) * 3);

	for (size_t y = firstLineIndex; y <= lastLineIndex; y++)
	{
		ColorspaceConvertBuffer888XTo888<true, false>(src, dst, avi->frameWidth);
		src += avi->frameWidth;
		dst -= avi->frameWidth * 3;
	}
}

void DRV_AviFileWriteExecute(AVIFile *theFile, const size_t bufferIndex)
{
	HRESULT error = S_OK;

	// Write the video stream to file.
	if (avi_file->video_added)
	{
		error = AVIStreamWrite(avi_file->compressed_streams[VIDEO_STREAM],
			avi_file->video_frames, 1,
			avi_file->convert_buffer + (avi_file->videoStreamBufferSize * bufferIndex), avi_file->bitmap_format.biSizeImage,
			AVIIF_KEYFRAME, NULL, &avi_file->ByteBuffer);

		if (FAILED(error))
		{
			DRV_AviEnd(false);
			return;
		}

		avi_file->video_frames++;
		avi_file->tBytes += avi_file->ByteBuffer;
	}

	// Write the audio stream to file.
	if (avi_file->sound_added)
	{
		const int frameSampleCount = avi_file->audio_buffer_pos[bufferIndex] / avi_file->wave_format.nBlockAlign;

		error = AVIStreamWrite(avi_file->compressed_streams[AUDIO_STREAM],
			avi_file->sound_samples, frameSampleCount,
			avi_file->audio_buffer + (AUDIO_STREAM_BUFFER_SIZE * bufferIndex), avi_file->audio_buffer_pos[bufferIndex],
			0, NULL, &avi_file->ByteBuffer);

		if (FAILED(error))
		{
			DRV_AviEnd(false);
			return;
		}

		avi_file->sound_samples += frameSampleCount;
		avi_file->tBytes += avi_file->ByteBuffer;
		avi_file->audio_buffer_pos[bufferIndex] = 0;
	}

	// segment / split AVI when it's almost 2 GB (2000MB, to be precise)
	//if(!(avi_file->video_frames % 60) && avi_file->tBytes > 2097152000) AviNextSegment();
	//NOPE: why does it have to break at 1 second? 
	//we need to support dumping HD stuff here; that means 100s of MBs per second
	//let's roll this back a bit to 1800MB to give us a nice huge 256MB wiggle room, and get rid of the 1 second check
	if (avi_file->tBytes > (1800 * 1024 * 1024)) AviNextSegment();
}

void DRV_AviFileWriteFinish()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	avi_file->fileWriteThread->finish();
}

static void* RunConvertBuffer555XTo888(void *arg)
{
	AVIConversionParam *convertParam = (AVIConversionParam *)arg;
	do_video_conversion555(convertParam->avi, convertParam->bufferIndex, (u16 *)convertParam->src, convertParam->firstLineIndex, convertParam->lastLineIndex);

	return NULL;
}

static void* RunConvertBuffer888XTo888(void *arg)
{
	AVIConversionParam *convertParam = (AVIConversionParam *)arg;
	do_video_conversion(convertParam->avi, convertParam->bufferIndex, (u32 *)convertParam->src, convertParam->firstLineIndex, convertParam->lastLineIndex);

	return NULL;
}

static void* RunAviFileWrite(void *arg)
{
	AVIFileWriteParam *fileWriteParam = (AVIFileWriteParam *)arg;
	DRV_AviFileWriteExecute(fileWriteParam->avi, fileWriteParam->bufferIndex);

	return NULL;
}


bool DRV_AviBegin(const char* fname, bool newsegment)
{
	DRV_AviEnd(newsegment);

	if (!newsegment)
		avi_segnum = 0;

	BITMAPINFOHEADER bi;
	memset(&bi, 0, sizeof(bi));
	bi.biSize = 0x28;    
	bi.biPlanes = 1;
	bi.biBitCount = 24;
	bi.biWidth = 256 * video.prescaleHD;
	bi.biHeight = 384 * video.prescaleHD;
	bi.biSizeImage = 3 * bi.biWidth * bi.biHeight;

	WAVEFORMATEX wf;
	wf.cbSize = sizeof(WAVEFORMATEX);
	wf.nAvgBytesPerSec = DESMUME_SAMPLE_RATE * sizeof(u16) * 2;
	wf.nBlockAlign = 4;
	wf.nChannels = 2;
	wf.nSamplesPerSec = DESMUME_SAMPLE_RATE;
	wf.wBitsPerSample = 16;
	wf.wFormatTag = WAVE_FORMAT_PCM;
	

	saved_avi_ext[0]='\0';

	//mbg 8/10/08 - decide whether there will be sound in this movie
	//if this is a new movie..
	/*if(!avi_file) {
		if(FSettings.SndRate)
			use_sound = true;
		else use_sound = false;
	}*/

	//mbg 8/10/08 - if there is no sound in this movie, then dont open the audio stream
	WAVEFORMATEX* pwf = &wf;
	//if(!use_sound)
	//	pwf = 0;


	if (!avi_open(fname, &bi, pwf, newsegment))
	{
		saved_avi_fname[0]='\0';
		return 0;
	}

	if (avi_segnum == 0)
	{
		avi_file->numThreads = CommonSettings.num_cores;

		if (avi_file->numThreads > MAX_CONVERT_THREADS)
		{
			avi_file->numThreads = MAX_CONVERT_THREADS;
		}
		else if (avi_file->numThreads < 2)
		{
			avi_file->numThreads = 0;
		}
		
		size_t linesPerThread = (avi_file->numThreads > 0) ? avi_file->frameHeight / avi_file->numThreads : avi_file->frameHeight;

		for (size_t i = 0; i < avi_file->numThreads; i++)
		{
			if (i == 0)
			{
				avi_file->convertParam[i].firstLineIndex = 0;
				avi_file->convertParam[i].lastLineIndex = linesPerThread - 1;
			}
			else if (i == (avi_file->numThreads - 1))
			{
				avi_file->convertParam[i].firstLineIndex = avi_file->convertParam[i - 1].lastLineIndex + 1;
				avi_file->convertParam[i].lastLineIndex = avi_file->frameHeight - 1;
			}
			else
			{
				avi_file->convertParam[i].firstLineIndex = avi_file->convertParam[i - 1].lastLineIndex + 1;
				avi_file->convertParam[i].lastLineIndex = avi_file->convertParam[i].firstLineIndex + linesPerThread - 1;
			}

			avi_file->convertParam[i].avi = avi_file;
			avi_file->convertParam[i].bufferIndex = avi_file->currentBufferIndex;
			avi_file->convertParam[i].src = NULL;

			avi_file->convertThread[i] = new Task();
			avi_file->convertThread[i]->start(false);
		}

		avi_file->fileWriteParam.avi = avi_file;
		avi_file->fileWriteParam.bufferIndex = avi_file->currentBufferIndex;

		avi_file->fileWriteThread = new Task();
		avi_file->fileWriteThread->start(false);

		// Don't display at file splits
		EMU_PrintMessage("AVI recording started.");
		driver->AddLine("AVI recording started.");
	}

	strncpy(saved_cur_avi_fnameandext,fname,MAX_PATH);
	strncpy(saved_avi_fname,fname,MAX_PATH);
	char* dot = strrchr(saved_avi_fname, '.');
	if (dot && dot > strrchr(saved_avi_fname, '/') && dot > strrchr(saved_avi_fname, '\\'))
	{
		strcpy(saved_avi_ext,dot);
		dot[0]='\0';
	}

	return 1;
}

void DRV_AviFrameStart()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	avi_file->currentBufferIndex = ((avi_file->currentBufferIndex + 1) % 2);
}

void DRV_AviVideoUpdate()
{
	//dont do anything if prescale has changed, it's just going to be garbage
	if (!AVI_IsRecording() || !avi_file->video_added || (video.prescaleHD != avi_file->prescaleLevel))
	{
		return;
	}

	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const void *buffer = dispInfo.masterCustomBuffer;

	if (gpu_bpp == 15)
	{
		if (avi_file->numThreads == 0)
		{
			do_video_conversion555(avi_file, avi_file->currentBufferIndex, (u16 *)buffer, 0, avi_file->frameHeight - 1);
		}
		else
		{
			for (size_t i = 0; i < avi_file->numThreads; i++)
			{
				avi_file->convertParam[i].src = buffer;
				avi_file->convertParam[i].bufferIndex = avi_file->currentBufferIndex;
				avi_file->convertThread[i]->execute(&RunConvertBuffer555XTo888, &avi_file->convertParam[i]);
			}
		}
	}
	else
	{
		if (avi_file->numThreads == 0)
		{
			do_video_conversion(avi_file, avi_file->currentBufferIndex, (u32 *)buffer, 0, avi_file->frameHeight - 1);
		}
		else
		{
			for (size_t i = 0; i < avi_file->numThreads; i++)
			{
				avi_file->convertParam[i].src = buffer;
				avi_file->convertParam[i].bufferIndex = avi_file->currentBufferIndex;
				avi_file->convertThread[i]->execute(&RunConvertBuffer888XTo888, &avi_file->convertParam[i]);
			}
		}
	}
}

void DRV_AviSoundUpdate(void *soundData, int soundLen)
{
	if (!AVI_IsRecording() || !avi_file->sound_added)
		return;

	const int soundSize = soundLen * avi_file->wave_format.nBlockAlign;
	memcpy(avi_file->audio_buffer + (AUDIO_STREAM_BUFFER_SIZE * avi_file->currentBufferIndex) + avi_file->audio_buffer_pos[avi_file->currentBufferIndex], soundData, soundSize);
	avi_file->audio_buffer_pos[avi_file->currentBufferIndex] += soundSize;
}

void DRV_AviFileWrite()
{
	if (!AVI_IsRecording())
	{
		return;
	}

	if (avi_file->video_added)
	{
		for (size_t i = 0; i < avi_file->numThreads; i++)
		{
			avi_file->convertThread[i]->finish();
		}
	}

	DRV_AviFileWriteFinish();

 	avi_file->fileWriteParam.bufferIndex = avi_file->currentBufferIndex;
	avi_file->fileWriteThread->execute(&RunAviFileWrite, &avi_file->fileWriteParam);
}

bool AVI_IsRecording()
{
	return (avi_file != NULL);
}

void DRV_AviEnd(bool newsegment)
{
	if (!AVI_IsRecording())
		return;

	avi_destroy(&avi_file);

	if (!newsegment)
	{
		EMU_PrintMessage("AVI recording ended.");
		driver->AddLine("AVI recording ended.");

		delete avi_file->fileWriteThread;
		avi_file->fileWriteThread = NULL;

		for (size_t i = 0; i < avi_file->numThreads; i++)
		{
			delete avi_file->convertThread[i];
			avi_file->convertThread[i] = NULL;
			avi_file->numThreads = 0;
		}

		free_aligned(avi_file->convert_buffer);
		free(avi_file);
		avi_file = NULL;
	}
}
