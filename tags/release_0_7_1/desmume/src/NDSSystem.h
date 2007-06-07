/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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

#ifndef NDSSYSTEM_H
#define NDSSYSTEM_H

#include "armcpu.h"
#include "MMU.h"

#include "GPU.h"
#include "SPU.h"

#include "mem.h"
#include "wifi.h"

#ifdef __cplusplus
extern "C" {
#endif

extern volatile BOOL execute;
extern BOOL click;

//#define LOG_ARM9
//#define LOG_ARM7

typedef struct
{
       char     gameTile[12];
       char     gameCode[4];
       u16      makerCode;
       u8       unitCode;
       u8       deviceCode;
       u8       cardSize;
       u8       cardInfo[8];
       u8       flags;
       
       u32      ARM9src;
       u32      ARM9exe;
       u32      ARM9cpy;
       u32      ARM9binSize;
       
       u32      ARM7src;
       u32      ARM7exe;
       u32      ARM7cpy;
       u32      ARM7binSize;
       
       u32      FNameTblOff;
       u32      FNameTblSize;
       
       u32      FATOff;
       u32      FATSize;
       
       u32     ARM9OverlayOff;
       u32     ARM9OverlaySize;
       u32     ARM7OverlayOff;
       u32     ARM7OverlaySize;
       
       u32     unknown2a;
       u32     unknown2b;
       
       u32     IconOff;
       u16     CRC16;
       u16     ROMtimeout;
       u32     ARM9unk;
       u32     ARM7unk;
       
       u8      unknown3c[8];
       u32     ROMSize;
       u32     HeaderSize;
       u8      unknown5[56];
       u8      logo[156];
       u16     logoCRC16;
       u16     headerCRC16;
       u8      reserved[160];
} NDS_header;

extern void debug();

typedef struct
{
       s32 ARM9Cycle;
       s32 ARM7Cycle;
       s32 cycles;
       s32 timerCycle[2][4];
       BOOL timerOver[2][4];
       s32 nextHBlank;
       u32 VCount;
       u32 old;
       s32 diff;
       BOOL lignerendu;
       
       u16 touchX;
       u16 touchY;
} NDSSystem;

typedef struct
{
       u8 favColor;
       u8 bMonth;
       u8 bDay;
       char nickName[10];
       u8 nickLen;
       char message[26];
       u8 msgLen;
       u8 language;
       
} NDSFirmware;

extern NDSSystem nds;
extern NDSFirmware firmware;

int NDS_Init( struct armcpu_memory_iface *arm9_mem_if,
              struct armcpu_ctrl_iface **arm9_ctrl_iface,
              struct armcpu_memory_iface *arm7_mem_if,
              struct armcpu_ctrl_iface **arm7_ctrl_iface);
void NDS_DeInit(void);

BOOL NDS_SetROM(u8 * rom, u32 mask);
NDS_header * NDS_getROMHeader(void);
 
void NDS_setTouchPos(u16 x, u16 y);
void NDS_releasTouch(void);

int NDS_LoadROM(const char *filename, int bmtype, u32 bmsize,
                 const char *cflash_disk_image_file);
void NDS_FreeROM(void);
void NDS_Reset(void);
int NDS_ImportSave(const char *filename);

int NDS_WriteBMP(const char *filename);
int NDS_LoadFirmware(const char *filename);
int NDS_CreateDummyFirmware(void);
u32
NDS_exec(s32 nb, BOOL force);

       static INLINE void NDS_ARM9HBlankInt(void)
       {
            if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0x10)
            {
                 MMU.reg_IF[0] |= 2;// & (MMU.reg_IME[0] << 1);// (MMU.reg_IE[0] & (1<<1));
                 NDS_ARM9.wIRQ = TRUE;
            }
       }
       
       static INLINE void NDS_ARM7HBlankInt(void)
       {
            if(T1ReadWord(MMU.ARM7_REG, 4) & 0x10)
            {
                 MMU.reg_IF[1] |= 2;// & (MMU.reg_IME[1] << 1);// (MMU.reg_IE[1] & (1<<1));
                 NDS_ARM7.wIRQ = TRUE;
            }
       }
       
       static INLINE void NDS_ARM9VBlankInt(void)
       {
            if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0x8)
            {
                 MMU.reg_IF[0] |= 1;// & (MMU.reg_IME[0]);// (MMU.reg_IE[0] & 1);
                 NDS_ARM9.wIRQ = TRUE;
                      //execute = FALSE;
                      /*logcount++;*/
            }
       }
       
       static INLINE void NDS_ARM7VBlankInt(void)
       {
            if(T1ReadWord(MMU.ARM7_REG, 4) & 0x8)
                 MMU.reg_IF[1] |= 1;// & (MMU.reg_IME[1]);// (MMU.reg_IE[1] & 1);
                 NDS_ARM7.wIRQ = TRUE;
                 //execute = FALSE;
       }
       
       static INLINE void NDS_swapScreen(void)
       {
	       u16 tmp = MainScreen.offset;
	       MainScreen.offset = SubScreen.offset;
	       SubScreen.offset = tmp;
       }
 
 
#ifdef __cplusplus
}
#endif

#endif

 	  	 
