/*	Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com 

	Copyright (C) 2007 shash
	Copyright (C) 2007-2009 DeSmuME team

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

#ifndef MMU_H
#define MMU_H

#include "FIFO.h"
#include "dscard.h"

#include "ARM9.h"
#include "mc.h"

/* theses macros are designed for reading/writing in memory (m is a pointer to memory, like MMU.MMU_MEM[proc], and a is an address, like 0x04000000 */
#define MEM_8(m, a)  (((u8*)(m[((a)>>20)&0xff]))[((a)&0xfff)])

/* theses ones for reading in rom data */
#define ROM_8(m, a)  (((u8*)(m))[(a)])

typedef const u32 TWaitState;

struct MMU_struct {
    //ARM7 mem
    u8 ARM7_BIOS[0x4000];
    u8 ARM7_ERAM[0x10000];
    u8 ARM7_REG[0x10000];
    u8 ARM7_WIRAM[0x10000];
        
	// VRAM mapping
	u8	VRAM_MAP[4][32];
	u32	LCD_VRAM_ADDR[10];
	u8	LCDCenable[10];

    //Shared ram
    u8 SWIRAM[0x8000];
    
    //Card rom & ram
    u8 * CART_ROM;
    u8 CART_RAM[0x10000];

	//Unused ram
	u8 UNUSED_RAM[4];

	//this is here so that we can trap glitchy emulator code
	//which is accessing offsets 5,6,7 of unused ram due to unaligned accesses
	//(also since the emulator doesn't prevent unaligned accesses)
	u8 MORE_UNUSED_RAM[4];
        
    static u8 * MMU_MEM[2][256];
    static u32 MMU_MASK[2][256];
    
    u8 ARM9_RW_MODE;

	static TWaitState MMU_WAIT16[2][16];
    static TWaitState MMU_WAIT32[2][16];

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

	BOOL divRunning;
	s64 divResult;
	s64 divMod;
	u32 divCnt;
	s32 divCycles;

	BOOL sqrtRunning;
	u32 sqrtResult;
	u32 sqrtCnt;
	s32 sqrtCycles;

	u8 powerMan_CntReg;
	BOOL powerMan_CntRegWritten;
	u8 powerMan_Reg[4];
	  
    memory_chip_t fw;
    memory_chip_t bupmem;
	  
    nds_dscard	dscard[2];
	u32			CheckTimers;
	u32			CheckDMAs;
		  
};

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


void mmu_select_savetype(int type, int *bmemtype, u32 *bmemsize);

void MMU_Init(void);
void MMU_DeInit(void);

void MMU_clearMem( void);

void MMU_setRom(u8 * rom, u32 mask);
void MMU_unsetRom( void);

void print_memory_profiling( void);

// Memory reading/writing (old)
u8 FASTCALL MMU_read8(u32 proc, u32 adr);
u16 FASTCALL MMU_read16(u32 proc, u32 adr);
u32 FASTCALL MMU_read32(u32 proc, u32 adr);
void FASTCALL MMU_write8(u32 proc, u32 adr, u8 val);
void FASTCALL MMU_write16(u32 proc, u32 adr, u16 val);
void FASTCALL MMU_write32(u32 proc, u32 adr, u32 val);
 
template<int PROCNUM> void FASTCALL MMU_doDMA(u32 num);

/*
 * The base ARM memory interfaces
 */
extern struct armcpu_memory_iface arm9_base_memory_iface;
extern struct armcpu_memory_iface arm7_base_memory_iface;
extern struct armcpu_memory_iface arm9_direct_memory_iface;	

extern u8 *MMU_RenderMapToLCD(u32 vram_addr);

template<int PROCNUM> u8 _MMU_read08(u32 addr);
template<int PROCNUM> u16 _MMU_read16(u32 addr);
template<int PROCNUM> u32 _MMU_read32(u32 addr);
template<int PROCNUM> void _MMU_write08(u32 addr, u8 val);
template<int PROCNUM> void _MMU_write16(u32 addr, u16 val);
template<int PROCNUM> void _MMU_write32(u32 addr, u32 val);

u8 _MMU_read08(int PROCNUM, u32 addr);
u16 _MMU_read16(int PROCNUM, u32 addr);
u32 _MMU_read32(int PROCNUM, u32 addr);
void _MMU_write08(int PROCNUM, u32 addr, u8 val);
void _MMU_write16(int PROCNUM, u32 addr, u16 val);
void _MMU_write32(int PROCNUM, u32 addr, u32 val);

#ifdef MMU_ENABLE_ACL
	void FASTCALL MMU_write8_acl(u32 proc, u32 adr, u8 val);
	void FASTCALL MMU_write16_acl(u32 proc, u32 adr, u16 val);
	void FASTCALL MMU_write32_acl(u32 proc, u32 adr, u32 val);
	u8 FASTCALL MMU_read8_acl(u32 proc, u32 adr, u32 access);
	u16 FASTCALL MMU_read16_acl(u32 proc, u32 adr, u32 access);
	u32 FASTCALL MMU_read32_acl(u32 proc, u32 adr, u32 access);
#else
	#define MMU_write8_acl(proc, adr, val)  _MMU_write08<proc>(adr, val)
	#define MMU_write16_acl(proc, adr, val) _MMU_write16<proc>(adr, val)
	#define MMU_write32_acl(proc, adr, val) _MMU_write32<proc>(adr, val)
	#define MMU_read8_acl(proc,adr,access)  _MMU_read08<proc>(adr)
	#define MMU_read16_acl(proc,adr,access) _MMU_read16<proc>(adr)
	#define MMU_read32_acl(proc,adr,access) _MMU_read32<proc>(adr)
#endif

// Use this macros for reading/writing, so the GDB stub isn't broken
#ifdef GDB_STUB
	#define READ32(a,b)		cpu->mem_if->read32(a,(b) & 0xFFFFFFFC)
	#define WRITE32(a,b,c)	cpu->mem_if->write32(a,(b) & 0xFFFFFFFC,c)
	#define READ16(a,b)		cpu->mem_if->read16(a,(b) & 0xFFFFFFFE)
	#define WRITE16(a,b,c)	cpu->mem_if->write16(a,(b) & 0xFFFFFFFE,c)
	#define READ8(a,b)		cpu->mem_if->read8(a,b)
	#define WRITE8(a,b,c)	cpu->mem_if->write8(a,b,c)
#else
	#define READ32(a,b)		_MMU_read32<PROCNUM>((b) & 0xFFFFFFFC)
	#define WRITE32(a,b,c)	_MMU_write32<PROCNUM>((b) & 0xFFFFFFFC,c)
	#define READ16(a,b)		_MMU_read16<PROCNUM>((b) & 0xFFFFFFFE)
	#define WRITE16(a,b,c)	_MMU_write16<PROCNUM>((b) & 0xFFFFFFFE,c)
	#define READ8(a,b)		_MMU_read08<PROCNUM>(b)
	#define WRITE8(a,b,c)	_MMU_write08<PROCNUM>(b, c)
#endif

template<int PROCNUM> 
u8 _MMU_read08(u32 addr) { return _MMU_read08(PROCNUM, addr); }

template<int PROCNUM> 
u16 _MMU_read16(u32 addr) { return _MMU_read16(PROCNUM, addr); }

template<int PROCNUM> 
u32 _MMU_read32(u32 addr) { return _MMU_read32(PROCNUM, addr); }

template<int PROCNUM> 
void _MMU_write08(u32 addr, u8 val) { _MMU_write08(PROCNUM, addr, val); }

template<int PROCNUM> 
void _MMU_write16(u32 addr, u16 val) { _MMU_write16(PROCNUM, addr, val); }

template<int PROCNUM> 
void _MMU_write32(u32 addr, u32 val) { _MMU_write32(PROCNUM, addr, val); }

#endif
