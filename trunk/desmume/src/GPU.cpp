/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright (C) 2006-2007 Theo Berkau
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

// CONTENTS
//	INITIALIZATION
//	ENABLING / DISABLING LAYERS
//	PARAMETERS OF BACKGROUNDS
//	PARAMETERS OF ROTOSCALE
//	PARAMETERS OF EFFECTS
//	PARAMETERS OF WINDOWS
//	ROUTINES FOR INSIDE / OUTSIDE WINDOW CHECKS
//	PIXEL RENDERING
//	BACKGROUND RENDERING -TEXT-
//	BACKGROUND RENDERING -ROTOSCALE-
//	BACKGROUND RENDERING -HELPER FUNCTIONS-
//	SPRITE RENDERING -HELPER FUNCTIONS-
//	SPRITE RENDERING
//	SCREEN FUNCTIONS
//	GRAPHICS CORE
//	GPU_ligne

#include <algorithm>
#include <string.h>
#include <stdlib.h>
#include <assert.h>
#include <iostream>
#include "MMU.h"
#include "GPU.h"
#include "debug.h"
#include "render3D.h"
#include "GPU_osd.h"

ARM9_struct ARM9Mem;

extern BOOL click;
NDS_Screen MainScreen;
NDS_Screen SubScreen;

//#define DEBUG_TRI

u8 GPU_screen[4*256*192];

OSDCLASS	*osd = NULL;
OSDCLASS	*osdA = NULL;
OSDCLASS	*osdB = NULL;

short sizeTab[4][4][2] =
{
      {{256,256}, {512, 256}, {256, 512}, {512, 512}},
      {{128,128}, {256, 256}, {512, 512}, {1024, 1024}},
      {{128,128}, {256, 256}, {512, 256}, {512, 512}},
      {{512,1024}, {1024, 512}, {0, 0}, {0, 0}},
//      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
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

static GraphicsInterface_struct *GFXCore=NULL;

// This should eventually be moved to the port specific code
GraphicsInterface_struct *GFXCoreList[] = {
&GFXDummy,
NULL
};

//static BOOL setFinalColorDirect				(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialNone		(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialBlend		(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialIncrease	(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialDecrease	(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);

//static BOOL setFinalColorDirectWnd			(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialNoneWnd		(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialBlendWnd	(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialIncreaseWnd (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);
static BOOL setFinalColorSpecialDecreaseWnd (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);


typedef  BOOL (*FinalColFunct)(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y);

FinalColFunct pixelBlitters[8] = {	//setFinalColorDirect,
									setFinalColorSpecialNone,
                                    setFinalColorSpecialBlend,
                                    setFinalColorSpecialIncrease,
                                    setFinalColorSpecialDecrease,

									//setFinalColorDirectWnd,
                                    setFinalColorSpecialNoneWnd,
									setFinalColorSpecialBlendWnd,
                                    setFinalColorSpecialIncreaseWnd,
                                    setFinalColorSpecialDecreaseWnd};



/*****************************************************************************/
//			INITIALIZATION
/*****************************************************************************/

GPU * GPU_Init(u8 l)
{
     GPU * g;

     if ((g = (GPU *) malloc(sizeof(GPU))) == NULL)
        return NULL;

     GPU_Reset(g, l);

	 g->setFinalColorBck = setFinalColorSpecialNone;
	 g->setFinalColorSpr = setFinalColorSpecialNone;

     return g;
}

void GPU_Reset(GPU *g, u8 l)
{
	memset(g, 0, sizeof(GPU));

	g->setFinalColorBck = setFinalColorSpecialNone;
	g->setFinalColorSpr = setFinalColorSpecialNone;
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
		// GPU core B
		g->dispx_st = (REG_DISPx*)(&ARM9Mem.ARM9_REG[REG_DISPB]);
		delete osdB;
		osdB = new OSDCLASS(1);
	}
	else
	{
		g->oam = (OAM *)(ARM9Mem.ARM9_OAM);
		g->sprMem = ARM9Mem.ARM9_AOBJ;
		// GPU core A
		g->dispx_st = (REG_DISPx*)(&ARM9Mem.ARM9_REG[0]);
		delete osdA;
		osdA = new OSDCLASS(0);
	}

	delete osd;
	osd = new OSDCLASS(-1);
}

void GPU_DeInit(GPU * gpu)
{
	delete osd; osd=NULL;
	delete osdA; osdA=NULL;
	delete osdB; osdB=NULL;
   free(gpu);
}

void GPU_resortBGs(GPU *gpu)
{
	int i, j, prio;
	struct _DISPCNT * cnt = &gpu->dispx_st->dispx_DISPCNT.bits;
	itemsForPriority_t * item;

	memset(gpu->sprWin,0, 256*256);

	// we don't need to check for windows here...
// if we tick boxes, invisible layers become invisible & vice versa
#define OP ^ !
// if we untick boxes, layers become invisible
//#define OP &&
	gpu->LayersEnable[0] = gpu->dispBG[0] OP(cnt->BG0_Enable/* && !(cnt->BG0_3D && (gpu->core==0))*/);
	gpu->LayersEnable[1] = gpu->dispBG[1] OP(cnt->BG1_Enable);
	gpu->LayersEnable[2] = gpu->dispBG[2] OP(cnt->BG2_Enable);
	gpu->LayersEnable[3] = gpu->dispBG[3] OP(cnt->BG3_Enable);
	gpu->LayersEnable[4] = gpu->dispOBJ   OP(cnt->OBJ_Enable);

	// KISS ! lower priority first, if same then lower num
	for (i=0;i<NB_PRIORITIES;i++) {
		item = &(gpu->itemsForPriority[i]);
		item->nbBGs=0;
		item->nbPixelsX=0;
	}
	for (i=NB_BG,j=0; i>0; ) {
		i--;
		if (!gpu->LayersEnable[i]) continue;
		prio = (gpu->dispx_st)->dispx_BGxCNT[i].bits.Priority;
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

void GPU_setMasterBrightness (GPU *gpu, u16 val)
{
	u8 temp = (val&31);

	gpu->MasterBrightFactor = (temp > 16 ? 16 : temp);
	gpu->MasterBrightMode	= (val>>14);
}

void SetupFinalPixelBlitter (GPU *gpu)
{
	u8 windowUsed = (gpu->WIN0_ENABLED | gpu->WIN1_ENABLED | gpu->WINOBJ_ENABLED);
	u8 blendMode  = (gpu->BLDCNT >> 6)&3;

	gpu->setFinalColorSpr = pixelBlitters[windowUsed*4 + 1];
	gpu->setFinalColorBck = pixelBlitters[windowUsed*4 + blendMode];
}
    

/* Sets up LCD control variables for Display Engines A and B for quick reading */
void GPU_setVideoProp(GPU * gpu, u32 p)
{
    BOOL LayersEnable[5];
    u16 WinBG=0;
	struct _DISPCNT * cnt;
	cnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;

	T1WriteLong((u8 *)&(gpu->dispx_st)->dispx_DISPCNT.val, 0, p);

	gpu->WIN0_ENABLED	= cnt->Win0_Enable;
	gpu->WIN1_ENABLED	= cnt->Win1_Enable;
	gpu->WINOBJ_ENABLED = cnt->WinOBJ_Enable;

	SetupFinalPixelBlitter (gpu);

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
		// boundary :
		// core A : 32k, 64k, 128k, 256k
		// core B : 32k, 64k, 128k, 128k
                gpu->sprBoundary = 5 + cnt->OBJ_Tile_1D_Bound ;
                if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3))
		{
			gpu->sprBoundary = 7;
		}
		gpu->spriteRender = sprite1D;
	} else {
		/* 2d sprite mapping */
		// boundary : 32k
		gpu->sprBoundary = 5;
		gpu->spriteRender = sprite2D;
	}
     
        if(cnt->OBJ_BMP_1D_Bound && (gpu->core == GPU_MAIN))
	{
		gpu->sprBMPBoundary = 8;
	}
	else
	{
		gpu->sprBMPBoundary = 7;
	}

	gpu->sprEnable = cnt->OBJ_Enable;
	
	GPU_setBGProp(gpu, 3, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 14));
	GPU_setBGProp(gpu, 2, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 12));
	GPU_setBGProp(gpu, 1, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 10));
	GPU_setBGProp(gpu, 0, T1ReadWord(ARM9Mem.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 8));
	
	//GPU_resortBGs(gpu);
}

/* this is writing in BGxCNT */
/* FIXME: all DEBUG_TRI are broken */
void GPU_setBGProp(GPU * gpu, u16 num, u16 p)
{
	struct _BGxCNT * cnt = &((gpu->dispx_st)->dispx_BGxCNT[num].bits), *cnt2;
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	int mode;
	
	T1WriteWord((u8 *)&(gpu->dispx_st)->dispx_BGxCNT[num].val, 0, p);
	
	GPU_resortBGs(gpu);
	
    if(gpu->core == GPU_SUB) {
		gpu->BG_tile_ram[num] = ((u8 *)ARM9Mem.ARM9_BBG);
		gpu->BG_bmp_ram[num]  = ((u8 *)ARM9Mem.ARM9_BBG);
		gpu->BG_map_ram[num]  = ARM9Mem.ARM9_BBG;
	} else {
                gpu->BG_tile_ram[num] = ((u8 *)ARM9Mem.ARM9_ABG) +  dispCnt->CharacBase_Block * ADDRESS_STEP_64kB ;
                gpu->BG_bmp_ram[num]  = ((u8 *)ARM9Mem.ARM9_ABG);
                gpu->BG_map_ram[num]  = ARM9Mem.ARM9_ABG +  dispCnt->ScreenBase_Block * ADDRESS_STEP_64kB;
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

	mode = mode2type[dispCnt->BG_Mode][num];
	gpu->BGSize[num][0] = sizeTab[mode][cnt->ScreenSize][0];
	gpu->BGSize[num][1] = sizeTab[mode][cnt->ScreenSize][1];
}

/*****************************************************************************/
//			ENABLING / DISABLING LAYERS
/*****************************************************************************/

void GPU_remove(GPU * gpu, u8 num)
{
	if (num == 4)	gpu->dispOBJ = 0;
	else		gpu->dispBG[num] = 0;
	GPU_resortBGs(gpu);
}
void GPU_addBack(GPU * gpu, u8 num)
{
	//REG_DISPx_pack_test(gpu);
	if (num == 4)	gpu->dispOBJ = 1;
	else		gpu->dispBG[num] = 1;
	GPU_resortBGs(gpu);
}


/*****************************************************************************/
//		ROUTINES FOR INSIDE / OUTSIDE WINDOW CHECKS
/*****************************************************************************/


/* check whether (x,y) is within the rectangle (including wraparounds) */
static INLINE BOOL withinRect (u8 x,u8 y, u16 startX, u16 startY, u16 endX, u16 endY)
{
	BOOL wrapx, wrapy;
	wrapx = startX > endX;
	wrapy = startY > endY;
	// when the start > end,
	//	all points between start & end are outside the window,
	// otherwise
	//	they are inside
	return ((wrapx? ((startX <= x)||(x <= endX)):((startX <= x)&&(x <= endX))) && 
			(wrapy? ((startY <= y)||(y <= endY)):((startY <= y)&&(y <= endY))));
}


//  Now assumes that *draw and *effect are different from 0 when called, so we can avoid
// setting some values twice
static INLINE void renderline_checkWindows(const GPU *gpu, u8 bgnum, u16 x, u16 y, BOOL *draw, BOOL *effect)
{
	// Check if win0 if enabled, and only check if it is
	if (gpu->WIN0_ENABLED)
	{
		// it is in win0, do we display ?
		// high priority	
		if (withinRect(	x, y, gpu->WIN0H0, gpu->WIN0V0, gpu->WIN0H1, gpu->WIN0V1))
		{
			*draw	= (gpu->WININ0 >> bgnum)&1;
			*effect = (gpu->WININ0_SPECIAL);
			return;
		}
	}

	// Check if win1 if enabled, and only check if it is
	if (gpu->WIN1_ENABLED)
	{
		// it is in win1, do we display ?
		// mid priority
		if (withinRect(	x, y, gpu->WIN1H0, gpu->WIN1V0, gpu->WIN1H1, gpu->WIN1V1))
		{
			*draw	= (gpu->WININ1 >> bgnum)&1;
			*effect = (gpu->WININ1_SPECIAL);
			return;
		}
	}

	if (gpu->WINOBJ_ENABLED)
	{
		// it is in winOBJ, do we display ?
		// low priority
		if (gpu->sprWin[y][x])
		{
			*draw	= (gpu->WINOBJ >> bgnum)&1;
			*effect	= (gpu->WINOBJ_SPECIAL);
			return;
		}
	}

	if (gpu->WINOBJ_ENABLED | gpu->WIN1_ENABLED | gpu->WIN0_ENABLED)
	{
		*draw	= (gpu->WINOUT >> bgnum) & 1;
		*effect	= (gpu->WINOUT_SPECIAL);
	}
}

/*****************************************************************************/
//			PIXEL RENDERING
/*****************************************************************************/

static BOOL setFinalColorSpecialNone (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	T2WriteWord(dst, passing, color);

	return 1;
}

static BOOL setFinalColorSpecialBlend (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	if ((gpu->BLDCNT >> bgnum)&1 && gpu->BLDALPHA_EVA)
	{
		u16 sourceFraction = gpu->BLDALPHA_EVA, sourceR, sourceG, sourceB,targetFraction;
		if (!sourceFraction) return 0;

		// no fraction of this BG to be showed, so don't do anything
		sourceR = ((color & 0x1F) * sourceFraction) >> 4 ;
		// weighted component from color to draw
		sourceG = (((color>>5) & 0x1F) * sourceFraction) >> 4 ;
		sourceB = (((color>>10) & 0x1F) * sourceFraction) >> 4 ;
		targetFraction = gpu->BLDALPHA_EVB;
		if (targetFraction) {
		// when we dont take any fraction from existing pixel, we can just draw
			u16 targetR, targetG, targetB;
			color = T2ReadWord(dst, passing) ;
			//if (color & 0x8000) {
			// the existing pixel is not invisible
				targetR = ((color & 0x1F) * targetFraction) >> 4 ;  // weighted component from color we draw on
				targetG = (((color>>5) & 0x1F) * targetFraction) >> 4 ;
				targetB = (((color>>10) & 0x1F) * targetFraction) >> 4 ;
				// limit combined components to 31 max
				sourceR = std::min(0x1F,targetR+sourceR) ;
				sourceG = std::min(0x1F,targetG+sourceG) ;
				sourceB = std::min(0x1F,targetB+sourceB) ;
			//}
		}
		color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;

		T2WriteWord(dst, passing, color);
	}
	else
	{
		T2WriteWord(dst, passing, color);
	}

	return 1;
}

static BOOL setFinalColorSpecialIncrease (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	if ((gpu->BLDCNT >> bgnum)&1)   // the bg to draw has a special color effect
	{
		if (gpu->BLDY_EVY != 0x0)
		{ // dont slow down if there is nothing to do
			u16 modFraction = gpu->BLDY_EVY;
			u16 sourceR = (color & 0x1F) ;
			u16 sourceG = ((color>>5) & 0x1F) ;
			u16 sourceB = ((color>>10) & 0x1F) ;
			sourceR += ((31-sourceR) * modFraction) >> 4 ;
			sourceG += ((31-sourceG) * modFraction) >> 4 ;
			sourceB += ((31-sourceB) * modFraction) >> 4 ;
			color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
		}

		T2WriteWord(dst, passing, color) ;
	}
	else
	{
		T2WriteWord(dst, passing, color);
	}

	return 1;
}

static BOOL setFinalColorSpecialDecrease (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	if ((gpu->BLDCNT >> bgnum)&1)   // the bg to draw has a special color effect
	{
		if (gpu->BLDY_EVY != 0x0)
		{ // dont slow down if there is nothing to do
			u16 modFraction = gpu->BLDY_EVY;
			u16 sourceR = (color & 0x1F) ;
			u16 sourceG = ((color>>5) & 0x1F) ;
			u16 sourceB = ((color>>10) & 0x1F) ;
			sourceR -= ((sourceR) * modFraction) >> 4 ;
			sourceG -= ((sourceG) * modFraction) >> 4 ;
			sourceB -= ((sourceB) * modFraction) >> 4 ;
			color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
		}
		T2WriteWord(dst, passing, color) ;
	}
	else
	{
		T2WriteWord(dst, passing, color);
	}

	return 1;
}

static BOOL setFinalColorSpecialNoneWnd (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	BOOL windowDraw = TRUE, windowEffect = TRUE;
	
	renderline_checkWindows(gpu,bgnum,x,y, &windowDraw, &windowEffect);

	if (((gpu->BLDCNT >> bgnum)&1) && windowEffect)   // the bg to draw has a special color effect
	{
		T2WriteWord(dst, passing, color);
	}
	else
	{
		if ((windowEffect && (gpu->BLDCNT & (0x100 << bgnum))) || windowDraw)
			T2WriteWord(dst, passing, color);
	}

	return windowDraw;
}

static BOOL setFinalColorSpecialBlendWnd (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	BOOL windowDraw = TRUE, windowEffect = TRUE;
	
	renderline_checkWindows(gpu,bgnum,x,y, &windowDraw, &windowEffect);

	if (((gpu->BLDCNT >> bgnum)&1) && windowEffect)   // the bg to draw has a special color effect
	{
		u16 sourceFraction = gpu->BLDALPHA_EVA,
			sourceR, sourceG, sourceB,targetFraction;
		if (!sourceFraction) 
			return 0;
		// no fraction of this BG to be showed, so don't do anything
		sourceR = ((color & 0x1F) * sourceFraction) >> 4 ;
		// weighted component from color to draw
		sourceG = (((color>>5) & 0x1F) * sourceFraction) >> 4 ;
		sourceB = (((color>>10) & 0x1F) * sourceFraction) >> 4 ;
		targetFraction = gpu->BLDALPHA_EVB;
		if (targetFraction) {
		// when we dont take any fraction from existing pixel, we can just draw
			u16 targetR, targetG, targetB;
			color = T2ReadWord(dst, passing) ;
			//if (color & 0x8000) {
			// the existing pixel is not invisible
				targetR = ((color & 0x1F) * targetFraction) >> 4 ;  // weighted component from color we draw on
				targetG = (((color>>5) & 0x1F) * targetFraction) >> 4 ;
				targetB = (((color>>10) & 0x1F) * targetFraction) >> 4 ;
				// limit combined components to 31 max
				sourceR = std::min(0x1F,targetR+sourceR) ;
				sourceG = std::min(0x1F,targetG+sourceG) ;
				sourceB = std::min(0x1F,targetB+sourceB) ;
			//}
		}
		color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;

		T2WriteWord(dst, passing, color);
	}
	else
	{
		if ((windowEffect && (gpu->BLDCNT & (0x100 << bgnum))) || windowDraw)
			T2WriteWord(dst, passing, color);
	}

	return windowDraw;
}

static BOOL setFinalColorSpecialIncreaseWnd (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	BOOL windowDraw = TRUE, windowEffect = TRUE;
	
	renderline_checkWindows(gpu,bgnum,x,y, &windowDraw, &windowEffect);

	if (((gpu->BLDCNT >> bgnum)&1) && windowEffect)   // the bg to draw has a special color effect
	{
		if (gpu->BLDY_EVY != 0x0)
		{ // dont slow down if there is nothing to do
			u16 modFraction = gpu->BLDY_EVY;
			u16 sourceR = (color & 0x1F) ;
			u16 sourceG = ((color>>5) & 0x1F) ;
			u16 sourceB = ((color>>10) & 0x1F) ;
			sourceR += ((31-sourceR) * modFraction) >> 4 ;
			sourceG += ((31-sourceG) * modFraction) >> 4 ;
			sourceB += ((31-sourceB) * modFraction) >> 4 ;
			color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
		}

		T2WriteWord(dst, passing, color) ;
	}
	else
	{
		if ((windowEffect && (gpu->BLDCNT & (0x100 << bgnum))) || windowDraw)
			T2WriteWord(dst, passing, color);
	}

	return windowDraw;
}

static BOOL setFinalColorSpecialDecreaseWnd (const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, u16 y)
{
	BOOL windowDraw = TRUE, windowEffect = TRUE;
	
	renderline_checkWindows(gpu,bgnum,x,y, &windowDraw, &windowEffect);

	if (((gpu->BLDCNT >> bgnum)&1) && windowEffect)   // the bg to draw has a special color effect
	{
		if (gpu->BLDY_EVY != 0x0)
		{ // dont slow down if there is nothing to do
			u16 modFraction = gpu->BLDY_EVY;
			u16 sourceR = (color & 0x1F) ;
			u16 sourceG = ((color>>5) & 0x1F) ;
			u16 sourceB = ((color>>10) & 0x1F) ;
			sourceR -= ((sourceR) * modFraction) >> 4 ;
			sourceG -= ((sourceG) * modFraction) >> 4 ;
			sourceB -= ((sourceB) * modFraction) >> 4 ;
			color = (sourceR & 0x1F) | ((sourceG & 0x1F) << 5) | ((sourceB & 0x1F) << 10) | 0x8000 ;
		}
		T2WriteWord(dst, passing, color) ;
	}
	else
	{
		if ((windowEffect && (gpu->BLDCNT & (0x100 << bgnum))) || windowDraw)
			T2WriteWord(dst, passing, color);
	}

	return windowDraw;
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/

/* render a text background to the combined pixelbuffer */
INLINE void renderline_textBG(const GPU * gpu, u8 num, u8 * dst, u32 Y, u16 XBG, u16 YBG, u16 LG)
{
	struct _BGxCNT * bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	u16 lg     = gpu->BGSize[num][0];
	u16 ht     = gpu->BGSize[num][1];
	u16 tmp    = ((YBG&(ht-1))>>3);
	u8 *map    = gpu->BG_map_ram[num] + (tmp&31) * 64;
	u8 *tile, *pal, *line;

	u16 color;
	u16 xoff   = XBG;
	u16 yoff;
	u16 x      = 0;
	u16 xfin;
	u16 palette_size;
	u16 mosaic = T1ReadWord((u8 *)&gpu->dispx_st->dispx_MISC.MOSAIC, 0);

	s8 line_dir = 1;
	u8 pt_xor   = 0;
	u8 * mapinfo;
	TILEENTRY tileentry;

	if(tmp>31) 
	{
		map+= ADDRESS_STEP_512B << bgCnt->ScreenSize ;
	}
	
	tile = (u8*) gpu->BG_tile_ram[num];
	if((!tile) || (!gpu->BG_map_ram[num])) return; 	/* no tiles or no map*/
	xoff = XBG;
	pal = ARM9Mem.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB ;

	if(!bgCnt->Palette_256)    /* color: 16 palette entries */
	{
		if (bgCnt->Mosaic_Enable){
/* test NDS: #2 of 
   http://desmume.sourceforge.net/forums/index.php?action=vthread&forum=2&topic=50&page=0#msg192 */

			u8 mw = (mosaic & 0xF) +1 ;            /* horizontal granularity of the mosaic */
			u8 mh = ((mosaic>>4) & 0xF) +1 ;       /* vertical granularity of the mosaic */
			YBG = (YBG / mh) * mh ;                         /* align y by vertical granularity */
			yoff = ((YBG&7)<<2);

			xfin = 8 - (xoff&7);
			for(x = 0; x < LG; xfin = std::min<u16>(x+8, LG))
			{
				u8 pt = 0, save = 0;
				tmp = ((xoff&(lg-1))>>3);
				mapinfo = map + (tmp&0x1F) * 2;
				if(tmp>31) mapinfo += 32*32*2;
				tileentry.val = T1ReadWord(mapinfo, 0);

				line = (u8*)tile + (tileentry.bits.TileNum * 0x20) + ((tileentry.bits.VFlip)? (7*4)-yoff:yoff);

				if(tileentry.bits.HFlip)
				{
					line += 3 - ((xoff&7)>>1);
					line_dir = -1;
					pt_xor = 0;
				} else {
					line += ((xoff&7)>>1);
					line_dir = 1;
					pt_xor = 1;
				}
// XXX
				for(; x < xfin; ) {
					if (!(pt % mw)) {               /* only update the color we draw every n mw pixels */
						if ((pt & 1)^pt_xor) {
							save = (*line) & 0xF ;
						} else {
							save = (*line) >> 4 ;
						}
					}
					
					color = T1ReadWord(pal, ((save) + (tileentry.bits.Palette*16)) << 1);
					if (save) 
						gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
					dst += 2; 
					x++; 
					xoff++;

					pt++ ;
					if (!(pt % mw)) {               /* next pixel next possible color update */
						if ((pt & 1)^pt_xor) {
							save = (*line) & 0xF ;
						} else {
							save = (*line) >> 4 ;
						}
					}

					color = T1ReadWord(pal, ((save) + (tileentry.bits.Palette*16)) << 1);
					if (save) 
						gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
					dst += 2; 
					x++; 
					xoff++;

					line+=line_dir; pt++ ;
				}
			}
		} else {                /* no mosaic mode */
			yoff = ((YBG&7)<<2);
			xfin = 8 - (xoff&7);
			for(x = 0; x < LG; xfin = std::min<u16>(x+8, LG))
			{
				u16 tilePalette = 0;
				tmp = ((xoff&(lg-1))>>3);
				mapinfo = map + (tmp&0x1F) * 2;
				if(tmp>31) mapinfo += 32*32*2;
				tileentry.val = T1ReadWord(mapinfo, 0);

				tilePalette = (tileentry.bits.Palette*16);

				line = (u8 * )tile + (tileentry.bits.TileNum * 0x20) + ((tileentry.bits.VFlip) ? (7*4)-yoff : yoff);				
				
				if(tileentry.bits.HFlip)
				{
					line += (3 - ((xoff&7)>>1));
					for(; x < xfin; line --) 
					{	
						u8 currLine = *line;

						if (currLine>>4) 
						{
							color = T1ReadWord(pal, ((currLine>>4) + tilePalette) << 1);
							gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
						}
						dst += 2; x++; xoff++;
						
						if (currLine&0xF) 
						{
							color = T1ReadWord(pal, ((currLine&0xF) + tilePalette) << 1);
							gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
						}
						dst += 2; x++; xoff++;
					}
				} else {
					line += ((xoff&7)>>1);
					for(; x < xfin; line ++) 
					{
						u8 currLine = *line;

						if (currLine&0xF) 
						{
							color = T1ReadWord(pal, ((currLine&0xF) + tilePalette) << 1);
							gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
						}

						dst += 2; x++; xoff++;

						if (currLine>>4) 
						{
							color = T1ReadWord(pal, ((currLine>>4) + tilePalette) << 1);
							gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
						}
						dst += 2; x++; xoff++;
					}
				}
			}
		}
		return;
	}

	palette_size=0; /* color: no extended palette */
	if(dispCnt->ExBGxPalette_Enable)  /* color: extended palette */
	{
		palette_size=0x100;
		pal = ARM9Mem.ExtPal[gpu->core][gpu->BGExtPalSlot[num]];
		if(!pal) return;
	}

	yoff = ((YBG&7)<<3);
	xfin = 8 - (xoff&7);
	for(x = 0; x < LG; xfin = std::min<u16>(x+8, LG))
	{
		tmp = (xoff & (lg-1))>>3;
		mapinfo = map + (tmp & 31) * 2;
		if(tmp > 31) mapinfo += 32*32*2;
		tileentry.val = T1ReadWord(mapinfo, 0);

		line = (u8 * )tile + (tileentry.bits.TileNum*0x40) + ((tileentry.bits.VFlip) ? (7*8)-yoff : yoff);

		if(tileentry.bits.HFlip)
		{
			line += (7 - (xoff&7));
			line_dir = -1;
		} else {
			line += (xoff&7);
			line_dir = 1;
		}
		for(; x < xfin; )
		{
			color = T1ReadWord(pal, ((*line) + (tileentry.bits.Palette*palette_size)) << 1);
			if (*line) 
				gpu->setFinalColorBck(gpu,0,num,dst,color,x,Y);
			dst += 2; x++; xoff++;

			line += line_dir;
		}
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -ROTOSCALE-
/*****************************************************************************/

void rot_tiled_8bit_entry(GPU * gpu, int num, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H) {
	u8 palette_entry;
	u16 tileindex, x, y, color;
	
	tileindex = map[(auxX>>3) + (auxY>>3) * (lg>>3)];
	x = (auxX&7); y = (auxY&7);

	palette_entry = tile[(tileindex<<6)+(y<<3)+x];
	color = T1ReadWord(pal, palette_entry << 1);
	if (palette_entry)
		gpu->setFinalColorBck(gpu,0,num,dst, color,auxX,auxY);
}

void rot_tiled_16bit_entry(GPU * gpu, int num, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H) {
	u8 palette_entry, palette_set;
	u16 tileindex, x, y, color;
	TILEENTRY tileentry;

	if (!tile) return;
	tileentry.val = T1ReadWord(map, ((auxX>>3) + (auxY>>3) * (lg>>3))<<1);
	x = (tileentry.bits.HFlip) ? 7 - (auxX&7) : (auxX&7);
	y = (tileentry.bits.VFlip) ? 7 - (auxY&7) : (auxY&7);

	palette_entry = tile[(tileentry.bits.TileNum<<6)+(y<<3)+x];
	color = T1ReadWord(pal, (palette_entry + (tileentry.bits.Palette<<8)) << 1);
	if (palette_entry>0)
		gpu->setFinalColorBck(gpu,0,num,dst, color, i, H);
}

void rot_256_map(GPU * gpu, int num, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H) {
	u8 palette_entry;
	u16 tileindex, color;

//	return;

	palette_entry = map[auxX + auxY * lg];
	color = T1ReadWord(pal, palette_entry << 1);
	if(palette_entry)
		gpu->setFinalColorBck(gpu,0,num,dst, color, i, H);

}

void rot_BMP_map(GPU * gpu, int num, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H) {
	u16 color;

//	return;

	color = T1ReadWord(map, (auxX + auxY * lg) << 1);
	if (color&0x8000)
		gpu->setFinalColorBck(gpu,0,num,dst, color, i, H);

}

typedef void (*rot_fun)(GPU * gpu, int num, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal , int i, u16 H);

INLINE void rot_scale_op(GPU * gpu, u8 num, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG, s32 wh, s32 ht, BOOL wrap, rot_fun fun, u8 * map, u8 * tile, u8 * pal)
{
	ROTOCOORD x, y;

	s32 dx = (s32)PA;
	s32 dy = (s32)PC;
	u32 i;
	s32 auxX, auxY;
	
	x.val = X + (s32)PB*(s32)H;
	y.val = Y + (s32)PD*(s32)H;

	for(i = 0; i < LG; ++i)
	{
		//auxX = x.val >> 8;
		//auxY = y.val >> 8;
		auxX = x.bits.Integer;
		auxY = y.bits.Integer;
	
		if(wrap)
		{
			// wrap
			auxX = auxX & (wh-1);
			auxY = auxY & (ht-1);
		}
	
		if ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht))
			fun(gpu, num, auxX, auxY, wh, dst, map, tile, pal, i, H);
		dst += 2;
		x.val += dx;
		y.val += dy;
	}
}

INLINE void apply_rot_fun(GPU * gpu, u8 num, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG, rot_fun fun, u8 * map, u8 * tile, u8 * pal)
{
	struct _BGxCNT * bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	s32 wh = gpu->BGSize[num][0];
	s32 ht = gpu->BGSize[num][1];
	rot_scale_op(gpu, num, dst, H, X, Y, PA, PB, PC, PD, LG, wh, ht, bgCnt->PaletteSet_Wrap, fun, map, tile, pal);	
}


INLINE void rotBG2(GPU * gpu, u8 num, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG) {
	u8 * map = gpu->BG_map_ram[num];
	u8 * tile = (u8 *)gpu->BG_tile_ram[num];
	u8 * pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
//	printf("rot mode\n");
	apply_rot_fun(gpu, num, dst, H,X,Y,PA,PB,PC,PD,LG, rot_tiled_8bit_entry, map, tile, pal);
}

INLINE void extRotBG2(GPU * gpu, u8 num, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, s16 LG)
{
	struct _BGxCNT * bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	
	u8 *map, *tile, *pal;
	u8 affineModeSelection ;

	/* see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol  */
	affineModeSelection = (bgCnt->Palette_256 << 1) | (bgCnt->CharacBase_Block & 1) ;
//	printf("extrot mode %d\n", affineModeSelection);
	switch(affineModeSelection)
	{
	case 0 :
	case 1 :
		map = gpu->BG_map_ram[num];
		tile = gpu->BG_tile_ram[num];
		pal = ARM9Mem.ExtPal[gpu->core][gpu->BGExtPalSlot[num]];
		if (!pal) return;

		// 16  bit bgmap entries
		apply_rot_fun(gpu, num, dst, H,X,Y,PA,PB,PC,PD,LG, rot_tiled_16bit_entry, map, tile, pal);
		return;
	case 2 :
		// 256 colors 
		map = gpu->BG_bmp_ram[num];
		pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		apply_rot_fun(gpu, num, dst, H,X,Y,PA,PB,PC,PD,LG, rot_256_map, map, NULL, pal);
		return;
	case 3 :
		// direct colors / BMP
		map = gpu->BG_bmp_ram[num];
		apply_rot_fun(gpu, num, dst, H,X,Y,PA,PB,PC,PD,LG, rot_BMP_map, map, NULL, NULL);
		return;
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

void lineText(GPU * gpu, u8 num, u16 l, u8 * DST)
{
	BGxOFS * ofs = &gpu->dispx_st->dispx_BGxOFS[num];
	renderline_textBG(gpu, num, DST, l, T1ReadWord((u8 *)&ofs->BGxHOFS, 0), l + T1ReadWord((u8 *)&ofs->BGxVOFS, 0), 256);
}

void lineRot(GPU * gpu, u8 num, u16 l, u8 * DST)
{	
	BGxPARMS * parms;
	if (num==2) {
		parms = &(gpu->dispx_st)->dispx_BG2PARMS;
	} else {
		parms = &(gpu->dispx_st)->dispx_BG3PARMS;		
	}
     rotBG2(gpu, num, DST, l,
              parms->BGxX,
              parms->BGxY,
              parms->BGxPA,
              parms->BGxPB,
              parms->BGxPC,
              parms->BGxPD,
              256);
}

void lineExtRot(GPU * gpu, u8 num, u16 l, u8 * DST)
{
	BGxPARMS * parms;
	if (num==2) {
		parms = &(gpu->dispx_st)->dispx_BG2PARMS;
	} else {
		parms = &(gpu->dispx_st)->dispx_BG3PARMS;		
	}
     extRotBG2(gpu, num, DST, l,
              parms->BGxX,
              parms->BGxY,
              parms->BGxPA,
              parms->BGxPB,
              parms->BGxPC,
              parms->BGxPD,
              256);
}

void textBG(const GPU * gpu, u8 num, u8 * DST)
{
	u32 i;
	for(i = 0; i < gpu->BGSize[num][1]; ++i)
	{
		renderline_textBG(gpu, num, DST + i*gpu->BGSize[num][0]*2, i, 0, i, gpu->BGSize[num][0]);
	}
}

void rotBG(GPU * gpu, u8 num, u8 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          rotBG2(gpu, num, DST + i*gpu->BGSize[num][0]*2, i, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
}

void extRotBG(GPU * gpu, u8 num, u8 * DST)
{
     u32 i;
     for(i = 0; i < gpu->BGSize[num][1]; ++i)
          extRotBG2(gpu, num, DST + i*gpu->BGSize[num][0]*2, i, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
}





/*****************************************************************************/
//			SPRITE RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

#define nbShow 128

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
INLINE void render_sprite_BMP (GPU * gpu, u16 l, u8 * dst, u16 * src, u8 * prioTab, 
							   u8 prio, int lg, int sprX, int x, int xdir) 
{
	int i; u16 color;
	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		color = LE_TO_LOCAL_16(src[x]);

		// alpha bit = invisible
		if ((color&0x8000)&&(prio<=prioTab[sprX]))
		{
			/* if we don't draw, do not set prio, or else */
			if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, color, sprX ,l))
				prioTab[sprX] = prio;
		}
	}
}


INLINE void render_sprite_256 (	GPU * gpu, u16 l, u8 * dst, u8 * src, u16 * pal, 
								u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir, u8 alpha)
{
	int i; 
	u8 palette_entry; 
	u16 color, oldBLDCNT;

	if (alpha)
	{
		oldBLDCNT = gpu->BLDCNT;
		gpu->BLDCNT = gpu->BLDCNT | 0x10;
	}

	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		palette_entry = src[(x&0x7) + ((x&0xFFF8)<<3)];
		color = LE_TO_LOCAL_16(pal[palette_entry]);

		// palette entry = 0 means backdrop
		if ((palette_entry>0)&&(prio<=prioTab[sprX]))
		{
			/* if we don't draw, do not set prio, or else */
			if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, color, sprX ,l))
				prioTab[sprX] = prio;
		}
	}

	if (alpha)
	{
		gpu->BLDCNT = oldBLDCNT;
	}
}

INLINE void render_sprite_16 (	GPU * gpu, u16 l, u8 * dst, u8 * src, u16 * pal, 
								u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir, u8 alpha)
{
	int i; 
	u8 palette, palette_entry;
	u16 color, oldBLDCNT, x1;

	if (alpha)
	{
		oldBLDCNT = gpu->BLDCNT;
		gpu->BLDCNT = gpu->BLDCNT | 0x10;
	}

	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		x1 = x>>1;
		palette = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
		if (x & 1) palette_entry = palette >> 4;
		else       palette_entry = palette & 0xF;
		color = LE_TO_LOCAL_16(pal[palette_entry]);

		// palette entry = 0 means backdrop
		if ((palette_entry>0)&&(prio<=prioTab[sprX]))
		{
			/* if we don't draw, do not set prio, or else */
			if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, color, sprX ,l))
				prioTab[sprX] = prio;
		}
	}

	if (alpha)
	{
		gpu->BLDCNT = oldBLDCNT;
	}
}

INLINE void render_sprite_Win (GPU * gpu, u16 l, u8 * src,
	int col256, int lg, int sprX, int x, int xdir) {
	int i; u8 palette, palette_entry;
	u16 x1;
	if (col256) {
		for(i = 0; i < lg; i++, sprX++,x+=xdir)
			gpu->sprWin[l][sprX] = (src[x])?1:0;
	} else {
		for(i = 0; i < lg; i++, ++sprX, x+=xdir)
		{
			x1 = x>>1;
			palette = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
			if (x & 1) palette_entry = palette >> 4;
			else       palette_entry = palette & 0xF;
			gpu->sprWin[l][sprX] = (palette_entry)?1:0;
		}
	}
}


// return val means if the sprite is to be drawn or not
INLINE BOOL compute_sprite_vars(_OAM_ * spriteInfo, u16 l, 
	size *sprSize, s32 *sprX, s32 *sprY, s32 *x, s32 *y, s32 *lg, int *xdir) {

	*x = 0;
	// get sprite location and size
	*sprX = (spriteInfo->X<<23)>>23;
	*sprY = spriteInfo->Y;
	*sprSize = sprSizeTab[spriteInfo->Size][spriteInfo->Shape];

	*lg = sprSize->x;
	
	if (*sprY>=192)
		*sprY = (s32)((s8)(spriteInfo->Y));
	
// FIXME: for rot/scale, a list of entries into the sprite should be maintained,
// that tells us where the first pixel of a screenline starts in the sprite,
// and how a step to the right in a screenline translates within the sprite

	if ((l<*sprY)||(l>=*sprY+sprSize->y) ||	/* sprite lines outside of screen */
		(*sprX==256)||(*sprX+sprSize->x<=0))	/* sprite pixels outside of line */
		return FALSE;				/* not to be drawn */

	// sprite portion out of the screen (LEFT)
	if(*sprX<0)
	{
		*lg += *sprX;	
		*x = -(*sprX);
		*sprX = 0;
	}
	// sprite portion out of the screen (RIGHT)
	if (*sprX+sprSize->x >= 256)
		*lg = 256 - *sprX;

	*y = l - *sprY;                           /* get the y line within sprite coords */

	// switch TOP<-->BOTTOM
	if (spriteInfo->VFlip)
		*y = sprSize->y - *y -1;
	
	// switch LEFT<-->RIGHT
	if (spriteInfo->HFlip) {
		*x = sprSize->x - *x -1;
		*xdir  = -1;
	} else {
		*xdir  = 1;
	}
	return TRUE;
}


/*****************************************************************************/
//			SPRITE RENDERING
/*****************************************************************************/

void sprite1D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab)
{
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	_OAM_ * spriteInfo = (_OAM_ *)(gpu->oam + (nbShow-1));// + 127;
	u8 block = gpu->sprBoundary;
	u16 i;
	
	//for(i = 0; i<nbShow; ++i, --spriteInfo)     /* check all sprites */
#ifdef WORDS_BIGENDIAN
	*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) >> 1) | *(((u16*)spriteInfo)+1) << 15;
	*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) >> 2) | *(((u16*)spriteInfo)+2) << 14;
#endif

	for(i = 0; i<nbShow; ++i, --spriteInfo
#ifdef WORDS_BIGENDIAN    
	,*(((u16*)(spriteInfo+1))+1) = (*(((u16*)(spriteInfo+1))+1) << 1) | *(((u16*)(spriteInfo+1))+1) >> 15
	,*(((u16*)(spriteInfo+1))+2) = (*(((u16*)(spriteInfo+1))+2) << 2) | *(((u16*)(spriteInfo+1))+2) >> 14
	,*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) >> 1) | *(((u16*)spriteInfo)+1) << 15
	,*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) >> 2) | *(((u16*)spriteInfo)+2) << 14
#endif
	)     /* check all sprites */
	{
		size sprSize;
		s32 sprX, sprY, x, y, lg;
		int xdir;
		u8 prio, * src;
		u16 i;

		// Check if sprite is disabled before everything
		if (spriteInfo->RotScale == 2)
			continue;

		prio = spriteInfo->Priority;

		if (spriteInfo->RotScale & 1) 
		{
			s32		fieldX, fieldY, auxX, auxY, realX, realY, offset;
			u8		blockparameter, *pal;
			s16		dx, dmx, dy, dmy;
			u16		colour;

			// Get sprite positions and size
			sprX = (spriteInfo->X<<23)>>23;
			sprY = spriteInfo->Y;
			sprSize = sprSizeTab[spriteInfo->Size][spriteInfo->Shape];

			lg = sprSize.x;
			
			if (sprY>=192)
				sprY = (s32)((s8)(spriteInfo->Y));

			// Copy sprite size, to check change it if needed
			fieldX = sprSize.x;
			fieldY = sprSize.y;

			// If we are using double size mode, double our control vars
			if (spriteInfo->RotScale & 2)
			{
				fieldX <<= 1;
				fieldY <<= 1;
				lg <<= 1;
			}

			// Check if sprite enabled
			if ((l   <sprY) || (l >= sprY+fieldY) ||
				(sprX==256) || (sprX+fieldX<=0))
				continue;

			y = l - sprY;

			// Get which four parameter block is assigned to this sprite
			blockparameter = (spriteInfo->RotScalIndex + (spriteInfo->HFlip<< 3) + (spriteInfo->VFlip << 4))*4;

			// Get rotation/scale parameters
			dx  = (s16)(gpu->oam + blockparameter+0)->attr3;
			dmx = (s16)(gpu->oam + blockparameter+1)->attr3;
			dy  = (s16)(gpu->oam + blockparameter+2)->attr3;
			dmy = (s16)(gpu->oam + blockparameter+3)->attr3;

			// Calculate fixed poitn 8.8 start offsets
			realX = ((sprSize.x) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx + y * dmx;
			realY = ((sprSize.y) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy + y * dmy;

			if(sprX<0)
			{
				// If sprite is not in the window
				if(sprX + fieldX <= 0)
					continue;

				// Otherwise, is partially visible
				lg += sprX;
				realX -= sprX*dx;
				realY -= sprX*dy;
				sprX = 0;
			}
			else
			{
				if(sprX+fieldX>256)
					lg = 255 - sprX;
			}

			// If we are using 1 palette of 256 colours
			if(spriteInfo->Depth)
			{
				src = gpu->sprMem + (spriteInfo->TileIndex << block);

				// If extended palettes are set, use them
				if (dispCnt->ExOBJPalette_Enable)
					pal = (ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);

				for(i = 0; i < lg; ++i, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)*sprSize.x*8) + ((auxY&0x7)*8);
						colour = src[offset];

						if (colour && (prioTab[sprX]>=prio))
						{ 
							if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, T1ReadWord(pal, colour<<1), sprX ,l))
								prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}

				continue;
			}
			// Rotozoomed direct color
			else if(spriteInfo->Mode == 3)
			{
				src = gpu->sprMem + (spriteInfo->TileIndex)*32;

				for(i = 0; i < lg; ++i, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						offset = (auxX) + (auxY<<5);
						colour = T1ReadWord (src, offset<<1);

						if((colour&0x8000) && (prioTab[sprX]>=prio))
						{
							if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, colour, sprX ,l))
								prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}

				continue;
			}
			// Rotozoomed 16/16 palette
			else
			{
				pal = ARM9Mem.ARM9_VMEM + 0x200 + gpu->core*0x400 + (spriteInfo->PaletteIndex*32);
				src = gpu->sprMem + (spriteInfo->TileIndex<<gpu->sprBoundary);

				for(i = 0; i < lg; ++i, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)*sprSize.x)*4 + ((auxY&0x7)*4);
						colour = src[offset];

						// Get 4bits value from the readed 8bits
						if (auxX&1)	colour >>= 4;
						else		colour &= 0xF;

						if(colour && (prioTab[sprX]>=prio))
						{
							if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, T1ReadWord(pal, colour<<1), sprX ,l))
								prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}

				continue;
			}
		}
		else
		{
			u16 * pal;

			if (!compute_sprite_vars(spriteInfo, l, &sprSize, &sprX, &sprY, &x, &y, &lg, &xdir))
				continue;

			if (spriteInfo->Mode == 2)
			{
				if (spriteInfo->Depth)
					src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8);
				else
					src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4);

				render_sprite_Win (gpu, l, src, spriteInfo->Depth, lg, sprX, x, xdir);
				continue;
			}

			if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
			{
					/* sprMemory + sprBoundary + 16Bytes per line (8pixels a 2 bytes) */
					//src = (gpu->sprMem) + (spriteInfo->TileIndex<<4) + (y<<gpu->sprBMPBoundary);
				// FIXME:this no correct, but work ???
				src = (gpu->sprMem) + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1);
				render_sprite_BMP (gpu, l, dst, (u16*)src, prioTab, prio, lg, sprX, x, xdir);

				continue;
			}
				
			if(spriteInfo->Depth)                   /* 256 colors */
			{
				src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8);
		
				if (dispCnt->ExOBJPalette_Enable)
					pal = (u16*)(ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
		
				render_sprite_256 (gpu, l, dst, src, pal, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);

				continue;
			}
			/* 16 colors */
			src = gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4);
			pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400);
			
			pal += (spriteInfo->PaletteIndex<<4);
			render_sprite_16 (gpu, l, dst, src, pal, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);
		}
	}

#ifdef WORDS_BIGENDIAN
	*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) << 1) | *(((u16*)spriteInfo)+1) >> 15;
	*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) << 2) | *(((u16*)spriteInfo)+2) >> 14;
#endif
}

void sprite2D(GPU * gpu, u16 l, u8 * dst, u8 * prioTab)
{
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	_OAM_ * spriteInfo = (_OAM_*)(gpu->oam + (nbShow-1));// + 127;
	u16 i;
	
#ifdef WORDS_BIGENDIAN
	*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) >> 1) | *(((u16*)spriteInfo)+1) << 15;
	*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) >> 2) | *(((u16*)spriteInfo)+2) << 14;
#endif

	for(i = 0; i<nbShow; ++i, --spriteInfo
#ifdef WORDS_BIGENDIAN    
	,*(((u16*)(spriteInfo+1))+1) = (*(((u16*)(spriteInfo+1))+1) << 1) | *(((u16*)(spriteInfo+1))+1) >> 15
	,*(((u16*)(spriteInfo+1))+2) = (*(((u16*)(spriteInfo+1))+2) << 2) | *(((u16*)(spriteInfo+1))+2) >> 14
	,*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) >> 1) | *(((u16*)spriteInfo)+1) << 15
	,*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) >> 2) | *(((u16*)spriteInfo)+2) << 14
#endif
	)     /* check all sprites */
	{
		size sprSize;
		s32 sprX, sprY, x, y, lg;
		int xdir;
		u8 prio, * src;
		//u16 * pal;
		u16 i,j;

		// Check if sprite is disabled before everything
		if (spriteInfo->RotScale == 2)
			continue;

		prio = spriteInfo->Priority;
	
		if (spriteInfo->RotScale & 1) 
		{
			s32		fieldX, fieldY, auxX, auxY, realX, realY, offset;
			u8		blockparameter, *pal;
			s16		dx, dmx, dy, dmy;
			u16		colour;

			// Get sprite positions and size
			sprX = (spriteInfo->X<<23)>>23;
			sprY = spriteInfo->Y;
			sprSize = sprSizeTab[spriteInfo->Size][spriteInfo->Shape];

			lg = sprSize.x;
			
			if (sprY>=192)
				sprY = (s32)((s8)(spriteInfo->Y));

			// Copy sprite size, to check change it if needed
			fieldX = sprSize.x;
			fieldY = sprSize.y;

			// If we are using double size mode, double our control vars
			if (spriteInfo->RotScale & 2)
			{
				fieldX <<= 1;
				fieldY <<= 1;
				lg <<= 1;
			}

			// Check if sprite enabled
			if ((l   <sprY) || (l >= sprY+fieldY) ||
				(sprX==256) || (sprX+fieldX<=0))
				continue;

			y = l - sprY;

			// Get which four parameter block is assigned to this sprite
			blockparameter = (spriteInfo->RotScalIndex + (spriteInfo->HFlip<< 3) + (spriteInfo->VFlip << 4))*4;

			// Get rotation/scale parameters
			dx  = (s16)(gpu->oam + blockparameter+0)->attr3;
			dmx = (s16)(gpu->oam + blockparameter+1)->attr3;
			dy  = (s16)(gpu->oam + blockparameter+2)->attr3;
			dmy = (s16)(gpu->oam + blockparameter+3)->attr3;

			// Calculate fixed poitn 8.8 start offsets
			realX = ((sprSize.x) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx + y * dmx;
			realY = ((sprSize.y) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy + y * dmy;

			if(sprX<0)
			{
				// If sprite is not in the window
				if(sprX + fieldX <= 0)
					continue;

				// Otherwise, is partially visible
				lg += sprX;
				realX -= sprX*dx;
				realY -= sprX*dy;
				sprX = 0;
			}
			else
			{
				if(sprX+fieldX>256)
					lg = 255 - sprX;
			}

			// If we are using 1 palette of 256 colours
			if(spriteInfo->Depth)
			{
				src = gpu->sprMem + ((spriteInfo->TileIndex) << 5);

				// If extended palettes are set, use them
				if (dispCnt->ExOBJPalette_Enable)
					pal = (ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
			
				for(i = 0; i < lg; ++i, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*8);
						colour = src[offset];

						if (colour && (prioTab[sprX]>=prio))
						{ 
							if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, T1ReadWord(pal, colour<<1), sprX ,l))
								prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}

				continue;
			}
			// Rotozoomed direct color
			else if(spriteInfo->Mode == 3)
			{
				src = gpu->sprMem + (((spriteInfo->TileIndex&0x03E0) * 8) + (spriteInfo->TileIndex&0x001F))*16;

				for(i = 0; i < lg; ++i, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						offset = auxX + (auxY<<8);
						colour = T1ReadWord(src, offset<<1);

						if((colour&0x8000) && (prioTab[sprX]>=prio))
						{
							if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, colour, sprX ,l))
								prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}

				continue;
			}
			// Rotozoomed 16/16 palette
			else
			{
				src = gpu->sprMem + (spriteInfo->TileIndex<<5);
				pal = ARM9Mem.ARM9_VMEM + 0x200 + (gpu->core*0x400 + (spriteInfo->PaletteIndex*32));
				
				for(i = 0; i < lg; ++i, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*4);
						colour = src[offset];

						if (auxX&1)	colour >>= 4;
						else		colour &= 0xF;

						if(colour && (prioTab[sprX]>=prio))
						{
							if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, T1ReadWord (pal, colour<<1), sprX ,l))
								prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}  

				continue;
			}
		}
		else
		{
			u16 *pal;

			if (!compute_sprite_vars(spriteInfo, l, &sprSize, &sprX, &sprY, &x, &y, &lg, &xdir))
				continue;

			if (spriteInfo->Mode == 2) {
				if (spriteInfo->Depth)
					src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
				else
					src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
				render_sprite_Win (gpu, l, src,
					spriteInfo->Depth, lg, sprX, x, xdir);
				continue;
			}

			if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
			{
				if (dispCnt->OBJ_BMP_2D_dim) // 256*256
					src = (gpu->sprMem) + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1);
				else // 128 * 512
					src = (gpu->sprMem) + (((spriteInfo->TileIndex&0x3F0) * 64  + (spriteInfo->TileIndex&0x0F) *8 + ( y << 8)) << 1);
		
				render_sprite_BMP (gpu, l, dst, (u16*)src, prioTab, prio, lg, sprX, x, xdir);

				continue;
			}
				
			if(spriteInfo->Depth)                   /* 256 colors */
			{
				src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
				pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
		
				render_sprite_256 (gpu, l, dst, src, pal,
					prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);

				continue;
			}
		
			/* 16 colors */
			src = gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
			pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400);

			pal += (spriteInfo->PaletteIndex<<4);
			render_sprite_16 (gpu, l, dst, src, pal,
				prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);
		}
	}

#ifdef WORDS_BIGENDIAN
	*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) << 1) | *(((u16*)spriteInfo)+1) >> 15;
	*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) << 2) | *(((u16*)spriteInfo)+2) >> 14;
#endif
}

/*****************************************************************************/
//			SCREEN FUNCTIONS
/*****************************************************************************/

int Screen_Init(int coreid) {
   MainScreen.gpu = GPU_Init(0);
   SubScreen.gpu = GPU_Init(1);

   return GPU_ChangeGraphicsCore(coreid);
}

void Screen_Reset(void) {
   GPU_Reset(MainScreen.gpu, 0);
   GPU_Reset(SubScreen.gpu, 1);
}

void Screen_DeInit(void) {
        GPU_DeInit(MainScreen.gpu);
        GPU_DeInit(SubScreen.gpu);

        if (GFXCore)
           GFXCore->DeInit();
}

/*****************************************************************************/
//			GRAPHICS CORE
/*****************************************************************************/

// This is for future graphics core switching. This is by no means set in stone

int GPU_ChangeGraphicsCore(int coreid)
{
   int i;

   // Make sure the old core is freed
   if (GFXCore)
      GFXCore->DeInit();

   // So which core do we want?
   if (coreid == GFXCORE_DEFAULT)
      coreid = 0; // Assume we want the first one

   // Go through core list and find the id
   for (i = 0; GFXCoreList[i] != NULL; i++)
   {
      if (GFXCoreList[i]->id == coreid)
      {
         // Set to current core
         GFXCore = GFXCoreList[i];
         break;
      }
   }

   if (GFXCore == NULL)
   {
      GFXCore = &GFXDummy;
      return -1;
   }

   if (GFXCore->Init() == -1)
   {
      // Since it failed, instead of it being fatal, we'll just use the dummy
      // core instead
      GFXCore = &GFXDummy;
   }

   return 0;
}

int GFXDummyInit();
void GFXDummyDeInit();
void GFXDummyResize(int width, int height, BOOL fullscreen);
void GFXDummyOnScreenText(char *string, ...);

GraphicsInterface_struct GFXDummy = {
GFXCORE_DUMMY,
"Dummy Graphics Interface",
0,
GFXDummyInit,
GFXDummyDeInit,
GFXDummyResize,
GFXDummyOnScreenText
};

int GFXDummyInit()
{
   return 0;
}

void GFXDummyDeInit()
{
}

void GFXDummyResize(int width, int height, BOOL fullscreen)
{
}

void GFXDummyOnScreenText(char *string, ...)
{
}


/*****************************************************************************/
//			GPU_ligne
/*****************************************************************************/

void GPU_set_DISPCAPCNT(GPU * gpu, u32 val) {
	gpu->dispCapCnt.val = val;
}

// trade off for speed is 1MB
u16 bright_more_colors[16][0x8000];
u16 bright_less_colors[16][0x8000];
BOOL bright_init=FALSE;

// comment this if want to use formulas instead
// #define BRIGHT_TABLES

void calc_bright_colors() {
	int base = 31 ;
	int factor;
	u16 red, green, blue;
	COLOR color_more, color_less, color_ref;

#define FORMULA_MORE(x)   x + ((base-x)*factor)/16
#define FORMULA_LESS(x)   x - (x*factor)/16

	if (bright_init) return;
	for (factor=0; factor<16; factor++)
	for (red  =0; red  <32; red++) {
		color_ref.bits.red = red;
		color_more.bits.red = FORMULA_MORE(red);
		color_less.bits.red = FORMULA_LESS(red);
		for (green=0; green<32; green++) {
			color_ref.bits.green = green;
			color_more.bits.green = FORMULA_MORE(green);
			color_less.bits.green = FORMULA_LESS(green);
			for (blue =0; blue <32; blue++) {
				color_ref.bits.blue = blue;
				color_more.bits.blue = FORMULA_MORE(blue);
				color_less.bits.blue = FORMULA_LESS(blue);
				bright_more_colors[factor][color_ref.bitx.bgr] = color_more.val;
				bright_less_colors[factor][color_ref.bitx.bgr] = color_less.val;
			}
		}
	}
	bright_init=TRUE;

#undef FORMULA_MORE
#undef FORMULA_LESS
}

void GPU_ligne(NDS_Screen * screen, u16 l)
{
	GPU * gpu = screen->gpu;
	struct _DISPCAPCNT * capcnt;
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	struct _MASTER_BRIGHT * mBright;
	u8 * dst =  GPU_screen + (screen->offset + l) * 512;
	u8 * mdst =  GPU_screen + (MainScreen.offset + l) * 512;
	u8 * sdst =  GPU_screen + (SubScreen.offset + l) * 512;
	itemsForPriority_t * item;
	u8 spr[512];
	u8 sprPrio[256];
	u8 prio;
	int i;
	int ix;
	int vram_bank;
	u16 i16;
	u32 c;
	
	/* initialize the scanline black */
	/* not doing this causes invalid colors when all active BGs are prevented to draw at some place */
	memset(dst,0,256*2) ;

	// This could almost be changed to use function pointers
	switch (gpu->dispMode)
	{
		case 1: // Display BG and OBJ layers
			break;
		case 0: // Display Off(Display white)
			for (i=0; i<256; i++)
				T2WriteWord(dst, i << 1, 0x7FFF);
			return;
		case 2: // Display framebuffer
		{
                        int ii = l * 256 * 2;
                        u8 * vram;

                        /* we only draw one of the VRAM blocks */
                        vram_bank = dispCnt->VRAM_Block ;

                        // This probably only needs to be calculated once per frame, but at least it's better than before >_<
                        if (MMU.vram_mode[vram_bank] & 4)
                           vram = ARM9Mem.ARM9_LCD + (MMU.vram_mode[vram_bank] & 3) * 0x20000;
                        else
                           vram = ARM9Mem.ARM9_ABG + MMU.vram_mode[vram_bank] * 0x20000;

                        for (i=0; i<(256 * 2); i+=2)
                        {
                           T2WriteWord(dst, i, T1ReadWord(vram, ii));
                           ii+=2;
                        }
		}
			return;
		case 3:
	// Read from FIFO MAIN_MEMORY_DISP_FIFO, two pixels at once format is x555, bit15 unused
	// Reference:  http://nocash.emubase.de/gbatek.htm#dsvideocaptureandmainmemorydisplaymode
	// (under DISP_MMEM_FIFO)
#if 0
			for (i=0; i<256;) {
				c = FIFOValue(MMU.fifos + MAIN_MEMORY_DISP_FIFO);
				T2WriteWord(dst, i << 1, c&0xFFFF); i++;
				T2WriteWord(dst, i << 1, c>>16); i++;
			}
else
		printlog("FIFO MAIN_MEMORY_DISP_FIFO\n");
#endif
			return;
	}


	c = T1ReadWord(ARM9Mem.ARM9_VMEM, gpu->core * 0x400);
	
	// init background color & priorities
	for(i = 0; i< 256; ++i)
	{
		T2WriteWord(dst, i << 1, c);
		T2WriteWord(spr, i << 1, c);
		sprPrio[i]=0xFF;
		gpu->sprWin[l][i]=0;
	}
	
	// init pixels priorities
	for (i=0;i<NB_PRIORITIES;i++) {
		gpu->itemsForPriority[i].nbPixelsX = 0;
	}

	// for all the pixels in the line
	if (gpu->LayersEnable[4]) {
		gpu->spriteRender(gpu, l, spr, sprPrio);
		for(i= 0; i<256; i++) {
			// assign them to the good priority item
			prio = sprPrio[i];
			if (prio >=4) continue;
			
			item = &(gpu->itemsForPriority[prio]);
			item->PixelsX[item->nbPixelsX]=i;
			item->nbPixelsX++;
		}
	}

	// paint lower priorities fist
	// then higher priorities on top
	for(prio=NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(gpu->itemsForPriority[prio]);
		// render BGs
		for (i=0; i < item->nbBGs; i++) 
		{
			i16 = item->BGs[i];

			// If BG0, core A, and 3D is enabled, ask the gpu3D plugin for data
			if (i16 == 0 && dispCnt->BG0_3D && gpu->core == 0)
			{
				gpu3D->NDS_3D_GetLine (l, (u16*)dst);
			}
			else
			{
				if (gpu->LayersEnable[i16])
					modeRender[dispCnt->BG_Mode][i16](gpu, i16, l, dst);
			}
		}
		// render sprite Pixels
		for (i=0; i < item->nbPixelsX; i++) {
			i16=item->PixelsX[i];
			T2WriteWord(dst, i16 << 1, T2ReadWord(spr, i16 << 1));
		}
	}
	
	/* DISPCAP */
	/* TODO: Capture source B and A+B */
	if(gpu->core == GPU_MAIN)	/* capture only for main gpu */
	{
		capcnt = &gpu->dispCapCnt.bits;
		if (capcnt->Capture_Enable)	
		{
			int capx, capy;
			u8 *capDst;
			
			/* find the dimensions of the capture */
			switch(capcnt->Capture_Size)	/* TODO: it could be done only once, when writting to dispcap register  */
			{
				case 0:
					capx = 128;
					capy = 128;
					break;
				case 1:
					capx = 256;
					capy = 64;
					break;
				case 2:
					capx = 256;
					capy = 128;
				case 3:
					capx = 256;
					capy = 192;
					break;
			}
			
			if(l < capy)	/* check if our line is in cature area */
			{
				/* calculate VRAM destination address */
				capDst = (ARM9Mem.ARM9_LCD 
						+ (capcnt->VRAM_Write_Block * 0x20000) 
						+ ((dispCnt->BG_Mode != 2) ? (capcnt->VRAM_Write_Offset * 0x8000) : 0)
						+ l * (capx<<1));	/* read offset ignored in VRAM display mode*/
				
// 				LOG("Capture line %d (%X) [dst: %X]...\n", l, gpu->dispCapCnt.val, capDst - ARM9Mem.ARM9_LCD);
				
				switch(capcnt->Capture_Source)
				{
					case 0:	/* source A only */
						if(capcnt->Source_A == 1)	/* capture 3D only */
						{
							u16 cap3DLine[256];	/* temp buffer for 3D line reading */
							gpu3D->NDS_3D_GetLine (l, cap3DLine);	/*FIXME: not sure it's good, since I hadn't seen how 3D works in desmume */
							for(i = 0; i < (capx<<1); i++) T1WriteWord(capDst, i, cap3DLine[i]);	/* copy this line to buffer */
						}
						else	/* capture all screen (BG + OBJ + 3D) */
						{
							for(i = 0; i < (capx<<1); i++) T1WriteWord(capDst, i, T2ReadWord(dst, i));	/* plain copy from screen to buffer */
						}
					
						break;
					case 1: /* source B only */
						if(capcnt->Source_B == 1)	/* capture from display FIFO */
						{
							/* TODO ... */
						}
						else	/* capture from VRAM */
						{
							/* calculate vram source address */
							u8 *capSrc = (ARM9Mem.ARM9_LCD 
									+ (dispCnt->VRAM_Block * 0x20000)
									+ ((dispCnt->BG_Mode != 2) ? (capcnt->VRAM_Write_Offset * 0x8000) : 0)
									+ l * capx * 2);	/* write offset ignored in VRAM display mode*/
								
							for(i = 0; i < (capx<<1); i++) T1WriteWord(capDst, i, T2ReadWord(capSrc, i));	/* plain copy from source to dest */
						}
					
						break;
					case 2: /* source A + B (using blending) */
					case 3:
					
						/* TODO... (the above code will need modifications in order to avoid redudance) */
						GPULOG("Unhandled capture source: %d\n", capcnt->Capture_Source);	/* TODO */
				}
				
				if(l + 1 == capy)		/* if it was the last line, we're done !*/
				{
					capcnt->Capture_Enable = 0;	/* done, now capture is disabled */
					T1WriteLong(ARM9Mem.ARM9_REG, 0x64, gpu->dispCapCnt.val);
				}
			}
		}
	}

// FIXME !!!
/* capture */
#if 0
// to be done with opengl
	capcnt = &gpu->dispCapCnt.bits;
	if (capcnt->Capture_Enable)
	{
		u16 * srcA, * srcB, *vram;
		u32 c; u8 vram_bank;
		COLOR color, colA, colB;
		u16 ilast= 128;
		if (capcnt->Capture_Size) ilast = 256;
	
		vram = (u16*)(ARM9Mem.ARM9_ABG
			+ MMU.vram_mode[capcnt->VRAM_Write_Block] * 0x20000
			+ capcnt->VRAM_Write_Offset * 0x08000);
	
		// I dunno yet how to do for 3D
		if (!capcnt->Source_A)
			srcA = (u16*)dst;
	
		if (!capcnt->Source_B) {
			vram_bank = dispCnt->VRAM_Block ;
			if (MMU.vram_mode[vram_bank] & 4) {
				srcB = (u16*)(ARM9Mem.ARM9_LCD 
					+ (MMU.vram_mode[vram_bank] & 3) * 0x20000
					+ capcnt->VRAM_Read_Offset * 0x08000);
			} else {
				srcB = (u16*)(ARM9Mem.ARM9_ABG 
					+ MMU.vram_mode[vram_bank] * 0x20000
					+ capcnt->VRAM_Read_Offset * 0x08000);
			}
		}
	
		printf("capture source %d\n",capcnt->Capture_Source);
	
		switch(capcnt->Capture_Source) {
		case 0: // only source A
			if (!capcnt->Source_A) {
				srcA = (u16*)dst;
				for (i=0; i<ilast; i++) {
					vram[i] = srcA[i];
				}
			}
			break;
		case 1: // only source B
			if (capcnt->Source_B) {
				for (i=0; i<ilast;) {
					c = FIFOValue(MMU.fifos + MAIN_MEMORY_DISP_FIFO);
					vram[i] = c&0xFFFF; i++;
					vram[i] = c>>16; i++;
				}
			} else {
				for (i=0; i<ilast; i++) {
					vram[i] = srcB[i];
				}
			}
			break;
		default: // blend A + B
			if (capcnt->Source_B) {
				for (i=0; i<ilast;) {
					c = FIFOValue(MMU.fifos + MAIN_MEMORY_DISP_FIFO);
					colA.val = c&0xFFFF;
					colB.val = srcB[i];
	#define FORMULA(field)	\
		color.bits.field = ((colA.bits.field * colA.bits.alpha * capcnt->BlendFactor_A) + (colB.bits.field * colB.bits.alpha * capcnt->BlendFactor_B)) / 16;
					FORMULA(red)
					FORMULA(green)
					FORMULA(blue)
					vram[i] = color.val;
					i++;
					colA.val = c >> 16;
					colB.val = srcB[i];
					FORMULA(red)
					FORMULA(green)
					FORMULA(blue)
					vram[i] = color.val;
					i++;
				}
			} else {
				for (i=0; i<ilast; i++) {
					colA.val = srcA[i];
					colB.val = srcB[i];
					FORMULA(red)
					FORMULA(green)
					FORMULA(blue)
					vram[i] = color.val;
				}
	#undef FORMULA
			}
			break;
		}
	}
#endif
/* end of capture */

#ifndef HAVE_LIBGDKGLEXT_X11_1_0
// damdoum :
//   brightness done with opengl
//   test are ok (gfx_test_brightness)
//   now, if we are going to support 3D, this becomes dead code
//   because it is obvious we'll use openGL / mesa3D

#ifdef BRIGHT_TABLES
	calc_bright_colors();
#endif

// Apply final brightness adjust (MASTER_BRIGHT)
//  Reference: http://nocash.emubase.de/gbatek.htm#dsvideo (Under MASTER_BRIGHTNESS)
/* Mightymax> it should be more effective if the windowmanager applies brightness when drawing */
/* it will most likly take acceleration, while we are stuck here with CPU power */
	
	switch (gpu->MasterBrightMode)
	{
		// Disabled
		case 0:
			break;

		// Bright up
		case 1:
		{
			COLOR dstColor;
			unsigned int masterBrightFactor = gpu->MasterBrightFactor;
			u16 * colors = bright_more_colors[masterBrightFactor];

			/* when we wont do anything, we dont need to loop */
			if (!masterBrightFactor) break ;

			for(i16 = 0; i16 < 256; ++i16)
			{
#ifndef BRIGHT_TABLES
				u8 base ;
				u8 r,g,b; // get components, 5bit each
				dstColor.val = *((u16 *) (dst + (i16 << 1)));
				r = dstColor.bits.red;
				g = dstColor.bits.green;
				b = dstColor.bits.blue;
				// Bright up and clamp to 5bit <-- automatic
				base = 31 ;
				dstColor.bits.red   = r + ((base-r)*masterBrightFactor)/16;
				dstColor.bits.green = g + ((base-g)*masterBrightFactor)/16;
				dstColor.bits.blue  = b + ((base-b)*masterBrightFactor)/16;
#else
				dstColor.val = T1ReadWord(dst, i16 << 1);
				dstColor.bitx.bgr = colors[dstColor.bitx.bgr];
#endif
				*((u16 *) (dst + (i16 << 1))) = dstColor.val;
			}
			break;
		}

		// Bright down
		case 2:
		{
/*
	NOTE: gbatek (in the reference above) seems to expect 6bit values 
	per component, but as desmume works with 5bit per component, 
			we use 31 as top, instead of 63. Testing it on a few games, 
			using 63 seems to give severe color wraping, and 31 works
			nicely, so for now we'll just that, until proven wrong.

	i have seen pics of pokemon ranger getting white with 31, with 63 it is nice.
	it could be pb of alpha or blending or...

	MightyMax> created a test NDS to check how the brightness values work,
	and 31 seems to be correct. FactorEx is a override for max brighten/darken
	See: http://mightymax.org/gfx_test_brightness.nds
	The Pokemon Problem could be a problem with 8/32 bit writes not recognized yet,
	i'll add that so you can check back.

*/
			COLOR dstColor;
			unsigned int    masterBrightFactor = gpu->MasterBrightFactor;
			u16 * colors = bright_less_colors[masterBrightFactor];
 
			/* when we wont do anything, we dont need to loop */
			if (!masterBrightFactor) break;
 
			for(i16 = 0; i16 < 256; ++i16)
			{
#ifndef BRIGHT_TABLES
				u8 r,g,b;
				dstColor.val = *((u16 *) (dst + (i16 << 1)));
				r = dstColor.bits.red;
				g = dstColor.bits.green;
				b = dstColor.bits.blue;
				// Bright up and clamp to 5bit <- automatic
				dstColor.bits.red   = r - (r*masterBrightFactor)/16;
				dstColor.bits.green = g - (g*masterBrightFactor)/16;
				dstColor.bits.blue  = b - (b*masterBrightFactor)/16;
#else
				dstColor.val = T1ReadWord(dst, i16 << 1);
				dstColor.bitx.bgr = colors[dstColor.bitx.bgr];
#endif
				*((u16 *) (dst + (i16 << 1))) = dstColor.val;
			}
			break;
		}

		// Reserved
		case 3:
			break;
	 }
#endif
}

void gpu_savestate(std::ostream* os)
{
	os->write((char*)GPU_screen,sizeof(GPU_screen));
}

bool gpu_loadstate(std::istream* is)
{
	is->read((char*)GPU_screen,sizeof(GPU_screen));
	return !is->fail();
}
