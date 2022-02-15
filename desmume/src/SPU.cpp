/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006 Theo Berkau
	Copyright (C) 2008-2021 DeSmuME team

	Ideas borrowed from Stephane Dallongeville's SCSP core

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


#define _USE_MATH_DEFINES
#include <math.h>
#ifndef M_PI
#define M_PI 3.1415926535897932386
#endif

#include <stdlib.h>
#include <string.h>
#include <queue>
#include <vector>

#include "debug.h"
#include "driver.h"
#include "MMU.h"
#include "SPU.h"
#include "mem.h"
#include "readwrite.h"
#include "armcpu.h"
#include "NDSSystem.h"
#include "emufile.h"
#include "matrix.h"
#include "utils/bits.h"


static inline s16 read16(u32 addr) { return (s16)_MMU_read16<ARMCPU_ARM7,MMU_AT_DEBUG>(addr); }
static inline u8 read08(u32 addr) { return _MMU_read08<ARMCPU_ARM7,MMU_AT_DEBUG>(addr); }
static inline s8 read_s8(u32 addr) { return (s8)_MMU_read08<ARMCPU_ARM7,MMU_AT_DEBUG>(addr); }

#define K_ADPCM_LOOPING_RECOVERY_INDEX 99999
#define COSINE_INTERPOLATION_RESOLUTION 8192

//#ifdef FASTBUILD
	#undef FORCEINLINE
	#define FORCEINLINE
//#endif

//static ISynchronizingAudioBuffer* _currentSynchronizer = metaspu_construct(ESynchMethod_Z);
static ISynchronizingAudioBuffer* _currentSynchronizer = metaspu_construct(ESynchMethod_N);

SPU_struct *SPU_core = 0;
SPU_struct *SPU_user = 0;
int SPU_currentCoreNum = SNDCORE_DUMMY;
static int _currentVolume = 100;


static size_t _currentBufferSize = 0;
static ESynchMode _currentSynchMode = ESynchMode_DualSynchAsynch;
static ESynchMethod _currentSynchMethod = ESynchMethod_N;

static int _currentSNDCoreId = -1;
static SoundInterface_struct *_currentSNDCore = NULL;
extern SoundInterface_struct *SNDCoreList[];

//const int shift = (FORMAT == 0 ? 2 : 1);
static const int format_shift[] = { 2, 1, 3, 0 };
static const u8 volume_shift[] = { 0, 1, 2, 4 };

static const s8 indextbl[8] =
{
	-1, -1, -1, -1, 2, 4, 6, 8
};

static const u16 adpcmtbl[89] =
{
	0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010,
	0x0011, 0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025,
	0x0029, 0x002D, 0x0032, 0x0037, 0x003C, 0x0042, 0x0049, 0x0050, 0x0058,
	0x0061, 0x006B, 0x0076, 0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1,
	0x00E6, 0x00FD, 0x0117, 0x0133, 0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE,
	0x0220, 0x0256, 0x0292, 0x02D4, 0x031C, 0x036C, 0x03C3, 0x0424, 0x048E,
	0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812, 0x08E0, 0x09C3, 0x0ABD,
	0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE, 0x1706, 0x1954,
	0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B, 0x3BB9,
	0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF
};

static const s16 wavedutytbl[8][8] = {
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF }
};

static s32 precalcdifftbl[89][16];
static u8 precalcindextbl[89][8];
static double cos_lut[COSINE_INTERPOLATION_RESOLUTION];

static const double ARM7_CLOCK = 33513982;

static const double samples_per_hline = (DESMUME_SAMPLE_RATE / 59.8261f) / 263.0f;

static double _samples = 0;
int spu_core_samples = 0;

template<typename T>
static FORCEINLINE T MinMax(T val, T min, T max)
{
	if (val < min)
		return min;
	else if (val > max)
		return max;

	return val;
}

//--------------external spu interface---------------

int SPU_ChangeSoundCore(int coreid, int newBufferSizeBytes)
{
	int i;

	_currentBufferSize = newBufferSizeBytes;

	delete SPU_user; SPU_user = NULL;

	// Make sure the old core is freed
	if (_currentSNDCore)
		_currentSNDCore->DeInit();

	// So which core do we want?
	if (coreid == SNDCORE_DEFAULT)
		coreid = 0; // Assume we want the first one

	SPU_currentCoreNum = coreid;

	// Go through core list and find the id
	for (i = 0; SNDCoreList[i] != NULL; i++)
	{
		if (SNDCoreList[i]->id == coreid)
		{
			// Set to current core
			_currentSNDCore = SNDCoreList[i];
			break;
		}
	}

	_currentSNDCoreId = coreid;

	//If the user picked the dummy core, disable the user spu
	if (_currentSNDCore == &SNDDummy)
		return 0;

	//If the core wasnt found in the list for some reason, disable the user spu
	if (_currentSNDCore == NULL)
		return -1;

	// Since it failed, instead of it being fatal, disable the user spu
	if (_currentSNDCore->Init(_currentBufferSize * 2) == -1)
	{
		_currentSNDCore = 0;
		return -1;
	}

	_currentSNDCore->SetVolume(_currentVolume);

	SPU_SetSynchMode(_currentSynchMode, _currentSynchMethod);

	return 0;
}

SoundInterface_struct *SPU_SoundCore()
{
	return _currentSNDCore;
}

void SPU_ReInit(bool fakeBoot)
{
	SPU_Init(_currentSNDCoreId, _currentBufferSize);

	// Firmware set BIAS to 0x200
	if (fakeBoot)
		SPU_WriteWord(0x04000504, 0x0200);
}

int SPU_Init(int coreid, int newBufferSizeBytes)
{
	// Build the cosine interpolation LUT
	for (size_t i = 0; i < COSINE_INTERPOLATION_RESOLUTION; i++)
		cos_lut[i] = (1.0 - cos(((double)i/(double)COSINE_INTERPOLATION_RESOLUTION) * M_PI)) * 0.5;

	SPU_core = new SPU_struct((int)ceil(samples_per_hline));
	SPU_Reset();

	//create adpcm decode accelerator lookups
	for (size_t i = 0; i < 16; i++)
	{
		for (size_t j = 0; j < 89; j++)
		{
			precalcdifftbl[j][i] = (((i & 0x7) * 2 + 1) * adpcmtbl[j] / 8);
			if (i & 0x8) precalcdifftbl[j][i] = -precalcdifftbl[j][i];
		}
	}
	for (size_t i = 0; i < 8; i++)
	{
		for (s8 j = 0; j < 89; j++)
		{
			const s8 idx = MinMax((j + indextbl[i]), 0, 88);
			precalcindextbl[j][i] = (u8)idx;
		}
	}

	SPU_SetSynchMode(CommonSettings.SPU_sync_mode, CommonSettings.SPU_sync_method);

	return SPU_ChangeSoundCore(coreid, newBufferSizeBytes);
}

void SPU_Pause(int pause)
{
	if (_currentSNDCore == NULL) return;

	if (pause)
		_currentSNDCore->MuteAudio();
	else
		_currentSNDCore->UnMuteAudio();
}

void SPU_CloneUser()
{
	if(SPU_user) {
		memcpy(SPU_user->channels,SPU_core->channels,sizeof(SPU_core->channels));
		SPU_user->regs = SPU_core->regs;
	}
}


void SPU_SetSynchMode(int mode, int method)
{
	_currentSynchMode = (ESynchMode)mode;
	if (_currentSynchMethod != (ESynchMethod)method)
	{
		_currentSynchMethod = (ESynchMethod)method;
		delete _currentSynchronizer;
		//grr does this need to be locked? spu might need a lock method
		  // or maybe not, maybe the platform-specific code that calls this function can deal with it.
		_currentSynchronizer = metaspu_construct(_currentSynchMethod);
	}

	delete SPU_user;
	SPU_user = NULL;
		
	if (_currentSynchMode == ESynchMode_DualSynchAsynch)
	{
		SPU_user = new SPU_struct(_currentBufferSize);
		SPU_CloneUser();
	}
}

void SPU_ClearOutputBuffer()
{
	if (_currentSNDCore && _currentSNDCore->ClearBuffer)
		_currentSNDCore->ClearBuffer();
}

void SPU_SetVolume(int newVolume)
{
	_currentVolume = newVolume;
	if (_currentSNDCore)
		_currentSNDCore->SetVolume(newVolume);
}


void SPU_Reset(void)
{
	int i;

	SPU_core->reset();

	if (SPU_user)
	{
		if (_currentSNDCore)
		{
			_currentSNDCore->DeInit();
			_currentSNDCore->Init(SPU_user->bufsize*2);
			_currentSNDCore->SetVolume(_currentVolume);
		}
		SPU_user->reset();
	}

	//zero - 09-apr-2010: this concerns me, regarding savestate synch.
	//After 0.9.6, lets experiment with removing it and just properly zapping the spu instead
	// Reset Registers
	for (i = 0x400; i < 0x51D; i++)
		T1WriteByte(MMU.ARM7_REG, i, 0);

	_samples = 0;
}

//------------------------------------------

void SPU_struct::reset()
{
	memset(sndbuf,0,bufsize*2*4);
	memset(outbuf,0,bufsize*2*2);

	memset((void *)channels, 0, sizeof(channel_struct) * 16);

	reconstruct(&regs);

	for(int i = 0; i < 16; i++)
	{
		channels[i].num = i;
	}
}

SPU_struct::SPU_struct(int buffersize)
	: bufpos(0)
	, buflength(0)
	, sndbuf(0)
	, outbuf(0)
	, bufsize(buffersize)
{
	sndbuf = new s32[buffersize*2];
	outbuf = new s16[buffersize*2];
	reset();
}

SPU_struct::~SPU_struct()
{
	if(sndbuf) delete[] sndbuf;
	if(outbuf) delete[] outbuf;
}

void SPU_DeInit(void)
{
	if (_currentSNDCore)
		_currentSNDCore->DeInit();
	_currentSNDCore = 0;

	delete SPU_core; SPU_core=0;
	delete SPU_user; SPU_user=0;
}

//////////////////////////////////////////////////////////////////////////////

void SPU_struct::ShutUp()
{
	for(int i=0;i<16;i++)
		 channels[i].status = CHANSTAT_STOPPED;
}

static FORCEINLINE void adjust_channel_timer(channel_struct *chan)
{
	chan->sampinc = (((double)ARM7_CLOCK) / (DESMUME_SAMPLE_RATE * 2)) / (double)(0x10000 - chan->timer);
}

void SPU_struct::KeyProbe(int chan_num)
{
	channel_struct &thischan = channels[chan_num];
	if(thischan.status == CHANSTAT_STOPPED)
	{
		if(thischan.keyon && regs.masteren)
			KeyOn(chan_num);
	}
	else if(thischan.status == CHANSTAT_PLAY)
	{
		if(!thischan.keyon || !regs.masteren)
			KeyOff(chan_num);
	}
}

void SPU_struct::KeyOff(int channel)
{
	//printf("keyoff%d\n",channel);
	channel_struct &thischan = channels[channel];
	thischan.status = CHANSTAT_STOPPED;
}

void SPU_struct::KeyOn(int channel)
{
	channel_struct &thischan = channels[channel];
	thischan.status = CHANSTAT_PLAY;

	thischan.totlength = thischan.length + thischan.loopstart;
	adjust_channel_timer(&thischan);

	//printf("keyon %d totlength:%d\n",channel,thischan.totlength);


	//LOG("Channel %d key on: vol = %d, volumeDiv = %d, hold = %d, pan = %d, waveduty = %d, repeat = %d, format = %d, source address = %07X,"
	//		"timer = %04X, loop start = %04X, length = %06X, MMU.ARM7_REG[0x501] = %02X\n", channel, chan->vol, chan->volumeDiv, chan->hold, 
	//		chan->pan, chan->waveduty, chan->repeat, chan->format, chan->addr, chan->timer, chan->loopstart, chan->length, T1ReadByte(MMU.ARM7_REG, 0x501));

	switch(thischan.format)
	{
	case 0: // 8-bit
	//	thischan.loopstart = thischan.loopstart << 2;
	//	thischan.length = (thischan.length << 2) + thischan.loopstart;
		thischan.sampcnt = -3;
		break;
	case 1: // 16-bit
	//	thischan.loopstart = thischan.loopstart << 1;
	//	thischan.length = (thischan.length << 1) + thischan.loopstart;
		thischan.sampcnt = -3;
		break;
	case 2: // ADPCM
		{
			thischan.pcm16b = (s16)read16(thischan.addr);
			thischan.pcm16b_last = thischan.pcm16b;
			thischan.index = read08(thischan.addr + 2) & 0x7F;;
			thischan.lastsampcnt = 7;
			thischan.sampcnt = -3;
			thischan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;
		//	thischan.loopstart = thischan.loopstart << 3;
		//	thischan.length = (thischan.length << 3) + thischan.loopstart;
			break;
		}
	case 3: // PSG
		{
			thischan.sampcnt = -1;
			thischan.x = 0x7FFF;
			break;
		}
	default: break;
	}

	thischan.double_totlength_shifted = (double)(thischan.totlength << format_shift[thischan.format]);

	if(thischan.format != 3)
	{
		if(thischan.double_totlength_shifted == 0)
		{
			printf("INFO: Stopping channel %d due to zero length\n",channel);
			thischan.status = CHANSTAT_STOPPED;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

u8 SPU_struct::ReadByte(u32 addr)
{
	//individual channel regs
	if ((addr & 0x0F00) == 0x0400)
	{
		u32 chan_num = (addr >> 4) & 0xF;
		const channel_struct& thischan = channels[chan_num];

		switch (addr & 0xF)
		{
			case 0x0: return thischan.vol;
			case 0x1: return (thischan.volumeDiv | (thischan.hold << 7));
			case 0x2: return thischan.pan;
			case 0x3: return (	thischan.waveduty
								| (thischan.repeat << 3)
								| (thischan.format << 5)
								| ((thischan.status == CHANSTAT_PLAY)?0x80:0)
								);
			case 0x8: return thischan.timer >> 0;
			case 0x9: return thischan.timer >> 8;
			case 0xA: return thischan.loopstart >> 0;
			case 0xB: return thischan.loopstart >> 8;
		}
		return 0;
	}

	switch(addr)
	{
		//SOUNDCNT
		case 0x500: return regs.mastervol;
		case 0x501: return (regs.ctl_left
							| (regs.ctl_right << 2)
							| (regs.ctl_ch1bypass << 4)
							| (regs.ctl_ch3bypass << 5)
							| (regs.masteren << 7)
							);

		//SOUNDBIAS
		case 0x504: return regs.soundbias >> 0;
		case 0x505: return regs.soundbias >> 8;
	
		//SNDCAP0CNT/SNDCAP1CNT
		case 0x508:
		case 0x509:
		{
			u32 which = (addr - 0x508);
			return regs.cap[which].add
				| (regs.cap[which].source << 1)
				| (regs.cap[which].oneshot << 2)
				| (regs.cap[which].bits8 << 3)
				| (regs.cap[which].runtime.running << 7);
		}	

		//SNDCAP0DAD
		case 0x510: return regs.cap[0].dad >> 0;
		case 0x511: return regs.cap[0].dad >> 8;
		case 0x512: return regs.cap[0].dad >> 16;
		case 0x513: return regs.cap[0].dad >> 24;

		//SNDCAP0LEN
		case 0x514: return regs.cap[0].len >> 0;
		case 0x515: return regs.cap[0].len >> 8;

		//SNDCAP1DAD
		case 0x518: return regs.cap[1].dad >> 0;
		case 0x519: return regs.cap[1].dad >> 8;
		case 0x51A: return regs.cap[1].dad >> 16;
		case 0x51B: return regs.cap[1].dad >> 24;

		//SNDCAP1LEN
		case 0x51C: return regs.cap[1].len >> 0;
		case 0x51D: return regs.cap[1].len >> 8;
	} //switch on address

	return 0;
}

u16 SPU_struct::ReadWord(u32 addr)
{
	//individual channel regs
	if ((addr & 0x0F00) == 0x0400)
	{
		u32 chan_num = (addr >> 4) & 0xF;
		const channel_struct& thischan = channels[chan_num];

		switch (addr & 0xF)
		{
			case 0x0: return	(thischan.vol
								| (thischan.volumeDiv << 8)
								| (thischan.hold << 15)
								);
			case 0x2: return	(thischan.pan
								| (thischan.waveduty << 8)
								| (thischan.repeat << 11)
								| (thischan.format << 13)
								| ((thischan.status == CHANSTAT_PLAY)?(1 << 15):0)
								);
			case 0x8: return thischan.timer;
			case 0xA: return thischan.loopstart;
		} //switch on individual channel regs
		return 0;
	}

	switch(addr)
	{
		//SOUNDCNT
		case 0x500: return	(regs.mastervol
					| (regs.ctl_left << 8)
					| (regs.ctl_right << 10)
					| (regs.ctl_ch1bypass << 12)
					| (regs.ctl_ch3bypass << 13)
					| (regs.masteren << 15)
					);

		//SOUNDBIAS
		case 0x504: return regs.soundbias;
	
		//SNDCAP0CNT/SNDCAP1CNT
		case 0x508:
		{
			u8 val0 =	regs.cap[0].add
						| (regs.cap[0].source << 1)
						| (regs.cap[0].oneshot << 2)
						| (regs.cap[0].bits8 << 3)
						| (regs.cap[0].runtime.running << 7);
			u8 val1 =	regs.cap[1].add
						| (regs.cap[1].source << 1)
						| (regs.cap[1].oneshot << 2)
						| (regs.cap[1].bits8 << 3)
						| (regs.cap[1].runtime.running << 7);
			return (u16)(val0 | (val1 << 8));
		}	

		//SNDCAP0DAD
		case 0x510: return regs.cap[0].dad >> 0;
		case 0x512: return regs.cap[0].dad >> 16;

		//SNDCAP0LEN
		case 0x514: return regs.cap[0].len;

		//SNDCAP1DAD
		case 0x518: return regs.cap[1].dad >> 0;
		case 0x51A: return regs.cap[1].dad >> 16;

		//SNDCAP1LEN
		case 0x51C: return regs.cap[1].len;
	} //switch on address

	return 0;
}

u32 SPU_struct::ReadLong(u32 addr)
{
	//individual channel regs
	if ((addr & 0x0F00) == 0x0400)
	{
		u32 chan_num = (addr >> 4) & 0xF;
		channel_struct &thischan=channels[chan_num];

		switch (addr & 0xF)
		{
			case 0x0: return	(thischan.vol
								| (thischan.volumeDiv << 8)
								| (thischan.hold << 15)
								| (thischan.pan << 16)
								| (thischan.waveduty << 24)
								| (thischan.repeat << 27)
								| (thischan.format << 29)
								| ((thischan.status == CHANSTAT_PLAY)?(1 << 31):0)
								);
			case 0x8: return (thischan.timer | (thischan.loopstart << 16));
		} //switch on individual channel regs
		return 0;
	}

	switch(addr)
	{
		//SOUNDCNT
		case 0x500: return	(regs.mastervol
							| (regs.ctl_left << 8)
							| (regs.ctl_right << 10)
							| (regs.ctl_ch1bypass << 12)
							| (regs.ctl_ch3bypass << 13)
							| (regs.masteren << 15)
							);

		//SOUNDBIAS
		case 0x504: return (u32)regs.soundbias;
	
		//SNDCAP0CNT/SNDCAP1CNT
		case 0x508:
		{
			u8 val0 =	regs.cap[0].add
						| (regs.cap[0].source << 1)
						| (regs.cap[0].oneshot << 2)
						| (regs.cap[0].bits8 << 3)
						| (regs.cap[0].runtime.running << 7);
			u8 val1 =	regs.cap[1].add
						| (regs.cap[1].source << 1)
						| (regs.cap[1].oneshot << 2)
						| (regs.cap[1].bits8 << 3)
						| (regs.cap[1].runtime.running << 7);
			return (u32)(val0 | (val1 << 8));
		}	

		//SNDCAP0DAD
		case 0x510: return regs.cap[0].dad;

		//SNDCAP0LEN
		case 0x514: return (u32)regs.cap[0].len;

		//SNDCAP1DAD
		case 0x518: return regs.cap[1].dad;

		//SNDCAP1LEN
		case 0x51C: return (u32)regs.cap[1].len;
	} //switch on address
	
	return 0;
}

SPUFifo::SPUFifo()
{
	reset();
}

void SPUFifo::reset()
{
	head = tail = size = 0;
}

void SPUFifo::enqueue(s16 val)
{
	if(size==16) return;
	buffer[tail] = val;
	tail++;
	tail &= 15;
	size++;
}

s16 SPUFifo::dequeue()
{
	if(size==0) return 0;
	head++;
	head &= 15;
	s16 ret = buffer[head];
	size--;
	return ret;
}

void SPUFifo::save(EMUFILE &fp)
{
	u32 version = 1;
	fp.write_32LE(version);
	fp.write_32LE(head);
	fp.write_32LE(tail);
	fp.write_32LE(size);
	for (int i = 0; i < 16; i++)
		fp.write_16LE(buffer[i]);
}

bool SPUFifo::load(EMUFILE &fp)
{
	u32 version;
	if (fp.read_32LE(version) != 1) return false;
	fp.read_32LE(head);
	fp.read_32LE(tail);
	fp.read_32LE(size);
	for (int i = 0; i < 16; i++)
		fp.read_16LE(buffer[i]);
	return true;
}

void SPU_struct::ProbeCapture(int which)
{
	//VERY UNTESTED -- HOW MUCH OF THIS RESETS, AND WHEN?

	if(!regs.cap[which].active)
	{
		regs.cap[which].runtime.running = 0;
		return;
	}

	REGS::CAP &cap = regs.cap[which];
	cap.runtime.running = 1;
	cap.runtime.curdad = cap.dad;
	u32 len = cap.len;
	if(len==0) len=1;
	cap.runtime.maxdad = cap.dad + len*4;
	cap.runtime.sampcnt = 0;
	cap.runtime.fifo.reset();
}

void SPU_struct::WriteByte(u32 addr, u8 val)
{
	//individual channel regs
	if ((addr & 0x0F00) == 0x0400)
	{
		u8 chan_num = (addr >> 4) & 0xF;
		channel_struct &thischan = channels[chan_num];

		//printf("SPU write08 channel%d, reg %04X, val %02X\n", chan_num, addr, val);

		switch (addr & 0x000F)
		{
			case 0x0: thischan.vol = (val & 0x7F); break;
			case 0x1:
				thischan.volumeDiv = (val & 0x03);
				thischan.hold = (val >> 7) & 0x01;
				break;
			case 0x2: thischan.pan = (val & 0x7F); break;
			case 0x3:
				thischan.waveduty = (val & 0x07);
				thischan.repeat = (val >> 3) & 0x03;
				thischan.format = (val >> 5) & 0x03;
				thischan.keyon = (val >> 7) & 0x01;
				KeyProbe(chan_num);
				break;
			case 0x4: thischan.addr &= 0xFFFFFF00; thischan.addr |= (val & 0xFC); break;
			case 0x5: thischan.addr &= 0xFFFF00FF; thischan.addr |= (val << 8); break;
			case 0x6: thischan.addr &= 0xFF00FFFF; thischan.addr |= (val << 16); break;
			case 0x7: thischan.addr &= 0x00FFFFFF; thischan.addr |= ((val&7) << 24); break; //only 27 bits of this register are used
			case 0x8: thischan.timer &= 0xFF00; thischan.timer |= (val << 0); adjust_channel_timer(&thischan); break;
			case 0x9: thischan.timer &= 0x00FF; thischan.timer |= (val << 8); adjust_channel_timer(&thischan); break;

			case 0xA: thischan.loopstart &= 0xFF00; thischan.loopstart |= (val << 0); break;
			case 0xB: thischan.loopstart &= 0x00FF; thischan.loopstart |= (val << 8); break;
			case 0xC: thischan.length &= 0xFFFFFF00; thischan.length |= (val << 0); break;
			case 0xD: thischan.length &= 0xFFFF00FF; thischan.length |= (val << 8); break;
			case 0xE: thischan.length &= 0xFF00FFFF; thischan.length |= ((val & 0x3F) << 16); //only 22 bits of this register are used
			case 0xF: break;

		} //switch on individual channel regs

		return;
	}

	switch(addr)
	{
		//SOUNDCNT
		case 0x500: regs.mastervol = (val & 0x7F); break;
		case 0x501:
			regs.ctl_left  = (val >> 0) & 3;
			regs.ctl_right = (val >> 2) & 3;
			regs.ctl_ch1bypass = (val >> 4) & 1;
			regs.ctl_ch3bypass = (val >> 5) & 1;
			regs.masteren = (val >> 7) & 1;
			//from r4925 - after changing 'masteren', we retrigger any sounds? doubtful. 
			//maybe we STOP sounds here, but we don't enable them (this would retrigger any previous sounds that had finished; glitched AC:WW)
			//(probably broken in r3299)
			//after commenting this out, I checked bug #1356. seems unrelated.
			//for(int i=0; i<16; i++) KeyProbe(i);
		break;
		
		//SOUNDBIAS
		case 0x504: regs.soundbias &= 0xFF00; regs.soundbias |= (val << 0); break;
		case 0x505: regs.soundbias &= 0x00FF; regs.soundbias |= ((val&3) << 8); break;

		//SNDCAP0CNT/SNDCAP1CNT
		case 0x508:
		case 0x509:
		{
			u32 which = (addr - 0x508);
			regs.cap[which].add = BIT0(val);
			regs.cap[which].source = BIT1(val);
			regs.cap[which].oneshot = BIT2(val);
			regs.cap[which].bits8 = BIT3(val);
			regs.cap[which].active = BIT7(val);
			ProbeCapture(which);
			break;
		}

		//SNDCAP0DAD
		case 0x510: regs.cap[0].dad &= 0xFFFFFF00; regs.cap[0].dad |= (val & 0xFC); break;
		case 0x511: regs.cap[0].dad &= 0xFFFF00FF; regs.cap[0].dad |= (val << 8); break;
		case 0x512: regs.cap[0].dad &= 0xFF00FFFF; regs.cap[0].dad |= (val << 16); break;
		case 0x513: regs.cap[0].dad &= 0x00FFFFFF; regs.cap[0].dad |= ((val&7) << 24); break;

		//SNDCAP0LEN
		case 0x514: regs.cap[0].len &= 0xFF00; regs.cap[0].len |= (val << 0); break;
		case 0x515: regs.cap[0].len &= 0x00FF; regs.cap[0].len |= (val << 8); break;

		//SNDCAP1DAD
		case 0x518: regs.cap[1].dad &= 0xFFFFFF00; regs.cap[1].dad |= (val & 0xFC); break;
		case 0x519: regs.cap[1].dad &= 0xFFFF00FF; regs.cap[1].dad |= (val << 8); break;
		case 0x51A: regs.cap[1].dad &= 0xFF00FFFF; regs.cap[1].dad |= (val << 16); break;
		case 0x51B: regs.cap[1].dad &= 0xFF000000; regs.cap[1].dad |= ((val&7) << 24); break;

		//SNDCAP1LEN
		case 0x51C: regs.cap[1].len &= 0xFF00; regs.cap[1].len |= (val << 0); break;
		case 0x51D: regs.cap[1].len &= 0x00FF; regs.cap[1].len |= (val << 8); break;
	} //switch on address
}

void SPU_struct::WriteWord(u32 addr, u16 val)
{
	//individual channel regs
	if ((addr & 0x0F00) == 0x0400)
	{
		u32 chan_num = (addr >> 4) & 0xF;
		channel_struct &thischan=channels[chan_num];

		//printf("SPU write16 channel%d, reg %04X, val %04X\n", chan_num, addr, val);

		switch (addr & 0xF)
		{
			case 0x0:
				thischan.vol = (val & 0x7F);
				thischan.volumeDiv = (val >> 8) & 0x3;
				thischan.hold = (val >> 15) & 0x1;
				break;
			case 0x2:
				thischan.pan = (val & 0x7F);
				thischan.waveduty = (val >> 8) & 0x7;
				thischan.repeat = (val >> 11) & 0x3;
				thischan.format = (val >> 13) & 0x3;
				thischan.keyon = (val >> 15) & 0x1;
				KeyProbe(chan_num);
				break;
			case 0x4: thischan.addr &= 0xFFFF0000; thischan.addr |= (val & 0xFFFC); break;
			case 0x6: thischan.addr &= 0x0000FFFF; thischan.addr |= ((val & 0x07FF) << 16); break;
			case 0x8: thischan.timer = val; adjust_channel_timer(&thischan); break;
			case 0xA: thischan.loopstart = val; break;
			case 0xC: thischan.length &= 0xFFFF0000; thischan.length |= (val << 0); break;
			case 0xE: thischan.length &= 0x0000FFFF; thischan.length |= ((val & 0x003F) << 16); break;
		} //switch on individual channel regs
		return;
	}

	switch (addr)
	{
		//SOUNDCNT
		case 0x500:
			regs.mastervol = (val & 0x7F);
			regs.ctl_left  = (val >> 8) & 0x03;
			regs.ctl_right = (val >> 10) & 0x03;
			regs.ctl_ch1bypass = (val >> 12) & 0x01;
			regs.ctl_ch3bypass = (val >> 13) & 0x01;
			regs.masteren = (val >> 15) & 0x01;
			for(u8 i=0; i<16; i++)
				KeyProbe(i);
			break;

		//SOUNDBIAS
		case 0x504: regs.soundbias = (val & 0x3FF); break;

		//SNDCAP0CNT/SNDCAP1CNT
		case 0x508:
		{
			regs.cap[0].add = BIT0(val);
			regs.cap[0].source = BIT1(val);
			regs.cap[0].oneshot = BIT2(val);
			regs.cap[0].bits8 = BIT3(val);
			regs.cap[0].active = BIT7(val);
			ProbeCapture(0);

			regs.cap[1].add = BIT8(val);
			regs.cap[1].source = BIT9(val);
			regs.cap[1].oneshot = BIT10(val);
			regs.cap[1].bits8 = BIT11(val);
			regs.cap[1].active = BIT15(val);
			ProbeCapture(1);
			break;
		}

		//SNDCAP0DAD
		case 0x510: regs.cap[0].dad &= 0xFFFF0000; regs.cap[0].dad |= (val & 0xFFFC); break;
		case 0x512: regs.cap[0].dad &= 0x0000FFFF; regs.cap[0].dad |= ((val & 0x07FF) << 16); break;

		//SNDCAP0LEN
		case 0x514: regs.cap[0].len = val; break;

		//SNDCAP1DAD
		case 0x518: regs.cap[1].dad &= 0xFFFF0000; regs.cap[1].dad |= (val & 0xFFFC); break;
		case 0x51A: regs.cap[1].dad &= 0x0000FFFF; regs.cap[1].dad |= ((val & 0x07FF) << 16); break;

		//SNDCAP1LEN
		case 0x51C: regs.cap[1].len = val; break;
	} //switch on address
}

void SPU_struct::WriteLong(u32 addr, u32 val)
{
	//individual channel regs
	if ((addr & 0x0F00) == 0x0400)
	{
		u32 chan_num = (addr >> 4) & 0xF;
		channel_struct &thischan=channels[chan_num];

		//printf("SPU write32 channel%d, reg %04X, val %08X\n", chan_num, addr, val);

		switch (addr & 0xF)
		{
			case 0x0: 
				thischan.vol = val & 0x7F;
				thischan.volumeDiv = (val >> 8) & 0x3;
				thischan.hold = (val >> 15) & 0x1;
				thischan.pan = (val >> 16) & 0x7F;
				thischan.waveduty = (val >> 24) & 0x7;
				thischan.repeat = (val >> 27) & 0x3;
				thischan.format = (val >> 29) & 0x3;
				thischan.keyon = (val >> 31) & 0x1;
				KeyProbe(chan_num);
			break;

			case 0x4: thischan.addr = (val & 0x07FFFFFC); break;
			case 0x8: 
				thischan.timer = (val & 0xFFFF);
				thischan.loopstart = ((val >> 16) & 0xFFFF);
				adjust_channel_timer(&thischan);
			break;

			case 0xC: thischan.length = (val & 0x003FFFFF); break; //only 22 bits of this register are used
		} //switch on individual channel regs
		return;
	}

	switch(addr)
	{
		//SOUNDCNT
		case 0x500:
			regs.mastervol = (val & 0x7F);
			regs.ctl_left  = ((val >> 8) & 3);
			regs.ctl_right = ((val>>10) & 3);
			regs.ctl_ch1bypass = ((val >> 12) & 1);
			regs.ctl_ch3bypass = ((val >> 13) & 1);
			regs.masteren = ((val >> 15) & 1);
			for(u8 i=0; i<16; i++)
				KeyProbe(i);
			break;

		//SOUNDBIAS
		case 0x504: regs.soundbias = (val & 0x3FF);

		//SNDCAP0CNT/SNDCAP1CNT
		case 0x508:
			regs.cap[0].add = BIT0(val);
			regs.cap[0].source = BIT1(val);
			regs.cap[0].oneshot = BIT2(val);
			regs.cap[0].bits8 = BIT3(val);
			regs.cap[0].active = BIT7(val);
			ProbeCapture(0);

			regs.cap[1].add = BIT8(val);
			regs.cap[1].source = BIT9(val);
			regs.cap[1].oneshot = BIT10(val);
			regs.cap[1].bits8 = BIT11(val);
			regs.cap[1].active = BIT15(val);
			ProbeCapture(1);
		break;

		//SNDCAP0DAD
		case 0x510: regs.cap[0].dad = (val & 0x07FFFFFC); break;

		//SNDCAP0LEN
		case 0x514: regs.cap[0].len = (val & 0xFFFF); break;

		//SNDCAP1DAD
		case 0x518: regs.cap[1].dad = (val & 0x07FFFFFC); break;

		//SNDCAP1LEN
		case 0x51C: regs.cap[1].len = (val & 0xFFFF); break;
	} //switch on address
}

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE s32 Interpolate(s32 a, s32 b, double ratio)
{
	double sampleA = (double)a;
	double sampleB = (double)b;
	ratio = ratio - sputrunc(ratio);
	
	switch (INTERPOLATE_MODE)
	{
		case SPUInterpolation_Cosine:
			// Cosine Interpolation Formula:
			// ratio2 = (1 - cos(ratio * M_PI)) / 2
			// sampleI = sampleA * (1 - ratio2) + sampleB * ratio2
			return s32floor((cos_lut[(unsigned int)(ratio * (double)COSINE_INTERPOLATION_RESOLUTION)] * (sampleB - sampleA)) + sampleA);
			break;
			
		case SPUInterpolation_Linear:
			// Linear Interpolation Formula:
			// sampleI = sampleA * (1 - ratio) + sampleB * ratio
			return s32floor((ratio * (sampleB - sampleA)) + sampleA);
			break;
			
		default:
			break;
	}
	
	return a;
}

//////////////////////////////////////////////////////////////////////////////

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE void Fetch8BitData(channel_struct *chan, s32 *data)
{
	if (chan->sampcnt < 0)
	{
		*data = 0;
		return;
	}

	u32 loc = sputrunc(chan->sampcnt);
	if(INTERPOLATE_MODE != SPUInterpolation_None)
	{
		s32 a = (s32)(read_s8(chan->addr + loc) << 8), b = a;
		if(loc < (chan->totlength << 2) - 1)
			b = (s32)(read_s8(chan->addr + loc + 1) << 8);
		else if(chan->repeat == 1)
			b = (s32)(read_s8(chan->addr + chan->loopstart*4) << 8);
		*data = Interpolate<INTERPOLATE_MODE>(a, b, chan->sampcnt);
	}
	else
		*data = (s32)read_s8(chan->addr + loc)<< 8;
}

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE void Fetch16BitData(const channel_struct * const chan, s32 *data)
{
	if (chan->sampcnt < 0)
	{
		*data = 0;
		return;
	}

	u32 loc = sputrunc(chan->sampcnt);
	if(INTERPOLATE_MODE != SPUInterpolation_None)
	{
		s32 a = (s32)read16(loc*2 + chan->addr), b = a;
		if(loc < (chan->totlength << 1) - 1)
			b = (s32)read16(chan->addr + loc*2 + 2);
		else if(chan->repeat == 1)
			b = (s32)read16(chan->addr + chan->loopstart*2);
		*data = Interpolate<INTERPOLATE_MODE>(a, b, chan->sampcnt);
	}
	else
		*data = read16(chan->addr + loc*2);
}

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE void FetchADPCMData(channel_struct * const chan, s32 * const data)
{
	if (chan->sampcnt < 8)
	{
		*data = 0;
		return;
	}

	// No sense decoding, just return the last sample
	if (chan->lastsampcnt != sputrunc(chan->sampcnt)){

		const u32 endExclusive = sputrunc(chan->sampcnt+1);
		for (u32 i = chan->lastsampcnt+1; i < endExclusive; i++)
		{
			const u32 shift = (i&1)<<2;
			const u32 data4bit = ((u32)read08(chan->addr + (i>>1))) >> shift;

			const s32 diff = precalcdifftbl[chan->index][data4bit & 0xF];
			chan->index = precalcindextbl[chan->index][data4bit & 0x7];

			chan->pcm16b_last = chan->pcm16b;
			chan->pcm16b = MinMax(chan->pcm16b+diff, -0x8000, 0x7FFF);

			if(i == (chan->loopstart<<3)) {
				if(chan->loop_index != K_ADPCM_LOOPING_RECOVERY_INDEX) printf("over-snagging\n");
				chan->loop_pcm16b = chan->pcm16b;
				chan->loop_index = chan->index;
			}
		}

		chan->lastsampcnt = sputrunc(chan->sampcnt);
	}

	if(INTERPOLATE_MODE != SPUInterpolation_None)
		*data = Interpolate<INTERPOLATE_MODE>((s32)chan->pcm16b_last,(s32)chan->pcm16b,chan->sampcnt);
	else
		*data = (s32)chan->pcm16b;
}

static FORCEINLINE void FetchPSGData(channel_struct *chan, s32 *data)
{
	if (chan->sampcnt < 0)
	{
		*data = 0;
		return;
	}

	if(chan->num < 8)
	{
		*data = 0;
	}
	else if(chan->num < 14)
	{
		*data = (s32)wavedutytbl[chan->waveduty][(sputrunc(chan->sampcnt)) & 0x7];
	}
	else
	{
		if(chan->lastsampcnt == sputrunc(chan->sampcnt))
		{
			*data = (s32)chan->psgnoise_last;
			return;
		}

		u32 max = sputrunc(chan->sampcnt);
		for(u32 i = chan->lastsampcnt; i < max; i++)
		{
			if(chan->x & 0x1)
			{
				chan->x = (chan->x >> 1) ^ 0x6000;
				chan->psgnoise_last = -0x7FFF;
			}
			else
			{
				chan->x >>= 1;
				chan->psgnoise_last = 0x7FFF;
			}
		}

		chan->lastsampcnt = sputrunc(chan->sampcnt);

		*data = (s32)chan->psgnoise_last;
	}
}

//////////////////////////////////////////////////////////////////////////////

static FORCEINLINE void MixL(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	data = spumuldiv7(data, chan->vol) >> volume_shift[chan->volumeDiv];
	SPU->sndbuf[SPU->bufpos<<1] += data;
}

static FORCEINLINE void MixR(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	data = spumuldiv7(data, chan->vol) >> volume_shift[chan->volumeDiv];
	SPU->sndbuf[(SPU->bufpos<<1)+1] += data;
}

static FORCEINLINE void MixLR(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	data = spumuldiv7(data, chan->vol) >> volume_shift[chan->volumeDiv];
	SPU->sndbuf[SPU->bufpos<<1] += spumuldiv7(data, 127 - chan->pan);
	SPU->sndbuf[(SPU->bufpos<<1)+1] += spumuldiv7(data, chan->pan);
}

//////////////////////////////////////////////////////////////////////////////

template<int FORMAT> static FORCEINLINE void TestForLoop(SPU_struct *SPU, channel_struct *chan)
{
	const int shift = (FORMAT == 0 ? 2 : 1);

	chan->sampcnt += chan->sampinc;

	if (chan->sampcnt > chan->double_totlength_shifted)
	{
		// Do we loop? Or are we done?
		if (chan->repeat == 1)
		{
			while (chan->sampcnt > chan->double_totlength_shifted)
				chan->sampcnt -= chan->double_totlength_shifted - (double)(chan->loopstart << shift);
			//chan->sampcnt = (double)(chan->loopstart << shift);
		}
		else
		{
			SPU->KeyOff(chan->num);
			SPU->bufpos = SPU->buflength;
		}
	}
}

static FORCEINLINE void TestForLoop2(SPU_struct *SPU, channel_struct *chan)
{
	// Minimum length (the sum of PNT+LEN) is 4 words (16 bytes), 
	// smaller values (0..3 words) are causing hang-ups 
	// (busy bit remains set infinite, but no sound output occurs).
	// fix: 7th Dragon (JP) - http://sourceforge.net/p/desmume/bugs/1357/
	if (chan->totlength < 4) return;

	chan->sampcnt += chan->sampinc;

	if (chan->sampcnt > chan->double_totlength_shifted)
	{
		// Do we loop? Or are we done?
		if (chan->repeat == 1)
		{
			double step = (chan->double_totlength_shifted - (double)(chan->loopstart << 3));
			
			while (chan->sampcnt > chan->double_totlength_shifted) chan->sampcnt -= step;

			if(chan->loop_index == K_ADPCM_LOOPING_RECOVERY_INDEX)
			{
				chan->pcm16b = (s16)read16(chan->addr);
				chan->index = read08(chan->addr+2) & 0x7F;
				chan->lastsampcnt = 7;
			}
			else
			{
				chan->pcm16b = chan->loop_pcm16b;
				chan->index = chan->loop_index;
				chan->lastsampcnt = (chan->loopstart << 3);
			}
		}
		else
		{
			chan->status = CHANSTAT_STOPPED;
			SPU->KeyOff(chan->num);
			SPU->bufpos = SPU->buflength;
		}
	}
}

template<int CHANNELS> FORCEINLINE static void SPU_Mix(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	switch(CHANNELS)
	{
		case 0: MixL(SPU, chan, data); break;
		case 1: MixLR(SPU, chan, data); break;
		case 2: MixR(SPU, chan, data); break;
		default: break;
	}
	SPU->lastdata = data;
}

//WORK
template<int FORMAT, SPUInterpolationMode INTERPOLATE_MODE, int CHANNELS> 
	FORCEINLINE static void ____SPU_ChanUpdate(SPU_struct* const SPU, channel_struct* const chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		if(CHANNELS != -1)
		{
			s32 data;
			switch(FORMAT)
			{
				case 0: Fetch8BitData<INTERPOLATE_MODE>(chan, &data); break;
				case 1: Fetch16BitData<INTERPOLATE_MODE>(chan, &data); break;
				case 2: FetchADPCMData<INTERPOLATE_MODE>(chan, &data); break;
				case 3: FetchPSGData(chan, &data); break;
				default: break;
			}
			SPU_Mix<CHANNELS>(SPU, chan, data);
		}

		switch(FORMAT) {
			case 0: case 1: TestForLoop<FORMAT>(SPU, chan); break;
			case 2: TestForLoop2(SPU, chan); break;
			case 3: chan->sampcnt += chan->sampinc; break;
			default: break;
		}
	}
}

template<int FORMAT, SPUInterpolationMode INTERPOLATE_MODE> 
	FORCEINLINE static void ___SPU_ChanUpdate(const bool actuallyMix, SPU_struct* const SPU, channel_struct* const chan)
{
	if(!actuallyMix)
		____SPU_ChanUpdate<FORMAT,INTERPOLATE_MODE,-1>(SPU,chan);
	else if (chan->pan == 0)
		____SPU_ChanUpdate<FORMAT,INTERPOLATE_MODE,0>(SPU,chan);
	else if (chan->pan == 127)
		____SPU_ChanUpdate<FORMAT,INTERPOLATE_MODE,2>(SPU,chan);
	else
		____SPU_ChanUpdate<FORMAT,INTERPOLATE_MODE,1>(SPU,chan);
}

template<SPUInterpolationMode INTERPOLATE_MODE> 
	FORCEINLINE static void __SPU_ChanUpdate(const bool actuallyMix, SPU_struct* const SPU, channel_struct* const chan)
{
	switch(chan->format)
	{
		case 0: ___SPU_ChanUpdate<0,INTERPOLATE_MODE>(actuallyMix, SPU, chan); break;
		case 1: ___SPU_ChanUpdate<1,INTERPOLATE_MODE>(actuallyMix, SPU, chan); break;
		case 2: ___SPU_ChanUpdate<2,INTERPOLATE_MODE>(actuallyMix, SPU, chan); break;
		case 3: ___SPU_ChanUpdate<3,INTERPOLATE_MODE>(actuallyMix, SPU, chan); break;
		default: assert(false);
	}
}

FORCEINLINE static void _SPU_ChanUpdate(const bool actuallyMix, SPU_struct* const SPU, channel_struct* const chan)
{
	switch(CommonSettings.spuInterpolationMode)
	{
	case SPUInterpolation_None: __SPU_ChanUpdate<SPUInterpolation_None>(actuallyMix, SPU, chan); break;
	case SPUInterpolation_Linear: __SPU_ChanUpdate<SPUInterpolation_Linear>(actuallyMix, SPU, chan); break;
	case SPUInterpolation_Cosine: __SPU_ChanUpdate<SPUInterpolation_Cosine>(actuallyMix, SPU, chan); break;
	default: assert(false);
	}
}

//ENTERNEW
static void SPU_MixAudio_Advanced(bool actuallyMix, SPU_struct *SPU, int length)
{
	//the advanced spu function correctly handles all sound control mixing options, as well as capture
	//this code is not entirely optimal, as it relies on sort of manhandling the core mixing functions
	//in order to get the results it needs.

	//THIS IS MAX HACKS!!!!
	//AND NEEDS TO BE REWRITTEN ALONG WITH THE DEEPEST PARTS OF THE SPU
	//ONCE WE KNOW THAT IT WORKS
	
	//BIAS gets ignored since our spu is still not bit perfect,
	//and it doesnt matter for purposes of capture

	//-----------DEBUG CODE
	bool skipcap = false;
	//-----------------

	s32 samp0[2] = {0,0};
	
	//believe it or not, we are going to do this one sample at a time.
	//like i said, it is slower.
	for (int samp = 0; samp < length; samp++)
	{
		SPU->sndbuf[0] = 0;
		SPU->sndbuf[1] = 0;
		SPU->buflength = 1;

		s32 capmix[2] = {0,0};
		s32 mix[2] = {0,0};
		s32 chanout[16];
		s32 submix[32];

		//generate each channel, and helpfully mix it at the same time
		for (int i = 0; i < 16; i++)
		{
			channel_struct *chan = &SPU->channels[i];

			if (chan->status == CHANSTAT_PLAY)
			{
				SPU->bufpos = 0;

				bool bypass = false;
				if (i==1 && SPU->regs.ctl_ch1bypass) bypass=true;
				if (i==3 && SPU->regs.ctl_ch3bypass) bypass=true;


				//output to mixer unless we are bypassed.
				//dont output to mixer if the user muted us
				bool outputToMix = true;
				if (CommonSettings.spu_muteChannels[i]) outputToMix = false;
				if (bypass) outputToMix = false;
				bool outputToCap = outputToMix;
				if (CommonSettings.spu_captureMuted && !bypass) outputToCap = true;

				//channels 1 and 3 should probably always generate their audio
				//internally at least, just in case they get used by the spu output
				bool domix = outputToCap || outputToMix || i==1 || i==3;

				//clear the output buffer since this is where _SPU_ChanUpdate wants to accumulate things
				SPU->sndbuf[0] = SPU->sndbuf[1] = 0;

				//get channel's next output sample.
				_SPU_ChanUpdate(domix, SPU, chan);
				chanout[i] = SPU->lastdata >> volume_shift[chan->volumeDiv];

				//save the panned results
				submix[i*2] = SPU->sndbuf[0];
				submix[i*2+1] = SPU->sndbuf[1];

				//send sample to our capture mix
				if (outputToCap)
				{
					capmix[0] += submix[i*2];
					capmix[1] += submix[i*2+1];
				}

				//send sample to our main mixer
				if (outputToMix)
				{
					mix[0] += submix[i*2];
					mix[1] += submix[i*2+1];
				}
			}
			else 
			{
				chanout[i] = 0;
				submix[i*2] = 0;
				submix[i*2+1] = 0;
			}
		} //foreach channel

		s32 mixout[2] = {mix[0],mix[1]};
		s32 capmixout[2] = {capmix[0],capmix[1]};
		s32 sndout[2];
		s32 capout[2];

		//create SPU output
		switch (SPU->regs.ctl_left)
		{
			case SPU_struct::REGS::LOM_LEFT_MIXER: sndout[0] = mixout[0]; break;
			case SPU_struct::REGS::LOM_CH1: sndout[0] = submix[1*2+0]; break;
			case SPU_struct::REGS::LOM_CH3: sndout[0] = submix[3*2+0]; break;
			case SPU_struct::REGS::LOM_CH1_PLUS_CH3: sndout[0] = submix[1*2+0] + submix[3*2+0]; break;
			default: break;
		}
		switch (SPU->regs.ctl_right)
		{
			case SPU_struct::REGS::ROM_RIGHT_MIXER: sndout[1] = mixout[1]; break;
			case SPU_struct::REGS::ROM_CH1: sndout[1] = submix[1*2+1]; break;
			case SPU_struct::REGS::ROM_CH3: sndout[1] = submix[3*2+1]; break;
			case SPU_struct::REGS::ROM_CH1_PLUS_CH3: sndout[1] = submix[1*2+1] + submix[3*2+1]; break;
			default: break;
		}


		//generate capture output ("capture bugs" from gbatek are not emulated)
		if (SPU->regs.cap[0].source == 0)
			capout[0] = capmixout[0]; //cap0 = L-mix
		else if (SPU->regs.cap[0].add)
			capout[0] = chanout[0] + chanout[1]; //cap0 = ch0+ch1
		else capout[0] = chanout[0]; //cap0 = ch0

		if (SPU->regs.cap[1].source == 0)
			capout[1] = capmixout[1]; //cap1 = R-mix
		else if (SPU->regs.cap[1].add)
			capout[1] = chanout[2] + chanout[3]; //cap1 = ch2+ch3
		else capout[1] = chanout[2]; //cap1 = ch2

		capout[0] = MinMax(capout[0],-0x8000,0x7FFF);
		capout[1] = MinMax(capout[1],-0x8000,0x7FFF);

		//write the output sample where it is supposed to go
		if (samp == 0)
		{
			samp0[0] = sndout[0];
			samp0[1] = sndout[1];
		}
		else
		{
			SPU->sndbuf[samp*2+0] = sndout[0];
			SPU->sndbuf[samp*2+1] = sndout[1];
		}

		for (int capchan = 0; capchan < 2; capchan++)
		{
			if (SPU->regs.cap[capchan].runtime.running)
			{
				SPU_struct::REGS::CAP& cap = SPU->regs.cap[capchan];
				u32 last = sputrunc(cap.runtime.sampcnt);
				cap.runtime.sampcnt += SPU->channels[1+2*capchan].sampinc;
				u32 curr = sputrunc(cap.runtime.sampcnt);
				for (u32 j = last; j < curr; j++)
				{
					//so, this is a little strange. why go through a fifo?
					//it seems that some games will set up a reverb effect by capturing
					//to the nearly same address as playback, but ahead by a couple.
					//So, playback will always end up being what was captured a couple of samples ago.
					//This system counts on playback always having read ahead 16 samples.
					//In that case, playback will end up being what was processed at one entire buffer length ago,
					//since the 16 samples would have read ahead before they got captured over

					//It's actually the source channels which should have a fifo, but we are
					//not going to take the hit in speed and complexity. Save it for a future rewrite.
					//Instead, what we do here is delay the capture by 16 samples to create a similar effect.
					//Subjectively, it seems to be working.

					//Don't do anything until the fifo is filled, so as to delay it
					if (cap.runtime.fifo.size < 16)
					{
						cap.runtime.fifo.enqueue(capout[capchan]);
						continue;
					}

					//(actually capture sample from fifo instead of most recently generated)
					u32 multiplier;
					s32 sample = cap.runtime.fifo.dequeue();
					cap.runtime.fifo.enqueue(capout[capchan]);

					//static FILE* fp = NULL;
					//if(!fp) fp = fopen("d:\\capout.raw","wb");
					//fwrite(&sample,2,1,fp);
					
					if (cap.bits8)
					{
						s8 sample8 = sample >> 8;
						if (skipcap) _MMU_write08<1,MMU_AT_DMA>(cap.runtime.curdad,0);
						else _MMU_write08<1,MMU_AT_DMA>(cap.runtime.curdad,sample8);
						cap.runtime.curdad++;
						multiplier = 4;
					}
					else
					{
						s16 sample16 = sample;
						if (skipcap) _MMU_write16<1,MMU_AT_DMA>(cap.runtime.curdad,0);
						else _MMU_write16<1,MMU_AT_DMA>(cap.runtime.curdad,sample16);
						cap.runtime.curdad+=2;
						multiplier = 2;
					}

					if (cap.runtime.curdad >= cap.runtime.maxdad)
					{
						cap.runtime.curdad = cap.dad;
						cap.runtime.sampcnt -= cap.len*multiplier;
					}
				} //sampinc loop
			} //if capchan running
		} //capchan loop
	} //main sample loop

	SPU->sndbuf[0] = samp0[0];
	SPU->sndbuf[1] = samp0[1];
}

//ENTER
static void SPU_MixAudio(bool actuallyMix, SPU_struct *SPU, int length)
{
	if (actuallyMix)
	{
		memset(SPU->sndbuf, 0, length*4*2);
		memset(SPU->outbuf, 0, length*2*2);
	}

	//we used to use master enable here, and do nothing if audio is disabled.
	//now, master enable is emulated better..
	//but for a speed optimization we will still do it
	//is this still a good idea? zeroing the capture buffers is important...
	if(!SPU->regs.masteren) return;

	bool advanced = CommonSettings.spu_advanced;

	//branch here so that slow computers don't have to take the advanced (slower) codepath.
	//it remainds to be seen exactly how much slower it is
	//if it isnt much slower then we should refactor everything to be simpler, once it is working
	if (advanced && SPU == SPU_core)
	{
		SPU_MixAudio_Advanced(actuallyMix, SPU, length);
	}
	else
	{
		//non-advanced mode
		for (int i = 0; i < 16; i++)
		{
			channel_struct *chan = &SPU->channels[i];

			if (chan->status != CHANSTAT_PLAY)
				continue;

			SPU->bufpos = 0;
			SPU->buflength = length;

			// Mix audio
			_SPU_ChanUpdate(!CommonSettings.spu_muteChannels[i] && actuallyMix, SPU, chan);
		}

		//zero out capture buffers - effectively transform no-advanced-spu-emulation to capturing-zeroes
		//this is needed so when the option is changed (or a state with a different setting is loaded)
		//this code is bulkier and slower than it might otherwise be to reduce the chance of bugs 
		//IDEALLY the non-advanced codepath would be removed (while the advanced codepath was optimized and improved)
		//and this code would disappear, to be replaced with code more capable of emitting zeroes at the opportune time.
		for (int capchan = 0; capchan < 2; capchan++)
		{
			SPU_struct::REGS::CAP& cap = SPU->regs.cap[capchan];
			if (cap.runtime.running)
			{
				for (int samp = 0; samp < length; samp++)
				{
					u32 last = sputrunc(cap.runtime.sampcnt);
					cap.runtime.sampcnt += SPU->channels[1+2*capchan].sampinc;
					u32 curr = sputrunc(cap.runtime.sampcnt);
					for (u32 j = last; j < curr; j++)
					{
						if (cap.bits8)
						{
							_MMU_write08<1,MMU_AT_DMA>(cap.runtime.curdad,0);
							cap.runtime.curdad++;
						}
						else
						{
							_MMU_write16<1,MMU_AT_DMA>(cap.runtime.curdad,0);
							cap.runtime.curdad+=2;
						}

						if (cap.runtime.curdad >= cap.runtime.maxdad)
						{
							cap.runtime.curdad = cap.dad;
							cap.runtime.sampcnt -= cap.len*(cap.bits8?4:2);
						}
					}
				}
			}
		}
	} //non-advanced branch

	//we used to bail out if speakers were disabled.
	//this is technically wrong. sound may still be captured, or something.
	//in all likelihood, any game doing this probably master disabled the SPU also
	//so, optimization of this case is probably not necessary.
	//later, we'll just silence the output
	bool speakers = T1ReadWord(MMU.ARM7_REG, 0x304) & 0x01;

	u8 vol = SPU->regs.mastervol;

	// convert from 32-bit->16-bit
	if (actuallyMix && speakers)
		for (int i = 0; i < length*2; i++)
		{
			// Apply Master Volume
			SPU->sndbuf[i] = spumuldiv7(SPU->sndbuf[i], vol);
			s16 outsample = MinMax(SPU->sndbuf[i],-0x8000,0x7FFF);
			SPU->outbuf[i] = outsample;
		}


}

//////////////////////////////////////////////////////////////////////////////


//emulates one hline of the cpu core.
//this will produce a variable number of samples, calculated to keep a 44100hz output
//in sync with the emulator framerate
void SPU_Emulate_core()
{
	bool needToMix = true;
	SoundInterface_struct *soundProcessor = SPU_SoundCore();
	
	_samples += samples_per_hline;
	spu_core_samples = (int)(_samples);
	_samples -= spu_core_samples;
	
	// We don't need to mix audio for Dual Synch/Asynch mode since we do this
	// later in SPU_Emulate_user(). Disable mixing here to speed up processing.
	// However, recording still needs to mix the audio, so make sure we're also
	// not recording before we disable mixing.
	if ( _currentSynchMode == ESynchMode_DualSynchAsynch &&
		!(driver->AVI_IsRecording() || driver->WAV_IsRecording()) )
	{
		needToMix = false;
	}
	
	SPU_MixAudio(needToMix, SPU_core, spu_core_samples);
	
	if (soundProcessor == NULL)
	{
		return;
	}
	
	if (soundProcessor->FetchSamples != NULL)
	{
		soundProcessor->FetchSamples(SPU_core->outbuf, spu_core_samples, _currentSynchMode, _currentSynchronizer);
	}
	else
	{
		SPU_DefaultFetchSamples(SPU_core->outbuf, spu_core_samples, _currentSynchMode, _currentSynchronizer);
	}
}

void SPU_Emulate_user(bool mix)
{
	static s16 *postProcessBuffer = NULL;
	static size_t postProcessBufferSize = 0;
	size_t freeSampleCount = 0;
	size_t processedSampleCount = 0;
	SoundInterface_struct *soundProcessor = SPU_SoundCore();
	
	if (soundProcessor == NULL)
	{
		return;
	}
	
	// Check to see how many free samples are available.
	// If there are some, fill up the output buffer.
	freeSampleCount = soundProcessor->GetAudioSpace();
	if (freeSampleCount == 0)
	{
		return;
	}
	
	//printf("mix %i samples\n", audiosize);
	if (freeSampleCount > _currentBufferSize)
	{
		freeSampleCount = _currentBufferSize;
	}
	
	// If needed, resize the post-process buffer to guarantee that
	// we can store all the sound data.
	if (postProcessBufferSize < freeSampleCount * 2 * sizeof(s16))
	{
		postProcessBufferSize = freeSampleCount * 2 * sizeof(s16);
		postProcessBuffer = (s16 *)realloc(postProcessBuffer, postProcessBufferSize);
	}
	
	if (soundProcessor->PostProcessSamples != NULL)
	{
		processedSampleCount = soundProcessor->PostProcessSamples(postProcessBuffer, freeSampleCount, _currentSynchMode, _currentSynchronizer);
	}
	else
	{
		processedSampleCount = SPU_DefaultPostProcessSamples(postProcessBuffer, freeSampleCount, _currentSynchMode, _currentSynchronizer);
	}
	
	soundProcessor->UpdateAudio(postProcessBuffer, processedSampleCount);
	WAV_WavSoundUpdate(postProcessBuffer, processedSampleCount, WAVMODE_USER);
}

void SPU_DefaultFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	if (synchMode == ESynchMode_Synchronous)
	{
		theSynchronizer->enqueue_samples(sampleBuffer, sampleCount);
	}
}

size_t SPU_DefaultPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer)
{
	size_t processedSampleCount = 0;
	
	switch (synchMode)
	{
		case ESynchMode_DualSynchAsynch:
			if(SPU_user != NULL)
			{
				SPU_MixAudio(true, SPU_user, requestedSampleCount);
				memcpy(postProcessBuffer, SPU_user->outbuf, requestedSampleCount * 2 * sizeof(s16));
				processedSampleCount = requestedSampleCount;
			}
			break;
			
		case ESynchMode_Synchronous:
			processedSampleCount = theSynchronizer->output_samples(postProcessBuffer, requestedSampleCount);
			break;
			
		default:
			break;
	}
	
	return processedSampleCount;
}

//////////////////////////////////////////////////////////////////////////////
// Dummy Sound Interface
//////////////////////////////////////////////////////////////////////////////

int SNDDummyInit(int buffersize);
void SNDDummyDeInit();
void SNDDummyUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDDummyGetAudioSpace();
void SNDDummyMuteAudio();
void SNDDummyUnMuteAudio();
void SNDDummySetVolume(int volume);
void SNDDummyClearBuffer();
void SNDDummyFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer);
size_t SNDDummyPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer);

SoundInterface_struct SNDDummy = {
	SNDCORE_DUMMY,
	"Dummy Sound Interface",
	SNDDummyInit,
	SNDDummyDeInit,
	SNDDummyUpdateAudio,
	SNDDummyGetAudioSpace,
	SNDDummyMuteAudio,
	SNDDummyUnMuteAudio,
	SNDDummySetVolume,
	SNDDummyClearBuffer,
	SNDDummyFetchSamples,
	SNDDummyPostProcessSamples
};

int SNDDummyInit(int buffersize) { return 0; }
void SNDDummyDeInit() {}
void SNDDummyUpdateAudio(s16 *buffer, u32 num_samples) { }
u32 SNDDummyGetAudioSpace() { return DESMUME_SAMPLE_RATE/60 + 5; }
void SNDDummyMuteAudio() {}
void SNDDummyUnMuteAudio() {}
void SNDDummySetVolume(int volume) {}
void SNDDummyClearBuffer() {}
void SNDDummyFetchSamples(s16 *sampleBuffer, size_t sampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer) {}
size_t SNDDummyPostProcessSamples(s16 *postProcessBuffer, size_t requestedSampleCount, ESynchMode synchMode, ISynchronizingAudioBuffer *theSynchronizer) { return 0; }

//---------wav writer------------

typedef struct {
	char id[4];
	u32 size;
} chunk_struct;

typedef struct {
	chunk_struct riff;
	char rifftype[4];
} waveheader_struct;

typedef struct {
	chunk_struct chunk;
	u16 compress;
	u16 numchan;
	u32 rate;
	u32 bytespersec;
	u16 blockalign;
	u16 bitspersample;
} fmt_struct;

WavWriter::WavWriter() 
: spufp(NULL)
{
}
bool WavWriter::open(const std::string & fname)
{
	waveheader_struct waveheader;
	fmt_struct fmt;
	chunk_struct data;
	size_t elems_written = 0;

	if ((spufp = fopen(fname.c_str(), "wb")) == NULL)
		return false;

	// Do wave header
	memcpy(waveheader.riff.id, "RIFF", 4);
	waveheader.riff.size = 0; // we'll fix this after the file is closed
	memcpy(waveheader.rifftype, "WAVE", 4);
	elems_written += fwrite((void *)&waveheader, 1, sizeof(waveheader_struct), spufp);

	// fmt chunk
	memcpy(fmt.chunk.id, "fmt ", 4);
	fmt.chunk.size = 16; // we'll fix this at the end
	fmt.compress = 1; // PCM
	fmt.numchan = 2; // Stereo
	fmt.rate = DESMUME_SAMPLE_RATE;
	fmt.bitspersample = 16;
	fmt.blockalign = fmt.bitspersample / 8 * fmt.numchan;
	fmt.bytespersec = fmt.rate * fmt.blockalign;
	elems_written += fwrite((void *)&fmt, 1, sizeof(fmt_struct), spufp);

	// data chunk
	memcpy(data.id, "data", 4);
	data.size = 0; // we'll fix this at the end
	elems_written += fwrite((void *)&data, 1, sizeof(chunk_struct), spufp);

	return true;
}

void WavWriter::close()
{
	if(!spufp) return;
	size_t elems_written = 0;
	long length = ftell(spufp);

	// Let's fix the riff chunk size and the data chunk size
	fseek(spufp, sizeof(waveheader_struct)-0x8, SEEK_SET);
	length -= 0x8;
	elems_written += fwrite((void *)&length, 1, 4, spufp);

	fseek(spufp, sizeof(waveheader_struct)+sizeof(fmt_struct)+0x4, SEEK_SET);
	length -= sizeof(waveheader_struct)+sizeof(fmt_struct);
	elems_written += fwrite((void *)&length, 1, 4, spufp);
	fclose(spufp);
	spufp = NULL;
}

void WavWriter::update(void* soundData, int numSamples)
{
	if(!spufp) return;
	//TODO - big endian for the s16 samples??
	size_t elems_written = fwrite(soundData, numSamples*2, 2, spufp);
}

bool WavWriter::isRecording() const
{
	return spufp != NULL;
}


static WavWriter wavWriter;

void WAV_End()
{
	wavWriter.close();
}

bool WAV_Begin(const char* fname, WAVMode mode)
{
	WAV_End();

	if(!wavWriter.open(fname))
		return false;

	if(mode == WAVMODE_ANY)
		mode = WAVMODE_CORE;
	wavWriter.mode = mode;

	driver->USR_InfoMessage("WAV recording started.");

	return true;
}

bool WAV_IsRecording(WAVMode mode)
{
	if(wavWriter.mode == mode || mode == WAVMODE_ANY)
		return wavWriter.isRecording();
	return false;
}

void WAV_WavSoundUpdate(void* soundData, int numSamples, WAVMode mode)
{
	if(wavWriter.mode == mode || mode == WAVMODE_ANY)
		wavWriter.update(soundData, numSamples);
}



//////////////////////////////////////////////////////////////////////////////

void spu_savestate(EMUFILE &os)
{
	//version
	os.write_32LE(6);

	SPU_struct *spu = SPU_core;

	for (int j = 0; j < 16; j++)
	{
		channel_struct &chan = spu->channels[j];
		os.write_32LE(chan.num);
		os.write_u8(chan.vol);
		os.write_u8(chan.volumeDiv);
		os.write_u8(chan.hold);
		os.write_u8(chan.pan);
		os.write_u8(chan.waveduty);
		os.write_u8(chan.repeat);
		os.write_u8(chan.format);
		os.write_u8(chan.status);
		os.write_32LE(chan.addr);
		os.write_16LE(chan.timer);
		os.write_16LE(chan.loopstart);
		os.write_32LE(chan.length);
		os.write_doubleLE(chan.sampcnt);
		os.write_doubleLE(chan.sampinc);
		os.write_32LE(chan.lastsampcnt);
		os.write_16LE(chan.pcm16b);
		os.write_16LE(chan.pcm16b_last);
		os.write_32LE(chan.index);
		os.write_16LE(chan.x);
		os.write_16LE(chan.psgnoise_last);
		os.write_u8(chan.keyon);
	}

	os.write_doubleLE(_samples);

	os.write_u8(spu->regs.mastervol);
	os.write_u8(spu->regs.ctl_left);
	os.write_u8(spu->regs.ctl_right);
	os.write_u8(spu->regs.ctl_ch1bypass);
	os.write_u8(spu->regs.ctl_ch3bypass);
	os.write_u8(spu->regs.masteren);
	os.write_16LE(spu->regs.soundbias);

	for (int i = 0; i < 2; i++)
	{
		os.write_u8(spu->regs.cap[i].add);
		os.write_u8(spu->regs.cap[i].source);
		os.write_u8(spu->regs.cap[i].oneshot);
		os.write_u8(spu->regs.cap[i].bits8);
		os.write_u8(spu->regs.cap[i].active);
		os.write_32LE(spu->regs.cap[i].dad);
		os.write_16LE(spu->regs.cap[i].len);
		os.write_u8(spu->regs.cap[i].runtime.running);
		os.write_32LE(spu->regs.cap[i].runtime.curdad);
		os.write_32LE(spu->regs.cap[i].runtime.maxdad);
		os.write_doubleLE(spu->regs.cap[i].runtime.sampcnt);
	}

	for (int i = 0; i < 2; i++)
		spu->regs.cap[i].runtime.fifo.save(os);
}

bool spu_loadstate(EMUFILE &is, int size)
{
	//note! if we load a state created with advanced spu logic on a system without it,
	//there's a high likelihood of captured data existing.
	//this would get played back forever without being replaced by captured data.
	//it's been solved by capturing zeroes though even when advanced spu logic is disabled.
	
	//read version
	u32 version;
	if (is.read_32LE(version) != 1) return false;

	SPU_struct *spu = SPU_core;
	reconstruct(&SPU_core->regs);

	for (int j = 0; j < 16; j++)
	{
		channel_struct &chan = spu->channels[j];
		is.read_32LE(chan.num);
		is.read_u8(chan.vol);
		is.read_u8(chan.volumeDiv);
		if (chan.volumeDiv == 4) chan.volumeDiv = 3;
		is.read_u8(chan.hold);
		is.read_u8(chan.pan);
		is.read_u8(chan.waveduty);
		is.read_u8(chan.repeat);
		is.read_u8(chan.format);
		is.read_u8(chan.status);
		is.read_32LE(chan.addr);
		is.read_16LE(chan.timer);
		is.read_16LE(chan.loopstart);
		is.read_32LE(chan.length);
		chan.totlength = chan.length + chan.loopstart;
		chan.double_totlength_shifted = (double)(chan.totlength << format_shift[chan.format]);
		//printf("%f\n",chan.double_totlength_shifted);
		if (version >= 2)
		{
			is.read_doubleLE(chan.sampcnt);
			is.read_doubleLE(chan.sampinc);
		}
		else
		{
			is.read_32LE(*(u32 *)&chan.sampcnt);
			is.read_32LE(*(u32 *)&chan.sampinc);
		}
		is.read_32LE(chan.lastsampcnt);
		is.read_16LE(chan.pcm16b);
		is.read_16LE(chan.pcm16b_last);
		is.read_32LE(chan.index);
		is.read_16LE(chan.x);
		is.read_16LE(chan.psgnoise_last);

		if (version >= 4)
			is.read_u8(chan.keyon);

		//hopefully trigger a recovery of the adpcm looping system
		chan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;
	}

	if (version >= 2)
	{
		is.read_doubleLE(_samples);
	}

	if (version >= 4)
	{
		is.read_u8(spu->regs.mastervol);
		is.read_u8(spu->regs.ctl_left);
		is.read_u8(spu->regs.ctl_right);
		is.read_u8(spu->regs.ctl_ch1bypass);
		is.read_u8(spu->regs.ctl_ch3bypass);
		is.read_u8(spu->regs.masteren);
		is.read_16LE(spu->regs.soundbias);
	}

	if (version >= 5)
	{
		for (int i = 0; i < 2; i++)
		{
			is.read_u8(spu->regs.cap[i].add);
			is.read_u8(spu->regs.cap[i].source);
			is.read_u8(spu->regs.cap[i].oneshot);
			is.read_u8(spu->regs.cap[i].bits8);
			is.read_u8(spu->regs.cap[i].active);
			is.read_32LE(spu->regs.cap[i].dad);
			is.read_16LE(spu->regs.cap[i].len);
			is.read_u8(spu->regs.cap[i].runtime.running);
			is.read_32LE(spu->regs.cap[i].runtime.curdad);
			is.read_32LE(spu->regs.cap[i].runtime.maxdad);
			is.read_doubleLE(spu->regs.cap[i].runtime.sampcnt);
		}
	}

	if (version >= 6)
		for (int i=0;i<2;i++) spu->regs.cap[i].runtime.fifo.load(is);
	else
		for (int i=0;i<2;i++) spu->regs.cap[i].runtime.fifo.reset();

	//older versions didnt store a mastervol; 
	//we must reload this or else games will start silent
	if (version < 4)
	{
		spu->regs.mastervol = T1ReadByte(MMU.ARM7_REG, 0x500) & 0x7F;
		spu->regs.masteren = BIT15(T1ReadWord(MMU.ARM7_REG, 0x500));
	}

	//copy the core spu (the more accurate) to the user spu
	SPU_CloneUser();

	return true;
}
