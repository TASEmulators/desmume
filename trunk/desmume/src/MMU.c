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

//#define RENDER3D

#include <stdlib.h>
#include <math.h>
#include <string.h>

#include "debug.h"
#include "NDSSystem.h"
#include "cflash.h"
#include "cp15.h"

#include "registers.h"

#define ROM_MASK 3

//#define LOG_CARD
//#define LOG_GPU
//#define LOG_DMA
//#define LOG_DMA2
//#define LOG_DIV

char szRomPath[512];
char szRomBaseName[512];

MMU_struct MMU;

u8 * MMU_ARM9_MEM_MAP[256]={
	//0X
	ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, 
	//1X
	//ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, ARM9Mem.ARM9_ITCM, 
	ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, ARM9Mem.ARM9_WRAM, 
	//2X
	ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM,
	//3x
	MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, 
	//4X
	ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, ARM9Mem.ARM9_REG, 
	//5X
	ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, ARM9Mem.ARM9_VMEM, 
	//60 61
	ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, 
	//62 63
	ARM9Mem.ARM9_BBG, ARM9Mem.ARM9_BBG,
	//64 65
	ARM9Mem.ARM9_AOBJ, ARM9Mem.ARM9_AOBJ,
	//66 67
	ARM9Mem.ARM9_BOBJ, ARM9Mem.ARM9_BOBJ,
	//68 69 ..
	ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD, ARM9Mem.ARM9_LCD,
	//7X
	ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM, ARM9Mem.ARM9_OAM,
	//8X
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	//9X
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	//AX
	MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, 
	//BX
	//CX
	//DX
	//EX
	//FX
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS, ARM9Mem.ARM9_BIOS,
};
	   
u32 MMU_ARM9_MEM_MASK[256]={
	//0X
	0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
	//1X
	0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
	//0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
	//2x
	0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF,
	//3X
	0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
	//4X
	0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF,
	//5X
	0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 
	//60 61
	0x7FFFF, 0x7FFFF, 
	//62 63 
	0x1FFFF, 0x1FFFF,
	//64 65
	0x3FFFF, 0x3FFFF,
	//66 67
	0x1FFFF, 0x1FFFF,
	//68 69 ....
	0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF, 0xFFFFF, 
	//7X		 
	0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 0x7FF, 
	//8X
	ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK,
	//9x
	ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK,
	//AX
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	//BX
	//CX
	//DX
	//EX
	//FX
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x0003, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF,
};

u8 * MMU_ARM7_MEM_MAP[256]={
	//0X
	MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, MMU.ARM7_BIOS, 
	//1X
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	//2X
	ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, ARM9Mem.MAIN_MEM, 
	//30 - 37
	MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, MMU.SWIRAM, 
	//38 - 3F
	MMU.ARM7_ERAM, MMU.ARM7_ERAM, MMU.ARM7_ERAM, MMU.ARM7_ERAM, MMU.ARM7_ERAM, MMU.ARM7_ERAM, MMU.ARM7_ERAM, MMU.ARM7_ERAM,
	//40 - 47
	MMU.ARM7_REG, MMU.ARM7_REG, MMU.ARM7_REG, MMU.ARM7_REG, MMU.ARM7_REG, MMU.ARM7_REG, MMU.ARM7_REG, MMU.ARM7_REG,
	//48 - 4F
	MMU.ARM7_WIRAM, MMU.ARM7_WIRAM, MMU.ARM7_WIRAM, MMU.ARM7_WIRAM, MMU.ARM7_WIRAM, MMU.ARM7_WIRAM, MMU.ARM7_WIRAM, MMU.ARM7_WIRAM,
	//5X
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	//6X
	ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG, ARM9Mem.ARM9_ABG,
	//7X
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	//8X
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	//9X
        NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 
	//AX
	MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, MMU.CART_RAM, 
	//BX
	//CX
	//DX
	//EX
	//FX
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, 
	MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM, MMU.UNUSED_RAM,
};

u32 MMU_ARM7_MEM_MASK[256]={
	//0X
	0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 0x3FFF, 
	//1X
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	//2x
	0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF, 0x3FFFFF,
	//30 - 37
	0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 0x7FFF, 
	//38 - 3F
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	//40 - 47
	0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 0xFFFFFF, 
	//48 - 4F
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 
	//5X
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	//6X
	0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF, 0x3FFFF,
	//7X
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	//8X
	ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK,
	//9x
	ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK, ROM_MASK,
	//AX
	0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
	//BX
	//CX
	//DX
	//EX
	//FX
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
	0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003, 0x0003,
};

u32 MMU_ARM9_WAIT16[16]={
	1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1,
};

u32 MMU_ARM9_WAIT32[16]={
	1, 1, 1, 1, 1, 2, 2, 1, 8, 8, 5, 1, 1, 1, 1, 1,
};

u32 MMU_ARM7_WAIT16[16]={
	1, 1, 1, 1, 1, 1, 1, 1, 5, 5, 5, 1, 1, 1, 1, 1,
};

u32 MMU_ARM7_WAIT32[16]={
	1, 1, 1, 1, 1, 1, 1, 1, 8, 8, 5, 1, 1, 1, 1, 1,
};

void MMU_Init(void) {
	int i;

	LOG("MMU init\n");

	memset(&MMU, 0, sizeof(MMU_struct));

	MMU.CART_ROM = MMU.UNUSED_RAM;

        for(i = 0x80; i<0xA0; ++i)
        {
           MMU_ARM9_MEM_MAP[i] = MMU.CART_ROM;
           MMU_ARM7_MEM_MAP[i] = MMU.CART_ROM;
        }

	MMU.MMU_MEM[0] = MMU_ARM9_MEM_MAP;
	MMU.MMU_MEM[1] = MMU_ARM7_MEM_MAP;
	MMU.MMU_MASK[0]= MMU_ARM9_MEM_MASK;
	MMU.MMU_MASK[1] = MMU_ARM7_MEM_MASK;

	MMU.ITCMRegion = 0x00800000;

	MMU.MMU_WAIT16[0] = MMU_ARM9_WAIT16;
	MMU.MMU_WAIT16[1] = MMU_ARM7_WAIT16;
	MMU.MMU_WAIT32[0] = MMU_ARM9_WAIT32;
	MMU.MMU_WAIT32[1] = MMU_ARM7_WAIT32;

	for(i = 0;i < 16;i++)
		FIFOInit(MMU.fifos + i);
	
        mc_init(&MMU.fw, MC_TYPE_FLASH);  /* init fw device */
        mc_alloc(&MMU.fw, NDS_FW_SIZE_V1);
        MMU.fw.fp = NULL;

        // Init Backup Memory device, this should really be done when the rom is loaded
        mc_init(&MMU.bupmem, MC_TYPE_AUTODETECT);
        mc_alloc(&MMU.bupmem, 1);
        MMU.bupmem.fp = NULL;
} 

void MMU_DeInit(void) {
	LOG("MMU deinit\n");
    if (MMU.fw.fp)
       fclose(MMU.fw.fp);
    mc_free(&MMU.fw);      
    if (MMU.bupmem.fp)
       fclose(MMU.bupmem.fp);
    mc_free(&MMU.bupmem);
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
     int i;

     memset(ARM9Mem.ARM9_ABG, 0, 0x80000);
     memset(ARM9Mem.ARM9_AOBJ, 0, 0x40000);
     memset(ARM9Mem.ARM9_BBG, 0, 0x20000);
     memset(ARM9Mem.ARM9_BOBJ, 0, 0x20000);
     memset(ARM9Mem.ARM9_DTCM, 0, 0x4000);
     memset(ARM9Mem.ARM9_ITCM, 0, 0x8000);
     memset(ARM9Mem.ARM9_LCD, 0, 0xA4000);
     memset(ARM9Mem.ARM9_OAM, 0, 0x800);
     memset(ARM9Mem.ARM9_REG, 0, 0x1000000);
     memset(ARM9Mem.ARM9_VMEM, 0, 0x800);
     memset(ARM9Mem.ARM9_WRAM, 0, 0x1000000);
     memset(ARM9Mem.MAIN_MEM, 0, 0x400000);
     
     memset(MMU.ARM7_ERAM, 0, 0x10000);
     memset(MMU.ARM7_REG, 0, 0x10000);

     for(i = 0;i < 16;i++)
        FIFOInit(MMU.fifos + i);

     MMU.DTCMRegion = 0;
     MMU.ITCMRegion = 0x00800000;
        
     memset(MMU.timer, 0, sizeof(u16) * 2 * 4);
     memset(MMU.timerMODE, 0, sizeof(s32) * 2 * 4);
     memset(MMU.timerON, 0, sizeof(u32) * 2 * 4);
     memset(MMU.timerRUN, 0, sizeof(u32) * 2 * 4);
     memset(MMU.timerReload, 0, sizeof(u16) * 2 * 4);
        
     memset(MMU.reg_IME, 0, sizeof(u32) * 2);
     memset(MMU.reg_IE, 0, sizeof(u32) * 2);
     memset(MMU.reg_IF, 0, sizeof(u32) * 2);
        
     memset(MMU.DMAStartTime, 0, sizeof(u32) * 2 * 4);
     memset(MMU.DMACycle, 0, sizeof(s32) * 2 * 4);
     memset(MMU.DMACrt, 0, sizeof(u32) * 2 * 4);
     memset(MMU.DMAing, 0, sizeof(BOOL) * 2 * 4);
		  
     memset(MMU.dscard, 0, sizeof(nds_dscard) * 2);

     MainScreen.offset = 192;
     SubScreen.offset = 0;
}

void MMU_setRom(u8 * rom, u32 mask)
{
    unsigned int i;
    MMU.CART_ROM = rom;
    
    for(i = 0x80; i<0xA0; ++i)
    {
	MMU_ARM9_MEM_MAP[i] = rom;
	MMU_ARM7_MEM_MAP[i] = rom;
	MMU_ARM9_MEM_MASK[i] = mask;
	MMU_ARM7_MEM_MASK[i] = mask;
    }
    rom_mask = mask;
}

void MMU_unsetRom()
{
    unsigned int i;
    MMU.CART_ROM=MMU.UNUSED_RAM;

    for(i = 0x80; i<0xA0; ++i)
    {
	MMU_ARM9_MEM_MAP[i] = MMU.UNUSED_RAM;
	MMU_ARM7_MEM_MAP[i] = MMU.UNUSED_RAM;
	MMU_ARM9_MEM_MASK[i] = ROM_MASK;
	MMU_ARM7_MEM_MASK[i] = ROM_MASK;
    }
    rom_mask = ROM_MASK;
}
char txt[80];	

u8 FASTCALL MMU_read8(u32 proc, u32 adr)
{
	if((proc==ARMCPU_ARM9)&((adr&(~0x3FFF))==MMU.DTCMRegion))
	{
		return ARM9Mem.ARM9_DTCM[adr&0x3FFF];
	}
	
	// CFlash reading, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000))
	   return (unsigned char)cflash_read(adr);

	adr &= 0x0FFFFFFF;

	switch(adr)
	{
		case 0x027FFCDC :
			return 0x20;
		case 0x027FFCDD :
			return 0x20;
		case 0x027FFCE2 :
		 //execute = FALSE;
			return 0xE0;
		case 0x027FFCE3 :
			return 0x80;
                /*case REG_POSTFLG :
			return 1;
                case REG_IME :
			execute = FALSE;*/
		default :
		    return MMU.MMU_MEM[proc][(adr>>20)&0xFF][adr&MMU.MMU_MASK[proc][(adr>>20)&0xFF]];
	}
}



u16 FASTCALL MMU_read16(u32 proc, u32 adr)
{    
	if((proc == ARMCPU_ARM9) && ((adr & ~0x3FFF) == MMU.DTCMRegion))
	{
		/* Returns data from DTCM (ARM9 only) */
		return T1ReadWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
	}
	
	// CFlash reading, Mic
	if ((adr>=0x08800000)&&(adr<0x09900000))
	   return (unsigned short)cflash_read(adr);
	
	adr &= 0x0FFFFFFF;

	if((adr>>24)==4)
	{
		/* Adress is an IO register */
		switch(adr)
		{
                        case REG_IPCFIFORECV :               /* TODO (clear): ??? */
				execute = FALSE;
				return 1;
				
			case REG_IME :
				return (u16)MMU.reg_IME[proc];
				
			case REG_IE :
				return (u16)MMU.reg_IE[proc];
			case REG_IE + 2 :
				return (u16)(MMU.reg_IE[proc]>>16);
				
			case REG_IF :
				return (u16)MMU.reg_IF[proc];
			case REG_IF + 2 :
				return (u16)(MMU.reg_IF[proc]>>16);
				
                        case REG_TM0CNTL :
                        case REG_TM1CNTL :
                        case REG_TM2CNTL :
                        case REG_TM3CNTL :
				return MMU.timer[proc][(adr&0xF)>>2];
			
			case 0x04000630 :
				LOG("vect res\r\n");	/* TODO (clear): ??? */
				//execute = FALSE;
				return 0;
			//case 0x27FFFAA :
			//case 0x27FFFAC :
			/*case 0x2330F84 :
				if(click) execute = FALSE;*/
			//case 0x27FF018 : execute = FALSE;
                        case REG_POSTFLG :
				return 1;
			default :
				break;
		}
	}
	else
	{
		/* TODO (clear): i don't known what are these 'registers', perhaps reset vectors ... */
		switch(adr)
		{
			case 0x027FFCD8 :
				return (0x20<<4);
			case 0x027FFCDA :
				return (0x20<<4);
			case 0x027FFCDE : 
				return (0xE0<<4);
			case 0x027FFCE0 :
			//execute = FALSE;
				return (0x80<<4);
			default :
				break;
		}
	}
	
	/* Returns data from memory */
	return T1ReadWord(MMU.MMU_MEM[proc][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[proc][(adr >> 20) & 0xFF]); 
}
	 
u32 FASTCALL MMU_read32(u32 proc, u32 adr)
{
	if((proc == ARMCPU_ARM9) && ((adr & ~0x3FFF) == MMU.DTCMRegion))
	{
		/* Returns data from DTCM (ARM9 only) */
		return T1ReadLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF);
	}
	
	// CFlash reading, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000))
	   return (unsigned long)cflash_read(adr);
		
	adr &= 0x0FFFFFFF;

	if((adr >> 24) == 4)
	{
		/* Adress is an IO register */
		switch(adr)
		{
			#ifdef RENDER3D
			case 0x04000600 :
				return ((OGLRender::nbpush[0]&1)<<13) | ((OGLRender::nbpush[2]&0x1F)<<8);
			#endif
			
			case REG_IME :
				return MMU.reg_IME[proc];
			case REG_IE :
				return MMU.reg_IE[proc];
			case REG_IF :
				return MMU.reg_IF[proc];
				
                        case REG_IPCFIFORECV :
			{
				u16 IPCFIFO_CNT = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
				if(IPCFIFO_CNT&0x8000)
				{
				//execute = FALSE;
				u32 fifonum = IPCFIFO+proc;
				u32 val = FIFOValue(MMU.fifos + fifonum);
				u32 remote = (proc+1) & 1;
				u16 IPCFIFO_CNT_remote = T1ReadWord(MMU.MMU_MEM[remote][0x40], 0x184);
				IPCFIFO_CNT |= (MMU.fifos[fifonum].empty<<8) | (MMU.fifos[fifonum].full<<9) | (MMU.fifos[fifonum].error<<14);
				IPCFIFO_CNT_remote |= (MMU.fifos[fifonum].empty) | (MMU.fifos[fifonum].full<<1);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, IPCFIFO_CNT);
				T1WriteWord(MMU.MMU_MEM[remote][0x40], 0x184, IPCFIFO_CNT_remote);
				if(MMU.fifos[fifonum].empty)
					MMU.reg_IF[proc] |=  ((IPCFIFO_CNT & (1<<2))<<15);// & (MMU.reg_IME[proc]<<17);// & (MMU.reg_IE[proc] & (1<<17));// 
				return val;
				}
			}
			return 0;
                        case REG_TM0CNTL :
                        case REG_TM1CNTL :
                        case REG_TM2CNTL :
                        case REG_TM3CNTL :
			{
				u32 val = T1ReadWord(MMU.MMU_MEM[proc][0x40], (adr + 2) & 0xFFF);
				return MMU.timer[proc][(adr&0xF)>>2] | (val<<16);
			}	
			case 0x04000640 :	/* TODO (clear): again, ??? */
				LOG("read proj\r\n");
			return 0;
			case 0x04000680 :
				LOG("read roat\r\n");
			return 0;
			case 0x04000620 :
				LOG("point res\r\n");
			return 0;
			
                        case REG_GCDATAIN:
			{
                                u32 val;

                                if(!MMU.dscard[proc].adress) return 0;
				
                                val = T1ReadLong(MMU.CART_ROM, MMU.dscard[proc].adress);

				MMU.dscard[proc].adress += 4;	/* increment adress */
				
				MMU.dscard[proc].transfer_count--;	/* update transfer counter */
				if(MMU.dscard[proc].transfer_count) /* if transfer is not ended */
				{
					return val;	/* return data */
				}
				else	/* transfer is done */
                                {                                                       
                                        T1WriteLong(MMU.MMU_MEM[proc][(REG_GCROMCTRL >> 20) & 0xff], REG_GCROMCTRL & 0xfff, T1ReadLong(MMU.MMU_MEM[proc][(REG_GCROMCTRL >> 20) & 0xff], REG_GCROMCTRL & 0xfff) & ~(0x00800000 | 0x80000000));
					/* = 0x7f7fffff */
					
					/* if needed, throw irq for the end of transfer */
                                        if(T1ReadWord(MMU.MMU_MEM[proc][(REG_AUXSPICNT >> 20) & 0xff], REG_AUXSPICNT & 0xfff) & 0x4000)
					{
                                                if(proc == ARMCPU_ARM7) NDS_makeARM7Int(19); 
                                                else NDS_makeARM9Int(19);
					}
					
					return val;
				}
			}

			default :
				break;
		}
	}
	
	/* Returns data from memory */
	return T1ReadLong(MMU.MMU_MEM[proc][(adr >> 20) & 0xFF], adr & MMU.MMU_MASK[proc][(adr >> 20) & 0xFF]);
}
	
void FASTCALL MMU_write8(u32 proc, u32 adr, u8 val)
{
	if((proc == ARMCPU_ARM9) && ((adr & ~0x3FFF) == MMU.DTCMRegion))
	{
		/* Writes data in DTCM (ARM9 only) */
		ARM9Mem.ARM9_DTCM[adr&0x3FFF] = val;
		return ;
	}
	
	// CFlash writing, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000)) {
		cflash_write(adr,val);
		return;
	}

	adr &= 0x0FFFFFFF;

        // This is bad, remove it
        if(proc == ARMCPU_ARM7)
        {
           if ((adr>=0x04000400)&&(adr<0x0400051D))
           {
              SPU_WriteByte(adr, val);
              return;
           }
        }

	switch(adr)
	{
		/* TODO: EEEK ! Controls for VRAMs A, B, C, D are missing ! */
		/* TODO: Not all mappings of VRAMs are handled... (especially BG and OBJ modes) */
                case REG_VRAMCNTE :
			if(proc == ARMCPU_ARM9)
			{
                                if((val & 7) == 5)
				{
					ARM9Mem.ExtPal[0][0] = ARM9Mem.ARM9_LCD + 0x80000;
					ARM9Mem.ExtPal[0][1] = ARM9Mem.ARM9_LCD + 0x82000;
					ARM9Mem.ExtPal[0][2] = ARM9Mem.ARM9_LCD + 0x84000;
					ARM9Mem.ExtPal[0][3] = ARM9Mem.ARM9_LCD + 0x86000;
				}
                                else if((val & 7) == 3)
				{
					ARM9Mem.texPalSlot[0] = ARM9Mem.ARM9_LCD + 0x80000;
					ARM9Mem.texPalSlot[1] = ARM9Mem.ARM9_LCD + 0x82000;
					ARM9Mem.texPalSlot[2] = ARM9Mem.ARM9_LCD + 0x84000;
					ARM9Mem.texPalSlot[3] = ARM9Mem.ARM9_LCD + 0x86000;
				}
                                else if((val & 7) == 4)
				{
					ARM9Mem.ExtPal[0][0] = ARM9Mem.ARM9_LCD + 0x80000;
					ARM9Mem.ExtPal[0][1] = ARM9Mem.ARM9_LCD + 0x82000;
					ARM9Mem.ExtPal[0][2] = ARM9Mem.ARM9_LCD + 0x84000;
					ARM9Mem.ExtPal[0][3] = ARM9Mem.ARM9_LCD + 0x86000;
				}
			}
			break;
		
                case REG_VRAMCNTF :
			if(proc == ARMCPU_ARM9)
			{
				switch(val & 0x1F)
				{
                                        case 4 :
						ARM9Mem.ExtPal[0][0] = ARM9Mem.ARM9_LCD + 0x90000;
						ARM9Mem.ExtPal[0][1] = ARM9Mem.ARM9_LCD + 0x92000;
						break;
						
                                        case 4 | (1 << 3) :
						ARM9Mem.ExtPal[0][2] = ARM9Mem.ARM9_LCD + 0x90000;
						ARM9Mem.ExtPal[0][3] = ARM9Mem.ARM9_LCD + 0x92000;
						break;
						
                                        case 3 :
						ARM9Mem.texPalSlot[0] = ARM9Mem.ARM9_LCD + 0x90000;
						break;
						
                                        case 3 | (1 << 3) :
						ARM9Mem.texPalSlot[1] = ARM9Mem.ARM9_LCD + 0x90000;
						break;
						
                                        case 3 | (2 << 3) :
						ARM9Mem.texPalSlot[2] = ARM9Mem.ARM9_LCD + 0x90000;
						break;
						
                                        case 3 | (3 << 3) :
						ARM9Mem.texPalSlot[3] = ARM9Mem.ARM9_LCD + 0x90000;
						break;
						
                                        case 5 :
                                        case 5 | (1 << 3) :
                                        case 5 | (2 << 3) :
                                        case 5 | (3 << 3) :
						ARM9Mem.ObjExtPal[0][0] = ARM9Mem.ARM9_LCD + 0x90000;
						ARM9Mem.ObjExtPal[0][1] = ARM9Mem.ARM9_LCD + 0x92000;
						break;
				}
		 	}
			break;
                case REG_VRAMCNTG :
			if(proc == ARMCPU_ARM9)
			{
		 		switch(val & 0x1F)
				{
                                        case 4 :
						ARM9Mem.ExtPal[0][0] = ARM9Mem.ARM9_LCD + 0x94000;
						ARM9Mem.ExtPal[0][1] = ARM9Mem.ARM9_LCD + 0x96000;
						break;
						
                                        case 4 | (1 << 3) :
						ARM9Mem.ExtPal[0][2] = ARM9Mem.ARM9_LCD + 0x94000;
						ARM9Mem.ExtPal[0][3] = ARM9Mem.ARM9_LCD + 0x96000;
						break;
						
                                        case 3 :
						ARM9Mem.texPalSlot[0] = ARM9Mem.ARM9_LCD + 0x94000;
						break;
						
                                        case 3 | (1 << 3) :
						ARM9Mem.texPalSlot[1] = ARM9Mem.ARM9_LCD + 0x94000;
						break;
						
                                        case 3 | (2 << 3) :
						ARM9Mem.texPalSlot[2] = ARM9Mem.ARM9_LCD + 0x94000;
						break;
						
                                        case 3 | (3 << 3) :
						ARM9Mem.texPalSlot[3] = ARM9Mem.ARM9_LCD + 0x94000;
						break;
						
                                        case 5 :
                                        case 5 | (1 << 3) :
                                        case 5 | (2 << 3) :
                                        case 5 | (3 << 3) :
						ARM9Mem.ObjExtPal[0][0] = ARM9Mem.ARM9_LCD + 0x94000;
						ARM9Mem.ObjExtPal[0][1] = ARM9Mem.ARM9_LCD + 0x96000;
						break;
				}
			}
			break;
			
                case REG_VRAMCNTH  :
			if(proc == ARMCPU_ARM9)
			{
                                if((val & 7) == 2)
				{
					ARM9Mem.ExtPal[1][0] = ARM9Mem.ARM9_LCD + 0x98000;
					ARM9Mem.ExtPal[1][1] = ARM9Mem.ARM9_LCD + 0x9A000;
					ARM9Mem.ExtPal[1][2] = ARM9Mem.ARM9_LCD + 0x9C000;
					ARM9Mem.ExtPal[1][3] = ARM9Mem.ARM9_LCD + 0x9E000;
				}
			}
			break;
			
                case REG_VRAMCNTI  :
			if(proc == ARMCPU_ARM9)
			{
                                if((val & 7) == 3)
				{
					ARM9Mem.ObjExtPal[1][0] = ARM9Mem.ARM9_LCD + 0xA0000;
					ARM9Mem.ObjExtPal[1][1] = ARM9Mem.ARM9_LCD + 0xA2000;
				}
			}
			break;
				case REG_DISPA_WIN0H:
                    GPU_setWINDOW_XDIM_Component(MainScreen.gpu,val,0) ;
					break ;
				case REG_DISPA_WIN0H+1:
                    GPU_setWINDOW_XDIM_Component(MainScreen.gpu,val,1) ;
					break ;
				case REG_DISPA_WIN1H:
                    GPU_setWINDOW_XDIM_Component(MainScreen.gpu,val,2) ;
					break ;
				case REG_DISPA_WIN1H+1:
                    GPU_setWINDOW_XDIM_Component(MainScreen.gpu,val,3) ;
					break ;
				case REG_DISPB_WIN0H:
                    GPU_setWINDOW_XDIM_Component(SubScreen.gpu,val,0) ;
					break ;
				case REG_DISPB_WIN0H+1:
                    GPU_setWINDOW_XDIM_Component(SubScreen.gpu,val,1) ;
					break ;
				case REG_DISPB_WIN1H:
                    GPU_setWINDOW_XDIM_Component(SubScreen.gpu,val,2) ;
					break ;
				case REG_DISPB_WIN1H+1:
                    GPU_setWINDOW_XDIM_Component(SubScreen.gpu,val,3) ;
					break ;
				case REG_DISPA_WIN0V:
                    GPU_setWINDOW_YDIM_Component(MainScreen.gpu,val,0) ;
					break ;
				case REG_DISPA_WIN0V+1:
                    GPU_setWINDOW_YDIM_Component(MainScreen.gpu,val,1) ;
					break ;
				case REG_DISPA_WIN1V:
                    GPU_setWINDOW_YDIM_Component(MainScreen.gpu,val,2) ;
					break ;
				case REG_DISPA_WIN1V+1:
                    GPU_setWINDOW_YDIM_Component(MainScreen.gpu,val,3) ;
					break ;
				case REG_DISPB_WIN0V:
                    GPU_setWINDOW_YDIM_Component(SubScreen.gpu,val,0) ;
					break ;
				case REG_DISPB_WIN0V+1:
                    GPU_setWINDOW_YDIM_Component(SubScreen.gpu,val,1) ;
					break ;
				case REG_DISPB_WIN1V:
                    GPU_setWINDOW_YDIM_Component(SubScreen.gpu,val,2) ;
					break ;
				case REG_DISPB_WIN1V+1:
                    GPU_setWINDOW_YDIM_Component(SubScreen.gpu,val,3) ;
					break ;
				case REG_DISPA_WININ:
					GPU_setWINDOW_INCNT_Component(MainScreen.gpu,val,0) ;
					break ;
				case REG_DISPA_WININ+1:
					GPU_setWINDOW_INCNT_Component(MainScreen.gpu,val,1) ;
					break ;
				case REG_DISPA_WINOUT:
					GPU_setWINDOW_OUTCNT_Component(MainScreen.gpu,val,0) ;
					break ;
				case REG_DISPA_WINOUT+1:
					GPU_setWINDOW_OUTCNT_Component(MainScreen.gpu,val,1) ;
					break ;
				case REG_DISPB_WININ:
					GPU_setWINDOW_INCNT_Component(SubScreen.gpu,val,0) ;
					break ;
				case REG_DISPB_WININ+1:
					GPU_setWINDOW_INCNT_Component(SubScreen.gpu,val,1) ;
					break ;
				case REG_DISPB_WINOUT:
					GPU_setWINDOW_OUTCNT_Component(SubScreen.gpu,val,0) ;
					break ;
				case REG_DISPB_WINOUT+1:
					GPU_setWINDOW_OUTCNT_Component(SubScreen.gpu,val,1) ;
					break ;
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
		
		default :
			break;
	}
	
	MMU.MMU_MEM[proc][(adr>>20)&0xFF][adr&MMU.MMU_MASK[proc][(adr>>20)&0xFF]]=val;
}

u16 partie = 1;

void FASTCALL MMU_write16(u32 proc, u32 adr, u16 val)
{
	if((proc == ARMCPU_ARM9) && ((adr & ~0x3FFF) == MMU.DTCMRegion))
	{
		/* Writes in DTCM (ARM9 only) */
		T1WriteWord(ARM9Mem.ARM9_DTCM, adr & 0x3FFF, val);
		return;
	}
	
	// CFlash writing, Mic
	if ((adr>=0x08800000)&&(adr<0x09900000))
	{
		cflash_write(adr,val);
		return;
	}

	adr &= 0x0FFFFFFF;

        // This is bad, remove it
        if(proc == ARMCPU_ARM7)
        {
           if ((adr>=0x04000400)&&(adr<0x0400051D))
           {
              SPU_WriteWord(adr, val);
              return;
           }
        }

	if((adr >> 24) == 4)
	{
		/* Adress is an IO register */
		switch(adr)
		{
			#ifdef RENDER3D
			case 0x04000610 :
				if(proc == ARMCPU_ARM9)
				{
					LOG("CUT DEPTH %08X\r\n", val);
				}
				return;
			case 0x04000340 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glAlphaFunc(val);
				}
				return;
                        case REG_DISPA_DISP3DCNT :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glControl(val);
				}
				return;
			case 0x04000354 :
				if(proc == ARMCPU_ARM9)
					OGLRender::glClearDepth(val);
				return;
			#endif
			
                        case REG_POWCNT1 :
				if(proc == ARMCPU_ARM9)
				{
					if(val & (1<<15))
					{
						LOG("Main core on top\n");
						MainScreen.offset = 0;
						SubScreen.offset = 192;
						//nds.swapScreen();
					}
					else
					{
						LOG("Main core on bottom (%04X)\n", val);
						MainScreen.offset = 192;
						SubScreen.offset = 0;
					}
				}
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x304, val);
				return;

                        case REG_AUXSPICNT:
                                T1WriteWord(MMU.MMU_MEM[proc][(REG_AUXSPICNT >> 20) & 0xff], REG_AUXSPICNT & 0xfff, val);
                                AUX_SPI_CNT = val;

                                if (val == 0)
                                   mc_reset_com(&MMU.bupmem);     /* reset backup memory device communication */
				return;
				
                        case REG_AUXSPIDATA:
                                if(val!=0)
                                {
                                   AUX_SPI_CMD = val & 0xFF;
                                }

                                T1WriteWord(MMU.MMU_MEM[proc][(REG_AUXSPIDATA >> 20) & 0xff], REG_AUXSPIDATA & 0xfff, bm_transfer(&MMU.bupmem, val));
				return;

			case REG_SPICNT :
				if(proc == ARMCPU_ARM7)
				{
					SPI_CNT = val;
					
                                        //MMU.fw.com == 0; /* reset fw device communication */
					
                                        mc_reset_com(&MMU.fw);     /* reset fw device communication */
                                }
				
				T1WriteWord(MMU.MMU_MEM[proc][(REG_SPICNT >> 20) & 0xff], REG_SPICNT & 0xfff, val);
				return;
				
			case REG_SPIDATA :
				if(proc==ARMCPU_ARM7)
				{
                                        u16 spicnt;

					if(val!=0)
					{
						SPI_CMD = val;
					}
			
                                        spicnt = T1ReadWord(MMU.MMU_MEM[proc][(REG_SPICNT >> 20) & 0xff], REG_SPICNT & 0xfff);
					
                                        switch((spicnt >> 8) & 0x3)
					{
                                                case 0 :
							break;
							
                                                case 1 : /* firmware memory device */
                                                        if(spicnt & 0x3 != 0)      /* check SPI baudrate (must be 4mhz) */
							{
								T1WriteWord(MMU.MMU_MEM[proc][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, 0);
								break;
							}
							T1WriteWord(MMU.MMU_MEM[proc][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, fw_transfer(&MMU.fw, val));
							return;
							
                                                case 2 :
							switch(SPI_CMD & 0x70)
							{
								case 0x00 :
									val = 0;
									break;
								case 0x10 :
									//execute = FALSE;
									if(SPI_CNT&(1<<11))
									{
										if(partie)
										{
											val = ((nds.touchY<<3)&0x7FF);
											partie = 0;
											//execute = FALSE;
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
				
				T1WriteWord(MMU.MMU_MEM[proc][(REG_SPIDATA >> 20) & 0xff], REG_SPIDATA & 0xfff, val);
				return;
				
				/* NOTICE: Perhaps we have to use gbatek-like reg names instead of libnds-like ones ...*/
				
                        case REG_DISPA_BG0HOFS :
				GPU_scrollX(MainScreen.gpu, 0, val);
				return;                             
                        case REG_DISPA_BG1HOFS :
				GPU_scrollX(MainScreen.gpu, 1, val);
				return;
                        case REG_DISPA_BG2HOFS :
				GPU_scrollX(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3HOFS :
				GPU_scrollX(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG0HOFS :
				GPU_scrollX(SubScreen.gpu, 0, val);
				return;
                        case REG_DISPB_BG1HOFS :
				GPU_scrollX(SubScreen.gpu, 1, val);
				return;
                        case REG_DISPB_BG2HOFS :
				GPU_scrollX(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG3HOFS :
				GPU_scrollX(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG0VOFS :
				GPU_scrollY(MainScreen.gpu, 0, val);
				return;
                        case REG_DISPA_BG1VOFS :
				GPU_scrollY(MainScreen.gpu, 1, val);
				return;
                        case REG_DISPA_BG2VOFS :
				GPU_scrollY(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3VOFS :
				GPU_scrollY(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG0VOFS :
				GPU_scrollY(SubScreen.gpu, 0, val);
				return;
                        case REG_DISPB_BG1VOFS :
				GPU_scrollY(SubScreen.gpu, 1, val);
				return;
                        case REG_DISPB_BG2VOFS :
				GPU_scrollY(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG3VOFS :
				GPU_scrollY(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG2PA :
				GPU_setPA(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2PB :
				GPU_setPB(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2PC :
				GPU_setPC(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2PD :
				GPU_setPD(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2PA :
				GPU_setPA(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2PB :
				GPU_setPB(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2PC :
				GPU_setPC(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2PD :
				GPU_setPD(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3PA :
				GPU_setPA(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3PB :
				GPU_setPB(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3PC :
				GPU_setPC(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3PD :
				GPU_setPD(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3PA :
				GPU_setPA(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3PB :
				GPU_setPB(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3PC :
				GPU_setPC(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3PD :
				GPU_setPD(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG2XL :
				GPU_setXL(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2XH :
				GPU_setXH(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2XL :
				GPU_setXL(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2XH :
				GPU_setXH(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3XL :
				GPU_setXL(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3XH :
				GPU_setXH(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3XL :
				GPU_setXL(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3XH :
				GPU_setXH(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG2YL :
				GPU_setYL(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2YH :
				GPU_setYH(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2YL :
				GPU_setYL(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2YH :
				GPU_setYH(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3YL :
				GPU_setYL(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3YH :
				GPU_setYH(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3YL :
				GPU_setYL(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3YH :
				GPU_setYH(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG0CNT :
				//GPULOG("MAIN BG0 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 0, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x8, val);
				return;
                        case REG_DISPA_BG1CNT :
				//GPULOG("MAIN BG1 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 1, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xA, val);
				return;
                        case REG_DISPA_BG2CNT :
				//GPULOG("MAIN BG2 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 2, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xC, val);
				return;
                        case REG_DISPA_BG3CNT :
				//GPULOG("MAIN BG3 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(MainScreen.gpu, 3, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xE, val);
				return;
                        case REG_DISPB_BG0CNT :
				//GPULOG("SUB BG0 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 0, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x1008, val);
				return;
                        case REG_DISPB_BG1CNT :
				//GPULOG("SUB BG1 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 1, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x100A, val);
				return;
                        case REG_DISPB_BG2CNT :
				//GPULOG("SUB BG2 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 2, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x100C, val);
				return;
                        case REG_DISPB_BG3CNT :
				//GPULOG("SUB BG3 SETPROP 16B %08X\r\n", val);
				GPU_setBGProp(SubScreen.gpu, 3, val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x100E, val);
				return;
			case REG_DISPA_BLDCNT:
				GPU_setBLDCNT(MainScreen.gpu,val) ;
				break ;
			case REG_DISPB_BLDCNT:
				GPU_setBLDCNT(SubScreen.gpu,val) ;
				break ;
			case REG_DISPA_BLDALPHA:
				GPU_setBLDALPHA(MainScreen.gpu,val) ;
				break ;
			case REG_DISPB_BLDALPHA:
				GPU_setBLDALPHA(SubScreen.gpu,val) ;
				break ;
			case REG_DISPA_BLDY:
				GPU_setBLDY(MainScreen.gpu,val) ;
				break ;
			case REG_DISPB_BLDY:
				GPU_setBLDY(SubScreen.gpu,val) ;
				break ;
			case REG_DISPA_MOSAIC:
				GPU_setMOSAIC(MainScreen.gpu,val) ;
				break ;
			case REG_DISPB_MOSAIC:
				GPU_setMOSAIC(SubScreen.gpu,val) ;
				break ;
			case REG_DISPA_MASTERBRIGHT:
				GPU_setMASTER_BRIGHT (MainScreen.gpu, val);
				break;
			case REG_DISPB_MASTERBRIGHT:
				GPU_setMASTER_BRIGHT (SubScreen.gpu, val);
				break;
			case REG_DISPA_WININ:
				GPU_setWINDOW_INCNT(MainScreen.gpu, val) ;
				break ;
			case REG_DISPA_WINOUT:
				GPU_setWINDOW_OUTCNT(MainScreen.gpu, val) ;
				break ;
			case REG_DISPB_WININ:
				GPU_setWINDOW_INCNT(SubScreen.gpu, val) ;
				break ;
			case REG_DISPB_WINOUT:
				GPU_setWINDOW_OUTCNT(SubScreen.gpu, val) ;
				break ;
			case REG_IME :
				MMU.reg_IME[proc] = val&1;
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x208, val);
				return;
				
			case REG_IE :
				MMU.reg_IE[proc] = (MMU.reg_IE[proc]&0xFFFF0000) | val;
				return;
			case REG_IE + 2 :
				execute = FALSE;
				MMU.reg_IE[proc] = (MMU.reg_IE[proc]&0xFFFF) | (((u32)val)<<16);
				return;
				
			case REG_IF :
				execute = FALSE;
				MMU.reg_IF[proc] &= (~((u32)val)); 
				return;
			case REG_IF + 2 :
				execute = FALSE;
				MMU.reg_IF[proc] &= (~(((u32)val)<<16));
				return;
				
                        case REG_IPCSYNC :
				{
				u32 remote = (proc+1)&1;
				u16 IPCSYNC_remote = T1ReadWord(MMU.MMU_MEM[remote][0x40], 0x180);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x180, (val&0xFFF0)|((IPCSYNC_remote>>8)&0xF));
				T1WriteWord(MMU.MMU_MEM[remote][0x40], 0x180, (IPCSYNC_remote&0xFFF0)|((val>>8)&0xF));
				MMU.reg_IF[remote] |= ((IPCSYNC_remote & (1<<14))<<2) & ((val & (1<<13))<<3);// & (MMU.reg_IME[remote] << 16);// & (MMU.reg_IE[remote] & (1<<16));// 
				//execute = FALSE;
				}
				return;
                        case REG_IPCFIFOCNT :
				{
				if(val & 0x4008)
				{
					FIFOInit(MMU.fifos + (IPCFIFO+((proc+1)&1)));
					T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, (val & 0xBFF4) | 1);
					T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184) | 1);
					MMU.reg_IF[proc] |= ((val & 4)<<15);// & (MMU.reg_IME[proc]<<17);// & (MMU.reg_IE[proc]&0x20000);//
					return;
				}
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184) | (val & 0xBFF4));
				}
				return;
                        case REG_TM0CNTL :
                        case REG_TM1CNTL :
                        case REG_TM2CNTL :
                        case REG_TM3CNTL :
				MMU.timerReload[proc][(adr>>2)&3] = val;
				return;
                        case REG_TM0CNTH :
                        case REG_TM1CNTH :
                        case REG_TM2CNTH :
                        case REG_TM3CNTH :
				if(val&0x80)
				{
				if(!(val&4)) MMU.timer[proc][((adr-2)>>2)&0x3] = MMU.timerReload[proc][((adr-2)>>2)&0x3];
				else MMU.timer[proc][((adr-2)>>2)&0x3] = 0;
				}
				MMU.timerON[proc][((adr-2)>>2)&0x3] = val & 0x80;
				switch(val&7)
				{
				case 0 :
					MMU.timerMODE[proc][((adr-2)>>2)&0x3] = 0+1;//proc;
					break;
				case 1 :
					MMU.timerMODE[proc][((adr-2)>>2)&0x3] = 6+1;//proc; 
					break;
				case 2 :
					MMU.timerMODE[proc][((adr-2)>>2)&0x3] = 8+1;//proc;
					break;
				case 3 :
					MMU.timerMODE[proc][((adr-2)>>2)&0x3] = 10+1;//proc;
					break;
				default :
					MMU.timerMODE[proc][((adr-2)>>2)&0x3] = 0xFFFF;
					break;
				}
				if(!(val & 0x80))
				MMU.timerRUN[proc][((adr-2)>>2)&0x3] = FALSE;
				T1WriteWord(MMU.MMU_MEM[proc][0x40], adr & 0xFFF, val);
				return;
                        case REG_DISPA_DISPCNT+2 : 
				{
				//execute = FALSE;
				u32 v = (T1ReadLong(MMU.MMU_MEM[proc][0x40], 0) & 0xFFFF) | ((u32) val << 16);
				GPU_setVideoProp(MainScreen.gpu, v);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0, v);
				}
				return;
                        case REG_DISPA_DISPCNT :
				{
				u32 v = (T1ReadLong(MMU.MMU_MEM[proc][0x40], 0) & 0xFFFF0000) | val;
				GPU_setVideoProp(MainScreen.gpu, v);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0, v);
				}
				return;
                        case REG_DISPB_DISPCNT+2 : 
				{
				//execute = FALSE;
				u32 v = (T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x1000) & 0xFFFF) | ((u32) val << 16);
				GPU_setVideoProp(SubScreen.gpu, v);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x1000, v);
				}
				return;
                        case REG_DISPB_DISPCNT :
				{
				u32 v = (T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x1000) & 0xFFFF0000) | val;
				GPU_setVideoProp(SubScreen.gpu, v);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x1000, v);
				}
				return;
			//case 0x020D8460 :
			/*case 0x0235A904 :
				LOG("ECRIRE %d %04X\r\n", proc, val);
				execute = FALSE;*/
                                case REG_DMA0CNTH :
				{
                                u32 v;

				//if(val&0x8000) execute = FALSE;
				//LOG("16 bit dma0 %04X\r\n", val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xBA, val);
				DMASrc[proc][0] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xB0);
				DMADst[proc][0] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xB4);
                                v = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xB8);
				MMU.DMAStartTime[proc][0] = (proc ? (v>>28) & 0x3 : (v>>27) & 0x7);
				MMU.DMACrt[proc][0] = v;
				if(MMU.DMAStartTime[proc][0] == 0)
					MMU_doDMA(proc, 0);
				#ifdef LOG_DMA2
				//else
				{
					LOG("proc %d, dma %d src %08X dst %08X %s\r\n", proc, 0, DMASrc[proc][0], DMADst[proc][0], (val&(1<<25))?"ON":"OFF");
				}
				#endif
				}
				return;
                                case REG_DMA1CNTH :
				{
                                u32 v;
				//if(val&0x8000) execute = FALSE;
				//LOG("16 bit dma1 %04X\r\n", val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xC6, val);
				DMASrc[proc][1] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xBC);
				DMASrc[proc][1] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xC0);
                                v = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xC4);
				MMU.DMAStartTime[proc][1] = (proc ? (v>>28) & 0x3 : (v>>27) & 0x7);
				MMU.DMACrt[proc][1] = v;
				if(MMU.DMAStartTime[proc][1] == 0)
					MMU_doDMA(proc, 1);
				#ifdef LOG_DMA2
				//else
				{
					LOG("proc %d, dma %d src %08X dst %08X %s\r\n", proc, 1, DMASrc[proc][1], DMADst[proc][1], (val&(1<<25))?"ON":"OFF");
				}
				#endif
				}
				return;
                                case REG_DMA2CNTH :
				{
                                u32 v;
				//if(val&0x8000) execute = FALSE;
				//LOG("16 bit dma2 %04X\r\n", val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xD2, val);
				DMASrc[proc][2] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xC8);
				DMASrc[proc][2] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xCC);
                                v = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xD0);
				MMU.DMAStartTime[proc][2] = (proc ? (v>>28) & 0x3 : (v>>27) & 0x7);
				MMU.DMACrt[proc][2] = v;
				if(MMU.DMAStartTime[proc][2] == 0)
					MMU_doDMA(proc, 2);
				#ifdef LOG_DMA2
				//else
				{
					LOG("proc %d, dma %d src %08X dst %08X %s\r\n", proc, 2, DMASrc[proc][2], DMADst[proc][2], (val&(1<<25))?"ON":"OFF");
				}
				#endif
				}
				return;
                                case REG_DMA3CNTH :
				{
                                u32 v;
				//if(val&0x8000) execute = FALSE;
				//LOG("16 bit dma3 %04X\r\n", val);
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0xDE, val);
				DMASrc[proc][3] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xD4);
				DMASrc[proc][3] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xD8);
                                v = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xDC);
				MMU.DMAStartTime[proc][3] = (proc ? (v>>28) & 0x3 : (v>>27) & 0x7);
				MMU.DMACrt[proc][3] = v;
		
				if(MMU.DMAStartTime[proc][3] == 0)
					MMU_doDMA(proc, 3);
				#ifdef LOG_DMA2
				//else
				{
					LOG("proc %d, dma %d src %08X dst %08X %s\r\n", proc, 3, DMASrc[proc][3], DMADst[proc][3], (val&(1<<25))?"ON":"OFF");
				}
				#endif
				}
				return;
                        //case REG_AUXSPICNT : execute = FALSE;
			default :
				T1WriteWord(MMU.MMU_MEM[proc][0x40], adr&MMU.MMU_MASK[proc][(adr>>20)&0xFF], val); 
				return;
		}
	}
	T1WriteWord(MMU.MMU_MEM[proc][(adr>>20)&0xFF], adr&MMU.MMU_MASK[proc][(adr>>20)&0xFF], val);
} 

u32 testval = 0;

void FASTCALL MMU_write32(u32 proc, u32 adr, u32 val)
{
	if((proc==ARMCPU_ARM9)&((adr&(~0x3FFF))==MMU.DTCMRegion))
	{
		T1WriteLong(ARM9Mem.ARM9_DTCM, adr & 0x3FFF, val);
		return ;
	}
	
	// CFlash writing, Mic
	if ((adr>=0x9000000)&&(adr<0x9900000)) {
	   cflash_write(adr,val);
	   return;
	}

	adr &= 0x0FFFFFFF;

        // This is bad, remove it
        if(proc == ARMCPU_ARM7)
        {
           if ((adr>=0x04000400)&&(adr<0x0400051D))
           {
              SPU_WriteLong(adr, val);
              return;
           }
        }

	if((adr>>24)==4)
	{
		switch(adr)
		{
#ifdef RENDER3D
			case 0x040004AC :
			T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x4AC, val);
			if(proc==ARMCPU_ARM9)
				OGLRender::glTexImage2D(testval, TRUE);
			//execute = FALSE;
			return;
			case 0x040004A8 :
				if(proc==ARMCPU_ARM9)
			{
				OGLRender::glTexImage2D(val, FALSE);
				//execute = FALSE;
				testval = val;
			}
			return;
			case 0x04000488 :
				if(proc==ARMCPU_ARM9)
			{
				OGLRender::glTexCoord(val);
				//execute = FALSE;
			}
			return;
			case 0x0400046C :
				if(proc==ARMCPU_ARM9)
				{
					OGLRender::glScale(val);
				}
				return;
			case 0x04000490 :
				if(proc==ARMCPU_ARM9)
				{
					//GPULOG("VERTEX 10 %d\r\n",val);
				}
				return;
			case 0x04000494 :
				if(proc==ARMCPU_ARM9)
				{
					//GPULOG(printf(txt, "VERTEXY %d\r\n",val);
				}
				return;
			case 0x04000498 :
				if(proc==ARMCPU_ARM9)
				{
					//GPULOG("VERTEXZ %d\r\n",val);
				}
				return;
			case 0x0400049C :
				if(proc==ARMCPU_ARM9)
				{
					//GPULOG("VERTEYZ %d\r\n",val);
				}
				return;
			case 0x04000400 :
				if(proc==ARMCPU_ARM9)
				{
					OGLRender::glCallList(val);
				}
				return;
			case 0x04000450 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glRestore();
				}
				return;
			case 0x04000580 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glViewPort(val);
				}
				return;
			case 0x04000350 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glClearColor(val);
				}
				return;
			case 0x04000440 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glMatrixMode(val);
				}
				return;
			case 0x04000458 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::ML4x4ajouter(val);
				}
				return;
			case 0x0400044C : 
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glStoreMatrix(val);
				}
				return;
			case 0x0400045C :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::ML4x3ajouter(val);
				}
				return;
			case 0x04000444 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glPushMatrix();
				}
				return;
			case 0x04000448 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glPopMatrix(val);
				}
				return;
			case 0x04000470 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::addTrans(val);
				}
				return;
			case 0x04000460 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glMultMatrix4x4(val);
				}
				return;
			case 0x04000464 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glMultMatrix4x3(val);
				}
				return;
			case 0x04000468 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glMultMatrix3x3(val);
				}
				return;
			case 0x04000500 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glBegin(val);
				}
				return;
			case 0x04000504 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glEnd();
				}
				return;
			case 0x04000480 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glColor3b(val);
				}
			return;
			case 0x0400048C :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glVertex3(val);
				}
				return;
			case 0x04000540 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glFlush();
				}
				return;
			case 0x04000454 :
				if(proc == ARMCPU_ARM9)
				{
					OGLRender::glLoadIdentity();
				}
				return;
#endif
                        case REG_DISPA_BG2PA :
				GPU_setPAPB(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2PC :
				GPU_setPCPD(MainScreen.gpu, 2, val);
				return;

                        case REG_DISPB_BG2PA :
				GPU_setPAPB(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2PC :
				GPU_setPCPD(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3PA :
				GPU_setPAPB(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3PC :
				GPU_setPCPD(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3PA :
				GPU_setPAPB(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3PC :
				GPU_setPCPD(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG2XL :
				GPU_setX(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG2YL :
				GPU_setY(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2XL :
				GPU_setX(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG2YL :
				GPU_setY(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3XL :
				GPU_setX(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG3YL :
				GPU_setY(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3XL :
				GPU_setX(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG3YL :
				GPU_setY(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_BG0HOFS :
				GPU_scrollXY(MainScreen.gpu, 0, val);
				return;
                        case REG_DISPA_BG1HOFS :
				GPU_scrollXY(MainScreen.gpu, 1, val);
				return;
                        case REG_DISPA_BG2HOFS :
				GPU_scrollXY(MainScreen.gpu, 2, val);
				return;
                        case REG_DISPA_BG3HOFS :
				GPU_scrollXY(MainScreen.gpu, 3, val);
				return;
                        case REG_DISPB_BG0HOFS :
				GPU_scrollXY(SubScreen.gpu, 0, val);
				return;
                        case REG_DISPB_BG1HOFS :
				GPU_scrollXY(SubScreen.gpu, 1, val);
				return;
                        case REG_DISPB_BG2HOFS :
				GPU_scrollXY(SubScreen.gpu, 2, val);
				return;
                        case REG_DISPB_BG3HOFS :
				GPU_scrollXY(SubScreen.gpu, 3, val);
				return;
                        case REG_DISPA_DISPCNT :
				GPU_setVideoProp(MainScreen.gpu, val);
				
				//GPULOG("MAIN INIT 32B %08X\r\n", val);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0, val);
				return;
				
                        case REG_DISPB_DISPCNT : 
				GPU_setVideoProp(SubScreen.gpu, val);
				//GPULOG("SUB INIT 32B %08X\r\n", val);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x1000, val);
				return;
			case REG_DISPA_WININ:
				GPU_setWINDOW_INCNT(MainScreen.gpu, val & 0xFFFF) ;
				GPU_setWINDOW_OUTCNT(MainScreen.gpu, (val >> 16) & 0xFFFF) ;
				break ;
			case REG_DISPB_WININ:
				GPU_setWINDOW_INCNT(SubScreen.gpu, val & 0xFFFF) ;
				GPU_setWINDOW_OUTCNT(SubScreen.gpu, (val >> 16) & 0xFFFF) ;
				break ;

			case REG_IME :
				MMU.reg_IME[proc] = val & 1;
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x208, val);
				return;
				
			case REG_IE :
				MMU.reg_IE[proc] = val;
				return;
			
			case REG_IF :
				MMU.reg_IF[proc] &= (~val); 
				return;
                        case REG_TM0CNTL :
                        case REG_TM1CNTL :
                        case REG_TM2CNTL :
                        case REG_TM3CNTL :
				MMU.timerReload[proc][(adr>>2)&0x3] = (u16)val;
				if(val&0x800000)
				{
					if(!(val&40000)) MMU.timer[proc][(adr>>2)&0x3] = MMU.timerReload[proc][(adr>>2)&0x3];
					else MMU.timer[proc][(adr>>2)&0x3] = 0;
				}
				MMU.timerON[proc][(adr>>2)&0x3] = val & 0x800000;
				switch((val>>16)&7)
				{
					case 0 :
					MMU.timerMODE[proc][(adr>>2)&0x3] = 0+1;//proc;
					break;
					case 1 :
					MMU.timerMODE[proc][(adr>>2)&0x3] = 6+1;//proc;
					break;
					case 2 :
					MMU.timerMODE[proc][(adr>>2)&0x3] = 8+1;//proc;
					break;
					case 3 :
					MMU.timerMODE[proc][(adr>>2)&0x3] = 10+1;//proc;
					break;
					default :
					MMU.timerMODE[proc][(adr>>2)&0x3] = 0xFFFF;
					break;
				}
				if(!(val & 0x800000))
				{
					MMU.timerRUN[proc][(adr>>2)&0x3] = FALSE;
				}
				T1WriteLong(MMU.MMU_MEM[proc][0x40], adr & 0xFFF, val);
				return;
                        case REG_DIVDENOM :
				{
                                        u16 cnt;
					s64 num = 0;
					s64 den = 1;
					s64 res;
					s64 mod;
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x298, val);
                                        cnt = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x280);
					switch(cnt&3)
					{
					case 0:
					{
						num = (s64) (s32) T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x290);
						den = (s64) (s32) T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x298);
					}
					break;
					case 1:
					{
						num = (s64) T1ReadQuad(MMU.MMU_MEM[proc][0x40], 0x290);
						den = (s64) (s32) T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x298);
					}
					break;
					case 2:
					{
						return;
					}
					break;
					default: 
						break;
					}
					if(den==0)
					{
						res = 0;
						mod = 0;
						cnt |= 0x4000;
						cnt &= 0x7FFF;
					}
					else
					{
						res = num / den;
						mod = num % den;
						cnt &= 0x3FFF;
					}
					DIVLOG("BOUT1 %08X%08X / %08X%08X = %08X%08X\r\n", (u32)(num>>32), (u32)num, 
											(u32)(den>>32), (u32)den, 
											(u32)(res>>32), (u32)res);
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2A0, (u32) res);
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2A4, (u32) (res >> 32));
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2A8, (u32) mod);
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2AC, (u32) (mod >> 32));
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x280, cnt);
				}
				return;
                        case REG_DIVDENOM+4 :
			{
                                u16 cnt;
				s64 num = 0;
				s64 den = 1;
				s64 res;
				s64 mod;
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x29C, val);
                                cnt = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x280);
				switch(cnt&3)
				{
				case 0:
				{
					return;
				}
				break;
				case 1:
				{
					return;
				}
				break;
				case 2:
				{
					num = (s64) T1ReadQuad(MMU.MMU_MEM[proc][0x40], 0x290);
					den = (s64) T1ReadQuad(MMU.MMU_MEM[proc][0x40], 0x298);
				}
				break;
				default: 
					break;
				}
				if(den==0)
				{
					res = 0;
					mod = 0;
					cnt |= 0x4000;
					cnt &= 0x7FFF;
				}
				else
				{
					res = num / den;
					mod = num % den;
					cnt &= 0x3FFF;
				}
				DIVLOG("BOUT2 %08X%08X / %08X%08X = %08X%08X\r\n", (u32)(num>>32), (u32)num, 
										(u32)(den>>32), (u32)den, 
										(u32)(res>>32), (u32)res);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2A0, (u32) res);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2A4, (u32) (res >> 32));
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2A8, (u32) mod);
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2AC, (u32) (mod >> 32));
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x280, cnt);
			}
			return;
                        case REG_SQRTPARAM :
				{
                                        u16 cnt;
					u64 v = 1;
					//execute = FALSE;
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2B8, val);
                                        cnt = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x2B0);
					switch(cnt&1)
					{
					case 0:
						v = (u64) T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x2B8);
						break;
					case 1:
						return;
					}
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2B4, (u32) sqrt(v));
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2B0, cnt & 0x7FFF);
					SQRTLOG("BOUT1 sqrt(%08X%08X) = %08X\r\n", (u32)(v>>32), (u32)v, 
										T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x2B4));
				}
				return;
                        case REG_SQRTPARAM+4 :
				{
                                        u16 cnt;
					u64 v = 1;
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2BC, val);
                                        cnt = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x2B0);
					switch(cnt&1)
					{
					case 0:
						return;
						//break;
					case 1:
						v = T1ReadQuad(MMU.MMU_MEM[proc][0x40], 0x2B8);
						break;
					}
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2B4, (u32) sqrt(v));
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x2B0, cnt & 0x7FFF);
					SQRTLOG("BOUT2 sqrt(%08X%08X) = %08X\r\n", (u32)(v>>32), (u32)v, 
										T1ReadLong(MMU.MMU_MEM[proc][0x40], 0x2B4));
				}
				return;
                        case REG_IPCSYNC :
				{
					//execute=FALSE;
					u32 remote = (proc+1)&1;
					u32 IPCSYNC_remote = T1ReadLong(MMU.MMU_MEM[remote][0x40], 0x180);
					T1WriteLong(MMU.MMU_MEM[proc][0x40], 0x180, (val&0xFFF0)|((IPCSYNC_remote>>8)&0xF));
					T1WriteLong(MMU.MMU_MEM[remote][0x40], 0x180, (IPCSYNC_remote&0xFFF0)|((val>>8)&0xF));
					MMU.reg_IF[remote] |= ((IPCSYNC_remote & (1<<14))<<2) & ((val & (1<<13))<<3);// & (MMU.reg_IME[remote] << 16);// & (MMU.reg_IE[remote] & (1<<16));//
				}
				return;
                        case REG_IPCFIFOCNT :
				if(val & 0x4008)
				{
					FIFOInit(MMU.fifos + (IPCFIFO+((proc+1)&1)));
					T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, (val & 0xBFF4) | 1);
					T1WriteWord(MMU.MMU_MEM[(proc+1)&1][0x40], 0x184, T1ReadWord(MMU.MMU_MEM[(proc+1)&1][0x40], 0x184) | 256);
					MMU.reg_IF[proc] |= ((val & 4)<<15);// & (MMU.reg_IME[proc] << 17);// & (MMU.reg_IE[proc] & 0x20000);//
					return;
				}
				T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, val & 0xBFF4);
				//execute = FALSE;
				return;
                        case REG_IPCFIFOSEND :
				{
					u16 IPCFIFO_CNT = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
					if(IPCFIFO_CNT&0x8000)
					{
					//if(val==43) execute = FALSE;
					u32 remote = (proc+1)&1;
					u32 fifonum = IPCFIFO+remote;
                                        u16 IPCFIFO_CNT_remote;
					FIFOAdd(MMU.fifos + fifonum, val);
					IPCFIFO_CNT = (IPCFIFO_CNT & 0xFFFC) | (MMU.fifos[fifonum].full<<1);
                                        IPCFIFO_CNT_remote = T1ReadWord(MMU.MMU_MEM[remote][0x40], 0x184);
					IPCFIFO_CNT_remote = (IPCFIFO_CNT_remote & 0xFCFF) | (MMU.fifos[fifonum].full<<10);
					T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, IPCFIFO_CNT);
					T1WriteWord(MMU.MMU_MEM[remote][0x40], 0x184, IPCFIFO_CNT_remote);
					MMU.reg_IF[remote] |= ((IPCFIFO_CNT_remote & (1<<10))<<8);// & (MMU.reg_IME[remote] << 18);// & (MMU.reg_IE[remote] & 0x40000);//
					//execute = FALSE;
					}
				}
				return;
                        case REG_DMA0CNTL :
				//LOG("32 bit dma0 %04X\r\n", val);
				DMASrc[proc][0] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xB0);
				DMADst[proc][0] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xB4);
				MMU.DMAStartTime[proc][0] = (proc ? (val>>28) & 0x3 : (val>>27) & 0x7);
				MMU.DMACrt[proc][0] = val;
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0xB8, val);
				if( MMU.DMAStartTime[proc][0] == 0)		// Start Immediately
					MMU_doDMA(proc, 0);
				#ifdef LOG_DMA2
				else
				{
					LOG("proc %d, dma %d src %08X dst %08X start taille %d %d\r\n", proc, 0, DMASrc[proc][0], DMADst[proc][0], 0, ((MMU.DMACrt[proc][0]>>27)&7));
				}
				#endif
				//execute = FALSE;
				return;
                                case REG_DMA1CNTL :
				//LOG("32 bit dma1 %04X\r\n", val);
				DMASrc[proc][1] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xBC);
				DMADst[proc][1] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xC0);
				MMU.DMAStartTime[proc][1] = (proc ? (val>>28) & 0x3 : (val>>27) & 0x7);
				MMU.DMACrt[proc][1] = val;
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0xC4, val);
				if(MMU.DMAStartTime[proc][1] == 0)		// Start Immediately
					MMU_doDMA(proc, 1);
				#ifdef LOG_DMA2
				else
				{
					LOG("proc %d, dma %d src %08X dst %08X start taille %d %d\r\n", proc, 1, DMASrc[proc][1], DMADst[proc][1], 0, ((MMU.DMACrt[proc][1]>>27)&7));
				}
				#endif
				return;
                        case REG_DMA2CNTL :
				//LOG("32 bit dma2 %04X\r\n", val);
				DMASrc[proc][2] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xC8);
				DMADst[proc][2] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xCC);
				MMU.DMAStartTime[proc][2] = (proc ? (val>>28) & 0x3 : (val>>27) & 0x7);
				MMU.DMACrt[proc][2] = val;
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0xD0, val);
				if(MMU.DMAStartTime[proc][2] == 0)		// Start Immediately
					MMU_doDMA(proc, 2);
				#ifdef LOG_DMA2
				else
				{
					LOG("proc %d, dma %d src %08X dst %08X start taille %d %d\r\n", proc, 2, DMASrc[proc][2], DMADst[proc][2], 0, ((MMU.DMACrt[proc][2]>>27)&7));
				}
				#endif
				return;
				case 0x040000DC :
				//LOG("32 bit dma3 %04X\r\n", val);
				DMASrc[proc][3] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xD4);
				DMADst[proc][3] = T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xD8);
				MMU.DMAStartTime[proc][3] = (proc ? (val>>28) & 0x3 : (val>>27) & 0x7);
				MMU.DMACrt[proc][3] = val;
				T1WriteLong(MMU.MMU_MEM[proc][0x40], 0xDC, val);
				if(	MMU.DMAStartTime[proc][3] == 0)		// Start Immediately
					MMU_doDMA(proc, 3);
				#ifdef LOG_DMA2
				else
				{
					LOG("proc %d, dma %d src %08X dst %08X start taille %d %d\r\n", proc, 3, DMASrc[proc][3], DMADst[proc][3], 0, ((MMU.DMACrt[proc][3]>>27)&7));
				}
				#endif
				return;
                        case REG_GCROMCTRL :
				{
					int i;

                                        if(MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT) == 0xB7)
					{
                                                MMU.dscard[proc].adress = (MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT+1) << 24) | (MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT+2) << 16) | (MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT+3) << 8) | (MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT+4));
						MMU.dscard[proc].transfer_count = 0x80;// * ((val>>24)&7));
					}
                                        else if (MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT) == 0xB8)
                                        {
                                                // Get ROM chip ID
                                                val |= 0x800000; // Data-Word Status
                                                T1WriteLong(MMU.MMU_MEM[proc][(REG_GCROMCTRL >> 20) & 0xff], REG_GCROMCTRL & 0xfff, val);
                                                MMU.dscard[proc].adress = 0;
                                        }
					else
					{
                                                LOG("CARD command: %02X\n", MEM_8(MMU.MMU_MEM[proc], REG_GCCMDOUT));
					}
					
					//CARDLOG("%08X : %08X %08X\r\n", adr, val, adresse[proc]);
                    val |= 0x00800000;
					
					if(MMU.dscard[proc].adress == 0)
					{
                                                val &= ~0x80000000; 
                                                T1WriteLong(MMU.MMU_MEM[proc][(REG_GCROMCTRL >> 20) & 0xff], REG_GCROMCTRL & 0xfff, val);
						return;
					}
                                        T1WriteLong(MMU.MMU_MEM[proc][(REG_GCROMCTRL >> 20) & 0xff], REG_GCROMCTRL & 0xfff, val);
										
					/* launch DMA if start flag was set to "DS Cart" */
					if(proc == ARMCPU_ARM7) i = 2;
					else i = 5;
					
					if(proc == ARMCPU_ARM9 && MMU.DMAStartTime[proc][0] == i)	/* dma0/1 on arm7 can't start on ds cart event */
					{
						MMU_doDMA(proc, 0);
						return;
					}
					else if(proc == ARMCPU_ARM9 && MMU.DMAStartTime[proc][1] == i)
					{
						MMU_doDMA(proc, 1);
						return;
					}
					else if(MMU.DMAStartTime[proc][2] == i)
					{
						MMU_doDMA(proc, 2);
						return;
					}
					else if(MMU.DMAStartTime[proc][3] == i)
					{
						MMU_doDMA(proc, 3);
						return;
					}
					return;

				}
				return;
                        case REG_DISPA_BG0CNT :
				GPU_setBGProp(MainScreen.gpu, 0, (val&0xFFFF));
				GPU_setBGProp(MainScreen.gpu, 1, (val>>16));
				//if((val>>16)==0x400) execute = FALSE;
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
			case REG_DISPA_BLDCNT:
				GPU_setBLDCNT	(MainScreen.gpu,val&0xffff);
				GPU_setBLDALPHA (MainScreen.gpu,val>>16);
				break;
			case REG_DISPB_BLDCNT:
				GPU_setBLDCNT	(SubScreen.gpu,val&0xffff);
				GPU_setBLDALPHA (SubScreen.gpu,val>>16);
				break;

			case REG_DISPA_DISPMMEMFIFO:
			{
				// NOTE: right now, the capture unit is not taken into account,
				//       I don't know is it should be handled here or 
			
				FIFOAdd(MMU.fifos + MAIN_MEMORY_DISP_FIFO, val);
				break;
			}
			//case 0x21FDFF0 :  if(val==0) execute = FALSE;
			//case 0x21FDFB0 :  if(val==0) execute = FALSE;
			default :
				T1WriteLong(MMU.MMU_MEM[proc][0x40], adr & MMU.MMU_MASK[proc][(adr>>20)&0xFF], val);
				return;
		}
	}
	T1WriteLong(MMU.MMU_MEM[proc][(adr>>20)&0xFF], adr&MMU.MMU_MASK[proc][(adr>>20)&0xFF], val);
}


void FASTCALL MMU_doDMA(u32 proc, u32 num)
{
	u32 src = DMASrc[proc][num];
	u32 dst = DMADst[proc][num];
        u32 taille;

	if(src==dst)
	{
	 T1WriteLong(MMU.MMU_MEM[proc][0x40], 0xB8 + (0xC*num), T1ReadLong(MMU.MMU_MEM[proc][0x40], 0xB8 + (0xC*num)) & 0x7FFFFFFF);
	 return;
	}
	
	if((!(MMU.DMACrt[proc][num]&(1<<31)))&&(!(MMU.DMACrt[proc][num]&(1<<25))))
	{       /* not enabled and not to be repeated */
	 MMU.DMAStartTime[proc][num] = 0;
	 MMU.DMACycle[proc][num] = 0;
	 //MMU.DMAing[proc][num] = FALSE;
	 return;
	}
	

	/* word count */
    taille = (MMU.DMACrt[proc][num]&0xFFFF);

	// If we are in "Main memory display" mode just copy an entire 
	// screen (256x192 pixels). 
	//    Reference:  http://nocash.emubase.de/gbatek.htm#dsvideocaptureandmainmemorydisplaymode
	//       (under DISP_MMEM_FIFO)
	if ((MMU.DMAStartTime[proc][num]==4) &&		// Must be in main memory display mode
		(taille==4) &&							// Word must be 4
		(((MMU.DMACrt[proc][num]>>26)&1) == 1))	// Transfer mode must be 32bit wide
		taille = 256*192/2;
	
	if(MMU.DMAStartTime[proc][num] == 5) taille *= 0x80;
	
	MMU.DMACycle[proc][num] = taille + nds.cycles;
	
	MMU.DMAing[proc][num] = TRUE;
	
	DMALOG("proc %d, dma %d src %08X dst %08X start %d taille %d repeat %s %08X\r\n", proc, num, src, dst, MMU.DMAStartTime[proc][num], taille, (MMU.DMACrt[proc][num]&(1<<25))?"on":"off",MMU.DMACrt[proc][num]);
	
	if(!(MMU.DMACrt[proc][num]&(1<<25))) MMU.DMAStartTime[proc][num] = 0;
	
	switch((MMU.DMACrt[proc][num]>>26)&1)
	{
	 case 1 :           /* 32 bit DMA transfers */
	 switch(((MMU.DMACrt[proc][num]>>21)&0xF))
	 {
		  u32 i;
		 case 0 :       /* dst and src increment */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst += 4;
			  src += 4;
		  }
		  break;		  
		 case 1 :       /* dst decrement, src increment */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst -= 4;
			  src += 4;
		  }
		  break;
		 case 2 :       /* dst fixed, src increment */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  src += 4;
		  }
		  break;
		 case 3 :       /*dst increment/reload, src increment */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst += 4;
			  src += 4;
		  }
		  break;
		 case 4 :       /* dst increment, src decrement */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst += 4;
			  src -= 4;
		  }
		  break;
		 case 5 :       /* dst decrement, src decrement */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst -= 4;
			  src -= 4;
		  }
		  break;
		 case 6 :       /* dst fixed, src decrement */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  src -= 4;
		  }
		  break;
		 case 7 :       /* dst increment/reload, src decrement */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst += 4;
			  src -= 4;
		  }
		  break;
		 case 8 :       /* dst increment, src fixed */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst += 4;
		  }
		  break;
		 case 9 :       /* dst decrement, src fixed */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst -= 4;
		  }
		  break;
		 case 10 :      /* dst fixed, src fixed */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
		  }
		  break;
		 case 11 :      /* dst increment/reload, src fixed */
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_writeWord(proc, dst, MMU_readWord(proc, src));
			  dst += 4;
		  }
		  break;
		 default :      /* reserved */
			break;
	 }
	 break;
	 case 0 :   /* 16 bit transfers */
	 switch(((MMU.DMACrt[proc][num]>>21)&0xF))
	 {
		  u32 i;
		 case 0 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst += 2;
			  src += 2;
		  }
		  break;		  
		 case 1 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst -= 2;
			  src += 2;
		  }
		  break;
		 case 2 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  src += 2;
		  }
		  break;
		 case 3 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst += 2;
			  src += 2;
		  }
		  break;
		 case 4 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst += 2;
			  src -= 2;
		  }
		  break;
		 case 5 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst -= 2;
			  src -= 2;
		  }
		  break;
		 case 6 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  src -= 2;
		  }
		  break;
		 case 7 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst += 2;
			  src -= 2;
		  }
		  break;
		 case 8 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst += 2;
		  }
		  break;
		 case 9 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst -= 2;
		  }
		  break;
		 case 10 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
		  }
		  break;
		 case 11 :
		  for(i = 0; i < taille; ++i)
		  {
			  MMU_write16(proc, dst, MMU_readHWord(proc, src));
			  dst += 2;
		  }
		  break;
		 default :
			break;
	 }
	 break;
	}
}

#ifdef MMU_ENABLE_ACL
u8 FASTCALL MMU_read8_acl(u32 proc, u32 adr, u32 access)
{
    if (proc == ARMCPU_ARM9)      /* on arm9 we need to check the MPU regions */
    {
         if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10)
         {
             /* is user mode access */
             access &= ~1 ;
         } else {
             /* every other mode: sys */
             access |= 1 ;
         }
         if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE)
         {
              execute = FALSE ;
         }
    }
     return MMU_read8(proc,adr) ;
}

u16 FASTCALL MMU_read16_acl(u32 proc, u32 adr, u32 access)
{
    if (proc == ARMCPU_ARM9)      /* on arm9 we need to check the MPU regions */
    {
         if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10)
         {
             /* is user mode access */
             access &= ~1 ;
         } else {
             /* every other mode: sys */
             access |= 1 ;
         }
         if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE)
         {
              execute = FALSE ;
         }
    }
     return MMU_read16(proc,adr) ;
}

u32 FASTCALL MMU_read32_acl(u32 proc, u32 adr, u32 access)
{
    if (proc == ARMCPU_ARM9)      /* on arm9 we need to check the MPU regions */
    {
         if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10)
         {
             /* is user mode access */
             access &= ~1 ;
         } else {
             /* every other mode: sys */
             access |= 1 ;
         }
         if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE)
         {
              execute = FALSE ;
         }
    }
     return MMU_read32(proc,adr) ;
}

void FASTCALL MMU_write8_acl(u32 proc, u32 adr, u8 val)
{
    if (proc == ARMCPU_ARM9)      /* on arm9 we need to check the MPU regions */
    {
         u32 access = CP15_ACCESS_WRITE ;
         if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10)
         {
             /* is user mode access */
             access &= ~1 ;
         } else {
             /* every other mode: sys */
             access |= 1 ;
         }
         if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE)
         {
              execute = FALSE ;
         }
    }
    MMU_write8(proc,adr,val) ;
}

void FASTCALL MMU_write16_acl(u32 proc, u32 adr, u16 val)
{
    if (proc == ARMCPU_ARM9)      /* on arm9 we need to check the MPU regions */
    {
         u32 access = CP15_ACCESS_WRITE ;
         if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10)
         {
             /* is user mode access */
             access &= ~1 ;
         } else {
             /* every other mode: sys */
             access |= 1 ;
         }
         if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE)
         {
              execute = FALSE ;
         }
    }
    MMU_write16(proc,adr,val) ;
}

void FASTCALL MMU_write32_acl(u32 proc, u32 adr, u32 val)
{
    if (proc == ARMCPU_ARM9)      /* on arm9 we need to check the MPU regions */
    {
         u32 access = CP15_ACCESS_WRITE ;
         if ((NDS_ARM9.CPSR.val & 0x1F) == 0x10)
         {
             /* is user mode access */
             access &= ~1 ;
         } else {
             /* every other mode: sys */
             access |= 1 ;
         }
         if (armcp15_isAccessAllowed((armcp15_t *)NDS_ARM9.coproc[15],adr,access)==FALSE)
         {
              execute = FALSE ;
         }
    }
    MMU_write32(proc,adr,val) ;
}
#endif
