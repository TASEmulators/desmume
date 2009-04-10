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

//#define NEW_IRQ 1

#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <assert.h>

#include "common.h"
#include "debug.h"
#include "NDSSystem.h"
#ifndef EXPERIMENTAL_GBASLOT
#include "cflash.h"
#endif
#include "cp15.h"
#include "wifi.h"
#include "registers.h"
#include "render3D.h"
#include "gfx3d.h"
#include "rtc.h"
#include "GPU_osd.h"
#include "mc.h"
#include "addons.h"
#include "mic.h"

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


static const int save_types[7][2] = {
        {MC_TYPE_AUTODETECT,1},
        {MC_TYPE_EEPROM1,MC_SIZE_4KBITS},
        {MC_TYPE_EEPROM2,MC_SIZE_64KBITS},
        {MC_TYPE_EEPROM2,MC_SIZE_512KBITS},
        {MC_TYPE_FRAM,MC_SIZE_256KBITS},
        {MC_TYPE_FLASH,MC_SIZE_2MBITS},
		{MC_TYPE_FLASH,MC_SIZE_4MBITS}
};

u16 partie = 1;
u32 _MMU_MAIN_MEM_MASK = 0x3FFFFF;

#define ROM_MASK 3

//#define	_MMU_DEBUG

#ifdef _MMU_DEBUG

#include <stdarg.h>
void mmu_log_debug_ARM9(u32 adr, const char *fmt, ...)
{
	if (adr < 0x4000000) return;
	if (adr > 0x4100014) return;

	if (adr >= 0x4000000 && adr <= 0x400006E) return;		// Display Engine A
	if (adr >= 0x40000B0 && adr <= 0x4000134) return;		// DMA, Timers and Keypad
	if (adr >= 0x4000180 && adr <= 0x40001BC) return;		// IPC/ROM
	if (adr >= 0x4000204 && adr <= 0x400024A) return;		// Memory & IRQ control
	if (adr >= 0x4000280 && adr <= 0x4000306) return;		// Maths
	if (adr >= 0x4000320 && adr <= 0x40006A3) return;		// 3D dispaly engine
	if (adr >= 0x4001000 && adr <= 0x400106E) return;		// Display Engine B
	if (adr >= 0x4100000 && adr <= 0x4100014) return;		// IPC/ROM

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

	if (adr >= 0x4000004 && adr <= 0x40001C4) return;		// ARM7 I/O Map
	if (adr >= 0x4000204 && adr <= 0x400030C) return;		// Memory and IRQ Control
	if (adr >= 0x4000400 && adr <= 0x400051E) return;		// Sound Registers
	if (adr >= 0x4100000 && adr <= 0x4100014) return;		// IPC/ROM
	//if (adr >= 0x4800000 && adr <= 0x4808FFF) return;		// WLAN Registers

	va_list list;
	char msg[512];

	memset(msg,0,512);

	va_start(list,fmt);
		_vsnprintf(msg,511,fmt,list);
	va_end(list);

	INFO("MMU ARM7 0x%08X: %s\n", adr, msg);

}
#endif

/*
 *
 */
#define EARLY_MEMORY_ACCESS 1

#define INTERNAL_DTCM_READ 1
#define INTERNAL_DTCM_WRITE 1

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

u8 * MMU_struct::MMU_MEM[2][256] = {
	//arm9
	{
		/* 0X*/	DUP16(ARM9Mem.ARM9_ITCM), 
		/* 1X*/	//DUP16(ARM9Mem.ARM9_ITCM)
		/* 1X*/	DUP16(MMU.UNUSED_RAM), 
		/* 2X*/	DUP16(ARM9Mem.MAIN_MEM),
		/* 3X*/	DUP16(MMU.SWIRAM),
		/* 4X*/	DUP16(ARM9Mem.ARM9_REG),
		/* 5X*/	DUP16(ARM9Mem.ARM9_VMEM),
		/* 6X*/	DUP2(ARM9Mem.ARM9_ABG), 
				DUP2(ARM9Mem.ARM9_BBG),
				DUP2(ARM9Mem.ARM9_AOBJ),
				DUP2(ARM9Mem.ARM9_BOBJ),
				DUP8(ARM9Mem.ARM9_LCD),
		/* 7X*/	DUP16(ARM9Mem.ARM9_OAM),
		/* 8X*/	DUP16(NULL),
		/* 9X*/	DUP16(NULL),
		/* AX*/	DUP16(MMU.CART_RAM),
		/* BX*/	DUP16(MMU.UNUSED_RAM),
		/* CX*/	DUP16(MMU.UNUSED_RAM),
		/* DX*/	DUP16(MMU.UNUSED_RAM),
		/* EX*/	DUP16(MMU.UNUSED_RAM),
		/* FX*/	DUP16(ARM9Mem.ARM9_BIOS)
	},
	//arm7
	{
		/* 0X*/	DUP16(MMU.ARM7_BIOS), 
		/* 1X*/	DUP16(MMU.UNUSED_RAM), 
		/* 2X*/	DUP16(ARM9Mem.MAIN_MEM),
		/* 3X*/	DUP8(MMU.SWIRAM),
				DUP8(MMU.ARM7_ERAM),
		/* 4X*/	DUP8(MMU.ARM7_REG),
				DUP8(MMU.ARM7_WIRAM),
		/* 5X*/	DUP16(MMU.UNUSED_RAM),
		/* 6X*/	DUP16(ARM9Mem.ARM9_ABG), 
		/* 7X*/	DUP16(MMU.UNUSED_RAM),
		/* 8X*/	DUP16(NULL),
		/* 9X*/	DUP16(NULL),
		/* AX*/	DUP16(MMU.CART_RAM),
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
		/* 6X*/	DUP2(0x0007FFFF), 
				DUP2(0x0001FFFF),
				DUP2(0x0003FFFF),
				DUP2(0x0001FFFF),
				DUP8(0x000FFFFF),
		/* 7X*/	DUP16(0x000007FF),
		/* 8X*/	DUP16(ROM_MASK),
		/* 9X*/	DUP16(ROM_MASK),
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
		/* 6X*/	DUP16(0x0003FFFF), 
		/* 7X*/	DUP16(0x00000003),
		/* 8X*/	DUP16(ROM_MASK),
		/* 9X*/	DUP16(ROM_MASK),
		/* AX*/	DUP16(0x0000FFFF),
		/* BX*/	DUP16(0x00000003),
		/* CX*/	DUP16(0x00000003),
		/* DX*/	DUP16(0x00000003),
		/* EX*/	DUP16(0x00000003),
		/* FX*/	DUP16(0x00000003)
		}
};


TWaitState MMU_struct::MMU_WAIT16[2][16] = {
	{ 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1 }, //arm9
	{ 1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1 }, //arm7
};

TWaitState MMU_struct::MMU_WAIT32[2][16] = {
	{ 1, 1, 1, 1, 1, 2, 2, 1, 8, 8, 5, 1, 1, 1, 1, 1 }, //arm9
	{ 1, 1, 1, 1, 1, 1, 1, 1, 8, 8, 5, 1, 1, 1, 1, 1 }, //arm7
};


// VRAM mapping
u8		*LCDdst[10] = { NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL };
u8		*EngineAddr[4] = { NULL, NULL, NULL, NULL };
const static u32	LCDdata[10][2]= {
					{0x6800000, 8},			// Bank A
					{0x6820000, 8},			// Bank B
					{0x6840000, 8},			// Bank C
					{0x6860000, 8},			// Bank D
					{0x6880000, 4},			// Bank E
					{0x6890000, 1},			// Bank F
					{0x6894000, 1},			// Bank G
					{0, 0},
					{0x6898000, 2},			// Bank H
					{0x68A0000, 1}};		// Bank I

void MMU_Init(void) {
	int i;

	LOG("MMU init\n");

	memset(&MMU, 0, sizeof(MMU_struct));

	MMU.CART_ROM = MMU.UNUSED_RAM;

        for(i = 0x80; i<0xA0; ++i)
        {
			MMU_struct::MMU_MEM[0][i] = MMU.CART_ROM;
			MMU_struct::MMU_MEM[1][i] = MMU.CART_ROM;
        }


	MMU.DTCMRegion = 0x027C0000;
	MMU.ITCMRegion = 0x00000000;

	IPC_FIFOinit(ARMCPU_ARM9);
	IPC_FIFOinit(ARMCPU_ARM7);
	GFX_FIFOclear();
	DISP_FIFOinit();
	
	mc_init(&MMU.fw, MC_TYPE_FLASH);  /* init fw device */
	mc_alloc(&MMU.fw, NDS_FW_SIZE_V1);
	MMU.fw.fp = NULL;

	// Init Backup Memory device, this should really be done when the rom is loaded
	mc_init(&MMU.bupmem, MC_TYPE_AUTODETECT);
	mc_alloc(&MMU.bupmem, 1);
	MMU.bupmem.fp = NULL;
	rtcInit();
	addonsInit();
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
	if (MMU.bupmem.fp)
		fclose(MMU.bupmem.fp);
	mc_free(&MMU.bupmem);
	addonsClose();
	Mic_DeInit();
}

//Card rom & ram

u16 SPI_CNT = 0;
u16 SPI_CMD = 0;
u16 AUX_SPI_CNT = 0;
u16 AUX_SPI_CMD = 0;

u32 rom_mask = 0;

u32 DMASrc[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};
u32 DMADst[2][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}};

void MMU_clearMem()
{
	memset(ARM9Mem.ARM9_ABG,  0, sizeof(ARM9Mem.ARM9_ABG));
	memset(ARM9Mem.ARM9_AOBJ, 0, sizeof(ARM9Mem.ARM9_AOBJ));
	memset(ARM9Mem.ARM9_BBG,  0, sizeof(ARM9Mem.ARM9_BBG));
	memset(ARM9Mem.ARM9_BOBJ, 0, sizeof(ARM9Mem.ARM9_BOBJ));

	memset(ARM9Mem.ARM9_DTCM, 0, sizeof(ARM9Mem.ARM9_DTCM));
	memset(ARM9Mem.ARM9_ITCM, 0, sizeof(ARM9Mem.ARM9_ITCM));
	memset(ARM9Mem.ARM9_LCD,  0, sizeof(ARM9Mem.ARM9_LCD));
	memset(ARM9Mem.ARM9_OAM,  0, sizeof(ARM9Mem.ARM9_OAM));
	memset(ARM9Mem.ARM9_REG,  0, sizeof(ARM9Mem.ARM9_REG));
	memset(ARM9Mem.ARM9_VMEM, 0, sizeof(ARM9Mem.ARM9_VMEM));
	memset(ARM9Mem.MAIN_MEM,  0, sizeof(ARM9Mem.MAIN_MEM));

	memset(ARM9Mem.blank_memory,  0, sizeof(ARM9Mem.blank_memory));
	
	memset(MMU.ARM7_ERAM,     0, sizeof(MMU.ARM7_ERAM));
	memset(MMU.ARM7_REG,      0, sizeof(MMU.ARM7_REG));

	IPC_FIFOinit(ARMCPU_ARM9);
	IPC_FIFOinit(ARMCPU_ARM7);
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
	memset(MMU.reg_IF,        0, sizeof(u32) * 2);
	
	memset(MMU.DMAStartTime,  0, sizeof(u32) * 2 * 4);
	memset(MMU.DMACycle,      0, sizeof(s32) * 2 * 4);
	memset(MMU.DMACrt,        0, sizeof(u32) * 2 * 4);
	memset(MMU.DMAing,        0, sizeof(BOOL) * 2 * 4);
	
	memset(MMU.dscard,        0, sizeof(nds_dscard) * 2);

	// Enable the sound speakers
	T1WriteWord(MMU.ARM7_REG, 0x304, 0x0001);
	
	MainScreen.offset = 0;
	SubScreen.offset  = 192;
	osdA->setOffset(MainScreen.offset);
	osdB->setOffset(SubScreen.offset);

	for(int i=0;i<4;i++)
		ARM9Mem.textureSlotAddr[i] = ARM9Mem.blank_memory;
	for(int i=0;i<6;i++)
		ARM9Mem.texPalSlot[i] = ARM9Mem.blank_memory;

	LCDdst[0] = ARM9Mem.ARM9_LCD;				// Bank A
	LCDdst[1] = ARM9Mem.ARM9_LCD + 0x20000;		// Bank B
	LCDdst[2] = ARM9Mem.ARM9_LCD + 0x40000;		// Bank C
	LCDdst[3] = ARM9Mem.ARM9_LCD + 0x60000;		// Bank D
	LCDdst[4] = ARM9Mem.ARM9_LCD + 0x80000;		// Bank E
	LCDdst[5] = ARM9Mem.ARM9_LCD + 0x90000;		// Bank F
	LCDdst[6] = ARM9Mem.ARM9_LCD + 0x94000;		// Bank G
	LCDdst[7] = NULL;
	LCDdst[8] = ARM9Mem.ARM9_LCD + 0x98000;		// Bank H
	LCDdst[9] = ARM9Mem.ARM9_LCD + 0xA0000;		// Bank I

	EngineAddr[0] = ARM9Mem.ARM9_ABG;			// Engine ABG
	EngineAddr[1] = ARM9Mem.ARM9_BBG;			// Engine BBG
	EngineAddr[2] = ARM9Mem.ARM9_AOBJ;			// Engine BOBJ
	EngineAddr[3] = ARM9Mem.ARM9_BOBJ;			// Engine BOBJ

	for (int i = 0; i < 4; i++)
	{
		ARM9Mem.ExtPal[0][i] = ARM9Mem.ARM9_LCD;
		ARM9Mem.ExtPal[1][i] = ARM9Mem.ARM9_LCD;
	}
	ARM9Mem.ObjExtPal[0][0] = ARM9Mem.ARM9_LCD;
	ARM9Mem.ObjExtPal[0][1] = ARM9Mem.ARM9_LCD;
	ARM9Mem.ObjExtPal[1][0] = ARM9Mem.ARM9_LCD;
	ARM9Mem.ObjExtPal[1][1] = ARM9Mem.ARM9_LCD;

	ARM9Mem.texPalSlot[0] = ARM9Mem.ARM9_LCD;
	ARM9Mem.texPalSlot[1] = ARM9Mem.ARM9_LCD;
	ARM9Mem.texPalSlot[2] = ARM9Mem.ARM9_LCD;
	ARM9Mem.texPalSlot[3] = ARM9Mem.ARM9_LCD;

	for (int i =0; i < 9; i++)
	{
		MMU.LCD_VRAM_ADDR[i] = 0;
		for (int t = 0; t < 32; t++)
			MMU.VRAM_MAP[i][t] = 7;
	}

	MMU.powerMan_CntReg = 0x00;
	MMU.powerMan_CntRegWritten = FALSE;
	MMU.powerMan_Reg[0] = 0x0B;
	MMU.powerMan_Reg[1] = 0x00;
	MMU.powerMan_Reg[2] = 0x01;
	MMU.powerMan_Reg[3] = 0x00;

	rtcInit();
	partie = 1;
	addonsReset();
	Mic_Reset();
}

// VRAM mapping control
u8 *MMU_RenderMapToLCD(u32 vram_addr)
{
	if ((vram_addr < 0x6000000)) return NULL;
	if ((vram_addr > 0x661FFFF)) return NULL;	// Engine BOBJ max 128KB

	// holes
	if ((vram_addr > 0x6080000) && (vram_addr < 0x6200000)) return NULL;	// Engine ABG max 512KB
	if ((vram_addr > 0x6220000) && (vram_addr < 0x6400000)) return NULL;	// Engine BBG max 128KB
	if ((vram_addr >= 0x6440000) && (vram_addr < 0x6600000)) {
		assert(false); //please verify
		vram_addr = (vram_addr & ADDRESS_MASK_256KB) + 0x6440000;	// Engine AOBJ max 256KB
	}
	
	vram_addr &= 0x0FFFFFF;
	u8	engine = (vram_addr >> 21);
	vram_addr &= 0x01FFFFF;
	u8	engine_offset = (vram_addr >> 14);
	u8	block = MMU.VRAM_MAP[engine][engine_offset];
	if (block == 7) return (EngineAddr[engine] + vram_addr);		// not mapped to LCD
	vram_addr -= MMU.LCD_VRAM_ADDR[block];
	return (LCDdst[block] + vram_addr);
}

static FORCEINLINE u32 MMU_LCDmap(u32 addr)
{
	//handle LCD memory mirroring
	if ((addr < 0x07000000) && (addr>=0x068A4000)) 
		return 0x06800000 + 
		//(addr%0xA4000); //yuck!! is this even how it mirrors? but we have to keep from overrunning the buffer somehow
		(addr&0x80000); //just as likely to be right (I have no clue how it should work) but faster.

	if ((addr < 0x6000000)) return addr;
	if ((addr > 0x661FFFF)) return addr;	// Engine BOBJ max 128KB

	// holes
	if ((addr > 0x6080000) && (addr < 0x6200000)) return addr;	// Engine ABG max 512KB
	if ((addr > 0x6220000) && (addr < 0x6400000)) return addr;	// Engine BBG max 128KB
	if ((addr > 0x6420000) && (addr < 0x6600000)) return addr;	// Engine AOBJ max 256KB
	
	u32	save_addr = addr;

	addr &= 0x0FFFFFF;
	u8	engine = (addr >> 21);
	addr &= 0x01FFFFF;
	u8	engine_offset = (addr >> 14);
	u8	block = MMU.VRAM_MAP[engine][engine_offset];
	if (block == 7) return (save_addr);		// not mapped to LCD
	addr -= MMU.LCD_VRAM_ADDR[block];
	return (addr + LCDdata[block][0]);
}

template<u8 DMA_CHANNEL>
void DMAtoVRAMmapping()
{
	if (DMADst[ARMCPU_ARM9][DMA_CHANNEL] < 0x6000000) return;
	if (DMADst[ARMCPU_ARM9][DMA_CHANNEL] > 0x661FFFF) return;

	u32 addr = DMADst[ARMCPU_ARM9][DMA_CHANNEL];

	addr &= 0x0FFFFFF;
	u8	engine = (addr >> 21);
	addr &= 0x01FFFFF;
	u8	engine_offset = (addr >> 14);
	u8	block = MMU.VRAM_MAP[engine][engine_offset];
	if (block == 7) return;
	addr -= MMU.LCD_VRAM_ADDR[block];

	//INFO("ARM9 DMA%i at dst address 0x%08X mapped to 0x%X\n", DMA_CHANNEL, DMADst[ARMCPU_ARM9][DMA_CHANNEL], (addr + LCDdata[block][0]) );

	DMADst[ARMCPU_ARM9][DMA_CHANNEL] = (addr + LCDdata[block][0]);
}

#define LOG_VRAM_ERROR() LOG("No data for block %i MST %i\n", block, VRAMBankCnt & 0x07);

static inline void MMU_VRAMmapControl(u8 block, u8 VRAMBankCnt)
{
	if (!(VRAMBankCnt & 0x80)) return;		// disabled

	u32	vram_map_addr = 0xFFFFFFFF;
	u8	*LCD_addr = LCDdst[block];
	bool changingTexOrTexPalette = false;

	for (int i = 0; i < 4; i++)
	{
		for (int t = 0; t < 32; t++)
			if (MMU.VRAM_MAP[i][t] == block) 
				MMU.VRAM_MAP[i][t] = 7;
	}
	
	switch (VRAMBankCnt & 0x07)
	{
		case 0:			// not mapped
			MMU.LCDCenable[block] = FALSE;
		return;
		case 1:
			switch(block)
			{
				case 0:		// A
				case 1:		// B
				case 2:		// C
				case 3:		// D		Engine A, BG
					vram_map_addr = ((VRAMBankCnt >> 3) & 3) * 0x20000;
					if(block == 2) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 2);
					if(block == 3) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 1);
				break ;
				case 4:		// E		Engine A, BG
					vram_map_addr = 0x0000000;
				break;
				case 5:		// F
				case 6:		// G		Engine A, BG
					vram_map_addr = (((VRAMBankCnt>>3)&1)*0x4000)+(((VRAMBankCnt>>4)&1)*0x10000);
				break;
				case 8:		// H		Engine B, BG
					vram_map_addr = 0x0200000;
				break ;
				case 9:		// I		Engine B, BG
					vram_map_addr = 0x0208000;
				break;
				default:
					LOG_VRAM_ERROR();
					break;
			}
		break ;

		case 2:
			switch(block)
			{
				case 0:		// A
				case 1:		// B		Engine A, OBJ
					vram_map_addr = 0x0400000 + (((VRAMBankCnt>>3)&1)*0x20000);
				break;
				case 2:		// C
					T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) | 1);
				break;
				case 3:		// D
					T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) | 2);
				break;
				case 4:		// E		Engine A, OBJ
					vram_map_addr = 0x0400000;
				break;
				case 5:		// F
				case 6:		// G		Engine A, OBJ
					vram_map_addr = 0x0400000 + (((VRAMBankCnt>>3)&1)*0x4000)+(((VRAMBankCnt>>4)&1)*0x10000);
				break;
				case 8:		// H		Engine B, BG
					ARM9Mem.ExtPal[1][0] = LCD_addr;
					ARM9Mem.ExtPal[1][1] = LCD_addr+0x2000;
					ARM9Mem.ExtPal[1][2] = LCD_addr+0x4000;
					ARM9Mem.ExtPal[1][3] = LCD_addr+0x6000;
				break;
				case 9:		// I		Engine B, OBJ
					vram_map_addr = 0x0600000;
				break;
				default:
					LOG_VRAM_ERROR();
					break;
			}
		break ;

		case 3:
			switch (block)
			{
				case 0:		// A
				case 1:		// B
				case 2:		// C
				case 3:		// D
					// Textures
					{
						changingTexOrTexPalette = true;
						int slot_index = (VRAMBankCnt >> 3) & 0x3;
						ARM9Mem.textureSlotAddr[slot_index] = LCD_addr;
						gpu3D->NDS_3D_VramReconfigureSignal();
						if(block == 2) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 2);
						if(block == 3) T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 1);
					}
				break;
				case 4:		// E
					changingTexOrTexPalette = true;
					ARM9Mem.texPalSlot[0] = LCD_addr;
					ARM9Mem.texPalSlot[1] = LCD_addr+0x4000;
					ARM9Mem.texPalSlot[2] = LCD_addr+0x8000;
					ARM9Mem.texPalSlot[3] = LCD_addr+0xC000;
					gpu3D->NDS_3D_VramReconfigureSignal();
				break;
				case 5:		// F
				case 6:		// G
				{
					changingTexOrTexPalette = true;
					u8 tmp_slot = ((VRAMBankCnt >> 3) & 0x01) + (((VRAMBankCnt >> 4) & 0x01)*4);
					ARM9Mem.texPalSlot[tmp_slot] = LCD_addr;
					gpu3D->NDS_3D_VramReconfigureSignal();
				}
				break;
				case 9:		// I		Engine B, OBJ
					ARM9Mem.ObjExtPal[1][0] = LCD_addr;
					ARM9Mem.ObjExtPal[1][1] = LCD_addr+0x2000;
				break;
				default:
					LOG_VRAM_ERROR();
					break;
			}
		break ;

		case 4:
			switch(block)
			{
				case 2:		// C		Engine B, BG
					vram_map_addr = 0x0200000;
					T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 2);
				break ;
				case 3:		// D		Engine B, OBJ
					vram_map_addr = 0x0600000;
					T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240, T1ReadByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x240) & 1);
				break ;
				case 4:		// E		Engine A, BG
					ARM9Mem.ExtPal[0][0] = LCD_addr;
					ARM9Mem.ExtPal[0][1] = LCD_addr+0x2000;
					ARM9Mem.ExtPal[0][2] = LCD_addr+0x4000;
					ARM9Mem.ExtPal[0][3] = LCD_addr+0x6000;
				break;
				case 5:		// F
				case 6:		// G		Engine A, BG
					{
						u8 tmp_slot = (VRAMBankCnt >> 2) & 0x02;
						ARM9Mem.ExtPal[0][tmp_slot] = LCD_addr;
						ARM9Mem.ExtPal[0][tmp_slot+1] = LCD_addr+0x2000;
					}
				break;
				default:
					LOG_VRAM_ERROR();
					break;
			}
		break;

		case 5:
			if ((block == 5) || (block == 6))		// F, G		Engine A, OBJ
			{
				ARM9Mem.ObjExtPal[0][0] = LCD_addr;
				ARM9Mem.ObjExtPal[0][1] = LCD_addr + 0x2000;
			}
		break;
	}

	if(changingTexOrTexPalette && !nds.isIn3dVblank())
	{
		PROGINFO("Changing texture or texture palette mappings outside of 3d vblank\n");
	}

	if (vram_map_addr != 0xFFFFFFFF)
	{
		u8	engine = (vram_map_addr >> 21);
		vram_map_addr &= 0x001FFFFF;
		u8	engine_offset = (vram_map_addr >> 14);
		MMU.LCD_VRAM_ADDR[block] = vram_map_addr;
		MMU.LCDCenable[block] = TRUE;

		for (unsigned int i = 0; i < LCDdata[block][1]; i++)
			MMU.VRAM_MAP[engine][engine_offset + i] = (u8)block;
		
		//INFO("VRAM %i mapping: eng=%i (offs=%i, size=%i), addr = 0x%X, MST=%i\n", 
		//	block, engine, engine_offset, LCDdata[block][1]*0x4000, MMU.LCD_VRAM_ADDR[block], VRAMBankCnt & 0x07);

		//unmap texmem
		for(int i=0;i<4;i++)
			if(ARM9Mem.textureSlotAddr[i] == LCD_addr)
				ARM9Mem.textureSlotAddr[i] = ARM9Mem.blank_memory;

		//unmap texpal mem. This is not a straightforward way to do it, 
		//but it is the only place we have this information stored.
		for(int i=0;i<4;i++)
			if( (ARM9Mem.texPalSlot[i] == LCD_addr + 0x4000*i) || (ARM9Mem.texPalSlot[i] == LCD_addr) )
				ARM9Mem.texPalSlot[i] = ARM9Mem.blank_memory;
		for(int i=4;i<6;i++)
			if(ARM9Mem.texPalSlot[i] == LCD_addr)
				ARM9Mem.texPalSlot[i] = ARM9Mem.blank_memory;
		return;
	}

	MMU.LCDCenable[block] = FALSE;
}

void MMU_setRom(u8 * rom, u32 mask)
{
	unsigned int i;
	MMU.CART_ROM = rom;
	MMU.CART_ROM_MASK = mask;
	
	for(i = 0x80; i<0xA0; ++i)
	{
		MMU_struct::MMU_MEM[0][i] = rom;
		MMU_struct::MMU_MEM[1][i] = rom;
		MMU_struct::MMU_MASK[0][i] = mask;
		MMU_struct::MMU_MASK[1][i] = mask;
	}
	rom_mask = mask;
}

void MMU_unsetRom()
{
	unsigned int i;
	MMU.CART_ROM=MMU.UNUSED_RAM;
	
	for(i = 0x80; i<0xA0; ++i)
	{
		MMU_struct::MMU_MEM[0][i] = MMU.UNUSED_RAM;
		MMU_struct::MMU_MEM[1][i] = MMU.UNUSED_RAM;
		MMU_struct::MMU_MASK[0][i] = ROM_MASK;
		MMU_struct::MMU_MASK[1][i] = ROM_MASK;
	}
	rom_mask = ROM_MASK;
}
char txt[80];	

static void execsqrt() {
	u32 ret;
	u16 cnt = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B0);

	if (cnt&1) { 
		u64 v = T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B8);
		ret = isqrt(v);
	} else {
		u32 v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B8);
		ret = isqrt(v);
	}
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B4, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2B0, cnt | 0x8000);

	MMU.sqrtCycles = (nds.cycles + 26);
	MMU.sqrtResult = ret;
	MMU.sqrtCnt = (cnt & 0x7FFF);
	MMU.sqrtRunning = TRUE;
}

static void execdiv() {
	u16 cnt = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x280);
	s64 num,den;
	s64 res,mod;

	switch(cnt&3)
	{
	case 0:
		num = (s64) (s32) T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290);
		den = (s64) (s32) T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298);
		MMU.divCycles = (nds.cycles + 36);
		break;
	case 1:
	case 3: //gbatek says this is same as mode 1
		num = (s64) T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290);
		den = (s64) (s32) T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298);
		MMU.divCycles = (nds.cycles + 68);
		break;
	case 2:
	default:
		num = (s64) T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x290);
		den = (s64) T1ReadQuad(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x298);
		MMU.divCycles = (nds.cycles + 68);
		break;
	}

	if(den==0)
	{
		res = ((num < 0) ? 1 : -1);
		mod = num;
		cnt |= 0x4000;
		cnt &= 0x7FFF;
	}
	else
	{
		res = num / den;
		mod = num % den;
		cnt &= 0x3FFF;
	}

	DIVLOG("DIV %08X%08X / %08X%08X = %08X%08X\r\n", (u32)(num>>32), (u32)num, 
							(u32)(den>>32), (u32)den, 
							(u32)(res>>32), (u32)res);

	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A0, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A4, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2A8, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x2AC, 0);
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x280, ((cnt & 0xBFFF) | 0x8000));

	MMU.divResult = res;
	MMU.divMod = mod;
	MMU.divCnt = (cnt & 0x7FFF);
	MMU.divRunning = TRUE;
}

template<int PROCNUM> 
void FASTCALL MMU_doDMA(u32 num)
{
	u32 src = DMASrc[PROCNUM][num];
	u32 dst = DMADst[PROCNUM][num];
        u32 taille;

	if(src==dst)
	{
		T1WriteLong(MMU.MMU_MEM[PROCNUM][0x40], 0xB8 + (0xC*num), T1ReadLong(MMU.MMU_MEM[PROCNUM][0x40], 0xB8 + (0xC*num)) & 0x7FFFFFFF);
		return;
	}
	
	if((!(MMU.DMACrt[PROCNUM][num]&(1<<31)))&&(!(MMU.DMACrt[PROCNUM][num]&(1<<25))))
	{       /* not enabled and not to be repeated */
		MMU.DMAStartTime[PROCNUM][num] = 0;
		MMU.DMACycle[PROCNUM][num] = 0;
		//MMU.DMAing[PROCNUM][num] = FALSE;
		return;
	}
	
	
	/* word count */
	taille = (MMU.DMACrt[PROCNUM][num]&0x1FFFF);
	
	// If we are in "Main memory display" mode just copy an entire 
	// screen (256x192 pixels). 
	//    Reference:  http://nocash.emubase.de/gbatek.htm#dsvideocaptureandmainmemorydisplaymode
	//       (under DISP_MMEM_FIFO)
	if ((MMU.DMAStartTime[PROCNUM][num]==4) &&		// Must be in main memory display mode
		(taille==4) &&							// Word must be 4
		(((MMU.DMACrt[PROCNUM][num]>>26)&1) == 1))	// Transfer mode must be 32bit wide
		taille = 24576; //256*192/2;
	
	if(MMU.DMAStartTime[PROCNUM][num] == 5)
		taille *= 0x80;
	
	MMU.DMACycle[PROCNUM][num] = taille + nds.cycles;
	MMU.DMAing[PROCNUM][num] = TRUE;
	MMU.CheckDMAs |= (1<<(num+(PROCNUM<<2)));
	
	DMALOG("PROCNUM %d, dma %d src %08X dst %08X start %d taille %d repeat %s %08X\r\n",
		PROCNUM, num, src, dst, MMU.DMAStartTime[PROCNUM][num], taille,
		(MMU.DMACrt[PROCNUM][num]&(1<<25))?"on":"off",MMU.DMACrt[PROCNUM][num]);
	
	if(!(MMU.DMACrt[PROCNUM][num]&(1<<25)))
		MMU.DMAStartTime[PROCNUM][num] = 0;
	
	// transfer
	{
		u32 i=0;
		// 32 bit or 16 bit transfer ?
		int sz = ((MMU.DMACrt[PROCNUM][num]>>26)&1)? 4 : 2; 
		int dstinc,srcinc;
		int u=(MMU.DMACrt[PROCNUM][num]>>21);
		switch(u & 0x3) {
			case 0 :  dstinc =  sz; break;
			case 1 :  dstinc = -sz; break;
			case 2 :  dstinc =   0; break;
			case 3 :  dstinc =  sz; break; //reload
			default:
				return;
		}
		switch((u >> 2)&0x3) {
			case 0 :  srcinc =  sz; break;
			case 1 :  srcinc = -sz; break;
			case 2 :  srcinc =   0; break;
			case 3 :  // reserved
				return;
			default:
				return;
		}

		if ((MMU.DMACrt[PROCNUM][num]>>26)&1)
			for(; i < taille; ++i)
			{
				_MMU_write32<PROCNUM>(dst, _MMU_read32<PROCNUM>(src));
				dst += dstinc;
				src += srcinc;
			}
		else
			for(; i < taille; ++i)
			{
				_MMU_write16<PROCNUM>(dst, _MMU_read16<PROCNUM>(src));
				dst += dstinc;
				src += srcinc;
			}

		//this is necessary for repeating DMA such as to scroll registers for NSMB level backdrop scrolling effect
//#if 0
		//write back the addresses
		DMASrc[PROCNUM][num] = src;
		if((u & 0x3)!=3) //but dont write back dst if we were supposed to reload
			DMADst[PROCNUM][num] = dst;
//#endif
	}
}

#ifdef MMU_ENABLE_ACL

INLINE void check_access(u32 adr, u32 access) {
	/* every other mode: sys */
	access |= 1;
	if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10) {
		/* is user mode access */
		access ^= 1 ;
	}
	if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE) {
		execute = FALSE ;
	}
}
INLINE void check_access_write(u32 adr) {
	u32 access = CP15_ACCESS_WRITE;
	check_access(adr, access)
}

u8 FASTCALL MMU_read8_acl(u32 proc, u32 adr, u32 access)
{
	/* on arm9 we need to check the MPU regions */
	if (proc == ARMCPU_ARM9)
		check_access(u32 adr, u32 access);
	return MMU_read8(proc,adr);
}
u16 FASTCALL MMU_read16_acl(u32 proc, u32 adr, u32 access)
{
	/* on arm9 we need to check the MPU regions */
	if (proc == ARMCPU_ARM9)
		check_access(u32 adr, u32 access);
	return MMU_read16(proc,adr);
}
u32 FASTCALL MMU_read32_acl(u32 proc, u32 adr, u32 access)
{
	/* on arm9 we need to check the MPU regions */
	if (proc == ARMCPU_ARM9)
		check_access(u32 adr, u32 access);
	return MMU_read32(proc,adr);
}

void FASTCALL MMU_write8_acl(u32 proc, u32 adr, u8 val)
{
	/* check MPU region on ARM9 */
	if (proc == ARMCPU_ARM9)
		check_access_write(adr);
	MMU_write8(proc,adr,val);
}
void FASTCALL MMU_write16_acl(u32 proc, u32 adr, u16 val)
{
	/* check MPU region on ARM9 */
	if (proc == ARMCPU_ARM9)
		check_access_write(adr);
	MMU_write16(proc,adr,val) ;
}
void FASTCALL MMU_write32_acl(u32 proc, u32 adr, u32 val)
{
	/* check MPU region on ARM9 */
	if (proc == ARMCPU_ARM9)
		check_access_write(adr);
	MMU_write32(proc,adr,val) ;
}
#endif

#ifdef PROFILE_MEMORY_ACCESS

#define PROFILE_PREFETCH 0
#define PROFILE_READ 1
#define PROFILE_WRITE 2

struct mem_access_profile {
  u64 num_accesses;
  u32 address_mask;
  u32 masked_value;
};

#define PROFILE_NUM_MEM_ACCESS_PROFILES 4

static u64 profile_num_accesses[2][3];
static u64 profile_unknown_addresses[2][3];
static struct mem_access_profile
profile_memory_accesses[2][3][PROFILE_NUM_MEM_ACCESS_PROFILES];

static void
setup_profiling( void) {
  int i;

  for ( i = 0; i < 2; i++) {
    int access_type;

    for ( access_type = 0; access_type < 3; access_type++) {
      profile_num_accesses[i][access_type] = 0;
      profile_unknown_addresses[i][access_type] = 0;

      /*
       * Setup the access testing structures
       */
      profile_memory_accesses[i][access_type][0].address_mask = 0x0e000000;
      profile_memory_accesses[i][access_type][0].masked_value = 0x00000000;
      profile_memory_accesses[i][access_type][0].num_accesses = 0;

      /* main memory */
      profile_memory_accesses[i][access_type][1].address_mask = 0x0f000000;
      profile_memory_accesses[i][access_type][1].masked_value = 0x02000000;
      profile_memory_accesses[i][access_type][1].num_accesses = 0;

      /* shared memory */
      profile_memory_accesses[i][access_type][2].address_mask = 0x0f800000;
      profile_memory_accesses[i][access_type][2].masked_value = 0x03000000;
      profile_memory_accesses[i][access_type][2].num_accesses = 0;

      /* arm7 memory */
      profile_memory_accesses[i][access_type][3].address_mask = 0x0f800000;
      profile_memory_accesses[i][access_type][3].masked_value = 0x03800000;
      profile_memory_accesses[i][access_type][3].num_accesses = 0;
    }
  }
}

static void
profile_memory_access( int arm9, u32 adr, int access_type) {
  static int first = 1;
  int mem_profile;
  int address_found = 0;

  if ( first) {
    setup_profiling();
    first = 0;
  }

  profile_num_accesses[arm9][access_type] += 1;

  for ( mem_profile = 0;
        mem_profile < PROFILE_NUM_MEM_ACCESS_PROFILES &&
          !address_found;
        mem_profile++) {
    if ( (adr & profile_memory_accesses[arm9][access_type][mem_profile].address_mask) ==
         profile_memory_accesses[arm9][access_type][mem_profile].masked_value) {
      /*printf( "adr %08x mask %08x res %08x expected %08x\n",
              adr,
              profile_memory_accesses[arm9][access_type][mem_profile].address_mask,
              adr & profile_memory_accesses[arm9][access_type][mem_profile].address_mask,
              profile_memory_accesses[arm9][access_type][mem_profile].masked_value);*/
      address_found = 1;
      profile_memory_accesses[arm9][access_type][mem_profile].num_accesses += 1;
    }
  }

  if ( !address_found) {
    profile_unknown_addresses[arm9][access_type] += 1;
  }
}


static const char *access_type_strings[] = {
  "prefetch",
  "read    ",
  "write   "
};

void
print_memory_profiling( void) {
  int arm;

  printf("------ Memory access profile ------\n");

  for ( arm = 0; arm < 2; arm++) {
    int access_type;

    for ( access_type = 0; access_type < 3; access_type++) {
      int mem_profile;
      printf("ARM%c: num of %s %lld\n",
             arm ? '9' : '7',
             access_type_strings[access_type],
             profile_num_accesses[arm][access_type]);

      for ( mem_profile = 0;
            mem_profile < PROFILE_NUM_MEM_ACCESS_PROFILES;
            mem_profile++) {
        printf( "address %08x: %lld\n",
                profile_memory_accesses[arm][access_type][mem_profile].masked_value,
                profile_memory_accesses[arm][access_type][mem_profile].num_accesses);
      }
              
      printf( "unknown addresses %lld\n",
              profile_unknown_addresses[arm][access_type]);

      printf( "\n");
    }
  }

  printf("------ End of Memory access profile ------\n\n");
}
#else
void
print_memory_profiling( void) {
}
#endif /* End of PROFILE_MEMORY_ACCESS area */

static u16 FASTCALL
arm9_prefetch16( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_PREFETCH);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if((adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Returns data from DTCM (ARM9 only) */
      return T1ReadWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
    }
  /* access to main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    return T1ReadWord( MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20) & 0xFF],
                       adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]);
  }
#endif

  return _MMU_read16<ARMCPU_ARM9>(adr);
}
static u32 FASTCALL
arm9_prefetch32( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_PREFETCH);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if((adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Returns data from DTCM (ARM9 only) */
      return T1ReadLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
    }
  /* access to main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    return T1ReadLong( MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20) & 0xFF],
                       adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]);
  }
#endif

  return _MMU_read32<ARMCPU_ARM9>(adr);
}

static u8 FASTCALL
arm9_read8( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_READ);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if( (adr&(~0x3FFF)) == MMU.DTCMRegion)
    {
      return ARM9Mem.ARM9_DTCM[adr&0x3FFF];
    }
  /* access to main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    return MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20) & 0xFF]
      [adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]];
  }
#endif

  return _MMU_read08<ARMCPU_ARM9>(adr);
}
static u16 FASTCALL
arm9_read16( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_READ);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if((adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Returns data from DTCM (ARM9 only) */
      return T1ReadWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
    }

  /* access to main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    return T1ReadWord( MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20) & 0xFF],
                       adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]);
  }
#endif

  return _MMU_read16<ARMCPU_ARM9>(adr);
}
static u32 FASTCALL
arm9_read32( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_READ);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if((adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Returns data from DTCM (ARM9 only) */
      return T1ReadLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
    }
  /* access to main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    return T1ReadLong( MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20) & 0xFF],
                       adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]);
  }
#endif

  return _MMU_read32<ARMCPU_ARM9>(adr);
}


static void FASTCALL
arm9_write8(void *data, u32 adr, u8 val) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_WRITE);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if( (adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Writes data in DTCM (ARM9 only) */
      ARM9Mem.ARM9_DTCM[adr&0x3FFF] = val;
      return ;
    }
  /* main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF]
      [adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF]] = val;
    return;
  }
#endif

 _MMU_write08<ARMCPU_ARM9>(adr, val);
}
static void FASTCALL
arm9_write16(void *data, u32 adr, u16 val) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_WRITE);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if((adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Writes in DTCM (ARM9 only) */
      T1WriteWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF, val);
      return;
    }
  /* main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    T1WriteWord( MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF],
                 adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF], val);
    return;
  }
#endif

  _MMU_write16<ARMCPU_ARM9>(adr, val);
}
static void FASTCALL
arm9_write32(void *data, u32 adr, u32 val) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 1, adr, PROFILE_WRITE);
#endif

#ifdef EARLY_MEMORY_ACCESS
  if((adr & ~0x3FFF) == MMU.DTCMRegion)
    {
      /* Writes in DTCM (ARM9 only) */
      T1WriteLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF, val);
      return;
    }
  /* main memory */
  if ( (adr & 0x0f000000) == 0x02000000) {
    T1WriteLong( MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF],
                 adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF], val);
    return;
  }
#endif

  _MMU_write32<ARMCPU_ARM9>(adr, val);
}




static u16 FASTCALL
arm7_prefetch16( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_PREFETCH);
#endif

#ifdef EARLY_MEMORY_ACCESS
  /* ARM7 private memory */
  if ( (adr & 0x0f800000) == 0x03800000) {
    T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF],
               adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]); 
  }
#endif

  return _MMU_read16<ARMCPU_ARM7>(adr);
}
static u32 FASTCALL
arm7_prefetch32( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_PREFETCH);
#endif

#ifdef EARLY_MEMORY_ACCESS
  /* ARM7 private memory */
  if ( (adr & 0x0f800000) == 0x03800000) {
    T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF],
               adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]); 
  }
#endif

  return _MMU_read32<ARMCPU_ARM7>(adr);
}

static u8 FASTCALL
arm7_read8( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_READ);
#endif

  return _MMU_read08<ARMCPU_ARM7>(adr);
}
static u16 FASTCALL
arm7_read16( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_READ);
#endif

  return _MMU_read16<ARMCPU_ARM7>(adr);
}
static u32 FASTCALL
arm7_read32( void *data, u32 adr) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_READ);
#endif

  return _MMU_read32<ARMCPU_ARM7>(adr);
}

static void FASTCALL
arm7_write8(void *data, u32 adr, u8 val) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_WRITE);
#endif

  _MMU_write08<ARMCPU_ARM7>(adr, val);
}
static void FASTCALL
arm7_write16(void *data, u32 adr, u16 val) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_WRITE);
#endif

  _MMU_write16<ARMCPU_ARM7>(adr, val);
}
static void FASTCALL
arm7_write32(void *data, u32 adr, u32 val) {
#ifdef PROFILE_MEMORY_ACCESS
  profile_memory_access( 0, adr, PROFILE_WRITE);
#endif

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

static INLINE void MMU_IPCSync(u8 proc, u32 val)
{
	//INFO("IPC%s sync 0x%04X (0x%02X|%02X)\n", proc?"7":"9", val, val >> 8, val & 0xFF);
	u32 sync_l = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x180) & 0xFFFF;
	u32 sync_r = T1ReadLong(MMU.MMU_MEM[proc^1][0x40], 0x180) & 0xFFFF;

	sync_l = ( sync_l & 0x600F ) | ( val & 0x0F00 );
	sync_r = ( sync_r & 0x6F00 ) | ( (val >> 8) & 0x000F );

	T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x180, sync_l);
	T1WriteLong(MMU.MMU_MEM[proc^1][0x40], 0x180, sync_r);

	if ((val & 0x2000) && (sync_r & 0x4000))
		MMU.reg_IF[proc^1] |= ( 1 << 16 );
}

//================================================================================================== ARM9 *
//=========================================================================================================
//=========================================================================================================
//================================================= MMU write 08
void FASTCALL _MMU_ARM9_write08(u32 adr, u8 val)
{
#ifdef INTERNAL_DTCM_WRITE
	if(((adr & ~0x3FFF) == MMU.DTCMRegion))
	{
		/* Writes data in DTCM (ARM9 only) */
		ARM9Mem.ARM9_DTCM[adr & 0x3FFF] = val;
		return ;
	}
#endif
	if(adr < 0x02000000)
	{
		T1WriteByte(ARM9Mem.ARM9_ITCM, adr&0x7FFF, val);
		return ;
	}

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write08(adr, val);
		return;
	}
#else
	// CFlash writing, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000)) {
		cflash_write(adr,val);
		return;
	}
#endif

	adr &= 0x0FFFFFFF;

	if (adr >> 24 == 4)
	{
		switch(adr)
		{
			case REG_DISPA_DISP3DCNT:
			{
				u32 &disp3dcnt = MainScreen.gpu->dispx_st->dispA_DISP3DCNT.val;
				disp3dcnt = (disp3dcnt&0xFF00) | val;
				gfx3d_Control(disp3dcnt);
				break;
			}
			case REG_DISPA_DISP3DCNT+1:
			{
				u32 &disp3dcnt = MainScreen.gpu->dispx_st->dispA_DISP3DCNT.val;
				disp3dcnt = (disp3dcnt&0x00FF) | (val<<8);
				gfx3d_Control(disp3dcnt);
				break;
			}

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

			case 0x4000247:	
				/* Update WRAMSTAT at the ARM7 side */
				T1WriteByte(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x241, val);
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
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM9(adr, "(write08) %0x%X", val);
#endif
		MMU.MMU_MEM[ARMCPU_ARM9][0x40][adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]]=val;
		return;
	}

	adr = MMU_LCDmap(adr);
	
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [shash]
	MMU.MMU_MEM[ARMCPU_ARM9][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20]]=val;
}

//================================================= MMU ARM9 write 16
void FASTCALL _MMU_ARM9_write16(u32 adr, u16 val)
{
#ifdef INTERNAL_DTCM_WRITE
	if((adr & ~0x3FFF) == MMU.DTCMRegion)
	{
		/* Writes in DTCM (ARM9 only) */
		T1WriteWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF, val);
		return;
	}
#endif
	if (adr < 0x02000000)
	{
		T1WriteWord(ARM9Mem.ARM9_ITCM, adr&0x7FFF, val);
		return ;
	}

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write16(adr, val);
		return;
	}
#else
	// CFlash writing, Mic
	if ((adr>=0x08800000)&&(adr<0x09900000))
	{
		cflash_write(adr,val);
		return;
	}
#endif

	adr &= 0x0FFFFFFF;

	if((adr >> 24) == 4)
	{
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
			case REG_DISPA_DISP3DCNT:
			{
				MainScreen.gpu->dispx_st->dispA_DISP3DCNT.val = val;
				gfx3d_Control(val);
				break;
			}

			// Alpha test reference value - Parameters:1
			case 0x04000340:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x340>>1] = val;
				gfx3d_glAlphaFunc(val);
				return;
			}
			// Clear background color setup - Parameters:2
			case 0x04000350:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x350>>1] = val;
				gfx3d_glClearColor(val);
				return;
			}
			// Clear background depth setup - Parameters:2
			case 0x04000354:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x354>>1] = val;
				gfx3d_glClearDepth(val);
				return;
			}
			// Fog Color - Parameters:4b
			case 0x04000358:
			{
				((u16 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x358>>1] = val;
				gfx3d_glFogColor(val);
				return;
			}
			case 0x0400035C:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x35C>>1] = val;
				gfx3d_glFogOffset(val);
				return;
			}

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
			case REG_DISPA_BG0HOFS:
				GPU_setBGxHOFS(0, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG0VOFS:
				GPU_setBGxVOFS(0, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG1HOFS:
				GPU_setBGxHOFS(1, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG1VOFS:
				GPU_setBGxVOFS(1, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG2HOFS:
				GPU_setBGxHOFS(2, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG2VOFS:
				GPU_setBGxVOFS(2, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG3HOFS:
				GPU_setBGxHOFS(3, MainScreen.gpu, val);
				break;
			case REG_DISPA_BG3VOFS:
				GPU_setBGxVOFS(3, MainScreen.gpu, val);
				break;

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

			case REG_DISPB_BG0HOFS:
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
				break;
			case REG_DISPB_WININ: 	 
				GPU_setWININ(SubScreen.gpu, val) ; 	 
				break ; 	 
			case REG_DISPB_WINOUT: 	 
				GPU_setWINOUT16(SubScreen.gpu, val) ; 	 
				break ;

			case REG_DISPB_MASTERBRIGHT:
				GPU_setMasterBrightness (SubScreen.gpu, val);
				break;
			
            case REG_POWCNT1 :
				{
// TODO: make this later
#if 0			
					u8	_LCD = (val) & 0x01;
					u8	_2DEngineA = (val>>1) & 0x01;
					u8	_2DEngineB = (val>>9) & 0x01;
					u8	_3DRender = (val>>2) & 0x01;
					u8	_3DGeometry = (val>>3) & 0x01;
#endif
					if(val & (1<<15))
					{
						//LOG("Main core on top\n");
						MainScreen.offset = 0;
						SubScreen.offset = 192;
					}
					else
					{
						//LOG("Main core on bottom (%04X)\n", val);
						MainScreen.offset = 192;
						SubScreen.offset = 0;
					}
					osdA->setOffset(MainScreen.offset);
					osdB->setOffset(SubScreen.offset);

					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x304, val);
				}
				
				return;

			case REG_EXMEMCNT:
			{
				u16 oldval = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x204, val);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204, (val & 0xFF80) | (oldval & 0x7F));
			}
			return;

			case REG_AUXSPICNT:
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][(REG_AUXSPICNT >> 20) & 0xff], REG_AUXSPICNT & 0xfff, val);
				AUX_SPI_CNT = val;

				if (val == 0)
				   mc_reset_com(&MMU.bupmem);     /* reset backup memory device communication */
				return;
				
			case REG_AUXSPIDATA:
				if(val!=0)
				   AUX_SPI_CMD = val & 0xFF;

				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, bm_transfer(&MMU.bupmem, val));
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
				{
					u32 old_val = MMU.reg_IME[ARMCPU_ARM9];
					u32 new_val = val & 0x01;
					MMU.reg_IME[ARMCPU_ARM9] = new_val;
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x208, val);
#ifndef NEW_IRQ
					if ( new_val && old_val != new_val) 
					{
						// raise an interrupt request to the CPU if needed
						if ( MMU.reg_IE[ARMCPU_ARM9] & MMU.reg_IF[ARMCPU_ARM9])
						{
							NDS_ARM9.wIRQ = TRUE;
							NDS_ARM9.waitIRQ = FALSE;
						}
					}
#endif
				return;
				}
			case REG_IE :
				MMU.reg_IE[ARMCPU_ARM9] = (MMU.reg_IE[ARMCPU_ARM9]&0xFFFF0000) | val;
#ifndef NEW_IRQ
				if ( MMU.reg_IME[ARMCPU_ARM9])
				{
					// raise an interrupt request to the CPU if needed
					if ( MMU.reg_IE[ARMCPU_ARM9] & MMU.reg_IF[ARMCPU_ARM9]) 
					{
						NDS_ARM9.wIRQ = TRUE;
						NDS_ARM9.waitIRQ = FALSE;
					}
				}
#endif
				return;
			case REG_IE + 2 :
				MMU.reg_IE[ARMCPU_ARM9] = (MMU.reg_IE[ARMCPU_ARM9]&0xFFFF) | (((u32)val)<<16);
#ifndef NEW_IRQ
				if ( MMU.reg_IME[ARMCPU_ARM9])
				{
					// raise an interrupt request to the CPU if needed
					if ( MMU.reg_IE[ARMCPU_ARM9] & MMU.reg_IF[ARMCPU_ARM9]) 
					{
						NDS_ARM9.wIRQ = TRUE;
						NDS_ARM9.waitIRQ = FALSE;
					}
				}
#endif
				return;
				
			case REG_IF :
				MMU.reg_IF[ARMCPU_ARM9] &= (~((u32)val)); 
				return;
			case REG_IF + 2 :
				MMU.reg_IF[ARMCPU_ARM9] &= (~(((u32)val)<<16));
				return;

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
				int mask		= ((val&0x80)>>7) << timerIndex;
				MMU.CheckTimers = (MMU.CheckTimers & (~mask)) | mask;

				if(val&0x80)
					MMU.timer[ARMCPU_ARM9][timerIndex] = MMU.timerReload[ARMCPU_ARM9][((adr-2)>>2)&0x3];

				MMU.timerON[ARMCPU_ARM9][((adr-2)>>2)&0x3] = val & 0x80;			

				switch(val&7)
				{
					case 0 :
						MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 0+1;//ARMCPU_ARM9;
						break;
					case 1 :
						MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 6+1;//ARMCPU_ARM9; 
						break;
					case 2 :
						MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 8+1;//ARMCPU_ARM9;
						break;
					case 3 :
						MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 10+1;//ARMCPU_ARM9;
						break;
					default :
						MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 0xFFFF;
						break;
				}

				if(!(val & 0x80))
					MMU.timerRUN[ARMCPU_ARM9][timerIndex] = FALSE;

				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & 0xFFF, val);
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
					GPU_set_DISPCAPCNT(val);
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x64, v);
					return;
				}
			case REG_DISPA_DISPCAPCNT + 2:
				{
					u32 v = (T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x64) & 0xFFFF) | val; 
					GPU_set_DISPCAPCNT(val);
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

			case REG_DMA0CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma0 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xBA, val);
					DMASrc[ARMCPU_ARM9][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xB0);
					DMADst[ARMCPU_ARM9][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xB4);
					DMAtoVRAMmapping<0>();
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xB8);
					MMU.DMAStartTime[ARMCPU_ARM9][0] = (v>>27) & 0x7;
					MMU.DMACrt[ARMCPU_ARM9][0] = v;
					if(MMU.DMAStartTime[ARMCPU_ARM9][0] == 0)
						MMU_doDMA<ARMCPU_ARM9>(0);
					#ifdef LOG_DMA2
					//else
					{
						LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM9, 0, DMASrc[ARMCPU_ARM9][0], DMADst[ARMCPU_ARM9][0], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
			case REG_DMA1CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma1 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC6, val);
					DMASrc[ARMCPU_ARM9][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xBC);
					DMADst[ARMCPU_ARM9][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC0);
					DMAtoVRAMmapping<1>();
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC4);
					MMU.DMAStartTime[ARMCPU_ARM9][1] = (v>>27) & 0x7;
					MMU.DMACrt[ARMCPU_ARM9][1] = v;
					if(MMU.DMAStartTime[ARMCPU_ARM9][1] == 0)
						MMU_doDMA<ARMCPU_ARM9>(1);
					#ifdef LOG_DMA2
					//else
					{
						LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM9, 1, DMASrc[ARMCPU_ARM9][1], DMADst[ARMCPU_ARM9][1], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
			case REG_DMA2CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma2 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD2, val);
					DMASrc[ARMCPU_ARM9][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC8);
					DMADst[ARMCPU_ARM9][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xCC);
					DMAtoVRAMmapping<2>();
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD0);
					MMU.DMAStartTime[ARMCPU_ARM9][2] = (v>>27) & 0x7;
					MMU.DMACrt[ARMCPU_ARM9][2] = v;
					if(MMU.DMAStartTime[ARMCPU_ARM9][2] == 0)
						MMU_doDMA<ARMCPU_ARM9>(2);
					#ifdef LOG_DMA2
					//else
					{
						LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM9, 2, DMASrc[ARMCPU_ARM9][2], DMADst[ARMCPU_ARM9][2], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
			case REG_DMA3CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma3 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xDE, val);
					DMASrc[ARMCPU_ARM9][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD4);
					DMADst[ARMCPU_ARM9][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD8);
					DMAtoVRAMmapping<3>();
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xDC);
					MMU.DMAStartTime[ARMCPU_ARM9][3] = (v>>27) & 0x7;
					MMU.DMACrt[ARMCPU_ARM9][3] = v;
			
					if(MMU.DMAStartTime[ARMCPU_ARM9][3] == 0)
						MMU_doDMA<ARMCPU_ARM9>(3);
					#ifdef LOG_DMA2
					//else
					{
						LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM9, 3, DMASrc[ARMCPU_ARM9][3], DMADst[ARMCPU_ARM9][3], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
                        //case REG_AUXSPICNT : emu_halt();
			case REG_DISPA_DISPMMEMFIFO:
			{
				DISP_FIFOsend(val);
				return;
			}
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM9(adr, "(write16) %0x%X", val);
#endif
		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val); 
		return;
	}

	adr = MMU_LCDmap(adr);			// VRAM mapping

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [shash]
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
} 

//================================================= MMU ARM9 write 32
void FASTCALL _MMU_ARM9_write32(u32 adr, u32 val)
{
#ifdef INTERNAL_DTCM_WRITE
	if((adr&(~0x3FFF)) == MMU.DTCMRegion)
	{
		T1WriteLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF, val);
		return ;
	}
#endif

#ifdef EARLY_MEMORY_ACCESS
	if ( (adr & 0x0F000000) == 0x02000000) {
		T1WriteLong( ARM9Mem.MAIN_MEM, adr & 0x3FFFFF, val);
		return;
	}
#endif

	if(adr<0x02000000)
	{
		T1WriteLong(ARM9Mem.ARM9_ITCM, adr&0x7FFF, val);
		return ;
	}

	if(adr>=0x02400000 && adr<0x03000000) {
		//int zzz=9;
	}

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write32(adr, val);
		return;
	}
#else
	// CFlash writing, Mic
	if ((adr>=0x9000000) && (adr<0x9900000)) {
	   cflash_write(adr,val);
	   return;
	}
#endif

	adr &= 0x0FFFFFFF;

#if 0
	if ((adr & 0xFF800000) == 0x04800000) {
		// access to non regular hw registers
		// return to not overwrite valid data
		return ;
	}
#endif

	if((adr>>24)==4)
	{

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
			case 0x400036:		//fog table
			case 0x400037:
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
				gfx3d_sendCommand(adr, val);
				return;
			default:
				break;
		}

		switch(adr)
		{
			case REG_DISPA_BG2XL:
				MainScreen.gpu->setAffineStart(2,0,val);
				break;
			case REG_DISPA_BG2YL:
				MainScreen.gpu->setAffineStart(2,1,val);
				break;
			case REG_DISPB_BG2XL:
				SubScreen.gpu->setAffineStart(2,0,val);
				break;
			case REG_DISPB_BG2YL:
				SubScreen.gpu->setAffineStart(2,1,val);
				break;
			case REG_DISPA_BG3XL:
				MainScreen.gpu->setAffineStart(3,0,val);
				break;
			case REG_DISPA_BG3YL:
				MainScreen.gpu->setAffineStart(3,1,val);
				break;
			case REG_DISPB_BG3XL:
				SubScreen.gpu->setAffineStart(3,0,val);
				break;
			case REG_DISPB_BG3YL:
				SubScreen.gpu->setAffineStart(3,1,val);
				break;

			case 0x04000600:
				GFX_FIFOcnt(val);
				return;
			// Alpha test reference value - Parameters:1
			case 0x04000340:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x340>>2] = val;
				gfx3d_glAlphaFunc(val);
				return;
			}
			// Clear background color setup - Parameters:2
			case 0x04000350:
			{
				((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][0x40]))[0x350>>2] = val;
				gfx3d_glClearColor(val);
				return;
			}
			// Clear background depth setup - Parameters:2
			case 0x04000354:
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
				{
			        u32 old_val = MMU.reg_IME[ARMCPU_ARM9];
					u32 new_val = val & 0x01;
					MMU.reg_IME[ARMCPU_ARM9] = new_val;
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x208, val);
#ifndef NEW_IRQ
					if ( new_val && old_val != new_val) 
					{
						// raise an interrupt request to the CPU if needed
						if ( MMU.reg_IE[ARMCPU_ARM9] & MMU.reg_IF[ARMCPU_ARM9]) 
						{
							NDS_ARM9.wIRQ = TRUE;
							NDS_ARM9.waitIRQ = FALSE;
						}
					}
#endif
				}
				return;
				
			case REG_IE :
				MMU.reg_IE[ARMCPU_ARM9] = val;
#ifndef NEW_IRQ
				if ( MMU.reg_IME[ARMCPU_ARM9]) 
				{
					// raise an interrupt request to the CPU if needed
					if ( MMU.reg_IE[ARMCPU_ARM9] & MMU.reg_IF[ARMCPU_ARM9]) 
					{
						NDS_ARM9.wIRQ = TRUE;
						NDS_ARM9.waitIRQ = FALSE;
					}
				}
#endif
				return;
			
			case REG_IF :
				MMU.reg_IF[ARMCPU_ARM9] &= (~val); 
				return;

            case REG_TM0CNTL:
            case REG_TM1CNTL:
            case REG_TM2CNTL:
            case REG_TM3CNTL:
			{
				int timerIndex = (adr>>2)&0x3;
				int mask = ((val & 0x800000)>>(16+7)) << timerIndex;
				MMU.CheckTimers = (MMU.CheckTimers & (~mask)) | mask;

				MMU.timerReload[ARMCPU_ARM9][timerIndex] = (u16)val;
				if(val&0x800000)
					MMU.timer[ARMCPU_ARM9][timerIndex] = MMU.timerReload[ARMCPU_ARM9][(adr>>2)&0x3];

				MMU.timerON[ARMCPU_ARM9][timerIndex] = val & 0x800000;
				switch((val>>16)&7)
				{
					case 0 :
					MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 0+1;//ARMCPU_ARM9;
					break;
					case 1 :
					MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 6+1;//ARMCPU_ARM9;
					break;
					case 2 :
					MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 8+1;//ARMCPU_ARM9;
					break;
					case 3 :
					MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 10+1;//ARMCPU_ARM9;
					break;
					default :
					MMU.timerMODE[ARMCPU_ARM9][timerIndex] = 0xFFFF;
					break;
				}
				if(!(val & 0x800000))
					MMU.timerRUN[ARMCPU_ARM9][timerIndex] = FALSE;

				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & 0xFFF, val);
				return;
			}

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

			case REG_DMA0CNTL :
				//LOG("32 bit dma0 %04X\r\n", val);
				DMASrc[ARMCPU_ARM9][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xB0);
				DMADst[ARMCPU_ARM9][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xB4);
				DMAtoVRAMmapping<0>();
				MMU.DMAStartTime[ARMCPU_ARM9][0] = (val>>27) & 0x7;
				MMU.DMACrt[ARMCPU_ARM9][0] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xB8, val);
				if( MMU.DMAStartTime[ARMCPU_ARM9][0] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM9][0] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM9>(0);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM9, 0, DMASrc[ARMCPU_ARM9][0], DMADst[ARMCPU_ARM9][0], 0, ((MMU.DMACrt[ARMCPU_ARM9][0]>>27)&7));
				}
				#endif
				//emu_halt();
				return;
			case REG_DMA1CNTL:
				//LOG("32 bit dma1 %04X\r\n", val);
				DMASrc[ARMCPU_ARM9][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xBC);
				DMADst[ARMCPU_ARM9][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC0);
				DMAtoVRAMmapping<1>();
				MMU.DMAStartTime[ARMCPU_ARM9][1] = (val>>27) & 0x7;
				MMU.DMACrt[ARMCPU_ARM9][1] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC4, val);
				if(MMU.DMAStartTime[ARMCPU_ARM9][1] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM9][1] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM9>(1);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM9, 1, DMASrc[ARMCPU_ARM9][1], DMADst[ARMCPU_ARM9][1], 0, ((MMU.DMACrt[ARMCPU_ARM9][1]>>27)&7));
				}
				#endif
				return;
			case REG_DMA2CNTL :
				//LOG("32 bit dma2 %04X\r\n", val);
				DMASrc[ARMCPU_ARM9][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xC8);
				DMADst[ARMCPU_ARM9][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xCC);
				DMAtoVRAMmapping<2>();
				MMU.DMAStartTime[ARMCPU_ARM9][2] = (val>>27) & 0x7;
				MMU.DMACrt[ARMCPU_ARM9][2] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD0, val);
				if(MMU.DMAStartTime[ARMCPU_ARM9][2] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM9][2] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM9>(2);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM9, 2, DMASrc[ARMCPU_ARM9][2], DMADst[ARMCPU_ARM9][2], 0, ((MMU.DMACrt[ARMCPU_ARM9][2]>>27)&7));
				}
				#endif
				return;
			case REG_DMA3CNTL :
				//LOG("32 bit dma3 %04X\r\n", val);
				DMASrc[ARMCPU_ARM9][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD4);
				DMADst[ARMCPU_ARM9][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xD8);
				DMAtoVRAMmapping<3>();
				MMU.DMAStartTime[ARMCPU_ARM9][3] = (val>>27) & 0x7;
				MMU.DMACrt[ARMCPU_ARM9][3] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0xDC, val);
				if(	MMU.DMAStartTime[ARMCPU_ARM9][3] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM9][3] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM9>(3);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM9 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM9, 3, DMASrc[ARMCPU_ARM9][3], DMADst[ARMCPU_ARM9][3], 0, ((MMU.DMACrt[ARMCPU_ARM9][3]>>27)&7));
				}
				#endif
				return;
             case REG_GCROMCTRL :
				{
					if(!(val & 0x80000000))
					{
						MMU.dscard[ARMCPU_ARM9].address = 0;
						MMU.dscard[ARMCPU_ARM9].transfer_count = 0;

						val &= 0x7F7FFFFF;
						T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1A4, val);
						return;
					}

                    switch(MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT))
					{
						/* Dummy */
						case 0x9F:
							{
								MMU.dscard[ARMCPU_ARM9].address = 0;
								MMU.dscard[ARMCPU_ARM9].transfer_count = 0x800;
							}
							break;

						/* Data read */
						case 0x00:
						case 0xB7:
							{
								MMU.dscard[ARMCPU_ARM9].address = (MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT+1) << 24) | (MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT+2) << 16) | (MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT+3) << 8) | (MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT+4));
								MMU.dscard[ARMCPU_ARM9].transfer_count = 0x80;
							}
							break;

						/* Get ROM chip ID */
						case 0x90:
						case 0xB8:
							{
								MMU.dscard[ARMCPU_ARM9].address = 0;
								MMU.dscard[ARMCPU_ARM9].transfer_count = 1;
							}
							break;

						default:
							{
								LOG("CARD command: %02X\n", MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT));
								MMU.dscard[ARMCPU_ARM9].address = 0;
								MMU.dscard[ARMCPU_ARM9].transfer_count = 0;
							}
							break;
					}

					if(MMU.dscard[ARMCPU_ARM9].transfer_count == 0)
					{
						val &= 0x7F7FFFFF;
						T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1A4, val);
						return;
					}
					
                    val |= 0x00800000;
                    T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1A4, val);
										
					/* launch DMA if start flag was set to "DS Cart" */
					if(MMU.DMAStartTime[ARMCPU_ARM9][0] == 5)
					{
						MMU_doDMA<ARMCPU_ARM9>(0);
					}
					if(MMU.DMAStartTime[ARMCPU_ARM9][1] == 5)
					{
						MMU_doDMA<ARMCPU_ARM9>(1);
					}
					if(MMU.DMAStartTime[ARMCPU_ARM9][2] == 5)
					{
						MMU_doDMA<ARMCPU_ARM9>(2);
					}
					if(MMU.DMAStartTime[ARMCPU_ARM9][3] == 5)
					{
						MMU_doDMA<ARMCPU_ARM9>(3);
					}
				}
				return;
			case REG_DISPA_DISPCAPCNT :
				//INFO("MMU write32: REG_DISPA_DISPCAPCNT 0x%X\n", val);
				GPU_set_DISPCAPCNT(val);
				T1WriteLong(ARM9Mem.ARM9_REG, 0x64, val);
				return;
				
			case REG_DISPA_BG0CNT :
				GPU_setBGProp(MainScreen.gpu, 0, (val&0xFFFF));
				GPU_setBGProp(MainScreen.gpu, 1, (val>>16));
				//if((val>>16)==0x400) emu_halt();
				T1WriteLong(ARM9Mem.ARM9_REG, 8, val);
				return;
			case REG_DISPA_BG2CNT :
					GPU_setBGProp(MainScreen.gpu, 2, (val&0xFFFF));
					GPU_setBGProp(MainScreen.gpu, 3, (val>>16));
					T1WriteLong(ARM9Mem.ARM9_REG, 0xC, val);
				return;
			case REG_DISPB_BG0CNT :
					GPU_setBGProp(SubScreen.gpu, 0, (val&0xFFFF));
					GPU_setBGProp(SubScreen.gpu, 1, (val>>16));
					T1WriteLong(ARM9Mem.ARM9_REG, 0x1008, val);
				return;
			case REG_DISPB_BG2CNT :
					GPU_setBGProp(SubScreen.gpu, 2, (val&0xFFFF));
					GPU_setBGProp(SubScreen.gpu, 3, (val>>16));
					T1WriteLong(ARM9Mem.ARM9_REG, 0x100C, val);
				return;
			case REG_DISPA_DISPMMEMFIFO:
			{
				DISP_FIFOsend(val);
				return;
			}
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM9(adr, "(write32) %0x%X", val);
#endif

		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
		return;
	}

	adr = MMU_LCDmap(adr);			// VRAM mapping

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [shash]
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM9][adr>>20], val);
}

//================================================= MMU ARM9 read 08
u8 FASTCALL _MMU_ARM9_read08(u32 adr)
{
#ifdef INTERNAL_DTCM_READ
	if((adr&(~0x3FFF)) == MMU.DTCMRegion)
	{
		return T1ReadByte(ARM9Mem.ARM9_DTCM, adr&0x3FFF);
	}
#endif

	if(adr<0x02000000)
		return T1ReadByte(ARM9Mem.ARM9_ITCM, adr&0x7FFF);

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read08(adr);
#else
	// CFlash reading, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000))
		return (unsigned char)cflash_read(adr);
#endif

#ifdef _MMU_DEBUG
		mmu_log_debug_ARM9(adr, "(read08) %0x%X", 
			MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF]]);
#endif
	adr = MMU_LCDmap(adr);

	return MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF]];
}

//================================================= MMU ARM9 read 16
u16 FASTCALL _MMU_ARM9_read16(u32 adr)
{    
#ifdef INTERNAL_DTCM_READ
	if((adr&(~0x3FFF)) == MMU.DTCMRegion)
	{
		return T1ReadWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
	}
#endif

	if(adr<0x02000000)
		return T1ReadWord(ARM9Mem.ARM9_ITCM, adr & 0x7FFF);	

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read16(adr);
#else
	// CFlash reading, Mic
	if ((adr>=0x08800000) && (adr<0x09900000))
	   return (unsigned short)cflash_read(adr);
#endif

	adr &= 0x0FFFFFFF;

	if (adr >> 24 == 4)
	{
		// Address is an IO register
		switch(adr)
		{
			// ============================================= 3D
			case 0x04000604:
				return (gfx3d_GetNumPolys()&2047);
			case 0x04000606:
				return (gfx3d_GetNumVertex()&8191);
			case 0x04000630:
			case 0x04000632:
			case 0x04000634:
				return gfx3d_glGetVecRes((adr & 0xF) >> 1);
			// ============================================= 3D end
			case REG_IME :
				return (u16)MMU.reg_IME[ARMCPU_ARM9];
				
			case REG_IE :
				return (u16)MMU.reg_IE[ARMCPU_ARM9];
			case REG_IE + 2 :
				return (u16)(MMU.reg_IE[ARMCPU_ARM9]>>16);
				
			case REG_IF :
				return (u16)MMU.reg_IF[ARMCPU_ARM9];
			case REG_IF + 2 :
				return (u16)(MMU.reg_IF[ARMCPU_ARM9]>>16);

			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
				return MMU.timer[ARMCPU_ARM9][(adr&0xF)>>2];

			case 0x04000130:
			case 0x04000136:
				//not sure whether these should trigger from byte reads
				LagFrameFlag=0;
				break;
			
			case REG_POSTFLG :
				return 1;
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM9(adr, "(read16) %0x%X", 
			T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]));
#endif
		return  T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]);
	}

	adr = MMU_LCDmap(adr);
	
	/* Returns data from memory */
	return T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20) & 0xFF]); 
}

//================================================= MMU ARM9 read 32
u32 FASTCALL _MMU_ARM9_read32(u32 adr)
{
#ifdef INTERNAL_DTCM_READ
	if((adr&(~0x3FFF)) == MMU.DTCMRegion)
	{
		/* Returns data from DTCM (ARM9 only) */
		return T1ReadLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
	}
#endif

#ifdef EARLY_MEMORY_ACCESS
	if ( (adr & 0x0F000000) == 0x02000000)
		return T1ReadLong( ARM9Mem.MAIN_MEM, adr & 0x3FFFFF);
#endif

	if(adr>=0x02400000 && adr<0x03000000) {
		//int zzz=9;
	}

	if(adr<0x02000000) 
		return T1ReadLong(ARM9Mem.ARM9_ITCM, adr&0x7FFF);

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read32(adr);
#else
	// CFlash reading, Mic
	if ((adr>=0x9000000) && (adr<0x9900000))
	   return (unsigned long)cflash_read(adr);
#endif

	adr &= 0x0FFFFFFF;

	// Address is an IO register
	if((adr >> 24) == 4)
	{
		switch(adr)
		{
#ifndef USE_GEOMETRY_FIFO_EMULATION
			case 0x04000600:	// Geometry Engine Status Register (R and R/W)
			{
				
				u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20)], 
								adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20)]);

				// this is hack
				//gxstat |= 0x00000002;
				return gxstat;
			}
#endif

			case 0x04000640:
			case 0x04000644:
			case 0x04000648:
			case 0x0400064C:
			case 0x04000650:
			case 0x04000654:
			case 0x04000658:
			case 0x0400065C:
			case 0x04000660:
			case 0x04000664:
			case 0x04000668:
			case 0x0400066C:
			case 0x04000670:
			case 0x04000674:
			case 0x04000678:
			case 0x0400067C:
			{
				//LOG("4000640h..67Fh - CLIPMTX_RESULT - Read Current Clip Coordinates Matrix (R)");
				return gfx3d_GetClipMatrix ((adr-0x04000640)/4);
			}
			case 0x04000680:
			case 0x04000684:
			case 0x04000688:
			case 0x0400068C:
			case 0x04000690:
			case 0x04000694:
			case 0x04000698:
			case 0x0400069C:
			case 0x040006A0:
			{
				//LOG("4000680h..6A3h - VECMTX_RESULT - Read Current Directional Vector Matrix (R)");
				return gfx3d_GetDirectionalMatrix ((adr-0x04000680)/4);
			}

			case 0x4000604:
			{
				return (gfx3d_GetNumPolys()&2047) & ((gfx3d_GetNumVertex()&8191) << 16);
				//LOG ("read32 - RAM_COUNT -> 0x%X", ((u32 *)(MMU.MMU_MEM[ARMCPU_ARM9][(adr>>20)&0xFF]))[(adr&MMU.MMU_MASK[ARMCPU_ARM9][(adr>>20)&0xFF])>>2]);
			}

			case 0x04000620:
			case 0x04000624:
			case 0x04000628:
			case 0x0400062C:
			{
				return gfx3d_glGetPosRes((adr & 0xF) >> 2);
			}
			//	======================================== 3D end
			
			case REG_IME :
				return MMU.reg_IME[ARMCPU_ARM9];
			case REG_IE :
				return MMU.reg_IE[ARMCPU_ARM9];
			case REG_IF :
				return MMU.reg_IF[ARMCPU_ARM9];
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
			/*
			case 0x04000640 :	// TODO (clear): again, ??? 
				LOG("read proj\r\n");
			return 0;
			case 0x04000680 :
				LOG("read roat\r\n");
			return 0;
			case 0x04000620 :
				LOG("point res\r\n");
			return 0;
			*/
            case REG_GCDATAIN:
			{
                u32 val = 0;

				if(MMU.dscard[ARMCPU_ARM9].transfer_count == 0)
					return 0;

				switch(MEM_8(MMU.MMU_MEM[ARMCPU_ARM9], REG_GCCMDOUT))
				{
					/* Dummy */
					case 0x9F:
						{
							val = 0xFFFFFFFF;
						}
						break;

					/* Data read */
					case 0x00:
					case 0xB7:
						{
							/* TODO: prevent read if the address is out of range */
							/* Make sure any reads below 0x8000 redirect to 0x8000+(adr&0x1FF) as on real cart */
							if(MMU.dscard[ARMCPU_ARM9].address < 0x8000)
							{
								MMU.dscard[ARMCPU_ARM9].address = (0x8000 + (MMU.dscard[ARMCPU_ARM9].address&0x1FF));
							}
							val = T1ReadLong(MMU.CART_ROM, MMU.dscard[ARMCPU_ARM9].address & MMU.CART_ROM_MASK);
						}
						break;

					/* Get ROM chip ID */
					case 0x90:
					case 0xB8:
						{
							/* TODO */
							val = 0x00000000;
						}
						break;
				}

				MMU.dscard[ARMCPU_ARM9].address += 4;	// increment address

				MMU.dscard[ARMCPU_ARM9].transfer_count--;	// update transfer counter
				if(MMU.dscard[ARMCPU_ARM9].transfer_count) // if transfer is not ended
					return val;	// return data

				// transfer is done
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1A4, 
					T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1A4) & 0x7F7FFFFF);
	
				// if needed, throw irq for the end of transfer
				if(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x1A0) & 0x4000)
					NDS_makeInt(ARMCPU_ARM9, 19);

				return val;
			}
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM9(adr, "(read32) %0x%X", 
			T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20)]));
#endif
		return T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20)]);
	}
	
	adr = MMU_LCDmap(adr);

	//Returns data from memory
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [zeromus, inspired by shash]
	return T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][(adr >> 20)], adr & MMU.MMU_MASK[ARMCPU_ARM9][(adr >> 20)]);
}
//================================================================================================== ARM7 *
//=========================================================================================================
//=========================================================================================================
//================================================= MMU ARM7 write 08
void FASTCALL _MMU_ARM7_write08(u32 adr, u8 val)
{
#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write08(adr, val);
		return;
	}
#else
	// CFlash writing, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000)) 
	{
		cflash_write(adr,val);
		return;
	}
#endif

	adr &= 0x0FFFFFFF;
    // This is bad, remove it
	if ((adr>=0x04000400)&&(adr<0x0400051D)) 
	{
		SPU_WriteByte(adr, val);
		return;
    }

	if(adr == 0x04000301)
	{
		switch(val)
		{
		case 0xC0: NDS_Sleep(); break;
		default: break;
		}
	}

#ifdef EXPERIMENTAL_WIFI
	if ((adr & 0xFF800000) == 0x04800000)
	{
		/* is wifi hardware, dont intermix with regular hardware registers */
		/* FIXME handle 8 bit writes */
		return ;
	}
#endif

	if ( adr == REG_RTC ) rtcWrite(val);
	
#ifdef _MMU_DEBUG
	mmu_log_debug_ARM7(adr, "(write08) %0x%X", val);
#endif
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [shash]
	MMU.MMU_MEM[ARMCPU_ARM7][adr>>20][adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20]]=val;
}

//================================================= MMU ARM7 write 16
void FASTCALL _MMU_ARM7_write16(u32 adr, u16 val)
{
#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write16(adr, val);
		return;
	}
#else
	// CFlash writing, Mic
	if ((adr>=0x08800000)&&(adr<0x09900000))
	{
		cflash_write(adr,val);
		return;
	}
#endif

#ifdef EXPERIMENTAL_WIFI

	/* wifi mac access */
	if ((adr>=0x04800000)&&(adr<0x05000000))
	{
		WIFI_write16(&wifiMac,adr,val) ;
		return ;
	}
#else
	//if ((adr>=0x04800000)&&(adr<0x05000000)) return ;
#endif

	adr &= 0x0FFFFFFF;

	// This is bad, remove it
	if ((adr>=0x04000400)&&(adr<0x0400051D))
	{
		SPU_WriteWord(adr, val);
		return;
	}

	if((adr >> 24) == 4)
	{
		/* Address is an IO register */
		switch(adr)
		{
			case REG_RTC:
				rtcWrite(val);
				break;

			case REG_EXMEMCNT:
			{
				u16 oldval = T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204);
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x204, (val & 0x7F) | (oldval & 0xFF80));
			}
			return;

			case REG_AUXSPICNT:
				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPICNT >> 20) & 0xff], REG_AUXSPICNT & 0xfff, val);
				AUX_SPI_CNT = val;

				if (val == 0)
				   mc_reset_com(&MMU.bupmem);     // reset backup memory device communication
			return;

			case REG_AUXSPIDATA:
				if(val!=0)
				   AUX_SPI_CMD = val & 0xFF;

				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, bm_transfer(&MMU.bupmem, val));
			return;

			case REG_SPICNT :
				{
					int reset_firmware = 1;

					if ( ((SPI_CNT >> 8) & 0x3) == 1) 
					{
						if ( ((val >> 8) & 0x3) == 1) 
						{
							if ( BIT11(SPI_CNT)) 
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
					  mc_reset_com(&MMU.fw);
					}
					SPI_CNT = val;
					
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPICNT >> 20) & 0xff], REG_SPICNT & 0xfff, val);
				}
				return;
				
			case REG_SPIDATA :
				{
					u16 spicnt;

					if(val!=0)
						SPI_CMD = val;
			
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
									if(MMU.powerMan_CntReg & 0x80)
									{
										val = MMU.powerMan_Reg[MMU.powerMan_CntReg & 0x3];
									}
									else
									{
										MMU.powerMan_Reg[MMU.powerMan_CntReg & 0x3] = val;
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
							T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, fw_transfer(&MMU.fw, val));
						return;
						
						case 2 :
							switch(SPI_CMD & 0x70)
							{
								case 0x00 :
									val = 0;
									break;
								case 0x10 :
									//emu_halt();
									if(SPI_CNT&(1<<11))
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
								case 0x30 :
									val = 0;
									break;
								case 0x40 :
									val = 0;
									break;
								case 0x50 :
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
				{
					u32 old_val = MMU.reg_IME[ARMCPU_ARM7];
					u32 new_val = val & 1;
					MMU.reg_IME[ARMCPU_ARM7] = new_val;
					T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x208, val);
#ifndef NEW_IRQ
					if ( new_val && old_val != new_val) 
					{
						/* raise an interrupt request to the CPU if needed */
						if ( MMU.reg_IE[ARMCPU_ARM7] & MMU.reg_IF[ARMCPU_ARM7])
						{
							NDS_ARM7.wIRQ = TRUE;
							NDS_ARM7.waitIRQ = FALSE;
						}
					}
#endif
				return;
				}
			case REG_IE :
				MMU.reg_IE[ARMCPU_ARM7] = (MMU.reg_IE[ARMCPU_ARM7]&0xFFFF0000) | val;
#ifndef NEW_IRQ
				if ( MMU.reg_IME[ARMCPU_ARM7]) 
				{
					/* raise an interrupt request to the CPU if needed */
					if ( MMU.reg_IE[ARMCPU_ARM7] & MMU.reg_IF[ARMCPU_ARM7]) 
					{
						NDS_ARM7.wIRQ = TRUE;
						NDS_ARM7.waitIRQ = FALSE;
					}
				}
#endif
				return;
			case REG_IE + 2 :
				//emu_halt();
				MMU.reg_IE[ARMCPU_ARM7] = (MMU.reg_IE[ARMCPU_ARM7]&0xFFFF) | (((u32)val)<<16);
#ifndef NEW_IRQ
				if ( MMU.reg_IME[ARMCPU_ARM7]) 
				{
					/* raise an interrupt request to the CPU if needed */
					if ( MMU.reg_IE[ARMCPU_ARM7] & MMU.reg_IF[ARMCPU_ARM7]) 
					{
						NDS_ARM7.wIRQ = TRUE;
						NDS_ARM7.waitIRQ = FALSE;
					}
				}
#endif
				return;
				
			case REG_IF :
				//emu_halt();
				MMU.reg_IF[ARMCPU_ARM7] &= (~((u32)val)); 
				return;
			case REG_IF + 2 :
				//emu_halt();
				MMU.reg_IF[ARMCPU_ARM7] &= (~(((u32)val)<<16));
				return;
				
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
				int mask		= ((val&0x80)>>7) << (timerIndex+(ARMCPU_ARM7<<2));
				MMU.CheckTimers = (MMU.CheckTimers & (~mask)) | mask;

				if(val&0x80)
					MMU.timer[ARMCPU_ARM7][timerIndex] = MMU.timerReload[ARMCPU_ARM7][((adr-2)>>2)&0x3];

				MMU.timerON[ARMCPU_ARM7][((adr-2)>>2)&0x3] = val & 0x80;			

				switch(val&7)
				{
					case 0 :
						MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 0+1;//ARMCPU_ARM7;
						break;
					case 1 :
						MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 6+1;//ARMCPU_ARM7; 
						break;
					case 2 :
						MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 8+1;//ARMCPU_ARM7;
						break;
					case 3 :
						MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 10+1;//ARMCPU_ARM7;
						break;
					default :
						MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 0xFFFF;
						break;
				}

				if(!(val & 0x80))
					MMU.timerRUN[ARMCPU_ARM7][timerIndex] = FALSE;

				T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], adr & 0xFFF, val);
				return;
			}

			case REG_DMA0CNTH :
				{
					u32 v;

					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma0 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xBA, val);
					DMASrc[ARMCPU_ARM7][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xB0);
					DMADst[ARMCPU_ARM7][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xB4);
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xB8);
					MMU.DMAStartTime[ARMCPU_ARM7][0] = (v>>28) & 0x3;
					MMU.DMACrt[ARMCPU_ARM7][0] = v;
					if(MMU.DMAStartTime[ARMCPU_ARM7][0] == 0)
						MMU_doDMA<ARMCPU_ARM7>(0);
					#ifdef LOG_DMA2
					//else
					{
						LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM7, 0, DMASrc[ARMCPU_ARM7][0], DMADst[ARMCPU_ARM7][0], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
			case REG_DMA1CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma1 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC6, val);
					DMASrc[ARMCPU_ARM7][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xBC);
					DMADst[ARMCPU_ARM7][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC0);
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC4);
					MMU.DMAStartTime[ARMCPU_ARM7][1] = (v>>28) & 0x3;
					MMU.DMACrt[ARMCPU_ARM7][1] = v;
					if(MMU.DMAStartTime[ARMCPU_ARM7][1] == 0)
						MMU_doDMA<ARMCPU_ARM7>(1);
					#ifdef LOG_DMA2
					//else
					{
						LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM7, 1, DMASrc[ARMCPU_ARM7][1], DMADst[ARMCPU_ARM7][1], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
			case REG_DMA2CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma2 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD2, val);
					DMASrc[ARMCPU_ARM7][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC8);
					DMADst[ARMCPU_ARM7][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xCC);
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD0);
					MMU.DMAStartTime[ARMCPU_ARM7][2] = (v>>28) & 0x3;
					MMU.DMACrt[ARMCPU_ARM7][2] = v;
					if(MMU.DMAStartTime[ARMCPU_ARM7][2] == 0)
						MMU_doDMA<ARMCPU_ARM7>(2);
					#ifdef LOG_DMA2
					//else
					{
					LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM7, 2, DMASrc[ARMCPU_ARM7][2], DMADst[ARMCPU_ARM7][2], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
			case REG_DMA3CNTH :
				{
					u32 v;
					//if(val&0x8000) emu_halt();
					//LOG("16 bit dma3 %04X\r\n", val);
					T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xDE, val);
					DMASrc[ARMCPU_ARM7][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD4);
					DMADst[ARMCPU_ARM7][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD8);
					v = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xDC);
					MMU.DMAStartTime[ARMCPU_ARM7][3] = (v>>28) & 0x3;
					MMU.DMACrt[ARMCPU_ARM7][3] = v;

					if(MMU.DMAStartTime[ARMCPU_ARM7][3] == 0)
						MMU_doDMA<ARMCPU_ARM7>(3);
					#ifdef LOG_DMA2
					//else
					{
					LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X %s\r\n", ARMCPU_ARM7, 3, DMASrc[ARMCPU_ARM7][3], DMADst[ARMCPU_ARM7][3], (val&(1<<25))?"ON":"OFF");
					}
					#endif
				}
				return;
                        //case REG_AUXSPICNT : emu_halt();
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM7(adr, "(write16) %0x%X", val);
#endif
		T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val); 
		return;
	}

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [shash]
	T1WriteWord(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
} 
//================================================= MMU ARM7 write 32
void FASTCALL _MMU_ARM7_write32(u32 adr, u32 val)
{
#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
	{
		addon.write32(adr, val);
		return;
	}
#else
	// CFlash writing, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000)) {
	   cflash_write(adr,val);
	   return;
	}
#endif

	

#ifdef EXPERIMENTAL_WIFI
	if ((adr & 0xFF800000) == 0x04800000) 
	{
		// access to non regular hw registers
		// return to not overwrite valid data
		WIFI_write16(&wifiMac, adr, val & 0xFFFF);
		WIFI_write16(&wifiMac, adr+2, val >> 16);
		return ;
	}
#endif

	adr &= 0x0FFFFFFF;

    // This is bad, remove it
    if ((adr>=0x04000400)&&(adr<0x0400051D))
    {
        SPU_WriteLong(adr, val);
        return;
    }

	if((adr>>24)==4)
	{
		switch(adr)
		{
			case REG_RTC:
				rtcWrite((u16)val);
				break;

			case REG_IME : 
			{
				u32 old_val = MMU.reg_IME[ARMCPU_ARM7];
				u32 new_val = val & 1;
				MMU.reg_IME[ARMCPU_ARM7] = new_val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x208, val);
#ifndef NEW_IRQ
				if ( new_val && old_val != new_val) 
				{
					// raise an interrupt request to the CPU if needed
					if ( MMU.reg_IE[ARMCPU_ARM7] & MMU.reg_IF[ARMCPU_ARM7]) 
					{
						NDS_ARM7.wIRQ = TRUE;
						NDS_ARM7.waitIRQ = FALSE;
					}
				}
#endif
				return;
			}
				
			case REG_IE :
				MMU.reg_IE[ARMCPU_ARM7] = val;
#ifndef NEW_IRQ
				if ( MMU.reg_IME[ARMCPU_ARM7])
				{
					/* raise an interrupt request to the CPU if needed */
					if ( MMU.reg_IE[ARMCPU_ARM7] & MMU.reg_IF[ARMCPU_ARM7]) 
					{
						NDS_ARM7.wIRQ = TRUE;
						NDS_ARM7.waitIRQ = FALSE;
					}
				}
#endif
				return;
			
			case REG_IF :
				MMU.reg_IF[ARMCPU_ARM7] &= (~val); 
				return;

            case REG_TM0CNTL:
            case REG_TM1CNTL:
            case REG_TM2CNTL:
            case REG_TM3CNTL:
			{
				int timerIndex = (adr>>2)&0x3;
				int mask = ((val & 0x800000)>>(16+7)) << (timerIndex+(ARMCPU_ARM7<<2));
				MMU.CheckTimers = (MMU.CheckTimers & (~mask)) | mask;

				MMU.timerReload[ARMCPU_ARM7][timerIndex] = (u16)val;
				if(val&0x800000)
					MMU.timer[ARMCPU_ARM7][timerIndex] = MMU.timerReload[ARMCPU_ARM7][(adr>>2)&0x3];

				MMU.timerON[ARMCPU_ARM7][timerIndex] = val & 0x800000;
				switch((val>>16)&7)
				{
					case 0 :
					MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 0+1;//ARMCPU_ARM7;
					break;
					case 1 :
					MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 6+1;//ARMCPU_ARM7;
					break;
					case 2 :
					MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 8+1;//ARMCPU_ARM7;
					break;
					case 3 :
					MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 10+1;//ARMCPU_ARM7;
					break;
					default :
					MMU.timerMODE[ARMCPU_ARM7][timerIndex] = 0xFFFF;
					break;
				}
				if(!(val & 0x800000))
					MMU.timerRUN[ARMCPU_ARM7][timerIndex] = FALSE;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], adr & 0xFFF, val);
				return;
			}

			case REG_IPCSYNC :
					MMU_IPCSync(ARMCPU_ARM7, val);
				return;

			case REG_IPCFIFOSEND :
					IPC_FIFOsend(ARMCPU_ARM7, val);
				return;
			case REG_DMA0CNTL :
				//LOG("32 bit dma0 %04X\r\n", val);
				DMASrc[ARMCPU_ARM7][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xB0);
				DMADst[ARMCPU_ARM7][0] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xB4);
				MMU.DMAStartTime[ARMCPU_ARM7][0] = (val>>28) & 0x3;
				MMU.DMACrt[ARMCPU_ARM7][0] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xB8, val);
				if( MMU.DMAStartTime[ARMCPU_ARM7][0] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM7][0] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM7>(0);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM7, 0, DMASrc[ARMCPU_ARM7][0], DMADst[ARMCPU_ARM7][0], 0, ((MMU.DMACrt[ARMCPU_ARM7][0]>>27)&7));
				}
				#endif
				//emu_halt();
				return;
			case REG_DMA1CNTL:
				//LOG("32 bit dma1 %04X\r\n", val);
				DMASrc[ARMCPU_ARM7][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xBC);
				DMADst[ARMCPU_ARM7][1] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC0);
				MMU.DMAStartTime[ARMCPU_ARM7][1] = (val>>28) & 0x3;
				MMU.DMACrt[ARMCPU_ARM7][1] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC4, val);
				if(MMU.DMAStartTime[ARMCPU_ARM7][1] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM7][1] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM7>(1);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM7, 1, DMASrc[ARMCPU_ARM7][1], DMADst[ARMCPU_ARM7][1], 0, ((MMU.DMACrt[ARMCPU_ARM7][1]>>27)&7));
				}
				#endif
				return;
			case REG_DMA2CNTL :
				//LOG("32 bit dma2 %04X\r\n", val);
				DMASrc[ARMCPU_ARM7][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xC8);
				DMADst[ARMCPU_ARM7][2] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xCC);
				MMU.DMAStartTime[ARMCPU_ARM7][2] = (val>>28) & 0x3;
				MMU.DMACrt[ARMCPU_ARM7][2] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD0, val);
				if(MMU.DMAStartTime[ARMCPU_ARM7][2] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM7][2] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM7>(2);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM7, 2, DMASrc[ARMCPU_ARM7][2], DMADst[ARMCPU_ARM7][2], 0, ((MMU.DMACrt[ARMCPU_ARM7][2]>>27)&7));
				}
				#endif
				return;
			case REG_DMA3CNTL :
				//LOG("32 bit dma3 %04X\r\n", val);
				DMASrc[ARMCPU_ARM7][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD4);
				DMADst[ARMCPU_ARM7][3] = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xD8);
				MMU.DMAStartTime[ARMCPU_ARM7][3] = (val>>28) & 0x3;
				MMU.DMACrt[ARMCPU_ARM7][3] = val;
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0xDC, val);
				if(	MMU.DMAStartTime[ARMCPU_ARM7][3] == 0 ||
					MMU.DMAStartTime[ARMCPU_ARM7][3] == 7)		// Start Immediately
					MMU_doDMA<ARMCPU_ARM7>(3);
				#ifdef LOG_DMA2
				else
				{
					LOG("ARMCPU_ARM7 %d, dma %d src %08X dst %08X start taille %d %d\r\n", ARMCPU_ARM7, 3, DMASrc[ARMCPU_ARM7][3], DMADst[ARMCPU_ARM7][3], 0, ((MMU.DMACrt[ARMCPU_ARM7][3]>>27)&7));
				}
				#endif
				return;
			case REG_GCROMCTRL :
				{
					if(!(val & 0x80000000))
					{
						MMU.dscard[ARMCPU_ARM7].address = 0;
						MMU.dscard[ARMCPU_ARM7].transfer_count = 0;

						val &= 0x7F7FFFFF;
						T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x1A4, val);
						return;
					}

                    switch(MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT))
					{
						/* Dummy */
						case 0x9F:
							{
								MMU.dscard[ARMCPU_ARM7].address = 0;
								MMU.dscard[ARMCPU_ARM7].transfer_count = 0x800;
							}
							break;

						/* Data read */
						case 0x00:
						case 0xB7:
							{
								MMU.dscard[ARMCPU_ARM7].address = (MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT+1) << 24) | (MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT+2) << 16) | (MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT+3) << 8) | (MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT+4));
								MMU.dscard[ARMCPU_ARM7].transfer_count = 0x80;
							}
							break;

						/* Get ROM chip ID */
						case 0x90:
						case 0xB8:
							{
								MMU.dscard[ARMCPU_ARM7].address = 0;
								MMU.dscard[ARMCPU_ARM7].transfer_count = 1;
							}
							break;

						default:
							{
								LOG("CARD command: %02X\n", MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT));
								MMU.dscard[ARMCPU_ARM7].address = 0;
								MMU.dscard[ARMCPU_ARM7].transfer_count = 0;
							}
							break;
					}

					if(MMU.dscard[ARMCPU_ARM7].transfer_count == 0)
					{
						val &= 0x7F7FFFFF;
						T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x1A4, val);
						return;
					}
					
                    val |= 0x00800000;
                    T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x1A4, val);

					/* launch DMA if start flag was set to "DS Cart" */

					if(MMU.DMAStartTime[ARMCPU_ARM7][2] == 2)
					{
						MMU_doDMA<ARMCPU_ARM7>(2);
					}
					if(MMU.DMAStartTime[ARMCPU_ARM7][3] == 2)
					{
						MMU_doDMA<ARMCPU_ARM7>(3);
					}

					return;

				}
				return;
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM7(adr, "(write32) %0x%X", val);
#endif
		T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], adr & MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
		return;
	}

	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [shash]
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][adr>>20], adr&MMU.MMU_MASK[ARMCPU_ARM7][adr>>20], val);
}

//================================================= MMU ARM7 read 08
u8 FASTCALL _MMU_ARM7_read08(u32 adr)
{
#ifdef EXPERIMENTAL_WIFI
	/* wifi mac access */
	if ((adr>=0x04800000)&&(adr<0x05000000))
	{
		if (adr & 1)
			return (WIFI_read16(&wifiMac,adr-1) >> 8) & 0xFF;
		else
			return WIFI_read16(&wifiMac,adr) & 0xFF;
	}
#endif

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read08(adr);
#else
	// CFlash reading, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000))
		return (unsigned char)cflash_read(adr);
#endif

	if (adr == REG_RTC) return (u8)rtcRead();

#ifdef _MMU_DEBUG
	mmu_log_debug_ARM7(adr, "(read08) %0x%X", 
		MMU.MMU_MEM[ARMCPU_ARM7][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM7][(adr>>20)&0xFF]]);
#endif

    return MMU.MMU_MEM[ARMCPU_ARM7][(adr>>20)&0xFF][adr&MMU.MMU_MASK[ARMCPU_ARM7][(adr>>20)&0xFF]];
}
//================================================= MMU ARM7 read 16
u16 FASTCALL _MMU_ARM7_read16(u32 adr)
{
#ifdef EXPERIMENTAL_WIFI
	/* wifi mac access */
	if ((adr>=0x04800000)&&(adr<0x05000000))
		return WIFI_read16(&wifiMac,adr) ;
#endif

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read16(adr);
#else
	// CFlash reading, Mic
	if ((adr>=0x08800000)&&(adr<0x09900000))
	   return (unsigned short)cflash_read(adr);
#endif

	adr &= 0x0FFFFFFF;

	if(adr>>24==4)
	{
		/* Address is an IO register */
		switch(adr)
		{
			case REG_RTC:
				return rtcRead();

			case REG_IME :
				return (u16)MMU.reg_IME[ARMCPU_ARM7];
				
			case REG_IE :
				return (u16)MMU.reg_IE[ARMCPU_ARM7];
			case REG_IE + 2 :
				return (u16)(MMU.reg_IE[ARMCPU_ARM7]>>16);
				
			case REG_IF :
				return (u16)MMU.reg_IF[ARMCPU_ARM7];
			case REG_IF + 2 :
				return (u16)(MMU.reg_IF[ARMCPU_ARM7]>>16);

			case REG_TM0CNTL :
			case REG_TM1CNTL :
			case REG_TM2CNTL :
			case REG_TM3CNTL :
				return MMU.timer[ARMCPU_ARM7][(adr&0xF)>>2];


			case 0x04000130:
			case 0x04000136:
				//here is an example of what not to do:
				//since the arm7 polls this every frame, we shouldnt count this as an input check
				//LagFrameFlag=0;
				break;
			
			case REG_POSTFLG :
				return 1;
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM7(adr, "(read16) %0x%X", 
			T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]));
#endif
		return T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]); 
	}

	/* Returns data from memory */
	return T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]); 
}
//================================================= MMU ARM7 read 32
u32 FASTCALL _MMU_ARM7_read32(u32 adr)
{
#ifdef EXPERIMENTAL_WIFI
	/* wifi mac access */
	if ((adr>=0x04800000)&&(adr<0x05000000))
		return (WIFI_read16(&wifiMac,adr) | (WIFI_read16(&wifiMac,adr+2) << 16));
#endif

#ifdef EXPERIMENTAL_GBASLOT
	if ( (adr >= 0x08000000) && (adr < 0x0A010000) )
		return addon.read32(adr);
#else
	// CFlash reading, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000))
	   return (unsigned long)cflash_read(adr);
#endif

	adr &= 0x0FFFFFFF;

	if((adr >> 24) == 4)
	{
		/* Address is an IO register */
		switch(adr)
		{
			case REG_RTC:
				return (u32)rtcRead();

			case REG_IME : 
				return MMU.reg_IME[ARMCPU_ARM7];
			case REG_IE :
				return MMU.reg_IE[ARMCPU_ARM7];
			case REG_IF :
				return MMU.reg_IF[ARMCPU_ARM7];
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
			{
                u32 val = 0;

				if(MMU.dscard[ARMCPU_ARM7].transfer_count == 0)
					return 0;

				switch(MEM_8(MMU.MMU_MEM[ARMCPU_ARM7], REG_GCCMDOUT))
				{
					/* Dummy */
					case 0x9F:
						{
							val = 0xFFFFFFFF;
						}
						break;

					/* Data read */
					case 0x00:
					case 0xB7:
						{
							/* TODO: prevent read if the address is out of range */
							val = T1ReadLong(MMU.CART_ROM, MMU.dscard[ARMCPU_ARM7].address);
						}
						break;

					/* Get ROM chip ID */
					case 0x90:
					case 0xB8:
						{
							/* TODO */
							val = 0x00000000;
						}
						break;
				}

				MMU.dscard[ARMCPU_ARM7].address += 4;	// increment address

				MMU.dscard[ARMCPU_ARM7].transfer_count--;	// update transfer counter
				if(MMU.dscard[ARMCPU_ARM7].transfer_count) // if transfer is not ended
					return val;	// return data

				// transfer is done
				T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x1A4, 
					T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x1A4) & 0x7F7FFFFF);
	
				// if needed, throw irq for the end of transfer
				if(T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][0x40], 0x1A0) & 0x4000)
					NDS_makeInt(ARMCPU_ARM7, 19);
	
				return val;
			}
		}
#ifdef _MMU_DEBUG
		mmu_log_debug_ARM7(adr, "(read32) %0x%X", 
			T1ReadWord(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20) & 0xFF]));
#endif
		return T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20)], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20)]);
	}

	//Returns data from memory
	// Removed the &0xFF as they are implicit with the adr&0x0FFFFFFFF [zeromus, inspired by shash]
	return T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM7][(adr >> 20)], adr & MMU.MMU_MASK[ARMCPU_ARM7][(adr >> 20)]);
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

void mmu_select_savetype(int type, int *bmemtype, u32 *bmemsize) {
        if (type<0 || type > 6) return;
        *bmemtype=save_types[type][0];
        *bmemsize=save_types[type][1];
        mc_realloc(&MMU.bupmem, *bmemtype, *bmemsize);
}
