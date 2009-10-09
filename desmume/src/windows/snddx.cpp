/*	snddx.cpp
	
	Copyright (C) 2005-2007 Theo Berkau
	Copyright (C) 2008-2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <winsock2.h>
#include <stdio.h>
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
#include "snddx.h"
#include "CWindow.h"
#include "windriver.h"

int SNDDXInit(int buffersize);
void SNDDXDeInit();
void SNDDXUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDDXGetAudioSpace();
void SNDDXMuteAudio();
void SNDDXUnMuteAudio();
void SNDDXSetVolume(int volume);

SoundInterface_struct SNDDIRECTX = {
	SNDCORE_DIRECTX,
	"Direct Sound Interface",
	SNDDXInit,
	SNDDXDeInit,
	SNDDXUpdateAudio,
	SNDDXGetAudioSpace,
	SNDDXMuteAudio,
	SNDDXUnMuteAudio,
	SNDDXSetVolume
};

LPDIRECTSOUND8 lpDS8;
LPDIRECTSOUNDBUFFER lpDSB, lpDSB2;

extern WINCLASS	*MainWindow;

static s16 *stereodata16;
static u32 soundoffset=0;
static u32 soundbufsize;
static LONG soundvolume;
static int issoundmuted;

//////////////////////////////////////////////////////////////////////////////

//extern volatile int win_sound_samplecounter;
HANDLE hSNDDXThread = INVALID_HANDLE_VALUE;
extern HANDLE hSoundThreadWakeup;
bool bTerminateSoundThread = false;
bool bSilence = false;

DWORD WINAPI SNDDXThread( LPVOID )
{
	for(;;) 
	{
		if(bTerminateSoundThread) break;

		if (bSilence)
		{
			if (WaitForSingleObject(hSoundThreadWakeup, 10) == WAIT_OBJECT_0)
				bSilence = false;
		}
		else
		{
			// If the sound thread wakeup event is not signaled after a quarter second, output silence
			if (WaitForSingleObject(hSoundThreadWakeup, 250) == WAIT_TIMEOUT)
				bSilence = true;
		}

		{
			Lock lock;
			SPU_Emulate_user(!bSilence);
		}
	}

	return 0;
}

int SNDDXInit(int buffersize)
{
	DSBUFFERDESC dsbdesc;
	WAVEFORMATEX wfx;
	HRESULT ret;
	char tempstr[512];

	if ((ret = DirectSoundCreate8(NULL, &lpDS8, NULL)) != DS_OK)
	{
		sprintf(tempstr, "DirectSound8Create error: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
		MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	if ((ret = lpDS8->SetCooperativeLevel(MainWindow->getHWnd(), DSSCL_PRIORITY)) != DS_OK)
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

	if ((ret = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSB, NULL)) != DS_OK)
	{
		sprintf(tempstr, "Error when creating primary sound buffer: %s - %s", DXGetErrorString8(ret), DXGetErrorDescription8(ret));
		MessageBox (NULL, tempstr, "Error",  MB_OK | MB_ICONINFORMATION);
		return -1;
	}

	soundbufsize = buffersize * 2 * 2;

	memset(&wfx, 0, sizeof(wfx));
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 2;
	wfx.nSamplesPerSec = 44100;
	wfx.wBitsPerSample = 16;
	wfx.nBlockAlign = (wfx.wBitsPerSample / 8) * wfx.nChannels;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if ((ret = lpDSB->SetFormat(&wfx)) != DS_OK)
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

	if ((ret = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSB2, NULL)) != DS_OK)
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

			if ((ret = lpDS8->CreateSoundBuffer(&dsbdesc, &lpDSB2, NULL)) != DS_OK)
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

	bSilence = false;
	bTerminateSoundThread = false;
	hSNDDXThread = CreateThread(0, 0, SNDDXThread, 0, 0, 0);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXDeInit()
{
	DWORD status=0;

	bTerminateSoundThread = true;
	SetEvent(hSoundThreadWakeup);
	WaitForSingleObject(hSNDDXThread, INFINITE);

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
}

//////////////////////////////////////////////////////////////////////////////

void SNDDXUpdateAudio(s16 *buffer, u32 num_samples)
{
	LPVOID buffer1;
	LPVOID buffer2;
	DWORD buffer1_size, buffer2_size;
	DWORD status;

	lpDSB2->GetStatus(&status);

	if (status & DSBSTATUS_BUFFERLOST)
		return; // fix me

	lpDSB2->Lock(soundoffset, num_samples * sizeof(s16) * 2, &buffer1, &buffer1_size, &buffer2, &buffer2_size, 0);

	if(bSilence) 
	{
		memset(buffer1, 0, buffer1_size);
		if(buffer2)
			memset(buffer2, 0, buffer2_size);
	}
	else
	{
		memcpy(buffer1, buffer, buffer1_size);
		if (buffer2)
			memcpy(buffer2, ((u8 *)buffer)+buffer1_size, buffer2_size);
	}

	soundoffset += buffer1_size + buffer2_size;
	soundoffset %= soundbufsize;

	lpDSB2->Unlock(buffer1, buffer1_size, buffer2, buffer2_size);
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDDXGetAudioSpace()
{
	//return 735;
	DWORD playcursor, writecursor;
	u32 freespace=0;

	if (lpDSB2->GetCurrentPosition(&playcursor, &writecursor) != DS_OK)
		return 0;

	if (soundoffset > playcursor)
		freespace = soundbufsize - soundoffset + playcursor;
	else
		freespace = playcursor - soundoffset;

	//   if (freespace > 512)
	return (freespace / 2 / 2);
	//   else
	//      return 0;
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
	if (!lpDSB2) return ;     /* might happen when changing sounddevice on the fly, caused a gpf */
	soundvolume = (((LONG)volume) - 100) * 100;
	if (!issoundmuted)
		lpDSB2->SetVolume(soundvolume);
}

//////////////////////////////////////////////////////////////////////////////


