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
#include "gfx3d.h"
#include "GPU_osd.h"
#include "debug.h"
#include "NDSSystem.h"
#include "readwrite.h"

//#undef FORCEINLINE
//#define FORCEINLINE

ARM9_struct ARM9Mem;

extern BOOL click;
NDS_Screen MainScreen;
NDS_Screen SubScreen;

//instantiate static instance
GPU::MosaicLookup GPU::mosaicLookup;

//#define DEBUG_TRI

CACHE_ALIGN u8 GPU_screen[4*256*192];
CACHE_ALIGN u8 GPU_tempScreen[4*256*192];
CACHE_ALIGN u8 *GPU_tempScanline;

CACHE_ALIGN u8 sprWin[256];

OSDCLASS	*osd = NULL;

u16			gpu_angle = 0;

const size sprSizeTab[4][4] = 
{
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};



static const BGType mode2type[8][4] = 
{
      {BGType_Text, BGType_Text, BGType_Text, BGType_Text},
      {BGType_Text, BGType_Text, BGType_Text, BGType_Affine},
      {BGType_Text, BGType_Text, BGType_Affine, BGType_Affine},
      {BGType_Text, BGType_Text, BGType_Text, BGType_AffineExt},
      {BGType_Text, BGType_Text, BGType_Affine, BGType_AffineExt},
      {BGType_Text, BGType_Text, BGType_AffineExt, BGType_AffineExt},
      {BGType_Invalid, BGType_Invalid, BGType_Large8bpp, BGType_Invalid},
      {BGType_Invalid, BGType_Invalid, BGType_Invalid, BGType_Invalid}
};

const short sizeTab[8][4][2] =
{
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //Invalid
    {{256,256}, {512,256}, {256,512}, {512,512}}, //text
    {{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine
    {{512,1024}, {1024,512}, {0,0}, {0,0}}, //large 8bpp
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //affine ext (to be elaborated with another value)
	{{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine ext 256x16
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext 256x1
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext direct
};

static GraphicsInterface_struct *GFXCore=NULL;

// This should eventually be moved to the port specific code
GraphicsInterface_struct *GFXCoreList[] = {
&GFXDummy,
NULL
};

static const CACHE_ALIGN u8 win_empty[256] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
static CACHE_ALIGN u16 fadeInColors[17][0x8000];
static CACHE_ALIGN u16 fadeOutColors[17][0x8000];

//this should be public, because it gets used somewhere else
CACHE_ALIGN u8 gpuBlendTable555[17][17][32][32];


/*****************************************************************************/
//			PIXEL RENDERING - 3D
/*****************************************************************************/

#define DECL3D \
	int x = dstX; \
	int passing = dstX<<1; \
	u16 color = _3dColorLine[srcX]; \
	u8 alpha = _3dAlphaLine[srcX]; \
	u8* dst = currDst;

FORCEINLINE void GPU::setFinal3DColorSpecialNone(int dstX, int srcX)
{
	DECL3D;

	// We must blend if the 3D layer has the highest prio
	if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && bg0HasHighestPrio)
	{
		int bg_under = bgPixels[dstX];
		u16 final = color;

		// If the layer we are drawing on is selected as 2nd source, we can blend
		if(BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha) + (c2.bits.red * (16 - alpha)))/16;
				cfinal.bits.green = ((c1.bits.green * alpha) + (c2.bits.green * (16 - alpha)))/16;
				cfinal.bits.blue = ((c1.bits.blue * alpha) + (c2.bits.blue * (16 - alpha)))/16;

				final = cfinal.val;
			}
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		bgPixels[x] = 0;
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialBlend(int dstX, int srcX)
{
	DECL3D;

	// We can blend if the 3D layer is selected as 1st target,
	//but also if the 3D layer has the highest prio.
	if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && ((BLDCNT & 0x1) || bg0HasHighestPrio))
	{
		int bg_under = bgPixels[x];
		u16 final = color;

		//If the layer we are drawing on is selected as 2nd source, we can blend
		if(BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) )) / 16;
				cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) )) / 16;
				cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) )) / 16;

				final = cfinal.val;
			}
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (color | 0x8000));
		bgPixels[x] = 0;
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialIncrease(int dstX, int srcX)
{
	DECL3D;
	u16 final = color;

	// We must blend if the 3D layer has the highest prio
	// But it doesn't seem to have priority over fading,
	// unlike semi-transparent sprites
	if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && bg0HasHighestPrio)
	{
		int bg_under = bgPixels[x];

		/* If the layer we are drawing on is selected as 2nd source, we can blend */
		if(BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) ))/16;
				cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) ))/16;
				cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) ))/16;

				final = cfinal.val;
			}
		}
	}
	
	if(BLDCNT & 0x1)
	{
		if (BLDY_EVY != 0x0)
		{
			final = fadeInColors[BLDY_EVY][final&0x7FFF];
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (final | 0x8000));
		bgPixels[x] = 0;
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialDecrease(int dstX, int srcX)
{
	DECL3D;

	u16 final = color;

	// We must blend if the 3D layer has the highest prio
	// But it doesn't seem to have priority over fading
	// unlike semi-transparent sprites
	if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && bg0HasHighestPrio)
	{
		int bg_under = bgPixels[x];

		// If the layer we are drawing on is selected as 2nd source, we can blend
		if(BLDCNT & (0x100 << bg_under))
		{
			{
				COLOR c1, c2, cfinal;

				c1.val = color;
				c2.val = T2ReadWord(dst, passing);

				cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) ))/16;
				cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) ))/16;
				cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) ))/16;

				final = cfinal.val;
			}
		}
	}
	
	if(BLDCNT & 0x1)
	{
		if (BLDY_EVY != 0x0)
		{
			final = fadeOutColors[BLDY_EVY][final&0x7FFF];
		}

		T2WriteWord(dst, passing, (final | 0x8000));
		bgPixels[x] = 0;
	}
	else
	{
		T2WriteWord(dst, passing, (final | 0x8000));
		bgPixels[x] = 0;
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialNoneWnd(int dstX, int srcX)
{
	DECL3D;

	bool windowDraw = true, windowEffect = true;
	
	renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		// We must blend if the 3D layer has the highest prio 
		if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && bg0HasHighestPrio)
		{
			int bg_under = bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if(BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) ))/16;
					cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) ))/16;
					cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) ))/16;

					final = cfinal.val;
				}
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			bgPixels[x] = 0;
		}
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialBlendWnd(int dstX, int srcX)
{
	DECL3D;

	bool windowDraw = true, windowEffect = true;
	
	renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		// We can blend if the 3D layer is selected as 1st target,
		// but also if the 3D layer has the highest prio.
		if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && (((BLDCNT & 0x1) && windowEffect) || bg0HasHighestPrio))
		{
			int bg_under = bgPixels[x];
			u16 final = color;

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if(BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) ))/16;
					cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) ))/16;
					cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) ))/16;

					final = cfinal.val;
				}
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (color | 0x8000));
			bgPixels[x] = 0;
		}
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialIncreaseWnd(int dstX, int srcX)
{
	DECL3D;

	bool windowDraw = true, windowEffect = true;
	u16 final = color;
	
	renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		// We must blend if the 3D layer has the highest prio 
		// But it doesn't seem to have priority over fading, 
		// unlike semi-transparent sprites 
		if((alpha < 16)) //zero 30-may-09 - i think 3d always blends && bg0HasHighestPrio)
		{
			int bg_under = bgPixels[x];

			// If the layer we are drawing on is selected as 2nd source, we can blend 
			if(BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) ))/16;
					cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) ))/16;
					cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) ))/16;

					final = cfinal.val;
				}
			}
		}
		
		if((BLDCNT & 0x1) && windowEffect)
		{
			if (BLDY_EVY != 0x0)
			{
				final = fadeInColors[BLDY_EVY][final&0x7FFF];
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (final | 0x8000));
			bgPixels[x] = 0;
		}
	}
}

FORCEINLINE void GPU::setFinal3DColorSpecialDecreaseWnd(int dstX, int srcX)
{
	DECL3D;

	bool windowDraw = true, windowEffect = true;
	u16 final = color;
	
	renderline_checkWindows(x,  windowDraw, windowEffect);

	if(windowDraw)
	{
		// We must blend if the 3D layer has the highest prio 
		// But it doesn't seem to have priority over fading, 
		// unlike semi-transparent sprites 
		if((alpha < 16)) ////zero 30-may-09 - i think 3d always blends && bg0HasHighestPrio)
		{
			int bg_under = bgPixels[x];

			// If the layer we are drawing on is selected as 2nd source, we can blend 
			if(BLDCNT & (0x100 << bg_under))
			{
				{
					COLOR c1, c2, cfinal;

					c1.val = color;
					c2.val = T2ReadWord(dst, passing);

					cfinal.bits.red = ((c1.bits.red * alpha ) + (c2.bits.red * (16 - alpha) ))/16;
					cfinal.bits.green = ((c1.bits.green * alpha ) + (c2.bits.green * (16 - alpha) ))/16;
					cfinal.bits.blue = ((c1.bits.blue * alpha ) + (c2.bits.blue * (16 - alpha) ))/16;

					final = cfinal.val;
				}
			}
		}
		
		if((BLDCNT & 0x1) && windowEffect)
		{
			if (BLDY_EVY != 0x0)
			{
				final = fadeOutColors[BLDY_EVY][final&0x7FFF];
			}

			T2WriteWord(dst, passing, (final | 0x8000));
			bgPixels[x] = 0;
		}
		else
		{
			T2WriteWord(dst, passing, (final | 0x8000));
			bgPixels[x] = 0;
		}
	}
}


static void setFinalOBJColorSpecialNone			(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialBlend		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialIncrease		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialDecrease		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialNoneWnd		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialBlendWnd		(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialIncreaseWnd	(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);
static void setFinalOBJColorSpecialDecreaseWnd	(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x);

const GPU::FinalOBJColFunct pixelBlittersOBJ[8] = {
									setFinalOBJColorSpecialNone,
									setFinalOBJColorSpecialBlend,
									setFinalOBJColorSpecialIncrease,
									setFinalOBJColorSpecialDecrease,
									setFinalOBJColorSpecialNoneWnd,
									setFinalOBJColorSpecialBlendWnd,
									setFinalOBJColorSpecialIncreaseWnd,
									setFinalOBJColorSpecialDecreaseWnd,};


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
	g->setFinalColorBck_funcNum = 0;
	g->setFinalColor3d_funcNum = 0;
	g->setFinalColorSpr = setFinalOBJColorSpecialNone;

	

	return g;
}

void GPU_Reset(GPU *g, u8 l)
{
	memset(g, 0, sizeof(GPU));

	g->setFinalColorBck_funcNum = 0;
	g->setFinalColor3d_funcNum = 0;
	g->setFinalColorSpr = setFinalOBJColorSpecialNone;
	g->core = l;
	g->BGSize[0][0] = g->BGSize[1][0] = g->BGSize[2][0] = g->BGSize[3][0] = 256;
	g->BGSize[0][1] = g->BGSize[1][1] = g->BGSize[2][1] = g->BGSize[3][1] = 256;
	g->dispOBJ = g->dispBG[0] = g->dispBG[1] = g->dispBG[2] = g->dispBG[3] = TRUE;

	g->spriteRenderMode = GPU::SPRITE_1D;

	g->bgPrio[4] = 0xFF;

	g->bg0HasHighestPrio = TRUE;

	if(g->core == GPU_SUB)
	{
		g->oam = (OAM *)(ARM9Mem.ARM9_OAM + ADDRESS_STEP_1KB);
		g->sprMem = ARM9MEM_BOBJ;
		// GPU core B
		g->dispx_st = (REG_DISPx*)(&ARM9Mem.ARM9_REG[REG_DISPB]);
	}
	else
	{
		g->oam = (OAM *)(ARM9Mem.ARM9_OAM);
		g->sprMem = ARM9MEM_AOBJ;
		// GPU core A
		g->dispx_st = (REG_DISPx*)(&ARM9Mem.ARM9_REG[0]);
	}
}

void GPU_DeInit(GPU * gpu)
{
	free(gpu);
}

static void GPU_resortBGs(GPU *gpu)
{
	int i, prio;
	struct _DISPCNT * cnt = &gpu->dispx_st->dispx_DISPCNT.bits;
	itemsForPriority_t * item;

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

static FORCEINLINE u16 _blend(u16 colA, u16 colB, GPU::TBlendTable* blendTable)
{
	u8 r = (*blendTable)[colA&0x1F][colB&0x1F];
	u8 g = (*blendTable)[(colA>>5)&0x1F][(colB>>5)&0x1F];
	u8 b = (*blendTable)[(colA>>10)&0x1F][(colB>>10)&0x1F];

	return r|(g<<5)|(b<<10);
}

FORCEINLINE u16 GPU::blend(u16 colA, u16 colB)
{
	return _blend(colA, colB, blendTable);
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
	gpu->setFinalColorBck_funcNum = windowUsed*4 + blendMode;
	gpu->setFinalColor3d_funcNum = windowUsed*4 + blendMode;
	
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
			break;
		case 1: // Display BG and OBJ layers
			break;
		case 2: // Display framebuffer
			gpu->VRAMaddr = (u8 *)ARM9Mem.ARM9_LCD + (gpu->vramBlock * 0x20000);
			break;
		case 3: // Display from Main RAM
			// nothing to be done here
			// see GPU_ligne who gets data from FIFO.
			break;
	}

	if(cnt->OBJ_Tile_mapping)
	{
		// 1-d sprite mapping 
		// boundary :
		// core A : 32k, 64k, 128k, 256k
		// core B : 32k, 64k, 128k, 128k
		gpu->sprBoundary = 5 + cnt->OBJ_Tile_1D_Bound ;
		
		//zero 10-apr-09 - not sure whether this is right...
		if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3)) gpu->sprBoundary = 7;

		gpu->spriteRenderMode = GPU::SPRITE_1D;
	} else {
		// 2d sprite mapping
		// boundary : 32k
		gpu->sprBoundary = 5;
		gpu->spriteRenderMode = GPU::SPRITE_2D;
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

//this handles writing in BGxCNT
void GPU_setBGProp(GPU * gpu, u16 num, u16 p)
{
	struct _BGxCNT * cnt = &((gpu->dispx_st)->dispx_BGxCNT[num].bits);
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	
	T1WriteWord((u8 *)&(gpu->dispx_st)->dispx_BGxCNT[num].val, 0, p);
	
	GPU_resortBGs(gpu);

	if(gpu->core == GPU_SUB)
	{
		gpu->BG_tile_ram[num] = ARM9MEM_BBG;
		gpu->BG_bmp_ram[num]  = ARM9MEM_BBG;
		gpu->BG_bmp_large_ram[num]  = ARM9MEM_BBG;
		gpu->BG_map_ram[num]  = ARM9MEM_BBG;
	} 
	else 
	{
		gpu->BG_tile_ram[num] = ARM9MEM_ABG +  dispCnt->CharacBase_Block * ADDRESS_STEP_64KB ;
		gpu->BG_bmp_ram[num]  = ARM9MEM_ABG;
		gpu->BG_bmp_large_ram[num] = ARM9MEM_ABG;
		gpu->BG_map_ram[num]  = ARM9MEM_ABG +  dispCnt->ScreenBase_Block * ADDRESS_STEP_64KB;
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

	BGType mode = mode2type[dispCnt->BG_Mode][num];

	//clarify affine ext modes 
	if(mode == BGType_AffineExt)
	{
		//see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol
		u8 affineModeSelection = (cnt->Palette_256 << 1) | (cnt->CharacBase_Block & 1) ;
		switch(affineModeSelection)
		{
		case 0:
		case 1:
			mode = BGType_AffineExt_256x16;
			break;
		case 2:
			mode = BGType_AffineExt_256x1;
			break;
		case 3:
			mode = BGType_AffineExt_Direct;
			break;
		}
	}

	gpu->BGTypes[num] = mode;

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
			draw = (WININ0 >> currBgNum)&1;
			if(currBgNum==5) draw = true; //backdrop must always be drawn. windows only control color effects.
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
			if(currBgNum==5) draw = true; //backdrop must always be drawn. windows only control color effects.
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
			if(currBgNum==5) draw = true; //backdrop must always be drawn. windows only control color effects.
			effect	= (WINOBJ_SPECIAL);
			return;
		}
	}

	if (WINOBJ_ENABLED | WIN1_ENABLED | WIN0_ENABLED)
	{
		draw	= (WINOUT >> currBgNum) & 1;
		if(currBgNum==5) draw = true; //backdrop must always be drawn. windows only control color effects.
		effect	= (WINOUT_SPECIAL);
	}
}

/*****************************************************************************/
//			PIXEL RENDERING - BGS
/*****************************************************************************/

FORCEINLINE void GPU::setFinalBGColorSpecialNone(u16 &color, const u8 x)
{
}

FORCEINLINE void GPU::setFinalBGColorSpecialBlend(u16 &color, const u8 x)
{
	if(blend1)
	{
		//If the layer we are drawing on is selected as 2nd source, we can blend
		int bg_under = bgPixels[x];
		if(blend2[bg_under])
			color = blend(color,T2ReadWord(currDst, x<<1));
	}
}

FORCEINLINE void GPU::setFinalBGColorSpecialIncrease (u16 &color, const u8 x)
{
	if(blend1)   // the bg to draw has a special color effect
	{
		color = currentFadeInColors[color];
	}
}

FORCEINLINE void GPU::setFinalBGColorSpecialDecrease(u16 &color, const u8 x)
{
	if(blend1)   // the bg to draw has a special color effect
	{
		color = currentFadeOutColors[color];
	}
}

FORCEINLINE bool GPU::setFinalBGColorSpecialNoneWnd(u16 &color, const u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	renderline_checkWindows(x, windowDraw, windowEffect);

	if (blend1 && windowEffect)   // the bg to draw has a special color effect
	{
		return true;
	}
	else
	{
		if ((windowEffect && (BLDCNT & (0x100 << currBgNum))) || windowDraw)
		{
			return true;
		}
	}
	return false;
}

FORCEINLINE bool GPU::setFinalBGColorSpecialBlendWnd(u16 &color, const u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if(blend1 && windowEffect)
		{
			int bg_under = bgPixels[x];

			// If the layer we are drawing on is selected as 2nd source, we can blend
			if(blend2[bg_under])
				color = blend(color,T2ReadWord(currDst, x<<1));
		}
		return true;
	}
	return false;
}

FORCEINLINE bool GPU::setFinalBGColorSpecialIncreaseWnd(u16 &color, const u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if(blend1 && windowEffect)
		{
			color = currentFadeInColors[color];
		}
		return true;
	}
	return false;
}

FORCEINLINE bool GPU::setFinalBGColorSpecialDecreaseWnd(u16 &color, const u8 x)
{
	bool windowDraw = true, windowEffect = true;
	
	renderline_checkWindows(x, windowDraw, windowEffect);

	if(windowDraw)
	{
		if(blend1 && windowEffect)
		{
			color = currentFadeOutColors[color];
		}
		return true;
	}
	return false;
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

static FORCEINLINE u16 blendSprite(GPU* gpu, u8 alpha, u16 sprColor, u16 backColor, u8 type)
{
	if(type == 3) 
		//handle translucent bitmap sprite
		return _blend(sprColor,backColor,&gpuBlendTable555[alpha+1][15-alpha]);
	else 
		return gpu->blend(sprColor,backColor);
}

static void setFinalOBJColorSpecialBlend(GPU *gpu, u32 passing, u8 *dst, u16 color, u8 alpha, u8 type, u16 x)
{
	if((gpu->BLDCNT & 0x10) || (type == 1))
	{
		int bg_under = gpu->bgPixels[x];
		u16 final = color;

		//If the layer we are drawing on is selected as 2nd source, we can blend
		if((bg_under != 4) && (gpu->BLDCNT & (0x100 << bg_under)))
		{
			//bmp translucent handling tested by disgaea clock hud
			final = blendSprite(gpu, alpha, color, T2ReadWord(dst, passing), type);
		}
		
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
			{
				final = gpu->blend(color,T2ReadWord(dst, passing));
			}

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

FORCEINLINE void GPU::setFinalColorBG(u16 color, u8 x)
{
	//It is not safe to assert this here.
	//This is probably the best place to enforce it, since almost every single color that comes in here
	//will be pulled from a palette that needs the top bit stripped off anyway.
	//assert((color&0x8000)==0);
	color &= 0x7FFF;

	//if someone disagrees with these, they could be reimplemented as a function pointer easily
	bool draw=true;
	switch(setFinalColorBck_funcNum)
	{
	case 0x0: setFinalBGColorSpecialNone(color,x); break;
	case 0x1: setFinalBGColorSpecialBlend(color,x); break;
	case 0x2: setFinalBGColorSpecialIncrease(color,x); break;
	case 0x3: setFinalBGColorSpecialDecrease(color,x); break;
	case 0x4: draw=setFinalBGColorSpecialNoneWnd(color,x); break;
	case 0x5: draw=setFinalBGColorSpecialBlendWnd(color,x); break;
	case 0x6: draw=setFinalBGColorSpecialIncreaseWnd(color,x); break;
	case 0x7: draw=setFinalBGColorSpecialDecreaseWnd(color,x); break;
	};

	if(draw) 
	{
		T2WriteWord(currDst, x<<1, color | 0x8000);
		bgPixels[x] = currBgNum;
	}
}


FORCEINLINE void GPU::setFinalColor3d(int dstX, int srcX)
{
	//if someone disagrees with these, they could be reimplemented as a function pointer easily
	switch(setFinalColor3d_funcNum)
	{
	case 0x0: setFinal3DColorSpecialNone(dstX,srcX); break;
	case 0x1: setFinal3DColorSpecialBlend(dstX,srcX); break;
	case 0x2: setFinal3DColorSpecialIncrease(dstX,srcX); break;
	case 0x3: setFinal3DColorSpecialDecrease(dstX,srcX); break;
	case 0x4: setFinal3DColorSpecialNoneWnd(dstX,srcX); break;
	case 0x5: setFinal3DColorSpecialBlendWnd(dstX,srcX); break;
	case 0x6: setFinal3DColorSpecialIncreaseWnd(dstX,srcX); break;
	case 0x7: setFinal3DColorSpecialDecreaseWnd(dstX,srcX); break;
	};
}

//this was forced inline because most of the time it just falls through to setFinalColorBck() and the function call
//overhead was ridiculous and terrible
template<bool MOSAIC> FORCEINLINE void GPU::__setFinalColorBck(u16 color, const u8 x, const bool opaque)
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
	if(!MOSAIC) {
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
		setFinalColorBG(color,x);
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

template<bool MOSAIC> void lineLarge8bpp(GPU * gpu)
{
	if(gpu->core == 1) {
		PROGINFO("Cannot use large 8bpp screen on sub core\n");
		return;
	}

	BGxOFS * ofs = &gpu->dispx_st->dispx_BGxOFS[gpu->currBgNum];
	u8 num = gpu->currBgNum;
	u16 XBG = T1ReadWord((u8 *)&ofs->BGxHOFS, 0);
	u16 YBG = gpu->currLine + T1ReadWord((u8 *)&ofs->BGxVOFS, 0);
	u16 lg     = gpu->BGSize[num][0];
	u16 ht     = gpu->BGSize[num][1];
	u16 wmask  = (lg-1);
	u16 hmask  = (ht-1);
	YBG &= hmask;

	//TODO - handle wrapping / out of bounds correctly from rot_scale_op?

	u32 tmp_map = gpu->BG_bmp_large_ram[num] + lg * YBG;
	u8* map = MMU_RenderMapToLCD(tmp_map);

	u8* pal = ARM9Mem.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB;

	for(int x = 0; x < lg; ++x, ++XBG)
	{
		XBG &= wmask;
		u8 pixel = map[XBG];
		u16 color = T1ReadWord(pal, pixel<<1);
		gpu->__setFinalColorBck<MOSAIC>(color,x,color!=0);
	}
	
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
template<bool MOSAIC> INLINE void renderline_textBG(GPU * gpu, u16 XBG, u16 YBG, u16 LG)
{
	u8 num = gpu->currBgNum;
	struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	u16 lg     = gpu->BGSize[num][0];
	u16 ht     = gpu->BGSize[num][1];
	u16 wmask  = (lg-1);
	u16 hmask  = (ht-1);
	u16 tmp    = ((YBG & hmask) >> 3);
	u32 map;
	u8 *pal, *line;
	u32 tile;
	u16 color;
	u16 xoff;
	u16 yoff;
	u32 x      = 0;
	u32 xfin;

	s8 line_dir = 1;
	u32 mapinfo;
	TILEENTRY tileentry;

	u32 tmp_map = gpu->BG_map_ram[num] + (tmp&31) * 64;
	if(tmp>31) 
		tmp_map+= ADDRESS_STEP_512B << bgCnt->ScreenSize ;

	//map = (u8*)MMU_RenderMapToLCD(tmp_map);
	map = tmp_map;
	//if(!map) return; 	// no map
	
	tile = gpu->BG_tile_ram[num];
	//if(!tile) return; 	// no tiles

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
			tileentry.val = T1ReadWord(MMU_gpu_map(mapinfo), 0);

			tilePalette = (tileentry.bits.Palette*16);

			line = (u8*)MMU_gpu_map(tile + (tileentry.bits.TileNum * 0x20) + ((tileentry.bits.VFlip) ? (7*4)-yoff : yoff));
			
			if(tileentry.bits.HFlip)
			{
				line += (3 - ((xoff&7)>>1));
				for(; x < xfin; line --) 
				{	
					u8 currLine = *line;

					if(!(xoff&1))
					{
						color = T1ReadWord(pal, ((currLine>>4) + tilePalette) << 1);
						gpu->__setFinalColorBck<MOSAIC>(color,x,currLine>>4);
						x++; xoff++;
					}
					
					color = T1ReadWord(pal, ((currLine&0xF) + tilePalette) << 1);
					gpu->__setFinalColorBck<MOSAIC>(color,x,currLine&0xF);
					x++; xoff++;
				}
			} else {
				line += ((xoff&7)>>1);
				for(; x < xfin; line ++) 
				{
					u8 currLine = *line;

					if(!(xoff&1))
					{
						color = T1ReadWord(pal, ((currLine&0xF) + tilePalette) << 1);
						gpu->__setFinalColorBck<MOSAIC>(color,x,currLine&0xF);

						extern int currFrameCounter;
						if(currFrameCounter == 0x20 && nds.VCount == 0x48) {
							int zzz=9;
						}

						x++; xoff++;
					}

					color = T1ReadWord(pal, ((currLine>>4) + tilePalette) << 1);
					gpu->__setFinalColorBck<MOSAIC>(color,x,currLine>>4);
					x++; xoff++;
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
		tileentry.val = T1ReadWord(MMU_gpu_map(mapinfo), 0);

		line = (u8*)MMU_gpu_map(tile + (tileentry.bits.TileNum*0x40) + ((tileentry.bits.VFlip) ? (7*8)-yoff : yoff));

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

			gpu->__setFinalColorBck<MOSAIC>(color,x,*line);
			
			x++; xoff++;

			line += line_dir;
		}
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -ROTOSCALE-
/*****************************************************************************/

template<bool MOSAIC> FORCEINLINE void rot_tiled_8bit_entry(GPU * gpu, s32 auxX, s32 auxY, int lg, u32 map, u32 tile, u8 * pal, int i, u8 extPal) {
	u8 palette_entry;
	u16 tileindex, x, y, color;

	tileindex = *(u8*)MMU_gpu_map(map + ((auxX>>3) + (auxY>>3) * (lg>>3)));

	x = (auxX&7); 
	y = (auxY&7);

	palette_entry = *(u8*)MMU_gpu_map(tile + ((tileindex<<6)+(y<<3)+x));
	color = T1ReadWord(pal, palette_entry << 1);
	gpu->__setFinalColorBck<MOSAIC>(color,i,palette_entry);
}

template<bool MOSAIC> FORCEINLINE void rot_tiled_16bit_entry(GPU * gpu, s32 auxX, s32 auxY, int lg, u32 map, u32 tile, u8 * pal, int i, u8 extPal) {
	u8 palette_entry;
	u16 x, y, color;
	TILEENTRY tileentry;

	void* map_addr = MMU_gpu_map(map + (((auxX>>3) + (auxY>>3) * (lg>>3))<<1));
	
	tileentry.val = T1ReadWord(map_addr, 0);

	x = (tileentry.bits.HFlip) ? 7 - (auxX&7) : (auxX&7);
	y = (tileentry.bits.VFlip) ? 7 - (auxY&7) : (auxY&7);

	palette_entry = *(u8*)MMU_gpu_map(tile + ((tileentry.bits.TileNum<<6)+(y<<3)+x));
	color = T1ReadWord(pal, (palette_entry + (extPal ? (tileentry.bits.Palette<<8) : 0)) << 1);
	gpu->__setFinalColorBck<MOSAIC>(color, i, palette_entry);
}

template<bool MOSAIC> FORCEINLINE void rot_256_map(GPU * gpu, s32 auxX, s32 auxY, int lg, u32 map, u32 tile, u8 * pal, int i, u8 extPal) {
	u8 palette_entry;
	u16 color;

	u8* adr = (u8*)MMU_gpu_map((map) + ((auxX + auxY * lg)));

	palette_entry = *adr;
	color = T1ReadWord(pal, palette_entry << 1);
	gpu->__setFinalColorBck<MOSAIC>(color, i, palette_entry);
}

template<bool MOSAIC> FORCEINLINE void rot_BMP_map(GPU * gpu, s32 auxX, s32 auxY, int lg, u32 map, u32 tile, u8 * pal, int i, u8 extPal) {
	u16 color;
	void* adr = MMU_gpu_map((map) + ((auxX + auxY * lg) << 1));
	color = T1ReadWord(adr, 0);
	gpu->__setFinalColorBck<MOSAIC>(color, i, color&0x8000);
}

typedef void (*rot_fun)(GPU * gpu, s32 auxX, s32 auxY, int lg, u32 map, u32 tile, u8 * pal , int i, u8 extPal);

template<rot_fun fun>
FORCEINLINE void rot_scale_op(GPU * gpu, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG, s32 wh, s32 ht, BOOL wrap, u32 map, u32 tile, u8 * pal, u8 extPal)
{
	ROTOCOORD x, y;
	x.val = X;
	y.val = Y;

	const s32 dx = (s32)PA;
	const s32 dy = (s32)PC;

	for(int i = 0; i < LG; ++i)
	{
		s32 auxX, auxY;
		auxX = x.bits.Integer;
		auxY = y.bits.Integer;
	
		bool checkBounds = true;
		if(wrap)
		{
			auxX = auxX & (wh-1);
			auxY = auxY & (ht-1);

			//since we just wrapped, we dont need to check bounds
			checkBounds = false;
		}
		
		if(!checkBounds || ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht)))
			fun(gpu, auxX, auxY, wh, map, tile, pal, i, extPal);

		x.val += dx;
		y.val += dy;
	}
}

template<rot_fun fun>
FORCEINLINE void apply_rot_fun(GPU * gpu, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG, u32 map, u32 tile, u8 * pal, u8 extPal)
{
	struct _BGxCNT * bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[gpu->currBgNum].bits;
	s32 wh = gpu->BGSize[gpu->currBgNum][0];
	s32 ht = gpu->BGSize[gpu->currBgNum][1];
	rot_scale_op<fun>(gpu, X, Y, PA, PB, PC, PD, LG, wh, ht, bgCnt->PaletteSet_Wrap, map, tile, pal, extPal);	
}


template<bool MOSAIC> FORCEINLINE void rotBG2(GPU * gpu, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, u16 LG)
{
	u8 num = gpu->currBgNum;
	u8 * pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
//	printf("rot mode\n");
	apply_rot_fun<rot_tiled_8bit_entry<MOSAIC> >(gpu,X,Y,PA,PB,PC,PD,LG, gpu->BG_map_ram[num], gpu->BG_tile_ram[num], pal, 0);
}

template<bool MOSAIC> FORCEINLINE void extRotBG2(GPU * gpu, s32 X, s32 Y, s16 PA, s16 PB, s16 PC, s16 PD, s16 LG)
{
	u8 num = gpu->currBgNum;
	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	
	u8 *map, *tile, *pal;

	switch(gpu->BGTypes[num])
	{
	case BGType_AffineExt_256x16:
		if(dispCnt->ExBGxPalette_Enable)
			pal = ARM9Mem.ExtPal[gpu->core][gpu->BGExtPalSlot[num]];
		else
			pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		if (!pal) return;
		// 16  bit bgmap entries
		apply_rot_fun<rot_tiled_16bit_entry<MOSAIC> >(gpu,X,Y,PA,PB,PC,PD,LG, gpu->BG_map_ram[num], gpu->BG_tile_ram[num], pal, dispCnt->ExBGxPalette_Enable);
		return;
	case BGType_AffineExt_256x1:
		// 256 colors 
		pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		apply_rot_fun<rot_256_map<MOSAIC> >(gpu,X,Y,PA,PB,PC,PD,LG, gpu->BG_bmp_ram[num], NULL, pal, 0);
		return;
	case BGType_AffineExt_Direct:
		// direct colors / BMP
		apply_rot_fun<rot_BMP_map<MOSAIC> >(gpu,X,Y,PA,PB,PC,PD,LG, gpu->BG_bmp_ram[num], NULL, NULL, 0);
		return;
	case BGType_Large8bpp:
		// large screen 256 colors
		pal = ARM9Mem.ARM9_VMEM + gpu->core * 0x400;
		apply_rot_fun<rot_256_map<MOSAIC> >(gpu,X,Y,PA,PB,PC,PD,LG, gpu->BG_bmp_large_ram[num], NULL, pal, 0);
		return;
	default: break;
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

#if 0
static void lineNull(GPU * gpu)
{
}
#endif

template<bool MOSAIC> void lineText(GPU * gpu)
{
	BGxOFS * ofs = &gpu->dispx_st->dispx_BGxOFS[gpu->currBgNum];
	renderline_textBG<MOSAIC>(gpu, T1ReadWord((u8 *)&ofs->BGxHOFS, 0), gpu->currLine + T1ReadWord((u8 *)&ofs->BGxVOFS, 0), 256);
}

template<bool MOSAIC> void lineRot(GPU * gpu)
{	
	BGxPARMS * parms;
	if (gpu->currBgNum==2) {
		parms = &(gpu->dispx_st)->dispx_BG2PARMS;
	} else {
		parms = &(gpu->dispx_st)->dispx_BG3PARMS;		
	}
     rotBG2<MOSAIC>(gpu, 
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

template<bool MOSAIC> void lineExtRot(GPU * gpu)
{
	BGxPARMS * parms;
	if (gpu->currBgNum==2) {
		parms = &(gpu->dispx_st)->dispx_BG2PARMS;
	} else {
		parms = &(gpu->dispx_st)->dispx_BG3PARMS;		
	}
     extRotBG2<MOSAIC>(gpu, 
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

namespace GPU_EXT {
	void textBG(GPU * gpu, u8 num, u8 * DST)
	{
		gpu->currBgNum = num;
		for(u32 i = 0; i < gpu->BGSize[num][1]; ++i)
		{
			gpu->currDst = DST + i*gpu->BGSize[num][0]*2;
			gpu->currLine = i;
			renderline_textBG<false>(gpu, 0, i, gpu->BGSize[num][0]);
		}
	}

	void rotBG(GPU * gpu, u8 num, u8 * DST)
	{
		gpu->currBgNum = num;
		 for(u32 i = 0; i < gpu->BGSize[num][1]; ++i)
		 {
				gpu->currDst = DST + i*gpu->BGSize[num][0]*2;
				gpu->currLine = i;
			  rotBG2<false>(gpu, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
		 }
	}

	void extRotBG(GPU * gpu, u8 num, u8 * DST)
	{
		 for(u32 i = 0; i < gpu->BGSize[num][1]; ++i)
  		 {
				gpu->currDst = DST + i*gpu->BGSize[num][0]*2;
				gpu->currLine = i;
				extRotBG2<false>(gpu, 0, 0, 256, 0, 0, 256, gpu->BGSize[num][0]);
		 }
	}
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

void GPU::spriteRender(u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
{
	if(spriteRenderMode == SPRITE_1D)
		_spriteRender<SPRITE_1D>(dst,dst_alpha,typeTab, prioTab);
	else
		_spriteRender<SPRITE_2D>(dst,dst_alpha,typeTab, prioTab);
}

//TODO - refactor this so there isnt as much duped code between rotozoomed and non-rotozoomed versions

template<GPU::SpriteRenderMode MODE>
void GPU::_spriteRender(u8 * dst, u8 * dst_alpha, u8 * typeTab, u8 * prioTab)
{
	u16 l = currLine;
	GPU *gpu = this;

	struct _DISPCNT * dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	_OAM_ * spriteInfo = (_OAM_ *)(gpu->oam + (nbShow-1));// + 127;
	u8 block = gpu->sprBoundary;
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
	)     
	{
		//for each sprite:

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
#ifdef WORDS_BIGENDIAN
			dx  = ((s16)(gpu->oam + blockparameter+0)->attr31 << 8) | ((s16)(gpu->oam + blockparameter+0)->attr30);
			dmx = ((s16)(gpu->oam + blockparameter+1)->attr31 << 8) | ((s16)(gpu->oam + blockparameter+1)->attr30);
			dy  = ((s16)(gpu->oam + blockparameter+2)->attr31 << 8) | ((s16)(gpu->oam + blockparameter+2)->attr30);
			dmy = ((s16)(gpu->oam + blockparameter+3)->attr31 << 8) | ((s16)(gpu->oam + blockparameter+3)->attr30);
#else
			dx  = (s16)(gpu->oam + blockparameter+0)->attr3;
			dmx = (s16)(gpu->oam + blockparameter+1)->attr3;
			dy  = (s16)(gpu->oam + blockparameter+2)->attr3;
			dmy = (s16)(gpu->oam + blockparameter+3)->attr3;
#endif

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
				//2d: src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex) << 5));
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex << block));
				if (!src) { 
					continue;
				}

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
						if(MODE == SPRITE_2D)
							offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*8);
						else
							offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)*sprSize.x*8) + ((auxY&0x7)*8);

						colour = src[offset];

						if (colour && (prioTab[sprX]>=prio))
						{ 
							T2WriteWord(dst, (sprX<<1), T2ReadWord(pal, (colour<<1)));
							dst_alpha[sprX] = 16;
							typeTab[sprX] = spriteInfo->Mode;
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
				if (dispCnt->OBJ_BMP_mapping)
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<sprBMPBoundary));
				else
					//NOT TESTED
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x03E0) * 8) + (spriteInfo->TileIndex&0x001F))*16);
				
				if (!src) { 
					continue;
				}

				for(j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						//if(MODE == SPRITE_2D) //tested by buffy sacrifice damage blood splatters in corner
						//else //tested by lego indiana jones
						offset = auxX + (auxY*sprSize.x);

						colour = T1ReadWord (src, offset<<1);

						if((colour&0x8000) && (prioTab[sprX]>=prio))
						{
							T2WriteWord(dst, (sprX<<1), colour);
							dst_alpha[sprX] = spriteInfo->PaletteIndex;
							typeTab[sprX] = spriteInfo->Mode;
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
				if(MODE == SPRITE_2D)
				{
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<5));
					pal = ARM9Mem.ARM9_VMEM + 0x200 + (gpu->core*0x400 + (spriteInfo->PaletteIndex*32));
				}
				else
				{
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<gpu->sprBoundary));
					pal = ARM9Mem.ARM9_VMEM + 0x200 + gpu->core*0x400 + (spriteInfo->PaletteIndex*32);
				}

				if (!src) { 
					continue;
				}

				for(j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX>>8);
					auxY = (realY>>8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						if(MODE == SPRITE_2D)
							offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*4);
						else
							offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)*sprSize.x)*4 + ((auxY&0x7)*4);
						
						colour = src[offset];

						// Get 4bits value from the readed 8bits
						if (auxX&1)	colour >>= 4;
						else		colour &= 0xF;

						if(colour && (prioTab[sprX]>=prio))
						{
							T2WriteWord(dst, (sprX<<1), LE_TO_LOCAL_16(T2ReadWord(pal, colour << 1)));
							dst_alpha[sprX] = 16;
							typeTab[sprX] = spriteInfo->Mode;
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

	
			if (!compute_sprite_vars(spriteInfo, l, sprSize, sprX, sprY, x, y, lg, xdir))
				continue;

			if (spriteInfo->Mode == 2)
			{
				if(MODE == SPRITE_2D)
				{
					if (spriteInfo->Depth)
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8));
					else
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4));
				}
				else
				{
					if (spriteInfo->Depth)
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8));
					else
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4));
				}
				if (!src) { 
					continue;
				}

				render_sprite_Win (gpu, l, src, spriteInfo->Depth, lg, sprX, x, xdir);
				continue;
			}

			if (spriteInfo->Mode == 3)              /* sprite is in BMP format */
			{

				//transparent (i think, dont bother to render?) if alpha is 0
				if(spriteInfo->PaletteIndex == 0)
					continue;

				if (dispCnt->OBJ_BMP_mapping)
				{
					//tested by buffy sacrifice damage blood splatters in corner
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<sprBMPBoundary) + (y*sprSize.x*2));
				}
				else
				{
					//NOT TESTED:

					if (dispCnt->OBJ_BMP_2D_dim) // 256*256
					{
						//verified by heroes of mana FMV intro
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3E0) * 64  + (spriteInfo->TileIndex&0x1F) *8 + ( y << 8)) << 1));
					}
					else // 128 * 512
						src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (((spriteInfo->TileIndex&0x3F0) * 64  + (spriteInfo->TileIndex&0x0F) *8 + ( y << 8)) << 1));
				}
				
				if (!src) { 
					continue;
				}

				render_sprite_BMP (gpu, i, l, dst, (u16*)src, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->PaletteIndex);
				continue;
			}
				
			if(spriteInfo->Depth)                   /* 256 colors */
			{
				if(MODE == SPRITE_2D)
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8));
				else
					src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8));
				
				if (!src) { 
					continue;
				}
		
				if (dispCnt->ExOBJPalette_Enable)
					pal = (u16*)(ARM9Mem.ObjExtPal[gpu->core][0]+(spriteInfo->PaletteIndex*0x200));
				else
					pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core *0x400);
		
				render_sprite_256 (gpu, i, l, dst, src, pal, 
					dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);

				continue;
			}
			// 16 colors 
			if(MODE == SPRITE_2D)
			{
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + ((spriteInfo->TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4));
			}
			else
			{
				src = (u8 *)MMU_RenderMapToLCD(gpu->sprMem + (spriteInfo->TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4));
			}
			
			if (!src) { 
				continue;
			}
		
			pal = (u16*)(ARM9Mem.ARM9_VMEM + 0x200 + gpu->core * 0x400);
			
			pal += (spriteInfo->PaletteIndex<<4);
			
			render_sprite_16 (gpu, l, dst, src, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo->Mode == 1);
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

int Screen_Init(int coreid)
{
	MainScreen.gpu = GPU_Init(0);
	SubScreen.gpu = GPU_Init(1);

	memset(GPU_tempScreen, 0, sizeof(GPU_tempScreen));
	for(int i = 0; i < (256*192*2); i++)
		((u16*)GPU_screen)[i] = 0x7FFF;
	disp_fifo.head = disp_fifo.tail = 0;

	if (osd)  {delete osd; osd =NULL; }
	osd  = new OSDCLASS(-1);

	return GPU_ChangeGraphicsCore(coreid);
}

void Screen_Reset(void)
{
	GPU_Reset(MainScreen.gpu, 0);
	GPU_Reset(SubScreen.gpu, 1);

	memset(GPU_tempScreen, 0, sizeof(GPU_tempScreen));
	for(int i = 0; i < (256*192*2); i++)
		((u16*)GPU_screen)[i] = 0x7FFF;

	disp_fifo.head = disp_fifo.tail = 0;
	osd->clear();
}

void Screen_DeInit(void)
{
	GPU_DeInit(MainScreen.gpu);
	GPU_DeInit(SubScreen.gpu);

	if (GFXCore)
		GFXCore->DeInit();

	if (osd)  {delete osd; osd =NULL; }
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
	gpu->dispCapCnt.EVA = std::min((u32)16, (val & 0x1F));
	gpu->dispCapCnt.EVB = std::min((u32)16, ((val >> 8) & 0x1F));
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
			gpu->dispCapCnt.capx = DISPCAPCNT::_128;
			gpu->dispCapCnt.capy = 128;
			break;
		case 1:
			gpu->dispCapCnt.capx = DISPCAPCNT::_256;
			gpu->dispCapCnt.capy = 64;
			break;
		case 2:
			gpu->dispCapCnt.capx = DISPCAPCNT::_256;
			gpu->dispCapCnt.capy = 128;
			break;
		case 3:
			gpu->dispCapCnt.capx = DISPCAPCNT::_256;
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
	itemsForPriority_t * item;
	u8 spr[512];
	u8 sprAlpha[256];
	u8 sprType[256];
	u8 sprPrio[256];
	u8 prio;
	u16 i16;
	BOOL BG_enabled  = TRUE;

	u16 backdrop_color = T1ReadWord(ARM9Mem.ARM9_VMEM, gpu->core * 0x400) & 0x7FFF;

	//we need to write backdrop colors in the same way as we do BG pixels in order to do correct window processing
	//this is currently eating up 2fps or so. it is a reasonable candidate for optimization. 
	gpu->currBgNum = 5;
	for(int x=0;x<256;x++) {
		gpu->__setFinalColorBck<false>(backdrop_color,x,1);
	}

	//this check isnt really helpful. it just slows us down in the cases where we need the most speed
	//if (!gpu->LayersEnable[0] && !gpu->LayersEnable[1] && !gpu->LayersEnable[2] && !gpu->LayersEnable[3] && !gpu->LayersEnable[4]) return;

	// init background color & priorities
	memset(sprAlpha, 0, 256);
	memset(sprType, 0, 256);
	memset(sprPrio, 0xFF, 256);
	memset(sprWin, 0, 256);
	
	// init pixels priorities
	for (int i=0; i<NB_PRIORITIES; i++) {
		gpu->itemsForPriority[i].nbPixelsX = 0;
	}
	
	// for all the pixels in the line
	if (gpu->LayersEnable[4]) 
	{
		//n.b. - this is clearing the sprite line buffer to the background color,
		//but it has been changed to write u32 instead of u16 for a little speedup
		for(int i = 0; i< 128; ++i) T2WriteLong(spr, i << 2, backdrop_color | (backdrop_color<<16));
		//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
		//how it interacts with this. I wish we knew why we needed this
		

		gpu->spriteRender(spr, sprAlpha, sprType, sprPrio);
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
					gpu->currBgNum = i16;
					gpu->blend1 = gpu->BLDCNT & (1 << gpu->currBgNum);
					for(int j=0;j<8;j++)
						gpu->blend2[j] = (gpu->BLDCNT & (0x100 << j));
					gpu->currentFadeInColors = &fadeInColors[gpu->BLDY_EVY][0];
					gpu->currentFadeOutColors = &fadeOutColors[gpu->BLDY_EVY][0];
					//gpu->bgFunc = gpu->setFinalColorBck_funcNum;

					struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[i16].bits;
					gpu->curr_mosaic_enabled = bgCnt->Mosaic_Enable;

					//mosaic test hacks
					//gpu->curr_mosaic_enabled = true;

					if (gpu->core == GPU_MAIN)
					{
						if (i16 == 0 && dispCnt->BG0_3D)
						{
							gpu->currBgNum = 0;

							BGxOFS *bgofs = &gpu->dispx_st->dispx_BGxOFS[i16];
							u16 hofs = (T1ReadWord((u8*)&bgofs->BGxHOFS, 0) & 0x1FF);

							gfx3d_GetLineData(l, &gpu->_3dColorLine, &gpu->_3dAlphaLine);
							u16* colorLine = gpu->_3dColorLine;

							for(int k = 0; k < 256; k++)
							{
								int q = ((k + hofs) & 0x1FF);

								if((q < 0) || (q > 255))
									continue;

								if(colorLine[q] & 0x8000)
									gpu->setFinalColor3d(k, q);
							}

							continue;
						}
					}

					//useful for debugging individual layers
					//if(gpu->core == 0 && i16 != 1) continue;

					if(gpu->curr_mosaic_enabled)
						gpu->modeRender<true>(i16);
					else gpu->modeRender<false>(i16);
				} //layer enabled
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
				gpu->setFinalColorSpr(gpu, (i16<<1), gpu->currDst, T2ReadWord(spr, (i16<<1)), sprAlpha[i16], sprType[i16], i16);
			}
		}
	}
}

// TODO: capture emulated not fully
static void GPU_ligne_DispCapture(u16 l)
{
	//this macro takes advantage of the fact that there are only two possible values for capx
	#define CAPCOPY(SRC,DST) \
	switch(gpu->dispCapCnt.capx) { \
		case DISPCAPCNT::_128: \
			for (int i = 0; i < 128; i++)  \
				T2WriteWord(DST, i << 1, T2ReadWord(SRC, i << 1) | (1<<15)); \
			break; \
		case DISPCAPCNT::_256: \
			for (int i = 0; i < 256; i++)  \
				T2WriteWord(DST, i << 1, T2ReadWord(SRC, i << 1) | (1<<15)); \
			break; \
			default: assert(false); \
		}
	
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

									u8 *src;
									src = (u8*)(GPU_tempScanline);
									CAPCOPY(src,cap_dst);
								}
							break;
							case 1:			// Capture 3D
								{
									//INFO("Capture 3D\n");
									u16* colorLine;
									gfx3d_GetLineData(l, &colorLine, NULL);
									CAPCOPY(((u8*)colorLine),cap_dst);
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
									CAPCOPY(src,cap_dst);
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

						if (gpu->dispCapCnt.srcA == 0)
						{
							// Capture screen (BG + OBJ + 3D)
							srcA = (u16*)(GPU_tempScanline);
						}
						else
						{
							gfx3d_GetLineData(l, &srcA, NULL);
						}

						if (gpu->dispCapCnt.srcB == 0)			// VRAM screen
							srcB = (u16 *)((gpu->dispCapCnt.src) + (l * 512));
						else
							srcB = NULL; // DISP FIFOS

						if ((srcA) && (srcB))
						{
							u16 a, r, g, b;
							const int todo = (gpu->dispCapCnt.capx==DISPCAPCNT::_128?128:256);
							for(u16 i = 0; i < todo; i++) 
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

	u8 * dst =  GPU_tempScreen + (screen->offset + l) * 512;
	u16 i16;

	//isn't it odd that we can set uselessly high factors here?
	//factors above 16 change nothing. curious.
	int factor = gpu->MasterBrightFactor;
	if(factor==0) return;
	if(factor>16) factor=16;


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
				((u16*)dst)[i16] = fadeInColors[factor][((u16*)dst)[i16]&0x7FFF];
			}
			break;
		}

		// Bright down
		case 2:
		{
			for(i16 = 0; i16 < 256; ++i16)
			{
				((u16*)dst)[i16] = fadeOutColors[factor][((u16*)dst)[i16]&0x7FFF];
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
		//this is speculative. the idea is as follows:
		//whenever the user updates the affine start position regs, it goes into the active regs immediately
		//(this is handled on the set event from MMU)
		//maybe it shouldnt take effect until the next hblank or something..
		//this is a based on a combination of:
		//heroes of mana intro FMV
		//SPP level 3-8 rotoscale room
		//NSMB raster fx backdrops
		//bubble bobble revolution classic mode
		//NOTE:
		//I am REALLY unsatisfied with this logic now. But it seems to be working..
		gpu->refreshAffineStartRegs(-1,-1);
	}

	//blacken the screen if it is turned off by the user
	if(!CommonSettings.showGpu.screens[gpu->core])
	{
		u8 * dst =  GPU_tempScreen + (screen->offset + l) * 512;
		memset(dst,0,512);
		return;
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

	//always generate the 2d+3d, no matter what we're displaying, since we may need to capture it
	//(if this seems inefficient in some cases, consider that the speed in those cases is not really a problem)
	GPU_tempScanline = screen->gpu->currDst = (u8 *)(GPU_tempScreen) + (screen->offset + l) * 512;
	GPU_ligne_layer(screen, l);

	if (gpu->core == GPU_MAIN) 
	{
		GPU_ligne_DispCapture(l);
		if (l == 191) { disp_fifo.head = disp_fifo.tail = 0; }
	}

	switch (gpu->dispMode)
	{
		case 0: // Display Off(Display white)
			{
				u8 * dst =  GPU_tempScreen + (screen->offset + l) * 512;

				for (int i=0; i<256; i++)
					T2WriteWord(dst, i << 1, 0x7FFF);
			}
			break;

		case 1: // Display BG and OBJ layers
			//do nothing: it has already been generated into the right place
			break;

		case 2: // Display framebuffer
			{
				u8 * dst = GPU_tempScreen + (screen->offset + l) * 512;
				u8 * src = gpu->VRAMaddr + (l*512);
				memcpy (dst, src, 512);
			}
			break;
		case 3: // Display memory FIFO
			{
				u8 * dst =  GPU_tempScreen + (screen->offset + l) * 512;
				for (int i=0; i < 128; i++)
					T1WriteLong(dst, i << 2, DISP_FIFOrecv() & 0x7FFF7FFF);
			}
			break;
	}


	
	GPU_ligne_MasterBrightness(screen, l);
}

void gpu_savestate(std::ostream* os)
{
	//version
	write32le(1,os);
	
	os->write((char*)GPU_screen,sizeof(GPU_screen));
	
	write32le(MainScreen.gpu->affineInfo[0].x,os);
	write32le(MainScreen.gpu->affineInfo[0].y,os);
	write32le(MainScreen.gpu->affineInfo[1].x,os);
	write32le(MainScreen.gpu->affineInfo[1].y,os);
	write32le(SubScreen.gpu->affineInfo[0].x,os);
	write32le(SubScreen.gpu->affineInfo[0].y,os);
	write32le(SubScreen.gpu->affineInfo[1].x,os);
	write32le(SubScreen.gpu->affineInfo[1].y,os);
}

bool gpu_loadstate(std::istream* is, int size)
{
	//read version
	int version;

	//sigh.. shouldve used a new version number
	if(size == 256*192*2*2)
		version = 0;
	else if(size== 0x30024)
	{
		read32le(&version,is);
		version = 1;
	}
	else
		if(read32le(&version,is) != 1) return false;
		

	if(version<0||version>1) return false;

	is->read((char*)GPU_screen,sizeof(GPU_screen));

	if(version==1)
	{
		read32le(&MainScreen.gpu->affineInfo[0].x,is);
		read32le(&MainScreen.gpu->affineInfo[0].y,is);
		read32le(&MainScreen.gpu->affineInfo[1].x,is);
		read32le(&MainScreen.gpu->affineInfo[1].y,is);
		read32le(&SubScreen.gpu->affineInfo[0].x,is);
		read32le(&SubScreen.gpu->affineInfo[0].y,is);
		read32le(&SubScreen.gpu->affineInfo[1].x,is);
		read32le(&SubScreen.gpu->affineInfo[1].y,is);
		MainScreen.gpu->refreshAffineStartRegs(-1,-1);
		SubScreen.gpu->refreshAffineStartRegs(-1,-1);
	}

	MainScreen.gpu->updateBLDALPHA();
	SubScreen.gpu->updateBLDALPHA();
	return !is->fail();
}

u32 GPU::getAffineStart(int layer, int xy)
{
	if(xy==0) return affineInfo[layer-2].x;
	else return affineInfo[layer-2].y;
}

void GPU::setAffineStartWord(int layer, int xy, u16 val, int word)
{
	u32 curr = getAffineStart(layer,xy);
	if(word==0) curr = (curr&0xFFFF0000)|val;
	else curr = (curr&0x0000FFFF)|(((u32)val)<<16);
	setAffineStart(layer,xy,curr);
}

void GPU::setAffineStart(int layer, int xy, u32 val)
{
	if(xy==0) affineInfo[layer-2].x = val;
	else affineInfo[layer-2].y = val;
	refreshAffineStartRegs(layer,xy);
}

void GPU::refreshAffineStartRegs(const int num, const int xy)
{
	if(num==-1)
	{
		refreshAffineStartRegs(2,xy);
		refreshAffineStartRegs(3,xy);
		return;
	}

	if(xy==-1)
	{
		refreshAffineStartRegs(num,0);
		refreshAffineStartRegs(num,1);
		return;
	}

	BGxPARMS * parms;
	if (num==2)
		parms = &(dispx_st)->dispx_BG2PARMS;
	else
		parms = &(dispx_st)->dispx_BG3PARMS;		

	if(xy==0)
		parms->BGxX = affineInfo[num-2].x;
	else
		parms->BGxY = affineInfo[num-2].y;
}

template<bool MOSAIC> void GPU::modeRender(int layer)
{
	switch(mode2type[dispCnt().BG_Mode][layer])
	{
		case BGType_Text: lineText<MOSAIC>(this); break;
		case BGType_Affine: lineRot<MOSAIC>(this); break;
		case BGType_AffineExt: lineExtRot<MOSAIC>(this); break;
		case BGType_Large8bpp: lineExtRot<MOSAIC>(this); break;
		case BGType_Invalid: 
			PROGINFO("Attempting to render an invalid BG type\n");
			break;
		default:
			break;
	}
}

void gpu_UpdateRender()
{
	int x = 0, y = 0;
	u16 *src = (u16*)GPU_tempScreen;
	u16	*dst = (u16*)GPU_screen;

	switch (gpu_angle)
	{
		case 0:
			memcpy(dst, src, 256*192*4);
			break;

		case 90:
			for(y = 0; y < 384; y++)
			{
				for(x = 0; x < 256; x++)
				{
					dst[(383 - y) + (x * 384)] = src[x + (y * 256)];
				}
			}
			break;
		case 180:
			for(y = 0; y < 384; y++)
			{
				for(x = 0; x < 256; x++)
				{
					dst[(255 - x) + ((383 - y) * 256)] = src[x + (y * 256)];
				}
			}
			break;
		case 270:
			for(y = 0; y < 384; y++)
			{
				for(x = 0; x < 256; x++)
				{
					dst[y + ((255 - x) * 384)] = src[x + (y * 256)];
				}
			}
		default:
			break;
	}
}

void gpu_SetRotateScreen(u16 angle)
{
	gpu_angle = angle;
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
