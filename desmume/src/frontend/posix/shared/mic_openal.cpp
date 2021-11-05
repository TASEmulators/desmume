/*
	Copyright (C) 2009-2010 DeSmuME team

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


#ifndef WIN32

#ifdef __APPLE__
  #include <OpenAL/al.h>
  #include <OpenAL/alc.h>
#else
  #include <AL/al.h>
  #include <AL/alc.h>
#endif

#include "types.h"
#include "mic.h"
#include "readwrite.h"
#include "emufile.h"
#include "debug.h"

#define MIC_BUFSIZE 512

BOOL Mic_Inited = FALSE;
u8 Mic_Buffer[2][2*MIC_BUFSIZE];
u16 Mic_BufPos;
u8 Mic_PlayBuf;
u8 Mic_WriteBuf;

int MicButtonPressed;

ALCdevice *alDevice;
ALCdevice *alCaptureDevice;
ALCcontext *alContext;

BOOL Mic_Init()
{
  ALenum err;
  const char *szDefaultCaptureDevice;

  if (!(alDevice = alcOpenDevice(0))) {
    INFO("Failed to Initialize Open AL\n");
    return FALSE;
  }
  if( !(alContext = alcCreateContext(alDevice, 0))) {
    INFO("Failed to create OpenAL context\n");
    return FALSE;
  }
  if( !alcMakeContextCurrent(alContext)) {
    INFO("Failed to make OpenAL context current\n");
    return FALSE;
  }
  alDistanceModel(AL_INVERSE_DISTANCE);

  szDefaultCaptureDevice = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
  LOG("Default OpenAL Capture Device is '%s'\n\n", szDefaultCaptureDevice);

  alCaptureDevice = alcCaptureOpenDevice(szDefaultCaptureDevice, 16000, AL_FORMAT_STEREO8, MIC_BUFSIZE);
  err = alGetError();
  if (err != AL_NO_ERROR) {
    INFO("Failed to alCaptureOpenDevice, ALenum %i\n", err);
    return 0;
  }
  alcCaptureStart(alCaptureDevice);

  Mic_Inited = TRUE;
  Mic_Reset();

  return TRUE;
}

void Mic_Reset()
{
  if (!Mic_Inited)
    return;

  memset(Mic_Buffer, 0, MIC_BUFSIZE*2*2);
  Mic_BufPos = 0;
  Mic_PlayBuf = 1;
  Mic_WriteBuf = 0;
}

void Mic_DeInit()
{
  if (!Mic_Inited)
    return;

  Mic_Inited = FALSE;

  if (alDevice) {
    if (alCaptureDevice) {
      alcCaptureStop(alCaptureDevice);
      alcCaptureCloseDevice(alCaptureDevice);
    }
    if (alContext) {
      alcMakeContextCurrent(0);
      alcDestroyContext(alContext);
    }
    alcCloseDevice(alDevice);
  }
}


static void alReadSound()
{
  ALenum err;
  int num;

  alcGetIntegerv(alCaptureDevice, ALC_CAPTURE_SAMPLES, 1, &num);
  if (num < MIC_BUFSIZE-1) {
    LOG("not enough microphone data waiting! (%i samples)\n", num);
  } else {
    LOG("%i samples waiting\n", num);
  }
  if (num > MIC_BUFSIZE)
    num = MIC_BUFSIZE;

  alcCaptureSamples(alCaptureDevice, Mic_Buffer[Mic_WriteBuf], num);
  err = alGetError();
  if (err != AL_NO_ERROR) {
    INFO("Failed to alcCaptureSamples, ALenum %i\n", err);
    return;
  }
}

u8 Mic_ReadSample()
{
  static u8 stats_max;
  static u8 stats_min;
  u8 ret;

  if (Mic_BufPos >= MIC_BUFSIZE) {
    alReadSound();
    Mic_BufPos = 0;
    Mic_PlayBuf ^= 1;
    Mic_WriteBuf ^= 1;
  }

  ret = Mic_Buffer[Mic_PlayBuf][Mic_BufPos >> 1];
  if (ret > stats_max)
    stats_max = ret;
  if (ret < stats_min)
    stats_min = ret;
  Mic_BufPos += 2;

  return ret;
}

void mic_savestate(EMUFILE &os)
{
	os.write_32LE(-1);
}

bool mic_loadstate(EMUFILE &is, int size)
{
	is.fseek(size, SEEK_CUR);
	return TRUE;
}

#endif

