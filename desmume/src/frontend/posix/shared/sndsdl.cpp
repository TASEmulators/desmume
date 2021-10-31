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

#include <stdlib.h>
#include <string.h>

#include <SDL.h>
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

static u8 *stereodata;
static u8 *mixerdata;
static u32 soundoff;
static u32 soundbufsize;
static SDL_AudioSpec audiofmt;
int audio_volume;

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


static void MixAudio(void *userdata, Uint8 *stream, int len) {
   int i;
   /* if the volume is max, we don't need to call the mixer and do additional
      memory copying/cleaning, but can copy directly into the SDL buffer. */
   u8 *dest = audio_volume == SDL_MIX_MAXVOLUME ? stream : mixerdata;
   SDL_LockAudio();
   if (len > soundoff) {
        //dprintf(2, "bu: want %u, got %u\n", (unsigned)len, (unsigned)soundoff);
        /* buffer underrun - rather than zeroing out SDL's audio buffer, we just
           hand back the existing data, which reduces cracking */
	len = soundoff;
   }
   memcpy(dest, stereodata, len);
   soundoff -= len;
   /* move rest of prebuffered data to the beginning of the buffer */
   if (soundoff)
      memmove(stereodata, stereodata+len, soundoff);
   SDL_UnlockAudio();
   if(audio_volume != SDL_MIX_MAXVOLUME) {
      memset(stream, 0, len);
      SDL_MixAudio(stream, mixerdata, len, audio_volume);
   }
}

//////////////////////////////////////////////////////////////////////////////

int SNDSDLInit(int buffersize)
{
   if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
      return -1;

   audiofmt.freq = DESMUME_SAMPLE_RATE;
   audiofmt.format = AUDIO_S16SYS;
   audiofmt.channels = 2;
   audiofmt.samples = (audiofmt.freq / 60) * 2;
   audiofmt.callback = MixAudio;
   audiofmt.userdata = NULL;

   //samples should be a power of 2 according to SDL-doc
   //so normalize it to the nearest power of 2 here
   u32 normSamples = 512;
   while (normSamples < audiofmt.samples) normSamples <<= 1;

   audiofmt.samples = normSamples;

   SDL_AudioSpec obtained;
   if (SDL_OpenAudio(&audiofmt, &obtained) != 0)
   {
      return -1;
   }

   audiofmt = obtained;

   /* allocate multiple of the size needed by SDL to be able to efficiently pre-buffer.
      we effectively need the engine to pre-buffer a couple milliseconds because the
      frontend might call SDL_Delay() to achieve the desired framerate, during which
      the buffer isn't updated by the SPU. */
   soundbufsize = audiofmt.size * 4;
   soundoff = 0;

   if ((stereodata = (u8*)calloc(soundbufsize, 1)) == NULL ||
       (mixerdata  = (u8*)calloc(soundbufsize, 1)) == NULL)
      return -1;

#ifdef _XBOX
	doterminate = false;
	terminated = false;
	CreateThread(0,0,SNDXBOXThread,0,0,0);
#endif

   SDL_PauseAudio(0);

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
   SDL_CloseAudio();

   free(stereodata);
   free(mixerdata);
   stereodata = NULL;
   mixerdata = NULL;
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUpdateAudio(s16 *buffer, u32 num_samples)
{
   // dprintf(2, "up: %u\n", num_samples);
   u8 *incoming = (u8*) buffer;
   u32 n = num_samples * 4;
   SDL_LockAudio();
   memcpy(stereodata+soundoff, incoming, n);
   soundoff += n;
   SDL_UnlockAudio();
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDSDLGetAudioSpace()
{
   u32 tmp;
   SDL_LockAudio();
   tmp = (soundbufsize - soundoff)/4;
   SDL_UnlockAudio();
   return tmp;
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLMuteAudio()
{
   SDL_PauseAudio(1);
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUnMuteAudio()
{
   SDL_PauseAudio(0);
}

//////////////////////////////////////////////////////////////////////////////

/* the engine's min/max is 0/100, SDLs 0/128 */
void SNDSDLSetVolume(int volume)
{
   u32 tmp = volume * SDL_MIX_MAXVOLUME;
   SDL_LockAudio();
   audio_volume = tmp / 100;
   SDL_UnlockAudio();
}

//////////////////////////////////////////////////////////////////////////////
/* these 2 are special functions that the GTK frontend calls directly.
   and it has a slider where its max is 128. doh. */

int SNDSDLGetAudioVolume()
{
   SDL_LockAudio();
   int tmp = audio_volume;
   SDL_UnlockAudio();
   return tmp;
}

void SNDSDLSetAudioVolume(int volume) {
   SDL_LockAudio();
   audio_volume = volume;
   SDL_UnlockAudio();
}
