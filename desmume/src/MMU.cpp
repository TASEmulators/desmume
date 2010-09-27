/*	Copyright (C) 2006 yopyop
	Copyright (C) 2007 shash
	Copyright (C) 2007-2010 DeSmuME team

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
#include "addons.h"
#include "slot1.h"
#include "mic.h"
#include "movie.h"
#include "readwrite.h"
#include "MMU_timing.h"

#ifdef DO_ASSERT_UNALIGNED
#define ASSERT_UNALIGNED(x) assert(x)
#else
#define ASSERT_UNALIGNED(x)
#endif

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

#define DUP2(x)  x, x
#define DUP4(x)  x, x, x, x
#define DUP8(x)  x, x, x, x,  x, x, x, x
#define DUP16(x) x, x, x, x,  x, x, x, x,  x, x, x, x,  x, x, x, x

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
		/* AX*/	DUP16(NULL),
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
		/* AX*/	DUP16(NULL),
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
		/* AX*/	DUP16(0x0000FFFF),
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
		/* AX*/	DUP16(0x0000FFFF),
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

//maps an ARM9 BG/OBJ or LCDC address into an LCDC address, and informs the caller of whether it isn't mapped
//TODO - in cases where this does some mapping work, we could bypass the logic at the end of the _read* and _write* routines
//this is a good optimization to consider
template<int PROCNUM> 
static FORCEINLINE u32 MMU_LCDmap(u32 addr, bool& unmapped, bool& restricted)
{
	unmapped = false;
	restricted = false; //this will track whether 8bit writes are allowed

	//in case the address is entirely outside of the interesting ranges
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
				if(bank == 2) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) | 2);
				if(bank == 3) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) | 1);
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
	//dont handle wram mappings in here
	if(block == 7) {
		//wram
		return;
	}

	//first, save the texture info so we can check it for changes and trigger purges of the texcache
	MMU_struct::TextureInfo oldTexInfo = MMU.texInfo;

	//unmap everything
	MMU_VRAM_unmap_all();

	//unmap VRAM_BANK_C and VRAM_BANK_D from arm7. theyll get mapped again in a moment if necessary
	T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 2);
	T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 1);

	//write the new value to the reg
	T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x240 + block, VRAMBankCnt);

	//refresh all bank settings
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
	MMU_VRAMmapRefreshBank(VRAM_BANK_D);
	MMU_VRAMmapRefreshBank(VRAM_BANK_C);
	MMU_VRAMmapRefreshBank(VRAM_BANK_B);
	MMU_VRAMmapRefreshBank(VRAM_BANK_A);

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



void MMU_Init(void) {
	LOG("MMU init\n");

	memset(&MMU, 0, sizeof(MMU_struct));

	MMU.CART_ROM = MMU.UNUSED_RAM;

	//MMU.DTCMRegion = 0x027C0000;
	//even though apps may change dtcm immediately upon startup, this is the correct hardware starting value:
	MMU.DTCMRegion = 0x08000000;
	MMU.ITCMRegion = 0x00000000;

	IPC_FIFOinit(ARMCPU_ARM9);
	IPC_FIFOinit(ARMCPU_ARM7);
	GFX_PIPEclear();
	GFX_FIFOclear();
	DISP_FIFOinit();
	new(&MMU_new) MMU_struct_new;	

	mc_init(&MMU.fw, MC_TYPE_FLASH);  /* init fw device */
	mc_alloc(&MMU.fw, NDS_FW_SIZE_V1);
	MMU.fw.fp = NULL;
	MMU.fw.isFirmware = true;

	// Init Backup Memory device, this should really be done when the rom is loaded
	//mc_init(&MMU.bupmem, MC_TYPE_AUTODETECT);
	//mc_alloc(&MMU.bupmem, 1);
	//MMU.bupmem.fp = NULL;
	rtcInit();
	addonsInit();
	slot1Init();
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
	//if (MMU.bupmem.fp)
	//	fclose(MMU.bupmem.fp);
	//mc_free(&MMU.bupmem);
	addonsClose();
	slot1Close();
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
	
	memset(MMU.dscard,        0, sizeof(nds_dscard) * 2);

	MMU.divRunning = 0;
	MMU.divResult = 0;
	MMU.divMod = 0;
	MMU.divCycles = 0;

	MMU.sqrtRunning = 0;
	MMU.sqrtResult = 0;
	MMU.sqrtCycles = 0;

	MMU.SPI_CNT = 0;
	MMU.AUX_SPI_CNT = 0;

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
	addonsReset();
	slot1Close();
	Mic_Reset();
	MMU.gfx3dCycles = 0;

	memset(MMU.dscard[ARMCPU_ARM9].command, 0, 8);
	MMU.dscard[ARMCPU_ARM9].address = 0;
	MMU.dscard[ARMCPU_ARM9].transfer_count = 0;
	MMU.dscard[ARMCPU_ARM9].mode = CardMode_Normal;

	memset(MMU.dscard[ARMCPU_ARM7].command, 0, 8);
	MMU.dscard[ARMCPU_ARM7].address = 0;
	MMU.dscard[ARMCPU_ARM7].transfer_count = 0;
	MMU.dscard[ARMCPU_ARM7].mode = CardMode_Normal;

	//HACK!!!
	//until we improve all our session tracking stuff, we need to save the backup memory filename
	std::string bleh = MMU_new.backupDevice.filename;
	BackupDevice tempBackupDevice;
	bool bleh2 = MMU_new.backupDevice.isMovieMode;
	if(bleh2) tempBackupDevice = MMU_new.backupDevice;
	reconstruct(&MMU_new);
	if(bleh2) {
		MMU_new.backupDevice = tempBackupDevice;
		MMU_new.backupDevice.reset_hardware();
	}
	else MMU_new.backupDevice.load_rom(bleh.c_str());

	MMU_timing.arm7codeFetch.Reset();
	MMU_timing.arm7dataFetch.Reset();
	MMU_timing.arm9codeFetch.Reset();
	MMU_timing.arm9dataFetch.Reset();
	MMU_timing.arm9codeCache.Reset();
	MMU_timing.arm9dataCache.Reset();
}

void MMU_setRom(u8 * rom, u32 mask)
{
	MMU.CART_ROM = rom;
}

void MMU_unsetRom()
{
	MMU.CART_ROM=MMU.UNUSED_RAM;
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

// TODO: 
// NAND flash support (used in Made in Ore/WarioWare D.I.Y.)
template<int PROCNUM>
void FASTCALL MMU_writeToGCControl(u32 val)
{
	const int TEST_PROCNUM = PROCNUM;
	nds_dscard& card = MMU.dscard[TEST_PROCNUM];

	memcpy(&card.command[0], &MMU.MMU_MEM[TEST_PROCNUM][0x40][0x1A8], 8);

	if(!(val & 0x80000000))
	{
		card.address = 0;
		card.transfer_count = 0;

		val &= 0x7F7FFFFF;
		T1WriteLong(MMU.MMU_MEM[TEST_PROCNUM][0x40], 0x1A4, val);
		return;
	}
	
	u32 shift = (val>>24&7); 
	if(shift == 7)
		card.transfer_count = 1;
	else if(shift == 0)
		card.transfer_count = 0;
	else
		card.transfer_count = (0x100<<shift)/4;

	switch (card.mode)
	{
	case CardMode_Normal: 
		break;

	case CardMode_KEY1:
		{
			// TODO
			INFO("Cartridge: KEY1 mode unsupported.\n");

			card.address = 0;
			card.transfer_count = 0;

			val &= 0x7F7FFFFF;
			T1WriteLong(MMU.MMU_MEM[TEST_PROCNUM][0x40], 0x1A4, val);
			return;
		}
		break;
	case CardMode_KEY2:
			INFO("Cartridge: KEY2 mode unsupported.\n");
		break;
	}

	switch(card.command[0])
	{
	case 0x9F: //Dummy
		card.address = 0;
		card.transfer_count = 0x800;
		break;

	//case 0x90: //Get ROM chip ID
	//	break;

	case 0x3C: //Switch to KEY1 mode
		card.mode = CardMode_KEY1;
		break;
	
	default:
		//fall through to the special slot1 handler
		slot1_device.write32(TEST_PROCNUM, REG_GCROMCTRL,val);
		break;
	}

	if(card.transfer_count == 0)
	{
		val &= 0x7F7FFFFF;
		T1WriteLong(MMU.MMU_MEM[TEST_PROCNUM][0x40], 0x1A4, val);
		return;
	}
	
    val |= 0x00800000;
    T1WriteLong(MMU.MMU_MEM[TEST_PROCNUM][0x40], 0x1A4, val);
						
	// Launch DMA if start flag was set to "DS Cart"
	//printf("triggering card dma\n");
	triggerDma(EDMAMode_Card);
}



template<int PROCNUM>
u32 MMU_readFromGC()
{
	const int TEST_PROCNUM = PROCNUM;

	nds_dscard& card = MMU.dscard[TEST_PROCNUM];
	u32 val = 0;

	if(card.transfer_count == 0)
		return 0;

	switch(card.command[0])
	{
		case 0x9F: //Dummy
			val = 0xFFFFFFFF;
			break;
	
		case 0x3C: //Switch to KEY1 mode
			val = 0xFFFFFFFF;
			break;

		default:
			val = slot1_device.read32(TEST_PROCNUM, REG_GCDATAIN);
			break;
	}

	card.address += 4;	// increment address

	card.transfer_count--;	// update transfer counter
	if(card.transfer_count) // if transfer is not ended
		return val;	// return data

	// transfer is done
	T1WriteLong(MMU.MMU_MEM[TEST_PROCNUM][0x40], 0x1A4, 
		T1ReadLong(MMU.MMU_MEM[TEST_PROCNUM][0x40], 0x1A4) & 0x7F7FFFFF);

	// if needed, throw irq for the end of transfer
	if(MMU.AUX_SPI_CNT & 0x4000)
		NDS_makeIrq(TEST_PROCNUM, IRQ_BIT_GC_TRANSFER_COMPLETE);

	return val;
}



//does some validation on the game's choice of IF value, correcting it if necessary
static void validateIF_arm9()
{
}

template<int PROCNUM> static void REG_IF_WriteByte(u32 addr, u8 val)
{
	//the following bits are generated from logic and should not be affected here
	//Bit 17    IPC Send FIFO Empty
	//Bit 18    IPC Recv FIFO Not Empty
	//Bit 21    NDS9 only: Geometry Command FIFO
	//arm9: IF &= ~0x00260000;
	//arm7: IF &= ~0x00060000;
	if(addr==2)
		if(PROCNUM==ARMCPU_ARM9)
			val &= ~0x26;
		else
			val &= ~0x06;
	
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

	//generate IPC IF states from the ipc registers
	u16 ipc = T1ReadWord(MMU.MMU_MEM[PROCNUM][0x40], 0x184);
	if(ipc&IPCFIFOCNT_FIFOENABLE)
	{
		if(ipc&IPCFIFOCNT_SENDIRQEN) if(ipc&IPCFIFOCNT_SENDEMPTY) 
			IF |= IRQ_MASK_IPCFIFO_SENDEMPTY;
		if(ipc&IPCFIFOCNT_RECVIRQEN) if(!(ipc&IPCFIFOCNT_RECVEMPTY))
			IF |= IRQ_MASK_IPCFIFO_RECVNONEMPTY;
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
		}
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

	if ((sync_l & 0x2000) && (sync_r & 0x4000))
		setIF(proc^1, ( 1 << 16 ));
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
	ret |= ((_hack_getMatrixStackLevel(0) << 13) | (_hack_getMatrixStackLevel(1) << 8)); //matrix stack levels //no proof that these are needed yet

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
	
	ret |= ((gxfifo_irq & 0x3) << 30); //user's irq flags

	//printf("vc=%03d Returning gxstat read: %08X\n",nds.VCount,ret);

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

	if(proc==0&&chan==0)
	{
		int zzz=9;
	}

	if(proc==1) {
		int zzz=9;
	}

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



	if(temp == 0xAF00 && size == 16)
	{
		int zzz=9;
	}
 
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
	read32le(&check,f); read32le(&running,f); read32le(&paused,f); read32le(&triggered,f); 
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
	write32le(check,f); write32le(running,f); write32le(paused,f); write32le(triggered,f); 
	write64le(nextEvent,f);
	write32le(saddr_user,f);
	write32le(daddr_user,f);
}

void DmaController::write32(const u32 val)
{
	if(this->chan==0 && this->procnum==0)
	{
		int zzz=9;
	}
	if(running)
	{
		//desp triggers this a lot. figure out whats going on
		//printf("thats weird..user edited dma control while it was running\n");
	}
	//printf("dma %d,%d WRITE %08X\n",procnum,chan,val);
	wordcount = val&0x1FFFFF;
	if(wordcount==0x9FbFC || wordcount == 0x1FFFFC || wordcount == 0x1EFFFC || wordcount == 0x1FFFFF) {
		int zzz=9;
	}
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

	if(val==0x84400076 && saddr ==0x023BCEC4)
	{
		int zzz=9;
	}

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
if(_startmode==0 && wordcount==1) {
	int zzz=9;
}
	if(enable)
	{
		int zzz=9;
	}

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
	check = FALSE;
	
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
			if(saddr ==0x023BCCEC && wordcount==118) {
				int zzz=9;
			}
			if(startmode==0 && daddr == 0x4000400) {
				int zzz=9;
			}
			running = TRUE;
			paused = FALSE;
			doCopy();
			//printf(";%d\n",gxFIFO.size);
		}
	}

	driver->DEBUG_UpdateIORegView(BaseDriver::EDEBUG_IOREG_DMA);
}

void DmaController::doCopy()
{
	//generate a copy count depending on various copy mode's behavior
	u32 todo = wordcount;
	if(todo == 0) todo = 0x200000; //according to gbatek.. //TODO - this should not work this way for arm7 according to gbatek
	if(startmode == EDMAMode_MemDisplay) todo = 128; //this is a hack. maybe an alright one though. it should be 4 words at a time. this is a whole scanline
	if(startmode == EDMAMode_Card) todo *= 0x80;
	if(startmode == EDMAMode_GXFifo) todo = std::min(todo,(u32)112);

	//determine how we're going to copy
	bool bogarted = false;
	u32 sz = (bitWidth==EDMABitWidth_16)?2:4;
	u32 dstinc,srcinc;
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
	if(sz==4) {
		for(s32 i=(s32)todo; i>0; i--)
		{
			u32 temp = _MMU_read32(procnum,MMU_AT_DMA,src);
			if(startmode == EDMAMode_GXFifo) {
				//printf("GXFIFO DMA OF %08X FROM %08X WHILE GXFIFO.SIZE=%d\n",temp,src,gxFIFO.size);
			}
			_MMU_write32(procnum,MMU_AT_DMA,dst, temp);
			dst += dstinc;
			src += srcinc;
		}
	} else {
		for(s32 i=(s32)todo; i>0; i--)
		{
			_MMU_write16(procnum,MMU_AT_DMA,dst, _MMU_read16(procnum,MMU_AT_DMA,src));
			dst += dstinc;
			src += srcinc;
		}
	}

	//reschedule an event for the end of this dma, and figure out how much it cost us
	doSchedule();

	// zeromus, check it
	if (wordcount > todo)
		nextEvent += todo/4; //TODO - surely this is a gross simplification
	//apparently moon has very, very tight timing (i didnt spy it using waitbyloop swi...)
	//so lets bump this down a bit for now,
	//(i think this code is in nintendo libraries)
		
	//write back the addresses
	saddr = src;
	if(dar != EDMADestinationUpdate_IncrementReload) //but dont write back dst if we were supposed to reload
		daddr = dst;

	//do wordcount accounting
	if(startmode == EDMAMode_Card) 
		todo /= 0x80; //divide this funky one back down before subtracting it 

	if(!repeatMode)
		wordcount -= todo;
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
	check = TRUE;
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
	if(ret == 0xAF000001) {
		int zzz=9;
	}
	return ret;
}

static INLINE void write_auxspicnt(const int proc, const int size, const int adr, const int val)
{
	//why val==0 to reset? is it a particular bit? its not bit 6...
	switch(size) {
		case 16:
			MMU.AUX_SPI_CNT = val;
			if (val == 0) MMU_new.backupDevice.reset_command();
			break;
		case 8:
			switch(adr) {
				case 0: 
					T1WriteByte((u8*)&MMU.AUX_SPI_CNT,0,val); 
					if (val == 0) MMU_new.backupDevice.reset_command();
					break;
				case 1: 
					T1WriteByte((u8*)&MMU.AUX_SPI_CNT,1,val); 
					break;
			}
	}
}


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
		T1WriteByte(MMU.ARM9_ITCM, adr&0x7FFF, val);
		return;
	}

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write08(adr, val);
		return;
	}

	//block 8bit writes to OAM and palette memory
	if((adr&0x0F000000)==0x07000000) return;
	if((adr&0x0F000000)==0x05000000) return;

	if (adr >> 24 == 4)
	{
		
		// TODO: add pal reg
		if (nds.power1.gpuMain == 0)
			if ((adr >= 0x04000008) && (adr<=0x0400005F)) return;
		if (nds.power1.gpuSub == 0)
			if ((adr >= 0x04001008) && (adr<=0x0400105F)) return;
		if (nds.power1.gfx3d_geometry == 0)
			if ((adr >= 0x04000400) && (adr<=0x040006FF)) return;
		if (nds.power1.gfx3d_render == 0)
			if ((adr >= 0x04000320) && (adr<=0x040003FF)) return;

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
			
#if 0
			case REG_DIVCNT: printf("ERROR 8bit DIVCNT WRITE\n"); return;
			case REG_DIVCNT+1: printf("ERROR 8bit DIVCNT1 WRITE\n"); return;
			case REG_DIVCNT+2: printf("ERROR 8bit DIVCNT2 WRITE\n"); return;
			case REG_DIVCNT+3: printf("ERROR 8bit DIVCNT3 WRITE\n"); return;
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
					printf("%c",val);
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
				MainScreen.gpu->setBLDALPHA_EVB(val);
				break;
			case REG_DISPA_BLDALPHA+1:
				MainScreen.gpu->setBLDALPHA_EVA(val);
				break;

			case REG_DISPB_BLDALPHA:
				SubScreen.gpu->setBLDALPHA_EVB(val);
				break;
			case REG_DISPB_BLDALPHA+1:
				SubScreen.gpu->setBLDALPHA_EVA(val);
				break;

			case REG_DISPA_BLDY: 	 
				GPU_setBLDY_EVY(MainScreen.gpu,val) ; 	 
				break ; 	 
			case REG_DISPB_BLDY: 	 
				GPU_setBLDY_EVY(SubScreen.gpu,val) ; 	 
				break;

			case REG_AUXSPICNT:
				write_auxspicnt(9,8,0,val);
				return;
			case REG_AUXSPICNT+1:
				write_auxspicnt(9,8,1,val);
				return;
			
			case REG_AUXSPIDATA:
				if(val!=0) MMU.AUX_SPI_CMD = val & 0xFF;
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, MMU_new.backupDevice.data_command((u8)val,ARMCPU_ARM9));
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;

			case REG_WRAMCNT:	
				/* Update WRAMSTAT at the ARM7 side */
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x241, val);
				break;

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
		T1WriteWord(MMU.ARM9_ITCM, adr&0x7FFF, val);
		return;
	}

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write16(adr, val);
		return;
	}

	if((adr >> 24) == 4)
	{
		// TODO: add pal reg
		if (nds.power1.gpuMain == 0)
			if ((adr >= 0x04000008) && (adr<=0x0400005F)) return;
		if (nds.power1.gpuSub == 0)
			if ((adr >= 0x04001008) && (adr<=0x0400105F)) return;
		if (nds.power1.gfx3d_geometry == 0)
			if ((adr >= 0x04000400) && (adr<=0x040006FF)) return;
		if (nds.power1.gfx3d_render == 0)
			if ((adr >= 0x04000320) && (adr<=0x040003FF)) return;

		if(MMU_new.is_dma(adr)) { 
			if(val==0x02e9) {
				int zzz=9;
			}
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
#if 0
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
				write_auxspicnt(9,16,0,val);
				return;

			case REG_AUXSPIDATA:
				if(val!=0)
				   MMU.AUX_SPI_CMD = val & 0xFF;

				//T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, bm_transfer(&MMU.bupmem, val));
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, MMU_new.backupDevice.data_command((u8)val,ARMCPU_ARM9));
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;

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
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+1, val >> 8);
				break;
			case REG_VRAMCNTG:
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				/* Update WRAMSTAT at the ARM7 side */
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x241, val >> 8);
				break;
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

            case REG_IPCSYNC :
					MMU_IPCSync(ARMCPU_ARM9, val);
				return;

			case REG_IPCFIFOCNT :
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
		}

		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val); 
		return;
	}


	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr, unmapped, restricted);
	if(unmapped) return;

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
		T1WriteLong(MMU.ARM9_ITCM, adr&0x7FFF, val);
		return ;
	}

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write32(adr, val);
		return;
	}

	if((adr&0x0F000000)==0x05000000)
	{
		int zzz=9;
	}

#if 0
	if ((adr & 0xFF800000) == 0x04800000) {
		// access to non regular hw registers
		// return to not overwrite valid data
		return ;
	}
#endif

	if((adr>>24)==4)
	{
		// TODO: add pal reg
		if (nds.power1.gpuMain == 0)
			if ((adr >= 0x04000008) && (adr<=0x0400005F)) return;
		if (nds.power1.gpuSub == 0)
			if ((adr >= 0x04001008) && (adr<=0x0400105F)) return;
		if (nds.power1.gfx3d_geometry == 0)
			if ((adr >= 0x04000400) && (adr<=0x040006FF)) return;
		if (nds.power1.gfx3d_render == 0)
			if ((adr >= 0x04000320) && (adr<=0x040003FF)) return;

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
					nds.freezeBus = TRUE;

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
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+1, (val >> 8) & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+2, (val >> 16) & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+3, (val >> 24) & 0xFF);
				break;
			case REG_VRAMCNTE:
				MMU_VRAMmapControl(adr-REG_VRAMCNTA, val & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+1, (val >> 8) & 0xFF);
				MMU_VRAMmapControl(adr-REG_VRAMCNTA+2, (val >> 16) & 0xFF);
				/* Update WRAMSTAT at the ARM7 side */
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x241, (val >> 24) & 0xFF);
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
			{
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2BC, val);
				execsqrt();
				return;
			}
			case REG_IPCSYNC :
					MMU_IPCSync(ARMCPU_ARM9, val);
				return;

			case REG_IPCFIFOSEND :
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
				slot1_device.write32(ARMCPU_ARM9, REG_GCDATAIN,val);
				return;
		}

		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM9>(adr, unmapped, restricted);
	if(unmapped) return;

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

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read08(adr);

	if (adr >> 24 == 4)
	{	//Address is an IO register

		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM9,8,adr);

		switch(adr)
		{
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM9>();
			case REG_IF+1: return (MMU.gen_IF<ARMCPU_ARM9>()>>8);
			case REG_IF+2: return (MMU.gen_IF<ARMCPU_ARM9>()>>16);
			case REG_IF+3: return (MMU.gen_IF<ARMCPU_ARM9>()>>24);

			case REG_DISPA_DISPSTAT:
				break;
			case REG_DISPA_DISPSTAT+1:
				break;
			case REG_DISPx_VCOUNT:
				break;
			case REG_DISPx_VCOUNT+1:
				break;
#if 0
			case REG_SQRTCNT: printf("ERROR 8bit SQRTCNT READ\n"); return 0;
			case REG_SQRTCNT+1: printf("ERROR 8bit SQRTCNT1 READ\n"); return 0;//(MMU_new.sqrt.read16() & 0xFF00)>>8;
#else
			case REG_SQRTCNT: return (MMU_new.sqrt.read16() & 0xFF);
			case REG_SQRTCNT+1: return ((MMU_new.sqrt.read16()>>8) & 0xFF);
#endif
			case REG_SQRTCNT+2: printf("ERROR 8bit SQRTCNT2 READ\n"); return 0;
			case REG_SQRTCNT+3: printf("ERROR 8bit SQRTCNT3 READ\n"); return 0;
#if 0
			case REG_DIVCNT: printf("ERROR 8bit DIVCNT READ\n"); return 0;
			case REG_DIVCNT+1: printf("ERROR 8bit DIVCNT1 READ\n"); return 0;
#else
			case REG_DIVCNT: return (MMU_new.div.read16() & 0xFF);
			case REG_DIVCNT+1: return ((MMU_new.div.read16()>>8) & 0xFF);
#endif
			case REG_DIVCNT+2: printf("ERROR 8bit DIVCNT2 READ\n"); return 0;
			case REG_DIVCNT+3: printf("ERROR 8bit DIVCNT3 READ\n"); return 0;

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

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read16(adr);

	if (adr >> 24 == 4)
	{
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM9,16,adr); 

		// Address is an IO register
		switch(adr)
		{
			case REG_DISPA_DISPSTAT:
				break;

			case REG_SQRTCNT: return MMU_new.sqrt.read16();
			case REG_DIVCNT: return MMU_new.div.read16();
			case eng_3D_GXSTAT: return MMU_new.gxstat.read(16,adr);

			case REG_DISPA_VCOUNT:
				if(nds.ensataEmulation && nds.ensataHandshake == ENSATA_HANDSHAKE_query)
				{
					nds.ensataHandshake = ENSATA_HANDSHAKE_ack;
					return 270;
				}
				break;

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

			case 0x04000130:
			case 0x04000136:
				//not sure whether these should trigger from byte reads
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

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read32(adr);

	// Address is an IO register
	if((adr >> 24) == 4)
	{
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM9,32,adr); 

		switch(adr)
		{
		case REG_DISPA_DISPSTAT:
			break;

			//Dolphin Island Underwater Adventures uses this amidst seemingly reasonable divs so we're going to emulate it.
			case REG_DIVCNT: return MMU_new.div.read16();
			//I guess we'll do this also
			case REG_SQRTCNT: return MMU_new.sqrt.read16();

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
     
			case REG_GCDATAIN: return MMU_readFromGC<ARMCPU_ARM9>();
            case REG_POWCNT1: return readreg_POWCNT1(32,adr);
			case REG_DISPA_DISP3DCNT: return readreg_DISP3DCNT(32,adr);
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

	if (adr < 0x4000) return;	// PU BIOS

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write08(adr, val);
		return;
	}

	if ((adr>=0x04000400)&&(adr<0x04000520)) 
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

	if (adr >> 24 == 4)
	{
		if(MMU_new.is_dma(adr)) { MMU_new.write_dma(ARMCPU_ARM7,8,adr,val); return; }

		switch(adr)
		{
			case REG_IF: REG_IF_WriteByte<ARMCPU_ARM7>(0,val); break;
			case REG_IF+1: REG_IF_WriteByte<ARMCPU_ARM7>(1,val); break;
			case REG_IF+2: REG_IF_WriteByte<ARMCPU_ARM7>(2,val); break;
			case REG_IF+3: REG_IF_WriteByte<ARMCPU_ARM7>(3,val); break;

			case REG_POSTFLG:
				// hack for patched firmwares
				if (val == 1)
				{
					if (_MMU_ARM7_read08(REG_POSTFLG) != 0)
						break;
					_MMU_write32<ARMCPU_ARM9>(0x27FFE24, gameInfo.header.ARM9exe);
					_MMU_write32<ARMCPU_ARM7>(0x27FFE34, gameInfo.header.ARM7exe);
				}
				break;

			case REG_HALTCNT:
				//printf("halt 0x%02X\n", val);
				switch(val)
				{
					case 0xC0: NDS_Sleep(); break;
					case 0x80: armcpu_Wait4IRQ(&NDS_ARM7); break;
					default: break;
				}
				break;
				
			case REG_RTC:
				rtcWrite(val);
				return;

			case REG_AUXSPICNT:
				write_auxspicnt(9,8,0,val);
				return;
			case REG_AUXSPICNT+1:
				write_auxspicnt(9,8,1,val);
				return;
			case REG_AUXSPIDATA:
				if(val!=0) MMU.AUX_SPI_CMD = val & 0xFF;
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, MMU_new.backupDevice.data_command((u8)val,ARMCPU_ARM7));
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
				return;
		}
		MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]]=val;
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return;
	
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]]=val;
}

//================================================= MMU ARM7 write 16
void FASTCALL _MMU_ARM7_write16(u32 adr, u16 val)
{
	adr &= 0x0FFFFFFE;

	mmu_log_debug_ARM7(adr, "(write16) 0x%04X", val);

	if (adr < 0x4000) return;	// PU BIOS
	
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write16(adr, val);
		return;
	}

	//wifi mac access
	if ((adr & 0xFFFF0000) == 0x04800000)
	{
		WIFI_write16(adr,val);
		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x48], adr&MMU.MMU_MASK[ARMCPU_ARM7][0x48], val);
		return;
	}

	if ((adr>=0x04000400)&&(adr<0x04000520))
	{
		SPU_WriteWord(adr, val);
		return;
	}

	if((adr >> 24) == 4)
	{
		if(MMU_new.is_dma(adr)) { MMU_new.write_dma(ARMCPU_ARM7,16,adr,val); return; }

		//Address is an IO register
		switch(adr)
		{
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
					nds.power2.wifi = BIT0(val);
				}
				return;


			case REG_AUXSPICNT:
				write_auxspicnt(7,16,0,val);
			return;

			case REG_AUXSPIDATA:
				if(val!=0)
				   MMU.AUX_SPI_CMD = val & 0xFF;

				//T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, bm_transfer(&MMU.bupmem, val));
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, MMU_new.backupDevice.data_command((u8)val,ARMCPU_ARM7));
				MMU.AUX_SPI_CNT &= ~0x80; //remove busy flag
			return;

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
				{
					u16 spicnt;

					if(val!=0)
						MMU.SPI_CMD = val;
			
					spicnt = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPICNT >> 20) & 0xff], REG_SPICNT & 0xfff);

					switch((spicnt >> 8) & 0x3)
					{
						case 0 :
							{
								if(!MMU.powerMan_CntRegWritten)
								{
									MMU.powerMan_CntReg = (val & 0xFF);
									MMU.powerMan_CntRegWritten = TRUE;
								}
								else
								{
									u16 reg = MMU.powerMan_CntReg&0x7F;
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
											
										enum PM_Bits //from libnds
										{
											PM_SOUND_AMP		= BIT(0) ,   /*!< \brief Power the sound hardware (needed to hear stuff in GBA mode too) */
											PM_SOUND_MUTE		= BIT(1),    /*!< \brief   Mute the main speakers, headphone output will still work. */
											PM_BACKLIGHT_BOTTOM	= BIT(2),    /*!< \brief   Enable the top backlight if set */
											PM_BACKLIGHT_TOP	= BIT(3)  ,  /*!< \brief   Enable the bottom backlight if set */
											PM_SYSTEM_PWR		= BIT(6) ,   /*!< \brief  Turn the power *off* if set */
										};

										//our totally pathetic register handling, only the one thing we've wanted so far
										if(MMU.powerMan_Reg[0]&PM_SYSTEM_PWR) {
											printf("SYSTEM POWERED OFF VIA ARM7 SPI POWER DEVICE\n");
											emu_halt();
										}
									}

									MMU.powerMan_CntRegWritten = FALSE;
								}
							}
						break;
						
						case 1 : /* firmware memory device */
							if((spicnt & 0x3) != 0)      /* check SPI baudrate (must be 4mhz) */
							{
								T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, 0);
								break;
							}
							T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, fw_transfer(&MMU.fw, (u8)val));
						return;
						
						case 2 :
							//printf("%08X\n",MMU.SPI_CMD);
							switch(MMU.SPI_CMD & 0x70)
							{
								case 0x00 :
									val = 0;
									break;
								case 0x10 :
									//emu_halt();
									if(MMU.SPI_CNT&(1<<11))
									{
										if(partie)
										{
											val = ((nds.touchY<<3)&0x7FF);
											partie = 0;
											//emu_halt();
											break;
										}
										val = (nds.touchY>>5);
										partie = 1;
										break;
									}
								val = ((nds.touchY<<3)&0x7FF);
								partie = 1;
								break;
								case 0x20 :
									val = 0;
									break;
								case 0x30: //Z1
									//used for pressure calculation - must be nonzero or else some softwares will think the stylus is up.
									if(nds.isTouch) val = 2048;
									else val = 0;
									break;
								case 0x40: //Z2
									//used for pressure calculation. we dont support pressure calculation so just return something.
									val = 2048;
									break;
								case 0x50:
									if(spicnt & 0x800)
									{
										if(partie)
										{
											val = ((nds.touchX<<3)&0x7FF);
											partie = 0;
											break;
										}
										val = (nds.touchX>>5);
										partie = 1;
										break;
									}
									val = ((nds.touchX<<3)&0x7FF);
									partie = 1;
									break;
								case 0x60 :
									if(!(val & 0x80))
										val = (Mic_ReadSample() & 0xFF);
									else
										val = 0;
									break;
								case 0x70 :
									val = 0;
									break;
						}
						break;
						
						case 3 :
						/* NOTICE: Device 3 of SPI is reserved (unused and unusable) */
						break;
					}
				}

				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, val);
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
				
            case REG_IPCSYNC :
				MMU_IPCSync(ARMCPU_ARM7, val);
				return;

			case REG_IPCFIFOCNT :
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

		}

		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val); 
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return;

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFF [shash]
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
} 
//================================================= MMU ARM7 write 32
void FASTCALL _MMU_ARM7_write32(u32 adr, u32 val)
{
	adr &= 0x0FFFFFFC;

	mmu_log_debug_ARM7(adr, "(write32) 0x%08X", val);

	if (adr < 0x4000) return;	// PU BIOS

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write32(adr, val);
		return;
	}

	if ((adr & 0xFFFF0000) == 0x04800000)
	{
		WIFI_write16(adr, val & 0xFFFF);
		WIFI_write16(adr+2, val >> 16);
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x48], adr&MMU.MMU_MASK[ARMCPU_ARM7][0x48], val);
		return;
	}

    if ((adr>=0x04000400)&&(adr<0x04000520))
    {
        SPU_WriteLong(adr, val);
        return;
    }

	if((adr>>24)==4)
	{
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


			case REG_IPCSYNC :
					MMU_IPCSync(ARMCPU_ARM7, val);
				return;

			case REG_IPCFIFOSEND :
					IPC_FIFOsend(ARMCPU_ARM7, val);
				return;
			
			case REG_GCROMCTRL :
				MMU_writeToGCControl<ARMCPU_ARM7>(val);
				return;

			case REG_GCDATAIN:
				slot1_device.write32(ARMCPU_ARM9, REG_GCDATAIN,val);
				return;
		}
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
		return;
	}

	bool unmapped, restricted;
	adr = MMU_LCDmap<ARMCPU_ARM7>(adr,unmapped, restricted);
	if(unmapped) return;

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
		//u32 prot = T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x04000308 & MMU.MMU_MASK[ARMCPU_ARM7][0x40]);
		//if (prot) INFO("MMU7 read 08 at 0x%08X (PC 0x%08X) BIOSPROT address 0x%08X\n", adr, NDS_ARM7.R[15], prot);
		
		//How accurate is this? our R[15] may not be exactly what the hardware uses (may use something less by up to 0x08)
		//This may be inaccurate at the very edge cases.
		if (NDS_ARM7.R[15] > 0x3FFF)
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

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read08(adr);

    if ((adr>=0x04000400)&&(adr<0x04000520))
    {
        return SPU_ReadByte(adr);
    }

	if (adr == REG_RTC) return (u8)rtcRead();

	if (adr >> 24 == 4)
	{
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM7,8,adr); 

		// Address is an IO register

		switch(adr)
		{
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM7>();
			case REG_IF+1: return (MMU.gen_IF<ARMCPU_ARM7>()>>8);
			case REG_IF+2: return (MMU.gen_IF<ARMCPU_ARM7>()>>16);
			case REG_IF+3: return (MMU.gen_IF<ARMCPU_ARM7>()>>24);
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
		//u32 prot = T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x04000308 & MMU.MMU_MASK[ARMCPU_ARM7][0x40]);
		//if (prot) INFO("MMU7 read 16 at 0x%08X (PC 0x%08X) BIOSPROT address 0x%08X\n", adr, NDS_ARM7.R[15], prot);
		if (NDS_ARM7.R[15] > 0x3FFF)
			return 0xFFFF;
	}

	//wifi mac access
	if ((adr & 0xFFFF0000) == 0x04800000)
		return WIFI_read16(adr) ;

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read16(adr);

    if ((adr>=0x04000400)&&(adr<0x04000520))
    {
        return SPU_ReadWord(adr);
    }

	if(adr>>24==4)
	{	//Address is an IO register

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

			case REG_RTC:
				return rtcRead();

			case REG_IME :
				return (u16)MMU.reg_IME[ARMCPU_ARM7];
				
			case REG_IE :
				return (u16)MMU.reg_IE[ARMCPU_ARM7];
			case REG_IE + 2 :
				return (u16)(MMU.reg_IE[ARMCPU_ARM7]>>16);
				
			case REG_IF: return MMU.gen_IF<ARMCPU_ARM7>();
			case REG_IF+2: return MMU.gen_IF<ARMCPU_ARM7>()>>16;

			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
				return read_timer(ARMCPU_ARM7,(adr&0xF)>>2);

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
					if(nds.isTouch) ret &= ~64;
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
		//u32 prot = T1ReadLong_guaranteedAligned(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x04000308 & MMU.MMU_MASK[ARMCPU_ARM7][0x40]);
		//if (prot) INFO("MMU7 read 32 at 0x%08X (PC 0x%08X) BIOSPROT address 0x%08X\n", adr, NDS_ARM7.R[15], prot);
		if (NDS_ARM7.R[15] > 0x3FFF)
			return 0xFFFFFFFF;
	}

	//wifi mac access
	if ((adr & 0xFFFF0000) == 0x04800000)
		return (WIFI_read16(adr) | (WIFI_read16(adr+2) << 16));

	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read32(adr);

    if ((adr>=0x04000400)&&(adr<0x04000520))
    {
        return SPU_ReadLong(adr);
    }

	if((adr >> 24) == 4)
	{	//Address is an IO register
		
		if(MMU_new.is_dma(adr)) return MMU_new.read_dma(ARMCPU_ARM7,32,adr); 
		
		switch(adr)
		{
			case REG_RTC:
				return (u32)rtcRead();

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
			case REG_GCROMCTRL:
			{
				//INFO("arm7 romctrl read\n");
				break;
			}
            case REG_GCDATAIN:
				return MMU_readFromGC<ARMCPU_ARM7>();

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
