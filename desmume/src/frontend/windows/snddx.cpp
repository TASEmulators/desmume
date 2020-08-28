/*	
	Copyright (C) 2005-2007 Theo Berkau
	Copyright (C) 2006-2016 DeSmuME team

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

#include "snddx.h"

#include <stdio.h>
#include <Windows.h>
#include <mmsystem.h>
#include "directx/dsound.h"

#ifdef __MINGW32__
// I have to do this because for some reason because the dxerr8.h header is fubared
const char*  __stdcall DXGetErrorString8A(HRESULT hr);
#define DXGetErrorString8 DXGetErrorString8A
const char*  __stdcall DXGetErrorDescription8A(HRESULT hr);
#define DXGetErrorDescription8 DXGetErrorDescription8A
#else
#include "directx/dxerr8.h"
#endif

#include "SPU.h"
#include "CWindow.h"
#include "windriver.h"

int SNDDXInit(int buffersize);
void SNDDXDeInit();
void SNDDXUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDDXGetAudioSpace();
void SNDDXMuteAudio();
void SNDDXUnMuteAudio();
void SNDDXSetVolume(int volume);
void SNDDXClearAudioBuffer();

SoundInterface_struct SNDDIRECTX = {
	SNDCORE_DIRECTX,
	"Direct Sound Interface",
	SNDDXInit,
	SNDDXDeInit,
	SNDDXUpdateAudio,
	SNDDXGetAudioSpace,
	SNDDXMuteAudio,
	SNDDXUnMuteAudio,
	SNDDXSetVolume,
	SNDDXClearAudioBuffer,
};

LPDIRECTSOUND8 lpDS8;
LPDIRECTSOUNDBUFFER lpDSB, lpDSB2;

extern WINCLASS	*MainWindow;

static s16 *stereodata16=0;
static u32 soundoffset=0;
static u32 soundbufsize;
static LONG soundvolume;
static int issoundmuted;
static bool insilence;
static int samplecounter_fakecontribution = 0;

//////////////////////////////////////////////////////////////////////////////
static volatile bool doterminate;
static volatile bool terminated;

extern volatile int win_sound_samplecounter;

DWORD WINAPI SNDDXThread( LPVOID )
{
	for(;;) {
		if(doterminate) break;
		{
			Lock lock;
			SPU_Emulate_user();
		}
		Sleep(10);
	}
	terminated = true;
	return 0;
}

int SNDDXInit(int buffersize)
{
	DSBUFFERDESC dsbdesc;
	WAVEFORMATEX wfx;
	HRESULT ret;
	char tempstr[512];

	if (FAILED(ret = DirectSoundCreate8(NULL, &lpDS8, NULL)))
	{
		sprintf(tempstr, "DirectSound8Create error: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
		MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	if (FAILED(ret = lpDS8->SetCooperativeLevel(MainWindow->getHWnd(), DSSCL_PRIORITY)))
	{
		sprintf(tempstr, "IDirectSound8_SetCooperativeLevel error: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
		MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	memset(&dsbdesc, 0, sizeof(dsbdesc));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER;
	dsbdesc.dwBufferBytes = 0;
	dsbdesc.lpwfxFormat = NULL;

	if (FAILED(ret = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSB, NULL)))
	{
		sprintf(tempstr, "Error when creating primary sound buffer: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
		MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	soundbufsize = buffersize * 2; // caller already multiplies buffersize by 2
	soundoffset = 0;

	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = DESMUME_SAMPLE_RATE;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if (FAILED(ret = lpDSB->SetFormat(&wfx)))
	{
		sprintf(tempstr, "IDirectSoundBuffer8_SetFormat error: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
		MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	memset(&dsbdesc, 0, sizeof(dsbdesc));
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_STICKYFOCUS |
		DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2 |
		DSBCAPS_LOCHARDWARE;
	dsbdesc.dwBufferBytes = soundbufsize;
	dsbdesc.lpwfxFormat = &wfx;

	if (FAILED(ret = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSB2, NULL)))
	{
		if (ret == DSERR_CONTROLUNAVAIL ||
			ret == DSERR_INVALIDCALL ||
			ret == E_FAIL || 
			ret == E_NOTIMPL)
		{
			// Try using a software buffer instead
			dsbdesc.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_STICKYFOCUS |
				DSBCAPS_CTRLVOLUME | DSBCAPS_GETCURRENTPOSITION2 |
				DSBCAPS_LOCSOFTWARE;

			if (FAILED(ret = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSB2, NULL)))
			{
				sprintf(tempstr, "Error when creating secondary sound buffer: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
				MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
				return -1;
			}
		}
		else
		{
			sprintf(tempstr, "Error when creating secondary sound buffer: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
			MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
			return -1;
		}
	}

	lpDSB2->Play(0, 0, DSBPLAY_LOOPING);

	if ((stereodata16 = new s16[soundbufsize / sizeof(s16)]) == NULL)
		return -1;

	memset(stereodata16, 0, soundbufsize);

	soundvolume = DSBVOLUME_MAX;
	issoundmuted = 0;

	doterminate = false;
	terminated = false;
	CreateThread(0,0,SNDDXThread,0,0,0);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXDeInit()
{
	DWORD status=0;

	doterminate = true;
	while(!terminated)
		Sleep(1);
	terminated = false;

	if (lpDSB2)
	{
		lpDSB2->GetStatus(&status);

		if(status == DSBSTATUS_PLAYING)
			lpDSB2->Stop();

		lpDSB2->Release();
		lpDSB2 = NULL;
	}

	if (lpDSB)
	{
		lpDSB->Release();
		lpDSB = NULL;
	}

	if (lpDS8)
	{
		lpDS8->Release();
		lpDS8 = NULL;
	}

	delete stereodata16;
	stereodata16=0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXUpdateAudio(s16 *buffer, u32 num_samples)
{
	int samplecounter;
	{
		Lock lock;
		if(num_samples)
		{
			samplecounter = win_sound_samplecounter -= num_samples - samplecounter_fakecontribution;
			samplecounter_fakecontribution = 0;
		}
		else
		{
			samplecounter = win_sound_samplecounter -= DESMUME_SAMPLE_RATE/180;
			samplecounter_fakecontribution += DESMUME_SAMPLE_RATE/180;
		}
	}

	bool silence = (samplecounter<-DESMUME_SAMPLE_RATE*15/60); //behind by more than a quarter second -> silence

	if(insilence)
	{
		if(silence)
			return;
		else
			insilence = false;
	}
	else
	{
		if(silence)
		{
#ifndef PUBLIC_RELEASE
			extern volatile bool execute;
			if(execute)
				printf("snddx: emergency cleared sound buffer. (%d, %d, %d)\n", win_sound_samplecounter, num_samples, samplecounter_fakecontribution);
#endif
			samplecounter_fakecontribution = 0;
			insilence = true;
			SNDDXClearAudioBuffer();
			return;
		}
	}

	LPVOID buffer1;
	LPVOID buffer2;
	DWORD buffer1_size, buffer2_size;

	HRESULT hr = lpDSB2->Lock(soundoffset, num_samples * sizeof(s16) * 2,
	                          &buffer1, &buffer1_size, &buffer2, &buffer2_size, 0);
	if(FAILED(hr))
	{
		if(hr == DSBSTATUS_BUFFERLOST)
			lpDSB2->Restore();
		return;
	}

	memcpy(buffer1, buffer, buffer1_size);
	if(buffer2)
		memcpy(buffer2, ((u8 *)buffer)+buffer1_size, buffer2_size);

	soundoffset += buffer1_size + buffer2_size;
	soundoffset %= soundbufsize;

	lpDSB2->Unlock(buffer1, buffer1_size, buffer2, buffer2_size);
}



void SNDDXClearAudioBuffer()
{
	// we shouldn't need to provide 2 buffers since it's 1 contiguous range
	// but maybe newer directsound implementations have issues
	LPVOID buffer1;
	LPVOID buffer2;
	DWORD buffer1_size, buffer2_size;
	HRESULT hr = lpDSB2->Lock(0, 0, &buffer1, &buffer1_size, &buffer2, &buffer2_size, DSBLOCK_ENTIREBUFFER);
	if(FAILED(hr))
		return;
	memset(buffer1, 0, buffer1_size);
	if(buffer2)
		memset(buffer2, 0, buffer2_size);
	lpDSB2->Unlock(buffer1, buffer1_size, buffer2, buffer2_size);
}



//////////////////////////////////////////////////////////////////////////////

static inline u32 circularDist(u32 from, u32 to, u32 size)
{
	if(size == 0)
		return 0;
	s32 diff = (s32)(to - from);
	while(diff < 0)
		diff += size;
	return (u32)diff;
}

u32 SNDDXGetAudioSpace()
{
	DWORD playcursor, writecursor;
	if(FAILED(lpDSB2->GetCurrentPosition(&playcursor, &writecursor)))
		return 0;

	u32 curToWrite = circularDist(soundoffset, writecursor, soundbufsize);
	u32 curToPlay = circularDist(soundoffset, playcursor, soundbufsize);

	if(curToWrite < curToPlay)
		return 0; // in-between the two cursors. we shouldn't write anything during this time.

	return curToPlay / (sizeof(s16) * 2);
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXMuteAudio()
{
	issoundmuted = 1;
	lpDSB2->SetVolume(DSBVOLUME_MIN);
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXUnMuteAudio()
{
	issoundmuted = 0;
	lpDSB2->SetVolume(soundvolume);
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXSetVolume(int volume)
{
	if (!lpDSB2) return ;     //might happen when changing sounddevice on the fly, caused a gpf

	if(volume==0)
		soundvolume = DSBVOLUME_MIN;
	else
	{
		float attenuate = 1000 * (float)log(100.0f/volume);
		soundvolume = -(int)attenuate;
	}
	
	if (!issoundmuted)
		lpDSB2->SetVolume(soundvolume);
}

//////////////////////////////////////////////////////////////////////////////


