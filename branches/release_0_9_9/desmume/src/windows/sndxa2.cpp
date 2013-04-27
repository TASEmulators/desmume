/*
	Copyright (C) 2006-2013 DeSmuME team

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

#include <winsock2.h>
#include "directx/XAudio2.h"
#include <windows.h>
#include "types.h"
#include "sndxa2.h"
#include "windriver.h"

int SNDXA2Init(int buffersize);
void SNDXA2DeInit();
void PushBuffer(u8 *pAudioData,u32 AudioBytes);
void SNDXA2UpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDXA2GetAudioSpace();
void SNDXA2MuteAudio();
void SNDXA2UnMuteAudio();
void SNDXA2SetVolume(int volume);
void SNDXA2ClearAudioBuffer();

SoundInterface_struct SNDXAUDIO2 = {
	SNDCORE_XAUDIO2,
	"Xaudio2 Interface",
	SNDXA2Init,
	SNDXA2DeInit,
	SNDXA2UpdateAudio,
	SNDXA2GetAudioSpace,
	SNDXA2MuteAudio,
	SNDXA2UnMuteAudio,
	SNDXA2SetVolume,
	SNDXA2ClearAudioBuffer,
};

static IXAudio2SourceVoice *pSourceVoice;
static IXAudio2 *pXAudio2;
static IXAudio2MasteringVoice* pMasterVoice;

static volatile LONG submittedBlocks;		// currently submitted XAudio2 buffers

static u32 bufferSize;  					// the size of soundBuffer
static u32 singleBufferSamples;				// samples in one block
static u32 singleBufferBytes;				// bytes in one block
static u32 blockCount;						// soundBuffer is divided into blockCount blocks

static u32 writeOffset;						// offset into the buffer for the next block
static u8 *soundBuffer;						// the buffer itself

static float soundVolume;
static bool isMuted;

static HANDLE bufferReadyEvent;
static HANDLE threadQuitEvent;
static volatile bool exitthread;

class _voiceCallback: public IXAudio2VoiceCallback
{
	STDMETHODIMP_(void) OnBufferEnd(void *pBufferContext)
	{
		InterlockedDecrement(&submittedBlocks);
		SetEvent(bufferReadyEvent);
	}
	STDMETHODIMP_(void) OnBufferStart(void *pBufferContext){}
	STDMETHODIMP_(void) OnLoopEnd(void *pBufferContext){}
	STDMETHODIMP_(void) OnStreamEnd() {}
	STDMETHODIMP_(void) OnVoiceError(void *pBufferContext, HRESULT Error) {}
	STDMETHODIMP_(void) OnVoiceProcessingPassEnd() {}
	STDMETHODIMP_(void) OnVoiceProcessingPassStart(UINT32 BytesRequired) {}
} voiceCallback;

DWORD WINAPI SNDXA2Thread( LPVOID )
{
	while(!exitthread) {
		{
			Lock lock;
			SPU_Emulate_user();
		}
		WaitForSingleObject(bufferReadyEvent,1000);
	}
	SetEvent(threadQuitEvent);
	return 0;
}

int SNDXA2Init(int buffersize)
{
	HRESULT hr;
	if ( FAILED(hr = XAudio2Create( &pXAudio2, 0 , XAUDIO2_DEFAULT_PROCESSOR ) ) ) {
		MessageBox (NULL, TEXT("XAudio2Create Error\nThis is usually caused by not having a recent DirectX release installed."),
			TEXT("Error"),
			MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	if ( FAILED(hr = pXAudio2->CreateMasteringVoice( &pMasterVoice, 2, DESMUME_SAMPLE_RATE, 0, 0 , NULL ) ) ) {
		MessageBox (NULL, TEXT("CreateMasteringVoice Error."),
			TEXT("Error"),
			MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = DESMUME_SAMPLE_RATE;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if( FAILED(hr = pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX*)&wfx,
			XAUDIO2_VOICE_NOSRC , XAUDIO2_DEFAULT_FREQ_RATIO, &voiceCallback, NULL, NULL ) ) ) {
		 MessageBox (NULL, TEXT("CreateMasteringVoice Error."),
			TEXT("Error"),
			MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	bufferReadyEvent = CreateEvent(NULL,TRUE,TRUE,NULL);
	threadQuitEvent = CreateEvent(NULL,TRUE,TRUE,NULL);
	exitthread = false;

	/* size tuning here
	*/
	blockCount = 8;
	singleBufferSamples = buffersize / 2 / blockCount;
	singleBufferBytes = singleBufferSamples * 4;
	bufferSize = singleBufferBytes * blockCount;

	soundBuffer = new u8[bufferSize];
	writeOffset = 0;

	submittedBlocks = 0;
	isMuted = false;
	soundVolume = 1.0f;

	for(int i=0;i<blockCount;i++)
		PushBuffer(0,singleBufferBytes);

	pSourceVoice->Start(0);

	CreateThread(0,0,SNDXA2Thread,0,0,0);

	return 0;
}

void SNDXA2DeInit()
{
	ResetEvent(threadQuitEvent);
	exitthread = true;
	SetEvent(bufferReadyEvent);
	WaitForSingleObject(threadQuitEvent,1000);

	if(pSourceVoice) {
		pSourceVoice->Stop(0);
		pSourceVoice->DestroyVoice();
		pSourceVoice = NULL;
	}
	if(pMasterVoice) {
		pMasterVoice->DestroyVoice();
		pMasterVoice = NULL;
	}
	if(soundBuffer) {
		delete [] soundBuffer;
		soundBuffer = NULL;
	}
	if(pXAudio2) {
		pXAudio2->Release();
		pXAudio2 = NULL;
	}
	if(bufferReadyEvent) {
		CloseHandle(bufferReadyEvent);
		bufferReadyEvent = NULL;
	}
	if(threadQuitEvent) {
		CloseHandle(threadQuitEvent);
		threadQuitEvent = NULL;
	}
}

void PushBuffer(u8 *pAudioData,u32 AudioBytes)
{
	u8 * curBuffer = soundBuffer + writeOffset;

	if(pAudioData)
		memcpy(curBuffer, pAudioData, AudioBytes);
	else
		memset(curBuffer, 0, AudioBytes);

	writeOffset += singleBufferBytes;
	writeOffset %= bufferSize;

	XAUDIO2_BUFFER xa2buffer={0};
	xa2buffer.AudioBytes=AudioBytes;
	xa2buffer.pAudioData=curBuffer;
	xa2buffer.pContext=NULL;
	InterlockedIncrement(&submittedBlocks);
	pSourceVoice->SubmitSourceBuffer(&xa2buffer);
}

static u32 min(u32 a, u32 b) { return (a < b) ? a : b; }

void SNDXA2UpdateAudio(s16 *buffer, u32 num_samples)
{
	if(submittedBlocks == 0) { // all blocks empty, fill silence
		for(int i=0;i<blockCount;i++)
			PushBuffer(0,singleBufferBytes);
		return;
	}
	u8 *sourceBuf = (u8 *)buffer;
	while(num_samples) {
		u32 bytesToWrite = min(num_samples * 4,singleBufferBytes);
		PushBuffer(sourceBuf, bytesToWrite);
		sourceBuf += bytesToWrite;
		num_samples -= bytesToWrite / 4;
	}
}
u32 SNDXA2GetAudioSpace()
{
	return (blockCount - submittedBlocks) * singleBufferSamples;
}
void SNDXA2MuteAudio()
{
	isMuted = true;
	pSourceVoice->SetVolume(0);
}
void SNDXA2UnMuteAudio()
{
	isMuted = false;
	pSourceVoice->SetVolume(soundVolume);
}
void SNDXA2SetVolume(int volume)
{
	soundVolume = volume/100.0f;
	if(!isMuted)
		pSourceVoice->SetVolume(soundVolume);
}
void SNDXA2ClearAudioBuffer()
{
	pSourceVoice->Stop(0);
	pSourceVoice->FlushSourceBuffers();
	pSourceVoice->Start(0);
	submittedBlocks = 0;
}