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

// Disabling capture when _currentSNDCore == SNDDummy can increase
// performance by disabling all mixing, but could potentially cause
// problems if the software is relying on the capture output. By
// default, we disable all mixing only when capture isn't running,
// as this is guaranteed to be safe.
#define ENABLE_DUMMY_SPU_CAPTURE 1

// This specifies how many samples to buffer for the channel FIFO
// If this is too low, then badly-synchronized streams can cause
// buffer overrun.
#define SPUCHAN_FIFO_DELAY 3
#if SPUCHAN_FIFO_DELAY >= SPUCHAN_PCM16B_SIZE
# error "Channel FIFO delay must less than SPUCHAN_PCM16B_SIZE"
#endif

// This controls the delay for the capture unit (how many output
// samples to stall for before actually writing anything).
// This seems to need matching to the channel playback delay (see KeyOn())?
#define SPUCAPTURE_FIFO_DELAY 3

#define K_ADPCM_LOOPING_RECOVERY_INDEX 255

#define CATMULLROM_INTERPOLATION_RESOLUTION_BITS 11
#define CATMULLROM_INTERPOLATION_RESOLUTION (1<<CATMULLROM_INTERPOLATION_RESOLUTION_BITS)

#define COSINE_INTERPOLATION_RESOLUTION_BITS 13
#define COSINE_INTERPOLATION_RESOLUTION (1<<COSINE_INTERPOLATION_RESOLUTION_BITS)

#define SPUCHAN_PCM16B_AT(x)    ((x) & (SPUCHAN_PCM16B_SIZE -1))
#define SPUCAPTURE_PCM16B_AT(x) ((x) & (SPUCAPTURE_FIFO_SIZE-1))

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

static s32 precalcdifftbl[89][16];
static u8 precalcindextbl[89][8];
static u16 catmullrom_lut[CATMULLROM_INTERPOLATION_RESOLUTION][4];
static u16 cos_lut[COSINE_INTERPOLATION_RESOLUTION];

static const u32 ARM7_CLOCK = 33513982;

static u32 _spu_core_cyclesCounter = 0;

template<typename T>
static FORCEINLINE T MinMax(T val, T min, T max)
{
	if (val < min)
		return min;
	else if (val > max)
		return max;

	return val;
}

// T must be unsigned type
template<typename T>
static FORCEINLINE T AddAndReturnCarry(T *a, T b) {
	T c = (*a >= (-b));
	*a += b;
	return c;
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
	// Build the interpolation LUTs
	for (size_t i = 0; i < CATMULLROM_INTERPOLATION_RESOLUTION; i++) {
		// This is the Catmull-Rom spline, refactored into a FIR filter
		// If we wanted to, we could stick entirely to integer maths
		// here, but I doubt it's worth the hassle.
		double x = i / (double)CATMULLROM_INTERPOLATION_RESOLUTION;
		double a = x*(x*(x - 2) + 1);
		double b = x*x*(3*x - 5) + 2;
		double c = x*(x*(-3*x + 4) + 1);
		double d = x*x*(1 - x);
		catmullrom_lut[i][0] = (u16)floor((double)(1<<15) * 0.5*a);
		catmullrom_lut[i][1] = (u16)floor((double)(1<<15) * 0.5*b);
		catmullrom_lut[i][2] = (u16)floor((double)(1<<15) * 0.5*c);
		catmullrom_lut[i][3] = (u16)floor((double)(1<<15) * 0.5*d);
	}
	for (size_t i = 0; i < COSINE_INTERPOLATION_RESOLUTION; i++)
		cos_lut[i] = (u16)floor((double)(1<<16) * ((1.0 - cos(((double)i/(double)COSINE_INTERPOLATION_RESOLUTION) * M_PI)) * 0.5));

	SPU_core = new SPU_struct();
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
		SPU_user = new SPU_struct();
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

	_spu_core_cyclesCounter = 0;
}

//------------------------------------------

void SPU_struct::reset()
{
	memset(outbuf,0,bufsize*sizeof(s16)*2);

	memset((void *)channels, 0, sizeof(channel_struct) * 16);

	reconstruct(&regs);

	for(int i = 0; i < 16; i++)
	{
		channels[i].num = i;
	}
}

void SPU_struct::resizeBuffer(int buffersize)
{
	if(outbuf) delete[] outbuf;
	outbuf = new s16[(size_t)buffersize*2];
	bufsize = buffersize;
}

SPU_struct::SPU_struct()
	: outbuf(NULL)
	, bufsize(0)
{
	// mixdata[] must be able to contain:
	// struct {
	//   s32 mixbuf     [N][2]
	//   s32 mutedmixbuf[N][2]
	//   s16 capbuf     [N][2]
	//   s16 chanbuf    [N][2]
	// };
	// where N is at most SPUCAPTURE_FIFO_SIZE
	mixdata = new s32[SPUCAPTURE_FIFO_SIZE * (sizeof(s32)+sizeof(s32)+sizeof(s16)+sizeof(s16))*2 / sizeof(s32)];
	reset();
}

SPU_struct::~SPU_struct()
{
	if(mixdata) delete[] mixdata;
	if(outbuf)  delete[] outbuf;
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

/*FORCEINLINE*/ static void adjust_channel_timer(channel_struct *chan)
{
	//  ARM7_CLOCK / (DESMUME_SAMPLE_RATE*2) / (2^16 - Timer)
	// = ARM7_CLOCK / (DESMUME_SAMPLE_RATE*2 * (2^16 - Timer))
	// Make sure to round DOWN, as we'd rather lag behind
	// than be ahead, as this causes synchronization issues
	u64 sampinc = (ARM7_CLOCK*(1ull << 32)) / (DESMUME_SAMPLE_RATE * 2ull * (0x10000 - chan->timer));
	chan->sampincInt = (u32)(sampinc >> 32), chan->sampincFrac = (u32)sampinc;
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
	thischan.totlength_shifted = thischan.totlength << format_shift[thischan.format];
	thischan.sampcntFrac = 0;
	adjust_channel_timer(&thischan);

	thischan.pcm16bOffs = 0;
	for(int i=0; i < SPUCHAN_PCM16B_SIZE; i++)
		thischan.pcm16b[i] = 0;

	//printf("keyon %d totlength:%d\n",channel,thischan.totlength);


	//LOG("Channel %d key on: vol = %d, volumeDiv = %d, hold = %d, pan = %d, waveduty = %d, repeat = %d, format = %d, source address = %07X,"
	//		"timer = %04X, loop start = %04X, length = %06X, MMU.ARM7_REG[0x501] = %02X\n", channel, chan->vol, chan->volumeDiv, chan->hold, 
	//		chan->pan, chan->waveduty, chan->repeat, chan->format, chan->addr, chan->timer, chan->loopstart, chan->length, T1ReadByte(MMU.ARM7_REG, 0x501));

	switch(thischan.format)
	{
	case 0: // 8-bit
	//	thischan.loopstart = thischan.loopstart << 2;
	//	thischan.length = (thischan.length << 2) + thischan.loopstart;
		thischan.sampcntInt = -3;
		break;
	case 1: // 16-bit
	//	thischan.loopstart = thischan.loopstart << 1;
	//	thischan.length = (thischan.length << 1) + thischan.loopstart;
		thischan.sampcntInt = -3;
		break;
	case 2: // ADPCM
		thischan.pcm16b[0] = (s16)read16(thischan.addr);
		thischan.index = read08(thischan.addr + 2) & 0x7F;
		thischan.sampcntInt = -3;
		thischan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;
	//	thischan.loopstart = thischan.loopstart << 3;
	//	thischan.length = (thischan.length << 3) + thischan.loopstart;
		break;
	case 3: // PSG
		thischan.sampcntInt = -1;
		thischan.x = 0x7FFF;
		break;
	default: break;
	}

	if(thischan.format != 3)
	{
		if(thischan.totlength_shifted == 0)
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

	// Original notes on the reasoning behind a FIFO for capture:
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
	REGS::CAP &cap = regs.cap[which];
	cap.runtime.running = 1;
	cap.runtime.dad = cap.dad;
	u32 len = cap.len;
	if(len==0) len=1;
	cap.runtime.sampcntFrac = 0, cap.runtime.sampcntInt = -SPUCAPTURE_FIFO_DELAY;
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

//////////////////////////////////////////////////////////////////////////////

template<SPUInterpolationMode INTERPOLATE_MODE>
FORCEINLINE static s16 Interpolate(const s16 *pcm16b, u8 pcm16bOffs, u32 subPos)
{
	switch (INTERPOLATE_MODE)
	{
		case SPUInterpolation_CatmullRom:
		{
			// Catmull-Rom spline
			// Delay: 2 samples, Maximum gain: 1.25
			// NOTE: Ideally, we would just re-scale the resampling
			// kernel to have a maximum gain of 1.0. However, this
			// would mean reducing the output volume, which can then
			// go on to make feedback capture (ie. echo effects)
			// decay abnormally quickly. Since Catmull-Rom is more
			// of a 'luxury' thing, we should be able to use MinMax
			// since if the user is using this interpolation method,
			// there's likely enough processing power to handle it.
			s32 a = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+0)];
			s32 b = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+1)];
			s32 c = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+2)];
			s32 d = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+3)];
			const u16 *w = catmullrom_lut[subPos >> (32 - CATMULLROM_INTERPOLATION_RESOLUTION_BITS)];
			return (s16)MinMax((-a*(s32)w[0] + b*(s32)w[1] + c*(s32)w[2] - d*(s32)w[3]) >> 15, -0x8000, +0x7FFF);
		}

		case SPUInterpolation_Cosine:
		{
			// Cosine Interpolation Formula:
			// ratio2 = (1 - cos(ratio * M_PI)) / 2
			// sampleI = sampleA * (1 - ratio2) + sampleB * ratio2
			// Delay: 1 sample, Maximum gain: 1.0
			// NOTE: Always cast the result to s16. (b-a) can
			// overflow, but a+(b-a)*subPos can't. So we might
			// have garbage in the upper 16 bits.
			s32 a = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+0)];
			s32 b = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+1)];
			s32 subPos16 = (s32)cos_lut[subPos >> (32 - COSINE_INTERPOLATION_RESOLUTION_BITS)];
			return (s16)(a + (((b - a)*subPos16) >> 16));
		}

		case SPUInterpolation_Linear:
		{
			// Linear Interpolation Formula:
			// sampleI = sampleA * (1 - ratio) + sampleB * ratio
			// Delay: 1 sample, Maximum gain: 1.0
			// NOTE: Always cast the result to s16 (see above).
			s32 a = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+0)];
			s32 b = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+1)];
			s32 subPos16 = subPos >> (32 - 16);
			return (s16)(a + (((b - a)*subPos16) >> 16));
		}

		default:
			// Delay: 0 samples, Maximum gain: 1.0
			return pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs-SPUCHAN_FIFO_DELAY+0)];
	}
}

FORCEINLINE static s16 Fetch8BitData(channel_struct *chan, s32 pos)
{
	if(pos < 0) return 0;

	return (s16)read_s8(chan->addr + pos*1) << 8;
}

FORCEINLINE static s16 Fetch16BitData(channel_struct *chan, s32 pos)
{
	if(pos < 0) return 0;

	return read16(chan->addr + pos*2);
}

// NOTE: The decoding state is updated during this function call
FORCEINLINE static s16 FetchADPCMData(channel_struct *chan, s32 pos)
{
	if(pos < 8) return 0;

	s16 last = chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)];

	// Stash loop sample and index
	// This saves having to decode to the loop point every time
	if(pos == ((s32)chan->loopstart<<3)) {
		chan->loop_pcm16b = last;
		chan->loop_index = chan->index;
	}
	
	const u8 shift    = ((u8)pos&1) * 4;
	const u8 data4bit = read08(chan->addr + (pos>>1)) >> shift;
	const s32 diff = precalcdifftbl [chan->index][data4bit & 0xF];
	chan->index    = precalcindextbl[chan->index][data4bit & 0x7];

	return (s16)MinMax(last + diff, -0x8000, 0x7FFF);
}

FORCEINLINE static s16 FetchPSGData(channel_struct *chan, s32 pos)
{
	if(pos < 0 || chan->num < 8) return 0;

	// Chan 8..13: Square wave, Chan 14..15: Noise
	if(chan->num < 14)
	{
		// Doing this avoids using a LUT
		return (((u8)pos%8u) > chan->waveduty) ? (-0x7FFF) : (+0x7FFF);
	}
	else
	{
		if(chan->x & 0x1)
		{
			chan->x = (chan->x >> 1) ^ 0x6000;
			return -0x7FFF;
		}
		else
		{
			chan->x >>= 1;
			return +0x7FFF;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

// Returns false when the channel needs to stop
// NOTE: Assumes channel has already reached the end of playback
template<int FORMAT>
/*FORCEINLINE*/ static bool TestForLoop(channel_struct *chan, s32 *pos, s32 totalLength)
{
	// Kill the channel if we don't repeat
	if(chan->repeat != 1)
	{
		//SPU->KeyOff(chan->num); // Inlining this avoids having to pass an SPU pointer around
		chan->status = CHANSTAT_STOPPED;
		return false;
	}

	// Wrap sampcnt
	u32 loopSize = totalLength - (chan->loopstart << format_shift[FORMAT]);
	do *pos -= loopSize; while(*pos >= totalLength);

	// ADPCM needs special handling
	if(FORMAT == 2)
	{
		// Minimum length (the sum of PNT+LEN) is 4 words (16 bytes), 
		// smaller values (0..3 words) are causing hang-ups 
		// (busy bit remains set infinite, but no sound output occurs).
		// fix: 7th Dragon (JP) - http://sourceforge.net/p/desmume/bugs/1357/
		if (totalLength < (4 << format_shift[FORMAT])) return true;

		// Fetch loop sample and index, and get the "new" current decoding position
		s32 curpos;
		s16 *pcm16Dst = &chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)];
		if(chan->loop_index == K_ADPCM_LOOPING_RECOVERY_INDEX)
		{
			// We need to decode from the start until current position,
			// as the loop sample/index is very likely to be incorrect
			*pcm16Dst = (s16)read16(chan->addr);
			chan->index = read08(chan->addr+2) & 0x7F;
			curpos = 8;
		}
		else
		{
			*pcm16Dst = chan->loop_pcm16b;
			chan->index = chan->loop_index;
			curpos = (chan->loopstart << format_shift[FORMAT]);
		}

		// Decode until we reach the target position
		// This is really only used for fast seeking (ie. SNDDummy
		// and loop reset), but makes the code much cleaner.
		while(curpos < *pos)
		{
			*pcm16Dst = FetchADPCMData(chan, curpos);
			curpos++;
		}
	}
	return true;
}

//////////////////////////////////////////////////////////////////////////////

//WORK
template<int CHANNELS, int FORMAT, SPUInterpolationMode INTERPOLATE_MODE> 
static void __SPU_GenerateChanData(channel_struct* const chan, s16 *chanbuf, int length)
{
	s32 totalLength = chan->totlength_shifted;

	if (!CHANNELS)
	{
		// When we aren't mixing at all, take a much faster path where
		// we simply update sampcnt. This can glitch interpolation for
		// up to SPUCHAN_PCM16B_SIZE source samples (since we're not
		// updating chan->pcm16b[]), but this glitching should really
		// only show up when switching from Dual SPU to Sync mode, or
		// when switching from SNDDummy core to an actual output core,
		// and only for non-ADPCM sources (ADPCM needs to decode data
		// all the time, so we keep pcm16b[] filled correctly anyway).
		s32 cursampcntInt = chan->sampcntInt;
		s64 newsampcnt  = (chan->sampcntFrac | (s64)   cursampcntInt<<32);
		    newsampcnt += (chan->sampincFrac | (u64)chan->sampincInt<<32) * length;
		s32 newsampcntInt = (s32)(newsampcnt >> 32);
		if(FORMAT == 2 && newsampcntInt <= totalLength)
		{
			// We won't go past the end, so decode until reaching the target position
			while(cursampcntInt < newsampcntInt)
			{
				s16 data = FetchADPCMData(chan, cursampcntInt);
				chan->pcm16bOffs++;
				chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)] = data;
				cursampcntInt++;
			}
		}
		else if(FORMAT != 3 && newsampcntInt >= totalLength) TestForLoop<FORMAT>(chan, &newsampcntInt, totalLength);
		chan->sampcntFrac = (u32)newsampcnt;
		chan->sampcntInt  = newsampcntInt;
		return;
	}

	// chan->vol is .7fxp, plus .4fxp for chan->volumeDiv (total .11fxp)
	// chan->pan is .7fxp
	// This gives us .18fxp, but we need at most .16fxp, so we shift down.
	s32 vol_shifted   = spumuladjust7(chan->vol);
	    vol_shifted <<= 4;
	    vol_shifted >>= volume_shift[chan->volumeDiv];
	s32 vol_left      = spumuladjust7(127 - chan->pan);
	    vol_left     *= vol_shifted;
	    vol_left    >>= 2; // .16fxp
	s32 vol_right     = spumuladjust7(chan->pan);
	    vol_right    *= vol_shifted;
	    vol_right   >>= 2; // .16fxp

	// Start mixing loop
	u32 sampcntFrac = chan->sampcntFrac;
	s32 sampcntInt  = chan->sampcntInt;
	do
	{
		// Advance sampcnt one sample at a time. This is
		// needed to keep pcm16b[] filled for interpolation.
		u32 nSamplesToSkip = chan->sampincInt + AddAndReturnCarry(&sampcntFrac, chan->sampincFrac);
		while(nSamplesToSkip--)
		{
			// If channel stops, fill the rest of the buffer with 0
			if(FORMAT != 3 && sampcntInt >= totalLength && !TestForLoop<FORMAT>(chan, &sampcntInt, totalLength))
			{
				memset(chanbuf, 0, length*sizeof(s16)*2);
				return;
			}

			s16 data = 0;
			switch(FORMAT)
			{
				case 0: data = Fetch8BitData (chan, sampcntInt); break;
				case 1: data = Fetch16BitData(chan, sampcntInt); break;
				case 2: data = FetchADPCMData(chan, sampcntInt); break;
				case 3: data = FetchPSGData  (chan, sampcntInt); break;
			}
			chan->pcm16bOffs++;
			chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)] = data;
			sampcntInt++;
		}

		// Because chanbuf[] is aligned to at least 32 bits, we can
		// cheat and store a hard-panned sample by writing as 32bit
		s16 sample, sampleL = 0, sampleR = 0; // <- Clearing these to 0 shuts the compiler up
		sample = Interpolate<INTERPOLATE_MODE>(chan->pcm16b, chan->pcm16bOffs, sampcntFrac);
		if(CHANNELS & (1<<0)) sampleL = (s16)(sample * vol_left  >> 16);
		if(CHANNELS & (1<<1)) sampleR = (s16)(sample * vol_right >> 16);
		switch(CHANNELS)
		{
			case (1<<0)|(0<<1):
#ifdef MSB_FIRST
				*(u32*)chanbuf = (u32)sampleL << 16;
#else
				*(u32*)chanbuf = (u32)sampleL;
#endif
				break;
			case (0<<0)|(1<<1):
#ifdef MSB_FIRST
				*(u32*)chanbuf = (u32)sampleR;
#else
				*(u32*)chanbuf = (u32)sampleR << 16;
#endif
				break;
			case (1<<0)|(1<<1):
				chanbuf[0] = sampleL;
				chanbuf[1] = sampleR;
				break;
		}
		chanbuf += 2;
	} while(--length);
	chan->sampcntFrac = sampcntFrac;
	chan->sampcntInt  = sampcntInt;
}

// Outputs {L,R} into chanbuf[]
// Assumes chanbuf[] is always aligned to at least 32 bits
FORCEINLINE static void _SPU_GenerateChanData(bool actuallyMix, channel_struct* const chan, s16 *chanbuf, int length)
{
	typedef void (*_SPU_GenerateChanData_Func_t)(channel_struct* const chan, s16 *chanbuf, int length);

	// This looks insane and pointless, but compilers generate
	// a massive if/elseif block in place of something like this,
	// since they don't know the range of values we use.
	// Note that we use SPUInterpolation_None in the case of PSG
	// channels, as we don't want to interpolate the raw samples.
	// We also use SPUInterpolation_None with actuallyMix==false,
	// so that we avoid instantiating the exact same code under
	// a new template instance.
	// The table is acessed as: FuncTable[INTERPOLATE_MODE][FORMAT][CHANNELS]
#define __GENERATE_FUNCTABLE(CHANNELS, FORMAT, INTERPOLATE_MODE) \
	__SPU_GenerateChanData<CHANNELS, FORMAT, INTERPOLATE_MODE>
#define _GENERATE_FUNCTABLE(FORMAT, INTERPOLATE_MODE) \
	{ \
		__GENERATE_FUNCTABLE((0<<0 | 0<<1), FORMAT, SPUInterpolation_None), \
		__GENERATE_FUNCTABLE((1<<0 | 0<<1), FORMAT, INTERPOLATE_MODE), \
		__GENERATE_FUNCTABLE((0<<0 | 1<<1), FORMAT, INTERPOLATE_MODE), \
		__GENERATE_FUNCTABLE((1<<0 | 1<<1), FORMAT, INTERPOLATE_MODE), \
	}
#define GENERATE_FUNCTABLE(INTERPOLATE_MODE) \
	{ \
		_GENERATE_FUNCTABLE(0, INTERPOLATE_MODE), \
		_GENERATE_FUNCTABLE(1, INTERPOLATE_MODE), \
		_GENERATE_FUNCTABLE(2, INTERPOLATE_MODE), \
		_GENERATE_FUNCTABLE(3, SPUInterpolation_None), \
	}
	static const _SPU_GenerateChanData_Func_t FuncTable[4][4][4] =
	{
		GENERATE_FUNCTABLE(SPUInterpolation_None),
		GENERATE_FUNCTABLE(SPUInterpolation_Linear),
		GENERATE_FUNCTABLE(SPUInterpolation_Cosine),
		GENERATE_FUNCTABLE(SPUInterpolation_CatmullRom),
	};
#undef GENERATE_FUNCTABLE
#undef _GENERATE_FUNCTABLE
#undef __GENERATE_FUNCTABLE

	const _SPU_GenerateChanData_Func_t *Funcs = FuncTable[CommonSettings.spuInterpolationMode][chan->format];
	     if(!actuallyMix)     Funcs[0](chan, chanbuf, length);
	else if(chan->pan == 0)   Funcs[1](chan, chanbuf, length);
	else if(chan->pan == 127) Funcs[2](chan, chanbuf, length);
	else                      Funcs[3](chan, chanbuf, length);
}

template<int CAP_BITS, bool USE_SRCBUF>
/*FORCEINLINE*/ static bool __SPU_WriteCapture(SPU_struct::REGS::CAP& cap, const channel_struct& srcChan, const s16 *srcBuf, int length)
{
	s32 capLen_shifted = cap.len * (32 / CAP_BITS);
	SPU_struct::REGS::CAP::Runtime& runtime = cap.runtime;
	s32 pos = runtime.sampcntInt;
	do
	{
		s16 *data = &runtime.pcm16b[SPUCAPTURE_PCM16B_AT(runtime.pcm16bOffs)];
		u32 nSamplesToProcess = srcChan.sampincInt + AddAndReturnCarry(&runtime.sampcntFrac, srcChan.sampincFrac);
		while(nSamplesToProcess--)
		{
			if(pos >= capLen_shifted)
			{
				if(cap.oneshot)
				{
					cap.active = runtime.running = 0;
					return false;
				}
				pos -= capLen_shifted;
			}

			if(pos >= 0)
			{
				s16 sample = *data;
				if (CAP_BITS == 8)
				{
					_MMU_write08<ARMCPU_ARM7,MMU_AT_DMA>(runtime.dad + pos*sizeof(s8), (u8)(sample >> 8));
				}
				else
				{
					_MMU_write16<ARMCPU_ARM7,MMU_AT_DMA>(runtime.dad + pos*sizeof(s16), (u16)(sample));
				}
			}
			pos++;
		}
		*data = USE_SRCBUF ? (*srcBuf) : 0;
		runtime.pcm16bOffs++;

		// srcBuf[] stores two samples per time unit
		// Either {Ch0[+Ch1],Ch2[+Ch3]}, or {LMix,RMix}
		if(USE_SRCBUF) srcBuf += 2;
	} while(--length);
	runtime.sampcntInt = pos;
	return true;
}

// Writes capture output to capture unit destination
// Returns false if capture has stopped
template<bool USE_SRCBUF>
FORCEINLINE static bool _SPU_WriteCapture(SPU_struct::REGS::CAP& cap, const channel_struct& srcChan, const s16 *srcBuf, int length)
{
	if(cap.bits8)
		return __SPU_WriteCapture< 8,USE_SRCBUF>(cap, srcChan, srcBuf, length);
	else
		return __SPU_WriteCapture<16,USE_SRCBUF>(cap, srcChan, srcBuf, length);
}

// Advances capture unit destination without writing anything
/*FORCEINLINE*/ static void _SPU_SeekCapture(SPU_struct::REGS::CAP& cap, const channel_struct& srcChan, int length)
{
	s32 capLen_shifted = cap.len * (cap.bits8 ? (32/8) : (32/16));
	SPU_struct::REGS::CAP::Runtime& runtime = cap.runtime;
	s64 pos64  = (runtime.sampcntFrac | (s64)runtime.sampcntInt<<32);
	    pos64 += (srcChan.sampincFrac | (u64)srcChan.sampincInt<<32) * length;
	runtime.sampcntFrac = (u32)pos64;
	runtime.sampcntInt  = (s32)(pos64 >> 32);
	if(runtime.sampcntInt >= capLen_shifted)
	{
		if(cap.oneshot)
		{
			cap.active = runtime.running = 0;
		}
		else
		{
			do runtime.sampcntInt -= capLen_shifted; while(runtime.sampcntInt >= capLen_shifted);
		}
	}
}

//ENTER
static void SPU_MixAudio(bool actuallyMix, SPU_struct *SPU, int length)
{
	if (actuallyMix)
	{
		if(SPU->bufsize < length) SPU->resizeBuffer(length);
		memset(SPU->outbuf, 0, length*sizeof(s16)*2);
	}

	//we used to use master enable here, and do nothing if audio is disabled.
	//now, master enable is emulated better..
	//but for a speed optimization we will still do it
	//is this still a good idea? zeroing the capture buffers is important...
	if(!SPU->regs.masteren) return;

	// We used to branch here into advanced/non-advanced mode here.
	// Hopefully, the current code is good enough to avoid the need now...

	/************************************************/

	// Overall flow:
	//  For each channel:
	//    Generate L/R sample data into chanbuf[]
	//    If not bypassed:
	//      If not muted:
	//        Mix chanbuf[] into mixbuf[]
	//      Else if capturing muted channels:
	//        Mix chanbuf[] into mutedmixbuf[]
	//    If capturing from channels:
	//      Copy/mix chanbuf[] into capbuf[]
	//    If not playing from mixer:
	//      Copy/mix chanbuf[] into outbuf[]
	//  If capturing from channels:
	//    Output capbuf[] to capture units
	//  If playing from mixer:
	//    Output mixbuf[] to outbuf[]
	//  If capturing from mixer:
	//    If capturing muted channels:
	//      Output mixbuf[]+mutedmixbuf[] to capture units
	//    Else
	//      Output mixbuf[] to capture units
	
	//we used to bail out if speakers were disabled.
	//this is technically wrong. sound may still be captured, or something.
	//in all likelihood, any game doing this probably master disabled the SPU also
	//so, optimization of this case is probably not necessary.
	//later, we'll just silence the output
	bool speakersOn = T1ReadWord(MMU.ARM7_REG, 0x304) & 0x01;

	// Translate the mixer and capture states.
	// This should improve the code generation so that
	// it doesn't have to reference a lot of memory and
	// can instead just bitwise-test as needed.
	//  -bypassMixer controls whether chanbuf[] should NOT be added to mixbuf[]
	//  -capbufFlags0/1 controls the following:
	//    -The least-significant bit enabled will store UN-PANNED chanbuf[] to capbuf[]
	//    -All other bits will add UN-PANNED chanbuf[] to capbuf[]
	//    -If all zero, capture is either disabled or comes from the mixer
	//  -outbufFlagsL/R controls the following:
	//    -The least-significant bit enabled will store PANNED chanbuf[] to outbuf[]
	//    -All other bits will add PANNED chanbuf[] to outbuf[]
	//    -If all zero, output comes from the mixer
	enum
	{
		CAPSRC_NONE,
		CAPSRC_MIXER, // Capture mixer output
		CAPSRC_CHAN,  // Capture channel 1/3 output
		CAPSRC_MIXED, // Capture channel 0+1/2+3 output (buggy on hardware)
	};
	u8   bypassMixer   = 0;
	u16  chanMuteFlags = CommonSettings.spu_muteChannels;
	u8   capbufFlags0  = 0;
	u8   capbufFlags1  = 0;
	u8   outbufFlagsL  = 0;
	u8   outbufFlagsR  = 0;
	u8   cap0Src       = CAPSRC_NONE;
	u8   cap1Src       = CAPSRC_NONE;
	u8  *mixdata       = (u8*)SPU->mixdata;
	s32 *mixbuf        = NULL;
	s32 *mutedmixbuf   = NULL;
	s16 *capbuf        = NULL;
	s16 *chanbuf       = NULL;
	s16  *outbuf       = SPU->outbuf;
	s32   masterVol    = spumuladjust7(SPU->regs.mastervol);
	int mixdataClearSizeBytes = 0;
	if(actuallyMix)
	{
		if(SPU->regs.ctl_ch1bypass) bypassMixer |= (1 << 1);
		if(SPU->regs.ctl_ch3bypass) bypassMixer |= (1 << 3);

		// Translate capture state
		if(SPU->regs.cap[0].runtime.running)
		{
			if(SPU->regs.cap[0].source == 0) cap0Src = CAPSRC_MIXER;
			else if(SPU->regs.cap[0].add) cap0Src = CAPSRC_MIXED;
			else cap0Src = CAPSRC_CHAN;
		}
		if(SPU->regs.cap[1].runtime.running)
		{
			if(SPU->regs.cap[1].source == 0) cap1Src = CAPSRC_MIXER;
			else if(SPU->regs.cap[1].add) cap1Src = CAPSRC_MIXED;
			else cap1Src = CAPSRC_CHAN;
		}
		if(cap0Src == CAPSRC_CHAN || cap0Src == CAPSRC_MIXED) capbufFlags0 |= (1 << 0);
		if(                          cap0Src == CAPSRC_MIXED) capbufFlags0 |= (1 << 1);
		if(cap1Src == CAPSRC_CHAN || cap1Src == CAPSRC_MIXED) capbufFlags1 |= (1 << 2);
		if(                          cap1Src == CAPSRC_MIXED) capbufFlags1 |= (1 << 3);
		bool isCapturing  = (cap0Src != CAPSRC_NONE) || (cap1Src != CAPSRC_NONE);
		bool captureMuted = isCapturing && CommonSettings.spu_captureMuted;
		if(!captureMuted)
		{
			capbufFlags0 &= ~chanMuteFlags;
			capbufFlags1 &= ~chanMuteFlags;
		}

		// Translate outputs
		switch(SPU->regs.ctl_left)
		{
			case SPU_struct::REGS::LOM_CH1:
				outbufFlagsL = (1 << 1);
				break;
			case SPU_struct::REGS::LOM_CH3:
				outbufFlagsL = (1 << 3);
				break;
			case SPU_struct::REGS::LOM_CH1_PLUS_CH3:
				outbufFlagsL = (1 << 1) | (1 << 3);
				break;
		}
		switch(SPU->regs.ctl_right)
		{
			case SPU_struct::REGS::ROM_CH1:
				outbufFlagsR = (1 << 1);
				break;
			case SPU_struct::REGS::ROM_CH3:
				outbufFlagsR = (1 << 3);
				break;
			case SPU_struct::REGS::ROM_CH1_PLUS_CH3:
				outbufFlagsR = (1 << 1) | (1 << 3);
				break;
		}

		// Generate mixing pointers
		// This setup is so we can clear everything in a single memset() call
		// PONDER: Can we put these on the stack?
		// ie. u8 mixdata[FIFO_SIZE * (sizeof(s32)+sizeof(s32)+sizeof(s16)+sizeof(s16))*2]
		int mixBufSize = MIN(length, SPUCAPTURE_FIFO_SIZE);
		u8 *nextdata = mixdata;
		if(actuallyMix)  mixbuf      = (s32*)nextdata, nextdata += mixBufSize * sizeof(s32)*2;
		if(captureMuted) mutedmixbuf = (s32*)nextdata, nextdata += mixBufSize * sizeof(s32)*2;
		if(isCapturing)  capbuf      = (s16*)nextdata, nextdata += mixBufSize * sizeof(s16)*2;
		if(actuallyMix)  chanbuf     = (s16*)nextdata; // <- Do not increment nextData
		mixdataClearSizeBytes = nextdata - mixdata;
	}
	else
	{
		// If we end up here, we're either mixing the core SPU while
		// in dual SPU mode (meaning we shouldn't output data from
		// the capture unit, as we'll do this in the user SPU), or
		// the output core is SNDDummy (with the capture units either
		// not running, or disabled via ENABLE_DUMMY_SPU_CAPTURE==0).
		// In the former case, we can just seek the capture position,
		// but in the latter case, we must output silence to avoid
		// potentially leaving the capture buffers filled with garbage.
		bool captureZeros = (SPU_SoundCore() == &SNDDummy);
		if(SPU->regs.cap[0].runtime.running)
		{
			if(captureZeros) _SPU_WriteCapture<false>(SPU->regs.cap[0], SPU->channels[1], NULL, length);
			else             _SPU_SeekCapture        (SPU->regs.cap[0], SPU->channels[1],       length);
		}
		if(SPU->regs.cap[1].runtime.running)
		{
			if(captureZeros) _SPU_WriteCapture<false>(SPU->regs.cap[1], SPU->channels[3], NULL, length);
			else             _SPU_SeekCapture        (SPU->regs.cap[1], SPU->channels[3],       length);
		}
	}

	while(length)
	{
		if(mixdataClearSizeBytes) memset(mixdata, 0, mixdataClearSizeBytes);

		// We can only process at most SPUCAPTURE_FIFO_SIZE samples
		// per mixing batch, in case the capture buffers wrap around.
		// Technically, we could actually check if this is needed at
		// all, but this should work well enough as is.
		int thisLength = MIN(length, SPUCAPTURE_FIFO_SIZE);
		length -= thisLength;

		// Process each channel in turn
		// Note that we are using unsigned overflow to avoid counting
		// directly, as we need to keep track of the bit index anyway
		channel_struct *chan = SPU->channels;
		for(u16 chanBit=1; chanBit != 0; chan++, chanBit <<= 1)
		{
			if (chan->status != CHANSTAT_PLAY) continue;
		
			// Generate data into chanbuf[]
			// NOTE: If actuallyMix==false, the channel is updated but no data is generated.
			_SPU_GenerateChanData(actuallyMix, chan, chanbuf, thisLength);
			if(!actuallyMix) continue;

			// Bypass means we must NOT mix this channel into mixbuf[] OR mutedmixbuf[]
			if((bypassMixer & chanBit) == 0)
			{
				s32 *mixtarget = ((chanMuteFlags & chanBit) == 0) ? mixbuf : mutedmixbuf;
				if(mixtarget)
				{
					for(int n=0; n < thisLength*2; n++) mixtarget[n] += chanbuf[n];
				}
			}

			// Generate outputs for channel capture
			// Yes, we have to undo the panning here, but that's fine.
			// Incidentally, this emulates the ch(a)+ch(b) overflow bug
			if((capbufFlags0 & chanBit) != 0)
			{
				if((capbufFlags0 & (chanBit-1)) == 0)
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+0]  = chanbuf[n*2+0] + chanbuf[n*2+1];
				else
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+0] += chanbuf[n*2+0] + chanbuf[n*2+1];
			}
			if((capbufFlags1 & chanBit) != 0)
			{
				if((capbufFlags1 & (chanBit-1)) == 0)
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+1]  = chanbuf[n*2+0] + chanbuf[n*2+1];
				else
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+1] += chanbuf[n*2+0] + chanbuf[n*2+1];
			}

			// If speakers are turned off or the channel is muted, we can skip
			// setting outbuf[]. Note that if the channel is muted and we are
			// generating outbuf[] from chanbuf[], outbuf[] must already have
			// been cleared to silence.
			if(!speakersOn || (chanMuteFlags & chanBit) != 0) continue;

			// Set outbuf[] from chanbuf[] when L/R source is not the mixer
			// Note that Ch1+Ch3 mode clips as intended; only capture has overflow bugs
			if((outbufFlagsL & chanBit) != 0)
			{
				if((outbufFlagsL & (chanBit-1)) == 0)
					for(int n=0; n < thisLength; n++)
						outbuf[n*2+0] =                        (chanbuf[n*2+0] * masterVol >> 7);
				else
					for(int n=0; n < thisLength; n++)
						outbuf[n*2+0] = MinMax(outbuf[n*2+0] + (chanbuf[n*2+0] * masterVol >> 7), -0x8000, +0x7FFF);
			}
			if((outbufFlagsR & chanBit) != 0)
			{
				if((outbufFlagsR & (chanBit-1)) == 0)
					for(int n=0; n < thisLength; n++)
						outbuf[n*2+1] =                        (chanbuf[n*2+1] * masterVol >> 7);
				else
					for(int n=0; n < thisLength; n++)
						outbuf[n*2+1] = MinMax(outbuf[n*2+1] + (chanbuf[n*2+1] * masterVol >> 7), -0x8000, +0x7FFF);
			}
		}

		// Generate mixer output to outbuf[]
		if(mixbuf && speakersOn)
		{
			if(outbufFlagsL == 0)
			{
				for(int n=0; n < thisLength; n++)
					outbuf[n*2+0] = MinMax(mixbuf[n*2+0] * masterVol >> 7, -0x8000, +0x7FFF);
			}
			if(outbufFlagsR == 0)
			{
				for(int n=0; n < thisLength; n++)
					outbuf[n*2+1] = MinMax(mixbuf[n*2+1] * masterVol >> 7, -0x8000, +0x7FFF);
			}
		}

		// Generate final capture output
		if(cap0Src != CAPSRC_NONE)
		{
			if(cap0Src == CAPSRC_MIXER)
			{
				if(mutedmixbuf)
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+0] = MinMax(mixbuf[n*2+0] + mutedmixbuf[n*2+0], -0x8000, +0x7FFF);
				else
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+0] = MinMax(mixbuf[n*2+0],                      -0x8000, +0x7FFF);
			}
			bool run = _SPU_WriteCapture<true>(SPU->regs.cap[0], SPU->channels[1], capbuf, thisLength);
			if(!run) cap0Src = CAPSRC_NONE, capbufFlags0 = 0;
		}
		if(cap1Src != CAPSRC_NONE)
		{
			if(cap1Src == CAPSRC_MIXER)
			{
				if(mutedmixbuf)
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+1] = MinMax(mixbuf[n*2+1] + mutedmixbuf[n*2+1], -0x8000, +0x7FFF);
				else
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+1] = MinMax(mixbuf[n*2+1],                      -0x8000, +0x7FFF);
			}
			bool run = _SPU_WriteCapture<true>(SPU->regs.cap[1], SPU->channels[3], capbuf+1, thisLength);
			if(!run) cap1Src = CAPSRC_NONE, capbufFlags1 = 0;
		}

		// Advance buffer
		outbuf += (size_t)thisLength*2;
	}
}

//////////////////////////////////////////////////////////////////////////////


// Emulates the cpu core for the specified number of ARM7 cycles.
// This will produce a variable number of samples to sync to DESMUME_SAMPLE_RATE
int SPU_Emulate_core(u32 numberOfARM7Cycles)
{
	SoundInterface_struct *soundProcessor = SPU_SoundCore();

	// We don't need to mix audio for Dual Synch/Asynch mode since we do this
	// later in SPU_Emulate_user(). Disable mixing here to speed up processing.
	// If we are outputting to the dummy core, we can disable all mixing if
	// we are not capturing the output at all, increasing performance.
	bool needToMix = false;
	if(soundProcessor != &SNDDummy)
		needToMix = (_currentSynchMode != ESynchMode_DualSynchAsynch);
#if ENABLE_DUMMY_SPU_CAPTURE
	else if(SPU_core->regs.cap[0].runtime.running || SPU_core->regs.cap[1].runtime.running)
		needToMix = true;
#endif

	// If we are recording, we will need to mix the core SPU regardless of anything else
	// NOTE: Technically, we should be checking wavWriter.mode==WAVMODE_CORE, but that
	// is only enabled with DEVELOPER_MENU_ITEMS, and we won't break anything anyway.
	needToMix = needToMix || driver->AVI_IsRecording() || driver->WAV_IsRecording();

	// NOTE: We used to keep a double-type counter here, and pre-divided by
	// ARM7_CLOCK. This is probably enough for most cases, but for the sake
	// of perfect accuracy (at least in regards to this), we use a cycles
	// counter instead here, and figure out the sample count from there.
	int samplesToMix;
	{
		// minMixSize controls the mixing latency, which reduces the
		// overhead of the update routines at the cost of synchronicity.
		// NOTE: minMixSize must be <= 128, or else _spu_core_cyclesCounter
		// would cause 32bit overflow if we postpone mixing for long enough.
		// We could use a 64bit counter instead, but 128 samples should be plenty.
		static const u32 doMix_minMixSize = 1; // <- Setting this too high can break streams, so keep at minimum
		static const u32 noMix_minMixSize = 64;
		u64 minDeltaCycles = (u64)(needToMix ? doMix_minMixSize : noMix_minMixSize) * ARM7_CLOCK;
		u64 cycles64 = _spu_core_cyclesCounter + (u64)numberOfARM7Cycles*DESMUME_SAMPLE_RATE;
		if(cycles64 < minDeltaCycles)
		{
			_spu_core_cyclesCounter = (u32)cycles64;
			return 0;
		}
		samplesToMix            = (int)(cycles64 / ARM7_CLOCK);
		_spu_core_cyclesCounter = (u32)(cycles64 % ARM7_CLOCK);
	}
	
	SPU_MixAudio(needToMix, SPU_core, samplesToMix);
	
	if (soundProcessor != NULL)
	{
		if (soundProcessor->FetchSamples != NULL)
		{
			soundProcessor->FetchSamples(SPU_core->outbuf, samplesToMix, _currentSynchMode, _currentSynchronizer);
		}
		else
		{
			SPU_DefaultFetchSamples(SPU_core->outbuf, samplesToMix, _currentSynchMode, _currentSynchronizer);
		}
	}

	return samplesToMix;
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
	
	//printf("mix %i samples\n", audiosize);
	if (freeSampleCount > _currentBufferSize)
	{
		freeSampleCount = _currentBufferSize;
	}
	if (freeSampleCount == 0)
	{
		return;
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
	size_t elems_written = fwrite(soundData, sizeof(s16)*2, numSamples, spufp);
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
	os.write_32LE(8);

	SPU_struct *spu = SPU_core;

	os.write_u8(SPUCHAN_PCM16B_SIZE);
	for (int j = 0; j < 16; j++)
	{
		channel_struct &chan = spu->channels[j];
		os.write_u8(chan.vol);
		os.write_u8(chan.volumeDiv);
		os.write_u8(chan.hold);
		os.write_u8(chan.pan);
		os.write_u8(chan.waveduty);
		os.write_u8(chan.repeat);
		os.write_u8(chan.format);
		os.write_u8(chan.status);
		os.write_u8(chan.pcm16bOffs);
		os.write_32LE(chan.addr);
		os.write_16LE(chan.timer);
		os.write_16LE(chan.loopstart);
		os.write_32LE(chan.length);
		os.write_32LE(chan.sampcntFrac);
		os.write_32LE(chan.sampcntInt);
		for (int i = 0; i < SPUCHAN_PCM16B_SIZE; i++) os.write_16LE(chan.pcm16b[i]);
		os.write_u8(chan.index);
		os.write_16LE(chan.x);
		os.write_u8(chan.keyon);
	}

	os.write_32LE(_spu_core_cyclesCounter);

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
		os.write_32LE(spu->regs.cap[i].runtime.dad);
		os.write_32LE(spu->regs.cap[i].runtime.sampcntFrac);
		os.write_32LE(spu->regs.cap[i].runtime.sampcntInt);
	}

	os.write_u8(SPUCAPTURE_FIFO_SIZE);
	for (int i = 0; i < 2; i++)
	{
		os.write_u8(spu->regs.cap[i].runtime.pcm16bOffs);
		for (int n = 0; n < SPUCAPTURE_FIFO_SIZE; n++)
			os.write_16LE(spu->regs.cap[i].runtime.pcm16b[n]);
	}
}

bool spu_loadstate(EMUFILE &is, int size)
{
	//read version
	u32 version;
	if (is.read_32LE(version) != 1) return false;

	SPU_struct *spu = SPU_core;
	reconstruct(&SPU_core->regs);

	int pcm16bSz_Chan = (version >= 8) ? (int)is.read_u8() : 4;
	for (int j = 0; j < 16; j++)
	{
		channel_struct &chan = spu->channels[j];
		if(version < 8) is.read_32LE(chan.num); else chan.num = j;
		is.read_u8(chan.vol);
		is.read_u8(chan.volumeDiv);
		if (chan.volumeDiv == 4) chan.volumeDiv = 3;
		is.read_u8(chan.hold);
		is.read_u8(chan.pan);
		is.read_u8(chan.waveduty);
		is.read_u8(chan.repeat);
		is.read_u8(chan.format);
		is.read_u8(chan.status);
		if (version >= 7) chan.pcm16bOffs = SPUCHAN_PCM16B_AT(is.read_u8());
		is.read_32LE(chan.addr);
		is.read_16LE(chan.timer);
		is.read_16LE(chan.loopstart);
		is.read_32LE(chan.length);
		chan.totlength = chan.length + chan.loopstart;
		chan.totlength_shifted = chan.totlength << format_shift[chan.format];
		if(version >= 7) {
			is.read_32LE(chan.sampcntFrac);
			is.read_32LE(chan.sampcntInt);
			if(version < 8) is.fseek(8, SEEK_CUR); // chan.sampincFrac (LE32), chan.sampincInt (LE32)
		}
		else /*if (version >= 2)*/ // <- This check (and its broken else clause) was never needed
		{
			s64 temp = (s64)(is.read_doubleLE() * (double)(1ll << 32));
			chan.sampcntFrac = (u32)temp;
			chan.sampcntInt  = (s32)(temp >> 32);
			is.fseek(8, SEEK_CUR); // chan.sampinc (LEdouble)
		}
		if (version >= 7) {
			for (int i = 0; i < pcm16bSz_Chan; i++) is.read_16LE(chan.pcm16b[SPUCHAN_PCM16B_AT(i)]);
		}
		else
		{
			is.fseek(4, SEEK_CUR);        // chan.lastsampcnt (LE32)
			is.read_16LE(chan.pcm16b[0]); // chan.pcm16b
			is.fseek(2, SEEK_CUR);        // chan.pcm16b_last (LE16)
		}
		chan.index = (version >= 8) ? is.read_u8() : (u8)is.read_s32LE();
		is.read_16LE(chan.x);
		if (version < 7) is.fseek(2, SEEK_CUR); // chan.psgnoise_last (LE16)

		if (version >= 4)
			is.read_u8(chan.keyon);

		// Because we don't save sampinc, we need to recalculate it
		adjust_channel_timer(&chan);

		//hopefully trigger a recovery of the adpcm looping system
		chan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;
	}

	if (version >= 8)
	{
		is.read_32LE(_spu_core_cyclesCounter);
	}
	else if (version >= 2)
	{
		_spu_core_cyclesCounter = (u32)(is.read_doubleLE() * ARM7_CLOCK); // _samples (doubleLE)
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
			if (version >= 8) is.read_32LE(spu->regs.cap[i].dad);
			else {
				is.fseek(4, SEEK_CUR); // regs.cap[i].runtime.curdad (LE32)
				is.read_32LE(spu->regs.cap[i].dad); // regs.cap[i].runtime.maxdad
				spu->regs.cap[i].dad -= spu->regs.cap[i].len*4;
			}
			if (version >= 7) {
				is.read_32LE(spu->regs.cap[i].runtime.sampcntFrac);
				is.read_32LE(spu->regs.cap[i].runtime.sampcntInt);
			}
			else
			{
				s64 temp = (s64)(is.read_doubleLE() * (double)(1ll << 32));
				spu->regs.cap[i].runtime.sampcntFrac = (u32)temp;
				spu->regs.cap[i].runtime.sampcntInt  = (s32)(temp >> 32);
			}
			if(version <= 7)
			{
				// Before, sampcnt incremented "as expected" and the FIFO
				// delay was implemented within the SndFifo construct.
				// Now, though, we create the delay by setting sampcnt to
				// -FIFO_SIZE on starting capture, so account for this here.
				spu->regs.cap[i].runtime.sampcntInt -= SPUCAPTURE_FIFO_SIZE;
			}
		}
	}

	int pcm16bSz_Capture = (version >= 8) ? (int)is.read_u8() : 16;
	if (version >= 8)
		for (int i=0;i<2;i++)
		{
			spu->regs.cap[i].runtime.pcm16bOffs = SPUCAPTURE_PCM16B_AT(is.read_u8());
			for (int n = 0; n < pcm16bSz_Capture; n++)
				is.read_16LE(spu->regs.cap[i].runtime.pcm16b[SPUCAPTURE_PCM16B_AT(n)]);
		}
	else if (version >= 6)
		for (int i=0;i<2;i++)
		{
			// Setting pcm16bOffs to -fifo.size ensures that we always
			// fill at the correct offset relative to the FIFO queue size
			SPUFifo fifo;
			fifo.load(is);
			spu->regs.cap[i].runtime.pcm16bOffs = (u8)(-fifo.size);
			for (int n = 0; n < 16; n++)
				spu->regs.cap[i].runtime.pcm16b[SPUCAPTURE_PCM16B_AT(n)] = fifo.dequeue();
		}

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
