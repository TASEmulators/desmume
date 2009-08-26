/*
	Microphone emulation for Win32

	The NDS microphone produces 8-bit sound sampled at 16khz.
	The sound data must be read sample-by-sample through the 
	ARM7 SPI device (touchscreen controller, channel 6).

	Note : I added these notes because the microphone isn't 
	documented on GBATek.
*/
#include "NDSSystem.h"
#include "../types.h"
#include "../debug.h"
#include "../mic.h"
#include "../movie.h"
#include "readwrite.h"
#include <iostream>
#include <fstream>

int MicDisplay;
int SampleLoaded=0;

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

static char* samplebuffer = NULL;
static int samplebuffersize = 0;
static FILE* fp = NULL;

bool LoadSample(const char *name)
{
	std::ifstream fl(name);

	if (!fl.is_open())
	{
		return false;
	}

	fl.seekg( 0, std::ios::end );
	size_t len = fl.tellg();

	// Avoid mem leaks
	if (samplebuffer != NULL)
		delete[] samplebuffer;

	samplebuffersize = len;
	samplebuffer = new char[len];

	fl.seekg(0, std::ios::beg);
	fl.read (samplebuffer, len);
	fl.close();

	SampleLoaded=1;

	return true;
}

BOOL Mic_Init() {

	if(Mic_Inited)
		return TRUE;

	Mic_Inited = FALSE;

	HRESULT hr;
	WAVEFORMATEX wfx;

	memset(Mic_TempBuf, 0, MIC_BUFSIZE);
	memset(Mic_Buffer[0], 0, MIC_BUFSIZE);
	memset(Mic_Buffer[1], 0, MIC_BUFSIZE);
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

	int x = sizeof(DWORD_PTR);

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
	return TRUE;
}

void Mic_Reset()
{
	if(!Mic_Inited)
		return;

	memset(Mic_TempBuf, 0, MIC_BUFSIZE);
	memset(Mic_Buffer[0], 0, MIC_BUFSIZE);
	memset(Mic_Buffer[1], 0, MIC_BUFSIZE);
	Mic_BufPos = 0;

	Mic_WriteBuf = 0;
	Mic_PlayBuf = 1;

	micReadSamplePos = 0;
}

void Mic_DeInit()
{
	if(!Mic_Inited)
		return;

	Mic_Inited = FALSE;

	waveInReset(waveIn);
	waveInClose(waveIn);
}

static const int random[32] = {0xB1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE9, 0x70, 
0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x18, 0x28, 0x00, 0x00, 0x00, 
0x00, 0x00, 0x20, 0xE1, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xE9};


u8 Mic_ReadSample()
{

	if(!Mic_Inited)
		return 0;

	u8 ret;
	u8 tmp;
	if(NDS_getFinalUserInput().mic.micButtonPressed) {
		if(SampleLoaded) {
			//use a sample
			//TODO: what if a movie is active?
			// for now I'm going to hope that if anybody records a movie with a sample loaded,
			// either they know what they're doing and plan to distribute the sample,
			// or they're playing a game where it doesn't even matter or they never press the mic button.
			tmp = samplebuffer[micReadSamplePos >> 1];
			micReadSamplePos++;
			if(micReadSamplePos == samplebuffersize*2)
				micReadSamplePos=0;
		} else {
			//use the "random" values
			tmp = random[micReadSamplePos >> 1];
			//tmp = rand()&0xFF;
			micReadSamplePos++;
			if(micReadSamplePos == ARRAY_SIZE(random)*2)
				micReadSamplePos=0;
		}
	} 
	else {
		if(movieMode != MOVIEMODE_INACTIVE)
		{
			//normal mic behavior
			tmp = (u8)Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];
		}
		else
		{
			//since we're not recording Mic_Buffer to the movie, use silence
			tmp = 0;
		}

		//reset mic button buffer pos if not pressed
		micReadSamplePos=0;
	}

	if(Mic_BufPos & 0x1)
	{
		ret = ((tmp & 0x1) << 7);
	}
	else
	{
		ret = ((tmp & 0xFE) >> 1);
	}

	MicDisplay = tmp;

	Mic_BufPos++;
	if(Mic_BufPos >= (MIC_BUFSIZE << 1))
	{
		Mic_BufPos = 0;
		Mic_PlayBuf ^= 1;
	}

	return ret;
}

// maybe a bit paranoid...
void mic_savestate(EMUFILE* os)
{
	//version
	write32le(0,os);
	assert(MIC_BUFSIZE == 4096); // else needs new version

	os->fwrite((char*)Mic_Buffer[0], MIC_BUFSIZE);
	os->fwrite((char*)Mic_Buffer[1], MIC_BUFSIZE);
	write16le(Mic_BufPos,os);
	write8le(Mic_WriteBuf,os); // seems OK to save...
	write8le(Mic_PlayBuf,os);
	write32le(micReadSamplePos,os);
}
bool mic_loadstate(EMUFILE* is, int size)
{
	u32 version;
	if(read32le(&version,is) != 1) return false;
	if(version > 0) { is->fseek(size-4, SEEK_CUR); return true; }

	is->fread((char*)Mic_Buffer[0], MIC_BUFSIZE);
	is->fread((char*)Mic_Buffer[1], MIC_BUFSIZE);
	read16le(&Mic_BufPos,is);
	read8le(&Mic_WriteBuf,is);
	read8le(&Mic_PlayBuf,is);
	read32le(&micReadSamplePos,is);
	return true;
}

