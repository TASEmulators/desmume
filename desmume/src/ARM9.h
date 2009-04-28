#ifndef ARM9_H
#define ARM9_H

#include "types.h"

struct ALIGN(16) ARM9_struct {
        //ARM9 mem
        u8 ARM9_ITCM[0x8000];
        u8 ARM9_DTCM[0x4000];
        u8 MAIN_MEM[0x800000]; //this has been expanded to 8MB to support debug consoles
        u8 ARM9_REG[0x1000000];
        u8 ARM9_BIOS[0x8000];
        u8 ARM9_VMEM[0x800];
        u8 ARM9_LCD[0xA4000+0x4000]; //an extra 16KB for blank memory
        u8 ARM9_OAM[0x800];

		u8* ExtPal[2][4];
		u8* ObjExtPal[2][2];
		
		struct TextureInfo {
			u8* texPalSlot[6];
			u8* textureSlotAddr[4];
		} texInfo;

		u8 blank_memory[0x20000];
};

extern ARM9_struct ARM9Mem;

#endif
