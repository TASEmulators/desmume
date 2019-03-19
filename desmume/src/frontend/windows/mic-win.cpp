/*
	Copyright (C) 2008-2019 DeSmuME team

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

/*
	The NDS microphone produces 8-bit sound sampled at 16khz.
	The sound data must be read sample-by-sample through the 
	ARM7 SPI device (touchscreen controller, channel 6).

	Note : I added these notes because the microphone isn't 
	documented on GBATek.
*/

#include "mic.h"

#include <mmsystem.h>
#include <windows.h>
#include <string.h>
#include <vector>
#include <fstream>

#include "types.h"
#include "NDSSystem.h"
#include "emufile.h"
#include "debug.h"
#include "movie.h"
#include "readwrite.h"

int MicDisplay;

#define MIC_CHECKERR(hr) if(hr != MMSYSERR_NOERROR) return FALSE;

#define MIC_BUFSIZE 4096

static BOOL Mic_Inited = FALSE;

static u8 Mic_TempBuf[MIC_BUFSIZE];
static u8 Mic_Buffer[2][MIC_BUFSIZE];
static u16 Mic_BufPos;
static u8 Mic_WriteBuf;
static u8 Mic_PlayBuf;

static int micReadSamplePos=0;

static HWAVEIN waveIn;
static WAVEHDR waveHdr;

static int CALLBACK waveInProc(HWAVEIN wavein, UINT msg, DWORD instance, DWORD_PTR param1, DWORD_PTR param2)
{
	LPWAVEHDR lpWaveHdr;
	HRESULT hr;

	if(!Mic_Inited)
		return 1;

	if(msg == WIM_DATA)
	{
		lpWaveHdr = (LPWAVEHDR)param1;

		memcpy(Mic_Buffer[Mic_WriteBuf], lpWaveHdr->lpData, MIC_BUFSIZE);
		Mic_WriteBuf ^= 1;

		hr = waveInAddBuffer(waveIn, lpWaveHdr, sizeof(WAVEHDR));
		if(hr != MMSYSERR_NOERROR)
			return 1;
	}

	return 0;
}

static FILE* fp = NULL;

static bool dataChunk(EMUFILE &inf)
{
	bool found = false;

	// seek to just after the RIFF header
	inf.fseek(12,SEEK_SET);

	// search for a format chunk
	for (;;)
	{
		char chunk_id[4];
		u32  chunk_length;

		if (inf.eof()) return found;
		if (inf.fread(chunk_id, 4) != 4) return found;
		if (!inf.read_32LE(chunk_length)) return found;

		// if we found a data chunk, excellent!
      if (memcmp(chunk_id, "data", 4) == 0)
	  {
		  found = true;
		  u8 *temp = new u8[chunk_length];
		  if (inf.fread(temp,chunk_length) != chunk_length || chunk_length == 0)
		  {
			  delete[] temp;
			  return false;
		  }
			micSamples.resize(micSamples.size()+1);
			std::vector<u8>& thisSample = micSamples[micSamples.size()-1];
			thisSample.resize(chunk_length);
			memcpy(&thisSample[0], temp, chunk_length);
		  delete[] temp;
		  chunk_length = 0;
	  }

	  inf.fseek(chunk_length,SEEK_CUR);
	}

	return found;
}

static bool formatChunk(EMUFILE &inf)
{
	// seek to just after the RIFF header
	inf.fseek(12,SEEK_SET);

	// search for a format chunk
	for (;;)
	{
		char chunk_id[4];
		u32  chunk_length;

		inf.fread(chunk_id, 4);
		if (!inf.read_32LE(chunk_length)) return false;

		// if we found a format chunk, excellent!
		if (memcmp(chunk_id, "fmt ", 4) == 0 && chunk_length >= 16)
		{

			// read format chunk
			u16 format_tag;
			u16 channel_count;
			u32 samples_per_second;
			//u32 bytes_per_second   = read32_le(chunk + 8);
			//u16 block_align        = read16_le(chunk + 12);
			u16 bits_per_sample;

			if (inf.read_16LE(format_tag) != 1) return false;
			if (inf.read_16LE(channel_count) != 1) return false;
			if (inf.read_32LE(samples_per_second) != 1) return false;
			inf.fseek(6,SEEK_CUR);
			if (inf.read_16LE(bits_per_sample) != 1) return false;

			chunk_length -= 16;

			// format_tag must be 1 (WAVE_FORMAT_PCM)
			// we only support mono 8bit
			if (format_tag != 1 ||
				channel_count != 1 ||
				bits_per_sample != 8)
			{
					MessageBox(0,"not a valid RIFF WAVE file; must be 8bit mono pcm",0,0);
					return false;
			}

			return true;
		}

		inf.fseek(chunk_length,SEEK_CUR);
	}
	return false;
}

void Mic_PrevSample()
{
	if(MicSampleSelection==0) return;
	MicSampleSelection--;
}

void Mic_NextSample()
{
	if(MicSampleSelection==255) return;
	MicSampleSelection++;
	if(MicSampleSelection >= micSamples.size())
	{
		if(micSamples.size()==0)
			MicSampleSelection = 0;
		else 
			MicSampleSelection = micSamples.size()-1;
	}
}

bool LoadSample(const char* name)
{
	EMUFILE_FILE inf(name,"rb");
	if (inf.fail()) return false;

	//wav reading code adapted from AUDIERE (LGPL)

	// read the RIFF header
	u8 riff_id[4];
	u32 riff_length;
	u8 riff_datatype[4];

	inf.fread(riff_id, 4);
	inf.read_32LE(riff_length);
	inf.fread(riff_datatype, 4);

	if (inf.size() < 12 ||
		memcmp(riff_id, "RIFF", 4) != 0 ||
		riff_length == 0 ||
		memcmp(riff_datatype, "WAVE", 4) != 0) {
		MessageBox(0,"not a valid RIFF WAVE file",0,0);
		return false;
	}

	if (!formatChunk(inf))
		return false;

	if (!dataChunk(inf))
	{
		MessageBox(0,"not a valid WAVE file. some unknown problem.",0,0);
		return false;
	}

	return true;
}

bool LoadSamples(const char *name)
{
	//unload any existing mic samples
	micSamples.resize(0);

	//select NO mic sample
	micReadSamplePos = 0;
	MicSampleSelection = 0;

	//if we're disabling the mic samples system, just bail now
	if (!name || !*name) return true;

	//analyze the filename for _0 at the end. anything with _0 at the end is assumed to be the beginning of a series of files
	//(and if not, it can still be loaded just fine)
	const char* ext = strrchr(name,'.');
	
	//in case the filename had no extension... it's an error.
	if(!ext) return false;

	const char* maybe_0 = ext-2;
	
	//in case this was an absurdly short filename
	if(ext<name)
		return LoadSample(name);

	//if it was not a _0, just load it
	if(strncmp(maybe_0,"_0",2))
		return LoadSample(name);
	
	//otherwise replace it with increasing numbers and load all those
	std::string prefix = name;
	prefix.resize(maybe_0-name+1); //take care to keep the _
	
	//if found, it's a wildcard. load all those samples
	//this is limited to 254 entries in order to prevent some surprises, because I was stupid and used a byte for the MicSampleSelection.
	//I should probably change that. But it has to be limited somehow...
	for(int i=0;i<255;i++)
	{
		char tmp[32];
		sprintf(tmp,"%d",i);
		std::string trial = prefix + tmp + ".wav";
		printf("Trying sample %s\n",trial.c_str());
		bool ok = LoadSample(trial.c_str());
		if(!ok)
		{
			if(i==0) return false;
			break;
		}
	}

	//OK, we loaded some samples. that's all we have to do
	return true;
}

BOOL Mic_DeInit_Physical()
{
	if(!Mic_Inited)
		return TRUE;

	INFO("win32 microphone DEinit OK\n");

	Mic_Inited = FALSE;

	waveInReset(waveIn);
	waveInClose(waveIn);

	return TRUE;
}

BOOL Mic_Init_Physical()
{
	if (Mic_Inited)
		return TRUE;

	Mic_Inited = FALSE;

	HRESULT hr;
	WAVEFORMATEX wfx;

	memset(Mic_TempBuf, 0x80, MIC_BUFSIZE);
	memset(Mic_Buffer[0], 0x80, MIC_BUFSIZE);
	memset(Mic_Buffer[1], 0x80, MIC_BUFSIZE);
	Mic_BufPos = 0;

	Mic_WriteBuf = 0;
	Mic_PlayBuf = 1;

	memset(&wfx, 0, sizeof(wfx));
	wfx.cbSize = 0;
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = 16000;
	wfx.nBlockAlign = 1;
	wfx.nAvgBytesPerSec = 16000;
	wfx.wBitsPerSample = 8;

	hr = waveInOpen(&waveIn, WAVE_MAPPER, &wfx, (DWORD_PTR)waveInProc, 0, CALLBACK_FUNCTION);
	MIC_CHECKERR(hr)

	memset(&waveHdr, 0, sizeof(waveHdr));
	waveHdr.lpData = (LPSTR)Mic_TempBuf;
	waveHdr.dwBufferLength = MIC_BUFSIZE;

	hr = waveInPrepareHeader(waveIn, &waveHdr, sizeof(WAVEHDR));
	MIC_CHECKERR(hr)

	hr = waveInAddBuffer(waveIn, &waveHdr, sizeof(WAVEHDR));
	MIC_CHECKERR(hr)

	hr = waveInStart(waveIn);
	MIC_CHECKERR(hr)

	Mic_Inited = TRUE;
	INFO("win32 microphone init OK\n");

	return TRUE;
}

BOOL Mic_Init()
{
	micReadSamplePos = 0;
	
	return TRUE;
}

void Mic_Reset()
{
	micReadSamplePos = 0;
	
	if (!Mic_Inited)
		return;

	//reset physical
	memset(Mic_TempBuf, 0x80, MIC_BUFSIZE);
	memset(Mic_Buffer[0], 0x80, MIC_BUFSIZE);
	memset(Mic_Buffer[1], 0x80, MIC_BUFSIZE);
	Mic_BufPos = 0;

	Mic_WriteBuf = 0;
	Mic_PlayBuf = 1;
}

void Mic_DeInit()
{
}

static const u8 random[32] = {
    0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xF5, 0xFF, 0xFF, 0xFF, 0xFF, 0x8E, 0xFF, 
    0xF4, 0xE1, 0xBF, 0x9A, 0x71, 0x58, 0x5B, 0x5F, 0x62, 0xC2, 0x25, 0x05, 0x01, 0x01, 0x01, 0x01, 
} ;



u8 Mic_ReadSample()
{
	u8 ret;
	u8 tmp;
	if (CommonSettings.micMode == TCommonSettings::Physical)
	{
		if (movieMode == MOVIEMODE_INACTIVE)
		{
			//normal mic behavior
			tmp = (u8)Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];
		}
		else
		{
			//since we're not recording Mic_Buffer to the movie, use silence
			tmp = 0x80;
		}
	}
	else
	{
		if (NDS_getFinalUserInput().mic.micButtonPressed)
		{
			if (micSamples.size() > 0)
			{
				//why is this reading every sample twice? I did this for some reason in 57dbe9128d0f8cbb4bd79154fb9cda3ab6fab386
				//maybe the game reads two samples in succession to check them or something. stuff definitely works better with the two-in-a-row
				if (micReadSamplePos == micSamples[MicSampleSelection].size()*2)
				{
					tmp = 0x80; //silence, with 8 bit signed values
				}
				else
				{
					tmp = micSamples[MicSampleSelection][micReadSamplePos >> 1];
					
					//nintendogs seems sensitive to clipped samples, at least.
					//by clamping these, we get better sounds on playback
					//I don't know how important it is, but I like nintendogs to sound good at least..
					if(tmp == 0) tmp = 1;
					if(tmp == 255) tmp = 254;

					micReadSamplePos++;
					if(micReadSamplePos == micSamples[MicSampleSelection].size()*2)
					{
						printf("Ended mic sample MicSampleSelection\n");
					}
				}
			}
			else
			{
				//use the "random" values
				if (CommonSettings.micMode == TCommonSettings::InternalNoise)
					tmp = random[micReadSamplePos >> 1];
				else tmp = rand();
				micReadSamplePos++;
				if (micReadSamplePos == ARRAY_SIZE(random)*2)
					micReadSamplePos=0;
			}
		}
		else
		{
			tmp = 0x80;

			//reset mic button buffer pos if not pressed
			micReadSamplePos=0;
		}
	}

	if (Mic_BufPos & 0x1)
	{
		ret = ((tmp & 0x1) << 7);
	}
	else
	{
		ret = ((tmp & 0xFE) >> 1);
	}

	MicDisplay = tmp;

	Mic_BufPos++;
	if (Mic_BufPos >= (MIC_BUFSIZE << 1))
	{
		Mic_BufPos = 0;
		Mic_PlayBuf ^= 1;
	}

	return ret;
}

// maybe a bit paranoid...
void mic_savestate(EMUFILE &os)
{
	//version
	os.write_32LE(1);
	assert(MIC_BUFSIZE == 4096); // else needs new version

	os.fwrite(Mic_Buffer[0], MIC_BUFSIZE);
	os.fwrite(Mic_Buffer[1], MIC_BUFSIZE);
	os.write_16LE(Mic_BufPos);
	os.write_u8(Mic_WriteBuf); // seems OK to save...
	os.write_u8(Mic_PlayBuf);
	os.write_32LE(micReadSamplePos);
}
bool mic_loadstate(EMUFILE &is, int size)
{
	u32 version;
	if (is.read_32LE(version) != 1) return false;
	if (version > 1 || version == 0) { is.fseek(size-4, SEEK_CUR); return true; }

	is.fread(Mic_Buffer[0], MIC_BUFSIZE);
	is.fread(Mic_Buffer[1], MIC_BUFSIZE);
	is.read_16LE(Mic_BufPos);
	is.read_u8(Mic_WriteBuf);
	is.read_u8(Mic_PlayBuf);
	is.read_32LE(micReadSamplePos);
	return true;
}

