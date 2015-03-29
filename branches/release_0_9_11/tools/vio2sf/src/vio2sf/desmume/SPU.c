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

#include "ARM9.h"
#include "MMU.h"
#include "SPU.h"
#include "mem.h"

#include "armcpu.h"

enum
{
	FORMAT_PCM8 = 0,
	FORMAT_PCM16 = 1,
	FORMAT_ADPCM = 2,
	FORMAT_PSG = 3
};

#define VOL_SHIFT 10

typedef struct
{
	int id;
	int status;
	int format;
	u8 *buf8; s16 *buf16;
	double pos, inc;
	int loopend, looppos;
	int loop, length;
	s32 adpcm;
	int adpcm_pos, adpcm_index;
	s32 adpcm_loop;
	int adpcm_loop_pos, adpcm_loop_index;
	int psg_duty;
	int timer;
	int volume;
	int pan;
	int shift;
	int repeat, hold;
	u32 addr;
	s32 volumel;
	s32 volumer;
	s16 output;
} SChannel;

typedef struct
{
	s32 *pmixbuf;
	s16 *pclipingbuf;
	u32 buflen;
	SChannel ch[16];
} SPU_struct;

static SPU_struct spu = { 0, 0, 0 };

static SoundInterface_struct *SNDCore=NULL;
extern SoundInterface_struct *SNDCoreList[];

int SPU_ChangeSoundCore(int coreid, int buffersize)
{
	int i;
	SPU_DeInit();

   // Allocate memory for sound buffer
	spu.buflen = buffersize * 2; /* stereo */
	spu.pmixbuf = malloc(spu.buflen * sizeof(s32));
	if (!spu.pmixbuf)
	{
		SPU_DeInit();
		return -1;
	}

	spu.pclipingbuf = malloc(spu.buflen * sizeof(s16));
	if (!spu.pclipingbuf)
	{
		SPU_DeInit();
		return -1;
	}

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

	if (SNDCore == NULL)
	{
		SPU_DeInit();
		return -1;
	}

	if (SNDCore->Init(spu.buflen) == -1)
	{
		// Since it failed, instead of it being fatal, we'll just use the dummy
		// core instead
		SNDCore = &SNDDummy;
	}

   return 0;
}
int SPU_Init(int coreid, int buffersize)
{
	SPU_DeInit();
	SPU_Reset();
	return SPU_ChangeSoundCore(coreid, buffersize);
}
void SPU_Pause(int pause)
{
	if(pause)
		SNDCore->MuteAudio();
	else
		SNDCore->UnMuteAudio();
}
void SPU_SetVolume(int volume)
{
	if (SNDCore)
		SNDCore->SetVolume(volume);
}
void SPU_DeInit(void)
{
	spu.buflen = 0;
	if (spu.pmixbuf)
	{
		free(spu.pmixbuf);
		spu.pmixbuf = 0;
	}
	if (spu.pclipingbuf)
	{
		free(spu.pclipingbuf);
		spu.pclipingbuf = 0;
	}
	if (SNDCore)
	{
		SNDCore->DeInit();
	}
	SNDCore = &SNDDummy;
}

static const short g_adpcm_index[16] = { -1, -1, -1, -1, -1, -1, -1, -1, 2, 2, 4, 4, 6, 6, 8, 8 };

static const int g_adpcm_mult[89] =
{
	0x0007, 0x0008, 0x0009, 0x000A, 0x000B, 0x000C, 0x000D, 0x000E, 0x0010, 0x0011,
	0x0013, 0x0015, 0x0017, 0x0019, 0x001C, 0x001F, 0x0022, 0x0025, 0x0029, 0x002D,
	0x0032, 0x0037, 0x003C, 0x0042, 0x0049, 0x0050, 0x0058, 0x0061, 0x006B, 0x0076,
	0x0082, 0x008F, 0x009D, 0x00AD, 0x00BE, 0x00D1, 0x00E6, 0x00FD, 0x0117, 0x0133,
	0x0151, 0x0173, 0x0198, 0x01C1, 0x01EE, 0x0220, 0x0256, 0x0292, 0x02D4, 0x031C,
	0x036C, 0x03C3, 0x0424, 0x048E, 0x0502, 0x0583, 0x0610, 0x06AB, 0x0756, 0x0812,
	0x08E0, 0x09C3, 0x0ABD, 0x0BD0, 0x0CFF, 0x0E4C, 0x0FBA, 0x114C, 0x1307, 0x14EE,
	0x1706, 0x1954, 0x1BDC, 0x1EA5, 0x21B6, 0x2515, 0x28CA, 0x2CDF, 0x315B, 0x364B,
	0x3BB9, 0x41B2, 0x4844, 0x4F7E, 0x5771, 0x602F, 0x69CE, 0x7462, 0x7FFF,
};

static const s16 g_psg_duty[8][8] = {
  { -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, +0x7FFF },
  { -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, +0x7FFF, +0x7FFF },
  { -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF },
  { -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF },
  { -0x7FFF, -0x7FFF, -0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF },
  { -0x7FFF, -0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF },
  { -0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF, +0x7FFF },
  { -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF },
};


static void reset_channel(SChannel *ch, int id)
{
	ch->status = 0;
	ch->id = id;
}

void SPU_Reset(void)
{
	int i;
	for (i = 0;i < 16; i++)
		reset_channel(&spu.ch[i], i);

	for (i = 0x400; i < 0x51D; i++)
		T1WriteByte(MMU.ARM7_REG, i, 0);
}
void SPU_KeyOn(int channel)
{
}

static INLINE void adjust_channel_timer(SChannel *ch)
{
	ch->inc = (((double)33512000) / (44100 * 2)) / (double)(0x10000 - ch->timer);
}

static int check_valid(u32 addr, u32 size)
{
	u32 t1, t2;

	if(size > MMU.MMU_MASK[1][(addr >> 20) & 0xff]) return 0;

	t1 = addr;
	t2 = (addr + size);
	t1 &= MMU.MMU_MASK[1][(addr >> 20) & 0xff];
	t2 &= MMU.MMU_MASK[1][(addr >> 20) & 0xff];

	if(t2 < t1) return 0;

	return 1;
}

static void start_channel(SChannel *ch)
{

	switch(ch->format)
	{
	case FORMAT_PCM8:
		{
			u8 *p = MMU.MMU_MEM[1][(ch->addr >> 20) & 0xff];
			u32 ofs = MMU.MMU_MASK[1][(ch->addr >> 20) & 0xff] & ch->addr;
			u32 size = ((ch->length + ch->loop) << 2);
			if((p != NULL) && check_valid(ch->addr, size))
			{
				ch->buf8 = p + ofs;
				ch->looppos = ch->loop << 2;
				ch->loopend = size;
				ch->pos = 0;
				ch->status = 1;
			}
		}
	break;
	case FORMAT_PCM16:
		{
			u8 *p = MMU.MMU_MEM[1][(ch->addr >> 20) & 0xff];
			u32 ofs = MMU.MMU_MASK[1][(ch->addr >> 20) & 0xff] & ch->addr;
			u32 size = ((ch->length + ch->loop) << 1);
			if((p != NULL) && check_valid(ch->addr, size << 1))
			{
				ch->buf16 = (s16 *)(p + ofs - (ofs & 1));
				ch->looppos = ch->loop << 1;
				ch->loopend = size;
				ch->pos = 0;
				ch->status = 1;
			}
		}
	break;
	case FORMAT_ADPCM:
		{
			u8 *p = MMU.MMU_MEM[1][(ch->addr >> 20) & 0xff];
			u32 ofs = MMU.MMU_MASK[1][(ch->addr >> 20) & 0xff] & ch->addr;
			u32 size = ((ch->length + ch->loop) << 3);
			if((p != NULL) && check_valid(ch->addr, size >> 1))
			{
				ch->buf8 = p + ofs;
#ifdef WORDS_BIGENDIAN
				ch->adpcm = ((s32)(s16)T1ReadWord((u8 *)ch->buf8, 0)) << 3;
#else
				ch->adpcm = ((s32)*(s16 *)ch->buf8) << 3;
#endif
				ch->adpcm_index = (ch->buf8[2] & 0x7F);
				ch->adpcm_pos = 8;
				ch->pos = 9;
				ch->looppos = ch->loop << 3;
				ch->loopend = size;
				ch->adpcm_loop_index = -1;
				ch->status = 1;
			}
		}
		break;
	case FORMAT_PSG:
		ch->status = 1;
		if(ch->id < 14)
		{
			ch->pos = 0;
		}
		else
		{
			ch->pos = 0x7FFF;
		}
		break;
	}
}

static void stop_channel(SChannel *ch)
{
	u32 addr = 0x400 + (ch->id << 4) + 3;
	ch->status = 0;
	T1WriteByte(MMU.ARM7_REG, addr, (u8)(T1ReadByte(MMU.ARM7_REG, addr) & ~0x80));
}
static void set_channel_volume(SChannel *ch)
{
	s32 vol1 = (T1ReadByte(MMU.ARM7_REG, 0x500) & 0x7F) * ch->volume;
	s32 vol2;
	vol2 = vol1 * ch->pan;
	vol1 = vol1 * (127-ch->pan);
	ch->volumel = vol1 >> (21 - VOL_SHIFT + ch->shift);
	ch->volumer = vol2 >> (21 - VOL_SHIFT + ch->shift);
}

void SPU_WriteByte(u32 addr, u8 x)
{
	addr &= 0x00000FFF;
	T1WriteByte(MMU.ARM7_REG, addr, x);

	if(addr < 0x500)
	{
		SChannel *ch;
		switch(addr & 0x0F)
		{
		case 0x0:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->volume = (x & 0x7F);
			set_channel_volume(ch);
			break;
		case 0x1:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->shift = (x & 0x03);
			ch->hold = (x >> 7 & 0x01);
			set_channel_volume(ch);
			break;
		case 0x2:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->pan = (x & 0x7F);
			set_channel_volume(ch);
			break;
		case 0x3:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->psg_duty = (x & 0x07);
			ch->repeat = (x >> 3 & 0x03);
			ch->format = (x >> 5 & 0x03);
			if(x & 0x80) start_channel(ch); else stop_channel(ch);
			break;
#if !DISABLE_XSF_TESTS
		case 0x04:
		case 0x05:
		case 0x06:
		case 0x07:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->addr = (T1ReadLong(MMU.ARM7_REG, addr & ~3) & 0x07FFFFFF);
			break;
		case 0x08:
		case 0x09:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->timer = T1ReadWord(MMU.ARM7_REG, addr & ~1);
			adjust_channel_timer(ch);
			break;
		case 0x0a:
		case 0x0b:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->loop = T1ReadWord(MMU.ARM7_REG, addr & ~1);
			break;
		case 0x0c:
		case 0x0e:
		case 0x0d:
		case 0x0f:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->length = (T1ReadLong(MMU.ARM7_REG, addr & ~3) & 0x003FFFFF);
			break;
#endif
		}
	}

}


void SPU_WriteWord(u32 addr, u16 x)
{
	addr &= 0x00000FFF;
	T1WriteWord(MMU.ARM7_REG, addr, x);

	if(addr < 0x500)
	{
		SChannel *ch;
		switch(addr & 0x00F)
		{
		case 0x0:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->volume = (x & 0x007F);
			ch->shift = (x >> 8 & 0x0003);
			ch->hold = (x >> 15 & 0x0001);
			set_channel_volume(ch);
			break;
		case 0x2:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->pan = (x & 0x007F);
			ch->psg_duty = (x >> 8 & 0x0007);
			ch->repeat = (x >> 11 & 0x0003);
			ch->format = (x >> 13 & 0x0003);
			set_channel_volume(ch);
			if(x & 0x8000) start_channel(ch); else stop_channel(ch);
			break;
		case 0x08:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->timer = x;
			adjust_channel_timer(ch);
			break;
		case 0x0a:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->loop = x;
			break;
#if !DISABLE_XSF_TESTS
		case 0x04:
		case 0x06:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->addr = (T1ReadLong(MMU.ARM7_REG, addr & ~3) & 0x07FFFFFF);
			break;
		case 0x0c:
		case 0x0e:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->length = (T1ReadLong(MMU.ARM7_REG, addr & ~3) & 0x003FFFFF);
			break;
#endif
		}
	}
}


void SPU_WriteLong(u32 addr, u32 x)
{
	addr &= 0x00000FFF;
	T1WriteLong(MMU.ARM7_REG, addr, x);

	if(addr < 0x500)
	{
		SChannel *ch;
		switch(addr & 0x00F)
		{
		case 0x0:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->volume = (x & 0x7F);
			ch->shift = (x >> 8 & 0x00000003);
			ch->hold = (x >> 15 & 0x00000001);
			ch->pan = (x >> 16 & 0x0000007F);
			ch->psg_duty = (x >> 24 & 0x00000007);
			ch->repeat = (x >> 27 & 0x00000003);
			ch->format = (x >> 29 & 0x00000003);
			set_channel_volume(ch);
			if(x & 0x80000000) start_channel(ch); else stop_channel(ch);
			break;
		case 0x04:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->addr = (x & 0x07FFFFFF);
			break;
		case 0x08:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->timer = (x & 0x0000FFFF);
			ch->loop = (x >> 16 & 0x0000FFFF);
			adjust_channel_timer(ch);
			break;
		case 0x0C:
			ch = spu.ch + (addr >> 4 & 0xF);
			ch->length = (x & 0x003FFFFF);
			break;
		}
	}
}

//////////////////////////////////////////////////////////////////////////////

u32 SPU_ReadLong(u32 addr)
{
	addr &= 0xFFF;
	return T1ReadLong(MMU.ARM7_REG, addr);
}

static INLINE s32 clipping(s32 x, s32 min, s32 max) {
#if 1 || defined(SIGNED_IS_NOT_2S_COMPLEMENT)
	if (x < min)
	{
		return (min);
	}
	else if (x > max)
	{
		return (max);
	}
	return (x);
#else
	return x ^ ((-(x < min)) & (x ^ min)) ^ ((-(x > max)) & (x ^ max));
#endif
}

extern unsigned long dwChannelMute;

static void decode_pcm8(SChannel *ch, s32 *out, int length)
{
	int oi;
	double pos, inc, len;
	if (!ch->buf8) return;

	pos = ch->pos; inc = ch->inc; len = ch->loopend;

	for(oi = 0; oi < length; oi++)
	{
		ch->output = ((s16)(s8)ch->buf8[(int)pos]) << 8;
		if (dwChannelMute & (1 << ch->id))
		{
			out++;
			out++;
		}
		else
		{
			*(out++) += (ch->output * ch->volumel) >> VOL_SHIFT;
			*(out++) += (ch->output * ch->volumer) >> VOL_SHIFT;
		}
		pos += inc;
		if(pos >= len)
		{
			switch(ch->repeat)
			{
#if !DISABLE_XSF_TESTS
			case 0:
#endif
			case 1:
				pos += ch->looppos - len;
				break;
			default:
				stop_channel(ch);
				oi = length;
				break;
			}
		}
	}

	ch->pos = pos;
	return;
}

static void decode_pcm16(SChannel *ch, s32 *out, int length)
{
	int oi;
	double pos, inc, len;

	if (!ch->buf16) return;

	pos = ch->pos; inc = ch->inc; len = ch->loopend;

	for(oi = 0; oi < length; oi++)
	{
#ifdef WORDS_BIGENDIAN
		ch->output = (s16)T1ReadWord((u8 *)ch->buf16, pos << 1);
#else
		ch->output = (s16)ch->buf16[(int)pos];
#endif
		if (dwChannelMute & (1 << ch->id))
		{
			out++;
			out++;
		}
		else
		{
			*(out++) += (ch->output * ch->volumel) >> VOL_SHIFT;
			*(out++) += (ch->output * ch->volumer) >> VOL_SHIFT;
		}
		pos += inc;
		if(pos >= len)
		{
			switch(ch->repeat)
			{
#if !DISABLE_XSF_TESTS
			case 0:
#endif
			case 1:
				pos += ch->looppos - len;
				break;
			default:
				stop_channel(ch);
				oi = length;
				break;
			}
		}
	}

	ch->pos = pos;
}

static INLINE void decode_adpcmone_P4(SChannel *ch, int m)
{
	int i, ci0;
	u8 *p;
	s32 s;
	int N;

	i = ch->adpcm_pos;
	p = (ch->buf8 + (i >> 1));
	ci0 = ch->adpcm_index;
	s = ch->adpcm;

	if (ch->adpcm_loop_index < 0 && m >= ch->looppos)
	{
		ch->adpcm_loop_index = ci0;
		ch->adpcm_loop  = s;
		ch->adpcm_loop_pos  = i;
	}

	if(i++ & 1)
	{
		s32 x1, d1;
		x1 = ((*(p++) >> 3) & 0x1F | 1);
		d1 = ((x1 & 0xF) * g_adpcm_mult[ci0] & ~7);
		ci0 = clipping((ci0 + g_adpcm_index[x1 & 0xE]), 0, 88);
#if 1 || defined(SIGNED_IS_NOT_2S_COMPLEMENT)
		if(x1 & 0x10) d1 = -d1;
#else
		d1 -= (d1 + d1) & (-(((x1 >> 4) & 1)));
#endif

		s = clipping((s + d1), (-32768 << 3), (32767 << 3));
	}

	N = (((m & ~1) - (i & ~1)) >> 1);
	for(i = 0; i < N; i++)
	{
		s32 x0, d0;
		s32 x1, d1;
		int ci1;
		x0 = ((*p << 1) & 0x1F | 1);
		x1 = ((*p >> 3) & 0x1F | 1);
		ci1 = clipping((ci0 + g_adpcm_index[x0 & 0xE]), 0, 88);
		d0 = ((x0 & 0xF) * g_adpcm_mult[ci0] & ~7);
		ci0 = clipping((ci1 + g_adpcm_index[x1 & 0xE]), 0, 88);
		d1 = ((x1 & 0xF) * g_adpcm_mult[ci1] & ~7);
#if 1 || defined(SIGNED_IS_NOT_2S_COMPLEMENT)
		if(x0 & 0x10) d0 = -d0;
		if(x1 & 0x10) d1 = -d1;
#else
		d0 -= (d0 + d0) & (-(((x0 >> 4) & 1)));
		d1 -= (d1 + d1) & (-(((x1 >> 4) & 1)));
#endif
		s = clipping((s + d0), (-32768 << 3), (32767 << 3));
		s = clipping((s + d1), (-32768 << 3), (32767 << 3));
		p++;
	}
	if(m & 1)
	{
		s32 x0, d0;
		x0 = ((*p << 1) & 0x1F | 1);
		d0 = ((x0 & 0xF) * g_adpcm_mult[ci0] & ~7);
		ci0 = clipping((ci0 + g_adpcm_index[x0 & 0xE]), 0, 88);
#if 1 || defined(SIGNED_IS_NOT_2S_COMPLEMENT)
		if(x0 & 0x10) d0 = -d0;
#else
		d0 -= (d0 + d0) & (-(((x0 >> 4) & 1)));
#endif
		s = clipping((s + d0), (-32768 << 3), (32767 << 3));
	}

	ch->output = (s16)(s >> 3);

	ch->adpcm = s;
	ch->adpcm_index = ci0;
	ch->adpcm_pos = m;
}

static INLINE void decode_adpcmone_XX(SChannel *ch, int m)
{
	int i, ci0;
	u8 *p;
	s32 s;

	i = ch->adpcm_pos;
	p = (ch->buf8 + (i >> 1));
	ci0 = ch->adpcm_index;
	s = ch->adpcm;

	if (ch->adpcm_loop_index < 0 && m >= ch->looppos)
	{
		ch->adpcm_loop_index = ci0;
		ch->adpcm_loop  = s;
		ch->adpcm_loop_pos  = i;
	}

	while (i < m)
	{
		s32 x1, d1;
		x1 = ((s32)*p) >> ((i & 1) << 2) & 0xf;
		x1 = x1 + x1 + 1;
		d1 = ((x1 & 0xF) * g_adpcm_mult[ci0] & ~7);
		ci0 = clipping((ci0 + g_adpcm_index[x1 & 0xE]), 0, 88);
#if 1 || defined(SIGNED_IS_NOT_2S_COMPLEMENT)
		if(x1 & 0x10) d1 = -d1;
#else
		d1 -= (d1 + d1) & (-(((x1 >> 4) & 1)));
#endif

		s = clipping((s + d1), (-32768 << 3), (32767 << 3));
		p += (i & 1);
		i++;
	}

	ch->output = (s16)(s >> 3);

	ch->adpcm = s;
	ch->adpcm_index = ci0;
	ch->adpcm_pos = m;
}

#define decode_adpcmone decode_adpcmone_P4

static void decode_adpcm(SChannel *ch, s32 *out, int length)
{
	int oi;
	double pos, inc, len;
	if (!ch->buf8) return;

	pos = ch->pos; inc = ch->inc; len = ch->loopend;

	for(oi = 0; oi < length; oi++)
	{
		int m = (int)pos;
		int i = ch->adpcm_pos;
		if(i < m)
			decode_adpcmone(ch, m);

		if (dwChannelMute & (1 << ch->id))
		{
			out++;
			out++;
		}
		else
		{
			*(out++) += (ch->output * ch->volumel) >> VOL_SHIFT;
			*(out++) += (ch->output * ch->volumer) >> VOL_SHIFT;
		}
		pos += inc;
		if(pos >= len)
		{
			switch(ch->repeat)
			{
			case 1:
				if (ch->adpcm_loop_index >= 0)
				{
					pos += ch->looppos - len;
					ch->adpcm_pos = ch->adpcm_loop_pos;
					ch->adpcm_index  = ch->adpcm_loop_index;
					ch->adpcm  = ch->adpcm_loop;
					break;
				}
#if !DISABLE_XSF_TESTS
			case 0:
				pos = 9 - len;
#ifdef WORDS_BIGENDIAN
				ch->adpcm = ((s32)(s16)T1ReadWord((u8 *)ch->buf8, 0)) << 3;
#else
				ch->adpcm = ((s32)*(s16 *)ch->buf8) << 3;
#endif
				ch->adpcm_index = (ch->buf8[2] & 0x7F);
				ch->adpcm_pos = 8;
				break;
#endif
			default:
				stop_channel(ch);
				oi = length;
				break;
			}
		}
	}
	ch->pos = pos;
}

static void decode_psg(SChannel *ch, s32 *out, int length)
{
	int oi;

	if(ch->id < 14)
	{
		// NOTE: square wave.
		double pos, inc, len;
		pos = ch->pos; inc = ch->inc; len = ch->length;
		for(oi = 0; oi < length; oi++)
		{
			ch->output = (s16)g_psg_duty[ch->psg_duty][(int)pos & 0x00000007];
			if (dwChannelMute & (1 << ch->id))
			{
				out++;
				out++;
			}
			else
			{
				*(out++) += (ch->output * ch->volumel) >> VOL_SHIFT;
				*(out++) += (ch->output * ch->volumer) >> VOL_SHIFT;
			}
			pos += inc;
		}
		ch->pos = pos;
	}
	else
	{
		// NOTE: noise.
		u16 X;
		X = (u16)ch->pos;
		for(oi = 0; oi < length; oi++)
		{
			if(X & 1)
			{
				X >>= 1;
				X ^= 0x6000;
				ch->output = -0x8000;
			}
			else
			{
				X >>= 1;
				ch->output = +0x7FFF;
			}
		}
		if (dwChannelMute & (1 << ch->id))
		{
			out++;
			out++;
		}
		else
		{
			*(out++) += (ch->output * ch->volumel) >> VOL_SHIFT;
			*(out++) += (ch->output * ch->volumer) >> VOL_SHIFT;
		}
		ch->pos = X;
	}
}



void SPU_EmulateSamples(u32 numsamples)
{
	u32 sizesmp = numsamples;
	u32 sizebyte = sizesmp << 2;
	if (sizebyte > spu.buflen * sizeof(s16)) sizebyte = spu.buflen * sizeof(s16);
	sizesmp = sizebyte >> 2;
	sizebyte = sizesmp << 2;
	if (sizesmp > 0)
	{
		unsigned i;
		SChannel *ch = spu.ch;
		memset(spu.pmixbuf, 0, spu.buflen * sizeof(s32));
		for (i = 0; i < 16; i++)
		{
			if (ch->status)
			{
				switch (ch->format)
				{
				case 0:
					decode_pcm8(ch, spu.pmixbuf, sizesmp);
					break;
				case 1:
					decode_pcm16(ch, spu.pmixbuf, sizesmp);
					break;
				case 2:
					decode_adpcm(ch, spu.pmixbuf, sizesmp);
					break;
				case 3:
					decode_psg(ch, spu.pmixbuf, sizesmp);
					break;
				}
			}
			ch++;
		}
		for (i = 0; i < sizesmp * 2; i++)
			spu.pclipingbuf[i] = (s16)clipping(spu.pmixbuf[i], -0x8000, 0x7fff);
		SNDCore->UpdateAudio(spu.pclipingbuf, sizesmp);
	}
}

void SPU_Emulate(void)
{
	SPU_EmulateSamples(SNDCore->GetAudioSpace());
}


//////////////////////////////////////////////////////////////////////////////
// Dummy Sound Interface
//////////////////////////////////////////////////////////////////////////////

static int SNDDummyInit(int buffersize)
{
   return 0;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDDummyDeInit()
{
}

//////////////////////////////////////////////////////////////////////////////

static void SNDDummyUpdateAudio(s16 *buffer, u32 num_samples)
{
}

//////////////////////////////////////////////////////////////////////////////

static u32 SNDDummyGetAudioSpace()
{
   return 735;
}

//////////////////////////////////////////////////////////////////////////////

static void SNDDummyMuteAudio()
{
}

//////////////////////////////////////////////////////////////////////////////

static void SNDDummyUnMuteAudio()
{
}

//////////////////////////////////////////////////////////////////////////////

static void SNDDummySetVolume(int volume)
{
}

//////////////////////////////////////////////////////////////////////////////

SoundInterface_struct SNDDummy =
{
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

