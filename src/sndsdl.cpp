/*  Copyright 2005-2006 Theo Berkau

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include "types.h"
#include "SPU.h"
#include "sndsdl.h"
#include "debug.h"

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

static u16 *stereodata16;
static u32 soundoffset;
static volatile u32 soundpos;
static u32 soundlen;
static u32 soundbufsize;
static SDL_AudioSpec audiofmt;

//////////////////////////////////////////////////////////////////////////////

static void MixAudio(void *userdata, Uint8 *stream, int len) {
   int i;
   Uint8 *soundbuf=(Uint8 *)stereodata16;

   for (i = 0; i < len; i++)
   {
      if (soundpos >= soundbufsize)
         soundpos = 0;

      stream[i] = soundbuf[soundpos];
      soundpos++;
   }
}

//////////////////////////////////////////////////////////////////////////////

int SNDSDLInit(int buffersize)
{
   if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0)
      return -1;

   audiofmt.freq = 44100;
   audiofmt.format = AUDIO_S16SYS;
   audiofmt.channels = 2;
   audiofmt.samples = (audiofmt.freq / 60) * 2;
   audiofmt.callback = MixAudio;
   audiofmt.userdata = NULL;

   //samples should be a power of 2 according to SDL-doc
   //so normalize it to the nearest power of 2 here
   u32 normSamples = 512;
   while (normSamples < audiofmt.samples) 
      normSamples <<= 1;

   audiofmt.samples = normSamples;
   
   soundlen = audiofmt.freq / 60; // 60 for NTSC
   soundbufsize = buffersize * sizeof(s16) * 2;

   if (SDL_OpenAudio(&audiofmt, NULL) != 0)
   {
      return -1;
   }

   if ((stereodata16 = (u16 *)malloc(soundbufsize)) == NULL)
      return -1;

   memset(stereodata16, 0, soundbufsize);

   soundpos = 0;

   SDL_PauseAudio(0);

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLDeInit()
{
   SDL_CloseAudio();

   if (stereodata16)
      free(stereodata16);
}

//////////////////////////////////////////////////////////////////////////////

void SNDSDLUpdateAudio(s16 *buffer, u32 num_samples)
{
   u32 copy1size=0, copy2size=0;
   SDL_LockAudio();

   if ((soundbufsize - soundoffset) < (num_samples * sizeof(s16) * 2))
   {
      copy1size = (soundbufsize - soundoffset);
      copy2size = (num_samples * sizeof(s16) * 2) - copy1size;
   }
   else
   {
      copy1size = (num_samples * sizeof(s16) * 2);
      copy2size = 0;
   }

   memcpy((((u8 *)stereodata16)+soundoffset), buffer, copy1size);
//   ScspConvert32uto16s((s32 *)leftchanbuffer, (s32 *)rightchanbuffer, (s16 *)(((u8 *)stereodata16)+soundoffset), copy1size / sizeof(s16) / 2);

   if (copy2size)
      memcpy(stereodata16, ((u8 *)buffer)+copy1size, copy2size);
//      ScspConvert32uto16s((s32 *)leftchanbuffer, (s32 *)rightchanbuffer, (s16 *)stereodata16, copy2size / sizeof(s16) / 2);

   soundoffset += copy1size + copy2size;
   soundoffset %= soundbufsize;

   SDL_UnlockAudio();
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDSDLGetAudioSpace()
{
   u32 freespace=0;

   if (soundoffset > soundpos)
      freespace = soundbufsize - soundoffset + soundpos;
   else
      freespace = soundpos - soundoffset;

   return (freespace / sizeof(s16) / 2);
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

void SNDSDLSetVolume(int volume)
{
}

//////////////////////////////////////////////////////////////////////////////
