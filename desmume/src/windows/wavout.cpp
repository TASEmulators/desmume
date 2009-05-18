/* wavout.cpp
 *
 * Copyright (C) 2006-2008 Zeromus
 *
 * This file is part of DeSmuME
 *
 * DeSmuME is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * DeSmuME is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with DeSmuME; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/
#include "main.h"		//I added this so that this file could gain access to SetMessageToDisplay.
#include "types.h"
#include "windriver.h"
#include "console.h"
#include "gfx3d.h"

#include <assert.h>
#include <vfw.h>
#include <stdio.h>

#include "debug.h"

static void EMU_PrintError(const char* msg) {
	LOG(msg);
}

static void EMU_PrintMessage(const char* msg) {
	LOG(msg);
}

bool DRV_WavBegin(const char* fname);
void DRV_WavEnd();
void DRV_WavSoundUpdate(void* soundData, int soundLen);
bool DRV_WavIsRecording();

static struct WAVFile
{
	bool				valid;

	WAVEFORMATEX		wav_format_master;
	WAVEFORMATEX		wav_format;
	HMMIO				wav_file;

	MMCKINFO			waveChunk;
	MMCKINFO			fmtChunk;
	MMCKINFO			dataChunk;
} *wav_file = NULL;

static void wav_create(struct WAVFile** wav_out)
{
	*wav_out = (struct WAVFile*)malloc(sizeof(struct WAVFile));
	memset(*wav_out, 0, sizeof(struct WAVFile));
}

static void wav_destroy(struct WAVFile** wav_out)
{
	if(!(*wav_out))
		return;

	if((*wav_out)->wav_file)
	{
		mmioAscend((*wav_out)->wav_file, &(*wav_out)->dataChunk, 0);
		mmioAscend((*wav_out)->wav_file, &(*wav_out)->waveChunk, 0);
//		mmioFlush((*wav_out)->wav_file, 0);
		mmioClose((*wav_out)->wav_file, 0);
		(*wav_out)->wav_file = NULL;
	}

	free(*wav_out);
	*wav_out = NULL;
}

static void set_sound_format(const WAVEFORMATEX* wave_format, struct WAVFile* wav_out)
{
	memcpy(&((*wav_out).wav_format_master), wave_format, sizeof(WAVEFORMATEX));
}

static int wav_open(const char* filename, const WAVEFORMATEX* pwfex)
{
	int error = 1;
	int result = 0;

	do
	{
		// close existing first
		DRV_WavEnd();

		// create the object
		wav_create(&wav_file);
		if(!wav_file)
			break;

		// add audio format
		set_sound_format(pwfex, wav_file);

		// open the file
		if(!(wav_file->wav_file = mmioOpen((LPSTR)filename, NULL, MMIO_CREATE|MMIO_WRITE)))
			break;

		// create WAVE chunk
		wav_file->waveChunk.fccType = mmioFOURCC('W', 'A', 'V', 'E');
		mmioCreateChunk(wav_file->wav_file, &wav_file->waveChunk, MMIO_CREATERIFF);

		// create Format chunk
		wav_file->fmtChunk.ckid = mmioFOURCC('f', 'm', 't', ' ');
		mmioCreateChunk(wav_file->wav_file, &wav_file->fmtChunk, 0);
		// then write header
		memcpy(&wav_file->wav_format, &wav_file->wav_format_master, sizeof(WAVEFORMATEX));
		wav_file->wav_format.cbSize = 0;
		mmioWrite(wav_file->wav_file, (HPSTR) &wav_file->wav_format, sizeof(WAVEFORMATEX));
		mmioAscend(wav_file->wav_file, &wav_file->fmtChunk, 0);

		// create Data chunk
		wav_file->dataChunk.ckid = mmioFOURCC('d', 'a', 't', 'a');
		mmioCreateChunk(wav_file->wav_file, &wav_file->dataChunk, 0);

		// success
		error = 0;
		result = 1;
		wav_file->valid = true;

	} while(false);

	if(!result)
	{
		wav_destroy(&wav_file);
		if(error)
			EMU_PrintError("Error writing WAV file");
	}

	return result;
}


bool DRV_WavBegin(const char* fname)
{
	DRV_WavEnd();

	WAVEFORMATEX wf;
	wf.cbSize = sizeof(WAVEFORMATEX);
	wf.nAvgBytesPerSec = 44100 * 4;
	wf.nBlockAlign = 4;
	wf.nChannels = 2;
	wf.nSamplesPerSec = 44100;
	wf.wBitsPerSample = 16;
	wf.wFormatTag = WAVE_FORMAT_PCM;
	WAVEFORMATEX* pwf = &wf;

	if(!wav_open(fname, pwf))
		return 0;

	EMU_PrintMessage("WAV recording started.");
	SetMessageToDisplay("WAV recording started.");

	return 1;
}

BOOL WAV_IsRecording()
{
	return wav_file && wav_file->valid;
}
void DRV_WavSoundUpdate(void* soundData, int soundLen)
{
	int nBytes;

	if(!WAV_IsRecording())
		return;

	nBytes = soundLen * wav_file->wav_format.nBlockAlign;
	// assumes mmio system has been opened data chunk
	mmioWrite(wav_file->wav_file, (HPSTR) soundData, nBytes);
//	mmioFlush(wav_file->wav_file, 0);
}

void DRV_WavEnd()
{
	if(!wav_file)
		return;

	EMU_PrintMessage("WAV recording ended.");
	SetMessageToDisplay("WAV recording ended.");

	wav_destroy(&wav_file);
}

bool DRV_WavIsRecording()
{
	if(wav_file)
		return true;

	return false;
}
