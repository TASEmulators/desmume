/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006 Theo Berkau

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

#ifndef GPU_H
#define GPU_H

#include "ARM9.h"
#include <stdio.h>
#include "mem.h"

#ifdef __cplusplus
extern "C" {
#endif


#define GPU_MAIN	0
#define GPU_SUB	1

/* human readable bitmask names */
#define ADDRESS_STEP_512B       0x00200
#define ADDRESS_STEP_1KB        0x00400
#define ADDRESS_STEP_2KB        0x00800
#define ADDRESS_STEP_4KB        0x01000
#define ADDRESS_STEP_8KB        0x02000
#define ADDRESS_STEP_16KB       0x04000
#define ADDRESS_STEP_32KB       0x08000
#define ADDRESS_STEP_64kB       0x10000

typedef struct
{
/*0*/	unsigned DisplayMode:3;
/*3*/	unsigned BG0_3D:1;
/*4*/	unsigned SpriteMode:3;
/*7*/	unsigned ForceBlank:1;
/*8*/	unsigned BG0_Enable:1;
/*9*/	unsigned BG1_Enable:1;
/*10*/	unsigned BG2_Enable:1;
/*11*/	unsigned BG3_Enable:1;
/*12*/	unsigned Sprite_Enable:1;
/*13*/	unsigned Win0_Enable:1;
/*14*/	unsigned Win1_Enable:1;
/*15*/	unsigned SpriteWin_Enable:1;
/*16*/	unsigned ExMode:2; // (00: Framebuffer, 01: GBA-style, 10: Framebuffer-alike, 11: ?)
/*18*/	unsigned FrameBufferSelect:2; // ExMode 2
/*20*/	unsigned ExSpriteMode:3;
/*23*/	unsigned :4;
/*27*/	unsigned ScreenBaseBlock:3;
/*30*/	unsigned ExBGPalette_Enable:1;
/*31*/	unsigned ExSpritePalette_Enable:1;
} _DISPCNT_;

#define DISPCNT_OBJMAPING1D(val)			(((val) >> 4) & 1)
#define DISPCNT_BG0ENABLED(val)				(((val) >> 8) & 1)
#define DISPCNT_BG1ENABLED(val)				(((val) >> 9) & 1)
#define DISPCNT_BG2ENABLED(val)				(((val) >> 10) & 1)
#define DISPCNT_BG3ENABLED(val)				(((val) >> 11) & 1)
#define DISPCNT_SPRITEENABLE(val)			(((val) >> 12) & 1)
#define DISPCNT_MODE(val)					((val) & 7)
/* display mode: gpu0: (val>>16) & 3, gpu1: (val>>16) & 1 */
#define DISPCNT_DISPLAY_MODE(val,num)		(((val) >> 16) & ((num)?1:3))
#define DISPCNT_VRAMBLOCK(val)				(((val) >> 18) & 3)
#define DISPCNT_TILEOBJ1D_BOUNDARY(val)		(((val) >> 20) & 3)
#define DISPCNT_BMPOBJ1D_BOUNDARY(val)		(((val) >> 22) & 1)
#define DISPCNT_SCREENBASEBLOCK(val)		(((val) >> 27) & 7)
#define DISPCNT_USEEXTPAL(val)				(((val) >> 30) & 1)

#define BGCNT_PRIORITY(val)					((val) & 3)
#define BGCNT_CHARBASEBLOCK(val)			(((val) >> 2) & 0x0F)
#define BGCNT_256COL(val)					(((val) >> 7) & 0x1)
#define BGCNT_SCREENBASEBLOCK(val)			(((val) >> 8) & 0x1F)
#define BGCNT_EXTPALSLOT(val)				(((val) >> 13) & 0x1)
#define BGCNT_SCREENSIZE(val)				(((val) >> 14) & 0x3)

typedef struct
{
// found here : http://www.bottledlight.com/ds/index.php/Video/Sprites
// attr0
/*0*/	unsigned Y:8;
/*8*/	unsigned RotScale:2; // (00: Normal, 01: Rot/scale, 10: Disabled, 11: Double-size rot/scale)
/*10*/	unsigned Mode:2;     // (00: Normal, 01: Transparent, 10: Object window, 11: Bitmap)
/*12*/	unsigned Mosaic:1;   // (1: Enabled)
/*13*/	unsigned Depth:1;    // (0: 16, 1: 256)
/*14*/	unsigned Shape:2;    // (00: Square, 01: Wide, 10: Tall, 11: Illegal)
// attr1
/*0*/	signed   X:9;
/*9*/	unsigned RotScalIndex:3; // Rot/scale matrix index 
/*12*/	unsigned HFlip:1;
/*13*/	unsigned VFlip:1;
/*14*/	unsigned Size:2;
// attr2
/*0*/	unsigned TileIndex:10;
/*10*/	unsigned Priority:2;
/*12*/	unsigned PaletteIndex:4;
// attr3
	unsigned attr3:16;
} _OAM_;

typedef struct
{
     u16 attr0;
     u16 attr1;
     u16 attr2;
     u16 attr3;
} OAM;

typedef struct
{
     s16 x;
     s16 y;
} size;

typedef struct _GPU GPU;

struct _GPU
{
       //GPU(u8 l);
       
       u32 prop;

       u16 BGProp[4];
		 
#define BGBmpBB BG_bmp_ram
#define BGChBB BG_tile_ram
		 
       u8 *(BG_bmp_ram[4]);
       u8 *(BG_tile_ram[4]);
       u8 *(BG_map_ram[4]);
		 
       u8 BGExtPalSlot[4];
       u32 BGSize[4][2];
       u16 BGSX[4];
       u16 BGSY[4];

       s32 BGX[4];
       s32 BGY[4];
       s16 BGPA[4];
       s16 BGPB[4];
       s16 BGPC[4];
       s16 BGPD[4];

       u8 lcd;
		u8 core;

       u8 dispMode;
       u8 vramBlock;

       u8 nbBGActif;
       u8 BGIndex[4];
       u8 ordre[4];
       BOOL dispBG[4];
       
       OAM * oam;
       u8 * sprMem;
       u8 sprBlock;
       u8 sprBMPBlock;
       u8 sprBMPMode;
		u32 sprEnable ;

       u16 BLDCNT ;
	   u16 BLDALPHA ;
	   u16 BLDY ;

	   u16 MOSAIC ;
       
       void (*spriteRender)(GPU * gpu, u16 l, u8 * dst, u8 * prioTab);
};

extern u8 GPU_screen[4*256*192];

GPU * GPU_Init(u8 l);
void GPU_Reset(GPU *g, u8 l);
void GPU_DeInit(GPU *);

void textBG(GPU * gpu, u8 num, u8 * DST);
void rotBG(GPU * gpu, u8 num, u8 * DST);
void extRotBG(GPU * gpu, u8 num, u8 * DST);
void sprite1D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab);
void sprite2D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab);

extern short sizeTab[4][4][2];
extern size sprSizeTab[4][4];
extern s8 mode2type[8][4];
extern void (*modeRender[8][4])(GPU * gpu, u8 num, u16 l, u8 * DST);

typedef struct {
	GPU * gpu;
	u16 offset;
} Screen;

extern Screen MainScreen;
extern Screen SubScreen;

void Screen_Init(void);
void Screen_Reset(void);
void Screen_DeInit(void);

static INLINE void GPU_ligne(Screen * screen, u16 l)
{
     GPU * gpu = screen->gpu;
     u8 * dst =  GPU_screen + (screen->offset + l) * 512;
     u8 spr[512];
     u8 sprPrio[256];
     u8 bgprio;
     int i;
     u8 i8;
     u16 i16;
     u32 c;

     // This could almost be changed to use function pointers
     switch (gpu->dispMode)
     {
        case 1: // Display BG and OBJ layers
           break;
        case 0: // Display Off(Display white)
        {
           for (i=0; i<256; i++)
           {
              T2WriteWord(dst, i << 1, 0x7FFF);
           }
           return;
        }
        case 2: // Display framebuffer
        {
           int ii = l * 256 * 2;
           for (i=0; i<(256 * 2); i+=2)
           {
              u8 * vram = ARM9Mem.ARM9_LCD + (gpu->vramBlock * 0x20000);

              T2WriteWord(dst, i, T1ReadWord(vram, ii));
              ii+=2;
           }
           return;
        }
        case 3: // Display from Main RAM(FIX ME)
           return;
     }
     
     c = T1ReadWord(ARM9Mem.ARM9_VMEM, gpu->lcd * 0x400);
     c |= (c<<16);
     
     for(i8 = 0; i8< 128; ++i8)
     {
	  T2WriteLong(dst, i8 << 2, c);
	  T2WriteLong(spr, i8 << 2, c);
	  T1WriteWord(sprPrio, i8 << 1, (4 << 8) | (4));
     }
     
     if(!gpu->nbBGActif)
     {
		 if (gpu->sprEnable)
		 {
			gpu->spriteRender(gpu, l, dst, sprPrio);
		 }

		return;
     }
     
	 if (gpu->sprEnable)
	 {
	     gpu->spriteRender(gpu, l, spr, sprPrio);
     
        if((gpu->BGProp[gpu->ordre[0]]&3)!=3)
        {
           for(i16 = 0; i16 < 128; ++i16) {
	         T2WriteLong(dst, i16 << 2, T2ReadLong(spr, i16 << 2));
	       }
        }
     }
     
     for(i8 = 0; i8 < gpu->nbBGActif; ++i8)
     {
          modeRender[gpu->prop&7][gpu->ordre[i8]](gpu, gpu->ordre[i8], l, dst);
          bgprio = gpu->BGProp[gpu->ordre[i8]]&3;
          if (gpu->sprEnable)
          {
            for(i16 = 0; i16 < 256; ++i16)
               if(bgprio>=sprPrio[i16])
		         T2WriteWord(dst, i16 << 1, T2ReadWord(spr, i16 << 1));
          }
     }
}
 
void GPU_setVideoProp(GPU *, u32 p);
void GPU_setBGProp(GPU *, u16 num, u16 p);
void GPU_scrollX(GPU *, u8 num, u16 v);
void GPU_scrollY(GPU *, u8 num, u16 v);
void GPU_scrollXY(GPU *, u8 num, u32 v);

void GPU_setX(GPU *, u8 num, u32 v);
void GPU_setXH(GPU *, u8 num, u16 v);
void GPU_setXL(GPU *, u8 num, u16 v);
void GPU_setY(GPU *, u8 num, u32 v);
void GPU_setYH(GPU *, u8 num, u16 v);
void GPU_setYL(GPU *, u8 num, u16 v);
void GPU_setPA(GPU *, u8 num, u16 v);
void GPU_setPB(GPU *, u8 num, u16 v);
void GPU_setPC(GPU *, u8 num, u16 v);
void GPU_setPD(GPU *, u8 num, u16 v);
void GPU_setPAPB(GPU *, u8 num, u32 v);
void GPU_setPCPD(GPU *, u8 num, u32 v);
       
void GPU_setBLDCNT(GPU *gpu, u16 v) ;
void GPU_setBLDALPHA(GPU *gpu, u16 v) ;
void GPU_setBLDY(GPU *gpu, u16 v) ;
void GPU_setMOSAIC(GPU *gpu, u16 v) ;

void GPU_remove(GPU *, u8 num);
void GPU_addBack(GPU *, u8 num);

#ifdef __cplusplus
}
#endif

#endif

