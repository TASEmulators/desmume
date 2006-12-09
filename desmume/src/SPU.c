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

#include "debug.h"
#include "ARM9.h"
#include "MMU.h"
#include "SPU.h"

#include "armcpu.h"

SPU_struct *SPU=NULL;

static SoundInterface_struct *SNDCore=NULL;
extern SoundInterface_struct *SNDCoreList[];

#define CHANSTAT_STOPPED          0
#define CHANSTAT_PLAY             1

#define ARM7_REG_8(a) (MMU.ARM7_REG[a])

#define ARM7_REG_16(a) (((u16 *)(MMU.ARM7_REG+addr))[0])
#define ARM7_REG_32(a) (((u32 *)(MMU.ARM7_REG+addr))[0])

int indextbl[8] =
{
  -1, -1, -1, -1, 2, 4, 6, 8
};

int adpcmtbl[89] =
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

s16 wavedutytbl[8][8] = {
{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF },
{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF },
{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
{ -0x7FFF, -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
{ -0x7FFF, -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
{ -0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF },
{ -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF, -0x7FFF }
};

FILE *spufp=NULL;

//////////////////////////////////////////////////////////////////////////////

int SPU_ChangeSoundCore(int coreid, int buffersize)
{
   int i;

   if (SPU->sndbuf)
      free(SPU->sndbuf);

   if (SPU->outbuf)
      free(SPU->outbuf);

   // Make sure the old core is freed
   if (SNDCore)
      SNDCore->DeInit();

   // Allocate memory for sound buffer
   if ((SPU->sndbuf = malloc(buffersize * 4 * 2)) == NULL)
      return -1;

   if ((SPU->outbuf = malloc(buffersize * 2 * 2)) == NULL)
      return -1;

   memset(SPU->sndbuf, 0, buffersize * 4 * 2);
   memset(SPU->outbuf, 0, buffersize * 2 * 2);

   SPU->bufsize = buffersize;

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
      SNDCore = &SNDDummy;
      return -1;
   }

   if (SNDCore->Init(buffersize * 2) == -1)
   {
      // Since it failed, instead of it being fatal, we'll just use the dummy
      // core instead
      SNDCore = &SNDDummy;
   }

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

int SPU_Init(int coreid, int buffersize)
{
   if ((SPU = (SPU_struct *)malloc(sizeof(SPU_struct))) == NULL)
      return -1;

   memset((void *)SPU, 0, sizeof(SPU_struct));

   SPU_Reset();

   return SPU_ChangeSoundCore(coreid, buffersize);
}

//////////////////////////////////////////////////////////////////////////////

void SPU_Pause(int pause)
{
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

   memset((void *)SPU->chan, 0, sizeof(channel_struct) * 16);

   // Reset Registers
   for (i = 0x400; i < 0x51D; i++)
      ARM7_REG_8(i) = 0;
}

//////////////////////////////////////////////////////////////////////////////

void SPU_DeInit(void)
{
   if (SPU->sndbuf)
      free(SPU->sndbuf);

   if (SPU->outbuf)
      free(SPU->outbuf);

   if (SNDCore)
      SNDCore->DeInit();

   if (SPU)
      free(SPU);
}

//////////////////////////////////////////////////////////////////////////////

void SPU_KeyOn(int channel)
{
   channel_struct *chan = &SPU->chan[channel];

   chan->sampinc = (16777216 / (0x10000 - (double)chan->timer)) / 44100;

//   LOG("Channel %d key on: vol = %d, datashift = %d, hold = %d, pan = %d, waveduty = %d, repeat = %d, format = %d, source address = %07X, timer = %04X, loop start = %04X, length = %06X, MMU.ARM7_REG[0x501] = %02X\n", channel, chan->vol, chan->datashift, chan->hold, chan->pan, chan->waveduty, chan->repeat, chan->format, chan->addr, chan->timer, chan->loopstart, chan->length, ARM7_REG_8(0x501));
   switch(chan->format)
   {
      case 0: // 8-bit
         chan->buf8 = &MMU.MMU_MEM[1][(chan->addr>>20)&0xFF][(chan->addr & MMU.MMU_MASK[1][(chan->addr >> 20) & 0xFF])];
         chan->loopstart = chan->loopstart << 2;
         chan->length = (chan->length << 2) + chan->loopstart;
         chan->sampcnt = 0;
         break;
      case 1: // 16-bit
         chan->buf16 = (u16 *)&MMU.MMU_MEM[1][(chan->addr>>20)&0xFF][(chan->addr & MMU.MMU_MASK[1][(chan->addr >> 20) & 0xFF])];
         chan->loopstart = chan->loopstart << 1;
         chan->length = (chan->length << 1) + chan->loopstart;
         chan->sampcnt = 0;
         break;
      case 2: // ADPCM
      {
         u32 temp;

         chan->buf8 = &MMU.MMU_MEM[1][(chan->addr>>20)&0xFF][(chan->addr & MMU.MMU_MASK[1][(chan->addr >> 20) & 0xFF])];
         chan->pcm16b = (s16)((chan->buf8[1] << 8) | chan->buf8[0]);
         chan->index = chan->buf8[2] & 0x7F;
         chan->lastsampcnt = 7;
         chan->sampcnt = 8;
         chan->loopstart = chan->loopstart << 3;
         chan->length = (chan->length << 3) + chan->loopstart;
         break;
      }
      case 3: // PSG
      {
         break;
      }
      default: break;
   }
}

//////////////////////////////////////////////////////////////////////////////

u8 SPU_ReadByte(u32 addr)
{
   addr &= 0xFFF;

   if (addr < 0x500)
   {
      switch (addr & 0xF)
      {
         case 0x0:
//            LOG("Sound Channel %d Volume read\n", (addr >> 4) & 0xF);            
            return ARM7_REG_8(addr);
         case 0x1:
         {
//            LOG("Sound Channel %d Data Shift/Hold read\n",(addr >> 4) & 0xF);
            return ARM7_REG_8(addr);
         }
         case 0x2:
//            LOG("Sound Channel %d Panning read\n",(addr >> 4) & 0xF);
            return ARM7_REG_8(addr);
         case 0x3:
//            LOG("Sound Channel %d Wave Duty/Repeat/Format/Start read: %02X\n", (addr >> 4) & 0xF, ARM7_REG_8(addr));
            return ARM7_REG_8(addr);
         case 0x4:
         case 0x5:
         case 0x6:
         case 0x7:
//            LOG("Sound Channel %d Data Source Register read: %08X\n",(addr >> 4) & 0xF, addr);
            return ARM7_REG_8(addr);
         case 0x8:
//            LOG("Sound Channel Timer(Low byte) read: %08X\n", addr);
            return ARM7_REG_8(addr);
         case 0x9:
//            LOG("Sound Channel Timer(High byte) read: %08X\n", addr);
            return ARM7_REG_8(addr);
         case 0xA:
//            LOG("Sound Channel Loop Start(Low byte) read: %08X\n", addr);
            return ARM7_REG_8(addr);
         case 0xB:
//            LOG("Sound Channel Loop Start(High byte) read: %08X\n", addr);
            return ARM7_REG_8(addr);
         case 0xC:
         case 0xD:
         case 0xE:
         case 0xF:
//            LOG("Sound Channel %d Length Register read: %08X\n",(addr >> 4) & 0xF, addr);
            return ARM7_REG_8(addr);
         default: break;
      }
   }
   else
   {
      switch (addr & 0x1F)
      {
         case 0x000:
         case 0x001:
//            LOG("Sound Control Register read: %08X\n", addr);
            return ARM7_REG_8(addr);
         case 0x004:
         case 0x005:
//            LOG("Sound Bias Register read: %08X\n", addr);
            return ARM7_REG_8(addr);
         case 0x008:
//            LOG("Sound Capture 0 Control Register read\n");
            return ARM7_REG_8(addr);
         case 0x009:
//            LOG("Sound Capture 1 Control Register read\n");
            return ARM7_REG_8(addr);
         default: break;
      }
   }

   return ARM7_REG_8(addr);
}

//////////////////////////////////////////////////////////////////////////////

u16 SPU_ReadWord(u32 addr)
{
   addr &= 0xFFF;

   if (addr < 0x500)
   {
      switch (addr & 0xF)
      {
         case 0x0:
//            LOG("Sound Channel %d Volume/data shift/hold word read\n", (addr >> 4) & 0xF);
            return ARM7_REG_16(addr);
         case 0x2:
         {
            channel_struct *chan=&SPU->chan[(addr >> 4) & 0xF]; 
//            LOG("Sound Channel %d Panning/Wave Duty/Repeat Mode/Format/Start word read\n", (addr >> 4) & 0xF);
            return ARM7_REG_16(addr);
         }
         case 0x4:
         case 0x6:
//            LOG("Sound Channel %d Data Source Register word read: %08X\n",(addr >> 4) & 0xF, addr);
            return ARM7_REG_16(addr);
         case 0x8:
//            LOG("Sound Channel %d Timer Register word read\n", (addr >> 4) & 0xF);
            return ARM7_REG_16(addr);
         case 0xA:
//            LOG("Sound Channel %d Loop start Register word read\n", (addr >> 4) & 0xF);
            return ARM7_REG_16(addr);
         case 0xC:
         case 0xE:
//            LOG("Sound Channel %d Length Register word read: %08X\n",(addr >> 4) & 0xF, addr);
            return ARM7_REG_16(addr);
         default: break;
      }
   }
   else
   {
      switch (addr & 0x1F)
      {
         case 0x000:
//            LOG("Sound Control Register word read\n");
            return ARM7_REG_16(addr);
         case 0x004:
//            LOG("Sound Bias Register word read\n");
            return ARM7_REG_16(addr);
         case 0x008:
//            LOG("Sound Capture 0/1 Control Register word read\n");
            return ARM7_REG_16(addr);
         default: break;
      }
   }

   return ARM7_REG_16(addr);
}

//////////////////////////////////////////////////////////////////////////////

u32 SPU_ReadLong(u32 addr)
{
   addr &= 0xFFF;

   if (addr < 0x500)
   {
      switch (addr & 0xF)
      {
         case 0x0:
//            LOG("Sound Channel %d Control Register long read\n", (addr >> 4) & 0xF);            
            return ARM7_REG_32(addr);
         case 0x4:
//            LOG("Sound Channel %d Data Source Register long read\n");
            return ARM7_REG_32(addr);
         case 0x8:
//            LOG("Sound Channel %d Timer/Loop Start Register long read\n", (addr >> 4) & 0xF);
            return ARM7_REG_32(addr);
         case 0xC:
//            LOG("Sound Channel %d Length Register long read\n", (addr >> 4) & 0xF);
            return ARM7_REG_32(addr);
         default:
            return ARM7_REG_32(addr);
      }
   }
   else
   {
      switch (addr & 0x1F)
      {
         case 0x000:
//            LOG("Sound Control Register long read\n");
            return ARM7_REG_32(addr);
         case 0x004:
//            LOG("Sound Bias Register long read\n");
            return ARM7_REG_32(addr);
         case 0x008:
//            LOG("Sound Capture 0/1 Control Register long read: %08X\n");
            return ARM7_REG_32(addr);
         default:
            return ARM7_REG_32(addr);
      }
   }

   return ARM7_REG_32(addr);
}

//////////////////////////////////////////////////////////////////////////////

void SPU_WriteByte(u32 addr, u8 val)
{
   addr &= 0xFFF;

   if (addr < 0x500)
   {
      switch (addr & 0xF)
      {
         case 0x0:
            SPU->chan[(addr >> 4) & 0xF].vol = val & 0x7F;
//            LOG("Sound Channel %d Volume write: %02X\n", (addr >> 4) & 0xF, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0x1:
         {
            channel_struct *chan=&SPU->chan[(addr >> 4) & 0xF]; 

//            LOG("Sound Channel %d Data Shift/Hold write: %02X\n",(addr >> 4) & 0xF, val);
            chan->datashift = val & 0x3;
            if (chan->datashift == 3)
               chan->datashift = 4;
            chan->hold = (val >> 7) & 0x1;
            ARM7_REG_8(addr) = val;
            return;
         }
         case 0x2:
//            LOG("Sound Channel %d Panning write: %02X\n",(addr >> 4) & 0xF, val);
            SPU->chan[(addr >> 4) & 0xF].pan = val & 0x7F;
            ARM7_REG_8(addr) = val;
            return;
         case 0x3:
         {
            channel_struct *chan=&SPU->chan[(addr >> 4) & 0xF]; 
//            LOG("Sound Channel %d Wave Duty/Repeat/Format/Start write: %08X - %02X. ARM7 PC = %08X\n", (addr >> 4) & 0xF, addr, val, NDS_ARM7.instruct_adr);
                               
            chan->waveduty = val & 0x7;
            chan->repeat = (val >> 3) & 0x3;
            chan->format = (val >> 5) & 0x3;
            chan->status = (val >> 7) & 0x1;

            if (SPU->chan[(addr >> 4) & 0xF].status)
               SPU_KeyOn((addr >> 4) & 0xF);
            ARM7_REG_8(addr) = val;
            return;
         }
         case 0x4:
         case 0x5:
         case 0x6:
         case 0x7:
//            LOG("Sound Channel %d Data Source Register write: %08X %02X\n",(addr >> 4) & 0xF, addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0x8:
//            LOG("Sound Channel Timer(Low byte) write: %08X - %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0x9:
//            LOG("Sound Channel Timer(High byte) write: %08X - %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0xA:
//            LOG("Sound Channel Loop Start(Low byte) write: %08X - %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0xB:
//            LOG("Sound Channel Loop Start(High byte) write: %08X - %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0xC:
         case 0xD:
         case 0xE:
         case 0xF:
//            LOG("Sound Channel %d Length Register write: %08X %02X\n",(addr >> 4) & 0xF, addr, val);
            ARM7_REG_8(addr) = val;
            return;
         default:
            LOG("Unsupported Sound Register byte write: %08X %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            break;
      }
   }
   else
   {
      switch (addr & 0x1F)
      {
         case 0x000:
         case 0x001:
//            LOG("Sound Control Register write: %08X %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0x004:
         case 0x005:
//            LOG("Sound Bias Register write: %08X %02X\n", addr, val);
            ARM7_REG_8(addr) = val;
            return;
         case 0x008:
//            LOG("Sound Capture 0 Control Register write: %02X\n", val);
            ARM7_REG_8(addr) = val;
            return;
         case 0x009:
//            LOG("Sound Capture 1 Control Register write: %02X\n", val);
            ARM7_REG_8(addr) = val;
            return;
         default: break;
      }
   }

   ARM7_REG_8(addr) = val;
}

//////////////////////////////////////////////////////////////////////////////

void SPU_WriteWord(u32 addr, u16 val)
{
   addr &= 0xFFF;

   if (addr < 0x500)
   {
      switch (addr & 0xF)
      {
         case 0x0:
         {
            channel_struct *chan=&SPU->chan[(addr >> 4) & 0xF]; 
//            LOG("Sound Channel %d Volume/data shift/hold write: %04X\n", (addr >> 4) & 0xF, val);
            chan->vol = val & 0x7F;
            chan->datashift = (val >> 8) & 0x3;
            if (chan->datashift == 3)
               chan->datashift = 4;
            chan->hold = (val >> 15) & 0x1;
            ARM7_REG_16(addr) = val;
            return;
         }
         case 0x2:
         {
            channel_struct *chan=&SPU->chan[(addr >> 4) & 0xF]; 
//            LOG("Sound Channel %d Panning/Wave Duty/Repeat Mode/Format/Start write: %04X\n", (addr >> 4) & 0xF, val);

            chan->pan = val & 0x7F;
            chan->waveduty = (val >> 8) & 0x7;
            chan->repeat = (val >> 11) & 0x3;
            chan->format = (val >> 13) & 0x3;
            chan->status = (val >> 15) & 0x1;
            if (chan->status)
               SPU_KeyOn((addr >> 4) & 0xF);

            ARM7_REG_16(addr) = val;
            return;
         }
         case 0x4:
         case 0x6:
//            LOG("Sound Channel %d Data Source Register write: %08X %04X\n",(addr >> 4) & 0xF, addr, val);
            ARM7_REG_16(addr) = val;
            return;
         case 0x8:
         {
            channel_struct *chan = &SPU->chan[(addr >> 4) & 0xF];
//            LOG("Sound Channel %d Timer Register write: %04X\n", (addr >> 4) & 0xF, val);
            chan->timer = val & 0xFFFF;
            chan->sampinc = (16777216 / (0x10000 - (double)chan->timer)) / 44100;
            ARM7_REG_16(addr) = val;
            return;
         }
         case 0xA:
//            LOG("Sound Channel %d Loop start Register write: %04X\n", (addr >> 4) & 0xF, val);
            SPU->chan[(addr >> 4) & 0xF].loopstart = val;
            ARM7_REG_16(addr) = val;
            return;
         case 0xC:
         case 0xE:
//            LOG("Sound Channel %d Length Register write: %08X %04X\n",(addr >> 4) & 0xF, addr, val);
            ARM7_REG_16(addr) = val;
            return;
         default:
//            LOG("Unsupported Sound Register word write: %08X %02X\n", addr, val);
            ARM7_REG_16(addr) = val;
            break;
      }
   }
   else
   {
      switch (addr & 0x1F)
      {
         case 0x000:
//            LOG("Sound Control Register write: %04X\n", val);
            ARM7_REG_16(addr) = val;
            return;
         case 0x004:
//            LOG("Sound Bias Register write: %04X\n", val);
            ARM7_REG_16(addr) = val;
            return;
         case 0x008:
//            LOG("Sound Capture 0/1 Control Register write: %04X\n", val);
            ARM7_REG_16(addr) = val;
            return;
         default: break;
      }
   }

   ARM7_REG_16(addr) = val;
}

//////////////////////////////////////////////////////////////////////////////

void SPU_WriteLong(u32 addr, u32 val)
{
   addr &= 0xFFF;

   if (addr < 0x500)
   {
      switch (addr & 0xF)
      {
         case 0x0:
         {
            channel_struct *chan=&SPU->chan[(addr >> 4) & 0xF];
//            LOG("Sound Channel %d long write: %08X\n", (addr >> 4) & 0xF, val);
            chan->vol = val & 0x7F;
            chan->datashift = (val >> 8) & 0x3;
            if (chan->datashift == 3)
               chan->datashift = 4;
            chan->hold = (val >> 15) & 0x1;
            chan->pan = (val >> 16) & 0x7F;
            chan->waveduty = (val >> 24) & 0x7;
            chan->repeat = (val >> 27) & 0x3;
            chan->format = (val >> 29) & 0x3;
            chan->status = (val >> 31) & 0x1;
            if (SPU->chan[(addr >> 4) & 0xF].status)
               SPU_KeyOn((addr >> 4) & 0xF);
            ARM7_REG_32(addr) = val;
            return;
         }
         case 0x4:
//            LOG("Sound Channel %d Data Source Register long write: %08X\n", (addr >> 4) & 0xF, val);
            SPU->chan[(addr >> 4) & 0xF].addr = val & 0x7FFFFFF;
            ARM7_REG_32(addr) = val;
            return;
         case 0x8:
         {
            channel_struct *chan = &SPU->chan[(addr >> 4) & 0xF];

//            LOG("Sound Channel %d Timer/Loop Start Register write: - %08X\n", (addr >> 4) & 0xF, val);
            chan->timer = val & 0xFFFF;
            chan->loopstart = val >> 16;
            chan->sampinc = (16777216 / (0x10000 - (double)chan->timer)) / 44100;
            ARM7_REG_32(addr) = val;
            return;
         }
         case 0xC:
//            LOG("Sound Channel %d Length Register long write: %08X\n", (addr >> 4) & 0xF, val);
            SPU->chan[(addr >> 4) & 0xF].length = val & 0x3FFFFF;
            ARM7_REG_32(addr) = val;
            return;
         default: break;
      }
   }
   else
   {
      switch (addr & 0x1F)
      {
         case 0x000:
//            LOG("Sound Control Register write: %08X\n", val);
            ARM7_REG_32(addr) = val;
            return;
         case 0x004:
//            LOG("Sound Bias Register write: %08X\n", val);
            ARM7_REG_32(addr) = val;
            return;
         case 0x008:
//            LOG("Sound Capture 0/1 Control Register write: %08X\n", val);
            ARM7_REG_32(addr) = val;
            return;
         default: break;
      }
   }

   ARM7_REG_32(addr) = val;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Fetch8BitData(channel_struct *chan, s32 *data)
{
   *data = (s32)chan->buf8[(int)chan->sampcnt] << 8;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void Fetch16BitData(channel_struct *chan, s32 *data)
{
   *data = (s32)chan->buf16[(int)chan->sampcnt];
}

//////////////////////////////////////////////////////////////////////////////

static INLINE int MinMax(int val, int min, int max)
{
   if (val < min)
      return min;
   else if (val > max)
      return max;

   return val;
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
      *data = (s32)chan->pcm16b;
      return;
   }

   for (i = chan->lastsampcnt+1; i < (int)chan->sampcnt+1; i++)
   {
      if (i & 0x1)
         data4bit = (chan->buf8[i >> 1] >> 4) & 0xF;
      else
         data4bit = chan->buf8[i >> 1] & 0xF;

      diff = ((data4bit & 0x7) * 2 + 1) * adpcmtbl[chan->index] / 8;
      if (data4bit & 0x8)
         diff = -diff;

      chan->pcm16b = (s16)MinMax(chan->pcm16b+diff, -0x8000, 0x7FFF);
      chan->index = MinMax(chan->index+indextbl[data4bit & 0x7], 0, 88);
   }

   chan->lastsampcnt = chan->sampcnt;
   *data = (s32)chan->pcm16b;
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void FetchPSGData(channel_struct *chan, s32 *data)
{
   *data = (s32)wavedutytbl[chan->waveduty][((int)chan->sampcnt) & 0x7];
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void MixL(channel_struct *chan, s32 data)
{
   if (data)
   {
      data = (data * chan->vol / 127) >> chan->datashift;
      SPU->sndbuf[SPU->bufpos<<1] += data;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void MixR(channel_struct *chan, s32 data)
{
   if (data)
   {
      data = (data * chan->vol / 127) >> chan->datashift;
      SPU->sndbuf[(SPU->bufpos<<1)+1] += data;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void MixLR(channel_struct *chan, s32 data)
{
   if (data)
   {
      data = ((data * chan->vol) / 127) >> chan->datashift;
      SPU->sndbuf[SPU->bufpos<<1] += data * (127 - chan->pan) / 127;
      SPU->sndbuf[(SPU->bufpos<<1)+1] += data * chan->pan / 127;
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void TestForLoop(channel_struct *chan)
{
   chan->sampcnt += chan->sampinc;

   if (chan->sampcnt > (double)chan->length)
   {
      // Do we loop? Or are we done?
      if (chan->repeat == 1)
         chan->sampcnt = (double)chan->loopstart; // Is this correct?
      else
      {
         chan->status = CHANSTAT_STOPPED;
         MMU.ARM7_REG[0x403 + ((((u32)chan-(u32)SPU->chan) / sizeof(channel_struct)) * 0x10)] &= 0x7F;
         SPU->bufpos = SPU->buflength;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

static INLINE void TestForLoop2(channel_struct *chan)
{
   chan->sampcnt += chan->sampinc;

   if (chan->sampcnt > (double)chan->length)
   {
      // Do we loop? Or are we done?
      if (chan->repeat == 1)
      {
         chan->sampcnt = (double)chan->loopstart; // Is this correct?
         chan->pcm16b = (s16)((chan->buf8[1] << 8) | chan->buf8[0]);
         chan->index = chan->buf8[2] & 0x7F;
         chan->lastsampcnt = 7;
      }
      else
      {
         chan->status = CHANSTAT_STOPPED;
         MMU.ARM7_REG[0x403 + ((((u32)chan-(u32)SPU->chan) / sizeof(channel_struct)) * 0x10)] &= 0x7F;
         SPU->bufpos = SPU->buflength;
      }
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdateNothing(channel_struct *chan)
{
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdate8LR(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       Fetch8BitData(chan, &data);

       MixLR(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdate8L(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       Fetch8BitData(chan, &data);

       MixL(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdate8R(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       Fetch8BitData(chan, &data);

       MixR(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdate16LR(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       Fetch16BitData(chan, &data);

       MixLR(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdate16L(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       Fetch16BitData(chan, &data);

       MixL(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdate16R(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       Fetch16BitData(chan, &data);

       MixR(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdateADPCMLR(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       FetchADPCMData(chan, &data);

       MixLR(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop2(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdateADPCML(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       FetchADPCMData(chan, &data);

       MixL(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop2(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdateADPCMR(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       FetchADPCMData(chan, &data);

       MixR(chan, data);

       // check to see if we're passed the length and need to loop, etc.
       TestForLoop2(chan);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdatePSGLR(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       FetchPSGData(chan, &data);

       MixLR(chan, data);

       chan->sampcnt += chan->sampinc;
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdatePSGL(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       FetchPSGData(chan, &data);

       MixL(chan, data);

       chan->sampcnt += chan->sampinc;
   }
}

//////////////////////////////////////////////////////////////////////////////

void SPU_ChanUpdatePSGR(channel_struct *chan)
{
   for (; SPU->bufpos < SPU->buflength; SPU->bufpos++)
   {
       s32 data;

       // fetch data from source address
       FetchPSGData(chan, &data);

       MixR(chan, data);

       chan->sampcnt += chan->sampinc;
   }
}

//////////////////////////////////////////////////////////////////////////////

void (*SPU_ChanUpdate[4][3])(channel_struct *chan) = {
   { // 8-bit PCM
      SPU_ChanUpdate8L,
      SPU_ChanUpdate8LR,
      SPU_ChanUpdate8R
   },
   { // 16-bit PCM
      SPU_ChanUpdate16L,
      SPU_ChanUpdate16LR,
      SPU_ChanUpdate16R
   },
   { // IMA-ADPCM
      SPU_ChanUpdateADPCML,
      SPU_ChanUpdateADPCMLR,
      SPU_ChanUpdateADPCMR
   },
   { // PSG/White Noise
      SPU_ChanUpdatePSGL,
      SPU_ChanUpdatePSGLR,
      SPU_ChanUpdatePSGR
   }
};

//////////////////////////////////////////////////////////////////////////////

void SPU_MixAudio(int length)
{
   channel_struct *chan;
   u8 vol;
   int i;

   memset(SPU->sndbuf, 0, length*4*2);

   // If Master Enable isn't set, don't output audio
   if (!(MMU.ARM7_REG[0x501] & 0x80))
      return;

   vol = MMU.ARM7_REG[0x500] & 0x7F;

   for(chan = &(SPU->chan[0]); chan < &(SPU->chan[16]); chan++)
   {
      if (chan->status != CHANSTAT_PLAY)
         continue;

      SPU->bufpos = 0;
      SPU->buflength = length;

      // Mix audio
      if (chan->pan == 0)
         SPU_ChanUpdate[chan->format][0](chan);
      else if (chan->pan == 127)
         SPU_ChanUpdate[chan->format][2](chan);
      else
         SPU_ChanUpdate[chan->format][1](chan);
   }

   // convert from 32-bit->16-bit
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

void SPU_Emulate(void)
{
   u32 audiosize;

   // Check to see how much free space there is
   // If there is some, fill up the buffer
   audiosize = SNDCore->GetAudioSpace();

   if (audiosize > 0)
   {
      if (audiosize > SPU->bufsize)
         audiosize = SPU->bufsize;
      SPU_MixAudio(audiosize);
      SNDCore->UpdateAudio(SPU->outbuf, audiosize);
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
   return 735;
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

   if ((spufp = fopen("ndsaudio.wav", "wb")) == NULL)
      return -1;

   // Do wave header
   memcpy(waveheader.riff.id, "RIFF", 4);
   waveheader.riff.size = 0; // we'll fix this after the file is closed
   memcpy(waveheader.rifftype, "WAVE", 4);
   fwrite((void *)&waveheader, 1, sizeof(waveheader_struct), spufp);

   // fmt chunk
   memcpy(fmt.chunk.id, "fmt ", 4);
   fmt.chunk.size = 16; // we'll fix this at the end
   fmt.compress = 1; // PCM
   fmt.numchan = 2; // Stereo
   fmt.rate = 44100;
   fmt.bitspersample = 16;
   fmt.blockalign = fmt.bitspersample / 8 * fmt.numchan;
   fmt.bytespersec = fmt.rate * fmt.blockalign;
   fwrite((void *)&fmt, 1, sizeof(fmt_struct), spufp);

   // data chunk
   memcpy(data.id, "data", 4);
   data.size = 0; // we'll fix this at the end
   fwrite((void *)&data, 1, sizeof(chunk_struct), spufp);

   return 0;
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileDeInit()
{
   if (spufp)
   {
      long length = ftell(spufp);

      // Let's fix the riff chunk size and the data chunk size
      fseek(spufp, sizeof(waveheader_struct)-0x8, SEEK_SET);
      length -= 0x4;
      fwrite((void *)&length, 1, 4, spufp);

      fseek(spufp, sizeof(waveheader_struct)+sizeof(fmt_struct)+0x4, SEEK_SET);
      length -= sizeof(waveheader_struct)+sizeof(fmt_struct);
      fwrite((void *)&length, 1, 4, spufp);
      fclose(spufp);
   }
}

//////////////////////////////////////////////////////////////////////////////

void SNDFileUpdateAudio(s16 *buffer, u32 num_samples)
{
   if (spufp)
      fwrite((void *)buffer, num_samples*2, 2, spufp);
}

//////////////////////////////////////////////////////////////////////////////

u32 SNDFileGetAudioSpace()
{
   return 735;
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

