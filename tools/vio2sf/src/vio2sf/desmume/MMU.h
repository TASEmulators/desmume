/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright (C) 2007 shash

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

#ifndef MMU_H
#define MMU_H

#include "FIFO.h"
#include "dscard.h"

#include "ARM9.h"
#include "mc.h"

#ifdef __cplusplus
extern "C" {
#endif

extern char szRomPath[512];
extern char szRomBaseName[512];

/* theses macros are designed for reading/writing in memory (m is a pointer to memory, like MMU.MMU_MEM[proc], and a is an adress, like 0x04000000 */
#define MEM_8(m, a)  (((u8*)(m[((a)>>20)&0xff]))[((a)&0xfff)])

/* theses ones for reading in rom data */
#define ROM_8(m, a)  (((u8*)(m))[(a)])
 
#define IPCFIFO  0
#define MAIN_MEMORY_DISP_FIFO 2
 
typedef struct {
        //ARM7 mem
        u8 ARM7_BIOS[0x4000];
        u8 ARM7_ERAM[0x10000];
        u8 ARM7_REG[0x10000];
        u8 ARM7_WIRAM[0x10000];
        
	u8 vram_mode[9];
	u8 vScreen;

        //Shared ram
        u8 SWIRAM[0x8000];
        
        //Card rom & ram
        u8 * CART_ROM;
        u8 CART_RAM[0x10000];

	//Unused ram
	u8 UNUSED_RAM[4];
        
        u8 * * MMU_MEM[2];
        u32 * MMU_MASK[2];
        
        u8 ARM9_RW_MODE;
        
        FIFO fifos[16];

        u32 * MMU_WAIT16[2];
        u32 * MMU_WAIT32[2];

        u32 DTCMRegion;
        u32 ITCMRegion;
        
        u16 timer[2][4];
        s32 timerMODE[2][4];
        u32 timerON[2][4];
        u32 timerRUN[2][4];
        u16 timerReload[2][4];
        
        u32 reg_IME[2];
        u32 reg_IE[2];
        u32 reg_IF[2];
        
        u32 DMAStartTime[2][4];
        s32 DMACycle[2][4];
        u32 DMACrt[2][4];
        BOOL DMAing[2][4];
		  
        memory_chip_t fw;
        memory_chip_t bupmem;
		  
        nds_dscard	dscard[2];
		u32			CheckTimers;
		u32			CheckDMAs;
		  
} MMU_struct;

extern MMU_struct MMU;


struct armcpu_memory_iface {
  /** the 32 bit instruction prefetch */
  u32 FASTCALL (*prefetch32)( void *data, u32 adr);

  /** the 16 bit instruction prefetch */
  u16 FASTCALL (*prefetch16)( void *data, u32 adr);

  /** read 8 bit data value */
  u8 FASTCALL (*read8)( void *data, u32 adr);
  /** read 16 bit data value */
  u16 FASTCALL (*read16)( void *data, u32 adr);
  /** read 32 bit data value */
  u32 FASTCALL (*read32)( void *data, u32 adr);

  /** write 8 bit data value */
  void FASTCALL (*write8)( void *data, u32 adr, u8 val);
  /** write 16 bit data value */
  void FASTCALL (*write16)( void *data, u32 adr, u16 val);
  /** write 32 bit data value */
  void FASTCALL (*write32)( void *data, u32 adr, u32 val);

  void *data;
};



static void mmu_select_savetype(int type, int *bmemtype, u32 *bmemsize) {
	if (type<0 || type > 5) return;
	*bmemtype=save_types[type][0];
	*bmemsize=save_types[type][1];
	mc_realloc(&MMU.bupmem, *bmemtype, *bmemsize);
}

void MMU_Init(void);
void MMU_DeInit(void);

void MMU_clearMem( void);

void MMU_setRom(u8 * rom, u32 mask);
void MMU_unsetRom( void);


/**
 * Memory reading
 */
u8 FASTCALL MMU_read8(u32 proc, u32 adr);
u16 FASTCALL MMU_read16(u32 proc, u32 adr);
u32 FASTCALL MMU_read32(u32 proc, u32 adr);

#ifdef MMU_ENABLE_ACL
	u8 FASTCALL MMU_read8_acl(u32 proc, u32 adr, u32 access);
	u16 FASTCALL MMU_read16_acl(u32 proc, u32 adr, u32 access);
	u32 FASTCALL MMU_read32_acl(u32 proc, u32 adr, u32 access);
#else
	#define MMU_read8_acl(proc,adr,access)  MMU_read8(proc,adr)
	#define MMU_read16_acl(proc,adr,access)  MMU_read16(proc,adr)
	#define MMU_read32_acl(proc,adr,access)  MMU_read32(proc,adr)
#endif

/**
 * Memory writing
 */
void FASTCALL MMU_write8(u32 proc, u32 adr, u8 val);
void FASTCALL MMU_write16(u32 proc, u32 adr, u16 val);
void FASTCALL MMU_write32(u32 proc, u32 adr, u32 val);

#ifdef MMU_ENABLE_ACL
	void FASTCALL MMU_write8_acl(u32 proc, u32 adr, u8 val);
	void FASTCALL MMU_write16_acl(u32 proc, u32 adr, u16 val);
	void FASTCALL MMU_write32_acl(u32 proc, u32 adr, u32 val);
#else
	#define MMU_write8_acl MMU_write8
	#define MMU_write16_acl MMU_write16
	#define MMU_write32_acl MMU_write32
#endif
 
void FASTCALL MMU_doDMA(u32 proc, u32 num);


/*
 * The base ARM memory interfaces
 */
extern struct armcpu_memory_iface arm9_base_memory_iface;
extern struct armcpu_memory_iface arm7_base_memory_iface;
extern struct armcpu_memory_iface arm9_direct_memory_iface;	


#ifdef __cplusplus
}
#endif

#endif
