/*  Copyright (C) 2006 Theo Berkau

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
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <stdlib.h>
#include <string.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include "debug.h"
#include "ARM9.h"
#include "MMU.h"
#include "SPU.h"
#include "mem.h"
#include "readwrite.h"
#include "armcpu.h"

SPU_struct *SPU_core = 0;
SPU_struct *SPU_user = 0;

static SoundInterface_struct *SNDCore=NULL;
extern SoundInterface_struct *SNDCoreList[];

#define CHANSTAT_STOPPED          0
#define CHANSTAT_PLAY             1

const s8 indextbl[8] =
{
	-1, -1, -1, -1, 2, 4, 6, 8
};

const u16 adpcmtbl[89] =
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

const s16 wavedutytbl[8][8] = {
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
	{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF }
};

s32 precalcdifftbl[89][16];
u8 precalcindextbl[89][8];

FILE *spufp=NULL;

//////////////////////////////////////////////////////////////////////////////

template<typename T>
static INLINE T MinMax(T val, T min, T max)
{
	if (val < min)
		return min;
	else if (val > max)
		return max;

	return val;
}

//////////////////////////////////////////////////////////////////////////////

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

	return 0;
}

SoundInterface_struct *SPU_SoundCore()
{
	return SNDCore;
}

//////////////////////////////////////////////////////////////////////////////

//static double cos_lut[256];

int SPU_Init(int coreid, int buffersize)
{
	int i, j;

	//for(int i=0;i<256;i++)
	//	cos_lut[i] = cos(i/256.0*M_PI);

	SPU_core = new SPU_struct(740);
	SPU_Reset();

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

//////////////////////////////////////////////////////////////////////////////

void SPU_Pause(int pause)
{
	if (SNDCore == NULL) return;

	if(pause)
		SNDCore->MuteAudio();
	else
		SNDCore->UnMuteAudio();
}

//////////////////////////////////////////////////////////////////////////////

void SPU_SetVolume(int volume)
{
	if (SNDCore)
		SNDCore->SetVolume(volume);
}

//////////////////////////////////////////////////////////////////////////////


void SPU_Reset(void)
{
	int i;

	SPU_core->reset();
	if(SPU_user) SPU_user->reset();

	if(SNDCore && SPU_user) {
		SNDCore->DeInit();
		SNDCore->Init(SPU_user->bufsize*2);
		//todo - check success?
	}

	// Reset Registers
	for (i = 0x400; i < 0x51D; i++)
		T1WriteByte(MMU.ARM7_REG, i, 0);
}

void SPU_struct::reset()
{
	memset((void *)channels, 0, sizeof(channel_struct) * 16);
	memset(sndbuf,0,bufsize*2*4);
	memset(outbuf,0,bufsize*2*2);

	for(int i = 0; i < 16; i++)
		channels[i].num = i;
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

void SPU_struct::KeyOn(int channel)
{
	channel_struct &thischan = channels[channel];

	thischan.sampinc = (16777216 / (0x10000 - (double)thischan.timer)) / 44100;

	//   LOG("Channel %d key on: vol = %d, datashift = %d, hold = %d, pan = %d, waveduty = %d, repeat = %d, format = %d, source address = %07X, timer = %04X, loop start = %04X, length = %06X, MMU.ARM7_REG[0x501] = %02X\n", channel, chan->vol, chan->datashift, chan->hold, chan->pan, chan->waveduty, chan->repeat, chan->format, chan->addr, chan->timer, chan->loopstart, chan->length, T1ReadByte(MMU.ARM7_REG, 0x501));
	switch(thischan.format)
	{
	case 0: // 8-bit
		thischan.buf8 = (s8*)&MMU.MMU_MEM[1][(thischan.addr>>20)&0xFF][(thischan.addr & MMU.MMU_MASK[1][(thischan.addr >> 20) & 0xFF])];
	//	thischan.loopstart = thischan.loopstart << 2;
	//	thischan.length = (thischan.length << 2) + thischan.loopstart;
		thischan.sampcnt = 0;
		break;
	case 1: // 16-bit
		thischan.buf16 = (s16 *)&MMU.MMU_MEM[1][(thischan.addr>>20)&0xFF][(thischan.addr & MMU.MMU_MASK[1][(thischan.addr >> 20) & 0xFF])];
	//	thischan.loopstart = thischan.loopstart << 1;
	//	thischan.length = (thischan.length << 1) + thischan.loopstart;
		thischan.sampcnt = 0;
		break;
	case 2: // ADPCM
		{
			thischan.buf8 = (s8*)&MMU.MMU_MEM[1][(thischan.addr>>20)&0xFF][(thischan.addr & MMU.MMU_MASK[1][(thischan.addr >> 20) & 0xFF])];
			thischan.pcm16b = (s16)((thischan.buf8[1] << 8) | thischan.buf8[0]);
			thischan.pcm16b_last = thischan.pcm16b;
			thischan.index = thischan.buf8[2] & 0x7F;
			thischan.lastsampcnt = 7;
			thischan.sampcnt = 8;
		//	thischan.loopstart = thischan.loopstart << 3;
		//	thischan.length = (thischan.length << 3) + thischan.loopstart;
			break;
		}
	case 3: // PSG
		{
			thischan.x = 0x7FFF;
			break;
		}
	default: break;
	}
}

//////////////////////////////////////////////////////////////////////////////

u32 SPU_ReadLong(u32 addr)
{
	addr &= 0xFFF;

	return T1ReadLong(MMU.ARM7_REG, addr);
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
			thischan.status = (val >> 7) & 0x1;
			if(thischan.status)
				KeyOn((addr >> 4) & 0xF);
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
		thischan.status = (val >> 15) & 0x1;
		if (thischan.status)
			KeyOn((addr >> 4) & 0xF);
		break;
	case 0x8:
		thischan.timer = val & 0xFFFF;
		thischan.sampinc = (16777216 / (0x10000 - (double)thischan.timer)) / 44100;
		break;
	case 0xA:
		thischan.loopstart = val;
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
		thischan.status = (val >> 31) & 0x1;
		if (thischan.status)
			KeyOn((addr >> 4) & 0xF);
		break;
	case 0x4:
		thischan.addr = val & 0x7FFFFFF;
		break;
	case 0x8:
		thischan.timer = val & 0xFFFF;
		thischan.loopstart = val >> 16;
		thischan.sampinc = (16777216 / (0x10000 - (double)thischan.timer)) / 44100;
		break;
	case 0xC:
		thischan.length = val & 0x3FFFFF;
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

#ifdef SPU_INTERPOLATE
static s32 Interpolate(s32 a, s32 b, double ratio)
{
	//ratio = ratio - (int)ratio;
	//double ratio2 = ((1.0f - cos(ratio * 3.14f)) / 2.0f);
	////double ratio2 = (1.0f - cos_lut[((int)(ratio*256.0))&0xFF]) / 2.0f;
	//return (((1-ratio2)*a) + (ratio2*b));
	
	//linear interpolation
	ratio = ratio - (int)ratio;
	return (1-ratio)*a + ratio*b;
}
#endif

//////////////////////////////////////////////////////////////////////////////

static INLINE void Fetch8BitData(channel_struct *chan, s32 *data)
{
#ifdef SPU_INTERPOLATE
	int loc = (int)chan->sampcnt;
	s32 a = (s32)(chan->buf8[loc] << 8);
	if(loc < (chan->length << 2) - 1) {
	/*	double ratio = chan->sampcnt-loc;*/
		s32 b = (s32)(chan->buf8[loc + 1] << 8);
	/*	a = (1-ratio)*a + ratio*b;*/
		a = Interpolate(a, b, chan->sampcnt);
	}
	*data = a;
#else
	*data = (s32)chan->buf8[(int)chan->sampcnt] << 8;
#endif
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Fetch16BitData(channel_struct *chan, s32 *data)
{
#ifdef SPU_INTERPOLATE
	int loc = (int)chan->sampcnt;
	s32 a = (s32)chan->buf16[loc];
	if(loc < (chan->length << 1) - 1) {
		//double ratio = chan->sampcnt-loc;
		s32 b = (s32)chan->buf16[loc + 1];
		//a = (1-ratio)*a + ratio*b;
		a = Interpolate(a, b, chan->sampcnt);
	}
	*data = a;
#else
	*data = (s32)chan->buf16[(int)chan->sampcnt];
#endif
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void FetchADPCMData(channel_struct *chan, s32 *data)
{
	u8 data4bit;
	int diff;
	int i;

	if (chan->lastsampcnt == (int)chan->sampcnt)
	{
		// No sense decoding, just return the last sample
#ifdef SPU_INTERPOLATE
		*data = Interpolate((s32)chan->pcm16b_last,(s32)chan->pcm16b,chan->sampcnt);
#else
		*data = (s32)chan->pcm16b;
#endif
		return;
	}

	for (i = chan->lastsampcnt+1; i < (int)chan->sampcnt+1; i++)
	{
		if (i & 0x1)
			data4bit = (chan->buf8[i >> 1] >> 4) & 0xF;
		else
			data4bit = chan->buf8[i >> 1] & 0xF;

		/*diff = ((data4bit & 0x7) * 2 + 1) * adpcmtbl[chan->index] / 8;
		if (data4bit & 0x8)
			diff = -diff;*/
		diff = precalcdifftbl[chan->index][data4bit];

		chan->pcm16b_last = chan->pcm16b;
		chan->pcm16b = MinMax(chan->pcm16b+diff, -0x8000, 0x7FFF);
		//chan->index = MinMax(chan->index+indextbl[data4bit & 0x7], 0, 88);
		chan->index = precalcindextbl[chan->index][data4bit & 0x7];
	}

	chan->lastsampcnt = chan->sampcnt;

#ifdef SPU_INTERPOLATE
	*data = Interpolate((s32)chan->pcm16b_last,(s32)chan->pcm16b,chan->sampcnt);
#else
	*data = (s32)chan->pcm16b;
#endif
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void FetchPSGData(channel_struct *chan, s32 *data)
{
	if(chan->num < 8)
	{
		*data = 0;
	}
	else if(chan->num < 14)
	{
		*data = (s32)wavedutytbl[chan->waveduty][((int)chan->sampcnt) & 0x7];
	}
	else
	{
		if(chan->lastsampcnt == (int)chan->sampcnt)
		{
			*data = (s32)chan->psgnoise_last;
			return;
		}

		for(int i = chan->lastsampcnt; i < (int)chan->sampcnt; i++)
		{
			if(chan->x & 0x1)
			{
				chan->x = (chan->x >> 1);
				chan->psgnoise_last = -0x7FFF;
			}
			else
			{
				chan->x = ((chan->x >> 1) ^ 0x6000);
				chan->psgnoise_last = 0x7FFF;
			}
		}

		chan->lastsampcnt = (int)chan->sampcnt;

		*data = (s32)chan->psgnoise_last;
	}
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void MixL(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	if (data)
	{
		data = (data * chan->vol / 127) >> chan->datashift;
		SPU->sndbuf[SPU->bufpos<<1] += data;
	}
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void MixR(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	if (data)
	{
		data = (data * chan->vol / 127) >> chan->datashift;
		SPU->sndbuf[(SPU->bufpos<<1)+1] += data;
	}
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void MixLR(SPU_struct* SPU, channel_struct *chan, s32 data)
{
	if (data)
	{
		data = ((data * chan->vol) / 127) >> chan->datashift;
		SPU->sndbuf[SPU->bufpos<<1] += data * (127 - chan->pan) / 127;
		SPU->sndbuf[(SPU->bufpos<<1)+1] += data * chan->pan / 127;
	}
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void TestForLoop(SPU_struct *SPU, channel_struct *chan)
{
	int shift = (chan->format == 0 ? 2 : 1);

	chan->sampcnt += chan->sampinc;

	if (chan->sampcnt > (double)((chan->length + chan->loopstart) << shift))
	{
		// Do we loop? Or are we done?
		if (chan->repeat == 1)
			chan->sampcnt = (double)(chan->loopstart << shift); // Is this correct?
		else
		{
			chan->status = CHANSTAT_STOPPED;

			if(SPU == SPU_core)
				MMU.ARM7_REG[0x403 + (((chan-SPU->channels) ) * 0x10)] &= 0x7F;
			SPU->bufpos = SPU->buflength;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void TestForLoop2(SPU_struct *SPU, channel_struct *chan)
{
	chan->sampcnt += chan->sampinc;

	if (chan->sampcnt > (double)((chan->length + chan->loopstart) << 3))
	{
		// Do we loop? Or are we done?
		if (chan->repeat == 1)
		{
			chan->sampcnt = (double)(chan->loopstart << 3); // Is this correct?
			chan->pcm16b = (s16)((chan->buf8[1] << 8) | chan->buf8[0]);
			chan->index = chan->buf8[2] & 0x7F;
			chan->lastsampcnt = 7;
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

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdate8LR(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		Fetch8BitData(chan, &data);

		MixLR(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdate8NoMix(SPU_struct *SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

static void SPU_ChanUpdate8L(SPU_struct *SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		Fetch8BitData(chan, &data);

		MixL(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdate8R(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		Fetch8BitData(chan, &data);

		MixR(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdate16NoMix(SPU_struct *SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}


static void SPU_ChanUpdate16LR(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		Fetch16BitData(chan, &data);

		MixLR(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdate16L(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		Fetch16BitData(chan, &data);

		MixL(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdate16R(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		Fetch16BitData(chan, &data);

		MixR(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdateADPCMNoMix(SPU_struct *SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		// check to see if we're passed the length and need to loop, etc.
		TestForLoop2(SPU, chan);
	}
}


static void SPU_ChanUpdateADPCMLR(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		FetchADPCMData(chan, &data);

		MixLR(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop2(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdateADPCML(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		FetchADPCMData(chan, &data);

		MixL(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop2(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdateADPCMR(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		FetchADPCMData(chan, &data);

		MixR(SPU, chan, data);

		// check to see if we're passed the length and need to loop, etc.
		TestForLoop2(SPU, chan);
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdatePSGNoMix(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		chan->sampcnt += chan->sampinc;
	}
}


static void SPU_ChanUpdatePSGLR(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		FetchPSGData(chan, &data);

		MixLR(SPU, chan, data);

		chan->sampcnt += chan->sampinc;
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdatePSGL(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		FetchPSGData(chan, &data);

		MixL(SPU, chan, data);

		chan->sampcnt += chan->sampinc;
	}
}

//////////////////////////////////////////////////////////////////////////////

static void SPU_ChanUpdatePSGR(SPU_struct* SPU, channel_struct *chan)
{
	for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
	{
		s32 data;

		// fetch data from source address
		FetchPSGData(chan, &data);

		MixR(SPU, chan, data);

		chan->sampcnt += chan->sampinc;
	}
}

//////////////////////////////////////////////////////////////////////////////

void (*SPU_ChanUpdate[4][4])(SPU_struct* SPU, channel_struct *chan) = {
	{ // 8-bit PCM
		SPU_ChanUpdate8L,
			SPU_ChanUpdate8LR,
			SPU_ChanUpdate8R,
			SPU_ChanUpdate8NoMix
	},
	{ // 16-bit PCM
		SPU_ChanUpdate16L,
			SPU_ChanUpdate16LR,
			SPU_ChanUpdate16R,
			SPU_ChanUpdate16NoMix,
		},
		{ // IMA-ADPCM
			SPU_ChanUpdateADPCML,
				SPU_ChanUpdateADPCMLR,
				SPU_ChanUpdateADPCMR,
				SPU_ChanUpdateADPCMNoMix
		},
		{ // PSG/White Noise
			SPU_ChanUpdatePSGL,
				SPU_ChanUpdatePSGLR,
				SPU_ChanUpdatePSGR,
				SPU_ChanUpdatePSGNoMix
			}
};

//////////////////////////////////////////////////////////////////////////////

template<bool actuallyMix> 
static void SPU_MixAudio(SPU_struct *SPU, int length)
{
	int i;
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

	for(i=0;i<16;i++)
	{
		channel_struct *chan = &SPU->channels[i];
	
		if (chan->status != CHANSTAT_PLAY)
			continue;

		SPU->bufpos = 0;
		SPU->buflength = length;

		// Mix audio
		if(!actuallyMix)
			SPU_ChanUpdate[chan->format][3](SPU,chan);
		else if (chan->pan == 0)
			SPU_ChanUpdate[chan->format][0](SPU,chan);
		else if (chan->pan == 127)
			SPU_ChanUpdate[chan->format][2](SPU,chan);
		else
			SPU_ChanUpdate[chan->format][1](SPU,chan);
	}

	// convert from 32-bit->16-bit
	if(actuallyMix)
		for (i = 0; i < length*2; i++)
		{
			// Apply Master Volume
			SPU->sndbuf[i] = SPU->sndbuf[i] * vol / 127;

			if (SPU->sndbuf[i] > 0x7FFF)
				SPU->outbuf[i] = 0x7FFF;
			else if (SPU->sndbuf[i] < -0x8000)
				SPU->outbuf[i] = -0x8000;
			else
				SPU->outbuf[i] = (s16)SPU->sndbuf[i];
		}
}

//////////////////////////////////////////////////////////////////////////////


//emulates one frame of the cpu core.
//this will produce a variable number of samples, calculated to keep a 44100hz output
//in sync with the emulator framerate
static float samples = 0;
static const float time_per_frame = 1.0f/59.8261f;
static const float samples_per_frame = time_per_frame * 44100;
int spu_core_samples = 0;
void SPU_Emulate_core()
{
	samples += samples_per_frame;
	spu_core_samples = (int)(samples);
	samples -= spu_core_samples;
	
	SPU_MixAudio<false>(SPU_core,spu_core_samples);
}

void SPU_Emulate_user()
{
	if(!SPU_user)
		return;

	u32 audiosize;

	// Check to see how much free space there is
	// If there is some, fill up the buffer
	audiosize = SNDCore->GetAudioSpace();

	if (audiosize > 0)
	{
		if (audiosize > SPU_user->bufsize)
			audiosize = SPU_user->bufsize;
		SPU_MixAudio<true>(SPU_user,audiosize);
		SNDCore->UpdateAudio(SPU_user->outbuf, audiosize);
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

//////////////////////////////////////////////////////////////////////////////

int SNDDummyInit(int buffersize)
{
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummyDeInit()
{
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummyUpdateAudio(s16 *buffer, u32 num_samples)
{
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDDummyGetAudioSpace()
{
	return 740;
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummyMuteAudio()
{
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummyUnMuteAudio()
{
}

//////////////////////////////////////////////////////////////////////////////

void SNDDummySetVolume(int volume)
{
}

//////////////////////////////////////////////////////////////////////////////
// WAV Write Interface
//////////////////////////////////////////////////////////////////////////////

int SNDFileInit(int buffersize);
void SNDFileDeInit();
void SNDFileUpdateAudio(s16 *buffer, u32 num_samples);
u32 SNDFileGetAudioSpace();
void SNDFileMuteAudio();
void SNDFileUnMuteAudio();
void SNDFileSetVolume(int volume);

SoundInterface_struct SNDFile = {
	SNDCORE_FILEWRITE,
	"WAV Write Sound Interface",
	SNDFileInit,
	SNDFileDeInit,
	SNDFileUpdateAudio,
	SNDFileGetAudioSpace,
	SNDFileMuteAudio,
	SNDFileUnMuteAudio,
	SNDFileSetVolume
};

//////////////////////////////////////////////////////////////////////////////

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

//////////////////////////////////////////////////////////////////////////////

int SNDFileInit(int buffersize)
{
	waveheader_struct waveheader;
	fmt_struct fmt;
	chunk_struct data;
	size_t elems_written = 0;

	if ((spufp = fopen("ndsaudio.wav", "wb")) == NULL)
		return -1;

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
	fmt.rate = 44100;
	fmt.bitspersample = 16;
	fmt.blockalign = fmt.bitspersample / 8 * fmt.numchan;
	fmt.bytespersec = fmt.rate * fmt.blockalign;
	elems_written += fwrite((void *)&fmt, 1, sizeof(fmt_struct), spufp);

	// data chunk
	memcpy(data.id, "data", 4);
	data.size = 0; // we'll fix this at the end
	elems_written += fwrite((void *)&data, 1, sizeof(chunk_struct), spufp);

	return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileDeInit()
{
	size_t elems_written = 0;
	if (spufp)
	{
		long length = ftell(spufp);

		// Let's fix the riff chunk size and the data chunk size
		fseek(spufp, sizeof(waveheader_struct)-0x8, SEEK_SET);
		length -= 0x8;
		elems_written += fwrite((void *)&length, 1, 4, spufp);

		fseek(spufp, sizeof(waveheader_struct)+sizeof(fmt_struct)+0x4, SEEK_SET);
		length -= sizeof(waveheader_struct)+sizeof(fmt_struct);
		elems_written += fwrite((void *)&length, 1, 4, spufp);
		fclose(spufp);
	}
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileUpdateAudio(s16 *buffer, u32 num_samples)
{
	size_t elems_written;
	if (spufp) {
		elems_written = fwrite((void *)buffer, num_samples*2, 2, spufp);
		//INFO("%i written\n", elems_written);
	}
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDFileGetAudioSpace()
{
	return 740;
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileMuteAudio()
{
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileUnMuteAudio()
{
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileSetVolume(int volume)
{
}

//////////////////////////////////////////////////////////////////////////////

void spu_savestate(std::ostream* os)
{
	//version
	write32le(0,os);

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
}

bool spu_loadstate(std::istream* is)
{
	//read version
	int version;
	if(read32le(&version,is) != 1) return false;
	if(version != 0) return false;

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
		u64 temp; 
		read64le(&temp,is); chan.sampcnt = u64_to_double(temp);
		read64le(&temp,is); chan.sampinc = u64_to_double(temp);
		read32le(&chan.lastsampcnt,is);
		read16le(&chan.pcm16b,is);
		read16le(&chan.pcm16b_last,is);
		read32le(&chan.index,is);
		read16le(&chan.x,is);
		read16le(&chan.psgnoise_last,is);

		//fixup the pointers which we had are supposed to keep cached
		chan.buf8 = (s8*)&MMU.MMU_MEM[1][(chan.addr>>20)&0xFF][(chan.addr & MMU.MMU_MASK[1][(chan.addr >> 20) & 0xFF])];
		chan.buf16 = (s16*)chan.buf8;
	}

	//copy the core spu (the more accurate) to the user spu
	if(SPU_user) {
		memcpy(SPU_user->channels,SPU_core->channels,sizeof(SPU_core->channels));
	}

	return true;
}
