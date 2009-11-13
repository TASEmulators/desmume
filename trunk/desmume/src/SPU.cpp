/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com
    Copyright (C) 2006 Theo Berkau
    Copyright (C) 2008-2009 DeSmuME team

    Ideas borrowed from Stephane Dallongeville's SCSP core

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
#include "MMU.h"
#include "SPU.h"
#include "mem.h"
#include "readwrite.h"
#include "armcpu.h"
#include "NDSSystem.h"
#include "matrix.h"

#include "metaspu/metaspu.h"

#define K_ADPCM_LOOPING_RECOVERY_INDEX 99999

//#undef FORCEINLINE
//#define FORCEINLINE



//static ISynchronizingAudioBuffer* synchronizer = metaspu_construct(ESynchMethod_Z);
static ISynchronizingAudioBuffer* synchronizer = metaspu_construct(ESynchMethod_N);

SPU_struct *SPU_core = 0;
SPU_struct *SPU_user = 0;
int SPU_currentCoreNum = SNDCORE_DUMMY;
static int volume = 100;


static ESynchMode synchmode = ESynchMode_DualSynchAsynch;
static ESynchMethod synchmethod = ESynchMethod_N;

static SoundInterface_struct *SNDCore=NULL;
extern SoundInterface_struct *SNDCoreList[];

//const int shift = (FORMAT == 0 ? 2 : 1);
static const int format_shift[] = { 2, 1, 3, 0 };

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

static const double ARM7_CLOCK = 33513982;

static const double samples_per_hline = (DESMUME_SAMPLE_RATE / 59.8261f) / 263.0f;

static double samples = 0;

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

int SPU_ChangeSoundCore(int coreid, int buffersize)
{
	int i;

	delete SPU_user; SPU_user = 0;

	// Make sure the old core is freed
	if (SNDCore)
		SNDCore->DeInit();

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
			SNDCore = SNDCoreList[i];
			break;
		}
	}

	//If the user picked the dummy core, disable the user spu
	if(SNDCore == &SNDDummy)
		return 0;

	//If the core wasnt found in the list for some reason, disable the user spu
	if (SNDCore == NULL)
		return -1;

	// Since it failed, instead of it being fatal, disable the user spu
	if (SNDCore->Init(buffersize * 2) == -1)
	{
		SNDCore = 0;
		return -1;
	}

	//enable the user spu
	SPU_user = new SPU_struct(buffersize);

	SNDCore->SetVolume(volume);

	return 0;
}

SoundInterface_struct *SPU_SoundCore()
{
	return SNDCore;
}

//static double cos_lut[256];
int SPU_Init(int coreid, int buffersize)
{
	int i, j;

	//for some reason we dont use the cos lut anymore... did someone decide it was slow?
	//for(int i=0;i<256;i++)
	//	cos_lut[i] = cos(i/256.0*M_PI);

	SPU_core = new SPU_struct((int)ceil(samples_per_hline));
	SPU_Reset();

	//create adpcm decode accelerator lookups
	for(i = 0; i < 16; i++)
	{
		for(j = 0; j < 89; j++)
		{
			precalcdifftbl[j][i] = (((i & 0x7) * 2 + 1) * adpcmtbl[j] / 8);
			if(i & 0x8) precalcdifftbl[j][i] = -precalcdifftbl[j][i];
		}
	}
	for(i = 0; i < 8; i++)
	{
		for(j = 0; j < 89; j++)
		{
			precalcindextbl[j][i] = MinMax((j + indextbl[i]), 0, 88);
		}
	}

	return SPU_ChangeSoundCore(coreid, buffersize);
}

void SPU_Pause(int pause)
{
	if (SNDCore == NULL) return;

	if(pause)
		SNDCore->MuteAudio();
	else
		SNDCore->UnMuteAudio();
}

void SPU_SetSynchMode(int mode, int method)
{
	synchmode = (ESynchMode)mode;
	if(synchmethod != (ESynchMethod)method)
	{
		synchmethod = (ESynchMethod)method;
		delete synchronizer;
		//grr does this need to be locked? spu might need a lock method
		  // or maybe not, maybe the platform-specific code that calls this function can deal with it.
		synchronizer = metaspu_construct(synchmethod);
	}
}

void SPU_ClearOutputBuffer()
{
	if(SNDCore && SNDCore->ClearBuffer)
		SNDCore->ClearBuffer();
}

void SPU_SetVolume(int volume)
{
	::volume = volume;
	if (SNDCore)
		SNDCore->SetVolume(volume);
}


void SPU_Reset(void)
{
	int i;

	SPU_core->reset();
	if(SPU_user) SPU_user->reset();

	if(SNDCore && SPU_user) {
		SNDCore->DeInit();
		SNDCore->Init(SPU_user->bufsize*2);
		SNDCore->SetVolume(volume);
		//todo - check success?
	}

	// Reset Registers
	for (i = 0x400; i < 0x51D; i++)
		T1WriteByte(MMU.ARM7_REG, i, 0);

	samples = 0;
}

//------------------------------------------

void SPU_struct::reset()
{
	memset(sndbuf,0,bufsize*2*4);
	memset(outbuf,0,bufsize*2*2);

	memset((void *)channels, 0, sizeof(channel_struct) * 16);

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
	if(SNDCore)
		SNDCore->DeInit();
	SNDCore = 0;

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

void SPU_struct::KeyOn(int channel)
{
	channel_struct &thischan = channels[channel];

	adjust_channel_timer(&thischan);

	//LOG("Channel %d key on: vol = %d, datashift = %d, hold = %d, pan = %d, waveduty = %d, repeat = %d, format = %d, source address = %07X,"
	//		"timer = %04X, loop start = %04X, length = %06X, MMU.ARM7_REG[0x501] = %02X\n", channel, chan->vol, chan->datashift, chan->hold, 
	//		chan->pan, chan->waveduty, chan->repeat, chan->format, chan->addr, chan->timer, chan->loopstart, chan->length, T1ReadByte(MMU.ARM7_REG, 0x501));

	switch(thischan.format)
	{
	case 0: // 8-bit
		thischan.buf8 = (s8*)&MMU.MMU_MEM[1][(thischan.addr>>20)&0xFF][(thischan.addr & MMU.MMU_MASK[1][(thischan.addr >> 20) & 0xFF])];
	//	thischan.loopstart = thischan.loopstart << 2;
	//	thischan.length = (thischan.length << 2) + thischan.loopstart;
		thischan.sampcnt = -3;
		break;
	case 1: // 16-bit
		thischan.buf16 = (s16 *)&MMU.MMU_MEM[1][(thischan.addr>>20)&0xFF][(thischan.addr & MMU.MMU_MASK[1][(thischan.addr >> 20) & 0xFF])];
	//	thischan.loopstart = thischan.loopstart << 1;
	//	thischan.length = (thischan.length << 1) + thischan.loopstart;
		thischan.sampcnt = -3;
		break;
	case 2: // ADPCM
		{
			thischan.buf8 = (s8*)&MMU.MMU_MEM[1][(thischan.addr>>20)&0xFF][(thischan.addr & MMU.MMU_MASK[1][(thischan.addr >> 20) & 0xFF])];
			thischan.pcm16b = (s16)((thischan.buf8[1] << 8) | thischan.buf8[0]);
			thischan.pcm16b_last = thischan.pcm16b;
			thischan.index = thischan.buf8[2] & 0x7F;
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

	if(thischan.format != 3)
	{
		if(thischan.double_totlength_shifted == 0)
		{
			printf("INFO: Stopping channel %d due to zero length\n",channel);
			thischan.status = CHANSTAT_STOPPED;
		}
	}
	
	thischan.double_totlength_shifted = (double)(thischan.totlength << format_shift[thischan.format]);
}

//////////////////////////////////////////////////////////////////////////////

void SPU_struct::WriteByte(u32 addr, u8 val)
{
	channel_struct &thischan=channels[(addr >> 4) & 0xF];
	switch(addr & 0xF) {
		case 0x0:
			thischan.vol = val & 0x7F;
			break;
		case 0x1: {
			thischan.datashift = val & 0x3;
			if (thischan.datashift == 3)
				thischan.datashift = 4;
			thischan.hold = (val >> 7) & 0x1;
			break;
		}
		case 0x2:
			thischan.pan = val & 0x7F;
			break;
		case 0x3: {
			thischan.waveduty = val & 0x7;
			thischan.repeat = (val >> 3) & 0x3;
			thischan.format = (val >> 5) & 0x3;
			if((!thischan.status) && BIT7(val))
				KeyOn((addr >> 4) & 0xF);
			thischan.status = BIT7(val);
			break;
		}
	}

}

void SPU_WriteByte(u32 addr, u8 val)
{
	addr &= 0xFFF;

	if (addr < 0x500)
	{
		SPU_core->WriteByte(addr,val);
		if(SPU_user) SPU_user->WriteByte(addr,val);
	}

	T1WriteByte(MMU.ARM7_REG, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void SPU_struct::WriteWord(u32 addr, u16 val)
{
	channel_struct &thischan=channels[(addr >> 4) & 0xF];
	switch(addr & 0xF)
	{
	case 0x0:
		thischan.vol = val & 0x7F;
		thischan.datashift = (val >> 8) & 0x3;
		if (thischan.datashift == 3)
			thischan.datashift = 4;
		thischan.hold = (val >> 15) & 0x1;
		break;
	case 0x2:
		thischan.pan = val & 0x7F;
		thischan.waveduty = (val >> 8) & 0x7;
		thischan.repeat = (val >> 11) & 0x3;
		thischan.format = (val >> 13) & 0x3;
		if((!thischan.status) && BIT15(val))
			KeyOn((addr >> 4) & 0xF);
		thischan.status = BIT15(val);
		break;
	case 0x8:
		thischan.timer = val & 0xFFFF;
		adjust_channel_timer(&thischan);
		break;
	case 0xA:
		thischan.loopstart = val;
		thischan.totlength = thischan.length + thischan.loopstart;
		thischan.double_totlength_shifted = (double)(thischan.totlength << format_shift[thischan.format]);
		break;
	case 0xC:
		WriteLong(addr,((u32)T1ReadWord(MMU.ARM7_REG, addr+2) << 16) | val);
		break;
	case 0xE:
		WriteLong(addr,((u32)T1ReadWord(MMU.ARM7_REG, addr-2)) | ((u32)val<<16));
		break;
	}
}

void SPU_WriteWord(u32 addr, u16 val)
{
	addr &= 0xFFF;

	if (addr < 0x500)
	{
		SPU_core->WriteWord(addr,val);
		if(SPU_user) SPU_user->WriteWord(addr,val);
	}

	T1WriteWord(MMU.ARM7_REG, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

void SPU_struct::WriteLong(u32 addr, u32 val)
{
	channel_struct &thischan=channels[(addr >> 4) & 0xF];
	switch(addr & 0xF)
	{
	case 0x0:
		thischan.vol = val & 0x7F;
		thischan.datashift = (val >> 8) & 0x3;
		if (thischan.datashift == 3)
			thischan.datashift = 4;
		thischan.hold = (val >> 15) & 0x1;
		thischan.pan = (val >> 16) & 0x7F;
		thischan.waveduty = (val >> 24) & 0x7;
		thischan.repeat = (val >> 27) & 0x3;
		thischan.format = (val >> 29) & 0x3;
		if((!thischan.status) && BIT31(val))
			KeyOn((addr >> 4) & 0xF);
		thischan.status = BIT31(val);
		break;
	case 0x4:
		thischan.addr = val & 0x7FFFFFF;
		break;
	case 0x8:
		thischan.timer = val & 0xFFFF;
		thischan.loopstart = val >> 16;
		adjust_channel_timer(&thischan);
		break;
	case 0xC:
		thischan.length = val & 0x3FFFFF;
		thischan.totlength = thischan.length + thischan.loopstart;
		thischan.double_totlength_shifted = (double)(thischan.totlength << format_shift[thischan.format]);
		break;
	}
}

void SPU_WriteLong(u32 addr, u32 val)
{
	addr &= 0xFFF;

	if (addr < 0x500)
	{
		SPU_core->WriteLong(addr,val);
		if(SPU_user) SPU_user->WriteLong(addr,val);
	}

	T1WriteLong(MMU.ARM7_REG, addr, val);
}

//////////////////////////////////////////////////////////////////////////////

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE s32 Interpolate(s32 a, s32 b, double _ratio)
{
	float ratio = (float)_ratio;
	if(INTERPOLATE_MODE == SPUInterpolation_Cosine)
	{
		//why did we change it away from the lookup table? somebody should research that
		ratio = ratio - (int)ratio;
		double ratio2 = ((1.0 - cos(ratio * M_PI)) * 0.5);
		//double ratio2 = (1.0f - cos_lut[((int)(ratio*256.0))&0xFF]) / 2.0f;
		return (s32)(((1-ratio2)*a) + (ratio2*b));
	}
	else
	{
		//linear interpolation
		ratio = ratio - sputrunc(ratio);
		return s32floor((1-ratio)*a + ratio*b);
	}
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
		s32 a = (s32)(chan->buf8[loc] << 8);
		if(loc < (chan->totlength << 2) - 1) {
			s32 b = (s32)(chan->buf8[loc + 1] << 8);
			a = Interpolate<INTERPOLATE_MODE>(a, b, chan->sampcnt);
		}
		*data = a;
	}
	else
		*data = (s32)chan->buf8[loc] << 8;
}

template<SPUInterpolationMode INTERPOLATE_MODE> static FORCEINLINE void Fetch16BitData(const channel_struct * const chan, s32 *data)
{
	if (chan->sampcnt < 0)
	{
		*data = 0;
		return;
	}

	if(INTERPOLATE_MODE != SPUInterpolation_None)
	{
		u32 loc = sputrunc(chan->sampcnt);
		s32 a = (s32)chan->buf16[loc], b;
		if(loc < (chan->totlength << 1) - 1)
		{
			b = (s32)chan->buf16[loc + 1];
			a = Interpolate<INTERPOLATE_MODE>(a, b, chan->sampcnt);
		}
		*data = a;
	}
	else
		*data = (s32)chan->buf16[sputrunc(chan->sampcnt)];
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
	    	const u32 data4bit = (((u32)chan->buf8[i >> 1]) >> shift);

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
	data = spumuldiv7(data, chan->vol) >> chan->datashift;
	SPU->sndbuf[SPU->bufpos<<1] += data;
}

static FORCEINLINE void MixR(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	data = spumuldiv7(data, chan->vol) >> chan->datashift;
	SPU->sndbuf[(SPU->bufpos<<1)+1] += data;
}

static FORCEINLINE void MixLR(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	data = spumuldiv7(data, chan->vol) >> chan->datashift;
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
			chan->status = CHANSTAT_STOPPED;

			if(SPU == SPU_core)
				MMU.ARM7_REG[0x403 + (((chan-SPU->channels) ) * 0x10)] &= 0x7F;
			SPU->bufpos = SPU->buflength;
		}
	}
}

static FORCEINLINE void TestForLoop2(SPU_struct *SPU, channel_struct *chan)
{
	chan->sampcnt += chan->sampinc;

	if (chan->sampcnt > chan->double_totlength_shifted)
	{
		// Do we loop? Or are we done?
		if (chan->repeat == 1)
		{
			while (chan->sampcnt > chan->double_totlength_shifted)
				chan->sampcnt -= chan->double_totlength_shifted - (double)(chan->loopstart << 3);

			if(chan->loop_index == K_ADPCM_LOOPING_RECOVERY_INDEX)
			{
				chan->pcm16b = (s16)((chan->buf8[1] << 8) | chan->buf8[0]);
				chan->index = chan->buf8[2] & 0x7F;
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
			if(SPU == SPU_core)
				MMU.ARM7_REG[0x403 + (((chan-SPU->channels) ) * 0x10)] &= 0x7F;
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
	}
}

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
			}
			SPU_Mix<CHANNELS>(SPU, chan, data);
		}

		switch(FORMAT) {
			case 0: case 1: TestForLoop<FORMAT>(SPU, chan); break;
			case 2: TestForLoop2(SPU, chan); break;
			case 3: chan->sampcnt += chan->sampinc; break;
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

static void SPU_MixAudio(bool actuallyMix, SPU_struct *SPU, int length)
{
	u8 vol;

	if(actuallyMix)
	{
		memset(SPU->sndbuf, 0, length*4*2);
		memset(SPU->outbuf, 0, length*2*2);
	}
	
	// If the sound speakers are disabled, don't output audio
	if(!(T1ReadWord(MMU.ARM7_REG, 0x304) & 0x01))
		return;

	// If Master Enable isn't set, don't output audio
	if (!(T1ReadByte(MMU.ARM7_REG, 0x501) & 0x80))
		return;

	vol = T1ReadByte(MMU.ARM7_REG, 0x500) & 0x7F;

	for(int i=0;i<16;i++)
	{
		channel_struct *chan = &SPU->channels[i];
	
		if (chan->status != CHANSTAT_PLAY)
			continue;

		SPU->bufpos = 0;
		SPU->buflength = length;

		// Mix audio
		_SPU_ChanUpdate(!CommonSettings.spu_muteChannels[i] && actuallyMix, SPU, chan);
	}

	// convert from 32-bit->16-bit
	if(actuallyMix)
		for (int i = 0; i < length*2; i++)
		{
			// Apply Master Volume
			SPU->sndbuf[i] = spumuldiv7(SPU->sndbuf[i], vol);

			if (SPU->sndbuf[i] > 0x7FFF)
				SPU->outbuf[i] = 0x7FFF;
			else if (SPU->sndbuf[i] < -0x8000)
				SPU->outbuf[i] = -0x8000;
			else
				SPU->outbuf[i] = (s16)SPU->sndbuf[i];
		}
}

//////////////////////////////////////////////////////////////////////////////


//emulates one hline of the cpu core.
//this will produce a variable number of samples, calculated to keep a 44100hz output
//in sync with the emulator framerate
int spu_core_samples = 0;
void SPU_Emulate_core()
{
	samples += samples_per_hline;
	spu_core_samples = (int)(samples);
	samples -= spu_core_samples;

	bool synchronize = (synchmode == ESynchMode_Synchronous);
	bool mix = driver->AVI_IsRecording() || driver->WAV_IsRecording() || synchronize;

	SPU_MixAudio(mix,SPU_core,spu_core_samples);
	if(synchronize)
		synchronizer->enqueue_samples(SPU_core->outbuf, spu_core_samples);
}

void SPU_Emulate_user(bool mix)
{
	if(!SPU_user)
		return;

	u32 audiosize;

	// Check to see how much free space there is
	// If there is some, fill up the buffer
	audiosize = SNDCore->GetAudioSpace();

	if (audiosize > 0)
	{
		//printf("mix %i samples\n", audiosize);
		if (audiosize > SPU_user->bufsize)
			audiosize = SPU_user->bufsize;

		int samplesOutput;
		if(synchmode == ESynchMode_Synchronous)
			samplesOutput = synchronizer->output_samples(SPU_user->outbuf, audiosize);
		else
			samplesOutput = (SPU_MixAudio(mix,SPU_user,audiosize), audiosize);

		SNDCore->UpdateAudio(SPU_user->outbuf, samplesOutput);
		WAV_WavSoundUpdate(SPU_user->outbuf, samplesOutput, WAVMODE_USER);
	}
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

SoundInterface_struct SNDDummy = {
	SNDCORE_DUMMY,
	"Dummy Sound Interface",
	SNDDummyInit,
	SNDDummyDeInit,
	SNDDummyUpdateAudio,
	SNDDummyGetAudioSpace,
	SNDDummyMuteAudio,
	SNDDummyUnMuteAudio,
	SNDDummySetVolume
};

int SNDDummyInit(int buffersize) { return 0; }
void SNDDummyDeInit() {}
void SNDDummyUpdateAudio(s16 *buffer, u32 num_samples) { }
u32 SNDDummyGetAudioSpace() { return DESMUME_SAMPLE_RATE/60 + 5; }
void SNDDummyMuteAudio() {}
void SNDDummyUnMuteAudio() {}
void SNDDummySetVolume(int volume) {}

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

void spu_savestate(EMUFILE* os)
{
	//version
	write32le(3,os);

	SPU_struct *spu = SPU_core;

	for(int j=0;j<16;j++) {
		channel_struct &chan = spu->channels[j];
		write32le(chan.num,os);
		write8le(chan.vol,os);
		write8le(chan.datashift,os);
		write8le(chan.hold,os);
		write8le(chan.pan,os);
		write8le(chan.waveduty,os);
		write8le(chan.repeat,os);
		write8le(chan.format,os);
		write8le(chan.status,os);
		write32le(chan.addr,os);
		write16le(chan.timer,os);
		write16le(chan.loopstart,os);
		write32le(chan.length,os);
		write64le(double_to_u64(chan.sampcnt),os);
		write64le(double_to_u64(chan.sampinc),os);
		write32le(chan.lastsampcnt,os);
		write16le(chan.pcm16b,os);
		write16le(chan.pcm16b_last,os);
		write32le(chan.index,os);
		write16le(chan.x,os);
		write16le(chan.psgnoise_last,os);
	}

	write64le(double_to_u64(samples),os);
}

bool spu_loadstate(EMUFILE* is, int size)
{
	u64 temp64; 
	
	//read version
	u32 version;
	if(read32le(&version,is) != 1) return false;

	SPU_struct *spu = SPU_core;

	for(int j=0;j<16;j++) {
		channel_struct &chan = spu->channels[j];
		read32le(&chan.num,is);
		read8le(&chan.vol,is);
		read8le(&chan.datashift,is);
		read8le(&chan.hold,is);
		read8le(&chan.pan,is);
		read8le(&chan.waveduty,is);
		read8le(&chan.repeat,is);
		read8le(&chan.format,is);
		read8le(&chan.status,is);
		read32le(&chan.addr,is);
		read16le(&chan.timer,is);
		read16le(&chan.loopstart,is);
		read32le(&chan.length,is);
		chan.totlength = chan.length + chan.loopstart;
		chan.double_totlength_shifted = (double)(chan.totlength << format_shift[chan.format]);
		if(version >= 2)
		{
			read64le(&temp64,is); chan.sampcnt = u64_to_double(temp64);
			read64le(&temp64,is); chan.sampinc = u64_to_double(temp64);
		}
		else
		{
			read32le((u32*)&chan.sampcnt,is);
			read32le((u32*)&chan.sampinc,is);
		}
		read32le(&chan.lastsampcnt,is);
		read16le(&chan.pcm16b,is);
		read16le(&chan.pcm16b_last,is);
		read32le(&chan.index,is);
		read16le(&chan.x,is);
		read16le(&chan.psgnoise_last,is);

		//hopefully trigger a recovery of the adpcm looping system
		chan.loop_index = K_ADPCM_LOOPING_RECOVERY_INDEX;

		//fixup the pointers which we had are supposed to keep cached
		chan.buf8 = (s8*)&MMU.MMU_MEM[1][(chan.addr>>20)&0xFF][(chan.addr & MMU.MMU_MASK[1][(chan.addr >> 20) & 0xFF])];
	}

	if(version>=2) {
		read64le(&temp64,is); samples = u64_to_double(temp64);
	}

	//copy the core spu (the more accurate) to the user spu
	if(SPU_user) {
		memcpy(SPU_user->channels,SPU_core->channels,sizeof(SPU_core->channels));
	}

	return true;
}
