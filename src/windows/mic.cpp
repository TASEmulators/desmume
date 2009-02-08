/*
	Microphone emulation for Win32

	The NDS microphone produces 8-bit sound sampled at 16khz.
	The sound data must be read sample-by-sample through the 
	ARM7 SPI device (touchscreen controller, channel 6).

	Note : I added these notes because the microphone isn't 
	documented on GBATek.
*/
#include <windows.h>
#include "../types.h"
#include "../debug.h"
#include "../mic.h"

#define MIC_CHECKERR(hr) if(hr != MMSYSERR_NOERROR) return FALSE;

#define MIC_BUFSIZE 4096

BOOL Mic_Inited = FALSE;

s8 Mic_Buffer[MIC_BUFSIZE];
u16 Mic_BufPos;

HWAVEIN waveIn;
WAVEHDR waveHdr;

static int CALLBACK waveInProc(HWAVEIN wavein, UINT msg, DWORD instance, DWORD param1, DWORD param2)
{
	LPWAVEHDR lpWaveHdr;
	HRESULT hr;

	if(!Mic_Inited)
		return 1;

	if(msg == WIM_DATA)
	{
		lpWaveHdr = (LPWAVEHDR)param1;

		hr = waveInAddBuffer(waveIn, lpWaveHdr, sizeof(WAVEHDR));
		if(hr != MMSYSERR_NOERROR)
			return 1;
	}

	return 0;
}

BOOL Mic_Init()
{
	if(Mic_Inited)
		return TRUE;

	Mic_Inited = FALSE;

	HRESULT hr;
	WAVEFORMATEX wfx;

	memset(Mic_Buffer, 0, MIC_BUFSIZE);
	Mic_BufPos = 0;

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
	waveHdr.lpData = (LPSTR)Mic_Buffer;
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

	memset(Mic_Buffer, 0, MIC_BUFSIZE);
	Mic_BufPos = 0;
}

void Mic_DeInit()
{
	if(!Mic_Inited)
		return;

	Mic_Inited = FALSE;

	waveInReset(waveIn);
	waveInClose(waveIn);
}

u8 Mic_ReadSample()
{
	if(!Mic_Inited)
		return 0;

	u8 tmp = (u8)Mic_Buffer[Mic_BufPos >> 1];
	u8 ret;

	if(Mic_BufPos & 0x1)
	{
		ret = ((tmp & 0x1) << 7);
	}
	else
	{
		ret = ((tmp & 0xFE) >> 1);
	}

	Mic_BufPos++;
	Mic_BufPos %= (MIC_BUFSIZE << 1);

	return ret;
}
