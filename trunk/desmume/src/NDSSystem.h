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

#ifdef __cplusplus
extern "C" {
#endif

extern volatile BOOL execute;
extern BOOL click;

//#define LOG_ARM9
//#define LOG_ARM7

#define REG_DIVCNT          (0x280>>2)
#define REG_DIV_NUMER_L     (0x290>>2)
#define REG_DIV_NUMER_H     (0x294>>2)
#define REG_DIV_DENOM_L     (0x298>>2)
#define REG_DIV_DENOM_H     (0x29C>>2)
#define REG_DIV_RESULT_L    (0x2A0>>2)
#define REG_DIV_RESULT_H    (0x2A4>>2)
#define REG_DIV_REMAIN_L    (0x2A8>>2)
#define REG_DIV_REMAIN_H    (0x2AC>>2)
#define REG_SQRTCNT         (0x2B0>>2)
#define REG_SQRT_PARAM_L    (0x2B8>>2)
#define REG_SQRT_PARAM_H    (0x2BC>>2)
#define REG_SQRT_RESULT     (0x2B4>>2)
#define reg_IPCSYNC         (0x180>>1)
#define reg_IPCFIFOCNT      (0x184>>1)

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

extern NDSSystem nds;

int NDSInit(void);
void NDSDeInit(void);

BOOL NDS_SetROM(u8 * rom, u32 mask);
NDS_header * NDS_getROMHeader(void);
 
void NDS_setTouchPos(u16 x, u16 y);
void NDS_releasTouch(void);

int NDS_LoadROM(const char *filename);
void NDS_FreeROM(void);
void NDS_Reset(void);

int NDS_WriteBMP(const char *filename);
int NDS_LoadFirmware(const char *filename);

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
 
       #define INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))
       static INLINE u32 NDS_exec(s32 nb, BOOL force)
       {
            nb += nds.cycles;//(nds.cycles>>26)<<26;
            
            for(; (nb >= nds.cycles) && ((force)||(execute)); )
            {
                 if(nds.ARM9Cycle<=nds.cycles)
                 {
                      #ifdef LOG_ARM9
                      if(logcount==3){
                      if(NDS_ARM9.CPSR.bits.T)
                           des_thumb_instructions_set[(NDS_ARM9.instruction)>>6](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, logbuf);
                      else
                           des_arm_instructions_set[INDEX(NDS_ARM9.instruction)](NDS_ARM9.instruct_adr, NDS_ARM9.instruction, logbuf);
                      sprintf(logbuf, "%s\t%08X\n\t R00: %08X, R01: %08X, R02: %08X, R03: %08X, R04: %08X, R05: %08X, R06: %08X, R07: %08X,\n\t R08: %08X, R09: %08X, R10: %08X, R11: %08X, R12: %08X, R13: %08X, R14: %08X, R15: %08X,\n\t CPSR: %08X , SPSR: %08X",
                                    logbuf, NDS_ARM9.instruction, NDS_ARM9.R[0],  NDS_ARM9.R[1],  NDS_ARM9.R[2],  NDS_ARM9.R[3],  NDS_ARM9.R[4],  NDS_ARM9.R[5],  NDS_ARM9.R[6],  NDS_ARM9.R[7], 
                                     NDS_ARM9.R[8],  NDS_ARM9.R[9],  NDS_ARM9.R[10],  NDS_ARM9.R[11],  NDS_ARM9.R[12],  NDS_ARM9.R[13],  NDS_ARM9.R[14],  NDS_ARM9.R[15],
                                     NDS_ARM9.CPSR, NDS_ARM9.SPSR);  
                      LOG(logbuf);
                      }
                      #endif
                      if(NDS_ARM9.waitIRQ)
                           nds.ARM9Cycle += 100;
                      else
                           //nds.ARM9Cycle += NDS_ARM9.exec();
								 nds.ARM9Cycle += armcpu_exec(&NDS_ARM9);
                 }
                 if(nds.ARM7Cycle<=nds.cycles)
                 {
                      #ifdef LOG_ARM7
                      if(logcount==1){
                      if(NDS_ARM7.CPSR.bits.T)
                           des_thumb_instructions_set[(NDS_ARM7.instruction)>>6](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, logbuf);
                      else
                           des_arm_instructions_set[INDEX(NDS_ARM7.instruction)](NDS_ARM7.instruct_adr, NDS_ARM7.instruction, logbuf);
                      sprintf(logbuf, "%s\n\t R00: %08X, R01: %08X, R02: %08X, R03: %08X, R04: %08X, R05: %08X, R06: %08X, R07: %08X,\n\t R08: %08X, R09: %08X, R10: %08X, R11: %08X, R12: %08X, R13: %08X, R14: %08X, R15: %08X,\n\t CPSR: %08X , SPSR: %08X",
                                    logbuf, NDS_ARM7.R[0],  NDS_ARM7.R[1],  NDS_ARM7.R[2],  NDS_ARM7.R[3],  NDS_ARM7.R[4],  NDS_ARM7.R[5],  NDS_ARM7.R[6],  NDS_ARM7.R[7], 
                                     NDS_ARM7.R[8],  NDS_ARM7.R[9],  NDS_ARM7.R[10],  NDS_ARM7.R[11],  NDS_ARM7.R[12],  NDS_ARM7.R[13],  NDS_ARM7.R[14],  NDS_ARM7.R[15],
                                     NDS_ARM7.CPSR, NDS_ARM7.SPSR);  
                      LOG(logbuf);
                      }
                      #endif
                      if(NDS_ARM7.waitIRQ)
                           nds.ARM7Cycle += 100;
                      else
                           //nds.ARM7Cycle += (NDS_ARM7.exec()<<1);
								 nds.ARM7Cycle += (armcpu_exec(&NDS_ARM7)<<1);
                 }
                 nds.cycles = (nds.ARM9Cycle<nds.ARM7Cycle)?nds.ARM9Cycle : nds.ARM7Cycle;
                 
                 debug();
                 
                 if(nds.cycles>=nds.nextHBlank)
                 {
                      if(!nds.lignerendu)
                      {
                         if(nds.VCount<192)
                           {
                                GPU_ligne(&MainScreen, nds.VCount);
                                GPU_ligne(&SubScreen, nds.VCount);
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 2);
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 2);
                                NDS_ARM9HBlankInt();
                                NDS_ARM7HBlankInt();
                                if(MMU.DMAStartTime[0][0] == 2)
                                     MMU_doDMA(0, 0);
                                if(MMU.DMAStartTime[0][1] == 2)
                                     MMU_doDMA(0, 1);
                                if(MMU.DMAStartTime[0][2] == 2)
                                     MMU_doDMA(0, 2);
                                if(MMU.DMAStartTime[0][3] == 2)
                                     MMU_doDMA(0, 3);
                           }
                           nds.lignerendu = TRUE;
                      }
                      if(nds.cycles>=nds.nextHBlank+1092)
                      {
                           u32 vmatch;

                           ++nds.VCount;
                           nds.nextHBlank += 4260;
			   T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFD);
			   T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFD);
                           
                           if(MMU.DMAStartTime[0][0] == 3)
                                MMU_doDMA(0, 0);
                           if(MMU.DMAStartTime[0][1] == 3)
                                MMU_doDMA(0, 1);
                           if(MMU.DMAStartTime[0][2] == 3)
                                MMU_doDMA(0, 2);
                           if(MMU.DMAStartTime[0][3] == 3)
                                MMU_doDMA(0, 3);
                                
                           nds.lignerendu = FALSE;
                           if(nds.VCount==193)
                           {
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 1);
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 1);
                                NDS_ARM9VBlankInt();
                                NDS_ARM7VBlankInt();
                                
                                if(MMU.DMAStartTime[0][0] == 1)
                                     MMU_doDMA(0, 0);
                                if(MMU.DMAStartTime[0][1] == 1)
                                     MMU_doDMA(0, 1);
                                if(MMU.DMAStartTime[0][2] == 1)
                                     MMU_doDMA(0, 2);
                                if(MMU.DMAStartTime[0][3] == 1)
                                     MMU_doDMA(0, 3);
                                
                                if(MMU.DMAStartTime[1][0] == 1)
                                     MMU_doDMA(1, 0);
                                if(MMU.DMAStartTime[1][1] == 1)
                                     MMU_doDMA(1, 1);
                                if(MMU.DMAStartTime[1][2] == 1)
                                     MMU_doDMA(1, 2);
                                if(MMU.DMAStartTime[1][3] == 1)
                                     MMU_doDMA(1, 3);
                           }
                           else                         
                                if(nds.VCount==263)
                                {
                                     nds.nextHBlank = 3168;
                                     nds.VCount = 0;
			             T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFE);
			             T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFE);

                                     nds.cycles -= (560190<<1);
                                     nds.ARM9Cycle -= (560190<<1);
                                     nds.ARM7Cycle -= (560190<<1);
                                     nb -= (560190<<1);
                                     if(MMU.timerON[0][0])
                                     nds.timerCycle[0][0] -= (560190<<1);
                                     if(MMU.timerON[0][1])
                                     nds.timerCycle[0][1] -= (560190<<1);
                                     if(MMU.timerON[0][2])
                                     nds.timerCycle[0][2] -= (560190<<1);
                                     if(MMU.timerON[0][3])
                                     nds.timerCycle[0][3] -= (560190<<1);
                                     
                                     if(MMU.timerON[1][0])
                                     nds.timerCycle[1][0] -= (560190<<1);
                                     if(MMU.timerON[1][1])
                                     nds.timerCycle[1][1] -= (560190<<1);
                                     if(MMU.timerON[1][2])
                                     nds.timerCycle[1][2] -= (560190<<1);
                                     if(MMU.timerON[1][3])
                                     nds.timerCycle[1][3] -= (560190<<1);
                                     if(MMU.DMAing[0][0])
                                          MMU.DMACycle[0][0] -= (560190<<1);
                                     if(MMU.DMAing[0][1])
                                          MMU.DMACycle[0][1] -= (560190<<1);
                                     if(MMU.DMAing[0][2])
                                          MMU.DMACycle[0][2] -= (560190<<1);
                                     if(MMU.DMAing[0][3])
                                          MMU.DMACycle[0][3] -= (560190<<1);
                                     if(MMU.DMAing[1][0])
                                          MMU.DMACycle[1][0] -= (560190<<1);
                                     if(MMU.DMAing[1][1])
                                          MMU.DMACycle[1][1] -= (560190<<1);
                                     if(MMU.DMAing[1][2])
                                          MMU.DMACycle[1][2] -= (560190<<1);
                                     if(MMU.DMAing[1][3])
                                          MMU.DMACycle[1][3] -= (560190<<1);
                                }
                                
			   T1WriteWord(ARM9Mem.ARM9_REG, 6, nds.VCount);
			   T1WriteWord(MMU.ARM7_REG, 6, nds.VCount);
                           
                           vmatch = T1ReadWord(ARM9Mem.ARM9_REG, 4);
                           if((nds.VCount==(vmatch>>8)|((vmatch<<1)&(1<<8))))
                           {
				T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) | 4);
                                if(T1ReadWord(ARM9Mem.ARM9_REG, 4) & 32)
                                     NDS_makeARM9Int(2);
                           }
                           else
			        T1WriteWord(ARM9Mem.ARM9_REG, 4, T1ReadWord(ARM9Mem.ARM9_REG, 4) & 0xFFFB);
                           
                           vmatch = T1ReadWord(MMU.ARM7_REG, 4);
                           if((nds.VCount==(vmatch>>8)|((vmatch<<1)&(1<<8))))
                           {
				T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) | 4);
                                if(T1ReadWord(MMU.ARM7_REG, 4) & 32)
                                     NDS_makeARM7Int(2);
                           }
                           else
			        T1WriteWord(MMU.ARM7_REG, 4, T1ReadWord(MMU.ARM7_REG, 4) & 0xFFFB);
                      }
                 }
                 if(MMU.timerON[0][0])
                 {
                      if(MMU.timerRUN[0][0])
                      {
                           switch(MMU.timerMODE[0][0])
                           {
                                case 0xFFFF :
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[0][0])>>MMU.timerMODE[0][0];
                                          nds.old = MMU.timer[0][0];
                                          MMU.timer[0][0] += nds.diff;
                                          nds.timerCycle[0][0] += (nds.diff << MMU.timerMODE[0][0]);
                                          nds.timerOver[0][0] = nds.old>MMU.timer[0][0];
                                          if(nds.timerOver[0][0])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x102) & 0x40)
                                                    NDS_makeARM9Int(3);
                                               MMU.timer[0][0] += MMU.timerReload[0][0];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[0][0] = TRUE;
                           nds.timerCycle[0][0] = nds.cycles;
                      }
                 }
                 if(MMU.timerON[0][1])
                 {
                      if(MMU.timerRUN[0][1])
                      {
                           switch(MMU.timerMODE[0][1])
                           {
                                case 0xFFFF :
                                     if(nds.timerOver[0][0])
                                     {
                                          ++(MMU.timer[0][1]);
                                          nds.timerOver[0][1] = !MMU.timer[0][1];
                                          if (nds.timerOver[0][1])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x106) & 0x40)
                                                    NDS_makeARM9Int(4);
                                          }
                                     }
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[0][1])>>MMU.timerMODE[0][1];
                                          nds.old = MMU.timer[0][1];
                                          MMU.timer[0][1] += nds.diff;
                                          nds.timerCycle[0][1] += nds.diff << MMU.timerMODE[0][1];
                                          nds.timerOver[0][1] = nds.old>MMU.timer[0][1];
                                          if(nds.timerOver[0][1])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x106) & 0x40)
                                                    NDS_makeARM9Int(4);
                                               MMU.timer[0][1] += MMU.timerReload[0][1];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[0][1] = TRUE;
                           nds.timerCycle[0][1] = nds.cycles;
                      }
                 }
                 if(MMU.timerON[0][2])
                 {
                      if(MMU.timerRUN[0][2])
                      {
                           switch(MMU.timerMODE[0][2])
                           {
                                case 0xFFFF :
                                     if(nds.timerOver[0][1])
                                     {
                                          ++(MMU.timer[0][2]);
                                          nds.timerOver[0][2] = !MMU.timer[0][2];
                                          if (nds.timerOver[0][2])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10A) & 0x40)
                                                    NDS_makeARM9Int(5);
                                          }
                                     }
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[0][2])>>MMU.timerMODE[0][2];
                                          nds.old = MMU.timer[0][2];
                                          MMU.timer[0][2] += nds.diff;
                                          nds.timerCycle[0][2] += nds.diff << MMU.timerMODE[0][2];
                                          nds.timerOver[0][2] = nds.old>MMU.timer[0][2];
                                          if(nds.timerOver[0][2])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10A) & 0x40)
                                                    NDS_makeARM9Int(5);
                                               MMU.timer[0][2] += MMU.timerReload[0][2];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[0][2] = TRUE;
                           nds.timerCycle[0][2] = nds.cycles;
                      }
                 }
                 if(MMU.timerON[0][3])
                 {
                      if(MMU.timerRUN[0][3])
                      {
                           switch(MMU.timerMODE[0][3])
                           {
                                case 0xFFFF :
                                     if(nds.timerOver[0][2])
                                     {
                                          ++(MMU.timer[0][3]);
                                          nds.timerOver[0][3] = !MMU.timer[0][3];
                                          if (nds.timerOver[0][3])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10E) & 0x40)
                                                    NDS_makeARM9Int(6);
                                          }
                                     }
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[0][3])>>MMU.timerMODE[0][3];
                                          nds.old = MMU.timer[0][3];
                                          MMU.timer[0][3] += nds.diff;
                                          nds.timerCycle[0][3] += nds.diff << MMU.timerMODE[0][3];
                                          nds.timerOver[0][3] = nds.old>MMU.timer[0][3];
                                          if(nds.timerOver[0][3])
                                          {
					       if(T1ReadWord(ARM9Mem.ARM9_REG, 0x10E) & 0x40)
                                                    NDS_makeARM9Int(6);
                                               MMU.timer[0][3] += MMU.timerReload[0][3];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[0][3] = TRUE;
                           nds.timerCycle[0][3] = nds.cycles;
                      }
                 }
          
                 if(MMU.timerON[1][0])
                 {
                      if(MMU.timerRUN[1][0])
                      {
                           switch(MMU.timerMODE[1][0])
                           {
                                case 0xFFFF :
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[1][0])>>MMU.timerMODE[1][0];
                                          nds.old = MMU.timer[1][0];
                                          MMU.timer[1][0] += nds.diff;
                                          nds.timerCycle[1][0] += nds.diff << MMU.timerMODE[1][0];
                                          nds.timerOver[1][0] = nds.old>MMU.timer[1][0];
                                          if(nds.timerOver[1][0])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x102) & 0x40)
                                                    NDS_makeARM7Int(3);
                                               MMU.timer[1][0] += MMU.timerReload[1][0];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[1][0] = TRUE;
                           nds.timerCycle[1][0] = nds.cycles;
                      }
                 }
                 if(MMU.timerON[1][1])
                 {
                      if(MMU.timerRUN[1][1])
                      {
                           switch(MMU.timerMODE[1][1])
                           {
                                case 0xFFFF :
                                     if(nds.timerOver[1][0])
                                     {
                                          ++(MMU.timer[1][1]);
                                          nds.timerOver[1][1] = !MMU.timer[1][1];
                                          if (nds.timerOver[1][1])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x106) & 0x40)
                                                    NDS_makeARM7Int(4);
                                          }
                                     }
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[1][1])>>MMU.timerMODE[1][1];
                                          nds.old = MMU.timer[1][1];
                                          MMU.timer[1][1] += nds.diff;
                                          nds.timerCycle[1][1] += nds.diff << MMU.timerMODE[1][1];
                                          nds.timerOver[1][1] = nds.old>MMU.timer[1][1];
                                          if(nds.timerOver[1][1])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x106) & 0x40)
                                                    NDS_makeARM7Int(4);
                                               MMU.timer[1][1] += MMU.timerReload[1][1];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[1][1] = TRUE;
                           nds.timerCycle[1][1] = nds.cycles;
                      }
                 }
                 if(MMU.timerON[1][2])
                 {
                      if(MMU.timerRUN[1][2])
                      {
                           switch(MMU.timerMODE[1][2])
                           {
                                case 0xFFFF :
                                     if(nds.timerOver[1][1])
                                     {
                                          ++(MMU.timer[1][2]);
                                          nds.timerOver[1][2] = !MMU.timer[1][2];
                                          if (nds.timerOver[1][2])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x10A) & 0x40)
                                                    NDS_makeARM7Int(5);
                                          }
                                     }
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[1][2])>>MMU.timerMODE[1][2];
                                          nds.old = MMU.timer[1][2];
                                          MMU.timer[1][2] += nds.diff;
                                          nds.timerCycle[1][2] += nds.diff << MMU.timerMODE[1][2];
                                          nds.timerOver[1][2] = nds.old>MMU.timer[1][2];
                                          if(nds.timerOver[1][2])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x10A) & 0x40)
                                                    NDS_makeARM7Int(5);
                                               MMU.timer[1][2] += MMU.timerReload[1][2];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[1][2] = TRUE;
                           nds.timerCycle[1][2] = nds.cycles;
                      }
                 }
                 if(MMU.timerON[1][3])
                 {
                      if(MMU.timerRUN[1][3])
                      {
                           switch(MMU.timerMODE[1][3])
                           {
                                case 0xFFFF :
                                     if(nds.timerOver[1][2])
                                     {
                                          ++(MMU.timer[1][3]);
                                          nds.timerOver[1][3] = !MMU.timer[1][3];
                                          if (nds.timerOver[1][3])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x10E) & 0x40)
                                                    NDS_makeARM7Int(6);
                                          }
                                     }
                                     break;
                                default :
                                     {
                                          nds.diff = (nds.cycles - nds.timerCycle[1][3])>>MMU.timerMODE[1][3];
                                          nds.old = MMU.timer[1][3];
                                          MMU.timer[1][3] += nds.diff;
                                          nds.timerCycle[1][3] += nds.diff << MMU.timerMODE[1][3];
                                          nds.timerOver[1][3] = nds.old>MMU.timer[1][3];
                                          if(nds.timerOver[1][3])
                                          {
					       if(T1ReadWord(MMU.ARM7_REG, 0x10E) & 0x40)
                                                    NDS_makeARM7Int(6);
                                               MMU.timer[1][3] += MMU.timerReload[1][3];
                                          }
                                     }
                                     break;
                           }
                      }
                      else
                      {
                           MMU.timerRUN[1][3] = TRUE;
                           nds.timerCycle[1][3] = nds.cycles;
                      }
                 }
                 
                 if((MMU.DMAing[0][0])&&(MMU.DMACycle[0][0]<=nds.cycles))
                 {
		      T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*0), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*0)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[0][0])&(1<<30)) NDS_makeARM9Int(8);
                      MMU.DMAing[0][0] = FALSE;
                 }
                 
                 if((MMU.DMAing[0][1])&&(MMU.DMACycle[0][1]<=nds.cycles))
                 {
		      T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*1), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*1)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[0][1])&(1<<30)) NDS_makeARM9Int(9);
                      MMU.DMAing[0][1] = FALSE;
                 }
                 
                 if((MMU.DMAing[0][2])&&(MMU.DMACycle[0][2]<=nds.cycles))
                 {
		      T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*2), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*2)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[0][2])&(1<<30)) NDS_makeARM9Int(10);
                      MMU.DMAing[0][2] = FALSE;
                 }
                 
                 if((MMU.DMAing[0][3])&&(MMU.DMACycle[0][3]<=nds.cycles))
                 {
		      T1WriteLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*3), T1ReadLong(ARM9Mem.ARM9_REG, 0xB8 + (0xC*3)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[0][3])&(1<<30)) NDS_makeARM9Int(11);
                      MMU.DMAing[0][3] = FALSE;
                 }
                 
                 if((MMU.DMAing[1][0])&&(MMU.DMACycle[1][0]<=nds.cycles))
                 {
		      T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*0), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*0)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[1][0])&(1<<30)) NDS_makeARM7Int(8);
                      MMU.DMAing[1][0] = FALSE;
                 }
                 
                 if((MMU.DMAing[1][1])&&(MMU.DMACycle[1][1]<=nds.cycles))
                 {
		      T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*1), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*1)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[1][1])&(1<<30)) NDS_makeARM7Int(9);
                      MMU.DMAing[1][1] = FALSE;
                 }
                 
                 if((MMU.DMAing[1][2])&&(MMU.DMACycle[1][2]<=nds.cycles))
                 {
		      T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*2), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*2)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[1][2])&(1<<30)) NDS_makeARM7Int(10);
                      MMU.DMAing[1][2] = FALSE;
                 }
                 
                 if((MMU.DMAing[1][3])&&(MMU.DMACycle[1][3]<=nds.cycles))
                 {
		      T1WriteLong(MMU.ARM7_REG, 0xB8 + (0xC*3), T1ReadLong(MMU.ARM7_REG, 0xB8 + (0xC*3)) & 0x7FFFFFFF);
                      if((MMU.DMACrt[1][3])&(1<<30)) NDS_makeARM7Int(11);
                      MMU.DMAing[1][3] = FALSE;
                 }
                 
                 if((MMU.reg_IF[0]&MMU.reg_IE[0]) && (MMU.reg_IME[0]))
                      //if(NDS_ARM9.irqExeption())
		      if(armcpu_irqExeption(&NDS_ARM9))
                      {
                           nds.ARM9Cycle = nds.cycles;
                      }
                      
                 if((MMU.reg_IF[1]&MMU.reg_IE[1]) && (MMU.reg_IME[1]))
                      if (armcpu_irqExeption(&NDS_ARM7))
                           nds.ARM7Cycle = nds.cycles;

            }
            return nds.cycles;
       }

#ifdef __cplusplus
}
#endif

#endif
