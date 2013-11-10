/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2007 shash
	Copyright (C) 2007-2013 DeSmuME team

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

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>
#include <sstream>

#include "common.h"
#include "debug.h"
#include "NDSSystem.h"
#include "cp15.h"
#include "wifi.h"
#include "registers.h"
#include "render3D.h"
#include "gfx3d.h"
#include "rtc.h"
#include "mc.h"
#include "slot1.h"
#include "slot2.h"
#include "mic.h"
#include "movie.h"
#include "readwrite.h"
#include "MMU_timing.h"
#include "firmware.h"
#include "encrypt.h"

#ifdef DO_ASSERT_UNALIGNED
#define ASSERT_UNALIGNED(x) assert(x)
#else
#define ASSERT_UNALIGNED(x)
#endif

//TODO - do we need these here?
_KEY2 key2;

//http://home.utah.edu/~nahaj/factoring/isqrt.c.html
static u64 isqrt (u64 x) {
  u64   squaredbit, remainder, root;

   if (x<1) return 0;
  
   /* Load the binary constant 01 00 00 ... 00, where the number
    * of zero bits to the right of the single one bit
    * is even, and the one bit is as far left as is consistant
    * with that condition.)
    */
   squaredbit  = (u64) ((((u64) ~0LL) >> 1) & 
                        ~(((u64) ~0LL) >> 2));
   /* This portable load replaces the loop that used to be 
    * here, and was donated by  legalize@xmission.com 
    */

   /* Form bits of the answer. */
   remainder = x;  root = 0;
   while (squaredbit > 0) {
     if (remainder >= (squaredbit | root)) {
         remainder -= (squaredbit | root);
         root >>= 1; root |= squaredbit;
     } else {
         root >>= 1;
     }
     squaredbit >>= 2; 
   }

   return root;
}

u32 partie = 1;
u32 _MMU_MAIN_MEM_MASK = 0x3FFFFF;
u32 _MMU_MAIN_MEM_MASK16 = 0x3FFFFF & ~1;
u32 _MMU_MAIN_MEM_MASK32 = 0x3FFFFF & ~3;

//#define	_MMU_DEBUG

#ifdef _MMU_DEBUG

#include <stdarg.h>
void mmu_log_debug_ARM9(u32 adr, const char *fmt, ...)
{
	if (adr < 0x4000000) return;
//	if (adr > 0x4100014) return;
//#if 1
	if (adr >= 0x4000000 && adr <= 0x400006E) return;		// Display Engine A
	if (adr >= 0x40000B0 && adr <= 0x4000134) return;		// DMA, Timers and Keypad
	if (adr >= 0x4000180 && adr <= 0x40001BC) return;		// IPC/ROM
	if (adr >= 0x4000204 && adr <= 0x400024A) return;		// Memory & IRQ control
	if (adr >= 0x4000280 && adr <= 0x4000306) return;		// Maths
	if (adr >= 0x4000320 && adr <= 0x40006A3) return;		// 3D dispaly engine
	if (adr >= 0x4001000 && adr <= 0x400106E) return;		// Display Engine B
	if (adr >= 0x4100000 && adr <= 0x4100014) return;		// IPC/ROM
//#endif
	va_list list;
	char msg[512];

	memset(msg,0,512);

	va_start(list,fmt);
		_vsnprintf(msg,511,fmt,list);
	va_end(list);

	INFO("MMU ARM9 0x%08X: %s\n", adr, msg);
}

void mmu_log_debug_ARM7(u32 adr, const char *fmt, ...)
{
	if (adr < 0x4000004) return;
	if (adr > 0x4808FFF) return;
#if 1
	if (adr >= 0x4000004 && adr < 0x4000180) return;		// ARM7 I/O Map
	if (adr >= 0x4000180 && adr <= 0x40001C4) return;		// IPC/ROM
	if (adr >= 0x4000204 && adr <= 0x400030C) return;		// Memory and IRQ Control
	if (adr >= 0x4000400 && adr <= 0x400051E) return;		// Sound Registers
	if (adr >= 0x4100000 && adr <= 0x4100014) return;		// IPC/ROM
	if (adr >= 0x4800000 && adr <= 0x4808FFF) return;		// WLAN Registers
#endif
	va_list list;
	char msg[512];

	memset(msg,0,512);

	va_start(list,fmt);
		_vsnprintf(msg,511,fmt,list);
	va_end(list);

	INFO("MMU ARM7 0x%08X: %s\n", adr, msg);

}
#else
#define mmu_log_debug_ARM9(...)
#define mmu_log_debug_ARM7(...)
#endif


//#define LOG_CARD
//#define LOG_GPU
//#define LOG_DMA
//#define LOG_DMA2
//#define LOG_DIV

MMU_struct MMU;
MMU_struct_new MMU_new;
MMU_struct_timing MMU_timing;

u8 * MMU_struct::MMU_MEM[2][256] = {
	//arm9
	{
		/* 0X*/	DUP16(MMU.ARM9_ITCM), 
		/* 1X*/	//DUP16(MMU.ARM9_ITCM)
		/* 1X*/	DUP16(MMU.UNUSED_RAM), 
		/* 2X*/	DUP16(MMU.MAIN_MEM),
		/* 3X*/	DUP16(MMU.SWIRAM),
		/* 4X*/	DUP16(MMU.ARM9_REG),
		/* 5X*/	DUP16(MMU.ARM9_VMEM),
		/* 6X*/	DUP16(MMU.ARM9_LCD),
		/* 7X*/	DUP16(MMU.ARM9_OAM),
		/* 8X*/	DUP16(NULL),
		/* 9X*/	DUP16(NULL),
		/* AX*/	DUP16(MMU.UNUSED_RAM),
		/* BX*/	DUP16(MMU.UNUSED_RAM),
		/* CX*/	DUP16(MMU.UNUSED_RAM),
		/* DX*/	DUP16(MMU.UNUSED_RAM),
		/* EX*/	DUP16(MMU.UNUSED_RAM),
		/* FX*/	DUP16(MMU.ARM9_BIOS)
	},
	//arm7
	{
		/* 0X*/	DUP16(MMU.ARM7_BIOS), 
		/* 1X*/	DUP16(MMU.UNUSED_RAM), 
		/* 2X*/	DUP16(MMU.MAIN_MEM),
		/* 3X*/	DUP8(MMU.SWIRAM),
				DUP8(MMU.ARM7_ERAM),
		/* 4X*/	DUP8(MMU.ARM7_REG),
				DUP8(MMU.ARM7_WIRAM),
		/* 5X*/	DUP16(MMU.UNUSED_RAM),
		/* 6X*/	DUP16(MMU.ARM9_LCD),
		/* 7X*/	DUP16(MMU.UNUSED_RAM),
		/* 8X*/	DUP16(NULL),
		/* 9X*/	DUP16(NULL),
		/* AX*/	DUP16(MMU.UNUSED_RAM),
		/* BX*/	DUP16(MMU.UNUSED_RAM),
		/* CX*/	DUP16(MMU.UNUSED_RAM),
		/* DX*/	DUP16(MMU.UNUSED_RAM),
		/* EX*/	DUP16(MMU.UNUSED_RAM),
		/* FX*/	DUP16(MMU.UNUSED_RAM)
		}
};

u32 MMU_struct::MMU_MASK[2][256] = {
	//arm9
	{
		/* 0X*/	DUP16(0x00007FFF), 
		/* 1X*/	//DUP16(0x00007FFF)
		/* 1X*/	DUP16(0x00000003),
		/* 2X*/	DUP16(0x003FFFFF),
		/* 3X*/	DUP16(0x00007FFF),
		/* 4X*/	DUP16(0x00FFFFFF),
		/* 5X*/	DUP16(0x000007FF),
		/* 6X*/	DUP16(0x00FFFFFF),
		/* 7X*/	DUP16(0x000007FF),
		/* 8X*/	DUP16(0x00000003),
		/* 9X*/	DUP16(0x00000003),
		/* AX*/	DUP16(0x00000003),
		/* BX*/	DUP16(0x00000003),
		/* CX*/	DUP16(0x00000003),
		/* DX*/	DUP16(0x00000003),
		/* EX*/	DUP16(0x00000003),
		/* FX*/	DUP16(0x00007FFF)
	},
	//arm7
	{
		/* 0X*/	DUP16(0x00003FFF), 
		/* 1X*/	DUP16(0x00000003),
		/* 2X*/	DUP16(0x003FFFFF),
		/* 3X*/	DUP8(0x00007FFF),
				DUP8(0x0000FFFF),
		/* 4X*/	DUP8(0x00FFFFFF),
				DUP8(0x0000FFFF),
		/* 5X*/	DUP16(0x00000003),
		/* 6X*/	DUP16(0x00FFFFFF),
		/* 7X*/	DUP16(0x00000003),
		/* 8X*/	DUP16(0x00000003),
		/* 9X*/	DUP16(0x00000003),
		/* AX*/	DUP16(0x00000003),
		/* BX*/	DUP16(0x00000003),
		/* CX*/	DUP16(0x00000003),
		/* DX*/	DUP16(0x00000003),
		/* EX*/	DUP16(0x00000003),
		/* FX*/	DUP16(0x00000003)
		}
};

// this logic was moved to MMU_timing.h
//CACHE_ALIGN
//TWaitState MMU_struct::MMU_WAIT16[2][16] = {
//	{ 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1 }, //arm9
//	{ 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1 }, //arm7
//};
//
//CACHE_ALIGN
//TWaitState MMU_struct::MMU_WAIT32[2][16] = {
//	{ 1, 1, 1, 1, 1, 2, 2, 1, 8, 8, 5, 1, 1, 1, 1, 1 }, //arm9
//	{ 1, 1, 1, 1, 1, 1, 1, 1, 8, 8, 5, 1, 1, 1, 1, 1 }, //arm7
//};

//////////////////////////////////////////////////////////////

//-------------
//VRAM MEMORY MAPPING
//-------------
//(Everything is mapped through to ARM9_LCD in blocks of 16KB)

//for all of the below, values = 41 indicate unmapped memory
#define VRAM_PAGE_UNMAPPED 41

#define VRAM_LCDC_PAGES 41
u8 vram_lcdc_map[VRAM_LCDC_PAGES];

//in the range of 0x06000000 - 0x06800000 in 16KB pages (the ARM9 vram mappable area)
//this maps to 16KB pages in the LCDC buffer which is what will actually contain the data
u8 vram_arm9_map[VRAM_ARM9_PAGES];

//this chooses which banks are mapped in the 128K banks starting at 0x06000000 in ARM7
u8 vram_arm7_map[2];

//----->
//consider these later, for better recordkeeping, instead of using the u8* in MMU

////for each 128KB texture slot, this maps to a 16KB starting page in the LCDC buffer
//#define VRAM_TEX_SLOTS 4
//u8 vram_tex_map[VRAM_TEX_SLOTS];
//
////for each 16KB tex palette slot, this maps to a 16KB starting page in the LCDC buffer
//#define VRAM_TEX_PALETTE_SLOTS 6
//u8 vram_tex_palette_map[VRAM_TEX_PALETTE_SLOTS];

//<---------


void MMU_VRAM_unmap_all();

struct TVramBankInfo {
	u8 page_addr, num_pages;
};

static const TVramBankInfo vram_bank_info[VRAM_BANKS] = {
	{0,8},
	{8,8},
	{16,8},
	{24,8},
	{32,4},
	{36,1},
	{37,1},
	{38,2},
	{40,1}
};

//this is to remind you that the LCDC mapping returns a strange value (not 0x06800000) as you would expect
//in order to play nicely with the MMU address and mask tables
#define LCDC_HACKY_LOCATION 0x06000000

#define ARM7_HACKY_IWRAM_LOCATION 0x03800000
#define ARM7_HACKY_SIWRAM_LOCATION 0x03000000

//maps an ARM9 BG/OBJ or LCDC address into an LCDC address, and informs the caller of whether it isn't mapped
//TODO - in cases where this does some mapping work, we could bypass the logic at the end of the _read* and _write* routines
//this is a good optimization to consider
//NOTE - this whole approach is probably fundamentally wrong.
//according to dasShiny research, its possible to map multiple banks to the same addresses. something more sophisticated would be needed.
//however, it hasnt proven necessary yet for any known test case.
template<int PROCNUM> 
static FORCEINLINE u32 MMU_LCDmap(u32 addr, bool& unmapped, bool& restricted)
{
	unmapped = false;
	restricted = false; //this will track whether 8bit writes are allowed

	//handle SIWRAM and non-shared IWRAM in here too, since it is quite similar to vram.
	//in fact it is probably implemented with the same pieces of hardware.
	//its sort of like arm7 non-shared IWRAM is lowest priority, and then SIWRAM goes on top.
	//however, we implement it differently than vram in emulator for historical reasons.
	//instead of keeping a page map like we do vram, we just have a list of all possible page maps (there are only 4 each for arm9 and arm7)
	if(addr >= 0x03000000 && addr < 0x04000000)
	{
		//blocks 0,1,2,3 is arm7 non-shared IWRAM and blocks 4,5 is SIWRAM, and block 8 is un-mapped zeroes
		int iwram_block_16k;
		int iwram_offset = addr & 0x3FFF;
		addr &= 0x00FFFFFF;
		if(PROCNUM == ARMCPU_ARM7)
		{
			static const int arm7_siwram_blocks[2][4][4] =
			{
				{
					{0,1,2,3}, //WRAMCNT = 0 -> map to IWRAM
					{4,4,4,4}, //WRAMCNT = 1 -> map to SIWRAM block 0
					{5,5,5,5}, //WRAMCNT = 2 -> map to SIWRAM block 1
					{4,5,4,5}, //WRAMCNT = 3 -> map to SIWRAM blocks 0,1
				},
					 //high region; always maps to non-shared IWRAM
				{
					{0,1,2,3}, {0,1,2,3}, {0,1,2,3}, {0,1,2,3}
				}
			};
			int region = (addr >> 23)&1;
			int block = (addr >> 14)&3;
			assert(region<2);
			assert(block<4);
			iwram_block_16k = arm7_siwram_blocks[region][MMU.WRAMCNT][block];
		} //PROCNUM == ARMCPU_ARM7
		else
		{
			//PROCNUM == ARMCPU_ARM9
			static const int arm9_siwram_blocks[4][4] =
			{
				{4,5,4,5}, //WRAMCNT = 0 -> map to SIWRAM blocks 0,1
				{5,5,5,5}, //WRAMCNT = 1 -> map to SIWRAM block 1
				{4,4,4,4}, //WRAMCNT = 2 -> map to SIWRAM block 0
				{8,8,8,8}, //WRAMCNT = 3 -> unmapped
			};
			int block = (addr >> 14)&3;
			assert(block<4);
			iwram_block_16k = arm9_siwram_blocks[MMU.WRAMCNT][block];
		}

		switch(iwram_block_16k>>2)
		{
		case 0: //arm7 non-shared IWRAM
			return ARM7_HACKY_IWRAM_LOCATION + (iwram_block_16k<<14) + iwram_offset;
		case 1: //SIWRAM
			return ARM7_HACKY_SIWRAM_LOCATION + ((iwram_block_16k&3)<<14) + iwram_offset;
		case 2: //zeroes
		CASE2:
			unmapped = true;
			return 0;
		default:
			assert(false); //how did this happen?
			goto CASE2;
		}
	}

	//in case the address is entirely outside of the interesting VRAM ranges
	if(addr < 0x06000000) return addr;
	if(addr >= 0x07000000) return addr;

	//shared wram mapping for arm7
	if(PROCNUM==ARMCPU_ARM7)
	{
		//necessary? not sure
		//addr &= 0x3FFFF;
		//addr += 0x06000000;
		u32 ofs = addr & 0x1FFFF;
		u32 bank = (addr >> 17)&1;
		if(vram_arm7_map[bank] == VRAM_PAGE_UNMAPPED)
		{
			unmapped = true;
			return 0;
		}
		return LCDC_HACKY_LOCATION + (vram_arm7_map[bank]<<14) + ofs;
	}

	restricted = true;

	//handle LCD memory mirroring
	//TODO - this is gross! this should be renovated if the vram mapping is ever done in a more sophisticated way taking into account dasShiny research
	if(addr>=0x068A4000)
		addr = 0x06800000 + 
		//(addr%0xA4000); //yuck!! is this even how it mirrors? but we have to keep from overrunning the buffer somehow
		(addr&0x80000); //just as likely to be right (I have no clue how it should work) but faster.

	u32 vram_page;
	u32 ofs = addr & 0x3FFF;

	//return addresses in LCDC range
	if(addr>=0x06800000) 
	{
		//already in LCDC range. just look it up to see whether it is unmapped
		vram_page = (addr>>14)&63;
		assert(vram_page<VRAM_LCDC_PAGES);
		vram_page = vram_lcdc_map[vram_page];
	}
	else
	{
		//map addresses in BG/OBJ range to an LCDC range
		vram_page = (addr>>14)&(VRAM_ARM9_PAGES-1);
		assert(vram_page<VRAM_ARM9_PAGES);
		vram_page = vram_arm9_map[vram_page];
	}

	if(vram_page == VRAM_PAGE_UNMAPPED)
	{
		unmapped = true;
		return 0;
	}
	else
		return LCDC_HACKY_LOCATION + (vram_page<<14) + ofs;
}


#define LOG_VRAM_ERROR() LOG("No data for block %i MST %i\n", block, VRAMBankCnt & 0x07);

VramConfiguration vramConfiguration;

std::string VramConfiguration::describePurpose(Purpose p) {
	switch(p) {
		case OFF: return "OFF";
		case INVALID: return "INVALID";
		case ABG: return "ABG";
		case BBG: return "BBG";
		case AOBJ: return "AOBJ";
		case BOBJ: return "BOBJ";
		case LCDC: return "LCDC";
		case ARM7: return "ARM7";
		case TEX: return "TEX";
		case TEXPAL: return "TEXPAL";
		case ABGEXTPAL: return "ABGEXTPAL";
		case BBGEXTPAL: return "BBGEXTPAL";
		case AOBJEXTPAL: return "AOBJEXTPAL";
		case BOBJEXTPAL: return "BOBJEXTPAL";
		default: return "UNHANDLED CASE";
	}
}

std::string VramConfiguration::describe() {
	std::stringstream ret;
	for(int i=0;i<VRAM_BANKS;i++) {
		ret << (char)(i+'A') << ": " << banks[i].ofs << " " << describePurpose(banks[i].purpose) << std::endl;
	}
	return ret.str();
}

//maps the specified bank to LCDC
static inline void MMU_vram_lcdc(const int bank)
{
	for(int i=0;i<vram_bank_info[bank].num_pages;i++)
	{
		int page = vram_bank_info[bank].page_addr+i;
		vram_lcdc_map[page] = page;
	}
}

//maps the specified bank to ARM9 at the provided page offset
static inline void MMU_vram_arm9(const int bank, const int offset)
{
	for(int i=0;i<vram_bank_info[bank].num_pages;i++)
	{
		int page = vram_bank_info[bank].page_addr+i;
		vram_arm9_map[i+offset] = page;
	}
}

static inline u8* MMU_vram_physical(const int page)
{
	return MMU.ARM9_LCD + (page*ADDRESS_STEP_16KB);
}

//todo - templateize
static inline void MMU_VRAMmapRefreshBank(const int bank)
{
	int block = bank;
	if(bank >= VRAM_BANK_H) block++;

	u8 VRAMBankCnt = T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x240 + block);
	
	//do nothing if the bank isnt enabled
	u8 en = VRAMBankCnt & 0x80;
	if(!en) return;

	int mst,ofs=0;
	switch(bank) {
		case VRAM_BANK_A:
		case VRAM_BANK_B:
			mst = VRAMBankCnt & 3;
			ofs = (VRAMBankCnt>>3) & 3;
			switch(mst)
			{
			case 0: //LCDC
				vramConfiguration.banks[bank].purpose = VramConfiguration::LCDC;
				MMU_vram_lcdc(bank);
				if(ofs != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
				break;
			case 1: //ABG
				vramConfiguration.banks[bank].purpose = VramConfiguration::ABG;
				MMU_vram_arm9(bank,VRAM_PAGE_ABG+ofs*8);
				break;
			case 2: //AOBJ
				vramConfiguration.banks[bank].purpose = VramConfiguration::AOBJ;
				switch(ofs) {
				case 0:
				case 1:
					MMU_vram_arm9(bank,VRAM_PAGE_AOBJ+ofs*8);
					break;
				default:
					PROGINFO("Unsupported ofs setting %d for engine A OBJ vram bank %c\n", ofs, 'A'+bank);
				}
				break;
			case 3: //texture
				vramConfiguration.banks[bank].purpose = VramConfiguration::TEX;
				MMU.texInfo.textureSlotAddr[ofs] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				break;
			default: goto unsupported_mst;
			}
			break;

		case VRAM_BANK_C:
		case VRAM_BANK_D:
			mst = VRAMBankCnt & 7;
			ofs = (VRAMBankCnt>>3) & 3;
			switch(mst)
			{
			case 0: //LCDC
				vramConfiguration.banks[bank].purpose = VramConfiguration::LCDC;
				MMU_vram_lcdc(bank);
				if(ofs != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
				break;
			case 1: //ABG
				vramConfiguration.banks[bank].purpose = VramConfiguration::ABG;
				MMU_vram_arm9(bank,VRAM_PAGE_ABG+ofs*8);
				break;
			case 2: //arm7
				vramConfiguration.banks[bank].purpose = VramConfiguration::ARM7;
				if(bank == 2) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) | 1);
				if(bank == 3) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) | 2);
				//printf("DING!\n");
				switch(ofs) {
				case 0:
				case 1:
					vram_arm7_map[ofs] = vram_bank_info[bank].page_addr;
					break;
				default:
					PROGINFO("Unsupported ofs setting %d for arm7 vram bank %c\n", ofs, 'A'+bank);
				}

				break;
			case 3: //texture
				vramConfiguration.banks[bank].purpose = VramConfiguration::TEX;
				MMU.texInfo.textureSlotAddr[ofs] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				break;
			case 4: //BGB or BOBJ
				if(bank == VRAM_BANK_C)  {
					vramConfiguration.banks[bank].purpose = VramConfiguration::BBG;
					MMU_vram_arm9(bank,VRAM_PAGE_BBG); //BBG
				} else {
					vramConfiguration.banks[bank].purpose = VramConfiguration::BOBJ;
					MMU_vram_arm9(bank,VRAM_PAGE_BOBJ); //BOBJ
				}
				if(ofs != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
				break;
			default: goto unsupported_mst;
			}
			break;

		case VRAM_BANK_E:
			mst = VRAMBankCnt & 7;
			if(((VRAMBankCnt>>3)&3) != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
			switch(mst) {
			case 0: //LCDC
				vramConfiguration.banks[bank].purpose = VramConfiguration::LCDC;
				MMU_vram_lcdc(bank);
				break;
			case 1: //ABG
				vramConfiguration.banks[bank].purpose = VramConfiguration::ABG;
				MMU_vram_arm9(bank,VRAM_PAGE_ABG);
				break;
			case 2: //AOBJ
				vramConfiguration.banks[bank].purpose = VramConfiguration::AOBJ;
				MMU_vram_arm9(bank,VRAM_PAGE_AOBJ);
				break;
			case 3: //texture palette
				vramConfiguration.banks[bank].purpose = VramConfiguration::TEXPAL;
				MMU.texInfo.texPalSlot[0] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				MMU.texInfo.texPalSlot[1] = MMU_vram_physical(vram_bank_info[bank].page_addr+1);
				MMU.texInfo.texPalSlot[2] = MMU_vram_physical(vram_bank_info[bank].page_addr+2);
				MMU.texInfo.texPalSlot[3] = MMU_vram_physical(vram_bank_info[bank].page_addr+3);
				break;
			case 4: //A BG extended palette
				vramConfiguration.banks[bank].purpose = VramConfiguration::ABGEXTPAL;
				MMU.ExtPal[0][0] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				MMU.ExtPal[0][1] = MMU.ExtPal[0][0] + ADDRESS_STEP_8KB;
				MMU.ExtPal[0][2] = MMU.ExtPal[0][1] + ADDRESS_STEP_8KB;
				MMU.ExtPal[0][3] = MMU.ExtPal[0][2] + ADDRESS_STEP_8KB;
				break;
			default: goto unsupported_mst;
			}
			break;
			
		case VRAM_BANK_F:
		case VRAM_BANK_G: {
			mst = VRAMBankCnt & 7;
			ofs = (VRAMBankCnt>>3) & 3;
			const int pageofslut[] = {0,1,4,5};
			const int pageofs = pageofslut[ofs];
			switch(mst)
			{
			case 0: //LCDC
				vramConfiguration.banks[bank].purpose = VramConfiguration::LCDC;
				MMU_vram_lcdc(bank);
				if(ofs != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
				break;
			case 1: //ABG
				vramConfiguration.banks[bank].purpose = VramConfiguration::ABG;
				MMU_vram_arm9(bank,VRAM_PAGE_ABG+pageofs);
				MMU_vram_arm9(bank,VRAM_PAGE_ABG+pageofs+2); //unexpected mirroring (required by spyro eternal night)
				break;
			case 2: //AOBJ
				vramConfiguration.banks[bank].purpose = VramConfiguration::AOBJ;
				MMU_vram_arm9(bank,VRAM_PAGE_AOBJ+pageofs);
				MMU_vram_arm9(bank,VRAM_PAGE_AOBJ+pageofs+2); //unexpected mirroring - I have no proof, but it is inferred from the ABG above
				break;
			case 3: //texture palette
				vramConfiguration.banks[bank].purpose = VramConfiguration::TEXPAL;
				MMU.texInfo.texPalSlot[pageofs] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				break;
			case 4: //A BG extended palette
				switch(ofs) {
				case 0:
				case 1:
					vramConfiguration.banks[bank].purpose = VramConfiguration::ABGEXTPAL;
					MMU.ExtPal[0][ofs*2] = MMU_vram_physical(vram_bank_info[bank].page_addr);
					MMU.ExtPal[0][ofs*2+1] = MMU.ExtPal[0][ofs*2] + ADDRESS_STEP_8KB;
					break;
				default:
					vramConfiguration.banks[bank].purpose = VramConfiguration::INVALID;
					PROGINFO("Unsupported ofs setting %d for engine A bgextpal vram bank %c\n", ofs, 'A'+bank);
					break;
				}
				break;
			case 5: //A OBJ extended palette
				vramConfiguration.banks[bank].purpose = VramConfiguration::AOBJEXTPAL;
				MMU.ObjExtPal[0][0] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				MMU.ObjExtPal[0][1] = MMU.ObjExtPal[0][1] + ADDRESS_STEP_8KB;
				if(ofs != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
				break;
			default: goto unsupported_mst;
			}
			break;
		}

		case VRAM_BANK_H:
			mst = VRAMBankCnt & 3;
			if(((VRAMBankCnt>>3)&3) != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
			switch(mst)
			{
			case 0: //LCDC
				vramConfiguration.banks[bank].purpose = VramConfiguration::LCDC;
				MMU_vram_lcdc(bank);
				break;
			case 1: //BBG
				vramConfiguration.banks[bank].purpose = VramConfiguration::BBG;
				MMU_vram_arm9(bank,VRAM_PAGE_BBG);
				MMU_vram_arm9(bank,VRAM_PAGE_BBG + 4); //unexpected mirroring
				break;
			case 2: //B BG extended palette
				vramConfiguration.banks[bank].purpose = VramConfiguration::BBGEXTPAL;
				MMU.ExtPal[1][0] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				MMU.ExtPal[1][1] = MMU.ExtPal[1][0] + ADDRESS_STEP_8KB;
				MMU.ExtPal[1][2] = MMU.ExtPal[1][1] + ADDRESS_STEP_8KB;
				MMU.ExtPal[1][3] = MMU.ExtPal[1][2] + ADDRESS_STEP_8KB;
				break;
			default: goto unsupported_mst;
			}
			break;

		case VRAM_BANK_I:
			mst = VRAMBankCnt & 3;
			if(((VRAMBankCnt>>3)&3) != 0) PROGINFO("Bank %i: MST %i OFS %i\n", mst, ofs);
			switch(mst)
			{
			case 0: //LCDC
				vramConfiguration.banks[bank].purpose = VramConfiguration::LCDC;
				MMU_vram_lcdc(bank);
				break;
			case 1: //BBG
				vramConfiguration.banks[bank].purpose = VramConfiguration::BBG;
				MMU_vram_arm9(bank,VRAM_PAGE_BBG+2);
				MMU_vram_arm9(bank,VRAM_PAGE_BBG+3); //unexpected mirroring
				break;
			case 2: //BOBJ
				vramConfiguration.banks[bank].purpose = VramConfiguration::BOBJ;
				MMU_vram_arm9(bank,VRAM_PAGE_BOBJ);
				MMU_vram_arm9(bank,VRAM_PAGE_BOBJ+1); //FF3 end scene (lens flare sprite) needs this as it renders a sprite off the end of the 16KB and back around
				break;
			case 3: //B OBJ extended palette
				vramConfiguration.banks[bank].purpose = VramConfiguration::BOBJEXTPAL;
				MMU.ObjExtPal[1][0] = MMU_vram_physical(vram_bank_info[bank].page_addr);
				MMU.ObjExtPal[1][1] = MMU.ObjExtPal[1][1] + ADDRESS_STEP_8KB;
				break;
			default: goto unsupported_mst;
			}
			break;

			
	} //switch(bank)

	vramConfiguration.banks[bank].ofs = ofs;

	return;

unsupported_mst:
	vramConfiguration.banks[bank].purpose = VramConfiguration::INVALID;
	PROGINFO("Unsupported mst setting %d for vram bank %c\n", mst, 'A'+bank);
}

void MMU_VRAM_unmap_all()
{
	vramConfiguration.clear();

	vram_arm7_map[0] = VRAM_PAGE_UNMAPPED;
	vram_arm7_map[1] = VRAM_PAGE_UNMAPPED;

	for(int i=0;i<VRAM_LCDC_PAGES;i++)
		vram_lcdc_map[i] = VRAM_PAGE_UNMAPPED;
	for(int i=0;i<VRAM_ARM9_PAGES;i++)
		vram_arm9_map[i] = VRAM_PAGE_UNMAPPED;

	for (int i = 0; i < 4; i++)
	{
		MMU.ExtPal[0][i] = MMU.blank_memory;
		MMU.ExtPal[1][i] = MMU.blank_memory;
	}

	MMU.ObjExtPal[0][0] = MMU.blank_memory;
	MMU.ObjExtPal[0][1] = MMU.blank_memory;
	MMU.ObjExtPal[1][0] = MMU.blank_memory;
	MMU.ObjExtPal[1][1] = MMU.blank_memory;

	for(int i=0;i<6;i++)
		MMU.texInfo.texPalSlot[i] = MMU.blank_memory;

	for(int i=0;i<4;i++)
		MMU.texInfo.textureSlotAddr[i] = MMU.blank_memory;
}

static inline void MMU_VRAMmapControl(u8 block, u8 VRAMBankCnt)
{
	//handle WRAM, first of all
	if(block == 7)
	{
		MMU.WRAMCNT = VRAMBankCnt & 3;
		return;
	}

	//first, save the texture info so we can check it for changes and trigger purges of the texcache
	MMU_struct::TextureInfo oldTexInfo = MMU.texInfo;

	//unmap everything
	MMU_VRAM_unmap_all();

	//unmap VRAM_BANK_C and VRAM_BANK_D from arm7. theyll get mapped again in a moment if necessary
	T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, 0);

	//write the new value to the reg
	T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x240 + block, VRAMBankCnt);

	//refresh all bank settings
	//zero XX-XX-200X (long before jun 2012)
	//these are enumerated so that we can tune the order they get applied
	//in order to emulate prioritization rules for memory regions
	//with multiple banks mapped.
	//We're probably still not mapping things 100% correctly, but this helped us get closer:
	//goblet of fire "care of magical creatures" maps I and D to BOBJ (the I is an accident)
	//and requires A to override it.
	//This may create other bugs....
	MMU_VRAMmapRefreshBank(VRAM_BANK_I);
	MMU_VRAMmapRefreshBank(VRAM_BANK_H);
	MMU_VRAMmapRefreshBank(VRAM_BANK_G);
	MMU_VRAMmapRefreshBank(VRAM_BANK_F);
	MMU_VRAMmapRefreshBank(VRAM_BANK_E);
	//zero 21-jun-2012
	//tomwi's streaming music demo sets A and D to ABG (the A is an accident).
	//in this case, D should get priority. 
	//this is somewhat risky. will it break other things?
	MMU_VRAMmapRefreshBank(VRAM_BANK_A);
	MMU_VRAMmapRefreshBank(VRAM_BANK_B);
	MMU_VRAMmapRefreshBank(VRAM_BANK_C);
	MMU_VRAMmapRefreshBank(VRAM_BANK_D);

	//printf(vramConfiguration.describe().c_str());
	//printf("vram remapped at vcount=%d\n",nds.VCount);

	//if texInfo changed, trigger notifications
	if(memcmp(&oldTexInfo,&MMU.texInfo,sizeof(MMU_struct::TextureInfo)))
	{
		//if(!nds.isIn3dVblank())
	//		PROGINFO("Changing texture or texture palette mappings outside of 3d vblank\n");
		gpu3D->NDS_3D_VramReconfigureSignal();
	}

	//-------------------------------
	//set up arm9 mirrorings
	//these are probably not entirely accurate. more study will be necessary.
	//in general, we find that it is not uncommon at all for games to accidentally do this.
	//
	//being able to easily do these experiments was one of the primary motivations for this remake of the vram mapping system

	//see the "unexpected mirroring" comments above for some more mirroring
	//so far "unexpected mirrorings" are tested by combining these games:
	//despereaux - storybook subtitles
	//NSMB - world map sub screen
	//drill spirits EU - mission select (just for control purposes, as it doesnt use H or I)
	//...
	//note that the "unexpected mirroring" items above may at some point rely on being executed in a certain order.
	//(sequentially A..I)

	const int types[] = {VRAM_PAGE_ABG,VRAM_PAGE_BBG,VRAM_PAGE_AOBJ,VRAM_PAGE_BOBJ};
	const int sizes[] = {32,8,16,8};
	for(int t=0;t<4;t++)
	{
		//the idea here is to pad out the mirrored space with copies of the mappable area,
		//without respect to what is mapped within that mappable area.
		//we hope that this is correct in all cases
		//required for driller spirits in mission select (mapping is simple A,B,C,D to each purpose)
		const int size = sizes[t];
		const int mask = size-1;
		const int type = types[t];
		for(int i=size;i<128;i++)
		{
			const int page = type + i;
			vram_arm9_map[page] = vram_arm9_map[type+(i&mask)];
		}

		//attempt #1: screen corruption in drill spirits EU
		//it seems like these shouldnt pad out 128K banks (space beyond those should have remained unmapped)
		//int mirrorMask = -1;
		//int type = types[t];
		////if(type==VRAM_PAGE_BOBJ) continue;
		//if(type==VRAM_PAGE_AOBJ) continue;
		//for(int i=0;i<128;i++)
		//{
		//	int page = type + i;
		//	if(vram_arm9_map[page] == VRAM_PAGE_UNMAPPED)
		//	{
		//		if(i==0) break; //can't mirror anything if theres nothing mapped!
		//		if(mirrorMask == -1)
		//			mirrorMask = i-1;
		//		vram_arm9_map[page] = vram_arm9_map[type+(i&mirrorMask)];
		//	}
		//}
	}

	//-------------------------------
}

//////////////////////////////////////////////////////////////
//end vram
//////////////////////////////////////////////////////////////



void MMU_Init(void)
{
	LOG("MMU init\n");

	memset(&MMU, 0, sizeof(MMU_struct));

	//MMU.DTCMRegion = 0x027C0000;
	//even though apps may change dtcm immediately upon startup, this is the correct hardware starting value:
	MMU.DTCMRegion = 0x08000000;
	MMU.ITCMRegion = 0x00000000;

	IPC_FIFOinit(ARMCPU_ARM9);
	IPC_FIFOinit(ARMCPU_ARM7);
	GFX_PIPEclear();
	GFX_FIFOclear();
	DISP_FIFOinit();

	mc_init(&MMU.fw, MC_TYPE_FLASH);  /* init fw device */
	mc_alloc(&MMU.fw, NDS_FW_SIZE_V1);
	MMU.fw.fp = NULL;
	MMU.fw.isFirmware = true;

	rtcInit();
	
	slot1_Init();
	slot2_Init();
	
	if(Mic_Init() == FALSE)
		INFO("Microphone init failed.\n");
	else
		INFO("Microphone successfully inited.\n");
} 

void MMU_DeInit(void) {
	LOG("MMU deinit\n");
	if (MMU.fw.fp)
		fclose(MMU.fw.fp);
	mc_free(&MMU.fw);      

	slot1_Shutdown();
	slot2_Shutdown();
	Mic_DeInit();
}

void MMU_Reset()
{
	memset(MMU.ARM9_DTCM, 0, sizeof(MMU.ARM9_DTCM));
	memset(MMU.ARM9_ITCM, 0, sizeof(MMU.ARM9_ITCM));
	memset(MMU.ARM9_LCD,  0, sizeof(MMU.ARM9_LCD));
	memset(MMU.ARM9_OAM,  0, sizeof(MMU.ARM9_OAM));
	memset(MMU.ARM9_REG,  0, sizeof(MMU.ARM9_REG));
	memset(MMU.ARM9_VMEM, 0, sizeof(MMU.ARM9_VMEM));
	memset(MMU.MAIN_MEM,  0, sizeof(MMU.MAIN_MEM));

	memset(MMU.blank_memory,  0, sizeof(MMU.blank_memory));
	memset(MMU.UNUSED_RAM,    0, sizeof(MMU.UNUSED_RAM));
	memset(MMU.MORE_UNUSED_RAM,    0, sizeof(MMU.UNUSED_RAM));
	
	memset(MMU.ARM7_ERAM,     0, sizeof(MMU.ARM7_ERAM));
	memset(MMU.ARM7_REG,      0, sizeof(MMU.ARM7_REG));
	memset(MMU.ARM7_WIRAM,	  0, sizeof(MMU.ARM7_WIRAM));
	memset(MMU.SWIRAM,	  0, sizeof(MMU.SWIRAM));

	IPC_FIFOinit(ARMCPU_ARM9);
	IPC_FIFOinit(ARMCPU_ARM7);
	GFX_PIPEclear();
	GFX_FIFOclear();
	DISP_FIFOinit();
	
	MMU.DTCMRegion = 0x027C0000;
	MMU.ITCMRegion = 0x00000000;
	
	memset(MMU.timer,         0, sizeof(u16) * 2 * 4);
	memset(MMU.timerMODE,     0, sizeof(s32) * 2 * 4);
	memset(MMU.timerON,       0, sizeof(u32) * 2 * 4);
	memset(MMU.timerRUN,      0, sizeof(u32) * 2 * 4);
	memset(MMU.timerReload,   0, sizeof(u16) * 2 * 4);
	
	memset(MMU.reg_IME,       0, sizeof(u32) * 2);
	memset(MMU.reg_IE,        0, sizeof(u32) * 2);
	memset(MMU.reg_IF_bits,   0, sizeof(u32) * 2);
	memset(MMU.reg_IF_pending,   0, sizeof(u32) * 2);
	
	memset(&MMU.dscard,        0, sizeof(MMU.dscard));

	MMU.divRunning = 0;
	MMU.divResult = 0;
	MMU.divMod = 0;
	MMU.divCycles = 0;

	MMU.sqrtRunning = 0;
	MMU.sqrtResult = 0;
	MMU.sqrtCycles = 0;

	MMU.SPI_CNT = 0;
	MMU.AUX_SPI_CNT = 0;

	reconstruct(&key2);

	MMU.WRAMCNT = 0;

	// Enable the sound speakers
	T1WriteWord(MMU.ARM7_REG, 0x304, 0x0001);
	
	MainScreen.offset = 0;
	SubScreen.offset  = 192;
	
	MMU_VRAM_unmap_all();

	MMU.powerMan_CntReg = 0x00;
	MMU.powerMan_CntRegWritten = FALSE;
	MMU.powerMan_Reg[0] = 0x0B;
	MMU.powerMan_Reg[1] = 0x00;
	MMU.powerMan_Reg[2] = 0x01;
	MMU.powerMan_Reg[3] = 0x00;

	rtcInit();
	partie = 1;
	slot1_Reset();
	slot2_Reset();
	Mic_Reset();
	MMU.gfx3dCycles = 0;

	MMU.dscard[0].transfer_count = 0;
	MMU.dscard[0].mode = eCardMode_RAW;
	MMU.dscard[1].transfer_count = 0;
	MMU.dscard[1].mode = eCardMode_RAW;

	reconstruct(&MMU_new);

	MMU_timing.arm7codeFetch.Reset();
	MMU_timing.arm7dataFetch.Reset();
	MMU_timing.arm9codeFetch.Reset();
	MMU_timing.arm9dataFetch.Reset();
	MMU_timing.arm9codeCache.Reset();
	MMU_timing.arm9dataCache.Reset();
}

void SetupMMU(bool debugConsole, bool dsi) {
	if(debugConsole) _MMU_MAIN_MEM_MASK = 0x7FFFFF;
	else _MMU_MAIN_MEM_MASK = 0x3FFFFF;
	if(dsi) _MMU_MAIN_MEM_MASK = 0xFFFFFF;
	_MMU_MAIN_MEM_MASK16 = _MMU_MAIN_MEM_MASK & ~1;
	_MMU_MAIN_MEM_MASK32 = _MMU_MAIN_MEM_MASK & ~3;
}

static void execsqrt() {
	u32 ret;
	u8 mode = MMU_new.sqrt.mode;
	MMU_new.sqrt.busy = 1;

	if (mode) { 
		u64 v = T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B8);
		ret = (u32)isqrt(v);
	} else {
		u32 v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B8);
		ret = (u32)isqrt(v);
	}

	//clear the result while the sqrt unit is busy
	//todo - is this right? is it reasonable?
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B4, 0);

	MMU.sqrtCycles = nds_timer + 26;
	MMU.sqrtResult = ret;
	MMU.sqrtRunning = TRUE;
	NDS_Reschedule();
}

static void execdiv() {

	s64 num,den;
	s64 res,mod;
	u8 mode = MMU_new.div.mode;
	MMU_new.div.busy = 1;
	MMU_new.div.div0 = 0;

	switch(mode)
	{
	case 0:	// 32/32
		num = (s64) (s32) T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290);
		den = (s64) (s32) T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298);
		MMU.divCycles = nds_timer + 36;
		break;
	case 1:	// 64/32
	case 3: //gbatek says this is same as mode 1
		num = (s64) T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290);
		den = (s64) (s32) T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298);
		MMU.divCycles = nds_timer + 68;
		break;
	case 2:	// 64/64
	default:
		num = (s64) T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290);
		den = (s64) T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298);
		MMU.divCycles = nds_timer + 68;
		break;
	}

	if(den==0)
	{
		res = ((num < 0) ? 1 : -1);
		mod = num;

		// the DIV0 flag in DIVCNT is set only if the full 64bit DIV_DENOM value is zero, even in 32bit mode
		if ((u64)T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298) == 0) 
			MMU_new.div.div0 = 1;
	}
	else
	{
		res = num / den;
		mod = num % den;
	}

	DIVLOG("DIV %08X%08X / %08X%08X = %08X%08X\r\n", (u32)(num>>32), (u32)num, 
							(u32)(den>>32), (u32)den, 
							(u32)(res>>32), (u32)res);

	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A0, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A4, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A8, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2AC, 0);

	MMU.divResult = res;
	MMU.divMod = mod;
	MMU.divRunning = TRUE;
	NDS_Reschedule();
}

DSI_TSC::DSI_TSC()
{
	for(int i=0;i<ARRAY_SIZE(registers);i++)
		registers[i] = 0x00;
	reset_command();
}

void DSI_TSC::reset_command()
{
	state = 0;
	readcount = 0;
	read_flag = 1;
}

u16 DSI_TSC::write16(u16 val)
{
	u16 ret;
	switch(state)
	{
	case 0:
		reg_selection = (val>>1)&0x7F;
		read_flag = val&1;
		state = 1;
		return read16();
	case 1:
		if(read_flag)
		{ }
		else
		{
			registers[reg_selection] = (u8)val;
		}
		ret = read16();
		reg_selection++;
		reg_selection &= 0x7F;
		return ret;
	}
	return 0;
}

u16 DSI_TSC::read16()
{
	u8 page = registers[0];
	switch(page)
	{
	case 3: //page 3
		switch(reg_selection)
		{
		case 9:
			if(nds.isTouch) 
				return 0;
			else return 0x40;
			break;
		case 14:
			if(nds.isTouch) 
				return 0;
			else return 0x02;
			break;
		}
		break;

	case 252: //page 252
		switch(reg_selection)
		{
		//high byte of X:
		case 1: case 3: case 5: case 7: case 9:
			return (nds.scr_touchX>>8)&0xFF;

		//low byte of X:
		case 2: case 4: case 6: case 8: case 10:
			return nds.scr_touchX&0xFF;

		//high byte of Y:
		case 11: case 13: case 15: case 17: case 19:
			return (nds.scr_touchY>>8)&0xFF;

		//low byte of Y:
		case 12: case 14: case 16: case 18: case 20:
			return nds.scr_touchY&0xFF;
		
		default:
			return 0xFF;
		}
		break;
	} //switch(page)

	//unknown page or register
	return 0xFF;
}

bool DSI_TSC::save_state(EMUFILE* os)
{
	u32 version = 0;
	write32le(version,os);

	write8le(reg_selection,os);
	write8le(read_flag,os);
	write32le(state,os);
	write32le(readcount,os);
	for(int i=0;i<ARRAY_SIZE(registers);i++)
		write8le(registers[i],os);

	return true;
}

bool DSI_TSC::load_state(EMUFILE* is)
{
	u32 version;
	read32le(&version,is);

	read8le(&reg_selection,is);
	read8le(&read_flag,is);
	read32le(&state,is);
	read32le(&readcount,is);
	for(int i=0;i<ARRAY_SIZE(registers);i++)
		read8le(&registers[i],is);

	return true;
}

void MMU_GC_endTransfer(u32 PROCNUM)
{
	u32 val = T1ReadLong(MMU.MMU_MEM[PROCNUM][0x40], 0x1A4) & 0x7F7FFFFF;
	T1WriteLong(MMU.MMU_MEM[PROCNUM][0x40], 0x1A4, val);

	// if needed, throw irq for the end of transfer
	if(MMU.AUX_SPI_CNT & 0x4000)
		NDS_makeIrq(PROCNUM, IRQ_BIT_GC_TRANSFER_COMPLETE);
}


template<int PROCNUM>
void FASTCALL MMU_writeToGCControl(u32 val)
{

	int dbsize = (val>>24)&7;
	static int gcctr=0;
	GCLOG("[GC] [%07d] GCControl: %08X (dbsize:%d)\n",gcctr,val,dbsize);
	gcctr++;
	
	GCBUS_Controller& card = MMU.dscard[PROCNUM];

	//....pick apart the fields....
	int keylength = (val&0x1FFF); //key1length high gcromctrl[21:16] ??
	u8 key2_encryptdata = (val>>13)&1;
	u8 bit15 = (val>>14)&1;
	u8 key2_applyseed = (val>>15)&1; //write only strobe
	//key1length high gcromctrl[21:16] ??
	u8 key2_encryptcommand = (val>>22)&1;
	//bit 23 read only status
	int blocksize_field = (val>>24)&7;
	u8 clockrate = (val>>27)&1;
	u8 secureareamode = (val>>28)&1;
	//RESB bit 29?
	u8 wr = (val>>30)&1;
	u8 start = (val>>31)&1; //doubles as busy on read
	static const int blocksize_table[] = {0,0x200,0x400,0x800,0x1000,0x2000,0x4000,4};
	int blocksize = blocksize_table[blocksize_field];

	//store written value, without bit 31 and bit 23 set (those will be patched in as operations proceed)
	//T1WriteLong(MMU.MMU_MEM[PROCNUM][0x40], 0x1A4, val & 0x7F7FFFFF);

	//if this operation has been triggered by strobing that bit, run it
	if (key2_applyseed)
	{
		key2.applySeed(PROCNUM);
	}

	//pluck out the command registers into a more convenient format
	GC_Command rawcmd = *(GC_Command*)&MMU.MMU_MEM[PROCNUM][0x40][0x1A8];

	//when writing a 1 to the start bit, a command runs.
	//the command is transferred to the GC during the next 8 clocks
	if(start)
	{
		GCLOG("[GC] command:"); rawcmd.print();
		slot1_device->write_command(PROCNUM, rawcmd);

		/*INFO("WRITE: %02X%02X%02X%02X%02X%02X%02X%02X ", 
			rawcmd.bytes[0], rawcmd.bytes[1], rawcmd.bytes[2], rawcmd.bytes[3],
			rawcmd.bytes[4], rawcmd.bytes[5], rawcmd.bytes[6], rawcmd.bytes[7]);
		INFO("FROM: %08X ", (PROCNUM ? NDS_ARM7:NDS_ARM9).instruct_adr);
		INFO("1A4: %08X ", val);
		INFO("SIZE: %08X\n", blocksize);*/
	}
	else
	{
		T1WriteLong(MMU.MMU_MEM[PROCNUM][0x40], 0x1A4, val & 0x7F7FFFFF);
		GCLOG("SCUTTLE????\n");
		return;
	}

	//the transfer size is determined by the specification here in GCROMCTRL, not any logic private to the card.
	card.transfer_count = blocksize;

	//if there was nothing to be done here, go ahead and flag it as done
	if(card.transfer_count == 0)
	{
		MMU_GC_endTransfer(PROCNUM);
		return;
	}

	val |= 0x00800000;
	T1WriteLong(MMU.MMU_MEM[PROCNUM][0x40], 0x1A4, val);

	// Launch DMA if start flag was set to "DS Cart"
	triggerDma(EDMAMode_Card);
}

/*template<int PROCNUM>
u32 FASTCALL MMU_readFromGCControl()
{
	return T1ReadLong(MMU.MMU_MEM[0][0x40], 0x1A4);
}*/

template<int PROCNUM>
u32 MMU_readFromGC()
{
	GCBUS_Controller& card = MMU.dscard[PROCNUM];

	//???? return the latched / last read value instead perhaps?
	if(card.transfer_count == 0)
		return 0;

	u32 val = slot1_device->read_GCDATAIN(PROCNUM);

	//update transfer counter and complete the transfer if necessary
	card.transfer_count -= 4;	
	if(card.transfer_count <= 0)
	{
		MMU_GC_endTransfer(PROCNUM);
	}

	return val;
}

template<int PROCNUM>
void MMU_writeToGC(u32 val)
{
	GCBUS_Controller& card = MMU.dscard[PROCNUM];

	slot1_device->write_GCDATAIN(PROCNUM,val);

	//update transfer counter and complete the transfer if necessary
	card.transfer_count -= 4;	
	if(card.transfer_count <= 0)
	{
		MMU_GC_endTransfer(PROCNUM);
	}
}

// ====================================================================== REG_SPIxxx
static void CalculateTouchPressure(int pressurePercent, u16 &z1, u16& z2)
{
	bool touch = nds.isTouch!=0;
	if(!touch)
	{
		z1 = z2 = 0;
		return;
	}
	int y = nds.scr_touchY;
	int x = nds.scr_touchX;
	float u = (x/256.0f);
	float v = (y/192.0f);	

	//these coefficients 

	float fPressurePercent = pressurePercent/100.0f;
	//z1 goes up as pressure goes up
	{
		float b0 = (96-80)*fPressurePercent + 80;
		float b1 = (970-864)*fPressurePercent + 864;
		float b2 = (192-136)*fPressurePercent + 136;
		float b3 = (1560-1100)*fPressurePercent + 1100;
		z1 = (u16)(int)(b0 + (b1-b0)*u + (b2-b0)*v + (b3-b2-b1+b0)*u*v);
	}
	
	//z2 goes down as pressure goes up
	{
		float b0=(1976-2300)*fPressurePercent + 2300;
		float b1=(2360-2600)*fPressurePercent + 2600;
		float b2=(3840-3900)*fPressurePercent + 3900;
		float b3=(3912-3950)*fPressurePercent + 3950;
		z2 = (u16)(int)(b0 + (b1-b0)*u + (b2-b0)*v + (b3-b2-b1+b0)*u*v);
	}

}

void FASTCALL MMU_writeToSPIData(u16 val)
{

	enum PM_Bits //from libnds
	{
		PM_SOUND_AMP		= BIT(0) ,   /*!< \brief Power the sound hardware (needed to hear stuff in GBA mode too) */
		PM_SOUND_MUTE		= BIT(1),    /*!< \brief   Mute the main speakers, headphone output will still work. */
		PM_BACKLIGHT_BOTTOM	= BIT(2),    /*!< \brief   Enable the top backlight if set */
		PM_BACKLIGHT_TOP	= BIT(3)  ,  /*!< \brief   Enable the bottom backlight if set */
		PM_SYSTEM_PWR		= BIT(6) ,   /*!< \brief  Turn the power *off* if set */
	};

	if (val !=0)
		MMU.SPI_CMD = val;

	u16 spicnt = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPICNT >> 20) & 0xff], REG_SPICNT & 0xfff);

	int device = (spicnt >> 8) & 0x3;
	int baudrate = spicnt & 0x3;
	switch(device)
	{
		case SPI_DEVICE_POWERMAN:
			if (!MMU.powerMan_CntRegWritten)
			{
				MMU.powerMan_CntReg = (val & 0xFF);
				MMU.powerMan_CntRegWritten = TRUE;
			}
			else
			{
				u16 reg = MMU.powerMan_CntReg & 0x7F;
				reg &= 0x7;
				if(reg==5 || reg==6 || reg==7) reg = 4;

				//(let's start with emulating a DS lite, since it is the more complex case)
				if(MMU.powerMan_CntReg & 0x80)
				{
					//read
					val = MMU.powerMan_Reg[reg];
				}
				else
				{
					//write
					MMU.powerMan_Reg[reg] = (u8)val;

					//our totally pathetic register handling, only the one thing we've wanted so far
					if(MMU.powerMan_Reg[0]&PM_SYSTEM_PWR)
					{
						printf("SYSTEM POWERED OFF VIA ARM7 SPI POWER DEVICE\n");
						printf("Did your main() return?\n");
						emu_halt();
					}
				}

				MMU.powerMan_CntRegWritten = FALSE;
			}
		break;

		case SPI_DEVICE_FIRMWARE:
			if(baudrate != SPI_BAUDRATE_4MHZ)		// check SPI baudrate (must be 4mhz)
			{
				printf("Wrong SPI baud rate for firmware access\n");
				val = 0;
			}
			else
				val = fw_transfer(&MMU.fw, (u8)val);
		break;

		case SPI_DEVICE_TOUCHSCREEN:
		{
			if(nds.Is_DSI())
			{
				//pass data to TSC
				val = MMU_new.dsi_tsc.write16(val);

				//apply reset command if appropriate
				if(!BIT11(MMU.SPI_CNT))
					MMU_new.dsi_tsc.reset_command();
				break;
			}

			int channel = (MMU.SPI_CMD&0x70)>>4;

			switch(channel)
			{
				case TSC_MEASURE_TEMP1:
					if(spicnt & 0x800)
					{
						if(partie)
						{
							val = ((716<<3)&0x7FF);
							partie = 0;
							break;
						}
						val = (716>>5);
						partie = 1;
						break;
					}
					val = ((716<<3)&0x7FF);
					partie = 1;
				break;

				case TSC_MEASURE_TEMP2:
					if(spicnt & 0x800)
					{
						if(partie)
						{
							val = ((865<<3)&0x7FF);
							partie = 0;
							break;
						}
						val = (865>>5);
						partie = 1;
						break;
					}
					val = ((865<<3)&0x7FF);
					partie = 1;
				break;

				case TSC_MEASURE_Y:
					//counter the number of adc touch coord reads and jitter it after a while to simulate a shaky human hand or multiple reads
					//this is actually important for some games.. seemingly due to bugs.
					nds.adc_jitterctr++;
					if(nds.adc_jitterctr == 25)
					{
						nds.adc_jitterctr = 0;
						if (nds.stylusJitter)
						{
							nds.adc_touchY ^= 16;
							nds.adc_touchX ^= 16;
						}
					}
					if(MMU.SPI_CNT&(1<<11))
					{
						if(partie)
						{
							val = (nds.adc_touchY<<3) & 0xFF;
							partie = 0;
							break;
						}

						val = (nds.adc_touchY>>5) & 0xFF;
						partie = 1;
						break;
					}
					val = (nds.adc_touchY<<3)&0xFF;
					partie = 1;
				break;

				case TSC_MEASURE_Z1: //Z1
				{
					//used for pressure calculation - must be nonzero or else some softwares will think the stylus is up.
					//something is wrong in here and some of these LSB dont make it back to libnds... whatever.
					u16 scratch;
					CalculateTouchPressure(CommonSettings.StylusPressure,val,scratch);

					if(spicnt & 0x800)
					{
						if(partie)
						{
							val = ((val<<3)&0x7FF);
							partie = 0;
							break;
						}
						val = (val>>5);
						partie = 1;
						break;
					}
					val = ((val<<3)&0x7FF);
					partie = 1;
					break;
				}

				case TSC_MEASURE_Z2: //Z2
				{
					//used for pressure calculation - must be nonzero or else some softwares will think the stylus is up.
					//something is wrong in here and some of these LSB dont make it back to libnds... whatever.
					u16 scratch;
					CalculateTouchPressure(CommonSettings.StylusPressure,scratch,val);

					if(spicnt & 0x800)
					{
						if(partie)
						{
							val = ((val<<3)&0x7FF);
							partie = 0;
							break;
						}
						val = (val>>5);
						partie = 1;
						break;
					}
					val = ((val<<3)&0x7FF);
					partie = 1;
					break;
				}

				case TSC_MEASURE_X:
					if(spicnt & 0x800)
					{
						if(partie)
						{
							val = (nds.adc_touchX << 3) & 0xFF;
							partie = 0;
							break;
						}
						val = (nds.adc_touchX>>5) & 0xFF;
						partie = 1;
						break;
					}
					val = (nds.adc_touchX<<3) & 0xFF;
					partie = 1;
					break;

				case TSC_MEASURE_AUX:
					if(!(val & 0x80))
						val = (Mic_ReadSample() & 0xFF);
					else
						val = 0;
				break;
			}
			break;
		}

		case 3 :
		// NOTICE: Device 3 of SPI is reserved (unused and unusable)
		break;
	}

	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, val & 0x00FF);
}
// ====================================================================== REG_SPIxxx END

//does some validation on the game's choice of IF value, correcting it if necessary
static void validateIF_arm9()
{
}

template<int PROCNUM> static void REG_IF_WriteByte(u32 addr, u8 val)
{
	//the following bits are generated from logic and should not be affected here
	//Bit 21    NDS9 only: Geometry Command FIFO
	//arm9: IF &= ~0x00200000;
	//arm7: IF &= ~0x00000000;
	//UPDATE IN setIF() ALSO!!!!!!!!!!!!!!!!
	//UPDATE IN mmu_loadstate ALSO!!!!!!!!!!!!
	if (addr==2) {
		if(PROCNUM==ARMCPU_ARM9)
			val &= ~0x20;
		else
			val &= ~0x00;
	}

	//ZERO 01-dec-2010 : I am no longer sure this approach is correct.. it proved to be wrong for IPC fifo.......
	//it seems as if IF bits should always be cached (only the user can clear them)
	
	MMU.reg_IF_bits[PROCNUM] &= (~(((u32)val)<<(addr<<3)));
	NDS_Reschedule();
}

template<int PROCNUM> static void REG_IF_WriteWord(u32 addr,u16 val)
{
	REG_IF_WriteByte<PROCNUM>(addr,val&0xFF);
	REG_IF_WriteByte<PROCNUM>(addr+1,(val>>8)&0xFF);
}

template<int PROCNUM> static void REG_IF_WriteLong(u32 val)
{
	REG_IF_WriteByte<PROCNUM>(0,val&0xFF);
	REG_IF_WriteByte<PROCNUM>(1,(val>>8)&0xFF);
	REG_IF_WriteByte<PROCNUM>(2,(val>>16)&0xFF);
	REG_IF_WriteByte<PROCNUM>(3,(val>>24)&0xFF);
}

template<int PROCNUM>
u32 MMU_struct::gen_IF()
{
	u32 IF = reg_IF_bits[PROCNUM];

	if(PROCNUM==ARMCPU_ARM9)
	{
		//according to gbatek, these flags are forced on until the condition is removed.
		//no proof of this though...
		switch(MMU_new.gxstat.gxfifo_irq)
		{
		case 0: //never
			break;
		case 1: //less than half full
			if(MMU_new.gxstat.fifo_low) 
				IF |= IRQ_MASK_ARM9_GXFIFO;
			break;
		case 2: //empty
			if(MMU_new.gxstat.fifo_empty) 
				IF |= IRQ_MASK_ARM9_GXFIFO;
			break;
		case 3: //reserved/unknown
			break;
		}
	}

	return IF;
}

static void writereg_DISP3DCNT(const int size, const u32 adr, const u32 val)
{
	//UGH. rewrite this shite to use individual values and reconstruct the return value instead of packing things in this !@#)ing register
	
	//nanostray2 cutscene will test this vs old desmumes by using some kind of 32bit access for setting up this reg for cutscenes
	switch(size)
	{
	case 8:
		switch(adr)
		{
		case REG_DISPA_DISP3DCNT: 
			MMU.reg_DISP3DCNT_bits &= 0xFFFFFF00;
			MMU.reg_DISP3DCNT_bits |= val;
			gfx3d_Control(MMU.reg_DISP3DCNT_bits);
			break;
		case REG_DISPA_DISP3DCNT+1:
			{
				u32 myval = (val & ~0x30) | (~val & ((MMU.reg_DISP3DCNT_bits>>8) & 0x30)); // bits 12,13 are ack bits
				myval &= 0x7F; //top bit isnt connected
				MMU.reg_DISP3DCNT_bits = MMU.reg_DISP3DCNT_bits&0xFFFF00FF;
				MMU.reg_DISP3DCNT_bits |= (myval<<8);
				gfx3d_Control(MMU.reg_DISP3DCNT_bits);
			}
			break;
		}
		break;
	case 16:
	case 32:
		writereg_DISP3DCNT(8,adr,val&0xFF);
		writereg_DISP3DCNT(8,adr+1,(val>>8)&0xFF);
		break;
	}
}

static u32 readreg_DISP3DCNT(const int size, const u32 adr)
{
	//UGH. rewrite this shite to use individual values and reconstruct the return value instead of packing things in this !@#)ing register
	switch(size)
	{
	case 8:
		switch(adr)
		{
		case REG_DISPA_DISP3DCNT: 
			return MMU.reg_DISP3DCNT_bits & 0xFF;
		case REG_DISPA_DISP3DCNT+1:
			return ((MMU.reg_DISP3DCNT_bits)>>8)& 0xFF;
		}
		break;
	case 16:
	case 32:
		return readreg_DISP3DCNT(8,adr)|(readreg_DISP3DCNT(8,adr+1)<<8);
	}
	assert(false);
	return 0;
}


static u32 readreg_POWCNT1(const int size, const u32 adr) { 
	switch(size)
	{
	case 8:
		switch(adr)
		{
			case REG_POWCNT1: {
				u8 ret = 0;		
				ret |= nds.power1.lcd?BIT(0):0;
				ret |= nds.power1.gpuMain?BIT(1):0;
				ret |= nds.power1.gfx3d_render?BIT(2):0;
				ret |= nds.power1.gfx3d_geometry?BIT(3):0;
				return ret;
			}
			case REG_POWCNT1+1: {
				u8 ret = 0;
				ret |= nds.power1.gpuSub?BIT(1):0;
				ret |= nds.power1.dispswap?BIT(7):0;
				return ret;
			}
			default:
				return 0;		}
	case 16:
	case 32:
		return readreg_POWCNT1(8,adr)|(readreg_POWCNT1(8,adr+1)<<8);
	}
	assert(false);
	return 0;
}
static void writereg_POWCNT1(const int size, const u32 adr, const u32 val) { 
	switch(size)
	{
	case 8:
		switch(adr)
		{
		case REG_POWCNT1:
			nds.power1.lcd = BIT0(val);
			nds.power1.gpuMain = BIT1(val);
			nds.power1.gfx3d_render = BIT2(val);
			nds.power1.gfx3d_geometry = BIT3(val);
			break;
		case REG_POWCNT1+1:
			nds.power1.gpuSub = BIT1(val);
			nds.power1.dispswap = BIT7(val);
			if(nds.power1.dispswap)
			{
				//printf("Main core on top (vcount=%d)\n",nds.VCount);
				MainScreen.offset = 0;
				SubScreen.offset = 192;
			}
			else
			{
				//printf("Main core on bottom (vcount=%d)\n",nds.VCount);
				MainScreen.offset = 192;
				SubScreen.offset = 0;
			}	
			break;
		}
		break;
	case 16:
	case 32:
		writereg_POWCNT1(8,adr,val&0xFF);
		writereg_POWCNT1(8,adr+1,(val>>8)&0xFF);
		break;
	}
}

static INLINE void MMU_IPCSync(u8 proc, u32 val)
{
	//INFO("IPC%s sync 0x%04X (0x%02X|%02X)\n", proc?"7":"9", val, val >> 8, val & 0xFF);
	u32 sync_l = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x180) & 0xFFFF;
	u32 sync_r = T1ReadLong(MMU.MMU_MEM[proc^1][0x40], 0x180) & 0xFFFF;

	sync_l = ( sync_l & 0x000F ) | ( val & 0x0F00 );
	sync_r = ( sync_r & 0x6F00 ) | ( (val >> 8) & 0x000F );

	sync_l |= val & 0x6000;

	if(nds.ensataEmulation && proc==1 && nds.ensataIpcSyncCounter<9) {
		u32 iteration = (val&0x0F00)>>8;

		if(iteration==8-nds.ensataIpcSyncCounter)
			nds.ensataIpcSyncCounter++;
		else printf("ERROR: ENSATA IPC SYNC HACK FAILED; BAD THINGS MAY HAPPEN\n");

		//for some reason, the arm9 doesn't handshake when ensata is detected.
		//so we complete the protocol here, which is to mirror the values 8..0 back to 
		//the arm7 as they are written by the arm7
		sync_r &= 0xF0FF;
		sync_r |= (iteration<<8);
		sync_l &= 0xFFF0;
		sync_l |= iteration;
	}

	T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x180, sync_l);
	T1WriteLong(MMU.MMU_MEM[proc^1][0x40], 0x180, sync_r);

	if ((sync_l & IPCSYNC_IRQ_SEND) && (sync_r & IPCSYNC_IRQ_RECV))
		NDS_makeIrq(proc^1, IRQ_BIT_IPCSYNC);

	NDS_Reschedule();
}

static INLINE u16 read_timer(int proc, int timerIndex)
{
	//chained timers are always up to date
	if(MMU.timerMODE[proc][timerIndex] == 0xFFFF)
		return MMU.timer[proc][timerIndex];

	//sometimes a timer will be read when it is not enabled.
	//we should have the value cached
	if(!MMU.timerON[proc][timerIndex])
		return MMU.timer[proc][timerIndex];

	//for unchained timers, we do not keep the timer up to date. its value will need to be calculated here
	s32 diff = (s32)(nds.timerCycle[proc][timerIndex] - nds_timer);
	assert(diff>=0);
	if(diff<0) 
		printf("NEW EMULOOP BAD NEWS PLEASE REPORT: TIME READ DIFF < 0 (%d) (%d) (%d)\n",diff,timerIndex,MMU.timerMODE[proc][timerIndex]);
	
	s32 units = diff / (1<<MMU.timerMODE[proc][timerIndex]);
	s32 ret;

	if(units==65536)
		ret = 0; //I'm not sure why this is happening...
		//whichever instruction setup this counter should advance nds_timer (I think?) and the division should truncate down to 65535 immediately
	else if(units>65536) {
		printf("NEW EMULOOP BAD NEWS PLEASE REPORT: UNITS %d:%d = %d\n",proc,timerIndex,units);
		ret = 0;
	}
	else ret = 65535 - units;

	return ret;
}

static INLINE void write_timer(int proc, int timerIndex, u16 val)
{
#if 0
	int mask		= ((val&0x80)>>7) << timerIndex;
	MMU.CheckTimers = (MMU.CheckTimers & (~mask)) | mask;
#endif

	if(val&0x80)
		MMU.timer[proc][timerIndex] = MMU.timerReload[proc][timerIndex];
	else
	{
		if(MMU.timerON[proc][timerIndex])
			//read the timer value one last time
			MMU.timer[proc][timerIndex] = read_timer(proc,timerIndex);
	}

	MMU.timerON[proc][timerIndex] = val & 0x80;

	switch(val&7)
	{
		case 0 :
			MMU.timerMODE[proc][timerIndex] = 0+1;
			break;
		case 1 :
			MMU.timerMODE[proc][timerIndex] = 6+1;
			break;
		case 2 :
			MMU.timerMODE[proc][timerIndex] = 8+1;
			break;
		case 3 :
			MMU.timerMODE[proc][timerIndex] = 10+1;
			break;
		default :
			MMU.timerMODE[proc][timerIndex] = 0xFFFF;
			break;
	}

	int remain = 65536 - MMU.timerReload[proc][timerIndex];
	nds.timerCycle[proc][timerIndex] = nds_timer + (remain<<MMU.timerMODE[proc][timerIndex]);

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x102+timerIndex*4, val);
	NDS_RescheduleTimers();
}

extern CACHE_ALIGN MatrixStack	mtxStack[4];
u32 TGXSTAT::read32()
{
	u32 ret = 0;

	ret |= tb|(tr<<1);

	int _hack_getMatrixStackLevel(int which);
	
	// stack position always equal zero. possible timings is wrong
	// using in "The Wild West"
	int proj_level = _hack_getMatrixStackLevel(0);
	int mv_level = _hack_getMatrixStackLevel(1);
	ret |= ((proj_level << 13) | (mv_level << 8)); //matrix stack levels //no proof that these are needed yet

	ret |= sb<<14;	//stack busy
	ret |= se<<15;
	ret |= (std::min(gxFIFO.size,(u32)255))<<16;
	if(gxFIFO.size>=255) ret |= BIT(24); //fifo full
	if(gxFIFO.size<128) ret |= BIT(25); //fifo half
	if(gxFIFO.size==0) ret |= BIT(26); //fifo empty
	//determine busy flag.
	//if we're waiting for a flush, we're busy
	if(isSwapBuffers) ret |= BIT(27);
	//if fifo is nonempty, we're busy
	if(gxFIFO.size!=0) ret |= BIT(27);

	//printf("[%d] %d %d %d\n",currFrameCounter,se,sb,gxFIFO.size!=0);

	
	ret |= ((gxfifo_irq & 0x3) << 30); //user's irq flags

	//printf("vc=%03d Returning gxstat read: %08X (isSwapBuffers=%d)\n",nds.VCount,ret,isSwapBuffers);

	//ret = (2 << 8);
	//INFO("gxSTAT 0x%08X (proj %i, pos %i)\n", ret, _hack_getMatrixStackLevel(1), _hack_getMatrixStackLevel(2));
	return ret;
}

void TGXSTAT::write32(const u32 val)
{
	gxfifo_irq = (val>>30)&3;
	if(BIT15(val)) 
	{
		// Writing "1" to Bit15 does reset the Error Flag (Bit15), 
		// and additionally resets the Projection Stack Pointer (Bit13)
		mtxStack[0].position = 0;
		se = 0; //clear stack error flag
	}
	//printf("gxstat write: %08X while gxfifo.size=%d\n",val,gxFIFO.size);

		//if (val & (1<<29))		// clear? (only in homebrew?)
	//{
	//	GFX_PIPEclear();
	//	GFX_FIFOclear();
	//	return;
	//}
}

void TGXSTAT::savestate(EMUFILE *f)
{
	write32le(1,f); //version
	write8le(tb,f); write8le(tr,f); write8le(se,f); write8le(gxfifo_irq,f); write8le(sb,f);
}
bool TGXSTAT::loadstate(EMUFILE *f)
{
	u32 version;
	if(read32le(&version,f) != 1) return false;
	if(version > 1) return false;

	read8le(&tb,f); read8le(&tr,f); read8le(&se,f); read8le(&gxfifo_irq,f); 
	if (version >= 1)
		read8le(&sb,f);

	return true;
}

//this could be inlined...
void MMU_struct_new::write_dma(const int proc, const int size, const u32 _adr, const u32 val)
{
	//printf("%08lld -- write_dma: %d %d %08X %08X\n",nds_timer,proc,size,_adr,val);
	const u32 adr = _adr - _REG_DMA_CONTROL_MIN;
	const u32 chan = adr/12;
	const u32 regnum = (adr - chan*12)>>2;

	MMU_new.dma[proc][chan].regs[regnum]->write(size,adr,val);
}
	

//this could be inlined...
u32 MMU_struct_new::read_dma(const int proc, const int size, const u32 _adr)
{
	const u32 adr = _adr - _REG_DMA_CONTROL_MIN;
	const u32 chan = adr/12;
	const u32 regnum = (adr - chan*12)>>2;

	const u32 temp = MMU_new.dma[proc][chan].regs[regnum]->read(size,adr);
	//printf("%08lld --  read_dma: %d %d %08X = %08X\n",nds_timer,proc,size,_adr,temp);

	return temp;
}

MMU_struct_new::MMU_struct_new()
{
	for(int i=0;i<2;i++)
		for(int j=0;j<4;j++) {
			dma[i][j].procnum = i;
			dma[i][j].chan = j;
		}
}

bool DmaController::loadstate(EMUFILE* f)
{
	u32 version;
	if(read32le(&version,f) != 1) return false;
	if(version >1) return false;

	read8le(&enable,f); read8le(&irq,f); read8le(&repeatMode,f); read8le(&_startmode,f);
	read8le(&userEnable,f);
	read32le(&wordcount,f);
	u8 temp;
	read8le(&temp,f); startmode = (EDMAMode)temp;
	read8le(&temp,f); bitWidth = (EDMABitWidth)temp;
	read8le(&temp,f); sar = (EDMASourceUpdate)temp;
	read8le(&temp,f); dar = (EDMADestinationUpdate)temp;
	read32le(&saddr,f); read32le(&daddr,f);
	read32le(&dmaCheck,f); read32le(&running,f); read32le(&paused,f); read32le(&triggered,f); 
	read64le(&nextEvent,f);

	if(version==1)
	{
		read32le(&saddr_user,f);
		read32le(&daddr_user,f);
	}

	return true;
}

void DmaController::savestate(EMUFILE *f)
{
	write32le(1,f); //version
	write8le(enable,f); write8le(irq,f); write8le(repeatMode,f); write8le(_startmode,f);
	write8le(userEnable,f);
	write32le(wordcount,f);
	write8le(startmode,f); 
	write8le(bitWidth,f); 
	write8le(sar,f); 
	write8le(dar,f); 
	write32le(saddr,f); write32le(daddr,f);
	write32le(dmaCheck,f); write32le(running,f); write32le(paused,f); write32le(triggered,f); 
	write64le(nextEvent,f);
	write32le(saddr_user,f);
	write32le(daddr_user,f);
}

void DmaController::write32(const u32 val)
{
	if(running)
	{
		//desp triggers this a lot. figure out whats going on
		//printf("thats weird..user edited dma control while it was running\n");
	}
	
	wordcount = val&0x1FFFFF;
	u8 wasRepeatMode = repeatMode;
	u8 wasEnable = enable;
	u32 valhi = val>>16;
	dar = (EDMADestinationUpdate)((valhi>>5)&3);
	sar = (EDMASourceUpdate)((valhi>>7)&3);
	repeatMode = BIT9(valhi);
	bitWidth = (EDMABitWidth)BIT10(valhi);
	_startmode = (valhi>>11)&7;
	if(procnum==ARMCPU_ARM7) _startmode &= 6;
	irq = BIT14(valhi);
	enable = BIT15(valhi);

	//printf("ARM%c DMA%d WRITE %08X count %08X, %08X -> %08X\n", procnum?'7':'9', chan, val, wordcount, saddr_user, daddr_user);

	//if(irq) printf("!!!!!!!!!!!!IRQ!!!!!!!!!!!!!\n");

	//make sure we don't get any old triggers
	if(!wasEnable && enable)
		triggered = FALSE;

	if(enable)
	{
		//address registers are reloaded from user's settings whenever dma is enabled
		//this is tested well by contra4 classic games, which use this to hdma scroll registers
		//specifically in the fit-screen mode.
		saddr = saddr_user;
		daddr = daddr_user;
	}

	//printf("dma %d,%d set to startmode %d with wordcount set to: %08X\n",procnum,chan,_startmode,wordcount);
	if (enable && procnum==1 && (!(chan&1)) && _startmode==6)
		printf("!!!---!!! WIFI DMA: %08X TO %08X, %i WORDS !!!---!!!\n", saddr, daddr, wordcount);

	//analyze enabling and startmode.
	//note that we only do this if the dma was freshly enabled.
	//we should probably also only be latching these other regs in that case too..
	//but for now just this one will do (otherwise the dma repeat stop procedure (in this case the ff4 title menu load with gamecard dma) will fail)
	//if(!running) enable = userEnable;

	//if we were previously in a triggered mode, and were already enabled,
	//then don't re-trigger now. this is rather confusing..
	//we really only want to auto-trigger gxfifo and immediate modes.
	//but we don't know what mode we're in yet.
	//so this is our workaround
	//(otherwise the dma repeat stop procedure (in this case the ff4 title menu load with gamecard dma) will fail)
	bool doNotStart = false;
	if(startmode != EDMAMode_Immediate && startmode != EDMAMode_GXFifo && wasEnable) doNotStart = true;

	//this dma may need to trigger now, so give it a chance
	//if(!(wasRepeatMode && !repeatMode)) //this was an older test
	if(!doNotStart)
		doSchedule();

	driver->DEBUG_UpdateIORegView(BaseDriver::EDEBUG_IOREG_DMA);
}

void DmaController::exec()
{
	//this function runs when the DMA ends. the dma start actually queues this event after some kind of guess as to how long the DMA should take

	//printf("ARM%c DMA%d execute, count %08X, mode %d%s\n", procnum?'7':'9', chan, wordcount, startmode, running?" - RUNNING":"");
	//we'll need to unfreeze the arm9 bus now
	if(procnum==ARMCPU_ARM9) nds.freezeBus &= ~(1<<(chan+1));

	dmaCheck = FALSE;

	if(running)
	{
		switch(startmode) {
			case EDMAMode_GXFifo:
				//this dma mode won't finish always its job when it gets signalled
				//sometimes it will have words left to transfer.
				//if(!paused) printf("gxfifo dma ended with %d remaining\n",wordcount); //only print this once
				if(wordcount>0) {
					doPause();
					break;
				}
			default:
				doStop();
				driver->DEBUG_UpdateIORegView(BaseDriver::EDEBUG_IOREG_DMA);
				return;
		}
	}
	
	if(enable)
	{
		//analyze startmode (this only gets latched when a dma begins)
		if(procnum==ARMCPU_ARM9) startmode = (EDMAMode)_startmode;
		else {
			//arm7 startmode analysis:
			static const EDMAMode lookup[] = {EDMAMode_Immediate,EDMAMode_VBlank,EDMAMode_Card,EDMAMode7_Wifi};
			//arm7 has a slightly different startmode encoding
			startmode = lookup[_startmode>>1];
			if(startmode == EDMAMode7_Wifi && (chan==1 || chan==3))
				startmode = EDMAMode7_GBASlot;
		}

		//make it run, if it is triggered
		//but first, scan for triggering conditions
		switch(startmode) {
			case EDMAMode_Immediate:
				triggered = TRUE;
				break;
			case EDMAMode_GXFifo:
				if(gxFIFO.size<=127)
					triggered = TRUE;
				break;
			default:
				break;
		}

		if(triggered)
		{
			//if(procnum==0) printf("vc=%03d %08lld trig type %d dma#%d w/words %d at src:%08X dst:%08X gxf:%d",nds.VCount,nds_timer,startmode,chan,wordcount,saddr,daddr,gxFIFO.size);
			running = TRUE;
			paused = FALSE;
			if(procnum == ARMCPU_ARM9) doCopy<ARMCPU_ARM9>();
			else doCopy<ARMCPU_ARM7>();
			//printf(";%d\n",gxFIFO.size);
		}
	}

	driver->DEBUG_UpdateIORegView(BaseDriver::EDEBUG_IOREG_DMA);
}

template<int PROCNUM>
void DmaController::doCopy()
{
	//generate a copy count depending on various copy mode's behavior
	u32 todo = wordcount;
	u32 sz = (bitWidth==EDMABitWidth_16)?2:4;
	u32 dstinc = 0, srcinc = 0;

	if(PROCNUM == ARMCPU_ARM9) if(todo == 0) todo = 0x200000; //according to gbatek.. we've verified this behaviour on the arm7
	if(startmode == EDMAMode_MemDisplay) 
	{
		todo = 128; //this is a hack. maybe an alright one though. it should be 4 words at a time. this is a whole scanline
	
		//apparently this dma turns off after it finishes a frame
		if(nds.VCount==191) enable = 0;
	}

	if(startmode == EDMAMode_Card) todo = MMU.dscard[PROCNUM].transfer_count / sz;
	if(startmode == EDMAMode_GXFifo) todo = std::min(todo,(u32)112);

	//determine how we're going to copy
	bool bogarted = false;
	switch(dar) {
		case EDMADestinationUpdate_Increment       :  dstinc =  sz; break;
		case EDMADestinationUpdate_Decrement       :  dstinc = (u32)-(s32)sz; break;
		case EDMADestinationUpdate_Fixed           :  dstinc =   0; break;
		case EDMADestinationUpdate_IncrementReload :  dstinc =  sz; break;
		default: bogarted = true; break;
	}
	switch(sar) {
		case EDMASourceUpdate_Increment : srcinc = sz; break;
		case EDMASourceUpdate_Decrement : srcinc = (u32)-(s32)sz; break;
		case EDMASourceUpdate_Fixed		: srcinc = 0; break;
		case EDMASourceUpdate_Invalid   : bogarted = true; break;
		default: bogarted = true; break;
	}

	//need to figure out what to do about this
	if(bogarted) 
	{
		printf("YOUR GAME IS BOGARTED!!! PLEASE REPORT!!!\n");
		assert(false);
		return;
	}

	u32 src = saddr;
	u32 dst = daddr;

	//if these do not use MMU_AT_DMA and the corresponding code in the read/write routines,
	//then danny phantom title screen will be filled with a garbage char which is made by
	//dmaing from 0x00000000 to 0x06000000
	//TODO - these might be losing out a lot by not going through the templated version anymore.
	//we might make another function to do just the raw copy op which can use them with checks
	//outside the loop
	int time_elapsed = 0;
	if(sz==4) {
		for(s32 i=(s32)todo; i>0; i--)
		{
			time_elapsed += _MMU_accesstime<PROCNUM,MMU_AT_DMA,32,MMU_AD_READ,TRUE>(src,true);
			time_elapsed += _MMU_accesstime<PROCNUM,MMU_AT_DMA,32,MMU_AD_WRITE,TRUE>(dst,true);
			u32 temp = _MMU_read32(procnum,MMU_AT_DMA,src);
			_MMU_write32(procnum,MMU_AT_DMA,dst, temp);
			dst += dstinc;
			src += srcinc;
		}
	} else {
		for(s32 i=(s32)todo; i>0; i--)
		{
			time_elapsed += _MMU_accesstime<PROCNUM,MMU_AT_DMA,16,MMU_AD_READ,TRUE>(src,true);
			time_elapsed += _MMU_accesstime<PROCNUM,MMU_AT_DMA,16,MMU_AD_WRITE,TRUE>(dst,true);
			u16 temp = _MMU_read16(procnum,MMU_AT_DMA,src);
			_MMU_write16(procnum,MMU_AT_DMA,dst, temp);
			dst += dstinc;
			src += srcinc;
		}
	}

	//printf("ARM%c dma of size %d from 0x%08X to 0x%08X took %d cycles\n",PROCNUM==0?'9':'7',todo*sz,saddr,daddr,time_elapsed);

	//reschedule an event for the end of this dma, and figure out how much it cost us
	doSchedule();
	nextEvent += time_elapsed;

	//freeze the ARM9 bus for the duration of this DMA
	//thats not entirely accurate
	if(procnum==ARMCPU_ARM9) 
		nds.freezeBus |= (1<<(chan+1));
		
	//write back the addresses
	saddr = src;
	if(dar != EDMADestinationUpdate_IncrementReload) //but dont write back dst if we were supposed to reload
		daddr = dst;

	if(!repeatMode)
	{
		if(startmode == EDMAMode_Card)
			wordcount = 0;
		else
			wordcount -= todo;
	}
}

void triggerDma(EDMAMode mode)
{
	MACRODO2(0, {
		const int i=X;
		MACRODO4(0, {
			const int j=X;
			MMU_new.dma[i][j].tryTrigger(mode);
		});
	});
}

void DmaController::tryTrigger(EDMAMode mode)
{
	if(startmode != mode) return;
	if(!enable) return;

	//hmm dont trigger it if its already running! 
	//but paused things need triggers to continue
	if(running && !paused) return; 
	triggered = TRUE;
	doSchedule();
}

void DmaController::doSchedule()
{
	dmaCheck = TRUE;
	nextEvent = nds_timer;
	NDS_RescheduleDMA();
}


void DmaController::doPause()
{
	triggered = FALSE;
	paused = TRUE;
}

void DmaController::doStop()
{
	//if(procnum==0) printf("%08lld stop type %d dma#%d\n",nds_timer,startmode,chan);
	running = FALSE;
	if(!repeatMode) enable = FALSE;
	if(irq) {
		NDS_makeIrq(procnum,IRQ_BIT_DMA_0+chan);
	}
}



u32 DmaController::read32()
{
	u32 ret = 0;
	ret |= enable<<31;
	ret |= irq<<30;
	ret |= _startmode<<27;
	ret |= bitWidth<<26;
	ret |= repeatMode<<25;
	ret |= sar<<23;
	ret |= dar<<21;
	ret |= wordcount;
	//printf("dma %d,%d READ  %08X\n",procnum,chan,ret);
	return ret;
}

static INLINE void write_auxspicnt(const int PROCNUM, const int size, const int adr, const int val)
{
	u16 oldCnt = MMU.AUX_SPI_CNT;

	switch(size)
	{
		case 16:
			MMU.AUX_SPI_CNT = val;
			break;
		case 8:
			T1WriteByte((u8*)&MMU.AUX_SPI_CNT, adr, val); 
			break;
	}

	bool csOld = (oldCnt & (1 << 6))?true:false;
	bool cs = (MMU.AUX_SPI_CNT & (1 << 6))?true:false;
	bool spi = (MMU.AUX_SPI_CNT & (1 << 13))?true:false;

	if ((!cs && csOld) || (spi && (oldCnt == 0) && !cs))
	{
		//printf("MMU%c: CS changed from HIGH to LOW *****\n", PROCNUM?'7':'9');
		slot1_device->auxspi_reset(PROCNUM);
	}

	//printf("MMU%c: cnt %04X, old %04X\n", PROCNUM?'7':'9', MMU.AUX_SPI_CNT, oldCnt);
}

template <u8 PROCNUM>
bool validateIORegsWrite(u32 addr, u8 size, u32 val)
{
	if (PROCNUM == ARMCPU_ARM9)
	{
		switch (addr & 0x0FFFFFFC)
		{
			// Display Engine A
			case REG_DISPA_DISPCNT:
			case REG_DISPA_DISPSTAT:
			case REG_DISPA_VCOUNT:
				// same as GBA...
			case REG_DISPA_BG0CNT:
			case REG_DISPA_BG1CNT:
			case REG_DISPA_BG2CNT:
			case REG_DISPA_BG3CNT:
			case REG_DISPA_BG0HOFS:
			case REG_DISPA_BG0VOFS:
			case REG_DISPA_BG1HOFS:
			case REG_DISPA_BG1VOFS:
			case REG_DISPA_BG2HOFS:
			case REG_DISPA_BG2VOFS:
			case REG_DISPA_BG3HOFS:
			case REG_DISPA_BG3VOFS:
			case REG_DISPA_BG2PA:
			case REG_DISPA_BG2PB:
			case REG_DISPA_BG2PC:
			case REG_DISPA_BG2PD:
			case REG_DISPA_BG2XL:
			case REG_DISPA_BG2XH:
			case REG_DISPA_BG2YL:
			case REG_DISPA_BG2YH:
			case REG_DISPA_BG3PA:
			case REG_DISPA_BG3PB:
			case REG_DISPA_BG3PC:
			case REG_DISPA_BG3PD:
			case REG_DISPA_BG3XL:
			case REG_DISPA_BG3XH:
			case REG_DISPA_BG3YL:
			case REG_DISPA_BG3YH:
			case REG_DISPA_WIN0H:
			case REG_DISPA_WIN1H:
			case REG_DISPA_WIN0V:
			case REG_DISPA_WIN1V:
			case REG_DISPA_WININ:
			case REG_DISPA_WINOUT:
			case REG_DISPA_MOSAIC:
			case REG_DISPA_BLDCNT:
			case REG_DISPA_BLDALPHA:
			case REG_DISPA_BLDY:
				// ...GBA
			case REG_DISPA_DISP3DCNT:
			case REG_DISPA_DISPCAPCNT:
			case REG_DISPA_DISPMMEMFIFO:

			case REG_DISPA_MASTERBRIGHT:

			// DMA
			case REG_DMA0SAD:
			case REG_DMA0DAD:
			case REG_DMA0CNTL:
			case REG_DMA0CNTH:
			case REG_DMA1SAD:
			case REG_DMA1DAD:
			case REG_DMA1CNTL:
			case REG_DMA2SAD:
			case REG_DMA2DAD:
			case REG_DMA2CNTL:
			case REG_DMA2CNTH:
			case REG_DMA3SAD:
			case REG_DMA3DAD:
			case REG_DMA3CNTL:
			case REG_DMA3CNTH:
			case REG_DMA0FILL:
			case REG_DMA1FILL:
			case REG_DMA2FILL:
			case REG_DMA3FILL:

			// Timers
			case REG_TM0CNTL:
			case REG_TM0CNTH:
			case REG_TM1CNTL:
			case REG_TM1CNTH:
			case REG_TM2CNTL:
			case REG_TM2CNTH:
			case REG_TM3CNTL:
			case REG_TM3CNTH:

			// Keypad Input
			case REG_KEYINPUT:
			case REG_KEYCNT:

			// IPC
			case REG_IPCSYNC:
			case REG_IPCFIFOCNT:
			case REG_IPCFIFOSEND:

			// ROM
			case REG_AUXSPICNT:
			case REG_AUXSPIDATA:
			case REG_GCROMCTRL:
			case REG_GCCMDOUT + 0x00: case REG_GCCMDOUT + 0x04:
			case REG_ENCSEED0L:
			case REG_ENCSEED1L:
			case REG_ENCSEED0H:
			case REG_ENCSEED1H:

			// Memory/IRQ
			case REG_EXMEMCNT:
			case REG_IME:
			case REG_IE:
			case REG_IF:
			case REG_VRAMCNTA:
			case REG_VRAMCNTB:
			case REG_VRAMCNTC:
			case REG_VRAMCNTD:
			case REG_VRAMCNTE:
			case REG_VRAMCNTF:
			case REG_VRAMCNTG:
			case REG_WRAMCNT:
			case REG_VRAMCNTH:
			case REG_VRAMCNTI:

			// Math
			case REG_DIVCNT:
			case REG_DIVNUMER + 0x00: case REG_DIVNUMER + 0x04:
			case REG_DIVDENOM + 0x00: case REG_DIVDENOM + 0x04:
			case REG_DIVRESULT + 0x00: case REG_DIVRESULT + 0x04:
			case REG_DIVREMRESULT + 0x00: case REG_DIVREMRESULT + 0x04:
			case REG_SQRTCNT:
			case REG_SQRTRESULT:
			case REG_SQRTPARAM + 0x00: case REG_SQRTPARAM + 0x04:

			// Other 
			case REG_POSTFLG:
			case REG_HALTCNT:
			case REG_POWCNT1:

			//R case eng_3D_RDLINES_COUNT:

			// 3D ===============================================================
			case eng_3D_EDGE_COLOR + 0x00: case eng_3D_EDGE_COLOR + 0x04: case eng_3D_EDGE_COLOR + 0x08: case eng_3D_EDGE_COLOR + 0x0C:
			case eng_3D_ALPHA_TEST_REF:
			case eng_3D_CLEAR_COLOR:
			case eng_3D_CLEAR_DEPTH:
			case eng_3D_CLRIMAGE_OFFSET:
			case eng_3D_FOG_COLOR:
			case eng_3D_FOG_OFFSET:
			case eng_3D_FOG_TABLE + 0x00: case eng_3D_FOG_TABLE + 0x04: case eng_3D_FOG_TABLE + 0x08: case eng_3D_FOG_TABLE + 0x0C:
			case eng_3D_FOG_TABLE + 0x10: case eng_3D_FOG_TABLE + 0x14: case eng_3D_FOG_TABLE + 0x18: case eng_3D_FOG_TABLE + 0x1C:
			case eng_3D_TOON_TABLE + 0x00: case eng_3D_TOON_TABLE + 0x04: case eng_3D_TOON_TABLE + 0x08: case eng_3D_TOON_TABLE + 0x0C:
			case eng_3D_TOON_TABLE + 0x10: case eng_3D_TOON_TABLE + 0x14: case eng_3D_TOON_TABLE + 0x18: case eng_3D_TOON_TABLE + 0x1C:
			case eng_3D_TOON_TABLE + 0x20: case eng_3D_TOON_TABLE + 0x24: case eng_3D_TOON_TABLE + 0x28: case eng_3D_TOON_TABLE + 0x2C:
			case eng_3D_TOON_TABLE + 0x30: case eng_3D_TOON_TABLE + 0x34: case eng_3D_TOON_TABLE + 0x38: case eng_3D_TOON_TABLE + 0x3C:
			case eng_3D_GXFIFO + 0x00: case eng_3D_GXFIFO + 0x04: case eng_3D_GXFIFO + 0x08: case eng_3D_GXFIFO + 0x0C:
			case eng_3D_GXFIFO + 0x10: case eng_3D_GXFIFO + 0x14: case eng_3D_GXFIFO + 0x18: case eng_3D_GXFIFO + 0x1C:
			case eng_3D_GXFIFO + 0x20: case eng_3D_GXFIFO + 0x24: case eng_3D_GXFIFO + 0x28: case eng_3D_GXFIFO + 0x2C:
			case eng_3D_GXFIFO + 0x30: case eng_3D_GXFIFO + 0x34: case eng_3D_GXFIFO + 0x38: case eng_3D_GXFIFO + 0x3C:

				// 3d commands
			case cmd_3D_MTX_MODE:
			case cmd_3D_MTX_PUSH:
			case cmd_3D_MTX_POP:
			case cmd_3D_MTX_STORE:
			case cmd_3D_MTX_RESTORE:
			case cmd_3D_MTX_IDENTITY:
			case cmd_3D_MTX_LOAD_4x4:
			case cmd_3D_MTX_LOAD_4x3:
			case cmd_3D_MTX_MULT_4x4:
			case cmd_3D_MTX_MULT_4x3:
			case cmd_3D_MTX_MULT_3x3:
			case cmd_3D_MTX_SCALE:
			case cmd_3D_MTX_TRANS:
			case cmd_3D_COLOR:
			case cmd_3D_NORMA:
			case cmd_3D_TEXCOORD:
			case cmd_3D_VTX_16:
			case cmd_3D_VTX_10:
			case cmd_3D_VTX_XY:
			case cmd_3D_VTX_XZ:
			case cmd_3D_VTX_YZ:
			case cmd_3D_VTX_DIFF:
			case cmd_3D_POLYGON_ATTR:
			case cmd_3D_TEXIMAGE_PARAM:
			case cmd_3D_PLTT_BASE:
			case cmd_3D_DIF_AMB:
			case cmd_3D_SPE_EMI:
			case cmd_3D_LIGHT_VECTOR:
			case cmd_3D_LIGHT_COLOR:
			case cmd_3D_SHININESS:
			case cmd_3D_BEGIN_VTXS:
			case cmd_3D_END_VTXS:
			case cmd_3D_SWAP_BUFFERS:
			case cmd_3D_VIEWPORT:
			case cmd_3D_BOX_TEST:
			case cmd_3D_POS_TEST:
			case cmd_3D_VEC_TEST:

			case eng_3D_GXSTAT:
			//R case eng_3D_RAM_COUNT:
			case eng_3D_DISP_1DOT_DEPTH:
			//R case eng_3D_POS_RESULT + 0x00: case eng_3D_POS_RESULT + 0x04: case eng_3D_POS_RESULT + 0x08: case eng_3D_POS_RESULT + 0x0C:
			//R case eng_3D_VEC_RESULT + 0x00: case eng_3D_VEC_RESULT + 0x04:
			//R case eng_3D_CLIPMTX_RESULT + 0x00: case eng_3D_CLIPMTX_RESULT + 0x04: case eng_3D_CLIPMTX_RESULT + 0x08: case eng_3D_CLIPMTX_RESULT + 0x0C:
			//R case eng_3D_CLIPMTX_RESULT + 0x10: case eng_3D_CLIPMTX_RESULT + 0x14: case eng_3D_CLIPMTX_RESULT + 0x18: case eng_3D_CLIPMTX_RESULT + 0x1C:
			//R case eng_3D_CLIPMTX_RESULT + 0x20: case eng_3D_CLIPMTX_RESULT + 0x24: case eng_3D_CLIPMTX_RESULT + 0x28: case eng_3D_CLIPMTX_RESULT + 0x2C:
			//R case eng_3D_CLIPMTX_RESULT + 0x30: case eng_3D_CLIPMTX_RESULT + 0x34: case eng_3D_CLIPMTX_RESULT + 0x38: case eng_3D_CLIPMTX_RESULT + 0x3C:
			//R case eng_3D_VECMTX_RESULT + 0x00: case eng_3D_VECMTX_RESULT + 0x04: case eng_3D_VECMTX_RESULT + 0x08: case eng_3D_VECMTX_RESULT + 0x0C:
			//R case eng_3D_VECMTX_RESULT + 0x20:

			// 0x04001xxx
			case REG_DISPB_DISPCNT:
				// same as GBA...
			case REG_DISPB_BG0CNT:
			case REG_DISPB_BG1CNT:
			case REG_DISPB_BG2CNT:
			case REG_DISPB_BG3CNT:
			case REG_DISPB_BG0HOFS:
			case REG_DISPB_BG0VOFS:
			case REG_DISPB_BG1HOFS:
			case REG_DISPB_BG1VOFS:
			case REG_DISPB_BG2HOFS:
			case REG_DISPB_BG2VOFS:
			case REG_DISPB_BG3HOFS:
			case REG_DISPB_BG3VOFS:
			case REG_DISPB_BG2PA:
			case REG_DISPB_BG2PB:
			case REG_DISPB_BG2PC:
			case REG_DISPB_BG2PD:
			case REG_DISPB_BG2XL:
			case REG_DISPB_BG2XH:
			case REG_DISPB_BG2YL:
			case REG_DISPB_BG2YH:
			case REG_DISPB_BG3PA:
			case REG_DISPB_BG3PB:
			case REG_DISPB_BG3PC:
			case REG_DISPB_BG3PD:
			case REG_DISPB_BG3XL:
			case REG_DISPB_BG3XH:
			case REG_DISPB_BG3YL:
			case REG_DISPB_BG3YH:
			case REG_DISPB_WIN0H:
			case REG_DISPB_WIN1H:
			case REG_DISPB_WIN0V:
			case REG_DISPB_WIN1V:
			case REG_DISPB_WININ:
			case REG_DISPB_WINOUT:
			case REG_DISPB_MOSAIC:
			case REG_DISPB_BLDCNT:
			case REG_DISPB_BLDALPHA:
			case REG_DISPB_BLDY:
				// ...GBA
			case REG_DISPB_MASTERBRIGHT:

			// 0x04100000
			case REG_IPCFIFORECV:
			case REG_GCDATAIN:
				//printf("MMU9 write%02d to register %08Xh = %08Xh (PC:%08X)\n", size, addr, val, ARMPROC.instruct_adr);
				return true;

			default:
#ifdef DEVELOPER
				printf("MMU9 write%02d to undefined register %08Xh = %08Xh (PC:%08X)\n", size, addr, val, ARMPROC.instruct_adr);
#endif
				return false;
		}
	}

	// ARM7
	if (PROCNUM == ARMCPU_ARM7)
	{
		switch (addr & 0x0FFFFFFC)
		{
			case REG_DISPA_DISPSTAT:
			case REG_DISPA_VCOUNT:

			// DMA
			case REG_DMA0SAD:
			case REG_DMA0DAD:
			case REG_DMA0CNTL:
			case REG_DMA0CNTH:
			case REG_DMA1SAD:
			case REG_DMA1DAD:
			case REG_DMA1CNTL:
			case REG_DMA2SAD:
			case REG_DMA2DAD:
			case REG_DMA2CNTL:
			case REG_DMA2CNTH:
			case REG_DMA3SAD:
			case REG_DMA3DAD:
			case REG_DMA3CNTL:
			case REG_DMA3CNTH:
			case REG_DMA0FILL:
			case REG_DMA1FILL:
			case REG_DMA2FILL:
			case REG_DMA3FILL:

			// Timers
			case REG_TM0CNTL:
			case REG_TM0CNTH:
			case REG_TM1CNTL:
			case REG_TM1CNTH:
			case REG_TM2CNTL:
			case REG_TM2CNTH:
			case REG_TM3CNTL:
			case REG_TM3CNTH:

			// SIO/Keypad Input/RTC
			case REG_SIODATA32:
			case REG_SIOCNT:
			case REG_KEYINPUT:
			case REG_KEYCNT:
			case REG_RCNT:
			case REG_EXTKEYIN:
			case REG_RTC:

			// IPC
			case REG_IPCSYNC:
			case REG_IPCFIFOCNT:
			case REG_IPCFIFOSEND:

			// ROM
			case REG_AUXSPICNT:
			case REG_AUXSPIDATA:
			case REG_GCROMCTRL:
			case REG_GCCMDOUT:
			case REG_GCCMDOUT + 4:
			case REG_ENCSEED0L:
			case REG_ENCSEED1L:
			case REG_ENCSEED0H:
			case REG_ENCSEED1H:
			case REG_SPICNT:
			case REG_SPIDATA:

			// Memory/IRQ
			case REG_EXMEMCNT:
			case REG_IME:
			case REG_IE:
			case REG_IF:
			case REG_VRAMSTAT:
			case REG_WRAMSTAT:

			// Other 
			case REG_POSTFLG:
			case REG_HALTCNT:
			case REG_POWCNT2:
			case REG_BIOSPROT:
			
			// Sound

			// 0x04100000 - IPC
			case REG_IPCFIFORECV:
			case REG_GCDATAIN:
				//printf("MMU7 write%02d to register %08Xh = %08Xh (PC:%08X)\n", size, addr, val, ARMPROC.instruct_adr);
				return true;

			default:
#ifdef DEVELOPER
				printf("MMU7 write%02d to undefined register %08Xh = %08Xh (PC:%08X)\n", size, addr, val, ARMPROC.instruct_adr);
#endif
				return false;
		}
	}

	return false;
}

#if 0
template <u8 PROCNUM>
bool validateIORegsRead(u32 addr, u8 size)
{
	if (PROCNUM == ARMCPU_ARM9)
	{
		switch (addr & 0x0FFFFFFC)
		{
			// Display Engine A
			case REG_DISPA_DISPCNT:
			case REG_DISPA_DISPSTAT:
			case REG_DISPA_VCOUNT:
				// same as GBA...
			case REG_DISPA_BG0CNT:
			case REG_DISPA_BG1CNT:
			case REG_DISPA_BG2CNT:
			case REG_DISPA_BG3CNT:
			case REG_DISPA_BG0HOFS:
			case REG_DISPA_BG0VOFS:
			case REG_DISPA_BG1HOFS:
			case REG_DISPA_BG1VOFS:
			case REG_DISPA_BG2HOFS:
			case REG_DISPA_BG2VOFS:
			case REG_DISPA_BG3HOFS:
			case REG_DISPA_BG3VOFS:
			case REG_DISPA_BG2PA:
			case REG_DISPA_BG2PB:
			case REG_DISPA_BG2PC:
			case REG_DISPA_BG2PD:
			case REG_DISPA_BG2XL:
			case REG_DISPA_BG2XH:
			case REG_DISPA_BG2YL:
			case REG_DISPA_BG2YH:
			case REG_DISPA_BG3PA:
			case REG_DISPA_BG3PB:
			case REG_DISPA_BG3PC:
			case REG_DISPA_BG3PD:
			case REG_DISPA_BG3XL:
			case REG_DISPA_BG3XH:
			case REG_DISPA_BG3YL:
			case REG_DISPA_BG3YH:
			case REG_DISPA_WIN0H:
			case REG_DISPA_WIN1H:
			case REG_DISPA_WIN0V:
			case REG_DISPA_WIN1V:
			case REG_DISPA_WININ:
			case REG_DISPA_WINOUT:
			case REG_DISPA_MOSAIC:
			case REG_DISPA_BLDCNT:
			case REG_DISPA_BLDALPHA:
			case REG_DISPA_BLDY:
				// ...GBA
			case REG_DISPA_DISP3DCNT:
			case REG_DISPA_DISPCAPCNT:
			case REG_DISPA_DISPMMEMFIFO:

			case REG_DISPA_MASTERBRIGHT:

			// DMA
			case REG_DMA0SAD:
			case REG_DMA0DAD:
			case REG_DMA0CNTL:
			case REG_DMA0CNTH:
			case REG_DMA1SAD:
			case REG_DMA1DAD:
			case REG_DMA1CNTL:
			case REG_DMA2SAD:
			case REG_DMA2DAD:
			case REG_DMA2CNTL:
			case REG_DMA2CNTH:
			case REG_DMA3SAD:
			case REG_DMA3DAD:
			case REG_DMA3CNTL:
			case REG_DMA3CNTH:
			case REG_DMA0FILL:
			case REG_DMA1FILL:
			case REG_DMA2FILL:
			case REG_DMA3FILL:

			// Timers
			case REG_TM0CNTL:
			case REG_TM0CNTH:
			case REG_TM1CNTL:
			case REG_TM1CNTH:
			case REG_TM2CNTL:
			case REG_TM2CNTH:
			case REG_TM3CNTL:
			case REG_TM3CNTH:

			// Keypad Input
			case REG_KEYINPUT:
			case REG_KEYCNT:

			// IPC
			case REG_IPCSYNC:
			case REG_IPCFIFOCNT:
			case REG_IPCFIFOSEND:

			// ROM
			case REG_AUXSPICNT:
			case REG_AUXSPIDATA:
			case REG_GCROMCTRL:
			case REG_GCCMDOUT + 0x00: case REG_GCCMDOUT + 0x04:
			case REG_ENCSEED0L:
			case REG_ENCSEED1L:
			case REG_ENCSEED0H:
			case REG_ENCSEED1H:

			// Memory/IRQ
			case REG_EXMEMCNT:
			case REG_IME:
			case REG_IE:
			case REG_IF:
			case REG_VRAMCNTA:
			case REG_VRAMCNTB:
			case REG_VRAMCNTC:
			case REG_VRAMCNTD:
			case REG_VRAMCNTE:
			case REG_VRAMCNTF:
			case REG_VRAMCNTG:
			case REG_WRAMCNT:
			case REG_VRAMCNTH:
			case REG_VRAMCNTI:

			// Math
			case REG_DIVCNT:
			case REG_DIVNUMER + 0x00: case REG_DIVNUMER + 0x04:
			case REG_DIVDENOM + 0x00: case REG_DIVDENOM + 0x04:
			case REG_DIVRESULT + 0x00: case REG_DIVRESULT + 0x04:
			case REG_DIVREMRESULT + 0x00: case REG_DIVREMRESULT + 0x04:
			case REG_SQRTCNT:
			case REG_SQRTRESULT:
			case REG_SQRTPARAM + 0x00: case REG_SQRTPARAM + 0x04:

			// Other 
			case REG_POSTFLG:
			case REG_HALTCNT:
			case REG_POWCNT1:

			case eng_3D_RDLINES_COUNT:

			// 3D ===============================================================
			//W case eng_3D_EDGE_COLOR + 0x00:
			//W case eng_3D_EDGE_COLOR + 0x04:
			//W case eng_3D_EDGE_COLOR + 0x08:
			//W case eng_3D_EDGE_COLOR + 0x0C:
			//W case eng_3D_ALPHA_TEST_REF:
			//W case eng_3D_CLEAR_COLOR:
			//W case eng_3D_CLEAR_DEPTH:
			//W case eng_3D_CLRIMAGE_OFFSET:
			//W case eng_3D_FOG_COLOR:
			//W case eng_3D_FOG_OFFSET:
			//W case eng_3D_FOG_TABLE + 0x00: case eng_3D_FOG_TABLE + 0x04: case eng_3D_FOG_TABLE + 0x08: case eng_3D_FOG_TABLE + 0x0C:
			//W case eng_3D_FOG_TABLE + 0x10: case eng_3D_FOG_TABLE + 0x14: case eng_3D_FOG_TABLE + 0x18: case eng_3D_FOG_TABLE + 0x1C:
			//W case eng_3D_TOON_TABLE + 0x00: case eng_3D_TOON_TABLE + 0x04: case eng_3D_TOON_TABLE + 0x08: case eng_3D_TOON_TABLE + 0x0C:
			//W case eng_3D_TOON_TABLE + 0x10: case eng_3D_TOON_TABLE + 0x14: case eng_3D_TOON_TABLE + 0x18: case eng_3D_TOON_TABLE + 0x1C:
			//W case eng_3D_TOON_TABLE + 0x20: case eng_3D_TOON_TABLE + 0x24: case eng_3D_TOON_TABLE + 0x28: case eng_3D_TOON_TABLE + 0x2C:
			//W case eng_3D_TOON_TABLE + 0x30: case eng_3D_TOON_TABLE + 0x34: case eng_3D_TOON_TABLE + 0x38: case eng_3D_TOON_TABLE + 0x3C:
			//W case eng_3D_GXFIFO + 0x00: case eng_3D_GXFIFO + 0x04: case eng_3D_GXFIFO + 0x08: case eng_3D_GXFIFO + 0x0C:
			//W case eng_3D_GXFIFO + 0x10: case eng_3D_GXFIFO + 0x14: case eng_3D_GXFIFO + 0x18: case eng_3D_GXFIFO + 0x1C:
			//W case eng_3D_GXFIFO + 0x20: case eng_3D_GXFIFO + 0x24: case eng_3D_GXFIFO + 0x28: case eng_3D_GXFIFO + 0x2C:
			//W case eng_3D_GXFIFO + 0x30: case eng_3D_GXFIFO + 0x34: case eng_3D_GXFIFO + 0x38: case eng_3D_GXFIFO + 0x3C:

				// 3d commands
			//W case cmd_3D_MTX_MODE:
			//W case cmd_3D_MTX_PUSH:
			//W case cmd_3D_MTX_POP:
			//W case cmd_3D_MTX_STORE:
			//W case cmd_3D_MTX_RESTORE:
			//W case cmd_3D_MTX_IDENTITY:
			//W case cmd_3D_MTX_LOAD_4x4:
			//W case cmd_3D_MTX_LOAD_4x3:
			//W case cmd_3D_MTX_MULT_4x4:
			//W case cmd_3D_MTX_MULT_4x3:
			//W case cmd_3D_MTX_MULT_3x3:
			//W case cmd_3D_MTX_SCALE:
			//W case cmd_3D_MTX_TRANS:
			//W case cmd_3D_COLOR:
			//W case cmd_3D_NORMA:
			//W case cmd_3D_TEXCOORD:
			//W case cmd_3D_VTX_16:
			//W case cmd_3D_VTX_10:
			//W case cmd_3D_VTX_XY:
			//W case cmd_3D_VTX_XZ:
			//W case cmd_3D_VTX_YZ:
			//W case cmd_3D_VTX_DIFF:
			//W case cmd_3D_POLYGON_ATTR:
			//W case cmd_3D_TEXIMAGE_PARAM:
			//W case cmd_3D_PLTT_BASE:
			//W case cmd_3D_DIF_AMB:
			//W case cmd_3D_SPE_EMI:
			//W case cmd_3D_LIGHT_VECTOR:
			//W case cmd_3D_LIGHT_COLOR:
			//W case cmd_3D_SHININESS:
			//W case cmd_3D_BEGIN_VTXS:
			//W case cmd_3D_END_VTXS:
			//W case cmd_3D_SWAP_BUFFERS:
			//W case cmd_3D_VIEWPORT:
			//W case cmd_3D_BOX_TEST:
			//W case cmd_3D_POS_TEST:
			//W case cmd_3D_VEC_TEST:

			case eng_3D_GXSTAT:
			case eng_3D_RAM_COUNT:
			//W case eng_3D_DISP_1DOT_DEPTH:
			case eng_3D_POS_RESULT + 0x00: case eng_3D_POS_RESULT + 0x04: case eng_3D_POS_RESULT + 0x08: case eng_3D_POS_RESULT + 0x0C:
			case eng_3D_VEC_RESULT + 0x00: case eng_3D_VEC_RESULT + 0x04:
			case eng_3D_CLIPMTX_RESULT + 0x00: case eng_3D_CLIPMTX_RESULT + 0x04: case eng_3D_CLIPMTX_RESULT + 0x08: case eng_3D_CLIPMTX_RESULT + 0x0C:
			case eng_3D_CLIPMTX_RESULT + 0x10: case eng_3D_CLIPMTX_RESULT + 0x14: case eng_3D_CLIPMTX_RESULT + 0x18: case eng_3D_CLIPMTX_RESULT + 0x1C:
			case eng_3D_CLIPMTX_RESULT + 0x20: case eng_3D_CLIPMTX_RESULT + 0x24: case eng_3D_CLIPMTX_RESULT + 0x28: case eng_3D_CLIPMTX_RESULT + 0x2C:
			case eng_3D_CLIPMTX_RESULT + 0x30: case eng_3D_CLIPMTX_RESULT + 0x34: case eng_3D_CLIPMTX_RESULT + 0x38: case eng_3D_CLIPMTX_RESULT + 0x3C:
			case eng_3D_VECMTX_RESULT + 0x00: case eng_3D_VECMTX_RESULT + 0x04: case eng_3D_VECMTX_RESULT + 0x08: case eng_3D_VECMTX_RESULT + 0x0C:
			case eng_3D_VECMTX_RESULT + 0x20:

			// 0x04001xxx
			case REG_DISPB_DISPCNT:
				// same as GBA...
			case REG_DISPB_BG0CNT:
			case REG_DISPB_BG1CNT:
			case REG_DISPB_BG2CNT:
			case REG_DISPB_BG3CNT:
			case REG_DISPB_BG0HOFS:
			case REG_DISPB_BG0VOFS:
			case REG_DISPB_BG1HOFS:
			case REG_DISPB_BG1VOFS:
			case REG_DISPB_BG2HOFS:
			case REG_DISPB_BG2VOFS:
			case REG_DISPB_BG3HOFS:
			case REG_DISPB_BG3VOFS:
			case REG_DISPB_BG2PA:
			case REG_DISPB_BG2PB:
			case REG_DISPB_BG2PC:
			case REG_DISPB_BG2PD:
			case REG_DISPB_BG2XL:
			case REG_DISPB_BG2XH:
			case REG_DISPB_BG2YL:
			case REG_DISPB_BG2YH:
			case REG_DISPB_BG3PA:
			case REG_DISPB_BG3PB:
			case REG_DISPB_BG3PC:
			case REG_DISPB_BG3PD:
			case REG_DISPB_BG3XL:
			case REG_DISPB_BG3XH:
			case REG_DISPB_BG3YL:
			case REG_DISPB_BG3YH:
			case REG_DISPB_WIN0H:
			case REG_DISPB_WIN1H:
			case REG_DISPB_WIN0V:
			case REG_DISPB_WIN1V:
			case REG_DISPB_WININ:
			case REG_DISPB_WINOUT:
			case REG_DISPB_MOSAIC:
			case REG_DISPB_BLDCNT:
			case REG_DISPB_BLDALPHA:
			case REG_DISPB_BLDY:
				// ...GBA
			case REG_DISPB_MASTERBRIGHT:

			// 0x04100000
			case REG_IPCFIFORECV:
			case REG_GCDATAIN:
				//printf("MMU9 read%02d from register %08Xh = %08Xh (PC:%08X)\n", size, addr, T1ReadLong(MMU.ARM9_REG, addr & 0x00FFFFFF), ARMPROC.instruct_adr);
				return true;

			default:
#ifdef DEVELOPER
				printf("MMU9 read%02d from undefined register %08Xh = %08Xh (PC:%08X)\n", size, addr, T1ReadLong(MMU.ARM9_REG, addr & 0x00FFFFFF), ARMPROC.instruct_adr);
#endif
				return false;
		}
	}

	// ARM7
	if (PROCNUM == ARMCPU_ARM7)
	{
		switch (addr & 0x0FFFFFFC)
		{
			case REG_DISPA_DISPSTAT:
			case REG_DISPA_VCOUNT:

			// DMA
			case REG_DMA0SAD:
			case REG_DMA0DAD:
			case REG_DMA0CNTL:
			case REG_DMA0CNTH:
			case REG_DMA1SAD:
			case REG_DMA1DAD:
			case REG_DMA1CNTL:
			case REG_DMA2SAD:
			case REG_DMA2DAD:
			case REG_DMA2CNTL:
			case REG_DMA2CNTH:
			case REG_DMA3SAD:
			case REG_DMA3DAD:
			case REG_DMA3CNTL:
			case REG_DMA3CNTH:
			case REG_DMA0FILL:
			case REG_DMA1FILL:
			case REG_DMA2FILL:
			case REG_DMA3FILL:

			// Timers
			case REG_TM0CNTL:
			case REG_TM0CNTH:
			case REG_TM1CNTL:
			case REG_TM1CNTH:
			case REG_TM2CNTL:
			case REG_TM2CNTH:
			case REG_TM3CNTL:
			case REG_TM3CNTH:

			// SIO/Keypad Input/RTC
			case REG_SIODATA32:
			case REG_SIOCNT:
			case REG_KEYINPUT:
			case REG_KEYCNT:
			case REG_RCNT:
			case REG_EXTKEYIN:
			case REG_RTC:

			// IPC
			case REG_IPCSYNC:
			case REG_IPCFIFOCNT:
			case REG_IPCFIFOSEND:

			// ROM
			case REG_AUXSPICNT:
			case REG_AUXSPIDATA:
			case REG_GCROMCTRL:
			case REG_GCCMDOUT:
			case REG_GCCMDOUT + 4:
			case REG_ENCSEED0L:
			case REG_ENCSEED1L:
			case REG_ENCSEED0H:
			case REG_ENCSEED1H:
			case REG_SPICNT:
			case REG_SPIDATA:

			// Memory/IRQ
			case REG_EXMEMCNT:
			case REG_IME:
			case REG_IE:
			case REG_IF:
			case REG_VRAMSTAT:
			case REG_WRAMSTAT:

			// Other 
			case REG_POSTFLG:
			case REG_HALTCNT:
			case REG_POWCNT2:
			case REG_BIOSPROT:
			
			// Sound

			// 0x04100000 - IPC
			case REG_IPCFIFORECV:
			case REG_GCDATAIN:
				//printf("MMU7 read%02d from register %08Xh = %08Xh (PC:%08X)\n", size, addr, T1ReadLong(MMU.ARM9_REG, addr & 0x00FFFFFF), ARMPROC.instruct_adr);
				return true;

			default:
#ifdef DEVELOPER
				printf("MMU7 read%02d from undefined register %08Xh = %08Xh (PC:%08X)\n", size, addr, T1ReadLong(MMU.ARM7_REG, addr & 0x00FFFFFF), ARMPROC.instruct_adr);
#endif
				return false;
		}
	}

	return false;
}

#define VALIDATE_IO_REGS_READ(PROC, SIZE) if (!validateIORegsRead<PROC>(adr, SIZE)) return 0;
#else
#define VALIDATE_IO_REGS_READ(PROC, SIZE) ;
#endif

//================================================================================================== ARM9 *
//=========================================================================================================
//=========================================================================================================
//================================================= MMU write 08
void FASTCALL _MMU_ARM9_write08(u32 adr, u8 val)
{
	adr &= 0x0FFFFFFF;

	mmu_log_debug_ARM9(adr, "(write08) 0x%02X", val);

	if(adr < 0x02000000)
	{
#ifdef HAVE_JIT
		JIT_COMPILED_FUNC_KNOWNBANK(adr, ARM9_ITCM, 0x7FFF, 0) = 0;
#endif
		T1WriteByte(MMU.ARM9_ITCM, adr & 0x7FFF, val);
		return;
	}

	if (slot2_write<ARMCPU_ARM9, u8>(adr, val))
		return;

	//block 8bit writes to OAM and palette memory
	if ((adr & 0x0F000000) == 0x07000000) return;
	if ((adr & 0x0F000000) == 0x05000000) return;

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		if (!validateIORegsWrite<ARMCPU_ARM9>(adr, 8, val)) return;
		
		// TODO: add pal reg
		if (nds.power1.gpuMain == 0)
			if ((adr >= 0x04000008) && (adr <= 0x0400005F)) return;
		if (nds.power1.gpuSub == 0)
			if ((adr >= 0x04001008) && (adr <= 0x0400105F)) return;
		if (nds.power1.gfx3d_geometry == 0)
			if ((adr >= 0x04000400) && (adr <= 0x040006FF)) return;
		if (nds.power1.gfx3d_render == 0)
			if ((adr >= 0x04000320) && (adr <= 0x040003FF)) return;

		if(MMU_new.is_dma(adr)) { 
			MMU_new.write_dma(ARMCPU_ARM9,8,adr,val); 
			return;
		}

		switch(adr)
		{
			case REG_SQRTCNT: printf("ERROR 8bit SQRTCNT WRITE\n"); return;
			case REG_SQRTCNT+1: printf("ERROR 8bit SQRTCNT1 WRITE\n"); return;
			case REG_SQRTCNT+2: printf("ERROR 8bit SQRTCNT2 WRITE\n"); return;
			case REG_SQRTCNT+3: printf("ERROR 8bit SQRTCNT3 WRITE\n"); return;
			
#if 1
			case REG_DIVCNT: printf("ERROR 8bit DIVCNT WRITE\n"); return;
			case REG_DIVCNT+1: printf("ERROR 8bit DIVCNT+1 WRITE\n"); return;
			case REG_DIVCNT+2: printf("ERROR 8bit DIVCNT+2 WRITE\n"); return;
			case REG_DIVCNT+3: printf("ERROR 8bit DIVCNT+3 WRITE\n"); return;
#endif

			//fog table: only write bottom 7 bits
			case eng_3D_FOG_TABLE+0x00: case eng_3D_FOG_TABLE+0x01: case eng_3D_FOG_TABLE+0x02: case eng_3D_FOG_TABLE+0x03: 
			case eng_3D_FOG_TABLE+0x04: case eng_3D_FOG_TABLE+0x05: case eng_3D_FOG_TABLE+0x06: case eng_3D_FOG_TABLE+0x07: 
			case eng_3D_FOG_TABLE+0x08: case eng_3D_FOG_TABLE+0x09: case eng_3D_FOG_TABLE+0x0A: case eng_3D_FOG_TABLE+0x0B: 
			case eng_3D_FOG_TABLE+0x0C: case eng_3D_FOG_TABLE+0x0D: case eng_3D_FOG_TABLE+0x0E: case eng_3D_FOG_TABLE+0x0F: 
			case eng_3D_FOG_TABLE+0x10: case eng_3D_FOG_TABLE+0x11: case eng_3D_FOG_TABLE+0x12: case eng_3D_FOG_TABLE+0x13: 
			case eng_3D_FOG_TABLE+0x14: case eng_3D_FOG_TABLE+0x15: case eng_3D_FOG_TABLE+0x16: case eng_3D_FOG_TABLE+0x17: 
			case eng_3D_FOG_TABLE+0x18: case eng_3D_FOG_TABLE+0x19: case eng_3D_FOG_TABLE+0x1A: case eng_3D_FOG_TABLE+0x1B: 
			case eng_3D_FOG_TABLE+0x1C: case eng_3D_FOG_TABLE+0x1D: case eng_3D_FOG_TABLE+0x1E: case eng_3D_FOG_TABLE+0x1F: 
				val &= 0x7F;
				break;

			//ensata putchar port
			case 0x04FFF000:
				if(nds.ensataEmulation)
				{
					printf("%c",val);
					fflush(stdout);
				}
				break;

			case eng_3D_GXSTAT:
				MMU_new.gxstat.write(8,adr,val);
				break;

			case REG_DISPA_WIN0H: 	 
				GPU_setWIN0_H1(MainScreen.gpu, val);
				break ; 	 
			case REG_DISPA_WIN0H+1: 	 
				GPU_setWIN0_H0 (MainScreen.gpu, val);
				break ; 	 
			case REG_DISPA_WIN1H: 	 
				GPU_setWIN1_H1 (MainScreen.gpu,val);
				break ; 	 
			case REG_DISPA_WIN1H+1: 	 
				GPU_setWIN1_H0 (MainScreen.gpu,val);
				break ; 	 

			case REG_DISPB_WIN0H: 	 
				GPU_setWIN0_H1(SubScreen.gpu,val);
				break ; 	 
			case REG_DISPB_WIN0H+1: 	 
				GPU_setWIN0_H0(SubScreen.gpu,val);
				break ; 	 
			case REG_DISPB_WIN1H: 	 
				GPU_setWIN1_H1(SubScreen.gpu,val);
				break ; 	 
			case REG_DISPB_WIN1H+1: 	 
				GPU_setWIN1_H0(SubScreen.gpu,val);
				break ;

			case REG_DISPA_WIN0V: 	 
				GPU_setWIN0_V1(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WIN0V+1: 	 
				GPU_setWIN0_V0(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WIN1V: 	 
				GPU_setWIN1_V1(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WIN1V+1: 	 
				GPU_setWIN1_V0(MainScreen.gpu,val) ; 	 
				break ; 	 

			case REG_DISPB_WIN0V: 	 
				GPU_setWIN0_V1(SubScreen.gpu,val) ;
				break ; 	 
			case REG_DISPB_WIN0V+1: 	 
				GPU_setWIN0_V0(SubScreen.gpu,val) ;
				break ; 	 
			case REG_DISPB_WIN1V: 	 
				GPU_setWIN1_V1(SubScreen.gpu,val) ;
				break ; 	 
			case REG_DISPB_WIN1V+1: 	 
				GPU_setWIN1_V0(SubScreen.gpu,val) ;
				break ;

			case REG_DISPA_WININ: 	 
				GPU_setWININ0(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WININ+1: 	 
				GPU_setWININ1(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WINOUT: 	 
				GPU_setWINOUT(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WINOUT+1: 	 
				GPU_setWINOBJ(MainScreen.gpu,val);
				break ; 	 

			case REG_DISPB_WININ: 	 
				GPU_setWININ0(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_WININ+1: 	 
				GPU_setWININ1(SubScreen.gpu,val) ; 	 
				break ; 
			case REG_DISPB_WINOUT: 	 
				GPU_setWINOUT(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_WINOUT+1: 	 
				GPU_setWINOBJ(SubScreen.gpu,val) ; 	 
				break ;

			case REG_DISPA_BLDCNT:
				GPU_setBLDCNT_HIGH(MainScreen.gpu,val);
				break;
			case REG_DISPA_BLDCNT+1:
				GPU_setBLDCNT_LOW (MainScreen.gpu,val);
				break;

			case REG_DISPB_BLDCNT: 	 
				GPU_setBLDCNT_HIGH (SubScreen.gpu,val);
				break;
			case REG_DISPB_BLDCNT+1: 	 
				GPU_setBLDCNT_LOW (SubScreen.gpu,val);
				break;

			case REG_DISPA_BLDALPHA: 	 
				MainScreen.gpu->setBLDALPHA_EVA(val);
				break;
			case REG_DISPA_BLDALPHA+1:
				MainScreen.gpu->setBLDALPHA_EVB(val);
				break;

			case REG_DISPB_BLDALPHA:
				SubScreen.gpu->setBLDALPHA_EVA(val);
				break;
			case REG_DISPB_BLDALPHA+1:
				SubScreen.gpu->setBLDALPHA_EVB(val);
				break;

			case REG_DISPA_BLDY: 	 
				GPU_setBLDY_EVY(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_BLDY: 	 
				GPU_setBLDY_EVY(SubScreen.gpu,val) ; 	 
				break;

			case REG_AUXSPICNT:
			case REG_AUXSPICNT+1:
				write_auxspicnt(ARMCPU_ARM9, 8, adr & 1, val);
				return;
			
			case REG_AUXSPIDATA:
			{
				//if(val!=0) MMU.AUX_SPI_CMD = val & 0xFF; //zero 20-aug-2013 - this seems pointless
				u8 spidata = slot1_device->auxspi_transaction(ARMCPU_ARM9,(u8)val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, spidata);
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;
			}

			case REG_POWCNT1: writereg_POWCNT1(8,adr,val); break;
			
			case REG_DISPA_DISP3DCNT: writereg_DISP3DCNT(8,adr,val); return;
			case REG_DISPA_DISP3DCNT+1: writereg_DISP3DCNT(8,adr,val); return;

			case REG_IF: REG_IF_WriteByte<ARMCPU_ARM9>(0,val); break;
			case REG_IF+1: REG_IF_WriteByte<ARMCPU_ARM9>(1,val); break;
			case REG_IF+2: REG_IF_WriteByte<ARMCPU_ARM9>(2,val); break;
			case REG_IF+3: REG_IF_WriteByte<ARMCPU_ARM9>(3,val); break;

			case eng_3D_CLEAR_COLOR+0: case eng_3D_CLEAR_COLOR+1:
			case eng_3D_CLEAR_COLOR+2: case eng_3D_CLEAR_COLOR+3:
				T1WriteByte((u8*)&gfx3d.state.clearColor,adr-eng_3D_CLEAR_COLOR,val); 
				break;

			case REG_VRAMCNTA:
			case REG_VRAMCNTB:
			case REG_VRAMCNTC:
			case REG_VRAMCNTD:
			case REG_VRAMCNTE:
			case REG_VRAMCNTF:
			case REG_VRAMCNTG:
			case REG_WRAMCNT:
			case REG_VRAMCNTH:
			case REG_VRAMCNTI:
					MMU_VRAMmapControl(adr-REG_VRAMCNTA, val);
				break;

			case REG_DISPA_DISPMMEMFIFO:
			{
				DISP_FIFOsend(val);
				return;
			}
		#ifdef LOG_CARD
			case 0x040001A0 : /* TODO (clear): ??? */
			case 0x040001A1 :
			case 0x040001A2 :
			case 0x040001A8 :
			case 0x040001A9 :
			case 0x040001AA :
			case 0x040001AB :
			case 0x040001AC :
			case 0x040001AD :
			case 0x040001AE :
			case 0x040001AF :
						LOG("%08X : %02X\r\n", adr, val);
		#endif
		}

		MMU.MMU_MEM[ARMCPU_ARM9][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]]=val;
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr, unmapped, restricted);
	if(unmapped) return;
	if(restricted) return; //block 8bit vram writes

#ifdef HAVE_JIT
	if (JIT_MAPPED(adr, ARMCPU_ARM9))
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM9, 0) = 0;
#endif

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	MMU.MMU_MEM[ARMCPU_ARM9][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]]=val;
}

//================================================= MMU ARM9 write 16
void FASTCALL _MMU_ARM9_write16(u32 adr, u16 val)
{
	adr &= 0x0FFFFFFE;

	mmu_log_debug_ARM9(adr, "(write16) 0x%04X", val);

	if (adr < 0x02000000)
	{
#ifdef HAVE_JIT
		JIT_COMPILED_FUNC_KNOWNBANK(adr, ARM9_ITCM, 0x7FFF, 0) = 0;
#endif
		T1WriteWord(MMU.ARM9_ITCM, adr & 0x7FFF, val);
		return;
	}

	if (slot2_write<ARMCPU_ARM9, u16>(adr, val))
		return;

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		if (!validateIORegsWrite<ARMCPU_ARM9>(adr, 16, val)) return;

		// TODO: add pal reg
		if (nds.power1.gpuMain == 0)
			if ((adr >= 0x04000008) && (adr <= 0x0400005F)) return;
		if (nds.power1.gpuSub == 0)
			if ((adr >= 0x04001008) && (adr <= 0x0400105F)) return;
		if (nds.power1.gfx3d_geometry == 0)
			if ((adr >= 0x04000400) && (adr <= 0x040006FF)) return;
		if (nds.power1.gfx3d_render == 0)
			if ((adr >= 0x04000320) && (adr <= 0x040003FF)) return;

		if(MMU_new.is_dma(adr)) { 
			MMU_new.write_dma(ARMCPU_ARM9,16,adr,val); 
			return;
		}

		switch (adr >> 4)
		{
						//toon table
			case 0x0400038:
			case 0x0400039:
			case 0x040003A:
			case 0x040003B:
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[(adr & 0xFFF)>>1] = val;
				gfx3d_UpdateToonTable((adr & 0x3F) >> 1, val);
			return;
		}
		// Address is an IO register
		switch(adr)
		{
		case eng_3D_GXSTAT:
			MMU_new.gxstat.write(16,adr,val);
			break;

		//fog table: only write bottom 7 bits
		case eng_3D_FOG_TABLE+0x00: case eng_3D_FOG_TABLE+0x02: case eng_3D_FOG_TABLE+0x04: case eng_3D_FOG_TABLE+0x06:
		case eng_3D_FOG_TABLE+0x08: case eng_3D_FOG_TABLE+0x0A: case eng_3D_FOG_TABLE+0x0C: case eng_3D_FOG_TABLE+0x0E:
		case eng_3D_FOG_TABLE+0x10: case eng_3D_FOG_TABLE+0x12: case eng_3D_FOG_TABLE+0x14: case eng_3D_FOG_TABLE+0x16:
		case eng_3D_FOG_TABLE+0x18: case eng_3D_FOG_TABLE+0x1A: case eng_3D_FOG_TABLE+0x1C: case eng_3D_FOG_TABLE+0x1E:
			val &= 0x7F7F;
			break;

		case REG_DISPA_BG2XL: MainScreen.gpu->setAffineStartWord(2,0,val,0); break;
		case REG_DISPA_BG2XH: MainScreen.gpu->setAffineStartWord(2,0,val,1); break;
		case REG_DISPA_BG2YL: MainScreen.gpu->setAffineStartWord(2,1,val,0); break;
		case REG_DISPA_BG2YH: MainScreen.gpu->setAffineStartWord(2,1,val,1); break;
		case REG_DISPA_BG3XL: MainScreen.gpu->setAffineStartWord(3,0,val,0); break;
		case REG_DISPA_BG3XH: MainScreen.gpu->setAffineStartWord(3,0,val,1); break;
		case REG_DISPA_BG3YL: MainScreen.gpu->setAffineStartWord(3,1,val,0); break;
		case REG_DISPA_BG3YH: MainScreen.gpu->setAffineStartWord(3,1,val,1); break;
		case REG_DISPB_BG2XL: SubScreen.gpu->setAffineStartWord(2,0,val,0); break;
		case REG_DISPB_BG2XH: SubScreen.gpu->setAffineStartWord(2,0,val,1); break;
		case REG_DISPB_BG2YL: SubScreen.gpu->setAffineStartWord(2,1,val,0); break;
		case REG_DISPB_BG2YH: SubScreen.gpu->setAffineStartWord(2,1,val,1); break;
		case REG_DISPB_BG3XL: SubScreen.gpu->setAffineStartWord(3,0,val,0); break;
		case REG_DISPB_BG3XH: SubScreen.gpu->setAffineStartWord(3,0,val,1); break;
		case REG_DISPB_BG3YL: SubScreen.gpu->setAffineStartWord(3,1,val,0); break;
		case REG_DISPB_BG3YH: SubScreen.gpu->setAffineStartWord(3,1,val,1); break;

		case REG_DISPA_DISP3DCNT: writereg_DISP3DCNT(16,adr,val); return;

			// Alpha test reference value - Parameters:1
			case eng_3D_ALPHA_TEST_REF:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x340>>1] = val;
				gfx3d_glAlphaFunc(val);
				return;
			}
			
			case eng_3D_CLEAR_COLOR:
			case eng_3D_CLEAR_COLOR+2:
			{
				T1WriteWord((u8*)&gfx3d.state.clearColor,adr-eng_3D_CLEAR_COLOR,val);
				break;
			}

			// Clear background depth setup - Parameters:2
			case eng_3D_CLEAR_DEPTH:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x354>>1] = val;
				gfx3d_glClearDepth(val);
				return;
			}
			// Fog Color - Parameters:4b
			case eng_3D_FOG_COLOR:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x358>>1] = val;
				gfx3d_glFogColor(val);
				return;
			}
			case eng_3D_FOG_OFFSET:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x35C>>1] = val;
				gfx3d_glFogOffset(val);
				return;
			}

			case REG_DIVCNT:
				MMU_new.div.write16(val);
				execdiv();
				return;
#if 1
			case REG_DIVNUMER:
			case REG_DIVNUMER+2:
			case REG_DIVNUMER+4:
				printf("DIV: 16 write NUMER %08X. PLEASE REPORT! \n", val);
				break;
			case REG_DIVDENOM:
			case REG_DIVDENOM+2:
			case REG_DIVDENOM+4:
				printf("DIV: 16 write DENOM %08X. PLEASE REPORT! \n", val);
				break;
#endif
			case REG_SQRTCNT:
				MMU_new.sqrt.write16(val);
				execsqrt();
				return;

			case REG_DISPA_BLDCNT: 	 
				GPU_setBLDCNT(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_BLDCNT: 	 
				GPU_setBLDCNT(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_BLDALPHA: 	 
				MainScreen.gpu->setBLDALPHA(val);
				break ; 	 
			case REG_DISPB_BLDALPHA: 	 
				SubScreen.gpu->setBLDALPHA(val);
				break ; 	 
			case REG_DISPA_BLDY: 	 
				GPU_setBLDY_EVY(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_BLDY: 	 
				GPU_setBLDY_EVY(SubScreen.gpu,val) ; 	 
				break;
			case REG_DISPA_MASTERBRIGHT:
				GPU_setMasterBrightness (MainScreen.gpu, val);
				break;
				/*
			case REG_DISPA_MOSAIC: 	 
				GPU_setMOSAIC(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_MOSAIC: 	 
				GPU_setMOSAIC(SubScreen.gpu,val) ; 	 
				break ;
				*/
			//case REG_DISPA_BG0HOFS:
			//	GPU_setBGxHOFS(0, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG0VOFS:
			//	GPU_setBGxVOFS(0, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG1HOFS:
			//	GPU_setBGxHOFS(1, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG1VOFS:
			//	GPU_setBGxVOFS(1, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG2HOFS:
			//	GPU_setBGxHOFS(2, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG2VOFS:
			//	GPU_setBGxVOFS(2, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG3HOFS:
			//	GPU_setBGxHOFS(3, MainScreen.gpu, val);
			//	break;
			//case REG_DISPA_BG3VOFS:
			//	GPU_setBGxVOFS(3, MainScreen.gpu, val);
			//	break;

			case REG_DISPA_WIN0H: 	 
				GPU_setWIN0_H (MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WIN1H: 	 
				GPU_setWIN1_H(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_WIN0H: 	 
				GPU_setWIN0_H(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_WIN1H: 	 
				GPU_setWIN1_H(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WIN0V: 	 
				GPU_setWIN0_V(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WIN1V: 	 
				GPU_setWIN1_V(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_WIN0V: 	 
				GPU_setWIN0_V(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_WIN1V: 	 
				GPU_setWIN1_V(SubScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPA_WININ: 	 
				GPU_setWININ(MainScreen.gpu, val) ; 	 
				break ; 	 
			case REG_DISPA_WINOUT: 	 
				GPU_setWINOUT16(MainScreen.gpu, val) ; 	 
				break ; 	 

		/*	case REG_DISPB_BG0HOFS:
				GPU_setBGxHOFS(0, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG0VOFS:
				GPU_setBGxVOFS(0, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG1HOFS:
				GPU_setBGxHOFS(1, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG1VOFS:
				GPU_setBGxVOFS(1, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG2HOFS:
				GPU_setBGxHOFS(2, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG2VOFS:
				GPU_setBGxVOFS(2, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG3HOFS:
				GPU_setBGxHOFS(3, SubScreen.gpu, val);
				break;
			case REG_DISPB_BG3VOFS:
				GPU_setBGxVOFS(3, SubScreen.gpu, val);
				break;*/

			case REG_DISPB_WININ: 	 
				GPU_setWININ(SubScreen.gpu, val) ; 	 
				break ; 	 
			case REG_DISPB_WINOUT: 	 
				GPU_setWINOUT16(SubScreen.gpu, val) ; 	 
				break ;

			case REG_DISPB_MASTERBRIGHT:
				GPU_setMasterBrightness (SubScreen.gpu, val);
				break;

            case REG_POWCNT1:
				writereg_POWCNT1(16,adr,val);
				return;

			case REG_EXMEMCNT:
			{
				u16 remote_proc = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x204, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204, (val & 0xFF80) | (remote_proc & 0x7F));
				return;
			}

			case REG_AUXSPICNT:
				write_auxspicnt(ARMCPU_ARM9, 16, 0, val);
				return;

			case REG_AUXSPIDATA:
			{
				//if(val!=0) MMU.AUX_SPI_CMD = val & 0xFF;  //zero 20-aug-2013 - this seems pointless
				u8 spidata = slot1_device->auxspi_transaction(ARMCPU_ARM9,(u8)val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, spidata);
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;
			}

			case REG_DISPA_BG0CNT :
				//GPULOG("MAIN BG0 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 0, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x8, val);
				return;
			case REG_DISPA_BG1CNT :
				//GPULOG("MAIN BG1 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 1, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xA, val);
				return;
			case REG_DISPA_BG2CNT :
				//GPULOG("MAIN BG2 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 2, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC, val);
				return;
			case REG_DISPA_BG3CNT :
				//GPULOG("MAIN BG3 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 3, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xE, val);
				return;
			case REG_DISPB_BG0CNT :
				//GPULOG("SUB BG0 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 0, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1008, val);
				return;
			case REG_DISPB_BG1CNT :
				//GPULOG("SUB BG1 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 1, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x100A, val);
				return;
			case REG_DISPB_BG2CNT :
				//GPULOG("SUB BG2 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 2, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x100C, val);
				return;
			case REG_DISPB_BG3CNT :
				//GPULOG("SUB BG3 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 3, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x100E, val);
				return;

			case REG_VRAMCNTA:
			case REG_VRAMCNTC:
			case REG_VRAMCNTE:
			case REG_VRAMCNTG:
			case REG_VRAMCNTH:
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+1, val >> 8);
				break;

			case REG_IME:
				NDS_Reschedule();
				MMU.reg_IME[ARMCPU_ARM9] = val & 0x01;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x208, val);
				return;
			case REG_IE :
				NDS_Reschedule();
				MMU.reg_IE[ARMCPU_ARM9] = (MMU.reg_IE[ARMCPU_ARM9]&0xFFFF0000) | val;
				return;
			case REG_IE + 2 :
				NDS_Reschedule();
				MMU.reg_IE[ARMCPU_ARM9] = (MMU.reg_IE[ARMCPU_ARM9]&0xFFFF) | (((u32)val)<<16);
				return;
			case REG_IF: REG_IF_WriteWord<ARMCPU_ARM9>(0,val); return;
			case REG_IF+2: REG_IF_WriteWord<ARMCPU_ARM9>(2,val); return;

            case REG_IPCSYNC:
				MMU_IPCSync(ARMCPU_ARM9, val);
				return;
			case REG_IPCFIFOCNT:
				IPC_FIFOcnt(ARMCPU_ARM9, val);
				return;

            case REG_TM0CNTL :
            case REG_TM1CNTL :
            case REG_TM2CNTL :
            case REG_TM3CNTL :
				MMU.timerReload[ARMCPU_ARM9][(adr>>2)&3] = val;
				return;
			case REG_TM0CNTH :
			case REG_TM1CNTH :
			case REG_TM2CNTH :
			case REG_TM3CNTH :
			{
				int timerIndex	= ((adr-2)>>2)&0x3;
				write_timer(ARMCPU_ARM9, timerIndex, val);
				return;
			}

			case REG_DISPA_DISPCNT :
				{
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0) & 0xFFFF0000) | val;
					GPU_setVideoProp(MainScreen.gpu, v);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0, v);
					return;
				}
			case REG_DISPA_DISPCNT+2 : 
				{
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0) & 0xFFFF) | ((u32) val << 16);
					GPU_setVideoProp(MainScreen.gpu, v);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0, v);
				}
				return;
			case REG_DISPA_DISPCAPCNT :
				{
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x64) & 0xFFFF0000) | val; 
					GPU_set_DISPCAPCNT(v);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x64, v);
					return;
				}
			case REG_DISPA_DISPCAPCNT + 2:
				{
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x64) & 0xFFFF) | ((u32)val << 16); 
					GPU_set_DISPCAPCNT(v);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x64, v);
					return;
				}

			case REG_DISPB_DISPCNT :
				{
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1000) & 0xFFFF0000) | val;
					GPU_setVideoProp(SubScreen.gpu, v);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1000, v);
					return;
				}
			case REG_DISPB_DISPCNT+2 : 
				{
					//emu_halt();
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1000) & 0xFFFF) | ((u32) val << 16);
					GPU_setVideoProp(SubScreen.gpu, v);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1000, v);
					return;
				}

			case REG_DISPA_DISPMMEMFIFO:
			{
				DISP_FIFOsend(val);
				return;
			}

			case REG_GCROMCTRL :
				MMU_writeToGCControl<ARMCPU_ARM9>( (T1ReadLong(MMU.MMU_MEM[0][0x40], 0x1A4) & 0xFFFF0000) | val);
				return;
			case REG_GCROMCTRL+2 :
				MMU_writeToGCControl<ARMCPU_ARM9>( (T1ReadLong(MMU.MMU_MEM[0][0x40], 0x1A4) & 0xFFFF) | ((u32) val << 16));
				return;
		}

		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val); 
		return;
	}


	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr, unmapped, restricted);
	if(unmapped) return;

#ifdef HAVE_JIT
	if (JIT_MAPPED(adr, ARMCPU_ARM9))
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM9, 0) = 0;
#endif

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
} 

//================================================= MMU ARM9 write 32
void FASTCALL _MMU_ARM9_write32(u32 adr, u32 val)
{
	adr &= 0x0FFFFFFC;
	
	mmu_log_debug_ARM9(adr, "(write32) 0x%08X", val);

	if(adr<0x02000000)
	{
#ifdef HAVE_JIT
		JIT_COMPILED_FUNC_KNOWNBANK(adr, ARM9_ITCM, 0x7FFF, 0) = 0;
		JIT_COMPILED_FUNC_KNOWNBANK(adr, ARM9_ITCM, 0x7FFF, 1) = 0;
#endif
		T1WriteLong(MMU.ARM9_ITCM, adr & 0x7FFF, val);
		return ;
	}

	if (slot2_write<ARMCPU_ARM9, u32>(adr, val))
		return;

#if 0
	if ((adr & 0xFF800000) == 0x04800000) {
		// access to non regular hw registers
		// return to not overwrite valid data
		return ;
	}
#endif

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		if (!validateIORegsWrite<ARMCPU_ARM9>(adr, 32, val)) return;

		// TODO: add pal reg
		if (nds.power1.gpuMain == 0)
			if ((adr >= 0x04000008) && (adr <= 0x0400005F)) return;
		if (nds.power1.gpuSub == 0)
			if ((adr >= 0x04001008) && (adr <= 0x0400105F)) return;
		if (nds.power1.gfx3d_geometry == 0)
			if ((adr >= 0x04000400) && (adr <= 0x040006FF)) return;
		if (nds.power1.gfx3d_render == 0)
			if ((adr >= 0x04000320) && (adr <= 0x040003FF)) return;

		// MightyMax: no need to do several ifs, when only one can happen
		// switch/case instead
		// both comparison >=,< per if can be replaced by one bit comparison since
		// they are 2^4 aligned and 2^4n wide
		// this looks ugly but should reduce load on register writes, they are done as 
		// lookups by the compiler
		switch (adr >> 4)
		{
			case 0x400033:		//edge color table
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[(adr & 0xFFF) >> 2] = val;
				return;

			case 0x400038:
			case 0x400039:
			case 0x40003A:
			case 0x40003B:		//toon table
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[(adr & 0xFFF) >> 2] = val;
				gfx3d_UpdateToonTable((adr & 0x3F) >> 1, val);
				return;

			case 0x400040:
			case 0x400041:
			case 0x400042:
			case 0x400043:		// FIFO Commands
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[(adr & 0xFFF) >> 2] = val;
				gfx3d_sendCommandToFIFO(val);
				return;
				
			case 0x400044:
			case 0x400045:
			case 0x400046:
			case 0x400047:
			case 0x400048:
			case 0x400049:
			case 0x40004A:
			case 0x40004B:
			case 0x40004C:
			case 0x40004D:
			case 0x40004E:
			case 0x40004F:
			case 0x400050:
			case 0x400051:
			case 0x400052:
			case 0x400053:
			case 0x400054:
			case 0x400055:
			case 0x400056:
			case 0x400057:
			case 0x400058:
			case 0x400059:
			case 0x40005A:
			case 0x40005B:
			case 0x40005C:		// Individual Commands
				if (gxFIFO.size > 254)
					nds.freezeBus |= 1;

				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[(adr & 0xFFF) >> 2] = val;
				gfx3d_sendCommand(adr, val);
				return;

			default:
				break;
		}

		if(MMU_new.is_dma(adr)) { 
			MMU_new.write_dma(ARMCPU_ARM9,32,adr,val);
			return;
		}

		switch(adr)
		{
			case REG_SQRTCNT: MMU_new.sqrt.write16((u16)val); return;
			case REG_DIVCNT: MMU_new.div.write16((u16)val); return;

			case REG_POWCNT1: writereg_POWCNT1(32,adr,val); break;

			//fog table: only write bottom 7 bits
			case eng_3D_FOG_TABLE+0x00: case eng_3D_FOG_TABLE+0x04: case eng_3D_FOG_TABLE+0x08: case eng_3D_FOG_TABLE+0x0C:
			case eng_3D_FOG_TABLE+0x10: case eng_3D_FOG_TABLE+0x14: case eng_3D_FOG_TABLE+0x18: case eng_3D_FOG_TABLE+0x1C:
				val &= 0x7F7F7F7F;
				break;


			//ensata handshaking port?
			case 0x04FFF010:
				if(nds.ensataEmulation && nds.ensataHandshake == ENSATA_HANDSHAKE_ack && val == 0x13579bdf)
					nds.ensataHandshake = ENSATA_HANDSHAKE_confirm;
				if(nds.ensataEmulation && nds.ensataHandshake == ENSATA_HANDSHAKE_confirm && val == 0xfdb97531)
				{
					printf("ENSATA HANDSHAKE COMPLETE\n");
					nds.ensataHandshake = ENSATA_HANDSHAKE_complete;
				}
				break;

			//todo - these are usually write only regs (these and 1000 more)
			//shouldnt we block them from getting written? ugh
			case eng_3D_CLIPMTX_RESULT:
				if(nds.ensataEmulation && nds.ensataHandshake == ENSATA_HANDSHAKE_none && val==0x2468ace0)
				{
					printf("ENSATA HANDSHAKE BEGIN\n");
					nds.ensataHandshake = ENSATA_HANDSHAKE_query;
				}
				break;

			case eng_3D_GXSTAT:
				MMU_new.gxstat.write32(val);
				break;
			case REG_DISPA_BG2XL:
				MainScreen.gpu->setAffineStart(2,0,val);
				return;
			case REG_DISPA_BG2YL:
				MainScreen.gpu->setAffineStart(2,1,val);
				return;
			case REG_DISPB_BG2XL:
				SubScreen.gpu->setAffineStart(2,0,val);
				return;
			case REG_DISPB_BG2YL:
				SubScreen.gpu->setAffineStart(2,1,val);
				return;
			case REG_DISPA_BG3XL:
				MainScreen.gpu->setAffineStart(3,0,val);
				return;
			case REG_DISPA_BG3YL:
				MainScreen.gpu->setAffineStart(3,1,val);
				return;
			case REG_DISPB_BG3XL:
				SubScreen.gpu->setAffineStart(3,0,val);
				return;
			case REG_DISPB_BG3YL:
				SubScreen.gpu->setAffineStart(3,1,val);
				return;

			// Alpha test reference value - Parameters:1
			case eng_3D_ALPHA_TEST_REF:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x340>>2] = val;
				gfx3d_glAlphaFunc(val);
				return;
			}

			case eng_3D_CLEAR_COLOR:
				T1WriteLong((u8*)&gfx3d.state.clearColor,0,val); 
				break;
				
			// Clear background depth setup - Parameters:2
			case eng_3D_CLEAR_DEPTH:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x354>>2] = val;
				gfx3d_glClearDepth(val);
				return;
			}
			// Fog Color - Parameters:4b
			case 0x04000358:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x358>>2] = val;
				gfx3d_glFogColor(val);
				return;
			}
			case 0x0400035C:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x35C>>2] = val;
				gfx3d_glFogOffset(val);
				return;
			}

			//case REG_DISPA_BG0HOFS:
			//	GPU_setBGxHOFS(0, MainScreen.gpu, val&0xFFFF);
			//	GPU_setBGxVOFS(0, MainScreen.gpu, (val>>16));
			//	break;

			case REG_DISPA_WININ: 	 
			{
				GPU_setWININ(MainScreen.gpu, val & 0xFFFF) ; 	 
				GPU_setWINOUT16(MainScreen.gpu, (val >> 16) & 0xFFFF) ; 	 
	            break;
			}
			case REG_DISPB_WININ:
			{
				GPU_setWININ(SubScreen.gpu, val & 0xFFFF) ; 	 
				GPU_setWINOUT16(SubScreen.gpu, (val >> 16) & 0xFFFF) ; 	 
	            break;
			}

			case REG_DISPA_WIN0H:
			{
				GPU_setWIN0_H(MainScreen.gpu, val&0xFFFF);
				GPU_setWIN1_H(MainScreen.gpu, val>>16);
				break;
			}
			case REG_DISPA_WIN0V:
			{
				GPU_setWIN0_V(MainScreen.gpu, val&0xFFFF);
				GPU_setWIN1_V(MainScreen.gpu, val>>16);
				break;
			}
			case REG_DISPB_WIN0H:
			{
				GPU_setWIN0_H(SubScreen.gpu, val&0xFFFF);
				GPU_setWIN1_H(SubScreen.gpu, val>>16);
				break;
			}
			case REG_DISPB_WIN0V:
			{
				GPU_setWIN0_V(SubScreen.gpu, val&0xFFFF);
				GPU_setWIN1_V(SubScreen.gpu, val>>16);
				break;
			}

			case REG_DISPA_MASTERBRIGHT:
				GPU_setMasterBrightness(MainScreen.gpu, val & 0xFFFF);
				break;
			case REG_DISPB_MASTERBRIGHT:
				GPU_setMasterBrightness(SubScreen.gpu, val & 0xFFFF);
				break;

			case REG_DISPA_BLDCNT:
			{
				GPU_setBLDCNT   (MainScreen.gpu,val&0xffff);
				MainScreen.gpu->setBLDALPHA(val>>16);
				break;
			}
			case REG_DISPB_BLDCNT:
			{
				GPU_setBLDCNT   (SubScreen.gpu,val&0xffff);
				SubScreen.gpu->setBLDALPHA(val>>16);
				break;
			}

			case REG_DISPA_BLDY:
				GPU_setBLDY_EVY(MainScreen.gpu,val&0xFFFF) ; 	 
				break ; 	 
			case REG_DISPB_BLDY: 	 
				GPU_setBLDY_EVY(SubScreen.gpu,val&0xFFFF);
				break;

			case REG_DISPA_DISPCNT :
				GPU_setVideoProp(MainScreen.gpu, val);
				//GPULOG("MAIN INIT 32B %08X\r\n", val);
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0, val);
				return;
				
			case REG_DISPB_DISPCNT : 
				GPU_setVideoProp(SubScreen.gpu, val);
				//GPULOG("SUB INIT 32B %08X\r\n", val);
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1000, val);
				return;

			case REG_VRAMCNTA:
			case REG_VRAMCNTE:
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+1, (val >> 8) & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+2, (val >> 16) & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+3, (val >> 24) & 0xFF);
				break;
			case REG_VRAMCNTH:
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+1, (val >> 8) & 0xFF);
				break;

			case REG_IME : 
				NDS_Reschedule();
				MMU.reg_IME[ARMCPU_ARM9] = val & 0x01;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x208, val);
				return;
				
			case REG_IE :
				NDS_Reschedule();
				MMU.reg_IE[ARMCPU_ARM9] = val;
				return;
			
			case REG_IF: REG_IF_WriteLong<ARMCPU_ARM9>(val); return;

            case REG_TM0CNTL:
            case REG_TM1CNTL:
            case REG_TM2CNTL:
            case REG_TM3CNTL:
			{
				int timerIndex = (adr>>2)&0x3;
				MMU.timerReload[ARMCPU_ARM9][timerIndex] = (u16)val;
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & 0xFFF, val);
				write_timer(ARMCPU_ARM9, timerIndex, val>>16);
				return;
			}

			case REG_DIVNUMER:
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290, val);
				execdiv();
				return;
			case REG_DIVNUMER+4:
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x294, val);
				execdiv();
				return;

            case REG_DIVDENOM :
				{
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298, val);
					execdiv();
					return;
				}
			case REG_DIVDENOM+4 :
				{
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x29C, val);
					execdiv();
					return;
				}

			case REG_SQRTPARAM :
			{
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B8, val);
				execsqrt();
				return;
			}
			case REG_SQRTPARAM+4 :
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2BC, val);
				execsqrt();
				return;
			
			case REG_IPCSYNC:
				MMU_IPCSync(ARMCPU_ARM9, val);
				return;
			case REG_IPCFIFOCNT:
				IPC_FIFOcnt(ARMCPU_ARM9, val);
				return;
			case REG_IPCFIFOSEND:
				IPC_FIFOsend(ARMCPU_ARM9, val);
				return;

           
			case REG_GCROMCTRL :
				MMU_writeToGCControl<ARMCPU_ARM9>(val);
				return;
			case REG_DISPA_DISPCAPCNT :
				//INFO("MMU write32: REG_DISPA_DISPCAPCNT 0x%X\n", val);
				GPU_set_DISPCAPCNT(val);
				T1WriteLong(MMU.ARM9_REG, 0x64, val);
				return;
				
			case REG_DISPA_BG0CNT :
				GPU_setBGProp(MainScreen.gpu, 0, (val&0xFFFF));
				GPU_setBGProp(MainScreen.gpu, 1, (val>>16));
				//if((val>>16)==0x400) emu_halt();
				T1WriteLong(MMU.ARM9_REG, 8, val);
				return;
			case REG_DISPA_BG2CNT :
					GPU_setBGProp(MainScreen.gpu, 2, (val&0xFFFF));
					GPU_setBGProp(MainScreen.gpu, 3, (val>>16));
					T1WriteLong(MMU.ARM9_REG, 0xC, val);
				return;
			case REG_DISPB_BG0CNT :
					GPU_setBGProp(SubScreen.gpu, 0, (val&0xFFFF));
					GPU_setBGProp(SubScreen.gpu, 1, (val>>16));
					T1WriteLong(MMU.ARM9_REG, 0x1008, val);
				return;
			case REG_DISPB_BG2CNT :
					GPU_setBGProp(SubScreen.gpu, 2, (val&0xFFFF));
					GPU_setBGProp(SubScreen.gpu, 3, (val>>16));
					T1WriteLong(MMU.ARM9_REG, 0x100C, val);
				return;
			case REG_DISPA_DISPMMEMFIFO:
			{
				DISP_FIFOsend(val);
				return;
			}

			case REG_DISPA_DISP3DCNT: writereg_DISP3DCNT(32,adr,val); return;

			case REG_GCDATAIN:
				MMU_writeToGC<ARMCPU_ARM9>(val);
				return;
		}

		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr, unmapped, restricted);
	if(unmapped) return;

#ifdef HAVE_JIT
	if (JIT_MAPPED(adr, ARMCPU_ARM9))
	{
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM9, 0) = 0;
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM9, 1) = 0;
	}
#endif

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
}

//================================================= MMU ARM9 read 08
u8 FASTCALL _MMU_ARM9_read08(u32 adr)
{
	adr &= 0x0FFFFFFF;
	
	mmu_log_debug_ARM9(adr, "(read08) 0x%02X", MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF]]);

	if(adr<0x02000000)
		return T1ReadByte(MMU.ARM9_ITCM, adr&0x7FFF);

	u8 slot2_val;
	if (slot2_read<ARMCPU_ARM9, u8>(adr, slot2_val))
		return slot2_val;

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		VALIDATE_IO_REGS_READ(ARMCPU_ARM9, 8);
		
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM9,8,adr);

		switch(adr)
		{
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM9>();
			case REG_IF+1: return (MMU.gen_IF<ARMCPU_ARM9>()>>8);
			case REG_IF+2: return (MMU.gen_IF<ARMCPU_ARM9>()>>16);
			case REG_IF+3: return (MMU.gen_IF<ARMCPU_ARM9>()>>24);

			case REG_WRAMCNT:
				return MMU.WRAMCNT;

			case REG_DISPA_DISPSTAT:
				break;
			case REG_DISPA_DISPSTAT+1:
				break;
			case REG_DISPx_VCOUNT: return nds.VCount & 0xFF;
			case REG_DISPx_VCOUNT+1: return (nds.VCount>>8) & 0xFF;

			case REG_SQRTCNT: return (MMU_new.sqrt.read16() & 0xFF);
			case REG_SQRTCNT+1: return ((MMU_new.sqrt.read16()>>8) & 0xFF);
				
			//sqrtcnt isnt big enough for these to exist. but they'd probably return 0 so its ok
			case REG_SQRTCNT+2: printf("ERROR 8bit SQRTCNT+2 READ\n"); return 0;
			case REG_SQRTCNT+3: printf("ERROR 8bit SQRTCNT+3 READ\n"); return 0;

			//Nostalgia's options menu requires that these work
			case REG_DIVCNT: return (MMU_new.div.read16() & 0xFF);
			case REG_DIVCNT+1: return ((MMU_new.div.read16()>>8) & 0xFF);

			//divcnt isnt big enough for these to exist. but they'd probably return 0 so its ok
			case REG_DIVCNT+2: printf("ERROR 8bit DIVCNT+2 READ\n"); return 0;
			case REG_DIVCNT+3: printf("ERROR 8bit DIVCNT+3 READ\n"); return 0;

			//fog table: write only
			case eng_3D_FOG_TABLE+0x00: case eng_3D_FOG_TABLE+0x01: case eng_3D_FOG_TABLE+0x02: case eng_3D_FOG_TABLE+0x03: 
			case eng_3D_FOG_TABLE+0x04: case eng_3D_FOG_TABLE+0x05: case eng_3D_FOG_TABLE+0x06: case eng_3D_FOG_TABLE+0x07: 
			case eng_3D_FOG_TABLE+0x08: case eng_3D_FOG_TABLE+0x09: case eng_3D_FOG_TABLE+0x0A: case eng_3D_FOG_TABLE+0x0B: 
			case eng_3D_FOG_TABLE+0x0C: case eng_3D_FOG_TABLE+0x0D: case eng_3D_FOG_TABLE+0x0E: case eng_3D_FOG_TABLE+0x0F: 
			case eng_3D_FOG_TABLE+0x10: case eng_3D_FOG_TABLE+0x11: case eng_3D_FOG_TABLE+0x12: case eng_3D_FOG_TABLE+0x13: 
			case eng_3D_FOG_TABLE+0x14: case eng_3D_FOG_TABLE+0x15: case eng_3D_FOG_TABLE+0x16: case eng_3D_FOG_TABLE+0x17: 
			case eng_3D_FOG_TABLE+0x18: case eng_3D_FOG_TABLE+0x19: case eng_3D_FOG_TABLE+0x1A: case eng_3D_FOG_TABLE+0x1B: 
			case eng_3D_FOG_TABLE+0x1C: case eng_3D_FOG_TABLE+0x1D: case eng_3D_FOG_TABLE+0x1E: case eng_3D_FOG_TABLE+0x1F: 
				return 0;

			case REG_POWCNT1: 
			case REG_POWCNT1+1: 
			case REG_POWCNT1+2: 
			case REG_POWCNT1+3:
				return readreg_POWCNT1(8,adr);

			case eng_3D_GXSTAT:
				return MMU_new.gxstat.read(8,adr);

			case REG_DISPA_DISP3DCNT: return readreg_DISP3DCNT(8,adr);
			case REG_DISPA_DISP3DCNT+1: return readreg_DISP3DCNT(8,adr);
			case REG_DISPA_DISP3DCNT+2: return readreg_DISP3DCNT(8,adr);
			case REG_DISPA_DISP3DCNT+3: return readreg_DISP3DCNT(8,adr);

			case REG_KEYINPUT:
				LagFrameFlag=0;
				break;
		}
	}

	bool unmapped, restricted;	
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr, unmapped, restricted);
	if(unmapped) return 0;

	return MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF]];
}

//================================================= MMU ARM9 read 16
u16 FASTCALL _MMU_ARM9_read16(u32 adr)
{    
	adr &= 0x0FFFFFFE;

	mmu_log_debug_ARM9(adr, "(read16) 0x%04X", T1ReadWord_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM9][adr >> 20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr >> 20]));

	if(adr<0x02000000)
		return T1ReadWord_guaranteedAligned(MMU.ARM9_ITCM, adr & 0x7FFE);	

	u16 slot2_val;
	if (slot2_read<ARMCPU_ARM9, u16>(adr, slot2_val))
		return slot2_val;

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		VALIDATE_IO_REGS_READ(ARMCPU_ARM9, 16);

		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM9,16,adr); 

		switch(adr)
		{
			case REG_DISPA_DISPSTAT:
				break;

			case REG_SQRTCNT: return MMU_new.sqrt.read16();
			//sqrtcnt isnt big enough for this to exist. but it'd probably return 0 so its ok
			case REG_SQRTCNT+2: printf("ERROR 16bit SQRTCNT+2 READ\n"); return 0;

			case REG_DIVCNT: return MMU_new.div.read16();
			//divcnt isnt big enough for this to exist. but it'd probably return 0 so its ok
			case REG_DIVCNT+2: printf("ERROR 16bit DIVCNT+2 READ\n"); return 0;

			case eng_3D_GXSTAT: return MMU_new.gxstat.read(16,adr);

			case REG_DISPA_VCOUNT:
				if(nds.ensataEmulation && nds.ensataHandshake == ENSATA_HANDSHAKE_query)
				{
					nds.ensataHandshake = ENSATA_HANDSHAKE_ack;
					return 270;
				} 
				else 
					return nds.VCount;

			// ============================================= 3D
			case eng_3D_RAM_COUNT:
				return 0;
				//almost worthless for now
				//return (gfx3d_GetNumPolys());
			case eng_3D_RAM_COUNT+2:
				return 0;
				//almost worthless for now
				//return (gfx3d_GetNumVertex());
			// ============================================= 3D end
			case REG_IME :
				return (u16)MMU.reg_IME[ARMCPU_ARM9];

			
			case REG_IE :
				return (u16)MMU.reg_IE[ARMCPU_ARM9];
			case REG_IE + 2 :
				return (u16)(MMU.reg_IE[ARMCPU_ARM9]>>16);
				
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM9>();
			case REG_IF+2: return MMU.gen_IF<ARMCPU_ARM9>()>>16;

			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
				return read_timer(ARMCPU_ARM9,(adr&0xF)>>2);

			case REG_AUXSPICNT:
				return MMU.AUX_SPI_CNT;

            case REG_POWCNT1: 
			case REG_POWCNT1+2:
				return readreg_POWCNT1(16,adr);

			case REG_DISPA_DISP3DCNT: return readreg_DISP3DCNT(16,adr);
			case REG_DISPA_DISP3DCNT+2: return readreg_DISP3DCNT(16,adr);

			case REG_KEYINPUT:
				LagFrameFlag=0;
				break;

			//fog table: write only
			case eng_3D_FOG_TABLE+0x00: case eng_3D_FOG_TABLE+0x02: case eng_3D_FOG_TABLE+0x04: case eng_3D_FOG_TABLE+0x06:
			case eng_3D_FOG_TABLE+0x08: case eng_3D_FOG_TABLE+0x0A: case eng_3D_FOG_TABLE+0x0C: case eng_3D_FOG_TABLE+0x0E:
			case eng_3D_FOG_TABLE+0x10: case eng_3D_FOG_TABLE+0x12: case eng_3D_FOG_TABLE+0x14: case eng_3D_FOG_TABLE+0x16:
			case eng_3D_FOG_TABLE+0x18: case eng_3D_FOG_TABLE+0x1A: case eng_3D_FOG_TABLE+0x1C: case eng_3D_FOG_TABLE+0x1E:
				return 0;
		}

		return  T1ReadWord_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]);
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr,unmapped, restricted);
	if(unmapped) return 0;
	
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF
	return T1ReadWord_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM9][adr >> 20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr >> 20]); 
}

//================================================= MMU ARM9 read 32
u32 FASTCALL _MMU_ARM9_read32(u32 adr)
{
	adr &= 0x0FFFFFFC;

	mmu_log_debug_ARM9(adr, "(read32) 0x%08X", T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM9][adr >> 20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]));

	if(adr<0x02000000) 
		return T1ReadLong_guaranteedAligned(MMU.ARM9_ITCM, adr&0x7FFC);

	u32 slot2_val;
	if (slot2_read<ARMCPU_ARM9, u32>(adr, slot2_val))
		return slot2_val;

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		VALIDATE_IO_REGS_READ(ARMCPU_ARM9, 32);
		
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM9,32,adr); 

		switch(adr)
		{
			case REG_DSIMODE:
				if(!nds.Is_DSI()) break;
				return 1;
			case 0x04004008:
				if(!nds.Is_DSI()) break;
				return 0x8000;

			case REG_DISPA_DISPSTAT:
				break;

			case REG_DISPx_VCOUNT:
				return nds.VCount;

			//despite these being 16bit regs,
			//Dolphin Island Underwater Adventures uses this amidst seemingly reasonable divs so we're going to emulate it.
			//well, it's pretty reasonable to read them as 32bits though, isnt it?
			case REG_DIVCNT: return MMU_new.div.read16();
			case REG_SQRTCNT: return MMU_new.sqrt.read16(); //I guess we'll do this also

			//fog table: write only
			case eng_3D_FOG_TABLE+0x00: case eng_3D_FOG_TABLE+0x04: case eng_3D_FOG_TABLE+0x08: case eng_3D_FOG_TABLE+0x0C:
			case eng_3D_FOG_TABLE+0x10: case eng_3D_FOG_TABLE+0x14: case eng_3D_FOG_TABLE+0x18: case eng_3D_FOG_TABLE+0x1C:
				return 0;

			case eng_3D_CLIPMTX_RESULT:
			case eng_3D_CLIPMTX_RESULT+4:
			case eng_3D_CLIPMTX_RESULT+8:
			case eng_3D_CLIPMTX_RESULT+12:
			case eng_3D_CLIPMTX_RESULT+16:
			case eng_3D_CLIPMTX_RESULT+20:
			case eng_3D_CLIPMTX_RESULT+24:
			case eng_3D_CLIPMTX_RESULT+28:
			case eng_3D_CLIPMTX_RESULT+32:
			case eng_3D_CLIPMTX_RESULT+36:
			case eng_3D_CLIPMTX_RESULT+40:
			case eng_3D_CLIPMTX_RESULT+44:
			case eng_3D_CLIPMTX_RESULT+48:
			case eng_3D_CLIPMTX_RESULT+52:
			case eng_3D_CLIPMTX_RESULT+56:
			case eng_3D_CLIPMTX_RESULT+60:
			{
				//LOG("4000640h..67Fh - CLIPMTX_RESULT - Read Current Clip Coordinates Matrix (R)");
				return gfx3d_GetClipMatrix ((adr-0x04000640)/4);
			}
			case eng_3D_VECMTX_RESULT:
			case eng_3D_VECMTX_RESULT+4:
			case eng_3D_VECMTX_RESULT+8:
			case eng_3D_VECMTX_RESULT+12:
			case eng_3D_VECMTX_RESULT+16:
			case eng_3D_VECMTX_RESULT+20:
			case eng_3D_VECMTX_RESULT+24:
			case eng_3D_VECMTX_RESULT+28:
			case eng_3D_VECMTX_RESULT+32:
			{
				//LOG("4000680h..6A3h - VECMTX_RESULT - Read Current Directional Vector Matrix (R)");
				return gfx3d_GetDirectionalMatrix ((adr-0x04000680)/4);
			}

			case eng_3D_RAM_COUNT:
			{
				return (gfx3d_GetNumPolys()) | ((gfx3d_GetNumVertex()) << 16);
				//LOG ("read32 - RAM_COUNT -> 0x%X", ((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF]))[(adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF])>>2]);
			}

			case eng_3D_POS_RESULT:
			case eng_3D_POS_RESULT+4:
			case eng_3D_POS_RESULT+8:
			case eng_3D_POS_RESULT+12:
			{
				return gfx3d_glGetPosRes((adr & 0xF) >> 2);
			}
			case eng_3D_GXSTAT:
				return MMU_new.gxstat.read(32,adr);
			//	======================================== 3D end

			
			case REG_IME :
				return MMU.reg_IME[ARMCPU_ARM9];
			case REG_IE :
				return MMU.reg_IE[ARMCPU_ARM9];
			
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM9>();

			case REG_IPCFIFORECV :
				return IPC_FIFOrecv(ARMCPU_ARM9);
			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
				{
					u32 val = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], (adr + 2) & 0xFFF);
					return MMU.timer[ARMCPU_ARM9][(adr&0xF)>>2] | (val<<16);
				}	
     
			case REG_GCDATAIN: 
				return MMU_readFromGC<ARMCPU_ARM9>();

			case REG_POWCNT1: return readreg_POWCNT1(32,adr);
			case REG_DISPA_DISP3DCNT: return readreg_DISP3DCNT(32,adr);

			case REG_KEYINPUT:
				LagFrameFlag=0;
				break;
		}
		return T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]);
	}
	
	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr,unmapped, restricted);
	if(unmapped) return 0;

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [zeromus, inspired by shash]
	return T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM9][adr >> 20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]);
}
//================================================================================================== ARM7 *
//=========================================================================================================
//=========================================================================================================
//================================================= MMU ARM7 write 08
void FASTCALL _MMU_ARM7_write08(u32 adr, u8 val)
{
	adr &= 0x0FFFFFFF;

	mmu_log_debug_ARM7(adr, "(write08) 0x%02X", val);

	if (adr < 0x02000000) return; //can't write to bios or entire area below main memory

	if (slot2_write<ARMCPU_ARM7, u8>(adr, val))
		return;

	if ((adr >= 0x04000400) && (adr < 0x04000520)) 
	{
		SPU_WriteByte(adr, val);
		return;
	}

	if ((adr & 0xFFFF0000) == 0x04800000)
	{
		/* is wifi hardware, dont intermix with regular hardware registers */
		// 8-bit writes to wifi I/O and RAM are ignored
		// Reference: http://nocash.emubase.de/gbatek.htm#dswifiiomap
		return;
	}

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		if (!validateIORegsWrite<ARMCPU_ARM7>(adr, 8, val)) return;

		if(MMU_new.is_dma(adr)) { MMU_new.write_dma(ARMCPU_ARM7,8,adr,val); return; }

		switch(adr)
		{
			case REG_IF: REG_IF_WriteByte<ARMCPU_ARM7>(0,val); break;
			case REG_IF+1: REG_IF_WriteByte<ARMCPU_ARM7>(1,val); break;
			case REG_IF+2: REG_IF_WriteByte<ARMCPU_ARM7>(2,val); break;
			case REG_IF+3: REG_IF_WriteByte<ARMCPU_ARM7>(3,val); break;

			case REG_POSTFLG:
				//printf("ARM7: write POSTFLG %02X PC:0x%08X\n", val, NDS_ARM7.instruct_adr);

				//The NDS7 register can be written to only from code executed in BIOS.
				if (NDS_ARM7.instruct_adr > 0x3FFF) return;
#ifdef HAVE_JIT
				// hack for firmware boot in JIT mode
				if (CommonSettings.UseExtFirmware && CommonSettings.BootFromFirmware && firmware->loaded() && val == 1)
					CommonSettings.jit_max_block_size = saveBlockSizeJIT;
#endif
				break;

			case REG_HALTCNT:
				{
					//printf("ARM7: Halt 0x%02X IME %02X, IE 0x%08X, IF 0x%08X PC 0x%08X\n", val, MMU.reg_IME[1], MMU.reg_IE[1], MMU.reg_IF_bits[1], NDS_ARM7.instruct_adr);
					switch(val)
					{
						//case 0x10: printf("GBA mode unsupported\n"); break;
						case 0xC0: NDS_Sleep(); break;
						case 0x80: armcpu_Wait4IRQ(&NDS_ARM7); break;
						default: break;
					}
					break;
				}
				
			case REG_RTC:
				rtcWrite(val);
				return;

			case REG_AUXSPICNT:
			case REG_AUXSPICNT+1:
				write_auxspicnt(ARMCPU_ARM7, 8, adr & 1, val);
				return;

			case REG_AUXSPIDATA:
			{
				//if(val!=0) MMU.AUX_SPI_CMD = val & 0xFF; //zero 20-aug-2013 - this seems pointless
				u8 spidata = slot1_device->auxspi_transaction(ARMCPU_ARM7,(u8)val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, spidata);
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;
			}

			case REG_SPIDATA:
				// CrazyMax: 27 May 2013: BIOS write 8bit commands to flash controller when load firmware header 
				// into RAM at 0x027FF830
				MMU_writeToSPIData(val);
				return;
		}
		MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]]=val;
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return;

#ifdef HAVE_JIT
	if (JIT_MAPPED(adr, ARMCPU_ARM7))
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM7, 0) = 0;
#endif
	
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]]=val;
}

//================================================= MMU ARM7 write 16
void FASTCALL _MMU_ARM7_write16(u32 adr, u16 val)
{
	adr &= 0x0FFFFFFE;

	mmu_log_debug_ARM7(adr, "(write16) 0x%04X", val);

	if (adr < 0x02000000) return; //can't write to bios or entire area below main memory

	if (slot2_write<ARMCPU_ARM7, u16>(adr, val))
		return;

	if ((adr >= 0x04000400) && (adr < 0x04000520))
	{
		SPU_WriteWord(adr, val);
		return;
	}

	//wifi mac access
	if ((adr & 0xFFFF0000) == 0x04800000)
	{
		WIFI_write16(adr,val);
		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x48], adr&MMU.MMU_MASK[ARMCPU_ARM7][0x48], val);
		return;
	}

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		if (!validateIORegsWrite<ARMCPU_ARM7>(adr, 16, val)) return;

		if(MMU_new.is_dma(adr)) { MMU_new.write_dma(ARMCPU_ARM7,16,adr,val); return; }

		//Address is an IO register
		switch(adr)
		{
		case REG_DISPA_VCOUNT:
			if (nds.VCount >= 202 && nds.VCount <= 212)
			{
				printf("VCOUNT set to %i (previous value %i)\n", val, nds.VCount);
				nds.VCount = val;
			}
			else
				printf("Attempt to set VCOUNT while not within 202-212 (%i), ignored\n", nds.VCount);
			return;

			case REG_RTC:
				rtcWrite(val);
				break;

			case REG_EXMEMCNT:
			{
				u16 remote_proc = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x204);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204, (val & 0x7F) | (remote_proc & 0xFF80));
			}
			return;

			case REG_EXTKEYIN: //readonly
				return;

			
			case REG_POWCNT2:
				{
					nds.power2.speakers = BIT0(val);
					nds.power2.wifi = BIT1(val);
				}
				return;


			case REG_AUXSPICNT:
				write_auxspicnt(ARMCPU_ARM7, 16, 0, val);
			return;

			case REG_AUXSPIDATA:
			{
				//if(val!=0) MMU.AUX_SPI_CMD = val & 0xFF; //zero 20-aug-2013 - this seems pointless
				u8 spidata = slot1_device->auxspi_transaction(ARMCPU_ARM7,(u8)val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, spidata);
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;
			}

			case REG_SPICNT :
				{
					int reset_firmware = 1;

					if ( ((MMU.SPI_CNT >> 8) & 0x3) == 1) 
					{
						if ( ((val >> 8) & 0x3) == 1) 
						{
							if ( BIT11(MMU.SPI_CNT)) 
							{
								// select held
								reset_firmware = 0;
							}
						}
					}

					//MMU.fw.com == 0; // reset fw device communication
					if ( reset_firmware) 
					{
					  // reset fw device communication
					  fw_reset_com(&MMU.fw);
					}
					MMU.SPI_CNT = val;
				
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPICNT >> 20) & 0xff], REG_SPICNT & 0xfff, val);
				}
				return;
				
			case REG_SPIDATA :
				MMU_writeToSPIData(val);
				return;

				/* NOTICE: Perhaps we have to use gbatek-like reg names instead of libnds-like ones ...*/
				
			case REG_IME : 
				NDS_Reschedule();
				MMU.reg_IME[ARMCPU_ARM7] = val & 0x01;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x208, val);
				return;
			case REG_IE :
				NDS_Reschedule();
				MMU.reg_IE[ARMCPU_ARM7] = (MMU.reg_IE[ARMCPU_ARM7]&0xFFFF0000) | val;
				return;
			case REG_IE + 2 :
				NDS_Reschedule();
				//emu_halt();
				MMU.reg_IE[ARMCPU_ARM7] = (MMU.reg_IE[ARMCPU_ARM7]&0xFFFF) | (((u32)val)<<16);
				return;
				
			case REG_IF: REG_IF_WriteWord<ARMCPU_ARM7>(0,val); return;
			case REG_IF+2: REG_IF_WriteWord<ARMCPU_ARM7>(2,val); return;
				
            case REG_IPCSYNC:
				MMU_IPCSync(ARMCPU_ARM7, val);
				return;
			case REG_IPCFIFOCNT:
				IPC_FIFOcnt(ARMCPU_ARM7, val);
				return;

            case REG_TM0CNTL :
            case REG_TM1CNTL :
            case REG_TM2CNTL :
            case REG_TM3CNTL :
				MMU.timerReload[ARMCPU_ARM7][(adr>>2)&3] = val;
				return;
			case REG_TM0CNTH :
			case REG_TM1CNTH :
			case REG_TM2CNTH :
			case REG_TM3CNTH :
			{
				int timerIndex	= ((adr-2)>>2)&0x3;
				write_timer(ARMCPU_ARM7, timerIndex, val);
				return;
			}
			
			case REG_GCROMCTRL :
				MMU_writeToGCControl<ARMCPU_ARM7>( (T1ReadLong(MMU.MMU_MEM[1][0x40], 0x1A4) & 0xFFFF0000) | val);
				return;
			case REG_GCROMCTRL+2 :
				MMU_writeToGCControl<ARMCPU_ARM7>( (T1ReadLong(MMU.MMU_MEM[1][0x40], 0x1A4) & 0xFFFF) | ((u32) val << 16));
				return;
		}

		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val); 
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return;

#ifdef HAVE_JIT
	if (JIT_MAPPED(adr, ARMCPU_ARM7))
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM7, 0) = 0;
#endif

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
}
//================================================= MMU ARM7 write 32
void FASTCALL _MMU_ARM7_write32(u32 adr, u32 val)
{
	adr &= 0x0FFFFFFC;

	mmu_log_debug_ARM7(adr, "(write32) 0x%08X", val);

	if (adr < 0x02000000) return; //can't write to bios or entire area below main memory

	if (slot2_write<ARMCPU_ARM7, u32>(adr, val))
		return;

	if ((adr >= 0x04000400) && (adr < 0x04000520))
	{
		SPU_WriteLong(adr, val);
		return;
	}

	if ((adr & 0xFFFF0000) == 0x04800000)
	{
		WIFI_write16(adr, val & 0xFFFF);
		WIFI_write16(adr+2, val >> 16);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x48], adr&MMU.MMU_MASK[ARMCPU_ARM7][0x48], val);
		return;
	}

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		if (!validateIORegsWrite<ARMCPU_ARM7>(adr, 32, val)) return;

		if(MMU_new.is_dma(adr)) { MMU_new.write_dma(ARMCPU_ARM7,32,adr,val); return; }

		switch(adr)
		{
			case REG_RTC:
				rtcWrite((u16)val);
				break;

			case REG_IME : 
				NDS_Reschedule();
				MMU.reg_IME[ARMCPU_ARM7] = val & 0x01;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x208, val);
				return;
				
			case REG_IE :
				NDS_Reschedule();
				MMU.reg_IE[ARMCPU_ARM7] = val;
				return;
			
			case REG_IF: REG_IF_WriteLong<ARMCPU_ARM7>(val); return;

            case REG_TM0CNTL:
            case REG_TM1CNTL:
            case REG_TM2CNTL:
            case REG_TM3CNTL:
			{
				int timerIndex = (adr>>2)&0x3;
				MMU.timerReload[ARMCPU_ARM7][timerIndex] = (u16)val;
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], adr & 0xFFF, val);
				write_timer(ARMCPU_ARM7, timerIndex, val>>16);
				return;
			}


			case REG_IPCSYNC:
				MMU_IPCSync(ARMCPU_ARM7, val);
				return;
			case REG_IPCFIFOCNT:
				IPC_FIFOcnt(ARMCPU_ARM7, val);
				return;
			case REG_IPCFIFOSEND:
				IPC_FIFOsend(ARMCPU_ARM7, val);
				return;
			
			case REG_GCROMCTRL :
				MMU_writeToGCControl<ARMCPU_ARM7>(val);
				return;

			case REG_GCDATAIN:
				MMU_writeToGC<ARMCPU_ARM7>(val);
				return;
		}
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return;

#ifdef HAVE_JIT
	if (JIT_MAPPED(adr, ARMCPU_ARM7))
	{
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM7, 0) = 0;
		JIT_COMPILED_FUNC_PREMASKED(adr, ARMCPU_ARM7, 1) = 0;
	}
#endif

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
}

//================================================= MMU ARM7 read 08
u8 FASTCALL _MMU_ARM7_read08(u32 adr)
{
	adr &= 0x0FFFFFFF;

	mmu_log_debug_ARM7(adr, "(read08) 0x%02X", MMU.MMU_MEM[ARMCPU_ARM7][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM7][(adr>>20)&0xFF]]);

	if (adr < 0x4000)
	{
		//the ARM7 bios can't be read by instructions outside of itself.
		//TODO - use REG_BIOSPROT
		//How accurate is this? our instruct_adr may not be exactly what the hardware uses (may use something +/- 0x08 or so)
		//This may be inaccurate at the very edge cases.
		if (NDS_ARM7.instruct_adr > 0x3FFF)
			return 0xFF;
	}

	// wifi mac access 
	if ((adr & 0xFFFF0000) == 0x04800000)
	{
		if (adr & 1)
			return (WIFI_read16(adr-1) >> 8) & 0xFF;
		else
			return WIFI_read16(adr) & 0xFF;
	}

	u8 slot2_val;
	if (slot2_read<ARMCPU_ARM7, u8>(adr, slot2_val))
		return slot2_val;

	if ((adr>=0x04000400)&&(adr<0x04000520))
	{
		return SPU_ReadByte(adr);
	}

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		VALIDATE_IO_REGS_READ(ARMCPU_ARM7, 8);
		
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM7,8,adr); 

		switch(adr)
		{
			case REG_RTC: return (u8)rtcRead();
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM7>();
			case REG_IF+1: return (MMU.gen_IF<ARMCPU_ARM7>()>>8);
			case REG_IF+2: return (MMU.gen_IF<ARMCPU_ARM7>()>>16);
			case REG_IF+3: return (MMU.gen_IF<ARMCPU_ARM7>()>>24);

			case REG_DISPx_VCOUNT: return nds.VCount&0xFF;
			case REG_DISPx_VCOUNT+1: return (nds.VCount>>8)&0xFF;

			case REG_WRAMSTAT: return MMU.WRAMCNT;
		}

		return MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]];
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return 0;

	return MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]];
}
//================================================= MMU ARM7 read 16
u16 FASTCALL _MMU_ARM7_read16(u32 adr)
{
	adr &= 0x0FFFFFFE;

	mmu_log_debug_ARM7(adr, "(read16) 0x%04X", T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr>>20)&0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr>>20)&0xFF]));

	if (adr < 0x4000)
	{
		if (NDS_ARM7.instruct_adr > 0x3FFF)
			return 0xFFFF;
	}

	//wifi mac access
	if ((adr & 0xFFFF0000) == 0x04800000)
		return WIFI_read16(adr) ;

	u16 slot2_val;
	if (slot2_read<ARMCPU_ARM7, u16>(adr, slot2_val))
		return slot2_val;

    if ((adr>=0x04000400)&&(adr<0x04000520))
    {
        return SPU_ReadWord(adr);
    }

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		VALIDATE_IO_REGS_READ(ARMCPU_ARM7, 16);

		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM7,16,adr); 

		switch(adr)
		{
			case REG_POWCNT2:
			{
				u16 ret = 0;
				ret |= nds.power2.speakers?BIT(0):0;
				ret |= nds.power2.wifi?BIT(1):0;
				return ret;
			}

			case REG_DISPx_VCOUNT: return nds.VCount;
			case REG_RTC: return rtcRead();
			case REG_IME: return (u16)MMU.reg_IME[ARMCPU_ARM7];
				
			case REG_IE:
				return (u16)MMU.reg_IE[ARMCPU_ARM7];
			case REG_IE + 2:
				return (u16)(MMU.reg_IE[ARMCPU_ARM7]>>16);
				
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM7>();
			case REG_IF+2: return MMU.gen_IF<ARMCPU_ARM7>()>>16;

			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
				return read_timer(ARMCPU_ARM7,(adr&0xF)>>2);

			case REG_VRAMSTAT:
				//make sure WRAMSTAT is stashed and then fallthrough to return the value from memory. i know, gross.
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x241, MMU.WRAMCNT);
				break;

			case REG_AUXSPICNT:
				return MMU.AUX_SPI_CNT;

			case REG_KEYINPUT:
				//here is an example of what not to do:
				//since the arm7 polls this (and EXTKEYIN) every frame, we shouldnt count this as an input check
				//LagFrameFlag=0;
				break;

			case REG_EXTKEYIN:
				{
					//this is gross. we should generate this whole reg instead of poking it in ndssystem
					u16 ret = MMU.ARM7_REG[0x136];
					if(nds.isTouch) 
						ret &= ~64;
					else ret |= 64;
					return ret;
				}
		}
		return T1ReadWord_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]); 
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return 0;

	/* Returns data from memory */
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF
	return T1ReadWord_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][adr >> 20], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr >> 20]); 
}
//================================================= MMU ARM7 read 32
u32 FASTCALL _MMU_ARM7_read32(u32 adr)
{
	adr &= 0x0FFFFFFC;

	mmu_log_debug_ARM7(adr, "(read32) 0x%08X", T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][(adr>>20)&0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr>>20)&0xFF]));

	if (adr < 0x4000)
	{
		//the ARM7 bios can't be read by instructions outside of itself.
		//TODO - use REG_BIOSPROT
		//How accurate is this? our instruct_adr may not be exactly what the hardware uses (may use something +/- 0x08 or so)
		//This may be inaccurate at the very edge cases.
		if (NDS_ARM7.instruct_adr > 0x3FFF)
			return 0xFFFFFFFF;
	}

	//wifi mac access
	if ((adr & 0xFFFF0000) == 0x04800000)
		return (WIFI_read16(adr) | (WIFI_read16(adr+2) << 16));

	u32 slot2_val;
	if (slot2_read<ARMCPU_ARM7, u32>(adr, slot2_val))
		return slot2_val;

    if ((adr>=0x04000400)&&(adr<0x04000520))
    {
        return SPU_ReadLong(adr);
    }

	// Address is an IO register
	if ((adr >> 24) == 4)
	{
		VALIDATE_IO_REGS_READ(ARMCPU_ARM7, 32);
		
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM7,32,adr); 
		
		switch(adr)
		{
			case REG_RTC: return (u32)rtcRead();
			case REG_DISPx_VCOUNT: return nds.VCount;

			case REG_IME : 
				return MMU.reg_IME[ARMCPU_ARM7];
			case REG_IE :
				return MMU.reg_IE[ARMCPU_ARM7];
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM7>();
			case REG_IPCFIFORECV :
				return IPC_FIFOrecv(ARMCPU_ARM7);
			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
			{
				u32 val = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], (adr + 2) & 0xFFF);
				return MMU.timer[ARMCPU_ARM7][(adr&0xF)>>2] | (val<<16);
			}	
			//case REG_GCROMCTRL:
			//	return MMU_readFromGCControl<ARMCPU_ARM7>();

			case REG_GCDATAIN:
				return MMU_readFromGC<ARMCPU_ARM7>();

			case REG_VRAMSTAT:
				//make sure WRAMSTAT is stashed and then fallthrough return the value from memory. i know, gross.
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x241, MMU.WRAMCNT);
				break;
		}

		return T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]);
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return 0;

	//Returns data from memory
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [zeromus, inspired by shash]
	return T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][adr >> 20], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr >> 20]);
}

//=========================================================================================================

u32 FASTCALL MMU_read32(u32 proc, u32 adr) 
{
	ASSERT_UNALIGNED((adr&3)==0);

	if(proc==0) 
		return _MMU_ARM9_read32(adr);
	else
		return _MMU_ARM7_read32(adr);
}

u16 FASTCALL MMU_read16(u32 proc, u32 adr) 
{
	ASSERT_UNALIGNED((adr&1)==0);

	if(proc==0)
		return _MMU_ARM9_read16(adr);
	else 
		return _MMU_ARM7_read16(adr);
}

u8 FASTCALL MMU_read8(u32 proc, u32 adr) 
{
	if(proc==0) 
		return _MMU_ARM9_read08(adr);
	else 
		return _MMU_ARM7_read08(adr);
}

void FASTCALL MMU_write32(u32 proc, u32 adr, u32 val)
{
	ASSERT_UNALIGNED((adr&3)==0);

	if(proc==0)
		_MMU_ARM9_write32(adr, val);
	else
		_MMU_ARM7_write32(adr,val);
}

void FASTCALL MMU_write16(u32 proc, u32 adr, u16 val)
{
	ASSERT_UNALIGNED((adr&1)==0);

	if(proc==0)
		_MMU_ARM9_write16(adr, val);
	else 
		_MMU_ARM7_write16(adr,val);
}

void FASTCALL MMU_write8(u32 proc, u32 adr, u8 val)
{
	if(proc==0) 
		_MMU_ARM9_write08(adr, val);
	else
		_MMU_ARM7_write08(adr,val);
}

void FASTCALL MMU_DumpMemBlock(u8 proc, u32 address, u32 size, u8 *buffer)
{
	u32 i;
	u32 curaddr;

	for(i = 0, curaddr = address; i < size; i++, curaddr++)
	{
		buffer[i] = _MMU_read08(proc,MMU_AT_DEBUG,curaddr);
	}
}


//these templates needed to be instantiated manually
template u32 MMU_struct::gen_IF<ARMCPU_ARM9>();
template u32 MMU_struct::gen_IF<ARMCPU_ARM7>();

////////////////////////////////////////////////////////////
//function pointer handlers for gdb stub stuff

static u16 FASTCALL arm9_prefetch16( void *data, u32 adr) {
	return _MMU_read16<ARMCPU_ARM9,MMU_AT_CODE>(adr);
}

static u32 FASTCALL arm9_prefetch32( void *data, u32 adr) {
	return _MMU_read32<ARMCPU_ARM9,MMU_AT_CODE>(adr);
}

static u8 FASTCALL arm9_read8( void *data, u32 adr) {
	return _MMU_read08<ARMCPU_ARM9>(adr);
}

static u16 FASTCALL arm9_read16( void *data, u32 adr) {
	return _MMU_read16<ARMCPU_ARM9>(adr);
}

static u32 FASTCALL arm9_read32( void *data, u32 adr) {
	return _MMU_read32<ARMCPU_ARM9>(adr);
}

static void FASTCALL arm9_write8(void *data, u32 adr, u8 val) {
	_MMU_write08<ARMCPU_ARM9>(adr, val);
}

static void FASTCALL arm9_write16(void *data, u32 adr, u16 val) {
	_MMU_write16<ARMCPU_ARM9>(adr, val);
}

static void FASTCALL arm9_write32(void *data, u32 adr, u32 val) {
	_MMU_write32<ARMCPU_ARM9>(adr, val);
}

static u16 FASTCALL arm7_prefetch16( void *data, u32 adr) {
  return _MMU_read16<ARMCPU_ARM7,MMU_AT_CODE>(adr);
}

static u32 FASTCALL arm7_prefetch32( void *data, u32 adr) {
  return _MMU_read32<ARMCPU_ARM7,MMU_AT_CODE>(adr);
}

static u8 FASTCALL arm7_read8( void *data, u32 adr) {
  return _MMU_read08<ARMCPU_ARM7>(adr);
}

static u16 FASTCALL arm7_read16( void *data, u32 adr) {
  return _MMU_read16<ARMCPU_ARM7>(adr);
}

static u32 FASTCALL arm7_read32( void *data, u32 adr) {
  return _MMU_read32<ARMCPU_ARM7>(adr);
}

static void FASTCALL arm7_write8(void *data, u32 adr, u8 val) {
  _MMU_write08<ARMCPU_ARM7>(adr, val);
}

static void FASTCALL arm7_write16(void *data, u32 adr, u16 val) {
  _MMU_write16<ARMCPU_ARM7>(adr, val);
}

static void FASTCALL arm7_write32(void *data, u32 adr, u32 val) {
  _MMU_write32<ARMCPU_ARM7>(adr, val);
}



/*
 * the base memory interfaces
 */
struct armcpu_memory_iface arm9_base_memory_iface = {
  arm9_prefetch32,
  arm9_prefetch16,

  arm9_read8,
  arm9_read16,
  arm9_read32,

  arm9_write8,
  arm9_write16,
  arm9_write32
};

struct armcpu_memory_iface arm7_base_memory_iface = {
  arm7_prefetch32,
  arm7_prefetch16,

  arm7_read8,
  arm7_read16,
  arm7_read32,

  arm7_write8,
  arm7_write16,
  arm7_write32
};

/*
 * The direct memory interface for the ARM9.
 * This avoids the ARM9 protection unit when accessing
 * memory.
 */
struct armcpu_memory_iface arm9_direct_memory_iface = {
  NULL,
  NULL,

  arm9_read8,
  arm9_read16,
  arm9_read32,

  arm9_write8,
  arm9_write16,
  arm9_write32
};


/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////

//#ifdef PROFILE_MEMORY_ACCESS
//
//#define PROFILE_PREFETCH 0
//#define PROFILE_READ 1
//#define PROFILE_WRITE 2
//
//struct mem_access_profile {
//  u64 num_accesses;
//  u32 address_mask;
//  u32 masked_value;
//};
//
//#define PROFILE_NUM_MEM_ACCESS_PROFILES 4
//
//static u64 profile_num_accesses[2][3];
//static u64 profile_unknown_addresses[2][3];
//static struct mem_access_profile
//profile_memory_accesses[2][3][PROFILE_NUM_MEM_ACCESS_PROFILES];
//
//static void
//setup_profiling( void) {
//  int i;
//
//  for ( i = 0; i < 2; i++) {
//    int access_type;
//
//    for ( access_type = 0; access_type < 3; access_type++) {
//      profile_num_accesses[i][access_type] = 0;
//      profile_unknown_addresses[i][access_type] = 0;
//
//      /*
//       * Setup the access testing structures
//       */
//      profile_memory_accesses[i][access_type][0].address_mask = 0x0e000000;
//      profile_memory_accesses[i][access_type][0].masked_value = 0x00000000;
//      profile_memory_accesses[i][access_type][0].num_accesses = 0;
//
//      /* main memory */
//      profile_memory_accesses[i][access_type][1].address_mask = 0x0f000000;
//      profile_memory_accesses[i][access_type][1].masked_value = 0x02000000;
//      profile_memory_accesses[i][access_type][1].num_accesses = 0;
//
//      /* shared memory */
//      profile_memory_accesses[i][access_type][2].address_mask = 0x0f800000;
//      profile_memory_accesses[i][access_type][2].masked_value = 0x03000000;
//      profile_memory_accesses[i][access_type][2].num_accesses = 0;
//
//      /* arm7 memory */
//      profile_memory_accesses[i][access_type][3].address_mask = 0x0f800000;
//      profile_memory_accesses[i][access_type][3].masked_value = 0x03800000;
//      profile_memory_accesses[i][access_type][3].num_accesses = 0;
//    }
//  }
//}
//
//static void
//profile_memory_access( int arm9, u32 adr, int access_type) {
//  static int first = 1;
//  int mem_profile;
//  int address_found = 0;
//
//  if ( first) {
//    setup_profiling();
//    first = 0;
//  }
//
//  profile_num_accesses[arm9][access_type] += 1;
//
//  for ( mem_profile = 0;
//        mem_profile < PROFILE_NUM_MEM_ACCESS_PROFILES &&
//          !address_found;
//        mem_profile++) {
//    if ( (adr & profile_memory_accesses[arm9][access_type][mem_profile].address_mask) ==
//         profile_memory_accesses[arm9][access_type][mem_profile].masked_value) {
//      /*printf( "adr %08x mask %08x res %08x expected %08x\n",
//              adr,
//              profile_memory_accesses[arm9][access_type][mem_profile].address_mask,
//              adr & profile_memory_accesses[arm9][access_type][mem_profile].address_mask,
//              profile_memory_accesses[arm9][access_type][mem_profile].masked_value);*/
//      address_found = 1;
//      profile_memory_accesses[arm9][access_type][mem_profile].num_accesses += 1;
//    }
//  }
//
//  if ( !address_found) {
//    profile_unknown_addresses[arm9][access_type] += 1;
//  }
//}
//
//
//static const char *access_type_strings[] = {
//  "prefetch",
//  "read    ",
//  "write   "
//};
//
//void
//print_memory_profiling( void) {
//  int arm;
//
//  printf("------ Memory access profile ------\n");
//
//  for ( arm = 0; arm < 2; arm++) {
//    int access_type;
//
//    for ( access_type = 0; access_type < 3; access_type++) {
//      int mem_profile;
//      printf("ARM%c: num of %s %lld\n",
//             arm ? '9' : '7',
//             access_type_strings[access_type],
//             profile_num_accesses[arm][access_type]);
//
//      for ( mem_profile = 0;
//            mem_profile < PROFILE_NUM_MEM_ACCESS_PROFILES;
//            mem_profile++) {
//        printf( "address %08x: %lld\n",
//                profile_memory_accesses[arm][access_type][mem_profile].masked_value,
//                profile_memory_accesses[arm][access_type][mem_profile].num_accesses);
//      }
//              
//      printf( "unknown addresses %lld\n",
//              profile_unknown_addresses[arm][access_type]);
//
//      printf( "\n");
//    }
//  }
//
//  printf("------ End of Memory access profile ------\n\n");
//}
//#else
//void
//print_memory_profiling( void) {
//}
//#endif /* End of PROFILE_MEMORY_ACCESS area */
