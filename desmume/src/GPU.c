/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006 Theo Berkau
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

#include <string.h>
#include <stdlib.h>

#include "MMU.h"
#include "GPU.h"
#include "debug.h"

ARM9_struct ARM9Mem;

extern BOOL click;
Screen MainScreen;
Screen SubScreen;

//#define DEBUG_TRI

u8 GPU_screen[4*256*192];

short sizeTab[4][4][2] =
{
      {{256,256}, {512, 256}, {256, 512}, {512, 512}},
      {{128,128}, {256, 256}, {512, 512}, {1024, 1024}},
//      {{128,128}, {256, 256}, {512, 256}, {512, 512}},
      {{512,1024}, {1024, 512}, {0, 0}, {0, 0}},
      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
};

size sprSizeTab[4][4] = 
{
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};

s8 mode2type[8][4] = 
{
      {0, 0, 0, 0},
      {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 0, 1},
      {0, 0, 1, 1},
      {0, 0, 1, 1},
      {3, 3, 2, 3},
      {0, 0, 0, 0}
};

void lineText(GPU * gpu, u8 num, u16 l, u8 * DST);
void lineRot(GPU * gpu, u8 num, u16 l, u8 * DST);
void lineExtRot(GPU * gpu, u8 num, u16 l, u8 * DST);

void (*modeRender[8][4])(GPU * gpu, u8 num, u16 l, u8 * DST)=
{
     {lineText, lineText, lineText, lineText},     //0
     {lineText, lineText, lineText, lineRot},      //1
     {lineText, lineText, lineRot, lineRot},       //2
     {lineText, lineText, lineText, lineExtRot},   //3
     {lineText, lineText, lineRot, lineExtRot},    //4
     {lineText, lineText, lineExtRot, lineExtRot}, //5
     {lineText, lineText, lineText, lineText},     //6
     {lineText, lineText, lineText, lineText},     //7
};

GPU * GPU_Init(u8 l)
{
     GPU * g;

     if ((g = (GPU *) malloc(sizeof(GPU))) == NULL)
        return NULL;

     GPU_Reset(g, l);

     return g;
}

void GPU_Reset(GPU *g, u8 l)
{
   memset(g, 0, sizeof(GPU));

   g->lcd = l;
   g->core = l;
   g->BGSize[0][0] = g->BGSize[1][0] = g->BGSize[2][0] = g->BGSize[3][0] = 256;
   g->BGSize[0][1] = g->BGSize[1][1] = g->BGSize[2][1] = g->BGSize[3][1] = 256;
   g->dispOBJ = g->dispBG[0] = g->dispBG[1] = g->dispBG[2] = g->dispBG[3] = TRUE;
     
  MMU.vram_mode[0] = 4 ;
  MMU.vram_mode[1] = 5 ;
  MMU.vram_mode[2] = 6 ;
  MMU.vram_mode[3] = 7 ;

   g->spriteRender = sprite1D;
     
   if(g->core == GPU_SUB)
   {
      g->oam = (OAM *)(ARM9Mem.ARM9_OAM + ADDRESS_STEP_1KB);
      g->sprMem = ARM9Mem.ARM9_BOBJ;
   }
   else
   {
      g->oam = (OAM *)(ARM9Mem.ARM9_OAM);
      g->sprMem = ARM9Mem.ARM9_AOBJ;
   }
}

void GPU_DeInit(GPU * gpu)
{
     free(gpu);
}

void GPU_resortBGs(GPU *gpu)
{
	u8 i, j, prio;
	struct _DISPCNT * cnt = &gpu->dispCnt.bits;
	itemsForPriority_t * item;

	for (i=0; i<192; i++)
		memset(gpu->sprWin[i],0, 256);

	// we don't need to check for windows here...
	gpu->LayersEnable[0] = gpu->dispBG[0] && cnt->BG0_Enable && !(gpu->dispCnt.bits.BG0_3D && (gpu->core==0)) ;
	gpu->LayersEnable[1] = gpu->dispBG[1] && cnt->BG1_Enable;
	gpu->LayersEnable[2] = gpu->dispBG[2] && cnt->BG2_Enable;
	gpu->LayersEnable[3] = gpu->dispBG[3] && cnt->BG3_Enable;
	gpu->LayersEnable[4] = gpu->dispOBJ &&   cnt->OBJ_Enable;

	// KISS ! lower priority first, if same then lower num
	for (i=0;i<NB_PRIORITIES;i++) {
		item = &(gpu->itemsForPriority[i]);
		item->nbBGs=0;
		item->nbPixelsX=0;
	}
	for (i=NB_BG,j=0; i>0; ) {
		i--;
		if (!gpu->LayersEnable[i]) continue;
		prio = gpu->bgCnt[i].bits.Priority;
		item = &(gpu->itemsForPriority[prio]);
		item->BGs[item->nbBGs]=i;
		item->nbBGs++;
		j++;
	}
	
#if 0
//debug
	for (i=0;i<NB_PRIORITIES;i++) {
		item = &(gpu->itemsForPriority[i]);
		printf("%d : ", i);
		for (j=0; j<NB_PRIORITIES; j++) {
			if (j<item->nbBGs) 
				printf("BG%d ", item->BGs[j]);
			else
				printf("... ", item->BGs[j]);
		}
	}
	printf("\n");
#endif
}




/* Sets up LCD control variables for Display Engines A and B for quick reading */
void GPU_setVideoProp(GPU * gpu, u32 p)
{
        BOOL LayersEnable[5];
        u16 WinBG=0;
	struct _DISPCNT * cnt = &gpu->dispCnt.bits;

	gpu->dispCnt.val = p;

//  gpu->dispMode = DISPCNT_DISPLAY_MODE(p,gpu->lcd) ;
    gpu->dispMode = cnt->DisplayMode & ((gpu->core)?1:3);

	switch (gpu->dispMode)
	{
		case 0: // Display Off
			return;
		case 1: // Display BG and OBJ layers
			break;
		case 2: // Display framebuffer
	//              gpu->vramBlock = DISPCNT_VRAMBLOCK(p) ;
			gpu->vramBlock = cnt->VRAM_Block;
			return;
		case 3: // Display from Main RAM
			// nothing to be done here
			// see GPU_ligne who gets data from FIFO.
			return;
	}

        if(cnt->OBJ_Tile_1D)
	{
		/* 1-d sprite mapping */
		/* TODO: better comment (and understanding btw 8S) */
                gpu->sprBlock = 5 + cnt->OBJ_Tile_1D_Bound ;
                if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3))
		{
			gpu->sprBlock = 7;
		}
		gpu->spriteRender = sprite1D;
	} else {
		/* 2d sprite mapping */
		gpu->sprBlock = 5;
		gpu->spriteRender = sprite2D;
	}
     
        if(cnt->OBJ_BMP_1D_Bound && (gpu->core == GPU_MAIN))
	{
		gpu->sprBMPBlock = 8;
	}
	else
	{
		gpu->sprBMPBlock = 7;
	}

	gpu->sprEnable = cnt->OBJ_Enable;
	
	GPU_setBGProp(gpu, 3, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 14));
	GPU_setBGProp(gpu, 2, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 12));
	GPU_setBGProp(gpu, 1, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 10));
	GPU_setBGProp(gpu, 0, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 8));
	
	GPU_resortBGs(gpu);
}

/* this is writing in BGxCNT */
/* FIXME: all DEBUG_TRI are broken */
void GPU_setBGProp(GPU * gpu, u16 num, u16 p)
{
	struct _BGxCNT * cnt = &(gpu->bgCnt[num].bits), *cnt2;
	int mode;
	gpu->bgCnt[num].val = p;
	
	GPU_resortBGs(gpu);
	
    if(gpu->core == GPU_SUB) {
		gpu->BG_tile_ram[num] = ((u8 *)ARM9Mem.ARM9_BBG);
		gpu->BG_bmp_ram[num]  = ((u8 *)ARM9Mem.ARM9_BBG);
		gpu->BG_map_ram[num]  = ARM9Mem.ARM9_BBG;
	} else {
                gpu->BG_tile_ram[num] = ((u8 *)ARM9Mem.ARM9_ABG) +  gpu->dispCnt.bits.CharacBase_Block * ADDRESS_STEP_64kB ;
                gpu->BG_bmp_ram[num]  = ((u8 *)ARM9Mem.ARM9_ABG);
                gpu->BG_map_ram[num]  = ARM9Mem.ARM9_ABG +  gpu->dispCnt.bits.ScreenBase_Block * ADDRESS_STEP_64kB;
	}

	gpu->BG_tile_ram[num] += (cnt->CharacBase_Block * ADDRESS_STEP_16KB);
	gpu->BG_bmp_ram[num]  += (cnt->ScreenBase_Block * ADDRESS_STEP_16KB);
	gpu->BG_map_ram[num]  += (cnt->ScreenBase_Block * ADDRESS_STEP_2KB);

	switch(num)
	{
		case 0:
		case 1:
			gpu->BGExtPalSlot[num] = cnt->PaletteSet_Wrap * 2 + num ;
			break;
			
		default:
			gpu->BGExtPalSlot[num] = (u8)num;
			break;
	}

	mode = mode2type[gpu->dispCnt.bits.BG_Mode][num];
	gpu->BGSize[num][0] = sizeTab[mode][cnt->ScreenSize][0];
	gpu->BGSize[num][1] = sizeTab[mode][cnt->ScreenSize][1];
}

void GPU_remove(GPU * gpu, u8 num)
{
	gpu->dispBG[num] = 0;
	GPU_resortBGs(gpu);
}

void GPU_addBack(GPU * gpu, u8 num)
{
	gpu->dispBG[num] = 1;
	GPU_resortBGs(gpu);
}

void GPU_toggleOBJ(GPU * gpu, u8 disp) {
	gpu->dispOBJ = disp;
}

void GPU_scrollX(GPU * gpu, u8 num, u16 v)
{
	gpu->BGSX[num] = v;
}

void GPU_scrollY(GPU * gpu, u8 num, u16 v)
{
	gpu->BGSY[num] = v;
}

void GPU_scrollXY(GPU * gpu, u8 num, u32 v)
{
	gpu->BGSX[num] = (v & 0xFFFF);
	gpu->BGSY[num] = (v >> 16);
}

void GPU_setX(GPU * gpu, u8 num, u32 v)
{
	gpu->BGX[num] = (((s32)(v<<4))>>4);
}

void GPU_setXH(GPU * gpu, u8 num, u16 v)
{
	gpu->BGX[num] = (((s32)((s16)(v<<4)))<<12) | (gpu->BGX[num]&0xFFFF);
}

void GPU_setXL(GPU * gpu, u8 num, u16 v)
{
	gpu->BGX[num] = (gpu->BGX[num]&0xFFFF0000) | v;
}

void GPU_setY(GPU * gpu, u8 num, u32 v)
{
	gpu->BGY[num] = (((s32)(v<<4))>>4);
}

void GPU_setYH(GPU * gpu, u8 num, u16 v)
{
	gpu->BGY[num] = (((s32)((s16)(v<<4)))<<12) | (gpu->BGY[num]&0xFFFF);
}

void GPU_setYL(GPU * gpu, u8 num, u16 v)
{
	gpu->BGY[num] = (gpu->BGY[num]&0xFFFF0000) | v;
}

void GPU_setPA(GPU * gpu, u8 num, u16 v)
{
	gpu->BGPA[num] = (s32)v;
}

void GPU_setPB(GPU * gpu, u8 num, u16 v)
{
	gpu->BGPB[num] = (s32)v;
}

void GPU_setPC(GPU * gpu, u8 num, u16 v)
{
	gpu->BGPC[num] = (s32)v;
}

void GPU_setPD(GPU * gpu, u8 num, u16 v)
{
	gpu->BGPD[num] = (s32)v;
}

void GPU_setPAPB(GPU * gpu, u8 num, u32 v)
{
	gpu->BGPA[num] = (s16)v;
	gpu->BGPB[num] = (s16)(v>>16);
}

void GPU_setPCPD(GPU * gpu, u8 num, u32 v)
{
	gpu->BGPC[num] = (s16)v;
	gpu->BGPD[num] = (s16)(v>>16);
}

void GPU_setBLDCNT(GPU *gpu, u16 v)
{
    gpu->BLDCNT = v ;
}

void GPU_setBLDALPHA(GPU *gpu, u16 v)
{
	gpu->BLDALPHA = v ;
}

void GPU_setBLDY(GPU *gpu, u16 v)
{
	gpu->BLDY = v ;
}

void GPU_setMOSAIC(GPU *gpu, u16 v)
{
	gpu->MOSAIC = v ;
}

void GPU_setWINDOW_XDIM(GPU *gpu, u16 v, u8 num)
{
	gpu->WINDOW_XDIM[num].val = v ;
}

void GPU_setWINDOW_XDIM_Component(GPU *gpu, u8 v, u8 num)  /* write start/end seperately */
{
	if (num & 1)
	{
		gpu->WINDOW_XDIM[num >> 1].bits.start = v ;
	} else
	{
		gpu->WINDOW_XDIM[num >> 1].bits.end = v ;
	}
}

void GPU_setWINDOW_YDIM(GPU *gpu, u16 v, u8 num)
{
	gpu->WINDOW_YDIM[num].val = v ;
}

void GPU_setWINDOW_YDIM_Component(GPU *gpu, u8 v, u8 num)  /* write start/end seperately */
{
	if (num & 1)
	{
		gpu->WINDOW_YDIM[num >> 1].bits.start = v ;
	} else
	{
		gpu->WINDOW_YDIM[num >> 1].bits.end = v ;
	}
}

void GPU_setWINDOW_INCNT(GPU *gpu, u16 v)
{
	gpu->WINDOW_INCNT.val = v ;
}

void GPU_setWINDOW_INCNT_Component(GPU *gpu, u8 v,u8 num)
{
	switch (num)
	{
		case 0:
			gpu->WINDOW_INCNT.bytes.low = v ;
			break ;
		case 1:
			gpu->WINDOW_INCNT.bytes.high = v ;
			break ;
	}
}

void GPU_setWINDOW_OUTCNT(GPU *gpu, u16 v)
{
	gpu->WINDOW_OUTCNT.val = v ;
}

void GPU_setWINDOW_OUTCNT_Component(GPU *gpu, u8 v,u8 num)
{
	switch (num)
	{
		case 0:
			gpu->WINDOW_OUTCNT.bytes.low = v ;
			break ;
		case 1:
			gpu->WINDOW_OUTCNT.bytes.high = v ;
			break ;
	}
}

void GPU_setMASTER_BRIGHT (GPU *gpu, u16 v)
{
	gpu->masterBright.val = v;
}

/* check whether (x,y) is within the rectangle (including wraparounds) */
INLINE BOOL withinRect (u8 x,u8 y, u16 startX, u16 startY, u16 endX, u16 endY)
{
	BOOL wrapx, wrapy, goodx, goody;
	wrapx = startX > endX;
	wrapy = startY > endY;
	// when the start > end,
	//	all points between start & end are outside the window,
	// otherwise
	//	they are inside 
	goodx = (wrapx)? ((startX <= x)||(x <= endX)):((startX <= x)&&(x <= endX));
	goody = (wrapy)? ((startY <= y)||(y <= endY)):((startY <= y)&&(y <= endY));
	return (goodx && goody);
}


BOOL renderline_checkWindows(GPU *gpu, u8 bgnum, u16 x, u16 y, BOOL *draw, BOOL *effect)
{
	BOOL win0,win1,winOBJ,outwin;
	BOOL wwin0, wwin1, wwobj, wwout, windows;

	// find who owns the BG
	windows=   gpu->dispCnt.bits.Win0_Enable 
		|| gpu->dispCnt.bits.Win1_Enable
		|| gpu->dispCnt.bits.WinOBJ_Enable;

	wwin0 = withinRect(	x,y,
		gpu->WINDOW_XDIM[0].bits.start,gpu->WINDOW_YDIM[0].bits.start,
		gpu->WINDOW_XDIM[0].bits.end,  gpu->WINDOW_YDIM[0].bits.end);
	wwin1 = withinRect(	x,y,
		gpu->WINDOW_XDIM[1].bits.start,gpu->WINDOW_YDIM[1].bits.start,
		gpu->WINDOW_XDIM[1].bits.end,  gpu->WINDOW_YDIM[1].bits.end);
	wwobj = gpu->sprWin[y][x];

	if (windows) {
		wwin0 = wwin0 && gpu->dispCnt.bits.Win0_Enable;
		wwin1 = wwin1 && gpu->dispCnt.bits.Win1_Enable;
		wwobj = wwobj && gpu->dispCnt.bits.WinOBJ_Enable;
		wwout = !(wwin0||wwin1||wwobj);
/*
	// HOW THE HELL THIS DOES NOT WORK !!!

		win0   = (gpu->WINDOW_INCNT.bytes.low   & (1<<bgnum))&&1;
		win1   = (gpu->WINDOW_INCNT.bytes.high  & (1<<bgnum))&&1;
		outwin = (gpu->WINDOW_OUTCNT.bytes.low  & (1<<bgnum))&&1;
		winOBJ = (gpu->WINDOW_OUTCNT.bytes.high & (1<<bgnum))&&1;

	// CHECK THE FOLLOWING, SAME MEANING BUT IT WORKS
*/
		win0   = (gpu->WINDOW_INCNT.bytes.low  >> bgnum)&1;
		win1   = (gpu->WINDOW_INCNT.bytes.high >> bgnum)&1;
		outwin = (gpu->WINDOW_OUTCNT.bytes.low >> bgnum)&1;
		winOBJ = (gpu->WINDOW_OUTCNT.bytes.high>> bgnum)&1;

		// it is in win0, do we display ?
		// high priority
		if (wwin0) {
			*draw   = win0 ;
			*effect = gpu->WINDOW_INCNT.bits.WIN0_Effect_Enable&&1;
			return TRUE;
		}
		// it is in win1, do we display ?
		// mid priority
		if (wwin1) {
			*draw   = win1;
			*effect = gpu->WINDOW_INCNT.bits.WIN1_Effect_Enable&&1;
			return TRUE;
		}
		// it is in winOBJ, do we display ?
		// low priority
		if (wwobj) {
			*draw   = winOBJ;
			*effect = gpu->WINDOW_OUTCNT.bits.WIN1_Effect_Enable&&1;
			return TRUE;
		}
		// it is outside of windows, do we display ?
		// fallback
		if (wwout) {
			*draw   = outwin;
			*effect = gpu->WINDOW_OUTCNT.bits.WIN0_Effect_Enable&&1;
			return TRUE;
		}
	}
	// now windows or no rule
	*draw   = TRUE;
	*effect = TRUE;
	return FALSE;
}

INLINE void renderline_setFinalColor(GPU *gpu,u32 passing,u8 bgnum,u8 *dst,u16 color,u16 x, u16 y) {
	BOOL windowDraw = TRUE, windowEffect = TRUE ;
	/* window priority: insides, if no rule, check outside */
	renderline_checkWindows(gpu,bgnum,x,y,&windowDraw,&windowEffect);

	if ((gpu->BLDCNT & (1 << bgnum)) && (windowEffect==TRUE))   /* the bg to draw has a special color effect */
	{
		switch (gpu->BLDCNT & 0xC0) /* type of special color effect */
		{
			case 0x00:              /* none (plain color passing) */
				T2WriteWord(dst, passing, color) ;
				break ;
			case 0x40:              /* alpha blending */
				{
					//if (!(color & 0x8000)) return ;
					/* we cant do alpha on an invisible pixel */

					u16 sourceFraction = (gpu->BLDALPHA & 0x1F),
						sourceR, sourceG, sourceB,targetFraction;
					if (!sourceFraction) return ;
					/* no fraction of this BG to be showed, so don't do anything */
					sourceR = ((color & 0x1F) * sourceFraction) >> 4 ;
					/* weighted component from color to draw */
					sourceG = (((color>>5) & 0x1F) * sourceFraction) >> 4 ;
					sourceB = (((color>>10) & 0x1F) * sourceFraction) >> 4 ;
					targetFraction = (gpu->BLDALPHA & 0x1F00) >> 8 ;
					if (targetFraction) {
					/* when we dont take any fraction from existing pixel, we can just draw */
						u16 targetR, targetG, targetB;
						color = T2ReadWord(dst, passing) ;
					//if (color & 0x8000) {
						/* the existing pixel is not invisible */
							targetR = ((color & 0x1F) * targetFraction) >> 4 ;  /* weighted component from color we draw on */
							targetG = (((color>>5) & 0x1F) * targetFraction) >> 4 ;
							targetB = (((color>>10) & 0x1F) * targetFraction) >> 4 ;
							sourceR = min(0x1F,targetR+sourceR) ;                   /* limit combined components to 31 max */
							sourceG = min(0x1F,targetG+sourceG) ;
							sourceB = min(0x1F,targetB+sourceB) ;
						//}
					}
					color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
					#undef min
				}
				T2WriteWord(dst, passing, color) ;
				break ;
			case 0x80:               /* brightness increase */
				{
					if (gpu->BLDY != 0x0) { /* dont slow down if there is nothing to do */
						u16 modFraction = (gpu->BLDY & 0x1F) ;
						u16 sourceR = (color & 0x1F) ;
						u16 sourceG = ((color>>5) & 0x1F) ;
						u16 sourceB = ((color>>10) & 0x1F) ;
						sourceR += ((31-sourceR) * modFraction) >> 4 ;
						sourceG += ((31-sourceG) * modFraction) >> 4 ;
						sourceB += ((31-sourceB) * modFraction) >> 4 ;
						color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
					} ;
				}
				T2WriteWord(dst, passing, color) ;
				break ;
			case 0xC0:               /* brightness decrease */
				{
					if (gpu->BLDY!=0) { /* dont slow down if there is nothing to do */
						u16 modFraction = (gpu->BLDY & 0x1F) ;
						u16 sourceR = (color & 0x1F) ;
						u16 sourceG = ((color>>5) & 0x1F) ;
						u16 sourceB = ((color>>10) & 0x1F) ;
						sourceR -= ((sourceR) * modFraction) >> 4 ;
						sourceG -= ((sourceG) * modFraction) >> 4 ;
						sourceB -= ((sourceB) * modFraction) >> 4 ;
						color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
					}
				}
				T2WriteWord(dst, passing, color) ;
				break ;
		}
	} else {
		/* only draw when effect is enabled on this pixel as source, or drawing itself is enabled */
		if (((windowEffect==TRUE) && (gpu->BLDCNT & (0x100 << bgnum))) || (windowDraw == TRUE))
			T2WriteWord(dst, passing, color) ;
	}
} ;

/* render a text background to the combined pixelbuffer */
INLINE void renderline_textBG(GPU * gpu, u8 num, u8 * DST, u32 Y, u16 XBG, u16 YBG, u16 LG)
{
	struct _BGxCNT bgCnt = gpu->bgCnt[num].bits;
	u16 lg     = gpu->BGSize[num][0];
	u16 ht     = gpu->BGSize[num][1];
	u16 tmp    = ((YBG&(ht-1))>>3);
	u8 * map   = gpu->BG_map_ram[num] + (tmp&31) * 64;
	u8 *dst    = DST;
	u8 *tile;
	u16 xoff   = XBG;
	u8 * pal;
	u16 yoff;
	u16 x;

	if(tmp>31) 
	{
		map+= ADDRESS_STEP_512B << bgCnt.ScreenSize ;
	}
	
	tile = (u8*) gpu->BG_tile_ram[num];
	if((!tile) || (!gpu->BG_map_ram[num])) return; 	/* no tiles or no map*/
	xoff = XBG;
	if(!bgCnt.Palette_256)    /* color: 16 palette entries */
	{
		if (bgCnt.Mosaic_Enable){
/* test NDS: #2 of 
   http://desmume.sourceforge.net/forums/index.php?action=vthread&forum=2&topic=50&page=0#msg192 */

			u8 mw = (gpu->MOSAIC & 0xF) +1 ;            /* horizontal granularity of the mosaic */
			u8 mh = ((gpu->MOSAIC>>4) & 0xF) +1 ;       /* vertical granularity of the mosaic */
			YBG = (YBG / mh) * mh ;                         /* align y by vertical granularity */

			yoff = ((YBG&7)<<2);
			pal = ARM9Mem.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB ;
			for(x = 0; x < LG;)
			{
				u8 * mapinfo;
				u16 mapinfovalue;
				u8 *line;
				u16 xfin;
				tmp = ((xoff&(lg-1))>>3);
				mapinfo = map + (tmp&0x1F) * 2;
				if(tmp>31) mapinfo += 32*32*2;
				mapinfovalue = T1ReadWord(mapinfo, 0);

				line = (u8 * )tile + ((mapinfovalue&0x3FF) * 0x20) + (((mapinfovalue)& 0x800 ? (7*4)-yoff : yoff));
				xfin = x + (8 - (xoff&7));
				if (xfin > LG)
					xfin = LG;

	#define RENDERL(c,m) \
		if (c) renderline_setFinalColor(gpu,0,num,dst,T1ReadWord(pal, ((c) + ((mapinfovalue>>12)&0xF) * m) << 1),x,Y) ; \
		dst += 2; x++; xoff++;


				if((mapinfovalue) & 0x400)
				{
					u8 pt = 0 ;
					u8 save = 0;

					line += 3 - ((xoff&7)>>1);
					for(; x < xfin; ) {
// XXX
						if ((pt % mw) == 0) {           /* only update the color we draw every mw pixels */
							if (pt & 1) {
								save = (*line) & 0xF ;
							} else {
								save = (*line) >> 4 ;
							}
						}
						RENDERL(save,0x10)
						pt++ ;
						if (!(pt % mw)) {               /* next pixel next possible color update */
							if (pt & 1) {
								save = (*line) & 0xF ;
							} else {
								save = (*line) >> 4 ;
							}
						}
						RENDERL(save,0x10)
						line--; pt++ ;
					}
				} else {
					u8 pt = 0 ;
					u8 save = 0;
					line += ((xoff&7)>>1);

					for(; x < xfin; ) {
// XXX
						if (!(pt % mw)) {               /* only update the color we draw every n mw pixels */
							if (!(pt & 1)) {
								save = (*line) & 0xF ;
							} else {
								save = (*line) >> 4 ;
							}
						}
						RENDERL(save,0x10)
						pt++ ;
						if (!(pt % mw)) {               /* next pixel next possible color update */
							if (!(pt & 1)) {
								save = (*line) & 0xF ;
							} else {
								save = (*line) >> 4 ;
							}
						}
						RENDERL(save,0x10)
						line++; pt++ ;
					}
				}
			}
		} else {                /* no mosaic mode */

			yoff = ((YBG&7)<<2);
			pal = ARM9Mem.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB ;
			for(x = 0; x < LG;)
			{
				u8 * mapinfo;
				u16 mapinfovalue;
				u8 *line;
				u16 xfin;
				tmp = ((xoff&(lg-1))>>3);
				mapinfo = map + (tmp&0x1F) * 2;
				if(tmp>31) mapinfo += 32*32*2;
				mapinfovalue = T1ReadWord(mapinfo, 0);

				line = (u8 * )tile + ((mapinfovalue&0x3FF) * 0x20) + (((mapinfovalue)& 0x800 ? (7*4)-yoff : yoff));
				xfin = x + (8 - (xoff&7));
				if (xfin > LG)
					xfin = LG;

				if((mapinfovalue) & 0x400)
				{
					line += 3 - ((xoff&7)>>1);
					for(; x < xfin; ) {
// XXX
						RENDERL(((*line)>>4),0x10)
						RENDERL(((*line)&0xF),0x10)
						line--;
					}
				} else {
					line += ((xoff&7)>>1);
					for(; x < xfin; ) {
// XXX
						RENDERL(((*line)&0xF),0x10)
						RENDERL(((*line)>>4),0x10)
						line++;
					}
				}
			}
		}
		return;
	}
	if(!gpu->dispCnt.bits.ExBGxPalette_Enable)  /* color: no extended palette */
	{
		yoff = ((YBG&7)<<3);
		pal = ARM9Mem.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB ;
		for(x = 0; x < LG;)
		{
			u8 * mapinfo;
			u16 mapinfovalue;
			u8 *line;
			u16 xfin;
			tmp = ((xoff&(lg-1))>>3);
			mapinfo = map + (tmp & 31) * 2;
			mapinfovalue;

			if(tmp > 31) mapinfo += 32*32*2;

			mapinfovalue = T1ReadWord(mapinfo, 0);

			line = (u8 * )tile + ((mapinfovalue&0x3FF)*0x40) + (((mapinfovalue)& 0x800 ? (7*8)-yoff : yoff));
			xfin = x + (8 - (xoff&7));
			if (xfin > LG)
				xfin = LG;
			
			if((mapinfovalue)& 0x400)
			{
				line += (7 - (xoff&7));
				for(; x < xfin; )
				{
					RENDERL((*line),0)
					line--;
				}
			} else
			{
				line += (xoff&7);
				for(; x < xfin; )
				{
					RENDERL((*line),0)
					line++;
				}
			}
		}
		return;
	}
													/* color: extended palette */
	pal = ARM9Mem.ExtPal[gpu->core][gpu->BGExtPalSlot[num]];
	if(!pal) return;

	yoff = ((YBG&7)<<3);

	for(x = 0; x < LG;)
	{
		u8 * mapinfo;
		u16 mapinfovalue;
		u8 * line;
		u16 xfin;
		tmp = ((xoff&(lg-1))>>3);
		mapinfo = map + (tmp & 0x1F) * 2;
		mapinfovalue;

		if(tmp>31) mapinfo += 32 * 32 * 2;

		mapinfovalue = T1ReadWord(mapinfo, 0);

		line = (u8 * )tile + ((mapinfovalue&0x3FF)*0x40) + (((mapinfovalue)& 0x800 ? (7*8)-yoff : yoff));
		xfin = x + (8 - (xoff&7));
		if (xfin > LG)
			xfin = LG;
		
		if((mapinfovalue)& 0x400)
		{
			line += (7 - (xoff&7));
			for(; x < xfin;)
			{
				/* this is was adapted */
				RENDERL((*line),0x100)
				line--;
			}
		} else
		{
			line += (xoff&7);
			for(; x < xfin; )
			{
				/* this is was adapted */
				RENDERL((*line),0x100)
				line++;
			}
		}
	}
#undef RENDERL
}

INLINE void rotBG2(GPU * gpu, u8 num, u8 * DST, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG)
{
	struct _BGxCNT bgCnt = gpu->bgCnt[num].bits;

     s32 x = X + (s32)PB*(s32)H;
     s32 y = Y + (s32)PD*(s32)H;


     s32 dx = (s32)PA;
     s32 dy = (s32)PC;

     s32 auxX;
     s32 auxY;

     s32 lg = gpu->BGSize[num][0];
     s32 ht = gpu->BGSize[num][1];
     s32 lgmap = (lg>>3);

     u8 * map = gpu->BG_map_ram[num];
     u8 * tile = (u8 *)gpu->BG_tile_ram[num];
     u8 * dst = DST;
     u8 mapinfo;
     u8 coul;
     u8 * pal;
     u32 i;

     if((!tile)||(!map)) return;

     pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
     for(i = 0; i < LG; ++i)
     {
          auxX = x>>8;
          auxY = y>>8;

          if(bgCnt.PaletteSet_Wrap)
          {
               auxX &= (lg-1);
               auxY &= (ht-1);
          }

          if ((auxX >= 0) && (auxX < lg) && (auxY >= 0) && (auxY < ht))
          {
               mapinfo = map[(auxX>>3) + ((auxY>>3) * lgmap)];
               coul = tile[mapinfo*64 + ((auxY&7)<<3) + (auxX&7)];
               if(coul)
		    		renderline_setFinalColor(gpu,0,num,dst,T1ReadWord(pal, coul << 1),i,X);
          }
          dst += 2;
          x += dx;
          y += dy;
     }
}

INLINE void extRotBG2(GPU * gpu, u8 num, u8 * DST, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, s16 LG)
{
	struct _BGxCNT bgCnt = gpu->bgCnt[num].bits;
	
	s32 x = X + (s32)PB*(s32)H;
	s32 y = Y + (s32)PD*(s32)H;
	
	s32 dx = PA;
	s32 dy = PC;
	
	s32 auxX;
	s32 auxY;
	
	s16 lg = gpu->BGSize[num][0];
	s16 ht = gpu->BGSize[num][1];
	u16 lgmap = (lg>>3);
	
	u8 * tile = (u8 *)gpu->BG_tile_ram[num];
	u8 * dst = DST;
	u8 * map;
	u16 mapinfo, i;
	u8 coul;
	u8 affineModeSelection ;

	/* see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol  */
	affineModeSelection = (bgCnt.Palette_256 << 1) | (bgCnt.CharacBase_Block & 1) ;
	switch(affineModeSelection)
	{
		case 0 :
		case 1 :
		{
		u8 * pal = ARM9Mem.ExtPal[gpu->core][gpu->BGExtPalSlot[num]];
		if(!pal) return;

#define LOOP(c) \
	for(i = 0; i < LG; ++i) { \
		auxX = x>>8; auxY = y>>8; \
		if(bgCnt.PaletteSet_Wrap) { \
			auxX &= (lg-1); auxY &= (ht-1); \
		} \
		if ((auxX >= 0) && (auxX < lg) && (auxY >= 0) && (auxY < ht)) c \
		dst += 2; x += dx; y += dy; \
	} 
		map = gpu->BG_map_ram[num];
		LOOP(
		{
			u16 x1;
			u16 y1;
			mapinfo = T1ReadWord(map, ((auxX>>3) + (auxY>>3) * lgmap) << 1);

			x1 = (mapinfo & 0x400) ? 7 - (auxX&7) : (auxX&7);
			y1 = (mapinfo & 0x800) ? 7 - (auxY&7) : (auxY&7);
			coul = tile[(mapinfo&0x3FF)*64 + x1 + (y1<<3)];
			if(coul)
			renderline_setFinalColor(gpu,0,num,dst, T1ReadWord(pal, (coul + (mapinfo>>12)*0x100) << 1),i,H);
		})
	
		}
		return;
	case 2 :
		{
		u8 * pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		u8 * map = gpu->BG_bmp_ram[num];
		LOOP(
		{
			mapinfo = map[auxX + auxY * lg];
			if(mapinfo)
				renderline_setFinalColor(gpu,0,num,dst, T1ReadWord(pal, mapinfo << 1),i,H);
		})
		}
		return;
	case 3 : /* direct color bitmap */
		{
		u8 * map = gpu->BG_bmp_ram[num];
		LOOP(
		{
			mapinfo = T1ReadWord(map, (auxX + auxY * lg) << 1);
			if ((mapinfo) && (mapinfo & 0x8000))
				renderline_setFinalColor(gpu,0,num,dst, mapinfo,i,H);
		})
		}
		return;
#undef LOOP
     }
}

void lineText(GPU * gpu, u8 num, u16 l, u8 * DST)
{
	renderline_textBG(gpu, num, DST, l, gpu->BGSX[num], l + gpu->BGSY[num], 256);
}

void lineRot(GPU * gpu, u8 num, u16 l, u8 * DST)
{
     rotBG2(gpu, num, DST, l,
              gpu->BGX[num],
              gpu->BGY[num],
              gpu->BGPA[num],
              gpu->BGPB[num],
              gpu->BGPC[num],
              gpu->BGPD[num],
              256);
}

void lineExtRot(GPU * gpu, u8 num, u16 l, u8 * DST)
{
     extRotBG2(gpu, num, DST, l,
              gpu->BGX[num],
              gpu->BGY[num],
              gpu->BGPA[num],
              gpu->BGPB[num],
              gpu->BGPC[num],
              gpu->BGPD[num],
              256);
}

void textBG(GPU * gpu, u8 num, u8 * DST)
{
	u32 i;
	for(i = 0; i < gpu->BGSize[num][1]; ++i)
	{
		renderline_textBG(gpu, num, DST + i*gpu->BGSize[num][0], i, 0, i, gpu->BGSize[num][0]);
	}
}

void rotBG(GPU * gpu, u8 num, u8 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          rotBG2(gpu, num, DST + i*gpu->BGSize[num][0], i, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
}

void extRotBG(GPU * gpu, u8 num, u8 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          extRotBG2(gpu, num, DST + i*gpu->BGSize[num][0], i, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
}






// spriteRender functions !

#define nbShow 128
#define RENDER_COND(cond) RENDER_COND_OFFSET(cond,)
#define RENDER_COND_OFFSET(cond,offset) \
	if((cond)&&(prioTab[sprX offset]>=prio)) \
	{ \
		renderline_setFinalColor(gpu, (sprX offset) << 1,4,dst, color,(sprX offset),l); \
		prioTab[sprX offset] = prio; \
	}

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
INLINE void render_sprite_BMP (GPU * gpu, u16 l, u8 * dst, u16 * src, 
	u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir) {
	int i; u16 color;
	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		color = src[x];
		//RENDER_COND(color,)
		// alpha bit = invisible
		RENDER_COND(color&0x8000)
	}
}

INLINE void render_sprite_256 (GPU * gpu, u16 l, u8 * dst, u8 * src, u16 * pal, 
	u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir) {
	int i; u8 palette_entry; u16 color;
	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		palette_entry = src[(x&0x7) + ((x&0xFFF8)<<3)];
		color = pal[palette_entry];
		// palette entry = 0 means backdrop
		RENDER_COND(palette_entry>0)
	}
}

INLINE void render_sprite_16 (GPU * gpu, u16 l, u8 * dst, u8 * src, u8 * pal, 
	u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir) {
	int i; u8 palette, palette_entry; u16 color;
#define BLOCK(op) \
	palette_entry = palette op; \
	color = T1ReadWord(pal, palette_entry << 1); \
	RENDER_COND(palette_entry>0) \
	++sprX;
#if 1

	if (xdir<0) x++;
	if(x&1)
	{
		s32 x1 = (x>>1);
		palette = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
		BLOCK(>> 4)
		x1 = ((x+lg-1)>>1);
		palette = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
		palette_entry = palette >> 4;
		color = T1ReadWord(pal, palette_entry << 1);
		RENDER_COND_OFFSET(palette_entry>0,+lg-1)
		++sprX;
	}
	if (xdir<0) x--;

	++sprX;
	lg >>= 1;
	x >>= 1;
	// 16 colors

	if (xdir<0) {
		for(i = 0; i < lg; ++i, x+=xdir)
		{
			palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
			BLOCK(>> 4)
			BLOCK(& 0xF)
		}
	} else {
		for(i = 0; i < lg; ++i, x+=xdir)
		{
			palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
			BLOCK(& 0xF)
			BLOCK(>> 4)
		}
	}

#else
	x >>= 1;
	sprX++;

	if (xdir<0) {
		if (lg&1) {
			palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
			BLOCK(>> 4)
			x+=xdir;
		}
		for(i = 1; i < lg; i+=2, x+=xdir)
		{
			palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
			BLOCK(>> 4)
			BLOCK(& 0xF)
		}
	} else {
		for(i = 1; i < lg; i+=2, x+=xdir)
		{
			palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
			BLOCK(& 0xF)
			BLOCK(>> 4)
		}
		if (lg&1) {
			palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
			BLOCK(& 0xF)
		}
	}
#endif
#undef BLOCK
}

INLINE void render_sprite_Win (GPU * gpu, u16 l, u8 * src,
	int col256, int lg, int sprX, int x, int xdir) {
	int i; u8 palette, palette_entry;
	if (col256) {
		for(i = 0; i < lg; i++, sprX++,x+=xdir)
			gpu->sprWin[l][sprX] = (src[x])?1:0;
	} else {

#define BLOCK(op) \
	palette_entry = palette op; \
	gpu->sprWin[l][sprX] = (palette_entry>0)?1:0; \
	++sprX;

		x >>= 1;
		sprX++;	
		if (xdir<0) {
			if (lg&1) {
				palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
				BLOCK(>> 4)
				x+=xdir;
			}
			for(i = 1; i < lg; i+=2, x+=xdir)
			{
				palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
				BLOCK(>> 4)
				BLOCK(& 0xF)
			}
		} else {
			for(i = 1; i < lg; i+=2, x+=xdir)
			{
				palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
				BLOCK(& 0xF)
				BLOCK(>> 4)
			}
			if (lg&1) {
				palette = src[(x&0x3) + ((x&0xFFFC)<<3)];
				BLOCK(& 0xF)
			}
		}
#undef BLOCK
	}
}

void sprite1D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab)
{
	_OAM_ * spriteInfo = (_OAM_ *)(gpu->oam + (nbShow-1));// + 127;
	u8 block = gpu->sprBlock;
	u16 i;
	
	for(i = 0; i<nbShow; ++i, --spriteInfo)     /* check all sprites */
	{
		s32 sprX;
		s32 sprY;
		s32 x = 0;
		u32 lg;
		size sprSize;
		s32 y;
		u8 prio;
		u8 * src;
		u8 * pal;
		u16 i,j;
		int xdir;

		u16 rotScaleA,rotScaleB,rotScaleC,rotScaleD ;
		if (spriteInfo->RotScale & 1) {
			/* if we need to do rotscale, gather its parameters */
			/* http://nocash.emubase.de/gbatek.htm#lcdobjoamrotationscalingparameters */
			rotScaleA = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x06,0) ;
			rotScaleB = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x0E,0) ;
			rotScaleC = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x16,0) ;
			rotScaleD = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x1E,0) ;
		} ;

	
		// get sprite location and size
		sprX = (spriteInfo->X<<23)>>23;
		sprY = spriteInfo->Y;
		sprSize = sprSizeTab[spriteInfo->Size][spriteInfo->Shape];
			
		lg = sprSize.x;

		if(sprY>192)
			sprY = (s32)((s8)(spriteInfo->Y));
	
/* FIXME: for rot/scale, a list of entries into the sprite should be maintained, that tells us where the first pixel */
/* of a screenline starts in the sprite, and how a step to the right in a screenline translates within the sprite */
		if( (spriteInfo->RotScale == 2) ||      /* rotscale == 2 => sprite disabled */
		    (l<sprY)||(l>=sprY+sprSize.y) ||    /* sprite occupies only lines outside the current drawn line */
		    (sprX==256) )                       /* sprite is outside our line */
			continue;                           /* for all these problems, continue with the next sprite */
	
		// sprite portion out of the screen (LEFT)
		if(sprX<0)
		{
			if(sprX+sprSize.x<0) continue; // fully out of screen
			lg += sprX;	
			x = -sprX;
			sprX = 0;
		// sprite portion out of the screen (RIGHT)
		} else if(sprX+sprSize.x >= 256)
			lg = 256 - sprX;

	
		y = l - sprY;                           /* get the y line within sprite coords */
		prio = spriteInfo->Priority;
	
		// switch TOP<-->BOTTOM
		if (spriteInfo->VFlip)
			y = sprSize.y - y -1;
		
		// switch LEFT<-->RIGHT
		if (spriteInfo->HFlip) {
			x = sprSize.x -x -1;
			xdir  = -1;
		} else {
			xdir  = 1;
		}
	
#if 1	
		if (spriteInfo->Mode == 2) {
			if (spriteInfo->Depth)
				src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8);
			else
				src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4);
			render_sprite_Win (gpu, l, src,
				spriteInfo->Depth, lg, sprX, x, xdir);
			continue;
		}
#endif
		if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
		{
			/* sprMemory + sprBlock + 16Bytes per line (8pixels a 2 bytes) */
			src = (gpu->sprMem) + (spriteInfo->TileIndex<<4) + (y<<gpu->sprBMPBlock);

			render_sprite_BMP (gpu, l, dst, src,
				prioTab, prio, lg, sprX, x, xdir);

			continue;
		}
			
		if(spriteInfo->Depth)                   /* 256 colors */
		{
			src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8);
	
			if (gpu->dispCnt.bits.ExOBJPalette_Enable)
				pal = ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200);
			else
				pal = ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400;
	
			render_sprite_256 (gpu, l, dst, src, pal,
				prioTab, prio, lg, sprX, x, xdir);

			continue;
		}
		/* 16 colors */
		src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4);
		pal = ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400;
		
		pal += (spriteInfo->PaletteIndex<<4)<<1;
		render_sprite_16 (gpu, l, dst, src, pal,
			prioTab, prio, lg, sprX, x, xdir);

	}
}

void sprite2D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab)
{
	_OAM_ * spriteInfo = (_OAM_*)(gpu->oam + (nbShow-1));// + 127;
	u16 i;
	
	for(i = 0; i<nbShow; ++i, --spriteInfo)
	{
		s32 sprX;
		s32 sprY;
		s32 x = 0;
		u32 lg;
		size sprSize;
		s32 y;
		u8 prio;
		u8 * src;
		u8 * pal;
		u16 i, j;
		int xdir, block;
	
		u16 rotScaleA,rotScaleB,rotScaleC,rotScaleD ;
		if (spriteInfo->RotScale & 1) {
			/* if we need to do rotscale, gather its parameters */
			/* http://nocash.emubase.de/gbatek.htm#lcdobjoamrotationscalingparameters */
			rotScaleA = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x06,0) ;
			rotScaleB = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x0E,0) ;
			rotScaleC = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x16,0) ;
			rotScaleD = T1ReadWord(gpu->oam + spriteInfo->RotScalIndex*0x20 + 0x1E,0) ;
		} ;

		sprX = (spriteInfo->X<<23)>>23;
		sprY = spriteInfo->Y;
		sprSize = sprSizeTab[spriteInfo->Size][spriteInfo->Shape];
	
		lg = sprSize.x;

		if(sprY>192)
		sprY = (s32)((s8)(spriteInfo->Y));
	
		if( (spriteInfo->RotScale == 2) ||
		    (l<sprY)||(l>=sprY+sprSize.y) ||
		    (sprX==256) )
			continue;
	
		// sprite portion out of the screen (LEFT)
		if(sprX<0)
		{
			if(sprX+sprSize.x<0) continue; // fully out of screen
			lg += sprX;	
			x = -sprX;
			sprX = 0;
		// sprite portion out of the screen (RIGHT)
		} else if(sprX+sprSize.x >= 256)
			lg = 256 - sprX;
	
		y = l - sprY;
		prio = spriteInfo->Priority;
	
		// switch TOP<-->BOTTOM
		if (spriteInfo->VFlip)
			y = sprSize.y - y -1;
		
		// switch LEFT<-->RIGHT
		if (spriteInfo->HFlip) {
			x = sprSize.x -x -1;
			xdir  = -1;
		} else {
			xdir  = 1;
		}
#if 1
		if (spriteInfo->Mode == 2) {
			if (spriteInfo->Depth)
				src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
			else
				src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
			render_sprite_Win (gpu, l, src,
				spriteInfo->Depth, lg, sprX, x, xdir);
			continue;
		}
#endif
		if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
		{
			src = (gpu->sprMem) + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1);
	
			render_sprite_BMP (gpu, l, dst, src,
				prioTab, prio, lg, sprX, x, xdir);

			continue;
		}
			
		if(spriteInfo->Depth)                   /* 256 colors */
		{
			src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
			pal = ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400;
	
			render_sprite_256 (gpu, l, dst, src, pal,
				prioTab, prio, lg, sprX, x, xdir);

			continue;
		}
	
		/* 16 colors */
		src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
		pal = ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400;

		pal += (spriteInfo->PaletteIndex<<4)<<1;
		render_sprite_16 (gpu, l, dst, src, pal,
			prioTab, prio, lg, sprX, x, xdir);
	}
}

void Screen_Init(void) {
        MainScreen.gpu = GPU_Init(0);
        SubScreen.gpu = GPU_Init(1);
}

void Screen_Reset(void) {
   GPU_Reset(MainScreen.gpu, 0);
   GPU_Reset(SubScreen.gpu, 1);
}

void Screen_DeInit(void) {
        GPU_DeInit(MainScreen.gpu);
        GPU_DeInit(SubScreen.gpu);
}
