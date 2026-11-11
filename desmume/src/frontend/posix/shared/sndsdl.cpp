/*
	Copyright (C) 2005-2006 Theo Berkau
	Copyright (C) 2006-2010 DeSmuME team

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
	Sound output through SDL3's AudioStream.

	The legacy SDL_OpenAudio()/callback model is gone in SDL3.  We now open a
	stream bound to the default playback device and push S16/stereo frames
	into it; SDL3 converts/resamples to whatever the hardware wants.  The
	stream's own queue replaces the two hand-rolled stereodata/mixerdata
	buffers and the SDL_LockAudio()/MixAudio() callback.  Volume is applied
	with SDL_SetAudioStreamGain() instead of mixing on the audio thread.
*/

#include <stdlib.h>
#include <string.h>

#include <SDL3/SDL.h>
#include "types.h"
#include "SPU.h"
#include "sndsdl.h"
#include "debug.h"

#ifdef _XBOX
#include <xtl.h>
#include <VectorIntrinsics.h>
#include <process.h>
#endif

int SNDSDLInit(int buffersize);
void SNDSDLDeInit();
void SNDSDLUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDSDLGetAudioSpace();
void SNDSDLMuteAudio();
void SNDSDLUnMuteAudio();
void SNDSDLSetVolume(int volume);

SoundInterface_struct SNDSDL = {
SNDCORE_SDL,
"SDL Sound Interface",
SNDSDLInit,
SNDSDLDeInit,
SNDSDLUpdateAudio,
SNDSDLGetAudioSpace,
SNDSDLMuteAudio,
SNDSDLUnMuteAudio,
SNDSDLSetVolume
};

/* Bytes per sample frame: stereo, signed 16-bit. */
#define SNDSDL_FRAME_BYTES 4

/* Upper bound on how much audio we let the stream queue.  The old callback
   backend pre-buffered four SDL buffers of 2048 frames each; keeping the
   same order of magnitude preserves its latency without a second ring
   buffer.  SNDSDLGetAudioSpace() reports the remaining room in frames. */
#define SNDSDL_QUEUE_FRAMES 8192
#define SNDSDL_QUEUE_BYTES  (SNDSDL_QUEUE_FRAMES * SNDSDL_FRAME_BYTES)

/* Stream bound to the default playback device.  Destroying it closes the
   device.  Its app side is always DESMUME_SAMPLE_RATE / SDL_AUDIO_S16 / 2. */
static SDL_AudioStream *audio_stream = NULL;
static bool audio_ref_held = false;

/* 0..SNDSDL_MAX_VOLUME, matching the engine's and the GTK slider's range. */
int audio_volume = 0;

//////////////////////////////////////////////////////////////////////////////
#ifdef _XBOX
static volatile bool doterminate;
static volatile bool terminated;

DWORD WINAPI SNDXBOXThread( LPVOID )
{
	for(;;) {
		if(doterminate) break;
		{
			SPU_Emulate_user();
		}
		Sleep(10);
	}
	terminated = true;
	return 0;
}
#endif

/* Keep the stream gain in sync with audio_volume.  SDL3 gain is a float
   multiplier where 1.0f corresponds to SNDSDL_MAX_VOLUME. */
static void sndsdl_apply_volume()
{
	if (audio_stream != NULL)
		SDL_SetAudioStreamGain(audio_stream, (float)audio_volume / SNDSDL_MAX_VOLUME);
}

static int sndsdl_clamp_volume(int volume)
{
	if (volume < 0) return 0;
	if (volume > SNDSDL_MAX_VOLUME) return SNDSDL_MAX_VOLUME;
	return volume;
}

//////////////////////////////////////////////////////////////////////////////

int SNDSDLInit(int buffersize)
{
	SDL_AudioSpec want;

	(void)buffersize;

	if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
		return -1;
	audio_ref_held = true;

	SDL_zero(want);
	want.freq = DESMUME_SAMPLE_RATE;
	want.format = SDL_AUDIO_S16;
	want.channels = 2;

	/* NULL callback = we push with SDL_PutAudioStreamData().  SDL3 converts
	   to the device's own format, so no "allowed changes" retry is needed. */
	audio_stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
	                                        &want, NULL, NULL);
	if (audio_stream == NULL)
	{
		fprintf(stderr, "SDL: failed to open audio device: %s\n", SDL_GetError());
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		audio_ref_held = false;
		return -1;
	}

	sndsdl_apply_volume();

#ifdef _XBOX
	doterminate = false;
	terminated = false;
	CreateThread(0,0,SNDXBOXThread,0,0,0);
#endif

	/* SDL_OpenAudioDeviceStream() starts the device paused. */
	SDL_ResumeAudioStreamDevice(audio_stream);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLDeInit()
{
#ifdef _XBOX
	doterminate = true;
	while(!terminated) {
		Sleep(1);
	}
#endif

	if (audio_stream != NULL)
	{
		SDL_PauseAudioStreamDevice(audio_stream);
		SDL_DestroyAudioStream(audio_stream);
		audio_stream = NULL;
	}

	if (audio_ref_held)
	{
		SDL_QuitSubSystem(SDL_INIT_AUDIO);
		audio_ref_held = false;
	}
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUpdateAudio(s16 *buffer, u32 num_samples)
{
	if (audio_stream == NULL)
		return;

	/* The SPU normally asks SNDSDLGetAudioSpace() first, but cap here too so
	   a missed query cannot make the stream's queue grow without bound. */
	u32 space = SNDSDLGetAudioSpace();
	if (num_samples > space)
		num_samples = space;
	if (num_samples == 0)
		return;

	SDL_PutAudioStreamData(audio_stream, buffer, (int)(num_samples * SNDSDL_FRAME_BYTES));
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDSDLGetAudioSpace()
{
	int queued;

	if (audio_stream == NULL)
		return 0;

	queued = SDL_GetAudioStreamQueued(audio_stream);
	if (queued < 0)
		return 0;
	if (queued >= SNDSDL_QUEUE_BYTES)
		return 0;

	return (u32)(SNDSDL_QUEUE_BYTES - queued) / SNDSDL_FRAME_BYTES;
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLMuteAudio()
{
	if (audio_stream != NULL)
		SDL_PauseAudioStreamDevice(audio_stream);
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUnMuteAudio()
{
	if (audio_stream != NULL)
		SDL_ResumeAudioStreamDevice(audio_stream);
}

//////////////////////////////////////////////////////////////////////////////

/* the engine's min/max is 0/100, ours is 0/SNDSDL_MAX_VOLUME */
void SNDSDLSetVolume(int volume)
{
	audio_volume = sndsdl_clamp_volume(volume * SNDSDL_MAX_VOLUME / 100);
	sndsdl_apply_volume();
}

//////////////////////////////////////////////////////////////////////////////
/* these 2 are special functions that the GTK frontend calls directly.
   and it has a slider where its max is SNDSDL_MAX_VOLUME. doh. */

int SNDSDLGetAudioVolume()
{
	return audio_volume;
}

void SNDSDLSetAudioVolume(int volume) {
	audio_volume = sndsdl_clamp_volume(volume);
	sndsdl_apply_volume();
}
