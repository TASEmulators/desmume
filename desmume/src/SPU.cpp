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

#define CATMULLROM_INTERPOLATION_RESOLUTION_BITS 11
#define CATMULLROM_INTERPOLATION_RESOLUTION (1<<CATMULLROM_INTERPOLATION_RESOLUTION_BITS)

#define COSINE_INTERPOLATION_RESOLUTION_BITS 13
#define COSINE_INTERPOLATION_RESOLUTION (1<<COSINE_INTERPOLATION_RESOLUTION_BITS)

#define SPUCHAN_PCM16B_AT(x) ((u32)(x) % SPUINTERPOLATION_TAPS)

#define SPUCAPTURE_PCM16B_AT(x) ((u32)(x) % SPUCAPTURE_FIFO_SIZE)

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

static s32 precalcdifftbl[89][16];
static u8 precalcindextbl[89][8];
static u16 catmullrom_lut[CATMULLROM_INTERPOLATION_RESOLUTION][4];
static u16 cos_lut[COSINE_INTERPOLATION_RESOLUTION];

static const u32 ARM7_CLOCK = 33513982;

// This is set to update once per scanline (SAMPLE_RATE / FRAME_RATE / HDOTS), as
// implemented via calling SPU_Emulate_core() in execHardware_hblank() (NDSSystem.cpp).
// For better performance, it might work better to update slightly less often, as this
// should reduce the mixing overhead. So in case we ever get around to changing the
// execution frequency, this has been pre-emptively renamed to "samples per update"
// rather than "samples per hblank".
static const u64 samples_per_core_update = ((DESMUME_SAMPLE_RATE*355*6*(1ull<<32)-1) / ARM7_CLOCK + 1); // 355 hdots, 6c per dot
static u32 _spucore_samplesFrac = 0; // We only need to keep track of the fractional part
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
		double a = x*(x*(-x + 2) - 1);
		double b = x*x*(3*x - 5) + 2;
		double c = x*(x*(-3*x + 4) + 1);
		double d = x*x*(x - 1);
		catmullrom_lut[i][0] = (u16)floor((1u<<15) * -0.5*a);
		catmullrom_lut[i][1] = (u16)floor((1u<<15) *  0.5*b);
		catmullrom_lut[i][2] = (u16)floor((1u<<15) *  0.5*c);
		catmullrom_lut[i][3] = (u16)floor((1u<<15) * -0.5*d);
	}
	for (size_t i = 0; i < COSINE_INTERPOLATION_RESOLUTION; i++)
		cos_lut[i] = (u16)floor((1u<<16) * ((1.0 - cos(((double)i/(double)COSINE_INTERPOLATION_RESOLUTION) * M_PI)) * 0.5));

	SPU_core = new SPU_struct((samples_per_core_update >> 32) + ((u32)samples_per_core_update != 0)); // ceil(samples_per_core_update)
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

	_spucore_samplesFrac = 0;
}

//------------------------------------------

void SPU_struct::reset()
{
	memset(mixbuf,0,SPUCAPTURE_FIFO_SIZE*sizeof(s32)*2);
	memset(outbuf,0,bufsize*sizeof(s16)*2);

	memset((void *)channels, 0, sizeof(channel_struct) * 16);

	reconstruct(&regs);

	for(int i = 0; i < 16; i++)
	{
		channels[i].num = i;
	}
}

SPU_struct::SPU_struct(int buffersize)
	: mixbuf(NULL)
	, outbuf(NULL)
	, capbuf(NULL)
	, chanbuf(NULL)
	, bufsize(buffersize)
{
	// We process at most SPUCAPTURE_FIFO_SIZE samples
	// per mixing batch, so mixbuf[],capbuf[],chanbuf[]
	// only need to be as large as that
	mixbuf  = new s32[SPUCAPTURE_FIFO_SIZE*2];
	outbuf  = new s16[buffersize*2];
	capbuf  = new s16[SPUCAPTURE_FIFO_SIZE*2];
	chanbuf = new s16[SPUCAPTURE_FIFO_SIZE*2];
	reset();
}

SPU_struct::~SPU_struct()
{
	if(mixbuf)  delete[] mixbuf;
	if(outbuf)  delete[] outbuf;
	if(capbuf)  delete[] capbuf;
	if(chanbuf) delete[] chanbuf;
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
	//  ARM7_CLOCK / (DESMUME_SAMPLE_RATE*2) / (2^16 - Timer)
	// = ARM7_CLOCK / (DESMUME_SAMPLE_RATE*2 * (2^16 - Timer))
	// ... and then round up for good measure
	u64 sampinc = (ARM7_CLOCK*(1ull << 32) - 1) / (DESMUME_SAMPLE_RATE * 2ull * (0x10000 - chan->timer)) + 1;
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
	adjust_channel_timer(&thischan);

	thischan.pcm16bOffs = 0;
	for(int i=0;i<SPUINTERPOLATION_TAPS;i++)
	{
		thischan.pcm16b[i] = 0;
	}

	//printf("keyon %d totlength:%d\n",channel,thischan.totlength);


	//LOG("Channel %d key on: vol = %d, volumeDiv = %d, hold = %d, pan = %d, waveduty = %d, repeat = %d, format = %d, source address = %07X,"
	//		"timer = %04X, loop start = %04X, length = %06X, MMU.ARM7_REG[0x501] = %02X\n", channel, chan->vol, chan->volumeDiv, chan->hold, 
	//		chan->pan, chan->waveduty, chan->repeat, chan->format, chan->addr, chan->timer, chan->loopstart, chan->length, T1ReadByte(MMU.ARM7_REG, 0x501));

	switch(thischan.format)
	{
	case 0: // 8-bit
	//	thischan.loopstart = thischan.loopstart << 2;
	//	thischan.length = (thischan.length << 2) + thischan.loopstart;
		thischan.sampcntFrac = 0, thischan.sampcntInt = -3;
		break;
	case 1: // 16-bit
	//	thischan.loopstart = thischan.loopstart << 1;
	//	thischan.length = (thischan.length << 1) + thischan.loopstart;
		thischan.sampcntFrac = 0, thischan.sampcntInt = -3;
		break;
	case 2: // ADPCM
		thischan.pcm16b[0] = (s16)read16(thischan.addr);
		thischan.index = read08(thischan.addr + 2) & 0x7F;
		thischan.sampcntFrac = 0, thischan.sampcntInt = -3;
		thischan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;
	//	thischan.loopstart = thischan.loopstart << 3;
	//	thischan.length = (thischan.length << 3) + thischan.loopstart;
		break;
	case 3: // PSG
		thischan.sampcntFrac = 0, thischan.sampcntInt = -1;
		thischan.x = 0x7FFF;
		break;
	default: break;
	}

	thischan.totlength_shifted = thischan.totlength << format_shift[thischan.format];

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
	cap.runtime.curdad = cap.dad;
	u32 len = cap.len;
	if(len==0) len=1;
	cap.runtime.maxdad = cap.dad + len*4;
	cap.runtime.sampcntFrac = 0, cap.runtime.sampcntInt = -SPUCAPTURE_FIFO_SIZE;
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

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE s16 Interpolate(const s16 *pcm16b, u8 pcm16bOffs, u32 subPos)
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
			s32 a = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 3)];
			s32 b = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 2)];
			s32 c = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 1)];
			s32 d = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 0)];
			const u16 *w = catmullrom_lut[subPos >> (32 - CATMULLROM_INTERPOLATION_RESOLUTION_BITS)];
			return (s16)MinMax((-a*(s32)w[0] + b*(s32)w[1] + c*(s32)w[2] - d*(s32)w[3]) >> 15, -0x8000, +0x7FFF);
		}

		case SPUInterpolation_Cosine:
		{
			// Cosine Interpolation Formula:
			// ratio2 = (1 - cos(ratio * M_PI)) / 2
			// sampleI = sampleA * (1 - ratio2) + sampleB * ratio2
			// Delay: 1 sample, Maximum gain: 1.0
			// NOTE: The weird casts are necessary to get correct
			// results. This is because (b-a) can overflow in 16bit
			// arithmetic, but a+(b-a)*subPos can't. This workaround
			// ensures that the overflowed result wraps back around
			// to the intended, not-overflowed result. Note that if
			// we ever return something larger than 16bit in the
			// future, the cast to s16 must still remain, as this
			// workaround only works when the result is 16bit.
			s32 a = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 1)];
			s32 b = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 0)];
			s32 subPos16 = (s32)cos_lut[subPos >> (32 - COSINE_INTERPOLATION_RESOLUTION_BITS)];
			return (s16)(a + ((u32)((b - a)*subPos16) >> 16));
		}

		case SPUInterpolation_Linear:
		{
			// Linear Interpolation Formula:
			// sampleI = sampleA * (1 - ratio) + sampleB * ratio
			// Delay: 1 sample, Maximum gain: 1.0
			// NOTE: Same weird casts as in cosine interpolation.
			s32 a = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 1)];
			s32 b = pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs - 0)];
			s32 subPos16 = subPos >> (32 - 16);
			return (s16)(a + ((u32)((b - a)*subPos16) >> 16));
		}

		default:
			// Delay: 0 samples, Maximum gain: 1.0
			return pcm16b[SPUCHAN_PCM16B_AT(pcm16bOffs)];
	}
}

static FORCEINLINE s16 Fetch8BitData(channel_struct *chan, s32 pos)
{
	if(pos < 0) return 0;

	return read_s8(chan->addr + pos*1) << 8;
}

static FORCEINLINE s16 Fetch16BitData(channel_struct *chan, s32 pos)
{
	if(pos < 0) return 0;

	return read16(chan->addr + pos*2);
}

static FORCEINLINE s16 FetchADPCMData(channel_struct *chan, s32 pos)
{
	if(pos < 8) return 0;

	s16 last = chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)];

	if(pos == (chan->loopstart<<3)) {
		//if(chan->loop_index != K_ADPCM_LOOPING_RECOVERY_INDEX) printf("over-snagging\n");
		chan->loop_pcm16b = last;
		chan->loop_index = chan->index;
	}
	
	const u32 shift    = (pos&1) * 4;
	const u32 data4bit = ((u32)read08(chan->addr + (pos>>1))) >> shift;
	const s32 diff = precalcdifftbl [chan->index][data4bit & 0xF];
	chan->index    = precalcindextbl[chan->index][data4bit & 0x7];

	return MinMax(last + diff, -0x8000, 0x7FFF);
}

static FORCEINLINE s16 FetchPSGData(channel_struct *chan, s32 pos)
{
	if(pos < 0 || chan->num < 8) return 0;

	// Chan 8..13: Square wave, Chan 14..15: Noise
	if(chan->num < 14)
	{
		// Doing this avoids using a LUT
		return ((pos%8u) > chan->waveduty) ? (-0x7FFF) : (+0x7FFF);
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
template<int FORMAT> static FORCEINLINE bool TestForLoop(SPU_struct *SPU, channel_struct *chan, s32& pos)
{
	s32 totalLength = chan->totlength_shifted;

	// Do nothing if we haven't reached the end
	if(pos < totalLength) return true;

	// Kill the channel if we don't repeat
	if(chan->repeat != 1)
	{
		SPU->KeyOff(chan->num);
		return false;
	}

	// ADPCM needs special handling
	if(FORMAT == 2)
	{
		// Minimum length (the sum of PNT+LEN) is 4 words (16 bytes), 
		// smaller values (0..3 words) are causing hang-ups 
		// (busy bit remains set infinite, but no sound output occurs).
		// fix: 7th Dragon (JP) - http://sourceforge.net/p/desmume/bugs/1357/
		if (totalLength < (4 << format_shift[FORMAT])) return true;

		// Stash loop sample and index
		if(chan->loop_index == K_ADPCM_LOOPING_RECOVERY_INDEX)
		{
			chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)] = (s16)read16(chan->addr);
			chan->index = read08(chan->addr+2) & 0x7F;
		}
		else
		{
			chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)] = chan->loop_pcm16b;
			chan->index = chan->loop_index;
		}
	}

	// Wrap sampcnt
	u32 step = totalLength - (chan->loopstart << format_shift[FORMAT]);
	do pos -= step; while (pos >= totalLength);
	return true;
}

//////////////////////////////////////////////////////////////////////////////

//WORK
// Outputs {L,R} into chanbuf[]
template<int CHANNELS, int FORMAT, SPUInterpolationMode INTERPOLATE_MODE> 
FORCEINLINE static void ____SPU_GenerateChanData(SPU_struct* const SPU, channel_struct* const chan, s16 *chanbuf, int length)
{
	if (!CHANNELS && FORMAT != 2)
	{
		// When we aren't mixing at all, take a much
		// faster path where we simply update sampcnt
		// NOTE: This fast path can't be used for ADPCM
		// as we need to keep the decoding state updated
		s64 pos64  = (chan->sampcntFrac | (s64)chan->sampcntInt<<32);
		    pos64 += (chan->sampincFrac | (u64)chan->sampincInt<<32) * length;
		chan->sampcntFrac = (u32)pos64;
		chan->sampcntInt  = (s32)(pos64 >> 32);
		if (FORMAT != 3) TestForLoop<FORMAT>(SPU, chan, chan->sampcntInt);
		return;
	}

	// chan->vol is .7fxp, plus .4fxp for chan->volumeDiv (total .11fxp)
	// chan->pan is .7fxp
	// This gives us .18fxp, but we need at most .16fxp, so we shift down.
	s32 vol_shifted   = chan->vol;
	    vol_shifted  += (vol_shifted == 127);
	    vol_shifted <<= 4;
	    vol_shifted >>= volume_shift[chan->volumeDiv];
	s32 vol_left      = 127 - chan->pan;
	    vol_left     += (vol_left >= 127); // Using >= might generate better code on some platforms
	    vol_left     *= vol_shifted;
	    vol_left    >>= 2; // .16fxp
	s32 vol_right     = chan->pan;
	    vol_right    += (vol_right >= 127);
	    vol_right    *= vol_shifted;
	    vol_right   >>= 2; // .16fxp
	
	// If we are only mixing to one channel, it should be more
	// performant to clear the buffer once rather than doing a
	// partial fill with 0s in every other sample
	if(CHANNELS && CHANNELS != ((1<<0)|(1<<1)))
		memset(chanbuf, 0, length*sizeof(s16)*2);

	s32 pos = chan->sampcntInt;
	do
	{
		// Advance sampcnt one sample at a time. This is
		// needed to keep pcm16b[] filled for interpolation.
		u32 nSamplesToSkip = chan->sampincInt + AddAndReturnCarry(&chan->sampcntFrac, chan->sampincFrac);
		while(nSamplesToSkip--)
		{
			s16 data = 0;
			switch(FORMAT)
			{
				case 0: data = Fetch8BitData (chan, pos); break;
				case 1: data = Fetch16BitData(chan, pos); break;
				case 2: data = FetchADPCMData(chan, pos); break;
				case 3: data = FetchPSGData  (chan, pos); break;
				default: break;
			}
			chan->pcm16bOffs++;
			chan->pcm16b[SPUCHAN_PCM16B_AT(chan->pcm16bOffs)] = data;

			pos++;
			if (FORMAT != 3)
			{
				// If channel stops, fill the rest of the buffer with 0
				if(!TestForLoop<FORMAT>(SPU, chan, pos))
				{
					// Note that if we aren't mixing to both channels,
					// the buffer was already cleared earlier.
					if(CHANNELS == ((1<<0)|(1<<1))) memset(chanbuf, 0, length*sizeof(s16)*2);
					return;
				}
			}
		}

		s32 sample;
		if(CHANNELS) sample = Interpolate<INTERPOLATE_MODE>(chan->pcm16b, chan->pcm16bOffs, chan->sampcntFrac);
		if(CHANNELS & (1<<0)) chanbuf[0] = (s16)(sample * vol_left  >> 16);
		if(CHANNELS & (1<<1)) chanbuf[1] = (s16)(sample * vol_right >> 16);
		chanbuf += 2;
	} while(--length);
	chan->sampcntInt = pos;
}

template<int FORMAT, SPUInterpolationMode INTERPOLATE_MODE> 
FORCEINLINE static void ___SPU_GenerateChanData(bool actuallyMix, SPU_struct* const SPU, channel_struct* const chan, s16 *chanbuf, int length)
{
	if(!actuallyMix)          ____SPU_GenerateChanData<(0<<0)|(0<<1),FORMAT,INTERPOLATE_MODE>(SPU, chan, chanbuf, length);
	else if(chan->pan == 0)   ____SPU_GenerateChanData<(1<<0)|(0<<1),FORMAT,INTERPOLATE_MODE>(SPU, chan, chanbuf, length);
	else if(chan->pan == 127) ____SPU_GenerateChanData<(0<<0)|(1<<1),FORMAT,INTERPOLATE_MODE>(SPU, chan, chanbuf, length);
	else                      ____SPU_GenerateChanData<(1<<0)|(1<<1),FORMAT,INTERPOLATE_MODE>(SPU, chan, chanbuf, length);
}

template<SPUInterpolationMode INTERPOLATE_MODE> 
FORCEINLINE static void __SPU_GenerateChanData(bool actuallyMix, SPU_struct* const SPU, channel_struct* const chan, s16 *chanbuf, int length)
{
	// NOTE: PSG doesn't use interpolation, or it would try to
	// interpolate between the raw sample points (very bad)
	switch(chan->format)
	{
		case 0: ___SPU_GenerateChanData<0,INTERPOLATE_MODE>(actuallyMix, SPU, chan, chanbuf, length); break;
		case 1: ___SPU_GenerateChanData<1,INTERPOLATE_MODE>(actuallyMix, SPU, chan, chanbuf, length); break;
		case 2: ___SPU_GenerateChanData<2,INTERPOLATE_MODE>(actuallyMix, SPU, chan, chanbuf, length); break;
		case 3: ___SPU_GenerateChanData<3,SPUInterpolation_None>(actuallyMix, SPU, chan, chanbuf, length); break;
		default: assert(false);
	}
}

FORCEINLINE static void _SPU_GenerateChanData(bool actuallyMix, SPU_struct* const SPU, channel_struct* const chan, s16 *chanbuf, int length)
{
	switch(CommonSettings.spuInterpolationMode)
	{
	case SPUInterpolation_None:       __SPU_GenerateChanData<SPUInterpolation_None>(actuallyMix, SPU, chan, chanbuf, length); break;
	case SPUInterpolation_Linear:     __SPU_GenerateChanData<SPUInterpolation_Linear>(actuallyMix, SPU, chan, chanbuf, length); break;
	case SPUInterpolation_Cosine:     __SPU_GenerateChanData<SPUInterpolation_Cosine>(actuallyMix, SPU, chan, chanbuf, length); break;
	case SPUInterpolation_CatmullRom: __SPU_GenerateChanData<SPUInterpolation_CatmullRom>(actuallyMix, SPU, chan, chanbuf, length); break;
	default: assert(false);
	}
}

template<int CAP_BITS>
FORCEINLINE static void _SPU_WriteCapture(SPU_struct::REGS::CAP& cap, const channel_struct& srcChan, const s16 *srcBuf, int length)
{
	u32 capLen_shifted = cap.len * (32 / CAP_BITS);
	SPU_struct::REGS::CAP::Runtime& runtime = cap.runtime;
	do
	{
		u32 nSamplesToProcess = srcChan.sampincInt + AddAndReturnCarry(&runtime.sampcntFrac, srcChan.sampincFrac);
		while(nSamplesToProcess--)
		{
			s16 *data = &runtime.pcm16b[SPUCAPTURE_PCM16B_AT(runtime.pcm16bOffs)];

			if(runtime.sampcntInt >= 0)
			{
				if (CAP_BITS == 8)
				{
					_MMU_write08<ARMCPU_ARM7,MMU_AT_DMA>(runtime.curdad, (u8)(*data >> 8));
					runtime.curdad += 1;
				}
				else
				{
					_MMU_write16<ARMCPU_ARM7,MMU_AT_DMA>(runtime.curdad, (u16)(*data));
					runtime.curdad += 2;
				}

				if(runtime.curdad >= runtime.maxdad)
				{
					if(cap.oneshot)
					{
						cap.active = runtime.running = 0;
						return;
					}
					runtime.curdad = cap.dad;
					runtime.sampcntInt -= capLen_shifted;
				}
			}

			// srcBuf[] stores two samples per time unit
			// Either {Ch0[+Ch1],Ch2[+Ch3]}, or {LMix,RMix}
			*data = *srcBuf; srcBuf += 2;
			runtime.pcm16bOffs++;
			runtime.sampcntInt++;
		}
	} while(--length);
}

//ENTER
static void SPU_MixAudio(bool actuallyMix, SPU_struct *SPU, int length)
{
	if(actuallyMix) memset(SPU->outbuf, 0, length*sizeof(s16)*2);

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
	//    If not muted or bypassed:
	//      Mix chanbuf[] into mixbuf[]
	//    If capturing from channels:
	//      Copy/mix chanbuf[] into capbuf[]
	//    If not playing from mixer:
	//      Copy/mix chanbuf[] into outbuf[]
	//  If capturing from channels:
	//    Output capbuf[] to capture units
	//  If playing from mixer:
	//    Output mixbuf[] to outbuf[]
	//  If capturing from mixer:
	//    Output mixbuf[] to capture units

	// PONDER: Can we put mixbuf[],capbuf[],chanbuf[] on the stack?
	s32 *mixbuf  = SPU->mixbuf;
	s16 *outbuf  = SPU->outbuf;
	s16 *capbuf  = SPU->capbuf;
	s16 *chanbuf = SPU->chanbuf;
	s32 masterVol = SPU->regs.mastervol; masterVol += (masterVol == 127);

	//we used to bail out if speakers were disabled.
	//this is technically wrong. sound may still be captured, or something.
	//in all likelihood, any game doing this probably master disabled the SPU also
	//so, optimization of this case is probably not necessary.
	//later, we'll just silence the output
	bool speakersOn = T1ReadWord(MMU.ARM7_REG, 0x304) & 0x01;

	// Translate the capture sources into something friendlier
	enum
	{
		CAPSRC_NONE,
		CAPSRC_MIXER,
		CAPSRC_CHAN,
		CAPSRC_MIXED,
	};
	u8 cap0Src = CAPSRC_NONE;
	u8 cap1Src = CAPSRC_NONE;
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

	// Translate the mixer and capture states into a bitarray
	// This should improve the code generation so that
	// it doesn't have to reference a lot of memory and
	// can instead just bitwise-test as needed.
	//  -bypassMixer controls whether chanbuf[] should NOT be added to mixbuf[]
	//  -captureFlags controls the following:
	//    Chan == 0: Store chanbuf[] in cap[0]'s capbuf[] (Src == Chan0 or Chan0+Chan1)
	//    Chan == 1: Add chanbuf[] to cap[0]'s capbuf[]   (Src == Chan0+Chan1)
	//    Chan == 2: Store chanbuf[] in cap[1]'s capbuf   (Src == Chan2 or Chan2+Chan3)
	//    Chan == 3: Add chanbuf[] to cap[1]'s capbuf[]   (Src == Chan2+Chan3)
	u8 bypassMixer  = 0;
	u8 captureFlags = 0;
	u8 outbufSetL   = 0;
	u8 outbufSetR   = 0;
	u8 outbufMixL   = 0;
	u8 outbufMixR   = 0;
	if(SPU->regs.ctl_ch1bypass) bypassMixer |= (1 << 1);
	if(SPU->regs.ctl_ch3bypass) bypassMixer |= (1 << 3);
	if(cap0Src == CAPSRC_CHAN || cap0Src == CAPSRC_MIXED) captureFlags |= (1 << 0);
	if(                          cap0Src == CAPSRC_MIXED) captureFlags |= (1 << 1);
	if(cap1Src == CAPSRC_CHAN || cap1Src == CAPSRC_MIXED) captureFlags |= (1 << 2);
	if(                          cap1Src == CAPSRC_MIXED) captureFlags |= (1 << 3);
	switch(SPU->regs.ctl_left)
	{
		case SPU_struct::REGS::LOM_CH1:
			outbufSetL = 1 << 1;
			break;
		case SPU_struct::REGS::LOM_CH3:
			outbufSetL = 1 << 3;
			break;
		case SPU_struct::REGS::LOM_CH1_PLUS_CH3:
			outbufSetL = 1 << 1;
			outbufMixL = 1 << 3;
			break;
	}
	switch(SPU->regs.ctl_right)
	{
		case SPU_struct::REGS::ROM_CH1:
			outbufSetR = 1 << 1;
			break;
		case SPU_struct::REGS::ROM_CH3:
			outbufSetR = 1 << 3;
			break;
		case SPU_struct::REGS::ROM_CH1_PLUS_CH3:
			outbufSetR = 1 << 1;
			outbufMixR = 1 << 3;
			break;
	}

	// We can only process at most SPUCAPTURE_FIFO_SIZE samples
	// per mixing batch, in case the capture buffers wrap around.
	// Technically, we could actually check if this is needed at
	// all, but this should work well enough as is.
	while(length)
	{
		int thisLength = MIN(length, SPUCAPTURE_FIFO_SIZE);
		length -= thisLength;

		// Giving memset a constant size should hopefully make things better
		// ^ Nope. Unless we are in Dual SPU mode, we mix in such small batches
		// that it's actually faster to give thisLength to memset().
		// capbuf[] doesn't need to be cleared, as it will be set directly.
		memset(mixbuf, 0, thisLength*sizeof(s32)*2);
		//memset(capbuf, 0, thisLength*sizeof(s16)*2);

		// Process each channel in turn
		for (int i=0; i < 16; i++)
		{
			channel_struct *chan = &SPU->channels[i];
			if (chan->status != CHANSTAT_PLAY) continue;
			u8 chanBit = (u8)(1 << i);
		
			// Generate data into chanbuf[]
			// NOTE: If actuallyMix==false, no data is generated
			// and the channels are simply updated, nothing more.
			_SPU_GenerateChanData(actuallyMix, SPU, chan, chanbuf, thisLength);
			if(!actuallyMix) continue;

			// Mix into mixbuf[] only when not muted or bypassed
			if(!CommonSettings.spu_muteChannels[i] && (bypassMixer & chanBit) == 0)
				for(int n=0; n<thisLength*2; n++) mixbuf[n] += chanbuf[n];

			// Generate outputs for channel capture
			// Yes, we have to undo the panning here, but that's fine.
			// Incidentally, this emulates the ch(a)+ch(b) overflow bug
			if((captureFlags & chanBit) != 0)
			{
				switch(i)
				{
				case 0:
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+0]  = chanbuf[n*2+0] + chanbuf[n*2+1];
					break;
				case 1:
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+0] += chanbuf[n*2+0] + chanbuf[n*2+1];
					break;
				case 2:
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+1]  = chanbuf[n*2+0] + chanbuf[n*2+1];
					break;
				case 3:
					for(int n=0; n < thisLength; n++)
						capbuf[n*2+1] += chanbuf[n*2+0] + chanbuf[n*2+1];
					break;
				}
			}

			// Set outbuf[] from chanbuf[] when L/R source is not the mixer
			// Note that Ch1+Ch3 mode clips as intended; only capture has overflow bugs
			if(!speakersOn) continue;
			if((outbufSetL & chanBit) != 0)
			{
				for(int n=0; n < thisLength; n++)
					outbuf[n*2+0] = chanbuf[n*2+0] * masterVol >> 7;
			}
			if((outbufSetR & chanBit) != 0)
			{
				for(int n=0; n < thisLength; n++)
					outbuf[n*2+1] = chanbuf[n*2+1] * masterVol >> 7;
			}
			if((outbufMixL & chanBit) != 0)
			{
				for(int n=0; n < thisLength; n++)
					outbuf[n*2+0] = MinMax(outbuf[n*2+0] + (chanbuf[n*2+0] * masterVol >> 7), -0x8000, +0x7FFF);
			}
			if((outbufMixR & chanBit) != 0)
			{
				for(int n=0; n < thisLength; n++)
					outbuf[n*2+1] = MinMax(outbuf[n*2+1] + (chanbuf[n*2+1] * masterVol >> 7), -0x8000, +0x7FFF);
			}
		}

		// Generate final channel capture
		if(cap0Src == CAPSRC_CHAN || cap0Src == CAPSRC_MIXED)
		{
			if(SPU->regs.cap[0].bits8)
				_SPU_WriteCapture<8> (SPU->regs.cap[0], SPU->channels[1], capbuf, thisLength);
			else
				_SPU_WriteCapture<16>(SPU->regs.cap[0], SPU->channels[1], capbuf, thisLength);
		}
		if(cap1Src == CAPSRC_CHAN || cap1Src == CAPSRC_MIXED)
		{
			if(SPU->regs.cap[1].bits8)
				_SPU_WriteCapture<8> (SPU->regs.cap[1], SPU->channels[3], capbuf+1, thisLength);
			else
				_SPU_WriteCapture<16>(SPU->regs.cap[1], SPU->channels[3], capbuf+1, thisLength);
		}

		// Generate mixer output to outbuf[]
		if(actuallyMix && speakersOn)
		{
			if(SPU->regs.ctl_left == SPU_struct::REGS::LOM_LEFT_MIXER)
			{
				for(int i=0; i < thisLength; i++)
					outbuf[i*2+0] = MinMax(mixbuf[i*2+0] * masterVol >> 7, -0x8000, +0x7FFF);
			}
			if(SPU->regs.ctl_right == SPU_struct::REGS::ROM_RIGHT_MIXER)
			{
				for(int i=0; i < thisLength; i++)
					outbuf[i*2+1] = MinMax(mixbuf[i*2+1] * masterVol >> 7, -0x8000, +0x7FFF);
			}
		}

		// Generate final mixer capture
		if(cap0Src == CAPSRC_MIXER)
		{
			for(int i=0; i < thisLength; i++)
				capbuf[i*2+0] = MinMax(mixbuf[i*2+0], -0x8000, +0x7FFF);
			if(SPU->regs.cap[0].bits8)
				_SPU_WriteCapture<8> (SPU->regs.cap[0], SPU->channels[1], capbuf, thisLength);
			else
				_SPU_WriteCapture<16>(SPU->regs.cap[0], SPU->channels[1], capbuf, thisLength);
		}
		if(cap1Src == CAPSRC_MIXER)
		{
			for(int i=0; i < thisLength; i++)
				capbuf[i*2+1] = MinMax(mixbuf[i*2+1], -0x8000, +0x7FFF);
			if(SPU->regs.cap[1].bits8)
				_SPU_WriteCapture<8> (SPU->regs.cap[1], SPU->channels[3], capbuf+1, thisLength);
			else
				_SPU_WriteCapture<16>(SPU->regs.cap[1], SPU->channels[3], capbuf+1, thisLength);
		}

		// Capture can end in one-shot mode, so update state when this happens
		if(!SPU->regs.cap[0].runtime.running) cap0Src = CAPSRC_NONE, captureFlags &= ~(1<<0 | 1<<1);
		if(!SPU->regs.cap[1].runtime.running) cap1Src = CAPSRC_NONE, captureFlags &= ~(1<<2 | 1<<3);

		// Advance buffer
		outbuf += thisLength*2;
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
	
	spu_core_samples = (samples_per_core_update >> 32) + AddAndReturnCarry(&_spucore_samplesFrac, (u32)(samples_per_core_update));
	
	// We don't need to mix audio for Dual Synch/Asynch mode since we do this
	// later in SPU_Emulate_user(). Disable mixing here to speed up processing.
	// However, recording still needs to mix the audio, so make sure we're also
	// not recording before we disable mixing.
	if ( (_currentSynchMode == ESynchMode_DualSynchAsynch || soundProcessor == NULL || soundProcessor == &SNDDummy) &&
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
	os.write_32LE(8);

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
		os.write_u8(chan.pcm16bOffs);
		os.write_32LE(chan.addr);
		os.write_16LE(chan.timer);
		os.write_16LE(chan.loopstart);
		os.write_32LE(chan.length);
		os.write_32LE(chan.sampcntFrac);
		os.write_32LE(chan.sampcntInt);
		os.write_32LE(chan.sampincFrac);
		os.write_32LE(chan.sampincInt);
		for (int i = 0; i < SPUINTERPOLATION_TAPS; i++) os.write_16LE(chan.pcm16b[i]);
		os.write_32LE(chan.index);
		os.write_16LE(chan.x);
		os.write_u8(chan.keyon);
	}

	os.write_32LE(_spucore_samplesFrac);

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
		os.write_32LE(spu->regs.cap[i].runtime.sampcntFrac);
		os.write_32LE(spu->regs.cap[i].runtime.sampcntInt);
	}

	for (int i = 0; i < 2; i++)
	{
		os.write_u8(spu->regs.cap[i].runtime.pcm16bOffs);
		for (int n = 0; n < SPUCAPTURE_FIFO_SIZE; n++)
			os.write_16LE(spu->regs.cap[i].runtime.pcm16b[n]);
	}
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
		if (version >= 7) is.read_u8(chan.pcm16bOffs); else chan.pcm16bOffs = 0;
		is.read_32LE(chan.addr);
		is.read_16LE(chan.timer);
		is.read_16LE(chan.loopstart);
		is.read_32LE(chan.length);
		chan.totlength = chan.length + chan.loopstart;
		chan.totlength_shifted = chan.totlength << format_shift[chan.format];
		if(version >= 7) {
			is.read_32LE(chan.sampcntFrac);
			is.read_32LE(chan.sampcntInt);
			is.read_32LE(chan.sampincFrac);
			is.read_32LE(chan.sampincInt);
		}
		else if (version >= 2)
		{
			double temp;
			s64 temp2;
			is.read_doubleLE(temp); temp2 = (s64)(temp * (1ll << 32));
			chan.sampcntFrac = (u32)temp2;
			chan.sampcntInt  = (s32)(temp2 >> 32);
			is.read_doubleLE(temp); temp2 = (u64)(temp * (1ull << 32)); // Intentionally unsigned
			chan.sampincFrac = (u32)temp2;
			chan.sampincInt  = (u32)(temp2 >> 32);
		}
		else
		{
			// FIXME
			// What even is supposed to be happening here?
			// sampcnt and sampinc were double type before
			// I even made any changes, so this is broken.
			chan.sampcntFrac = 0;
			is.read_32LE(chan.sampcntInt);
			chan.sampincFrac = 0;
			is.read_32LE(chan.sampincInt);
		}
		if (version >= 7) {
			for (int i = 0; i < SPUINTERPOLATION_TAPS; i++) is.read_16LE(chan.pcm16b[i]);
		}
		else
		{
			is.fseek(4, SEEK_CUR);        // chan.lastsampcnt (LE32)
			is.read_16LE(chan.pcm16b[0]); // chan.pcm16b
			is.fseek(2, SEEK_CUR);        // chan.pcm16b_last
		}
		is.read_32LE(chan.index);
		is.read_16LE(chan.x);
		if (version < 7) is.fseek(2, SEEK_CUR); // chan.psgnoise_last (LE16)

		if (version >= 4)
			is.read_u8(chan.keyon);

		//hopefully trigger a recovery of the adpcm looping system
		chan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;
	}

	if (version >= 8)
	{
		is.read_32LE(_spucore_samplesFrac);
	}
	else if (version >= 2)
	{
		double temp; is.read_doubleLE(temp);
		_spucore_samplesFrac = (u32)(temp * (1ull<<32));
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
			if (version >= 7) {
				is.read_32LE(spu->regs.cap[i].runtime.sampcntFrac);
				is.read_32LE(spu->regs.cap[i].runtime.sampcntInt);
			}
			else
			{
				double temp;
				u64 temp2;
				is.read_doubleLE(temp); temp2 = (u64)(temp * (1ull << 32));
				spu->regs.cap[i].runtime.sampcntFrac = (u32)temp2;
				spu->regs.cap[i].runtime.sampcntInt  = (s32)(temp2 >> 32);
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

	if (version >= 8)
		for (int i=0;i<2;i++)
		{
			is.read_u8(spu->regs.cap[i].runtime.pcm16bOffs);
			for (int n = 0; n < SPUCAPTURE_FIFO_SIZE; n++)
				is.read_16LE(spu->regs.cap[i].runtime.pcm16b[n]);
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
				spu->regs.cap[i].runtime.pcm16b[n] = fifo.dequeue();
		}
	else
		for (int i=0;i<2;i++)
		{
			for (int n = 0; n < 16; n++)
				spu->regs.cap[i].runtime.pcm16b[n] = 0;
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
