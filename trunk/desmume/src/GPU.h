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
       
       u8 nbBGActif;
       u8 BGIndex[4];
       u8 ordre[4];
       BOOL dispBG[4];
       
       OAM * oam;
       u8 * sprMem;
       u8 sprBlock;
       u8 sprBMPBlock;
       u8 sprBMPMode;
       
       void (*spriteRender)(GPU * gpu, u16 l, u16 * dst, u8 * prioTab);
};

extern u16 GPU_screen[2*256*192];

GPU * GPU_Init(u8 l);
void GPU_Reset(GPU *g, u8 l);
void GPU_DeInit(GPU *);

void textBG(GPU * gpu, u8 num, u16 * DST);
void rotBG(GPU * gpu, u8 num, u16 * DST);
void extRotBG(GPU * gpu, u8 num, u16 * DST);
void sprite1D(GPU * gpu, u16 l, u16 * dst, u8 * prioTab);
void sprite2D(GPU * gpu, u16 l, u16 * dst, u8 * prioTab);

extern short sizeTab[4][4][2];
extern size sprSizeTab[4][4];
extern s8 mode2type[8][4];
extern void (*modeRender[8][4])(GPU * gpu, u8 num, u16 l, u16 * DST);

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
     u16 * dst =  GPU_screen + (screen->offset + l) * 256;
     u16 spr[256];
     u8 sprPrio[256];
     u8 bgprio;
     int i;
     u8 i8;
     u16 i16;

     /* FIXME I've just quickly added mic's framebuffer patch here.
      * I'm really not sure it's correct.
      */
     if (gpu->lcd == 0) {
        u32 mainlcdcnt = ((u32 *)ARM9Mem.ARM9_REG)[0];
        int ii = l*256;
        if ((mainlcdcnt&0x10000)==0) {
           for (i=0; i<256; i++) {
               ((u16 *)dst)[i] = ((u16 *)ARM9Mem.ARM9_LCD)[ii];
               ii++;
           }
           return;
        }
     }
     
     u32 c = T1ReadWord(ARM9Mem.ARM9_VMEM, gpu->lcd * 0x400);
     c |= (c<<16);
     
     for(i8 = 0; i8< 128; ++i8)
     {
          ((u32 *)dst)[i8] = c;
          ((u32 *)spr)[i8] = c;
	  T1WriteWord(sprPrio, i8 << 1, (4 << 8) | (4));
     }
     
     if(!gpu->nbBGActif)
     {
          gpu->spriteRender(gpu, l, dst, sprPrio);
          return;
     }
     
     gpu->spriteRender(gpu, l, spr, sprPrio);
     
     if((gpu->BGProp[gpu->ordre[0]]&3)!=3)
     {
          for(i16 = 0; i16 < 128; ++i16)
               ((u32 *)dst)[i16] = ((u32 *)spr)[i16];
     }
     
     for(i8 = 0; i8 < gpu->nbBGActif; ++i8)
     {
          modeRender[gpu->prop&7][gpu->ordre[i8]](gpu, gpu->ordre[i8], l, dst);
          bgprio = gpu->BGProp[gpu->ordre[i8]]&3;
          for(i16 = 0; i16 < 256; ++i16)
               if(bgprio>=sprPrio[i16]) 
                    dst[i16] = spr[i16];
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
       
void GPU_remove(GPU *, u8 num);
void GPU_addBack(GPU *, u8 num);

#ifdef __cplusplus
}
#endif

#endif

