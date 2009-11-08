/*  SPU.h

	Copyright 2006 Theo Berkau
    Copyright (C) 2006-2009 DeSmuME team

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

#ifndef SPU_H
#define SPU_H

#include <iosfwd>
#include <string>
#include <assert.h>
#include "types.h"
#include "matrix.h"
#include "emufile.h"


#define SNDCORE_DEFAULT         -1
#define SNDCORE_DUMMY           0

#define CHANSTAT_STOPPED          0
#define CHANSTAT_PLAY             1

static FORCEINLINE u32 sputrunc(float f) { return u32floor(f); }
static FORCEINLINE u32 sputrunc(double d) { return u32floor(d); }
static FORCEINLINE s32 spumuldiv7(s32 val, u8 multiplier) {
	assert(multiplier <= 127);
	return (multiplier == 127) ? val : ((val * multiplier) >> 7);
}

enum SPUInterpolationMode
{
	SPUInterpolation_None = 0,
	SPUInterpolation_Linear = 1,
	SPUInterpolation_Cosine = 2
};

struct SoundInterface_struct
{
   int id;
   const char *Name;
   int (*Init)(int buffersize);
   void (*DeInit)();
   void (*UpdateAudio)(s16 *buffer, u32 num_samples);
   u32 (*GetAudioSpace)();
   void (*MuteAudio)();
   void (*UnMuteAudio)();
   void (*SetVolume)(int volume);
   void (*ClearBuffer)();
};

extern SoundInterface_struct SNDDummy;
extern SoundInterface_struct SNDFile;
extern int SPU_currentCoreNum;

struct channel_struct
{
	channel_struct()
	{}
	u32 num;
   u8 vol;
   u8 datashift;
   u8 hold;
   u8 pan;
   u8 waveduty;
   u8 repeat;
   u8 format;
   u8 status;
   u32 addr;
   u16 timer;
   u16 loopstart;
   u32 length;
   u32 totlength;
   double double_totlength_shifted;
   union {
		s8 *buf8;
		s16 *buf16;
   };
   double sampcnt;
   double sampinc;
   // ADPCM specific
   u32 lastsampcnt;
   s16 pcm16b, pcm16b_last;
   s16 loop_pcm16b;
   int index;
   int loop_index;
   u16 x;
   s16 psgnoise_last;
} ;

class SPU_struct
{
public:
	SPU_struct(int buffersize);
   u32 bufpos;
   u32 buflength;
   s32 *sndbuf;
   s16 *outbuf;
   u32 bufsize;
   channel_struct channels[16];

   void reset();
   ~SPU_struct();
   void KeyOn(int channel);
   void WriteByte(u32 addr, u8 val);
   void WriteWord(u32 addr, u16 val);
   void WriteLong(u32 addr, u32 val);
   
   //kills all channels but leaves SPU otherwise running normally
   void ShutUp();
};

int SPU_ChangeSoundCore(int coreid, int buffersize);
SoundInterface_struct *SPU_SoundCore();

int SPU_Init(int coreid, int buffersize);
void SPU_Pause(int pause);
void SPU_SetVolume(int volume);
void SPU_SetSynchMode(int mode, int method);
void SPU_ClearOutputBuffer(void);
void SPU_Reset(void);
void SPU_DeInit(void);
void SPU_KeyOn(int channel);
void SPU_WriteByte(u32 addr, u8 val);
void SPU_WriteWord(u32 addr, u16 val);
void SPU_WriteLong(u32 addr, u32 val);
void SPU_Emulate_core(void);
void SPU_Emulate_user(bool mix = true);

extern SPU_struct *SPU_core, *SPU_user;
extern int spu_core_samples;

void spu_savestate(EMUFILE* os);
bool spu_loadstate(EMUFILE* is, int size);

enum WAVMode
{
	WAVMODE_ANY = -1,
	WAVMODE_CORE = 0,
	WAVMODE_USER = 1
};

class WavWriter
{
public:
	WavWriter();
	bool open(const std::string & fname);
	void close();
	void update(void* soundData, int numSamples);
	bool isRecording() const;
	WAVMode mode;
private:
	FILE *spufp;
};

void WAV_End();
bool WAV_Begin(const char* fname, WAVMode mode=WAVMODE_CORE);
bool WAV_IsRecording(WAVMode mode=WAVMODE_ANY);
void WAV_WavSoundUpdate(void* soundData, int numSamples, WAVMode mode=WAVMODE_CORE);

// we should make this configurable eventually
// but at least defining it somewhere is probably a step in the right direction
#define DESMUME_SAMPLE_RATE 44100
//#define DESMUME_SAMPLE_RATE 48000

#endif
