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
#include "debug.h"
#include "NDSSystem.h"

//#define CHECKSPRITES

#ifdef CHECKSPRITES
#define CHECK_SPRITE(type) \
if (!src) {\
	INFO("Sprite%s(%s) in mode %i %s\n",\
	type==1?"1D":"2D",\
	dispCnt->OBJ_BMP_mapping==1?"1D":"2D",\
	spriteInfo->Mode,\
	(spriteInfo->RotScale & 1)?"(Rotoscaled)":"");\
continue;\
};
#else
#define CHECK_SPRITE(type) if (!src) { continue; };
#endif


ARM9_struct ARM9Mem;

extern BOOL click;
NDS_Screen MainScreen;
NDS_Screen SubScreen;

//instantiate static instance
GPU::MosaicLookup GPU::mosaicLookup;

//#define DEBUG_TRI

CACHE_ALIGN u8 GPU_screen[4*256*192];

CACHE_ALIGN u8 sprWin[256];

OSDCLASS	*osd = NULL;
OSDCLASS	*osdA = NULL;
OSDCLASS	*osdB = NULL;

const short sizeTab[4][4][2] =
{
      {{256,256}, {512, 256}, {256, 512}, {512, 512}},
      {{128,128}, {256, 256}, {512, 512}, {1024, 1024}},
      {{128,128}, {256, 256}, {512, 256}, {512, 512}},
      {{512,1024}, {1024, 512}, {0, 0}, {0, 0}},
//      {{0, 0}, {0, 0}, {0, 0}, {0, 0}}
};

const size sprSizeTab[4][4] = 
{
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};

const s8 mode2type[8][4] = 
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

//static BOOL setFinalColorDirect				(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x);
static void setFinalBGColorSpecialNone			(GPU *gpu, u8 *dst, u16 color, u8 x);
static void setFinalBGColorSpecialBlend			(GPU *gpu, u8 *dst, u16 color, u8 x);
static void setFinalBGColorSpecialIncrease		(GPU *gpu, u8 *dst, u16 color, u8 x);
static void setFinalBGColorSpecialDecrease		(GPU *gpu, u8 *dst, u16 color, u8 x);

//static BOOL setFinalColorDirectWnd			(const GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x);
static void setFinalBGColorSpecialNoneWnd		(GPU *gpu, u8 *dst, u16 color, u8 x);
static void setFinalBGColorSpecialBlendWnd		(GPU *gpu, u8 *dst, u16 color, u8 x);
static void setFinalBGColorSpecialIncreaseWnd	(GPU *gpu, u8 *dst, u16 color, u8 x);
static void setFinalBGColorSpecialDecreaseWnd	(GPU *gpu, u8 *dst, u16 color, u8 x);

static void setFinalOBJColorSpecialNone			(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialBlend		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialIncrease		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialDecrease		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialNoneWnd		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialBlendWnd		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialIncreaseWnd	(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialDecreaseWnd	(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);

static void setFinal3DColorSpecialNone			(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialBlend			(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialIncrease		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialDecrease		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialNoneWnd		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialBlendWnd		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialIncreaseWnd	(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);
static void setFinal3DColorSpecialDecreaseWnd	(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x);




GPU::FinalBGColFunct pixelBlittersBG[8] = {	//setFinalColorDirect,
									setFinalBGColorSpecialNone,
                                    setFinalBGColorSpecialBlend,
                                    setFinalBGColorSpecialIncrease,
                                    setFinalBGColorSpecialDecrease,

									//setFinalColorDirectWnd,
                                    setFinalBGColorSpecialNoneWnd,
									setFinalBGColorSpecialBlendWnd,
                                    setFinalBGColorSpecialIncreaseWnd,
                                    setFinalBGColorSpecialDecreaseWnd};

GPU::FinalOBJColFunct pixelBlittersOBJ[8] = {
									setFinalOBJColorSpecialNone,
									setFinalOBJColorSpecialBlend,
									setFinalOBJColorSpecialIncrease,
									setFinalOBJColorSpecialDecrease,
									setFinalOBJColorSpecialNoneWnd,
									setFinalOBJColorSpecialBlendWnd,
									setFinalOBJColorSpecialIncreaseWnd,
									setFinalOBJColorSpecialDecreaseWnd,};

GPU::Final3DColFunct pixelBlitters3D[8] = {
									setFinal3DColorSpecialNone,
                                    setFinal3DColorSpecialBlend,
                                    setFinal3DColorSpecialIncrease,
                                    setFinal3DColorSpecialDecrease,
                                    setFinal3DColorSpecialNoneWnd,
									setFinal3DColorSpecialBlendWnd,
                                    setFinal3DColorSpecialIncreaseWnd,
                                    setFinal3DColorSpecialDecreaseWnd};

static const CACHE_ALIGN u8 win_empty[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static CACHE_ALIGN u16 fadeInColors[17][0x8000];
static CACHE_ALIGN u16 fadeOutColors[17][0x8000];
CACHE_ALIGN u8 gpuBlendTable555[17][17][32][32];

/*****************************************************************************/
//			INITIALIZATION
/*****************************************************************************/


static void GPU_InitFadeColors()
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

	for(int i = 0; i <= 16; i++)
	{
		for(int j = 0x8000; j < 0x10000; j++)
		{
			COLOR cur;

			cur.val = j;
			cur.bits.red = (cur.bits.red + ((31 - cur.bits.red) * i / 16));
			cur.bits.green = (cur.bits.green + ((31 - cur.bits.green) * i / 16));
			cur.bits.blue = (cur.bits.blue + ((31 - cur.bits.blue) * i / 16));
			fadeInColors[i][j & 0x7FFF] = cur.val;

			cur.val = j;
			cur.bits.red = (cur.bits.red - (cur.bits.red * i / 16));
			cur.bits.green = (cur.bits.green - (cur.bits.green * i / 16));
			cur.bits.blue = (cur.bits.blue - (cur.bits.blue * i / 16));
			fadeOutColors[i][j & 0x7FFF] = cur.val;
		}
	}


	for(int c0=0;c0<=31;c0++) 
		for(int c1=0;c1<=31;c1++) 
			for(int eva=0;eva<=16;eva++)
				for(int evb=0;evb<=16;evb++)
				{
					int blend = ((c0 * eva / 16) + (c1 * evb / 16) );
					int final = std::min<int>(31,blend);
					gpuBlendTable555[eva][evb][c0][c1] = final;
				}
}



GPU * GPU_Init(u8 l)
{
	GPU * g;

	if ((g = (GPU *) malloc(sizeof(GPU))) == NULL)
		return NULL;

	GPU_Reset(g, l);
	GPU_InitFadeColors();

	g->curr_win[0] = win_empty;
	g->curr_win[1] = win_empty;
	g->need_update_winh[0] = true;
	g->need_update_winh[1] = true;
	g->setFinalColorBck = setFinalBGColorSpecialNone;
	g->setFinalColorSpr = setFinalOBJColorSpecialNone;
	g->setFinalColor3D = setFinal3DColorSpecialNone;

	return g;
}

void GPU_Reset(GPU *g, u8 l)
{
	memset(g, 0, sizeof(GPU));

	g->setFinalColorBck = setFinalBGColorSpecialNone;
	g->setFinalColorSpr = setFinalOBJColorSpecialNone;
	g->setFinalColor3D = setFinal3DColorSpecialNone;
	g->core = l;
	g->BGSize[0][0] = g->BGSize[1][0] = g->BGSize[2][0] = g->BGSize[3][0] = 256;
	g->BGSize[0][1] = g->BGSize[1][1] = g->BGSize[2][1] = g->BGSize[3][1] = 256;
	g->dispOBJ = g->dispBG[0] = g->dispBG[1] = g->dispBG[2] = g->dispBG[3] = TRUE;

	g->spriteRender = sprite1D;

	g->bgPrio[4] = 0xFF;

	g->bg0HasHighestPrio = TRUE;

	if(g->core == GPU_SUB)
	{
		g->oam = (OAM *)(ARM9Mem.ARM9_OAM + ADDRESS_STEP_1KB);
		g->sprMem = ARM9MEM_BOBJ;
		// GPU core B
		g->dispx_st = (REG_DISPx*)(&ARM9Mem.ARM9_REG[REG_DISPB]);
		delete osdB;
		osdB = new OSDCLASS(1);
	}
	else
	{
		g->oam = (OAM *)(ARM9Mem.ARM9_OAM);
		g->sprMem = ARM9MEM_AOBJ;
		// GPU core A
		g->dispx_st = (REG_DISPx*)(&ARM9Mem.ARM9_REG[0]);
		delete osdA;
		osdA = new OSDCLASS(0);
	}

	delete osd;
	osd = new OSDCLASS(-1);
	//DISP_FIFOclear(&g->disp_fifo);
	memset(GPU_screen, 0, sizeof(GPU_screen));
}

void GPU_DeInit(GPU * gpu)
{
	delete osd; osd=NULL;
	delete osdA; osdA=NULL;
	delete osdB; osdB=NULL;
   free(gpu);
}

static void GPU_resortBGs(GPU *gpu)
{
	int i, prio;
	struct _DISPCNT * cnt = &gpu->dispx_st->dispx_DISPCNT.bits;
	itemsForPriority_t * item;

	//zero 29-dec-2008 - this really doesnt make sense to me.
	//i changed the sprwin to be line by line,
	//and resetting it here is pointless since line rendering is instantaneous
	//and completely produces and consumes sprwin after which the contents of this buffer are useless
	//memset(gpu->sprWin,0, 256*192);

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
	for (i=NB_BG; i>0; ) {
		i--;
		if (!gpu->LayersEnable[i]) continue;
		prio = (gpu->dispx_st)->dispx_BGxCNT[i].bits.Priority;
		item = &(gpu->itemsForPriority[prio]);
		item->BGs[item->nbBGs]=i;
		item->nbBGs++;
	}

	int bg0Prio = gpu->dispx_st->dispx_BGxCNT[0].bits.Priority;
	gpu->bg0HasHighestPrio = TRUE;
	for(i = 1; i < 4; i++)
	{
		if(gpu->LayersEnable[i])
		{
			if(gpu->dispx_st->dispx_BGxCNT[i].bits.Priority < bg0Prio)
			{
				gpu->bg0HasHighestPrio = FALSE;
				break;
			}
		}
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

u16 GPU::blend(u16 colA, u16 colB)
{
	COLOR c1, c2, cfinal;
	c1.val = colA;
	c2.val = colB;

	cfinal.bits.red = (*blendTable)[c1.bits.red][c2.bits.red];
	cfinal.bits.green = (*blendTable)[c1.bits.green][c2.bits.green];
	cfinal.bits.blue = (*blendTable)[c1.bits.blue][c2.bits.blue];

	return cfinal.val;
}


void GPU_setMasterBrightness (GPU *gpu, u16 val)
{
	if(!nds.isInVblank()) {
		PROGINFO("Changing master brightness outside of vblank\n");
	}
 	gpu->MasterBrightFactor = (val & 0x1F);
	gpu->MasterBrightMode	= (val>>14);
}

void SetupFinalPixelBlitter (GPU *gpu)
{
	u8 windowUsed = (gpu->WIN0_ENABLED | gpu->WIN1_ENABLED | gpu->WINOBJ_ENABLED);
	u8 blendMode  = (gpu->BLDCNT >> 6)&3;

	gpu->setFinalColorSpr = pixelBlittersOBJ[windowUsed*4 + blendMode];
	gpu->setFinalColorBck = pixelBlittersBG[windowUsed*4 + blendMode];
	gpu->setFinalColor3D = pixelBlitters3D[windowUsed*4 + blendMode];
}
    
/* Sets up LCD control variables for Display Engines A and B for quick reading */
void GPU_setVideoProp(GPU * gpu, u32 p)
{
	struct _DISPCNT * cnt;
	cnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;

	T1WriteLong((u8 *)&(gpu->dispx_st)->dispx_DISPCNT.val, 0, p);

	gpu->WIN0_ENABLED	= cnt->Win0_Enable;
	gpu->WIN1_ENABLED	= cnt->Win1_Enable;
	gpu->WINOBJ_ENABLED = cnt->WinOBJ_Enable;

	SetupFinalPixelBlitter (gpu);

    gpu->dispMode = cnt->DisplayMode & ((gpu->core)?1:3);

	gpu->vramBlock = cnt->VRAM_Block;

	switch (gpu->dispMode)
	{
		case 0: // Display Off
			return;
		case 1: // Display BG and OBJ layers
			break;
		case 2: // Display framebuffer
			gpu->VRAMaddr = (u8 *)ARM9Mem.ARM9_LCD + (gpu->vramBlock * 0x20000);
			return;
		case 3: // Display from Main RAM
			// nothing to be done here
			// see GPU_ligne who gets data from FIFO.
			return;
	}

	if(cnt->OBJ_Tile_mapping)
	{
		/* 1-d sprite mapping */
		// boundary :
		// core A : 32k, 64k, 128k, 256k
		// core B : 32k, 64k, 128k, 128k
		gpu->sprBoundary = 5 + cnt->OBJ_Tile_1D_Bound ;
		if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3))
			gpu->sprBoundary = 7;

		gpu->spriteRender = sprite1D;
	} else {
		/* 2d sprite mapping */
		// boundary : 32k
		gpu->sprBoundary = 5;
		gpu->spriteRender = sprite2D;
	}
     
	if(cnt->OBJ_BMP_1D_Bound && (gpu->core == GPU_MAIN))
		gpu->sprBMPBoundary = 8;
	else
		gpu->sprBMPBoundary = 7;

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
	struct _BGxCNT * cnt = &((gpu->dispx_st)->dispx_BGxCNT[num].bits);
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	int mode;
	
	T1WriteWord((u8 *)&(gpu->dispx_st)->dispx_BGxCNT[num].val, 0, p);
	
	GPU_resortBGs(gpu);

	if(gpu->core == GPU_SUB)
	{
		gpu->BG_tile_ram[num] = ARM9MEM_BBG;
		gpu->BG_bmp_ram[num]  = ARM9MEM_BBG;
		gpu->BG_map_ram[num]  = ARM9MEM_BBG;
	} 
	else 
	{
		gpu->BG_tile_ram[num] = ARM9MEM_ABG +  dispCnt->CharacBase_Block * ADDRESS_STEP_64kB ;
		gpu->BG_bmp_ram[num]  = ARM9MEM_ABG;
		gpu->BG_map_ram[num]  = ARM9MEM_ABG +  dispCnt->ScreenBase_Block * ADDRESS_STEP_64kB;
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
	
	gpu->bgPrio[num] = (p & 0x3);
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

template<int WIN_NUM>
FORCEINLINE bool GPU::withinRect(u8 x) const
{
	return curr_win[WIN_NUM][x];
}



//  Now assumes that *draw and *effect are different from 0 when called, so we can avoid
// setting some values twice
FORCEINLINE void GPU::renderline_checkWindows(u16 x, bool &draw, bool &effect) const
{
	// Check if win0 if enabled, and only check if it is
	// howevever, this has already been taken care of by the window precalculation
	//if (WIN0_ENABLED)
	{
		// it is in win0, do we display ?
		// high priority	
		if (withinRect<0>(x))
		{
			//INFO("bg%i passed win0 : (%i %i) was within (%i %i)(%i %i)\n", bgnum, x, gpu->currLine, gpu->WIN0H0, gpu->WIN0V0, gpu->WIN0H1, gpu->WIN0V1);
			draw	= (WININ0 >> currBgNum)&1;
			effect = (WININ0_SPECIAL);
			return;
		}
	}

	// Check if win1 if enabled, and only check if it is
	//if (WIN1_ENABLED)
	// howevever, this has already been taken care of by the window precalculation
	{
		// it is in win1, do we display ?
		// mid priority
		if(withinRect<1>(x))
		{
			//INFO("bg%i passed win1 : (%i %i) was within (%i %i)(%i %i)\n", bgnum, x, gpu->currLine, gpu->WIN1H0, gpu->WIN1V0, gpu->WIN1H1, gpu->WIN1V1);
			draw	= (WININ1 >> currBgNum)&1;
			effect = (WININ1_SPECIAL);
			return;
		}
	}

	//if(true) //sprwin test hack
	if (WINOBJ_ENABLED)
	{
		// it is in winOBJ, do we display ?
		// low priority
		if (sprWin[x])
		{
			draw	= (WINOBJ >> currBgNum)&1;
			effect	= (WINOBJ_SPECIAL);
			return;
		}
	}

	if (WINOBJ_ENABLED | WIN1_ENABLED | WIN0_ENABLED)
	{
		draw	= (WINOUT >> currBgNum) & 1;
		effect	= (WINOUT_SPECIAL);
	}
}

/*****************************************************************************/
//			PIXEL RENDERING - BGS
/*****************************************************************************/

static void setFinalBGColorSpecialNone (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	//sprwin test hack - use this code
	//BOOL windowDraw = TRUE, windowEffect = TRUE;
	//renderline_checkWindows(gpu,bgnum,x,y, &windowDraw, &windowEffect);
	//if(windowDraw) T2WriteWord(dst, passing, color);
	//return 1;

	T2WriteWord(dst, 0, color);
	gpu->bgPixels[x] = gpu->currBgNum;
}

static void setFinalBGColorSpecialBlend (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	if(gpu->BLDCNT & (1 << gpu->currBgNum))
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		// If the layer we are drawing on is selected as 2nd source, we can blend
		if(gpu->BLDCNT & (0x100 << bg_under))
			final = gpu->blend(color,T2ReadWord(dst, 0));

		T2WriteWord(dst, 0, (final | 0x8000));
		gpu->bgPixels[x] = gpu->currBgNum;
	}
	else
	{
		T2WriteWord(dst, 0, (color | 0x8000));
		gpu->bgPixels[x] = gpu->currBgNum;
	}
}

static void setFinalBGColorSpecialIncrease (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	if ((gpu->BLDCNT >> gpu->currBgNum)&1)   // the bg to draw has a special color effect
	{
		if (gpu->BLDY_EVY != 0x0)
		{ // dont slow down if there is nothing to do
			color = (fadeInColors[gpu->BLDY_EVY][color&0x7FFF] | 0x8000);
		}

		T2WriteWord(dst, 0, color) ;
		gpu->bgPixels[x] = gpu->currBgNum;
	}
	else
	{
		T2WriteWord(dst, 0, color);
		gpu->bgPixels[x] = gpu->currBgNum;
	}
}

static void setFinalBGColorSpecialDecrease (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	if ((gpu->BLDCNT >> gpu->currBgNum)&1)   // the bg to draw has a special color effect
	{
		if (gpu->BLDY_EVY != 0x0)
		{ // dont slow down if there is nothing to do
			color = (fadeOutColors[gpu->BLDY_EVY][color&0x7FFF] | 0x8000);
		}
		T2WriteWord(dst, 0, color) ;
		gpu->bgPixels[x] = gpu->currBgNum;
	}
	else
	{
		T2WriteWord(dst, 0, color);
		gpu->bgPixels[x] = gpu->currBgNum;
	}
}

static void setFinalBGColorSpecialNoneWnd (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if (((gpu->BLDCNT >> gpu->currBgNum)&1) && windowEffect)   // the bg to draw has a special color effect
	{
		T2WriteWord(dst, 0, color);
		gpu->bgPixels[x] = gpu->currBgNum;
	}
	else
	{
		if ((windowEffect && (gpu->BLDCNT & (0x100 << gpu->currBgNum))) || windowDraw)
		{
			T2WriteWord(dst, 0, color);
			gpu->bgPixels[x] = gpu->currBgNum;
		}
	}
}

static void setFinalBGColorSpecialBlendWnd (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if((gpu->BLDCNT & (1 << gpu->currBgNum)) && windowEffect)
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if(gpu->BLDCNT & (0x100 << bg_under))
				final = gpu->blend(color,T2ReadWord(dst, 0));
			

			T2WriteWord(dst, 0, (final | 0x8000));
			gpu->bgPixels[x] = gpu->currBgNum;
		}
		else
		{
			T2WriteWord(dst, 0, (color | 0x8000));
			gpu->bgPixels[x] = gpu->currBgNum;
		}
	}
}

static void setFinalBGColorSpecialIncreaseWnd (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if((gpu->BLDCNT & (1 << gpu->currBgNum)) && windowEffect)
		{
			if (gpu->BLDY_EVY != 0x0)
			{
				color = fadeInColors[gpu->BLDY_EVY][color&0x7FFF];
			}

			T2WriteWord(dst, 0, (color | 0x8000));
			gpu->bgPixels[x] = gpu->currBgNum;
		}
		else
		{
			T2WriteWord(dst, 0, (color | 0x8000));
			gpu->bgPixels[x] = gpu->currBgNum;
		}
	}
}

static void setFinalBGColorSpecialDecreaseWnd (GPU *gpu, u8 *dst, u16 color, u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if((gpu->BLDCNT & (1 << gpu->currBgNum)) && windowEffect)
		{
			if (gpu->BLDY_EVY != 0x0)
			{
				color = fadeOutColors[gpu->BLDY_EVY][color&0x7FFF];
			}

			T2WriteWord(dst, 0, (color | 0x8000));
			gpu->bgPixels[x] = gpu->currBgNum;
		}
		else
		{
			T2WriteWord(dst, 0, (color | 0x8000));
			gpu->bgPixels[x] = gpu->currBgNum;
		}
	}
}

/*****************************************************************************/
//			PIXEL RENDERING - OBJS
/*****************************************************************************/

static void setFinalOBJColorSpecialNone(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	if(type == 1)
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		// If the layer we are drawing on is selected as 2nd source, we can blend
		if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
			final = gpu->blend(color,T2ReadWord(dst, passing));

		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 4;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 4;
	}
}

static void setFinalOBJColorSpecialBlend(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	if((gpu->BLDCNT & 0x10) || (type == 1))
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		//If the layer we are drawing on is selected as 2nd source, we can blend
		if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
			final = gpu->blend(color,T2ReadWord(dst, passing));
		
		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 4;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 4;
	}
}

static void setFinalOBJColorSpecialIncrease(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	if(type == 1)
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		//If the layer we are drawing on is selected as 2nd source, we can blend
		if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
			final = gpu->blend(color,T2ReadWord(dst, passing));
		
		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 4;
	}
	else if(gpu->BLDCNT & 0x10)
	{
		if (gpu->BLDY_EVY != 0x0)
		{
			color = fadeInColors[gpu->BLDY_EVY][color&0x7FFF];
		}

		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 4;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 4;
	}

}

static void setFinalOBJColorSpecialDecrease(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	if(type == 1)
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		//If the layer we are drawing on is selected as 2nd source, we can blend
		if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
			final = gpu->blend(color,T2ReadWord(dst, passing));

		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 4;
	}
	else if(gpu->BLDCNT & 0x10)
	{
		if (gpu->BLDY_EVY != 0x0)
		{
			color = fadeOutColors[gpu->BLDY_EVY][color&0x7FFF];
		}

		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 4;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 4;
	}
}

static void setFinalOBJColorSpecialNoneWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	bool windowDraw = true, windowEffect = true;

	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if((type == 1) && windowEffect)
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
				final = gpu->blend(color,T2ReadWord(dst, passing));
			
			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 4;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 4;
		}
	}
}

static void setFinalOBJColorSpecialBlendWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	bool windowDraw = true, windowEffect = true;

	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if(((gpu->BLDCNT & 0x10) || (type == 1)) && windowEffect)
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
				final = gpu->blend(color,T2ReadWord(dst, passing));

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 4;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 4;
		}
	}
}

static void setFinalOBJColorSpecialIncreaseWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	bool windowDraw = true, windowEffect = true;

	gpu->renderline_checkWindows(x, windowDraw,windowEffect);

	if(windowDraw)
	{
		if((type == 1) && windowEffect)
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
				final = gpu->blend(color,T2ReadWord(dst, passing));

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 4;
		}
		else if((gpu->BLDCNT & 0x10) && windowEffect)
		{
			if (gpu->BLDY_EVY != 0x0)
			{
				color = fadeInColors[gpu->BLDY_EVY][color&0x7FFF];
			}

			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 4;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 4;
		}
	}

}

static void setFinalOBJColorSpecialDecreaseWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	bool windowDraw = true, windowEffect = true;

	gpu->renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if((type == 1) && windowEffect)
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
				final = gpu->blend(color,T2ReadWord(dst, passing));

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 4;
		}
		else if((gpu->BLDCNT & 0x10) && windowEffect)
		{
			if (gpu->BLDY_EVY != 0x0)
			{
				color = fadeOutColors[gpu->BLDY_EVY][color&0x7FFF];
			}

			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 4;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 4;
		}
	}
}

/*****************************************************************************/
//			PIXEL RENDERING - 3D
/*****************************************************************************/

static void setFinal3DColorSpecialNone(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	/* We must blend if the 3D layer has the highest prio */
	if((alpha < 16) && gpu->bg0HasHighestPrio)
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		/* If the layer we are drawing on is selected as 2nd source, we can blend */
		if(gpu->BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
				cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
				cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

				final = cfinal.val;
			}
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 0;
	}
}

static void setFinal3DColorSpecialBlend(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	/* We can blend if the 3D layer is selected as 1st target, */
	/* but also if the 3D layer has the highest prio. */
	if((alpha < 16) && ((gpu->BLDCNT & 0x1) || gpu->bg0HasHighestPrio))
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		/* If the layer we are drawing on is selected as 2nd source, we can blend */
		if(gpu->BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
				cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
				cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

				final = cfinal.val;
			}
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		gpu->bgPixels[x] = 0;
	}
}

static void setFinal3DColorSpecialIncrease(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	u16 final = color;

	/* We must blend if the 3D layer has the highest prio */
	/* But it doesn't seem to have priority over fading, */
	/* unlike semi-transparent sprites */
	if((alpha < 16) && gpu->bg0HasHighestPrio)
	{
		int bg_under = gpu->bgPixels[x];

		/* If the layer we are drawing on is selected as 2nd source, we can blend */
		if(gpu->BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
				cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
				cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

				final = cfinal.val;
			}
		}
	}
	
	if(gpu->BLDCNT & 0x1)
	{
		if (gpu->BLDY_EVY != 0x0)
		{
			final = fadeInColors[gpu->BLDY_EVY][final&0x7FFF];
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 0;
	}
}

static void setFinal3DColorSpecialDecrease(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	u16 final = color;

	/* We must blend if the 3D layer has the highest prio */
	/* But it doesn't seem to have priority over fading, */
	/* unlike semi-transparent sprites */
	if((alpha < 16) && gpu->bg0HasHighestPrio)
	{
		int bg_under = gpu->bgPixels[x];

		/* If the layer we are drawing on is selected as 2nd source, we can blend */
		if(gpu->BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
				cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
				cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

				final = cfinal.val;
			}
		}
	}
	
	if(gpu->BLDCNT & 0x1)
	{
		if (gpu->BLDY_EVY != 0x0)
		{
			final = fadeOutColors[gpu->BLDY_EVY][final&0x7FFF];
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (final | 0x8000));
		gpu->bgPixels[x] = 0;
	}
}

static void setFinal3DColorSpecialNoneWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	bool windowDraw = true, windowEffect = true;
	
	gpu->renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		/* We must blend if the 3D layer has the highest prio */
		if((alpha < 16) && gpu->bg0HasHighestPrio)
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			/* If the layer we are drawing on is selected as 2nd source, we can blend */
			if(gpu->BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
					cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
					cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

					final = cfinal.val;
				}
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 0;
		}
	}
}

static void setFinal3DColorSpecialBlendWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	bool windowDraw = true, windowEffect = true;
	
	gpu->renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		/* We can blend if the 3D layer is selected as 1st target, */
		/* but also if the 3D layer has the highest prio. */
		if((alpha < 16) && (((gpu->BLDCNT & 0x1) && windowEffect) || gpu->bg0HasHighestPrio))
		{
			int bg_under = gpu->bgPixels[x];
			u16 final = color;

			/* If the layer we are drawing on is selected as 2nd source, we can blend */
			if(gpu->BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
					cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
					cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

					final = cfinal.val;
				}
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			gpu->bgPixels[x] = 0;
		}
	}
}

static void setFinal3DColorSpecialIncreaseWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	bool windowDraw = true, windowEffect = true;
	u16 final = color;
	
	gpu->renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		/* We must blend if the 3D layer has the highest prio */
		/* But it doesn't seem to have priority over fading, */
		/* unlike semi-transparent sprites */
		if((alpha < 16) && gpu->bg0HasHighestPrio)
		{
			int bg_under = gpu->bgPixels[x];

			/* If the layer we are drawing on is selected as 2nd source, we can blend */
			if(gpu->BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
					cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
					cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

					final = cfinal.val;
				}
			}
		}
		
		if((gpu->BLDCNT & 0x1) && windowEffect)
		{
			if (gpu->BLDY_EVY != 0x0)
			{
				final = fadeInColors[gpu->BLDY_EVY][final&0x7FFF];
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 0;
		}
	}
}

static void setFinal3DColorSpecialDecreaseWnd(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u16 x)
{
	bool windowDraw = true, windowEffect = true;
	u16 final = color;
	
	gpu->renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		/* We must blend if the 3D layer has the highest prio */
		/* But it doesn't seem to have priority over fading, */
		/* unlike semi-transparent sprites */
		if((alpha < 16) && gpu->bg0HasHighestPrio)
		{
			int bg_under = gpu->bgPixels[x];

			/* If the layer we are drawing on is selected as 2nd source, we can blend */
			if(gpu->BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha / 16) + (c2.bits.red * (16 - alpha) / 16));
					cfinal.bits.green = ((c1.bits.green * alpha / 16) + (c2.bits.green * (16 - alpha) / 16));
					cfinal.bits.blue = ((c1.bits.blue * alpha / 16) + (c2.bits.blue * (16 - alpha) / 16));

					final = cfinal.val;
				}
			}
		}
		
		if((gpu->BLDCNT & 0x1) && windowEffect)
		{
			if (gpu->BLDY_EVY != 0x0)
			{
				final = fadeOutColors[gpu->BLDY_EVY][final&0x7FFF];
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (final | 0x8000));
			gpu->bgPixels[x] = 0;
		}
	}
}

//this was forced inline because most of the time it just falls through to setFinalColorBck() and the function call
//overhead was ridiculous and terrible
FORCEINLINE void GPU::__setFinalColorBck(u8 *dst, u16 color, u8 x, bool opaque)
{
	//I commented out this line to make a point.
	//indeed, since x is a u8 we cannot pass in anything >=256
	//but in fact, someone is going to try. specifically, that is the map viewer debug tools
	//which try to render the enter BG. in cases where that is large, it could be up to 1024 wide.
	//I think it survives this truncation to 8bits.
	//assert(x<256);

	int x_int;

	//due to this early out, we will get incorrect behavior in cases where 
	//we enable mosaic in the middle of a frame. this is deemed unlikely.
	if(!curr_mosaic_enabled) {
		if(opaque) goto finish;
		else return;
	}

	if(!opaque) color = 0xFFFF;
	else color &= 0x7FFF;

	//due to the early out, enabled must always be true
	//x_int = enabled ? GPU::mosaicLookup.width[x].trunc : x;
	x_int = GPU::mosaicLookup.width[x].trunc;

	if(GPU::mosaicLookup.width[x].begin && GPU::mosaicLookup.height[currLine].begin) {}
	else color = mosaicColors.bg[currBgNum][x_int];
	mosaicColors.bg[currBgNum][x] = color;

	if(color != 0xFFFF)
	{
finish:
		setFinalColorBck(this,dst,color,x);
	}
}

//this is fantastically inaccurate.
//we do the early return even though it reduces the resulting accuracy
//because we need the speed, and because it is inaccurate anyway
static void mosaicSpriteLinePixel(GPU * gpu, int x, u16 l, u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
{
	int x_int;
	u8 y = l;

	_OAM_ * spriteInfo = (_OAM_ *)(gpu->oam + gpu->sprNum[x]);
	bool enabled = spriteInfo->Mosaic;
	if(!enabled)
		return;

	bool opaque = prioTab[x] <= 4;

	GPU::MosaicColor::Obj objColor;
	objColor.color = T1ReadWord(dst,x<<1);
	objColor.alpha = dst_alpha[x];
	objColor.opaque = opaque;

	x_int = enabled ? GPU::mosaicLookup.width[x].trunc : x;

	if(enabled)
	{
		if(GPU::mosaicLookup.width[x].begin && GPU::mosaicLookup.height[y].begin) {}
		else objColor = gpu->mosaicColors.obj[x_int];
	}
	gpu->mosaicColors.obj[x] = objColor;

	T1WriteWord(dst,x<<1,objColor.color);
	dst_alpha[x] = objColor.alpha;
	if(!objColor.opaque) prioTab[x] = 0xFF;
}

static void mosaicSpriteLine(GPU * gpu, u16 l, u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
{
	//don't even try this unless the mosaic is effective
	if(gpu->mosaicLookup.widthValue != 0 || gpu->mosaicLookup.heightValue != 0)
		for(int i=0;i<256;i++)
			mosaicSpriteLinePixel(gpu,i,l,dst,dst_alpha,typeTab,prioTab);
}


/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
INLINE void renderline_textBG(GPU * gpu, u8 num, u8 * dst, u32 Y, u16 XBG, u16 YBG, u16 LG)
{
	gpu->currBgNum = num;

	struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	u16 lg     = gpu->BGSize[num][0];
	u16 ht     = gpu->BGSize[num][1];
	u16 wmask  = (lg-1);
	u16 hmask  = (ht-1);
	u16 tmp    = ((YBG & hmask) >> 3);
	u8 *map    = NULL;
	u8 *tile, *pal, *line;
	u16 color;
	u16 xoff;
	u16 yoff;
	u16 x      = 0;
	u16 xfin;

	s8 line_dir = 1;
	u8 * mapinfo;
	TILEENTRY tileentry;

	u32 tmp_map = gpu->BG_map_ram[num] + (tmp&31) * 64;
	if(tmp>31) 
		tmp_map+= ADDRESS_STEP_512B << bgCnt->ScreenSize ;

	map = (u8*)MMU_RenderMapToLCD(tmp_map);
	if(!map) return; 	// no map
	
	tile = (u8*) MMU_RenderMapToLCD(gpu->BG_tile_ram[num]);
	if(!tile) return; 	// no tiles

	xoff = XBG;
	pal = ARM9Mem.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB;

	if(!bgCnt->Palette_256)    // color: 16 palette entries
	{
		yoff = ((YBG&7)<<2);
		xfin = 8 - (xoff&7);
		for(x = 0; x < LG; xfin = std::min<u16>(x+8, LG))
		{
			u16 tilePalette = 0;
			tmp = ((xoff&wmask)>>3);
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

					if(!(xoff&1))
					{
						color = T1ReadWord(pal, ((currLine>>4) + tilePalette) << 1);
						gpu->__setFinalColorBck(dst,color,x,currLine>>4);
						dst += 2; x++; xoff++;
					}
					
					color = T1ReadWord(pal, ((currLine&0xF) + tilePalette) << 1);
					gpu->__setFinalColorBck(dst,color,x,currLine&0xF);
					dst += 2; x++; xoff++;
				}
			} else {
				line += ((xoff&7)>>1);
				for(; x < xfin; line ++) 
				{
					u8 currLine = *line;

					if(!(xoff&1))
					{
						color = T1ReadWord(pal, ((currLine&0xF) + tilePalette) << 1);
						gpu->__setFinalColorBck(dst,color,x,currLine&0xF);

						dst += 2; x++; xoff++;
					}

					color = T1ReadWord(pal, ((currLine>>4) + tilePalette) << 1);
					gpu->__setFinalColorBck(dst,color,x,currLine>>4);
					dst += 2; x++; xoff++;
				}
			}
		}
		return;
	}

	//256-color BG

	if(dispCnt->ExBGxPalette_Enable)  // color: extended palette
	{
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
			if(dispCnt->ExBGxPalette_Enable)
				color = T1ReadWord(pal, ((*line) + (tileentry.bits.Palette<<8)) << 1);
			else
				color = T1ReadWord(pal, (*line) << 1);

			gpu->__setFinalColorBck(dst,color,x,*line);
			
			dst += 2; x++; xoff++;

			line += line_dir;
		}
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -ROTOSCALE-
/*****************************************************************************/

FORCEINLINE void rot_tiled_8bit_entry(GPU * gpu, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H, u8 extPal) {
	u8 palette_entry;
	u16 tileindex, x, y, color;
	
	tileindex = map[(auxX>>3) + (auxY>>3) * (lg>>3)];
	x = (auxX&7); y = (auxY&7);

	palette_entry = tile[(tileindex<<6)+(y<<3)+x];
	color = T1ReadWord(pal, palette_entry << 1);
	gpu->__setFinalColorBck(dst, color,i,palette_entry);
}

FORCEINLINE void rot_tiled_16bit_entry(GPU * gpu, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H, u8 extPal) {
	u8 palette_entry;
	u16 x, y, color;
	TILEENTRY tileentry;

	if (!tile) return;
	tileentry.val = T1ReadWord(map, ((auxX>>3) + (auxY>>3) * (lg>>3))<<1);
	x = (tileentry.bits.HFlip) ? 7 - (auxX&7) : (auxX&7);
	y = (tileentry.bits.VFlip) ? 7 - (auxY&7) : (auxY&7);

	palette_entry = tile[(tileentry.bits.TileNum<<6)+(y<<3)+x];
	color = T1ReadWord(pal, (palette_entry + (extPal ? (tileentry.bits.Palette<<8) : 0)) << 1);
	gpu->__setFinalColorBck(dst, color, i, palette_entry);
}

FORCEINLINE void rot_256_map(GPU * gpu, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H, u8 extPal) {
	u8 palette_entry;
	u16 color;

	palette_entry = map[auxX + auxY * lg];
	color = T1ReadWord(pal, palette_entry << 1);
	gpu->__setFinalColorBck(dst, color, i, palette_entry);

}

FORCEINLINE void rot_BMP_map(GPU * gpu, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal, int i, u16 H, u8 extPal) {
	u16 color;

	color = T1ReadWord(map, (auxX + auxY * lg) << 1);
	gpu->__setFinalColorBck(dst, color, i, color&0x8000);

}

typedef void (*rot_fun)(GPU * gpu, s32 auxX, s32 auxY, int lg, u8 * dst, u8 * map, u8 * tile, u8 * pal , int i, u16 H, u8 extPal);

template<rot_fun fun>
FORCEINLINE void rot_scale_op(GPU * gpu, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG, s32 wh, s32 ht, BOOL wrap, u8 * map, u8 * tile, u8 * pal, u8 extPal)
{
	ROTOCOORD x, y;

	s32 dx = (s32)PA;
	s32 dy = (s32)PC;
	u32 i;
	s32 auxX, auxY;
	
	x.val = X;
	y.val = Y;

	for(i = 0; i < LG; ++i)
	{
		auxX = x.bits.Integer;
		auxY = y.bits.Integer;
	
		if(wrap)
		{
			// wrap
			auxX = auxX & (wh-1);
			auxY = auxY & (ht-1);
		}
		
		if ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht))
			fun(gpu, auxX, auxY, wh, dst, map, tile, pal, i, H, extPal);
		dst += 2;
		x.val += dx;
		y.val += dy;
	}
}

template<rot_fun fun>
FORCEINLINE void apply_rot_fun(GPU * gpu, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG, u8 * map, u8 * tile, u8 * pal, u8 extPal)
{
	struct _BGxCNT * bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[gpu->currBgNum].bits;
	s32 wh = gpu->BGSize[gpu->currBgNum][0];
	s32 ht = gpu->BGSize[gpu->currBgNum][1];
	rot_scale_op<fun>(gpu, dst, H, X, Y, PA, PB, PC, PD, LG, wh, ht, bgCnt->PaletteSet_Wrap, map, tile, pal, extPal);	
}


FORCEINLINE void rotBG2(GPU * gpu, u8 num, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG)
{
	gpu->currBgNum = num;
	u8 * map = (u8 *)MMU_RenderMapToLCD(gpu->BG_map_ram[num]);
	if (!map) return;
	u8 * tile = (u8 *)MMU_RenderMapToLCD(gpu->BG_tile_ram[num]);
	if (!tile) return;
	u8 * pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
//	printf("rot mode\n");
	apply_rot_fun<rot_tiled_8bit_entry>(gpu, dst, H,X,Y,PA,PB,PC,PD,LG, map, tile, pal, 0);
}

FORCEINLINE void extRotBG2(GPU * gpu, u8 num, u8 * dst, u16 H, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, s16 LG)
{
	gpu->currBgNum = num;
	struct _BGxCNT * bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	
	u8 *map, *tile, *pal;
	u8 affineModeSelection ;

	/* see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol  */
	affineModeSelection = (bgCnt->Palette_256 << 1) | (bgCnt->CharacBase_Block & 1) ;
//	printf("extrot mode %d\n", affineModeSelection);
	switch(affineModeSelection)
	{
	case 0 :
	case 1 :
		map = (u8 *)MMU_RenderMapToLCD(gpu->BG_map_ram[num]);
		if (!map) return;
		tile = (u8 *)MMU_RenderMapToLCD(gpu->BG_tile_ram[num]);
		if (!tile) return;
		if(dispCnt->ExBGxPalette_Enable)
			pal = ARM9Mem.ExtPal[gpu->core][gpu->BGExtPalSlot[num]];
		else
			pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		if (!pal) return;
		// 16  bit bgmap entries
		apply_rot_fun<rot_tiled_16bit_entry>(gpu, dst, H,X,Y,PA,PB,PC,PD,LG, map, tile, pal, dispCnt->ExBGxPalette_Enable);
		return;
	case 2 :
		// 256 colors 
		map = (u8 *)MMU_RenderMapToLCD(gpu->BG_bmp_ram[num]);
		if (!map) return;
		pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		apply_rot_fun<rot_256_map>(gpu, dst, H,X,Y,PA,PB,PC,PD,LG, map, NULL, pal, 0);
		return;
	case 3 :
		// direct colors / BMP
		map = (u8 *)MMU_RenderMapToLCD(gpu->BG_bmp_ram[num]);
		if (!map) return;
		apply_rot_fun<rot_BMP_map>(gpu, dst, H,X,Y,PA,PB,PC,PD,LG, map, NULL, NULL, 0);
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
	 parms->BGxX += parms->BGxPB;
	 parms->BGxY += parms->BGxPD;
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
	 parms->BGxX += parms->BGxPB;
	 parms->BGxY += parms->BGxPD;
}

void textBG(GPU * gpu, u8 num, u8 * DST)
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
INLINE void render_sprite_BMP (GPU * gpu, u8 spriteNum, u16 l, u8 * dst, u16 * src, u8 * dst_alpha, u8 * typeTab, u8 * prioTab, 
							   u8 prio, int lg, int sprX, int x, int xdir, u8 alpha) 
{
	int i; u16 color;
	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		color = LE_TO_LOCAL_16(src[x]);

		// alpha bit = invisible
		if ((color&0x8000)&&(prio<=prioTab[sprX]))
		{
			/* if we don't draw, do not set prio, or else */
		//	if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, color, sprX))
			{
				T2WriteWord(dst, (sprX<<1), color);
				dst_alpha[sprX] = alpha;
				typeTab[sprX] = 3;
				prioTab[sprX] = prio;
				gpu->sprNum[sprX] = spriteNum;
			}
		}
	}
}

INLINE void render_sprite_256 (	GPU * gpu, u8 spriteNum, u16 l, u8 * dst, u8 * src, u16 * pal, 
								u8 * dst_alpha, u8 * typeTab, u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir, u8 alpha)
{
	int i; 
	u8 palette_entry; 
	u16 color;

	for(i = 0; i < lg; i++, ++sprX, x+=xdir)
	{
		palette_entry = src[(x&0x7) + ((x&0xFFF8)<<3)];
		color = LE_TO_LOCAL_16(pal[palette_entry]);

		// palette entry = 0 means backdrop
		if ((palette_entry>0)&&(prio<=prioTab[sprX]))
		{
			/* if we don't draw, do not set prio, or else */
			//if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, color, sprX))
			{
				T2WriteWord(dst, (sprX<<1), color);
				dst_alpha[sprX] = 16;
				typeTab[sprX] = (alpha ? 1 : 0);
				prioTab[sprX] = prio;
				gpu->sprNum[sprX] = spriteNum;
			}
		}
	}
}

INLINE void render_sprite_16 (	GPU * gpu, u16 l, u8 * dst, u8 * src, u16 * pal, 
								u8 * dst_alpha, u8 * typeTab, u8 * prioTab, u8 prio, int lg, int sprX, int x, int xdir, u8 alpha)
{
	int i; 
	u8 palette, palette_entry;
	u16 color, x1;

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
			//if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, color, sprX ))
			{
				T2WriteWord(dst, (sprX<<1), color);
				dst_alpha[sprX] = 16;
				typeTab[sprX] = (alpha ? 1 : 0);
				prioTab[sprX] = prio;
			}
		}
	}
}

INLINE void render_sprite_Win (GPU * gpu, u16 l, u8 * src,
	int col256, int lg, int sprX, int x, int xdir) {
	int i; u8 palette, palette_entry;
	u16 x1;
	if (col256) {
		for(i = 0; i < lg; i++, sprX++,x+=xdir)
			//sprWin[sprX] = (src[x])?1:0;
			if(src[x])
				sprWin[sprX] = 1;
	} else {
		for(i = 0; i < lg; i++, ++sprX, x+=xdir)
		{
			x1 = x>>1;
			palette = src[(x1&0x3) + ((x1&0xFFFC)<<3)];
			if (x & 1) palette_entry = palette >> 4;
			else       palette_entry = palette & 0xF;
			//sprWin[sprX] = (palette_entry)?1:0;
			if(palette_entry)
				sprWin[sprX] = 1;
		}
	}
}

// return val means if the sprite is to be drawn or not
FORCEINLINE BOOL compute_sprite_vars(_OAM_ * spriteInfo, u16 l, 
	size &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, int &xdir) {

	x = 0;
	// get sprite location and size
	sprX = (spriteInfo->X/*<<23*/)/*>>23*/;
	sprY = spriteInfo->Y;
	sprSize = sprSizeTab[spriteInfo->Size][spriteInfo->Shape];

	lg = sprSize.x;
	
	if (sprY>=192)
		sprY = (s32)((s8)(spriteInfo->Y));
	
// FIXME: for rot/scale, a list of entries into the sprite should be maintained,
// that tells us where the first pixel of a screenline starts in the sprite,
// and how a step to the right in a screenline translates within the sprite

	if ((l<sprY)||(l>=sprY+sprSize.y) ||	/* sprite lines outside of screen */
		(sprX==256)||(sprX+sprSize.x<=0))	/* sprite pixels outside of line */
		return FALSE;				/* not to be drawn */

	// sprite portion out of the screen (LEFT)
	if(sprX<0)
	{
		lg += sprX;	
		x = -(sprX);
		sprX = 0;
	}
	// sprite portion out of the screen (RIGHT)
	if (sprX+sprSize.x >= 256)
		lg = 256 - sprX;

	y = l - sprY;                           /* get the y line within sprite coords */

	// switch TOP<-->BOTTOM
	if (spriteInfo->VFlip)
		y = sprSize.y - y -1;
	
	// switch LEFT<-->RIGHT
	if (spriteInfo->HFlip) {
		x = sprSize.x - x -1;
		xdir  = -1;
	} else {
		xdir  = 1;
	}
	return TRUE;
}

/*****************************************************************************/
//			SPRITE RENDERING
/*****************************************************************************/
void sprite1D(GPU * gpu, u16 l, u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
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
		u16 j;

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
					lg = 256 - sprX;
			}

			// If we are using 1 palette of 256 colours
			if(spriteInfo->Depth)
			{
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex << block));
				CHECK_SPRITE(1);

				// If extended palettes are set, use them
				if (dispCnt->ExOBJPalette_Enable)
					pal = (ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);

				for(j = 0; j < lg; ++j, ++sprX)
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
							//if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, T1ReadWord(pal, colour<<1), sprX ))
							{
								T2WriteWord(dst, (sprX<<1), T2ReadWord(pal, (colour<<1)));
								dst_alpha[sprX] = 16;
								typeTab[sprX] = spriteInfo->Mode;
								prioTab[sprX] = prio;
							}
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
				if (dispCnt->OBJ_BMP_mapping)
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex)*32);
				else
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x03E0) * 8) + (spriteInfo->TileIndex&0x001F))*16);
				CHECK_SPRITE(1);

				for(j = 0; j < lg; ++j, ++sprX)
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
						//	if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, colour, sprX))
							{
								T2WriteWord(dst, (sprX<<1), colour);
								dst_alpha[sprX] = spriteInfo->PaletteIndex;
								typeTab[sprX] = spriteInfo->Mode;
								prioTab[sprX] = prio;
							}
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
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<gpu->sprBoundary));
				CHECK_SPRITE(1);

				for(j = 0; j < lg; ++j, ++sprX)
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
							//if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, T1ReadWord(pal, colour<<1), sprX ))
							{
								T2WriteWord(dst, (sprX<<1), T2ReadWord(pal, (colour<<1)));
								dst_alpha[sprX] = 16;
								typeTab[sprX] = spriteInfo->Mode;
								prioTab[sprX] = prio;
							}
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

			if (!compute_sprite_vars(spriteInfo, l, sprSize, sprX, sprY, x, y, lg, xdir))
				continue;

			if (spriteInfo->Mode == 2)
			{
				if (spriteInfo->Depth)
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8));
				else
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4));
				CHECK_SPRITE(1);

				render_sprite_Win (gpu, l, src, spriteInfo->Depth, lg, sprX, x, xdir);
				continue;
			}

			if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
			{
				if (dispCnt->OBJ_BMP_mapping)
				{
					// TODO: fix it for sprite1D
					//src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1));
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<4) + (y<<gpu->sprBMPBoundary));
				}
				else
				{
					if (dispCnt->OBJ_BMP_2D_dim) // 256*256
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1));
					else // 128 * 512
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3F0) * 64  + (spriteInfo->TileIndex&0x0F) *8 + ( y << 8)) << 1));
				}
				CHECK_SPRITE(1);

				render_sprite_BMP (gpu, i, l, dst, (u16*)src, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->PaletteIndex);
				continue;
			}
				
			if(spriteInfo->Depth)                   /* 256 colors */
			{
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8));
				CHECK_SPRITE(1);
		
				if (dispCnt->ExOBJPalette_Enable)
					pal = (u16*)(ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
		
				//sprwin test hack - to enable, only draw win and not sprite
				render_sprite_256 (gpu, i, l, dst, src, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);
				//render_sprite_Win (gpu, l, src, spriteInfo->Depth, lg, sprX, x, xdir);

				continue;
			}
			/* 16 colors */
			src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4));
			CHECK_SPRITE(1);
			pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400);
			
			pal += (spriteInfo->PaletteIndex<<4);
			
			//sprwin test hack - to enable, only draw win and not sprite
			render_sprite_16 (gpu, l, dst, src, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);
			//render_sprite_Win (gpu, l, src, spriteInfo->Depth, lg, sprX, x, xdir);
		}
	}

#ifdef WORDS_BIGENDIAN
	*(((u16*)spriteInfo)+1) = (*(((u16*)spriteInfo)+1) << 1) | *(((u16*)spriteInfo)+1) >> 15;
	*(((u16*)spriteInfo)+2) = (*(((u16*)spriteInfo)+2) << 2) | *(((u16*)spriteInfo)+2) >> 14;
#endif
}

void sprite2D(GPU * gpu, u16 l, u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
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
		u16 j;

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
					lg = 256 - sprX;
			}

			// If we are using 1 palette of 256 colours
			if(spriteInfo->Depth)
			{
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex) << 5));
				CHECK_SPRITE(2);

				// If extended palettes are set, use them
				if (dispCnt->ExOBJPalette_Enable)
					pal = (ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
			
				for(j = 0; j < lg; ++j, ++sprX)
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
						//	if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, T1ReadWord(pal, colour<<1), sprX ))
							{
								T2WriteWord(dst, (sprX<<1), T2ReadWord(pal, (colour<<1)));
								dst_alpha[sprX] = 16;
								typeTab[sprX] = spriteInfo->Mode;
								prioTab[sprX] = prio;
							}
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
				if (dispCnt->OBJ_BMP_mapping)
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex)*32);
				else
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x03E0) * 8) + (spriteInfo->TileIndex&0x001F))*16);
				CHECK_SPRITE(2);

				for(j = 0; j < lg; ++j, ++sprX)
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
						//	if (gpu->setFinalColorSpr(gpu, sprX << 1, 4, dst, colour, sprX ))
							{
								T2WriteWord(dst, (sprX<<1), colour);
								dst_alpha[sprX] = spriteInfo->PaletteIndex;
								typeTab[sprX] = spriteInfo->Mode;
								prioTab[sprX] = prio;
							}
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
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<5));
				CHECK_SPRITE(2);
				pal = ARM9Mem.ARM9_VMEM + 0x200 + (gpu->core*0x400 + (spriteInfo->PaletteIndex*32));
				
				for(j = 0; j < lg; ++j, ++sprX)
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
						//	if (gpu->setFinalColorSpr(gpu, sprX << 1,4,dst, T1ReadWord (pal, colour<<1), sprX))
							{
								T2WriteWord(dst, (sprX<<1), T2ReadWord(pal, (colour<<1)));
								dst_alpha[sprX] = 16;
								typeTab[sprX] = spriteInfo->Mode;
								prioTab[sprX] = prio;
							}
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

			if (!compute_sprite_vars(spriteInfo, l, sprSize, sprX, sprY, x, y, lg, xdir))
				continue;

			if (spriteInfo->Mode == 2) {
				if (spriteInfo->Depth)
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8));
				else
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4));
				CHECK_SPRITE(2);

				render_sprite_Win (gpu, l, src, spriteInfo->Depth, lg, sprX, x, xdir);
				continue;
			}

			if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
			{
				if (dispCnt->OBJ_BMP_mapping)
				{
					// TODO: fix it for sprite1D
					//src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1));
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<4) + (y<<gpu->sprBMPBoundary));
				}
				else
				{
					if (dispCnt->OBJ_BMP_2D_dim) // 256*256
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1));
					else // 128 * 512
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3F0) * 64  + (spriteInfo->TileIndex&0x0F) *8 + ( y << 8)) << 1));
				}
				CHECK_SPRITE(2);

				render_sprite_BMP (gpu, i, l, dst, (u16*)src, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->PaletteIndex);

				continue;
			}
				
			if(spriteInfo->Depth)                   /* 256 colors */
			{
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8));
				CHECK_SPRITE(2);

				if (dispCnt->ExOBJPalette_Enable)
					pal = (u16*)(ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
		
				render_sprite_256 (gpu, i, l, dst, src, pal,
					dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);

				continue;
			}
		
			/* 16 colors */
			src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4));
			CHECK_SPRITE(2);
			pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400);

			pal += (spriteInfo->PaletteIndex<<4);
			render_sprite_16 (gpu, l, dst, src, pal,
				dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);
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

void GPU_set_DISPCAPCNT(u32 val)
{
	GPU * gpu = MainScreen.gpu;	
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;

	gpu->dispCapCnt.val = val;
	gpu->dispCapCnt.EVA = val & 0x1F;
	gpu->dispCapCnt.EVB = (val >> 8) & 0x1F;
	gpu->dispCapCnt.writeBlock =  (val >> 16) & 0x03;
	gpu->dispCapCnt.writeOffset = (val >> 18) & 0x03;
	gpu->dispCapCnt.readBlock = dispCnt->VRAM_Block;

	if (dispCnt->DisplayMode == 2)
		gpu->dispCapCnt.readOffset = 0;
	else
		gpu->dispCapCnt.readOffset = (val >> 26) & 0x03;
	
	gpu->dispCapCnt.srcA = (val >> 24) & 0x01;
	gpu->dispCapCnt.srcB = (val >> 25) & 0x01;
	gpu->dispCapCnt.capSrc = (val >> 29) & 0x03;

	gpu->dispCapCnt.dst = (ARM9Mem.ARM9_LCD + 
							(gpu->dispCapCnt.writeBlock * 0x20000) +
							(gpu->dispCapCnt.writeOffset * 0x8000)
							);
	gpu->dispCapCnt.src = (ARM9Mem.ARM9_LCD + 
							(gpu->dispCapCnt.readBlock * 0x20000) +
							(gpu->dispCapCnt.readOffset * 0x8000)
							);

	switch((val >> 20) & 0x03)
	{
		case 0:
			gpu->dispCapCnt.capx = 128;
			gpu->dispCapCnt.capy = 128;
			break;
		case 1:
			gpu->dispCapCnt.capx = 256;
			gpu->dispCapCnt.capy = 64;
			break;
		case 2:
			gpu->dispCapCnt.capx = 256;
			gpu->dispCapCnt.capy = 128;
			break;
		case 3:
			gpu->dispCapCnt.capx = 256;
			gpu->dispCapCnt.capy = 192;
			break;
	}

	/*INFO("Capture 0x%X:\n EVA=%i, EVB=%i, wBlock=%i, wOffset=%i, capX=%i, capY=%i\n rBlock=%i, rOffset=%i, srcCap=%i, dst=0x%X, src=0x%X\n srcA=%i, srcB=%i\n\n",
			val, gpu->dispCapCnt.EVA, gpu->dispCapCnt.EVB, gpu->dispCapCnt.writeBlock, gpu->dispCapCnt.writeOffset,
			gpu->dispCapCnt.capx, gpu->dispCapCnt.capy, gpu->dispCapCnt.readBlock, gpu->dispCapCnt.readOffset, 
			gpu->dispCapCnt.capSrc, gpu->dispCapCnt.dst - ARM9Mem.ARM9_LCD, gpu->dispCapCnt.src - ARM9Mem.ARM9_LCD,
			gpu->dispCapCnt.srcA, gpu->dispCapCnt.srcB);*/
}
// #define BRIGHT_TABLES


static void GPU_ligne_layer(NDS_Screen * screen, u16 l)
{
	GPU * gpu = screen->gpu;
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	u8 * dst = (u8 *)(GPU_screen) + (screen->offset + l) * 512;
	itemsForPriority_t * item;
	u8 spr[512];
	u8 sprAlpha[256];
	u8 sprType[256];
	u8 sprPrio[256];
	u8 prio;
	u16 i16;
	u32 c;
	BOOL BG_enabled  = TRUE;

	c = T1ReadWord(ARM9Mem.ARM9_VMEM, gpu->core * 0x400) & 0x7FFF;

	/* Apply fading to backdrop */
	if((gpu->BLDCNT & 0x20) && (gpu->BLDY_EVY > 0))
	{
		switch(gpu->BLDCNT & 0xC0)
		{
		case 0x80:	/* Fade in */
			c = fadeInColors[gpu->BLDY_EVY][c];
			break;
		case 0xC0:	/* Fade out */
			c = fadeOutColors[gpu->BLDY_EVY][c];
			break;
		default: break;
		}
	}

	for(int i = 0; i< 256; ++i) T2WriteWord(dst, i << 1, c);

	/* reset them to backdrop */
	memset(gpu->bgPixels, 5, 256);

	if (!gpu->LayersEnable[0] && !gpu->LayersEnable[1] && 
			!gpu->LayersEnable[2] && !gpu->LayersEnable[3] && 
				!gpu->LayersEnable[4]) return;

	// init background color & priorities
	memset(sprAlpha, 0, 256);
	memset(sprType, 0, 256);
	memset(sprPrio,0xFF,256);
	memset(sprWin,0,256);
	
	// init pixels priorities
	for (int i=0; i<NB_PRIORITIES; i++) {
		gpu->itemsForPriority[i].nbPixelsX = 0;
	}
	
	// for all the pixels in the line
	if (gpu->LayersEnable[4]) 
	{
		for(int i = 0; i< 256; ++i) T2WriteWord(spr, i << 1, c);
		gpu->spriteRender(gpu, l, spr, sprAlpha, sprType, sprPrio);
		mosaicSpriteLine(gpu, l, spr, sprAlpha, sprType, sprPrio);


		for(int i = 0; i<256; i++) 
		{
			// assign them to the good priority item
			prio = sprPrio[i];
			if (prio >=4) continue;
			
			item = &(gpu->itemsForPriority[prio]);
			item->PixelsX[item->nbPixelsX]=i;
			item->nbPixelsX++;
		}
	}

	
	if (!gpu->LayersEnable[0] && !gpu->LayersEnable[1] && !gpu->LayersEnable[2] && !gpu->LayersEnable[3])
		BG_enabled = FALSE;

	// paint lower priorities fist
	// then higher priorities on top
	for(prio=NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(gpu->itemsForPriority[prio]);
		// render BGs
		if (BG_enabled)
		{
			for (int i=0; i < item->nbBGs; i++) 
			{
				i16 = item->BGs[i];
				if (gpu->LayersEnable[i16])
				{

					struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[i16].bits;
					gpu->curr_mosaic_enabled = bgCnt->Mosaic_Enable;

					//mosaic test hacks
					//gpu->curr_mosaic_enabled = true;

					if (gpu->core == GPU_MAIN)
					{
						if (i16 == 0 && dispCnt->BG0_3D)
						{
							gpu->currBgNum = 0;
							u16 line3Dcolor[512];
							u8 line3Dalpha[512];

							memset(line3Dcolor, 0, sizeof(line3Dcolor));
							memset(line3Dalpha, 0, sizeof(line3Dalpha));

							BGxOFS *bgofs = &gpu->dispx_st->dispx_BGxOFS[i16];
							u16 hofs = (T1ReadWord((u8*)&bgofs->BGxHOFS, 0) & 0x1FF);
							
							gpu3D->NDS_3D_GetLine(l, 0, 255, line3Dcolor, line3Dalpha);

							for(int k = 0; k < 256; k++)
							{
								int q = ((k + hofs) & 0x1FF);

								if(line3Dcolor[q] & 0x8000)
									gpu->setFinalColor3D(gpu, (k << 1), dst, line3Dcolor[q], line3Dalpha[q], k);
							}

							continue;
						}
					}
					modeRender[dispCnt->BG_Mode][i16](gpu, i16, l, dst);
				}
			}
		}

		// render sprite Pixels
		if (gpu->LayersEnable[4])
		{
			gpu->currBgNum = 4;
			////analyze mosaic configuration
			//u16 mosaic_control = T1ReadWord((u8 *)&gpu->dispx_st->dispx_MISC.MOSAIC, 0);
			//gpu->curr_mosaic_enabled

			for (int i=0; i < item->nbPixelsX; i++)
			{
				i16=item->PixelsX[i];
			//	T2WriteWord(dst, i16 << 1, T2ReadWord(spr, i16 << 1));
			//	gpu->bgPixels[i16] = 4;
				gpu->setFinalColorSpr(gpu, (i16<<1), dst, T2ReadWord(spr, (i16<<1)), sprAlpha[i16], sprType[i16], i16);
			}
		}
	}
}

// TODO: capture emulated not fully
static void GPU_ligne_DispCapture(u16 l)
{
	GPU * gpu = MainScreen.gpu;

	if (l == 0)
	{
		if (gpu->dispCapCnt.val & 0x80000000)
		{
			gpu->dispCapCnt.enabled = TRUE;
			T1WriteLong(ARM9Mem.ARM9_REG, 0x64, gpu->dispCapCnt.val);
		}
	}

	if (gpu->dispCapCnt.enabled)
	{
		u8	*cap_dst = (u8 *)(gpu->dispCapCnt.dst) + (l * 512);

		if (l < gpu->dispCapCnt.capy)
		{
			// TODO: Read/Write block wrap to 00000h when exceeding 1FFFFh (128k)

			switch (gpu->dispCapCnt.capSrc)
			{
				case 0:		// Capture source is SourceA
					{
						//INFO("Capture source is SourceA\n");
						switch (gpu->dispCapCnt.srcA)
						{
							case 0:			// Capture screen (BG + OBJ + 3D)
								{
									//INFO("Capture screen (BG + OBJ + 3D)\n");

									//here we have a special hack. if the main screen is in an unusual display mode
									//(other than the regular layer combination mode)
									//then we havent combined layers including the 3d.
									//in that case, we need to skip straight to the capture 3d only
									if(MainScreen.gpu->dispMode != 1)
										goto cap3d;

									u8 *src = (u8 *)(GPU_screen) + (MainScreen.offset + l) * 512;
									for (int i = 0; i < gpu->dispCapCnt.capx; i++)
										T2WriteWord(cap_dst, i << 1, T2ReadWord(src, i << 1) | (1<<15));

								}
							break;
							case 1:			// Capture 3D
								{
									//INFO("Capture 3D\n");
								cap3d:
									u16 cap3DLine[512];
									gpu3D->NDS_3D_GetLineCaptured(l, (u16*)cap3DLine);
									for (int i = 0; i < gpu->dispCapCnt.capx; i++)
										T2WriteWord(cap_dst, i << 1, (u16)cap3DLine[i]);
								}
							break;
						}
					}
				break;
				case 1:		// Capture source is SourceB
					{
						//INFO("Capture source is SourceB\n");
						switch (gpu->dispCapCnt.srcB)
						{
							case 0:			// Capture VRAM
								{
									//INFO("Capture VRAM\n");
									u8 *src = (u8 *)(gpu->dispCapCnt.src) + (l * 512);
									for (int i = 0; i < gpu->dispCapCnt.capx; i++)
										T2WriteWord(cap_dst, i << 1, T2ReadWord(src, i << 1) | (1<<15));
								}
								break;
							case 1:			// Capture Main Memory Display FIFO
								{
									//INFO("Capture Main Memory Display FIFO\n");
								}
								break;
						}
					}
				break;
				default:	// Capture source is SourceA+B blended
					{
						//INFO("Capture source is SourceA+B blended\n");
						u16 *srcA = NULL;
						u16 *srcB = NULL;
						u16 cap3DLine[512];

						if (gpu->dispCapCnt.srcA == 0)			// Capture screen (BG + OBJ + 3D)
							srcA = (u16 *)(GPU_screen) + (MainScreen.offset + l) * 512;
						else
						{
							gpu3D->NDS_3D_GetLineCaptured(l, (u16*)cap3DLine);
							srcA = (u16 *)cap3DLine; // 3D screen
						}

						if (gpu->dispCapCnt.srcB == 0)			// VRAM screen
							srcB = (u16 *)(gpu->dispCapCnt.src) + (l * 512);
						else
							srcB = NULL; // DISP FIFOS

						if ((srcA) && (srcB))
						{
							u16 a, r, g, b;
							for(u16 i = 0; i < gpu->dispCapCnt.capx; i++) 
							{
								a = r = g = b =0;

								if (gpu->dispCapCnt.EVA && (srcA[i] & 0x8000))
								{
									a = 0x8000;
									r = ((srcA[i] & 0x1F) * gpu->dispCapCnt.EVA);
									g = (((srcA[i] >>  5) & 0x1F) * gpu->dispCapCnt.EVA);
									b = (((srcA[i] >>  10) & 0x1F) * gpu->dispCapCnt.EVA);
								}

								if (gpu->dispCapCnt.EVB && (srcB[i] & 0x8000))
								{
									a = 0x8000;
									r += ((srcB[i] & 0x1F) * gpu->dispCapCnt.EVB);
									g += (((srcB[i] >>  5) & 0x1F) * gpu->dispCapCnt.EVB);
									b += (((srcB[i] >> 10) & 0x1F) * gpu->dispCapCnt.EVB);
								}

								r >>= 4;
								g >>= 4;
								b >>= 4;

								T2WriteWord(cap_dst, i << 1, (u16)(a | (b << 10) | (g << 5) | r));
							}
						}
					}
				break;
			}
		}

		if (l>=191)
		{
			gpu->dispCapCnt.enabled = FALSE;
			gpu->dispCapCnt.val &= 0x7FFFFFFF;
			T1WriteLong(ARM9Mem.ARM9_REG, 0x64, gpu->dispCapCnt.val);
			return;
		}
	}
}

static INLINE void GPU_ligne_MasterBrightness(NDS_Screen * screen, u16 l)
{
	GPU * gpu = screen->gpu;

	u8 * dst =  GPU_screen + (screen->offset + l) * 512;
	u16 i16;

	if (!gpu->MasterBrightFactor) return;

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
			for(i16 = 0; i16 < 256; ++i16)
			{
				((u16*)dst)[i16] = fadeInColors[gpu->MasterBrightFactor][((u16*)dst)[i16]&0x7FFF];
			}
			break;
		}

		// Bright down
		case 2:
		{
			for(i16 = 0; i16 < 256; ++i16)
			{
				((u16*)dst)[i16] = fadeOutColors[gpu->MasterBrightFactor][((u16*)dst)[i16]&0x7FFF];
			}
			break;
		}

		// Reserved
		case 3:
			break;
	 }

}

template<int WIN_NUM>
FORCEINLINE void GPU::setup_windows()
{
	u8 y = currLine;
	u16 startY,endY;

	if(WIN_NUM==0)
	{
		startY = WIN0V0;
		endY = WIN0V1;
	}
	else
	{
		startY = WIN1V0;
		endY = WIN1V1;
	}

	if(WIN_NUM == 0 && !WIN0_ENABLED) goto allout;
	if(WIN_NUM == 1 && !WIN1_ENABLED) goto allout;

	if(startY > endY)
	{
		if((y < startY) && (y > endY)) goto allout;
	}
	else
	{
		if((y < startY) || (y >= endY)) goto allout;
	}

	//the x windows will apply for this scanline
	curr_win[WIN_NUM] = h_win[WIN_NUM];
	return;
	
allout:
	curr_win[WIN_NUM] = win_empty;
}

void GPU::update_winh(int WIN_NUM)
{
	//dont even waste any time in here if the window isnt enabled
	if(WIN_NUM==0 && !WIN0_ENABLED) return;
	if(WIN_NUM==1 && !WIN1_ENABLED) return;

	need_update_winh[WIN_NUM] = false;
	u16 startX,endX;

	if(WIN_NUM==0)
	{
		startX = WIN0H0;
		endX = WIN0H1;
	}
	else
	{
		startX = WIN1H0;
		endX = WIN1H1;
	}

	//the original logic: if you doubt the window code, please check it against the newer implementation below
	//if(startX > endX)
	//{
	//	if((x < startX) && (x > endX)) return false;
	//}
	//else
	//{
	//	if((x < startX) || (x >= endX)) return false;
	//}

	if(startX > endX)
	{
		for(int i=0;i<=endX;i++)
			h_win[WIN_NUM][i] = 1;
		for(int i=endX+1;i<startX;i++)
			h_win[WIN_NUM][i] = 0;
		for(int i=startX;i<256;i++)
			h_win[WIN_NUM][i] = 1;
	} else
	{
		for(int i=0;i<startX;i++)
			h_win[WIN_NUM][i] = 0;
		for(int i=startX;i<endX;i++)
			h_win[WIN_NUM][i] = 1;
		for(int i=endX;i<256;i++)
			h_win[WIN_NUM][i] = 0;
	}
}

void GPU_ligne(NDS_Screen * screen, u16 l)
{
	GPU * gpu = screen->gpu;

	//here is some setup which is only done on line 0
	if(l == 0) {
		for(int num=2;num<=3;num++)
		{
			BGxPARMS * parms;
			if (num==2)
				parms = &(gpu->dispx_st)->dispx_BG2PARMS;
			else
				parms = &(gpu->dispx_st)->dispx_BG3PARMS;		

			parms->BGxX = gpu->affineInfo[num-2].x;
			parms->BGxY = gpu->affineInfo[num-2].y;
		}
	}

	//cache some parameters which are assumed to be stable throughout the rendering of the entire line
	gpu->currLine = (u8)l;
	u16 mosaic_control = T1ReadWord((u8 *)&gpu->dispx_st->dispx_MISC.MOSAIC, 0);
	u16 mosaic_width = (mosaic_control & 0xF);
	u16 mosaic_height = ((mosaic_control>>4) & 0xF);

	//mosaic test hacks
	//mosaic_width = mosaic_height = 3;

	GPU::mosaicLookup.widthValue = mosaic_width;
	GPU::mosaicLookup.heightValue = mosaic_height;
	GPU::mosaicLookup.width = &GPU::mosaicLookup.table[mosaic_width][0];
	GPU::mosaicLookup.height = &GPU::mosaicLookup.table[mosaic_height][0];

	if(gpu->need_update_winh[0]) gpu->update_winh(0);
	if(gpu->need_update_winh[1]) gpu->update_winh(1);

	gpu->setup_windows<0>();
	gpu->setup_windows<1>();



	// initialize the scanline black
	// not doing this causes invalid colors when all active BGs are prevented to draw at some place
	// ZERO TODO - shouldnt this be BG palette color 0?
	//memset(dst,0,256*2) ;

	// This could almost be changed to use function pointers
	switch (gpu->dispMode)
	{
		case 0: // Display Off(Display white)
			{
				u8 * dst =  GPU_screen + (screen->offset + l) * 512;

				for (int i=0; i<256; i++)
					T2WriteWord(dst, i << 1, 0x7FFF);
			}
			break;

		case 1: // Display BG and OBJ layers
				GPU_ligne_layer(screen, l);
			break;

		case 2: // Display framebuffer
			{
				u8 * dst = GPU_screen + (screen->offset + l) * 512;
				u8 * src = gpu->VRAMaddr + (l*512);
				memcpy (dst, src, 512);
			}
			break;
		case 3: // Display memory FIFO
			{
				u8 * dst =  GPU_screen + (screen->offset + l) * 512;
				for (int i=0; i < 128; i++)
					T1WriteLong(dst, i << 2, DISP_FIFOrecv() & 0x7FFF7FFF);
			}
			break;
	}

	if (gpu->core == GPU_MAIN) 
	{
		GPU_ligne_DispCapture(l);
		if (l == 191) { disp_fifo.head = disp_fifo.tail = 0; }
	}
	GPU_ligne_MasterBrightness(screen, l);
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

void GPU::setAffineStart(int layer, int xy, u32 val)
{
	if(xy==0) affineInfo[layer-2].x = val;
	else affineInfo[layer-2].y = val;
}

//here is an old bg mosaic with some old code commented out. I am going to leave it here for a while to look at it
//sometimes in case I find a problem with the mosaic.
//static void __setFinalColorBck(GPU *gpu, u32 passing, u8 bgnum, u8 *dst, u16 color, u16 x, bool opaque)
//{
//	struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[bgnum].bits;
//	bool enabled = bgCnt->Mosaic_Enable;
//
////	if(!opaque) color = 0xFFFF;
////	else color &= 0x7FFF;
//	if(!opaque)
//		return;
//
//	//mosaic test hacks
//	enabled = true;
//
//	//due to this early out, we will get incorrect behavior in cases where 
//	//we enable mosaic in the middle of a frame. this is deemed unlikely.
//	if(enabled)
//	{
//		u8 y = gpu->currLine;
//
//		//I intend to cache all this at the beginning of line rendering
//		
//		u16 mosaic_control = T1ReadWord((u8 *)&gpu->dispx_st->dispx_MISC.MOSAIC, 0);
//		u8 mw = (mosaic_control & 0xF);
//		u8 mh = ((mosaic_control>>4) & 0xF); 
//
//		//mosaic test hacks
//		mw = 3;
//		mh = 3;
//
//		MosaicLookup::TableEntry &te_x = mosaicLookup.table[mw][x];
//		MosaicLookup::TableEntry &te_y = mosaicLookup.table[mh][y];
//
//		//int x_int;
//		//if(enabled) 
//			int x_int = te_x.trunc;
//		//else x_int = x;
//
//		if(te_x.begin && te_y.begin) {}
//		else color = gpu->MosaicColors.bg[bgnum][x_int];
//		gpu->MosaicColors.bg[bgnum][x] = color;
//	}
//
////	if(color != 0xFFFF)
//		gpu->setFinalColorBck(gpu,0,bgnum,dst,color,x);
//}
