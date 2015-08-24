/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2008-2015 DeSmuME team

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

#include "GPU.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <algorithm>
#include <iostream>

#ifdef ENABLE_SSE2
#include <emmintrin.h>
#endif

#ifdef ENABLE_SSSE3
#include <tmmintrin.h>
#endif

#include "common.h"
#include "MMU.h"
#include "FIFO.h"
#include "debug.h"
#include "render3D.h"
#include "registers.h"
#include "gfx3d.h"
#include "debug.h"
#include "GPU_osd.h"
#include "NDSSystem.h"
#include "readwrite.h"
#include "matrix.h"
#include "emufile.h"

#ifdef FASTBUILD
	#undef FORCEINLINE
	#define FORCEINLINE
	//compilation speed hack (cuts time exactly in half by cutting out permutations)
	#define DISABLE_MOSAIC
#endif

//instantiate static instance
GPU::MosaicLookup GPU::mosaicLookup;

NDS_Screen MainScreen;
NDS_Screen SubScreen;
NDSDisplay MainDisplay(NDSDisplayID_Main, GPUCOREID_MAIN);
NDSDisplay TouchDisplay(NDSDisplayID_Touch, GPUCOREID_SUB);

static NDSDisplayInfo _displayInfo;
static u16 *GPU_CustomFramebuffer = NULL;
u16 *GPU_screen = GPU_CustomFramebuffer; // TODO: Old pointer - need to eliminate direct reference in frontends

static size_t _gpuLargestDstLineCount = 1;
static size_t _gpuVRAMBlockOffset = GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH;
static bool _gpuIsNativeSize = true;

static u16 *_gpuDstToSrcIndex = NULL; // Key: Destination pixel index / Value: Source pixel index

static CACHE_ALIGN size_t _gpuDstPitchCount[GPU_FRAMEBUFFER_NATIVE_WIDTH];	// Key: Source pixel index in x-dimension / Value: Number of x-dimension destination pixels for the source pixel
static CACHE_ALIGN size_t _gpuDstPitchIndex[GPU_FRAMEBUFFER_NATIVE_WIDTH];	// Key: Source pixel index in x-dimension / Value: First destination pixel that maps to the source pixel
static CACHE_ALIGN size_t _gpuDstLineCount[GPU_FRAMEBUFFER_NATIVE_HEIGHT];	// Key: Source line index / Value: Number of destination lines for the source line
static CACHE_ALIGN size_t _gpuDstLineIndex[GPU_FRAMEBUFFER_NATIVE_HEIGHT];	// Key: Source line index / Value: First destination line that maps to the source line
static CACHE_ALIGN size_t _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES + 1];	// Key: Source line index / Value: First destination line that maps to the source line

static CACHE_ALIGN u8 sprWin[GPU_FRAMEBUFFER_NATIVE_WIDTH];

static VRAM3DUsageProperties vram3DUsage;
static u16 *_gpuCustomVRAM = NULL;
static u16 *_gpuCustomVRAMBlank = NULL;

static const CACHE_ALIGN SpriteSize sprSizeTab[4][4] = {
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};

static const CACHE_ALIGN BGType GPU_mode2type[8][4] = {
      {BGType_Text, BGType_Text, BGType_Text, BGType_Text},
      {BGType_Text, BGType_Text, BGType_Text, BGType_Affine},
      {BGType_Text, BGType_Text, BGType_Affine, BGType_Affine},
      {BGType_Text, BGType_Text, BGType_Text, BGType_AffineExt},
      {BGType_Text, BGType_Text, BGType_Affine, BGType_AffineExt},
      {BGType_Text, BGType_Text, BGType_AffineExt, BGType_AffineExt},
      {BGType_Invalid, BGType_Invalid, BGType_Large8bpp, BGType_Invalid},
      {BGType_Invalid, BGType_Invalid, BGType_Invalid, BGType_Invalid}
};

//dont ever think of changing these to bits because you could avoid the multiplies in the main tile blitter.
//it doesnt really help any
static const CACHE_ALIGN u16 sizeTab[8][4][2] = {
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //Invalid
    {{256,256}, {512,256}, {256,512}, {512,512}}, //text
    {{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine
    {{512,1024}, {1024,512}, {0,0}, {0,0}}, //large 8bpp
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //affine ext (to be elaborated with another value)
	{{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine ext 256x16
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext 256x1
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext direct
};

static const CACHE_ALIGN u8 win_empty[GPU_FRAMEBUFFER_NATIVE_WIDTH] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

static CACHE_ALIGN u16 fadeInColors[17][0x8000];
static CACHE_ALIGN u16 fadeOutColors[17][0x8000];

//this should be public, because it gets used somewhere else
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
			cur.bits.alpha = 0;
			fadeInColors[i][j & 0x7FFF] = cur.val;

			cur.val = j;
			cur.bits.red = (cur.bits.red - (cur.bits.red * i / 16));
			cur.bits.green = (cur.bits.green - (cur.bits.green * i / 16));
			cur.bits.blue = (cur.bits.blue - (cur.bits.blue * i / 16));
			cur.bits.alpha = 0;
			fadeOutColors[i][j & 0x7FFF] = cur.val;
		}
	}


	for(int c0=0;c0<=31;c0++) 
		for(int c1=0;c1<=31;c1++) 
			for(int eva=0;eva<=16;eva++)
				for(int evb=0;evb<=16;evb++)
				{
					int blend = ((c0 * eva) + (c1 * evb) ) / 16;
					int final = std::min<int>(31,blend);
					gpuBlendTable555[eva][evb][c0][c1] = final;
				}
}

static CACHE_ALIGN GPU GPU_main, GPU_sub;

GPU* GPU_Init(const GPUCoreID coreID)
{
	GPU *gpu = (coreID == GPUCOREID_MAIN) ? &GPU_main : &GPU_sub;
	gpu->core = coreID;
	gpu->tempScanlineBuffer = NULL;
	gpu->bgPixels = NULL;
	
	GPU_Reset(gpu);
	GPU_InitFadeColors();
	
	gpu->targetDisplayID = (coreID == GPUCOREID_MAIN) ? NDSDisplayID_Main : NDSDisplayID_Touch;
	gpu->curr_win[0] = win_empty;
	gpu->curr_win[1] = win_empty;
	gpu->need_update_winh[0] = true;
	gpu->need_update_winh[1] = true;
	gpu->setFinalColorBck_funcNum = 0;
	gpu->setFinalColor3d_funcNum = 0;
	gpu->setFinalColorSpr_funcNum = 0;
	gpu->VRAMaddrNative = (u16 *)MMU.ARM9_LCD;
	gpu->VRAMaddrCustom = _gpuCustomVRAM;
	
	gpu->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	gpu->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
#ifdef ENABLE_COMBINATION_RENDERING
	gpu->nativeBuffer = (coreID == GPUCOREID_MAIN) ? GPU_NativeFramebuffer : GPU_NativeFramebuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
#else
	gpu->nativeBuffer = NULL;
#endif
	gpu->customBuffer = NULL;
	gpu->renderedBuffer = gpu->nativeBuffer;
	
	return gpu;
}

void GPU_Reset(GPU *gpu)
{
	// TODO: The memset() here completely wipes out the GPU structure, so any states
	// that need to be persistent will be lost here. So we're just going to save
	// whatever persistent states we need now, do the memset(), and then restore them.
	// It would be better if we were more precise about GPU resets.
	const GPUCoreID currentCoreID = gpu->core;
	const NDSDisplayID currentDisplayID = gpu->targetDisplayID;
	u16 *currentTempScanlineBuffer = gpu->tempScanlineBuffer;
	u8 *currentBGPixels = gpu->bgPixels;
	bool currentIsCustomRenderingNeeded = gpu->isCustomRenderingNeeded;
	u16 *currentCustomBuffer = gpu->customBuffer;
	u16	*currentNativeBuffer = gpu->nativeBuffer;
	
	memset(gpu, 0, sizeof(GPU));
	
	gpu->core = currentCoreID;
	gpu->targetDisplayID = currentDisplayID;
	gpu->tempScanlineBuffer = currentTempScanlineBuffer;
	gpu->bgPixels = currentBGPixels;
	gpu->isCustomRenderingNeeded = currentIsCustomRenderingNeeded;
	gpu->customBuffer = currentCustomBuffer;
	gpu->nativeBuffer = currentNativeBuffer;
	
	gpu->vramBlockBGIndex = VRAM_NO_3D_USAGE;
	gpu->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	gpu->vramBGLayer = VRAM_NO_3D_USAGE;
	
	// Clear the separate memory blocks that weren't cleared in the last memset()
	if (gpu->tempScanlineBuffer != NULL) memset(gpu->tempScanlineBuffer, 0, _displayInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	if (gpu->bgPixels != NULL) memset(gpu->bgPixels, 0, _displayInfo.customWidth * _gpuLargestDstLineCount * 4 * sizeof(u8));
	
	//important for emulator stability for this to initialize, since we have to setup a table based on it
	gpu->BLDALPHA_EVA = 0;
	gpu->BLDALPHA_EVB = 0;
	//make sure we have our blend table setup even if the game blends without setting the blend variables
	gpu->updateBLDALPHA();

	gpu->setFinalColorBck_funcNum = 0;
	gpu->setFinalColor3d_funcNum = 0;
	gpu->setFinalColorSpr_funcNum = 0;
	gpu->BGSize[0][0] = gpu->BGSize[1][0] = gpu->BGSize[2][0] = gpu->BGSize[3][0] = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	gpu->BGSize[0][1] = gpu->BGSize[1][1] = gpu->BGSize[2][1] = gpu->BGSize[3][1] = GPU_FRAMEBUFFER_NATIVE_WIDTH;

	gpu->spriteRenderMode = GPU::SPRITE_1D;

	gpu->bgPrio[4] = 0xFF;

	gpu->bg0HasHighestPrio = true;

	if (gpu->core == GPUCOREID_SUB)
	{
		gpu->oamList = (OAMAttributes *)(MMU.ARM9_OAM + ADDRESS_STEP_1KB);
		gpu->sprMem = MMU_BOBJ;
		// GPU core B
		gpu->dispx_st = (REG_DISPx*)(&MMU.ARM9_REG[REG_DISPB]);
	}
	else
	{
		gpu->oamList = (OAMAttributes *)(MMU.ARM9_OAM);
		gpu->sprMem = MMU_AOBJ;
		// GPU core A
		gpu->dispx_st = (REG_DISPx*)(&MMU.ARM9_REG[0]);
	}
}

void GPU_DeInit(GPU *gpu)
{
	if (gpu == &GPU_main || gpu == &GPU_sub) return;
	
	free_aligned(gpu->tempScanlineBuffer);
	gpu->tempScanlineBuffer = NULL;
	free_aligned(gpu->bgPixels);
	gpu->bgPixels = NULL;
	
	free(gpu);
	gpu = NULL;
}

static void GPU_resortBGs(GPU *gpu)
{
	int i, prio;
	struct _DISPCNT * cnt = &gpu->dispx_st->dispx_DISPCNT.bits;
	itemsForPriority_t *item;

	// we don't need to check for windows here...
// if we tick boxes, invisible layers become invisible & vice versa
#define OP ^ !
// if we untick boxes, layers become invisible
//#define OP &&
	gpu->LayersEnable[0] = CommonSettings.dispLayers[gpu->core][0] OP(cnt->BG0_Enable/* && !(cnt->BG0_3D && (gpu->core==0))*/);
	gpu->LayersEnable[1] = CommonSettings.dispLayers[gpu->core][1] OP(cnt->BG1_Enable);
	gpu->LayersEnable[2] = CommonSettings.dispLayers[gpu->core][2] OP(cnt->BG2_Enable);
	gpu->LayersEnable[3] = CommonSettings.dispLayers[gpu->core][3] OP(cnt->BG3_Enable);
	gpu->LayersEnable[4] = CommonSettings.dispLayers[gpu->core][4] OP(cnt->OBJ_Enable);

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
	gpu->bg0HasHighestPrio = true;
	for(i = 1; i < 4; i++)
	{
		if(gpu->LayersEnable[i])
		{
			if(gpu->dispx_st->dispx_BGxCNT[i].bits.Priority < bg0Prio)
			{
				gpu->bg0HasHighestPrio = false;
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

static FORCEINLINE u16 _blend(const u16 colA, const u16 colB, GPU::TBlendTable *blendTable)
{
	const u8 r = (*blendTable)[colA&0x1F][colB&0x1F];
	const u8 g = (*blendTable)[(colA>>5)&0x1F][(colB>>5)&0x1F];
	const u8 b = (*blendTable)[(colA>>10)&0x1F][(colB>>10)&0x1F];

	return r|(g<<5)|(b<<10);
}

FORCEINLINE u16 GPU::blend(const u16 colA, const u16 colB)
{
	return _blend(colA, colB, blendTable);
}


void GPU_setMasterBrightness(GPU *gpu, const u16 val)
{
	if (!nds.isInVblank())
	{
		PROGINFO("Changing master brightness outside of vblank\n");
	}
	
 	gpu->MasterBrightFactor = (val & 0x1F);
	gpu->MasterBrightMode = (GPUMasterBrightMode)(val>>14);
	//printf("MASTER BRIGHTNESS %d to %d at %d\n",gpu->core,gpu->MasterBrightFactor,nds.VCount);
}

void SetupFinalPixelBlitter(GPU *gpu)
{
	const u8 windowUsed = (gpu->WIN0_ENABLED | gpu->WIN1_ENABLED | gpu->WINOBJ_ENABLED);
	const u8 blendMode = (gpu->BLDCNT >> 6) & 3;
	const int funcNum = (windowUsed * 4) + blendMode;

	gpu->setFinalColorSpr_funcNum = funcNum;
	gpu->setFinalColorBck_funcNum = funcNum;
	gpu->setFinalColor3d_funcNum = funcNum;
}
    
//Sets up LCD control variables for Display Engines A and B for quick reading
void GPU_setVideoProp(GPU *gpu, const u32 ctrlBits)
{
	struct _DISPCNT *cnt;
	cnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;

	gpu->dispx_st->dispx_DISPCNT.val = LE_TO_LOCAL_32(ctrlBits);

	gpu->WIN0_ENABLED	= cnt->Win0_Enable;
	gpu->WIN1_ENABLED	= cnt->Win1_Enable;
	gpu->WINOBJ_ENABLED = cnt->WinOBJ_Enable;

	SetupFinalPixelBlitter(gpu);

    gpu->dispMode = (GPUDisplayMode)( cnt->DisplayMode & ((gpu->core == GPUCOREID_SUB)?1:3) );
	gpu->vramBlock = cnt->VRAM_Block;

	switch (gpu->dispMode)
	{
		case GPUDisplayMode_Off: // Display Off
			break;
			
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			break;
			
		case GPUDisplayMode_VRAM: // Display framebuffer
			gpu->VRAMaddrNative = (u16 *)MMU.ARM9_LCD + (gpu->vramBlock * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
			gpu->VRAMaddrCustom = _gpuCustomVRAM + (gpu->vramBlock * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * _displayInfo.customWidth);
			break;
			
		case GPUDisplayMode_MainMemory: // Display from Main RAM
			// nothing to be done here
			// see GPU_RenderLine who gets data from FIFO.
			break;
	}

	if (cnt->OBJ_Tile_mapping)
	{
		//1-d sprite mapping boundaries:
		//32k, 64k, 128k, 256k
		gpu->sprBoundary = 5 + cnt->OBJ_Tile_1D_Bound;
		
		//do not be deceived: even though a sprBoundary==8 (256KB region) is impossible to fully address
		//in GPU_SUB, it is still fully legal to address it with that granularity.
		//so don't do this: //if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3)) gpu->sprBoundary = 7;

		gpu->spriteRenderMode = GPU::SPRITE_1D;
	}
	else
	{
		//2d sprite mapping
		//boundary : 32k
		gpu->sprBoundary = 5;
		gpu->spriteRenderMode = GPU::SPRITE_2D;
	}
     
	if (cnt->OBJ_BMP_1D_Bound && (gpu->core == GPUCOREID_MAIN))
		gpu->sprBMPBoundary = 8;
	else
		gpu->sprBMPBoundary = 7;

	gpu->sprEnable = cnt->OBJ_Enable;
	
	GPU_setBGProp(gpu, 3, T1ReadWord(MMU.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 14));
	GPU_setBGProp(gpu, 2, T1ReadWord(MMU.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 12));
	GPU_setBGProp(gpu, 1, T1ReadWord(MMU.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 10));
	GPU_setBGProp(gpu, 0, T1ReadWord(MMU.ARM9_REG, gpu->core * ADDRESS_STEP_4KB + 8));
	
	//GPU_resortBGs(gpu);
}

//this handles writing in BGxCNT
void GPU_setBGProp(GPU *gpu, const size_t num, const u16 ctrlBits)
{
	struct _BGxCNT *cnt = &((gpu->dispx_st)->dispx_BGxCNT[num].bits);
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	
	gpu->dispx_st->dispx_BGxCNT[num].val = LE_TO_LOCAL_16(ctrlBits);
	
	GPU_resortBGs(gpu);

	if (gpu->core == GPUCOREID_SUB)
	{
		gpu->BG_tile_ram[num] = MMU_BBG;
		gpu->BG_bmp_ram[num]  = MMU_BBG;
		gpu->BG_bmp_large_ram[num]  = MMU_BBG;
		gpu->BG_map_ram[num]  = MMU_BBG;
	} 
	else 
	{
		gpu->BG_tile_ram[num] = MMU_ABG +  dispCnt->CharacBase_Block * ADDRESS_STEP_64KB;
		gpu->BG_bmp_ram[num]  = MMU_ABG;
		gpu->BG_bmp_large_ram[num] = MMU_ABG;
		gpu->BG_map_ram[num]  = MMU_ABG +  dispCnt->ScreenBase_Block * ADDRESS_STEP_64KB;
	}

	gpu->BG_tile_ram[num] += (cnt->CharacBase_Block * ADDRESS_STEP_16KB);
	gpu->BG_bmp_ram[num]  += (cnt->ScreenBase_Block * ADDRESS_STEP_16KB);
	gpu->BG_map_ram[num]  += (cnt->ScreenBase_Block * ADDRESS_STEP_2KB);

	switch (num)
	{
		case 0:
		case 1:
			gpu->BGExtPalSlot[num] = cnt->PaletteSet_Wrap * 2 + num;
			break;
			
		default:
			gpu->BGExtPalSlot[num] = (u8)num;
			break;
	}

	BGType mode = GPU_mode2type[dispCnt->BG_Mode][num];

	//clarify affine ext modes 
	if (mode == BGType_AffineExt)
	{
		//see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol
		const u8 affineModeSelection = (cnt->Palette_256 << 1) | (cnt->CharacBase_Block & 1);
		switch (affineModeSelection)
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
	
	gpu->bgPrio[num] = (ctrlBits & 0x3);
}

/*****************************************************************************/
//			ENABLING / DISABLING LAYERS
/*****************************************************************************/

void GPU_remove(GPU *gpu, const size_t num)
{
	CommonSettings.dispLayers[gpu->core][num] = false;
	GPU_resortBGs(gpu);
}

void GPU_addBack(GPU *gpu, const size_t num)
{
	CommonSettings.dispLayers[gpu->core][num] = true;
	GPU_resortBGs(gpu);
}


/*****************************************************************************/
//		ROUTINES FOR INSIDE / OUTSIDE WINDOW CHECKS
/*****************************************************************************/

template<int WIN_NUM>
FORCEINLINE u8 GPU::withinRect(const size_t x) const
{
	return curr_win[WIN_NUM][x];
}



//  Now assumes that *draw and *effect are different from 0 when called, so we can avoid
// setting some values twice
FORCEINLINE void GPU::renderline_checkWindows(const size_t srcX, bool &draw, bool &effect) const
{
	// Check if win0 if enabled, and only check if it is
	// howevever, this has already been taken care of by the window precalculation
	//if (WIN0_ENABLED)
	{
		// it is in win0, do we display ?
		// high priority	
		if (withinRect<0>(srcX))
		{
			//INFO("bg%i passed win0 : (%i %i) was within (%i %i)(%i %i)\n", bgnum, x, gpu->currLine, gpu->WIN0H0, gpu->WIN0V0, gpu->WIN0H1, gpu->WIN0V1);
			draw = (WININ0 >> currBgNum) & 1;
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
		if (withinRect<1>(srcX))
		{
			//INFO("bg%i passed win1 : (%i %i) was within (%i %i)(%i %i)\n", bgnum, x, gpu->currLine, gpu->WIN1H0, gpu->WIN1V0, gpu->WIN1H1, gpu->WIN1V1);
			draw = (WININ1 >> currBgNum) & 1;
			effect = (WININ1_SPECIAL);
			return;
		}
	}

	//if(true) //sprwin test hack
	if (WINOBJ_ENABLED)
	{
		// it is in winOBJ, do we display ?
		// low priority
		if (sprWin[srcX])
		{
			draw = (WINOBJ >> currBgNum) & 1;
			effect = (WINOBJ_SPECIAL);
			return;
		}
	}

	if (WINOBJ_ENABLED | WIN1_ENABLED | WIN0_ENABLED)
	{
		draw = (WINOUT >> currBgNum) & 1;
		effect = (WINOUT_SPECIAL);
	}
}

/*****************************************************************************/
//			PIXEL RENDERING
/*****************************************************************************/

template<BlendFunc FUNC, bool WINDOW>
FORCEINLINE FASTCALL void GPU::_master_setFinal3dColor(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const FragmentColor src)
{
	u8 alpha = src.a;
	u16 final;

	bool windowEffect = blend1; //bomberman land touch dialogbox will fail without setting to blend1
	
	//TODO - should we do an alpha==0 -> bail out entirely check here?

	if (WINDOW)
	{
		bool windowDraw = false;
		renderline_checkWindows(srcX, windowDraw, windowEffect);

		//we never have anything more to do if the window rejected us
		if (!windowDraw) return;
	}

	const size_t bg_under = bgPixelsLine[dstX];
	if (blend2[bg_under])
	{
		alpha++;
		if (alpha < 32)
		{
			//if the layer underneath is a blend bottom layer, then 3d always alpha blends with it
			COLOR c2, cfinal;

			c2.val = dstLine[dstX];

			cfinal.bits.red = ((src.r * alpha) + ((c2.bits.red<<1) * (32 - alpha)))>>6;
			cfinal.bits.green = ((src.g * alpha) + ((c2.bits.green<<1) * (32 - alpha)))>>6;
			cfinal.bits.blue = ((src.b * alpha) + ((c2.bits.blue<<1) * (32 - alpha)))>>6;

			final = cfinal.val;
		}
		else final = R6G6B6TORGB15(src.r, src.g, src.b);
	}
	else
	{
		final = R6G6B6TORGB15(src.r, src.g, src.b);
		//perform the special effect
		if (windowEffect)
		{
			switch (FUNC)
			{
				case Increase: final = currentFadeInColors[final&0x7FFF]; break;
				case Decrease: final = currentFadeOutColors[final&0x7FFF]; break;
				case NoBlend:
				case Blend:
					break;
			}
		}
	}
	
	dstLine[dstX] = final | 0x8000;
	bgPixelsLine[dstX] = 0;
}

template<bool BACKDROP, BlendFunc FUNC, bool WINDOW>
FORCEINLINE FASTCALL bool GPU::_master_setFinalBGColor(const size_t srcX, const size_t dstX, const u16 *dstLine, const u8 *bgPixelsLine, u16 &outColor)
{
	//no further analysis for no special effects. on backdrops. just draw it.
	if ((FUNC == NoBlend) && BACKDROP) return true;

	//blend backdrop with what?? this doesn't make sense
	if ((FUNC == Blend) && BACKDROP) return true;

	bool windowEffect = true;

	if (WINDOW)
	{
		bool windowDraw = false;
		renderline_checkWindows(srcX, windowDraw, windowEffect);

		//backdrop must always be drawn
		if (BACKDROP) windowDraw = true;

		//we never have anything more to do if the window rejected us
		if (!windowDraw) return false;
	}

	//special effects rejected. just draw it.
	if (!(blend1 && windowEffect))
		return true;

	const size_t bg_under = bgPixelsLine[dstX];

	//perform the special effect
	switch (FUNC)
	{
		case Blend: if(blend2[bg_under]) outColor = blend(outColor, dstLine[dstX]); break;
		case Increase: outColor = currentFadeInColors[outColor]; break;
		case Decrease: outColor = currentFadeOutColors[outColor]; break;
		case NoBlend: break;
	}
	return true;
}

template<BlendFunc FUNC, bool WINDOW>
FORCEINLINE FASTCALL void GPU::_master_setFinalOBJColor(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const u16 src, const u8 alpha, const u8 type)
{
	bool windowEffectSatisfied = true;

	if (WINDOW)
	{
		bool windowDraw = true;
		renderline_checkWindows(srcX, windowDraw, windowEffectSatisfied);
		if (!windowDraw)
			return;
	}
	
	u16 finalDstColor = src;
	
	//if the window effect is satisfied, then we can do color effects to modify the color
	if (windowEffectSatisfied)
	{
		const bool isObjTranslucentType = type == GPU_OBJ_MODE_Transparent || type == GPU_OBJ_MODE_Bitmap;
		const size_t bg_under = bgPixelsLine[dstX];
		const bool firstTargetSatisfied = blend1;
		const bool secondTargetSatisfied = (bg_under != 4) && blend2[bg_under];
		BlendFunc selectedFunc = NoBlend;

		u8 eva = BLDALPHA_EVA;
		u8 evb = BLDALPHA_EVB;

		//if normal BLDCNT layer target conditions are met, then we can use the BLDCNT-specified color effect
		if (FUNC == Blend)
		{
			//blending requires first and second target screens to be satisfied
			if (firstTargetSatisfied && secondTargetSatisfied) selectedFunc = FUNC;
		}
		else 
		{
			//brightness up and down requires only the first target screen to be satisfied
			if (firstTargetSatisfied) selectedFunc = FUNC;
		}

		//translucent-capable OBJ are forcing the function to blend when the second target is satisfied
		if (isObjTranslucentType && secondTargetSatisfied)
		{
			selectedFunc = Blend;
		
			//obj without fine-grained alpha are using EVA/EVB for blending. this is signified by receiving 255 in the alpha
			//it's tested by the spriteblend demo and the glory of heracles title screen
			if (alpha != 255)
			{
				eva = alpha;
				evb = 16 - alpha;
			}
		}
		
		switch (selectedFunc)
		{
			case NoBlend:
				break;
				
			case Increase:
				finalDstColor = currentFadeInColors[src & 0x7FFF];
				break;
				
			case Decrease:
				finalDstColor = currentFadeOutColors[src & 0x7FFF];
				break;
				
			case Blend:
				finalDstColor = _blend(src, dstLine[dstX], &gpuBlendTable555[eva][evb]);
				break;
				
			default:
				break;
		}
	}
	
	dstLine[dstX] = finalDstColor | 0x8000;
	bgPixelsLine[dstX] = 4;
}

//FUNCNUM is only set for backdrop, for an optimization of looking it up early
template<bool BACKDROP, int FUNCNUM> 
FORCEINLINE void GPU::setFinalColorBG(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, u16 src)
{
	//It is not safe to assert this here.
	//This is probably the best place to enforce it, since almost every single color that comes in here
	//will be pulled from a palette that needs the top bit stripped off anyway.
	//assert((src&0x8000)==0);
	if (!BACKDROP) src &= 0x7FFF; //but for the backdrop we can easily guarantee earlier that theres no bit here

	bool draw = false;

	const int test = (BACKDROP) ? FUNCNUM : setFinalColorBck_funcNum;
	switch (test)
	{
		case 0: draw = _master_setFinalBGColor<BACKDROP,NoBlend, false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 1: draw = _master_setFinalBGColor<BACKDROP,Blend,   false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 2: draw = _master_setFinalBGColor<BACKDROP,Increase,false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 3: draw = _master_setFinalBGColor<BACKDROP,Decrease,false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 4: draw = _master_setFinalBGColor<BACKDROP,NoBlend,  true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 5: draw = _master_setFinalBGColor<BACKDROP,Blend,    true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 6: draw = _master_setFinalBGColor<BACKDROP,Increase, true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 7: draw = _master_setFinalBGColor<BACKDROP,Decrease, true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		default: break;
	};

	if (BACKDROP || draw) //backdrop must always be drawn
	{
		dstLine[dstX] = src | 0x8000;
		if (!BACKDROP) bgPixelsLine[dstX] = currBgNum; //lets do this in the backdrop drawing loop, should be faster
	}
}

FORCEINLINE void GPU::setFinalColor3d(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const FragmentColor src)
{
	switch (setFinalColor3d_funcNum)
	{
		case 0x0: _master_setFinal3dColor<NoBlend, false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x1: _master_setFinal3dColor<Blend,   false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x2: _master_setFinal3dColor<Increase,false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x3: _master_setFinal3dColor<Decrease,false>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x4: _master_setFinal3dColor<NoBlend,  true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x5: _master_setFinal3dColor<Blend,    true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x6: _master_setFinal3dColor<Increase, true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
		case 0x7: _master_setFinal3dColor<Decrease, true>(srcX, dstX, dstLine, bgPixelsLine, src); break;
	};
}

FORCEINLINE void GPU::setFinalColorSpr(const size_t srcX, const size_t dstX, u16 *dstLine, u8 *bgPixelsLine, const u16 src, const u8 alpha, const u8 type)
{
	switch (setFinalColorSpr_funcNum)
	{
		case 0x0: _master_setFinalOBJColor<NoBlend, false>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x1: _master_setFinalOBJColor<Blend,   false>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x2: _master_setFinalOBJColor<Increase,false>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x3: _master_setFinalOBJColor<Decrease,false>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x4: _master_setFinalOBJColor<NoBlend,  true>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x5: _master_setFinalOBJColor<Blend,    true>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x6: _master_setFinalOBJColor<Increase, true>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
		case 0x7: _master_setFinalOBJColor<Decrease, true>(srcX, dstX, dstLine, bgPixelsLine, src, alpha, type); break;
	};
}

template<bool MOSAIC, bool BACKDROP>
FORCEINLINE void GPU::__setFinalColorBck(const u16 color, const size_t srcX, const bool opaque)
{
	return ___setFinalColorBck<MOSAIC, BACKDROP, false, 0>(color, srcX, opaque);
}

//this was forced inline because most of the time it just falls through to setFinalColorBck() and the function call
//overhead was ridiculous and terrible
template<bool MOSAIC, bool BACKDROP, bool USECUSTOMVRAM, int FUNCNUM>
FORCEINLINE void GPU::___setFinalColorBck(u16 color, const size_t srcX, const bool opaque)
{
	//due to this early out, we will get incorrect behavior in cases where 
	//we enable mosaic in the middle of a frame. this is deemed unlikely.
	if (!MOSAIC)
	{
		if (opaque)
		{
			u16 *dstLine = this->currDst;
			u8 *bgLine = this->bgPixels;
			
			for (size_t line = 0; line < _gpuDstLineCount[this->currLine]; line++)
			{
				const u16 *srcLine = (USECUSTOMVRAM) ? _gpuCustomVRAM + (this->vramBlockBGIndex * _gpuVRAMBlockOffset) + ((_gpuDstLineIndex[this->currLine] + line) * _displayInfo.customWidth) : NULL;
				
				for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
				{
					const size_t dstX = _gpuDstPitchIndex[srcX] + p;
					
					setFinalColorBG<BACKDROP,FUNCNUM>(srcX,
													  dstX,
													  dstLine,
													  bgLine,
													  (USECUSTOMVRAM) ? srcLine[dstX] : color);
				}
				
				dstLine += _displayInfo.customWidth;
				bgLine += _displayInfo.customWidth;
			}
		}
		
		return;
	}
	
	if (!opaque) color = 0xFFFF;
	else color &= 0x7FFF;
	
	if (GPU::mosaicLookup.width[srcX].begin && GPU::mosaicLookup.height[this->currLine].begin)
	{
		// Do nothing.
	}
	else
	{
		//due to the early out, enabled must always be true
		//x_int = enabled ? GPU::mosaicLookup.width[srcX].trunc : srcX;
		const size_t x_int = GPU::mosaicLookup.width[srcX].trunc;
		color = mosaicColors.bg[this->currBgNum][x_int];
	}
	
	mosaicColors.bg[this->currBgNum][srcX] = color;
	
	if (color != 0xFFFF)
	{
		u16 *dstLine = currDst;
		u8 *bgLine = bgPixels;
		
		for (size_t line = 0; line < _gpuDstLineCount[this->currLine]; line++)
		{
			const u16 *srcLine = (USECUSTOMVRAM) ? _gpuCustomVRAM + (this->vramBlockBGIndex * _gpuVRAMBlockOffset) + ((_gpuDstLineIndex[this->currLine] + line) * _displayInfo.customWidth) : NULL;
			
			for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
			{
				const size_t dstX = _gpuDstPitchIndex[srcX] + p;
				
				setFinalColorBG<BACKDROP,FUNCNUM>(srcX,
												  dstX,
												  dstLine,
												  bgLine,
												  (USECUSTOMVRAM) ? srcLine[dstX] : color);
			}
			
			dstLine += _displayInfo.customWidth;
			bgLine += _displayInfo.customWidth;
		}
	}
}

//this is fantastically inaccurate.
//we do the early return even though it reduces the resulting accuracy
//because we need the speed, and because it is inaccurate anyway
static void mosaicSpriteLinePixel(GPU *gpu, const size_t x, u16 l, u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab)
{
	const bool enableMosaic = (gpu->oamList[gpu->sprNum[x]].Mosaic != 0);
	if (!enableMosaic)
		return;

	const bool opaque = prioTab[x] <= 4;

	GPU::MosaicColor::Obj objColor;
	objColor.color = LE_TO_LOCAL_16(dst[x]);
	objColor.alpha = dst_alpha[x];
	objColor.opaque = opaque;

	const size_t x_int = (enableMosaic) ? GPU::mosaicLookup.width[x].trunc : x;

	if (enableMosaic)
	{
		const size_t y = l;
		
		if (GPU::mosaicLookup.width[x].begin && GPU::mosaicLookup.height[y].begin)
		{
			// Do nothing.
		}
		else
		{
			objColor = gpu->mosaicColors.obj[x_int];
		}
	}
	gpu->mosaicColors.obj[x] = objColor;
	
	dst[x] = LE_TO_LOCAL_16(objColor.color);
	dst_alpha[x] = objColor.alpha;
	if(!objColor.opaque) prioTab[x] = 0xFF;
}

FORCEINLINE static void mosaicSpriteLine(GPU *gpu, u16 l, u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab)
{
	//don't even try this unless the mosaic is effective
	if (gpu->mosaicLookup.widthValue != 0 || gpu->mosaicLookup.heightValue != 0)
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			mosaicSpriteLinePixel(gpu, i, l, dst, dst_alpha, typeTab, prioTab);
}

template<bool MOSAIC>
void lineLarge8bpp(GPU *gpu)
{
	if (gpu->core == GPUCOREID_SUB)
	{
		PROGINFO("Cannot use large 8bpp screen on sub core\n");
		return;
	}

	u8 num = gpu->currBgNum;
	u16 XBG = gpu->getHOFS(gpu->currBgNum);
	u16 YBG = gpu->currLine + gpu->getVOFS(gpu->currBgNum);
	u16 lg     = gpu->BGSize[num][0];
	u16 ht     = gpu->BGSize[num][1];
	u16 wmask  = (lg-1);
	u16 hmask  = (ht-1);
	YBG &= hmask;

	//TODO - handle wrapping / out of bounds correctly from rot_scale_op?

	u32 tmp_map = gpu->BG_bmp_large_ram[num] + lg * YBG;
	u8 *map = (u8 *)MMU_gpu_map(tmp_map);

	const u16 *pal = (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);

	for (size_t x = 0; x < lg; ++x, ++XBG)
	{
		XBG &= wmask;
		const u16 color = LE_TO_LOCAL_16( pal[map[XBG]] );
		gpu->__setFinalColorBck<MOSAIC,false>(color,x,(color!=0));
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
template<bool MOSAIC>
INLINE void renderline_textBG(GPU *gpu, u16 XBG, u16 YBG, u16 LG)
{
	const u8 num = gpu->currBgNum;
	struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[num].bits;
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	const u16 lg     = gpu->BGSize[num][0];
	const u16 ht     = gpu->BGSize[num][1];
	const u16 wmask  = (lg-1);
	const u16 hmask  = (ht-1);
	u16 tmp    = ((YBG & hmask) >> 3);
	u32 map;
	u32 tile;
	u16 xoff;
	u16 yoff;
	u32 xfin;

	s8 line_dir = 1;
	u32 mapinfo;
	TILEENTRY tileentry;

	u32 tmp_map = gpu->BG_map_ram[num] + (tmp&31) * 64;
	if (tmp > 31)
		tmp_map+= ADDRESS_STEP_512B << bgCnt->ScreenSize ;

	map = tmp_map;
	tile = gpu->BG_tile_ram[num];

	xoff = XBG;

	if (!bgCnt->Palette_256) // color: 16 palette entries
	{
		const u16 *pal = (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);
		
		yoff = ((YBG&7)<<2);
		xfin = 8 - (xoff&7);
		for (size_t x = 0; x < LG; xfin = std::min<u16>(x+8, LG))
		{
			u16 tilePalette = 0;
			tmp = ((xoff&wmask)>>3);
			mapinfo = map + (tmp&0x1F) * 2;
			if(tmp>31) mapinfo += 32*32*2;
			tileentry.val = LOCAL_TO_LE_16( *(u16 *)MMU_gpu_map(mapinfo) );

			tilePalette = (tileentry.bits.Palette*16);

			u8 *line = (u8 *)MMU_gpu_map(tile + (tileentry.bits.TileNum * 0x20) + ((tileentry.bits.VFlip) ? (7*4)-yoff : yoff));
			u8 offset = 0;
			
			if (tileentry.bits.HFlip)
			{
				line += ( 3 - ((xoff & 7) >> 1) );
				for (; x < xfin; line--)
				{
					if (!(xoff & 1))
					{
						offset = *line >> 4;
						const u16 color = LE_TO_LOCAL_16( pal[offset + tilePalette] );
						gpu->__setFinalColorBck<MOSAIC,false>(color, x, (offset != 0));
						x++; xoff++;
					}
					
					if (x < xfin)
					{
						offset = *line & 0xF;
						const u16 color = LE_TO_LOCAL_16( pal[offset + tilePalette] );
						gpu->__setFinalColorBck<MOSAIC,false>(color, x, (offset != 0));
						x++; xoff++;
					}
				}
			}
			else
			{
				line += ((xoff & 7) >> 1);
				for (; x < xfin; line++)
				{
					if (!(xoff & 1))
					{
						offset = *line & 0xF;
						const u16 color = LE_TO_LOCAL_16( pal[offset + tilePalette] );
						gpu->__setFinalColorBck<MOSAIC,false>(color, x, (offset != 0));
						x++; xoff++;
					}
					
					if (x < xfin)
					{
						offset = *line >> 4;
						const u16 color = LE_TO_LOCAL_16( pal[offset + tilePalette] );
						gpu->__setFinalColorBck<MOSAIC,false>(color, x, (offset != 0));
						x++; xoff++;
					}
				}
			}
		}
	}
	else //256-color BG
	{
		const u16 *pal = (dispCnt->ExBGxPalette_Enable) ? (u16 *)MMU.ExtPal[gpu->core][gpu->BGExtPalSlot[num]] : (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);
		if (pal == NULL)
		{
			return;
		}
		
		yoff = ((YBG&7)<<3);
		xfin = 8 - (xoff&7);
		const u32 extPalMask = -dispCnt->ExBGxPalette_Enable;
		
		for (size_t x = 0; x < LG; xfin = std::min<u16>(x+8, LG))
		{
			tmp = (xoff & (lg-1))>>3;
			mapinfo = map + (tmp & 31) * 2;
			if(tmp > 31) mapinfo += 32*32*2;
			tileentry.val = LOCAL_TO_LE_16( *(u16 *)MMU_gpu_map(mapinfo) );
			
			const u16 *tilePal = (u16 *)((u8 *)pal + ((tileentry.bits.Palette<<9) & extPalMask));
			u8 *line = (u8 *)MMU_gpu_map(tile + (tileentry.bits.TileNum * 0x40) + ((tileentry.bits.VFlip) ? (7*8)-yoff : yoff));
			
			if (tileentry.bits.HFlip)
			{
				line += (7 - (xoff&7));
				line_dir = -1;
			}
			else
			{
				line += (xoff&7);
				line_dir = 1;
			}
			
			while (x < xfin)
			{
				const u16 color = LE_TO_LOCAL_16(tilePal[*line]);
				gpu->__setFinalColorBck<MOSAIC,false>(color, x, (*line != 0));
				
				x++;
				xoff++;
				line += line_dir;
			}
		}
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -ROTOSCALE-
/*****************************************************************************/

template<bool MOSAIC>
FORCEINLINE void rot_tiled_8bit_entry(GPU *gpu, const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *pal, const size_t i)
{
	const u16 tileindex = *(u8*)MMU_gpu_map(map + ((auxX>>3) + (auxY>>3) * (lg>>3)));
	const u16 x = auxX & 7;
	const u16 y = auxY & 7;
	const u8 palette_entry = *(u8*)MMU_gpu_map(tile + ((tileindex<<6)+(y<<3)+x));
	const u16 color = LE_TO_LOCAL_16( pal[palette_entry] );
	
	gpu->__setFinalColorBck<MOSAIC,false>(color, i, (palette_entry != 0));
}

template<bool MOSAIC, bool extPal>
FORCEINLINE void rot_tiled_16bit_entry(GPU *gpu, const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *pal, const size_t i)
{
	TILEENTRY tileentry;
	tileentry.val = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map(map + (((auxX>>3) + (auxY>>3) * (lg>>3))<<1)) );

	const u16 x = ((tileentry.bits.HFlip) ? 7 - (auxX) : (auxX)) & 7;
	const u16 y = ((tileentry.bits.VFlip) ? 7 - (auxY) : (auxY)) & 7;
	const u8 palette_entry = *(u8*)MMU_gpu_map(tile + ((tileentry.bits.TileNum<<6)+(y<<3)+x));
	const u16 color = LE_TO_LOCAL_16( pal[(palette_entry + (extPal ? (tileentry.bits.Palette<<8) : 0))] );
	
	gpu->__setFinalColorBck<MOSAIC,false>(color, i, (palette_entry != 0));
}

template<bool MOSAIC>
FORCEINLINE void rot_256_map(GPU *gpu, const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *pal, const size_t i)
{
	const u8 palette_entry = *(u8*)MMU_gpu_map((map) + ((auxX + auxY * lg)));
	const u16 color = LE_TO_LOCAL_16( pal[palette_entry] );
	
	gpu->__setFinalColorBck<MOSAIC,false>(color, i, (palette_entry != 0));
}

template<bool MOSAIC, bool USECUSTOMVRAM>
FORCEINLINE void rot_BMP_map(GPU *gpu, const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *pal, const size_t i)
{
	const u16 color = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map((map) + ((auxX + auxY * lg) << 1)) );
	
	if (USECUSTOMVRAM)
	{
		gpu->___setFinalColorBck<MOSAIC, false, true, 0>(color, i, ((color & 0x8000) != 0));
	}
	else
	{
		gpu->__setFinalColorBck<MOSAIC, false>(color, i, ((color & 0x8000) != 0));
	}
}

typedef void (*rot_fun)(GPU *gpu, const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *pal, const size_t i);

template<rot_fun fun, bool WRAP>
FORCEINLINE void rot_scale_op(GPU *gpu, const BGxPARMS &param, const u16 LG, const s32 wh, const s32 ht, const u32 map, const u32 tile, const u16 *pal)
{
	ROTOCOORD x, y;
	x.val = param.BGxX;
	y.val = param.BGxY;

	const s32 dx = (s32)param.BGxPA;
	const s32 dy = (s32)param.BGxPC;

	// as an optimization, specially handle the fairly common case of
	// "unrotated + unscaled + no boundary checking required"
	if (dx == GPU_FRAMEBUFFER_NATIVE_WIDTH && dy == 0)
	{
		s32 auxX = (WRAP) ? x.bits.Integer & (wh-1) : x.bits.Integer;
		const s32 auxY = (WRAP) ? y.bits.Integer & (ht-1) : y.bits.Integer;
		
		if (WRAP || (auxX + LG < wh && auxX >= 0 && auxY < ht && auxY >= 0))
		{
			for (size_t i = 0; i < LG; i++)
			{
				fun(gpu, auxX, auxY, wh, map, tile, pal, i);
				auxX++;
				
				if (WRAP)
					auxX = auxX & (wh-1);
			}
			
			return;
		}
	}
	
	for (size_t i = 0; i < LG; i++, x.val += dx, y.val += dy)
	{
		const s32 auxX = (WRAP) ? x.bits.Integer & (wh-1) : x.bits.Integer;
		const s32 auxY = (WRAP) ? y.bits.Integer & (ht-1) : y.bits.Integer;
		
		if (WRAP || ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht)))
			fun(gpu, auxX, auxY, wh, map, tile, pal, i);
	}
}

template<rot_fun fun>
FORCEINLINE void apply_rot_fun(GPU *gpu, const BGxPARMS &param, const u16 LG, const u32 map, const u32 tile, const u16 *pal)
{
	struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[gpu->currBgNum].bits;
	s32 wh = gpu->BGSize[gpu->currBgNum][0];
	s32 ht = gpu->BGSize[gpu->currBgNum][1];
	
	if (bgCnt->PaletteSet_Wrap)
		rot_scale_op<fun,true>(gpu, param, LG, wh, ht, map, tile, pal);
	else
		rot_scale_op<fun,false>(gpu, param, LG, wh, ht, map, tile, pal);
}


template<bool MOSAIC>
FORCEINLINE void rotBG2(GPU *gpu, const BGxPARMS &param, const u16 LG)
{
	const size_t num = gpu->currBgNum;
	const u16 *pal = (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);
//	printf("rot mode\n");
	apply_rot_fun< rot_tiled_8bit_entry<MOSAIC> >(gpu, param, LG, gpu->BG_map_ram[num], gpu->BG_tile_ram[num], pal);
}

template<bool MOSAIC>
FORCEINLINE void extRotBG2(GPU *gpu, const BGxPARMS &param, const u16 LG)
{
	const size_t num = gpu->currBgNum;
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	
	u16 *pal = NULL;

	switch(gpu->BGTypes[num])
	{
		case BGType_AffineExt_256x16: // 16  bit bgmap entries
			pal = (dispCnt->ExBGxPalette_Enable) ? (u16 *)(MMU.ExtPal[gpu->core][gpu->BGExtPalSlot[num]]) : (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);
			if (pal == NULL) return;
			
			if(dispCnt->ExBGxPalette_Enable)
				apply_rot_fun< rot_tiled_16bit_entry<MOSAIC, true> >(gpu, param, LG, gpu->BG_map_ram[num], gpu->BG_tile_ram[num], pal);
			else
				apply_rot_fun< rot_tiled_16bit_entry<MOSAIC, false> >(gpu, param, LG, gpu->BG_map_ram[num], gpu->BG_tile_ram[num], pal);
			break;
			
		case BGType_AffineExt_256x1: // 256 colors
			pal = (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);
			apply_rot_fun< rot_256_map<MOSAIC> >(gpu, param, LG, gpu->BG_bmp_ram[num], 0, pal);
			break;
			
		case BGType_AffineExt_Direct: // direct colors / BMP
		{
			if (!_gpuIsNativeSize && gpu->isCustomRenderingNeeded && (gpu->currBgNum == gpu->vramBGLayer))
			{
				apply_rot_fun< rot_BMP_map<MOSAIC, true> >(gpu, param, LG, gpu->BG_bmp_ram[num], 0, NULL);
			}
			else
			{
				apply_rot_fun< rot_BMP_map<MOSAIC, false> >(gpu, param, LG, gpu->BG_bmp_ram[num], 0, NULL);
			}
			break;
		}
			
		case BGType_Large8bpp: // large screen 256 colors
			pal = (u16 *)(MMU.ARM9_VMEM + gpu->core * ADDRESS_STEP_1KB);
			apply_rot_fun< rot_256_map<MOSAIC> >(gpu, param, LG, gpu->BG_bmp_large_ram[num], 0, pal);
			break;
			
		default:
			break;
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

#if 0
static void lineNull(GPU *gpu)
{
}
#endif

template<bool MOSAIC>
void lineText(GPU *gpu)
{
	if(gpu->debug)
	{
		const s32 wh = gpu->BGSize[gpu->currBgNum][0];
		renderline_textBG<MOSAIC>(gpu, 0, gpu->currLine, wh);
	}
	else
	{
		const u16 vofs = gpu->getVOFS(gpu->currBgNum);
		const u16 hofs = gpu->getHOFS(gpu->currBgNum);
		renderline_textBG<MOSAIC>(gpu, hofs, gpu->currLine + vofs, 256);
	}
}

template<bool MOSAIC>
void lineRot(GPU *gpu)
{
	if (gpu->debug)
	{
		static const BGxPARMS debugParams = {256, 0, 0, -77, 0, (s16)gpu->currLine*GPU_FRAMEBUFFER_NATIVE_WIDTH};
		const s32 wh = gpu->BGSize[gpu->currBgNum][0];
		rotBG2<MOSAIC>(gpu, debugParams, wh);
	}
	else
	{
		BGxPARMS &params = (gpu->currBgNum == 2) ? (gpu->dispx_st)->dispx_BG2PARMS : (gpu->dispx_st)->dispx_BG3PARMS;
		
		rotBG2<MOSAIC>(gpu, params, 256);
		params.BGxX += params.BGxPB;
		params.BGxY += params.BGxPD;
	}
}

template<bool MOSAIC>
void lineExtRot(GPU *gpu)
{
	if (gpu->debug)
	{
		static BGxPARMS debugParams = {256, 0, 0, -77, 0, (s16)gpu->currLine*GPU_FRAMEBUFFER_NATIVE_WIDTH};
		const s32 wh = gpu->BGSize[gpu->currBgNum][0];
		extRotBG2<MOSAIC>(gpu, debugParams, wh);
	}
	else
	{
		BGxPARMS &params = (gpu->currBgNum == 2) ? (gpu->dispx_st)->dispx_BG2PARMS : (gpu->dispx_st)->dispx_BG3PARMS;
		
		extRotBG2<MOSAIC>(gpu, params, 256);
		params.BGxX += params.BGxPB;
		params.BGxY += params.BGxPD;
	}
}

/*****************************************************************************/
//			SPRITE RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
INLINE void render_sprite_BMP(GPU *gpu, const u8 spriteNum, const u16 l, u16 *dst, const u32 srcadr, u8 *dst_alpha, u8 *typeTab, u8 *prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
{
	for (size_t i = 0; i < lg; i++, ++sprX, x += xdir)
	{
		const u16 color = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map(srcadr + (x << 1)) );
		
		//a cleared alpha bit suppresses the pixel from processing entirely; it doesnt exist
		if ((color & 0x8000) && (prio < prioTab[sprX]))
		{
			dst[sprX] = color;
			dst_alpha[sprX] = alpha+1;
			typeTab[sprX] = 3;
			prioTab[sprX] = prio;
			gpu->sprNum[sprX] = spriteNum;
		}
	}
}

INLINE void render_sprite_256(GPU *gpu, const u8 spriteNum, const u16 l, u16 *dst, const u32 srcadr, const u16 *pal, u8 *dst_alpha, u8 *typeTab, u8 *prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
{
	for (size_t i = 0; i < lg; i++, ++sprX, x += xdir)
	{
		const u32 adr = srcadr + (x & 0x7) + ((x & 0xFFF8) << 3);
		const u8 *src = (u8 *)MMU_gpu_map(adr);
		const u8 palette_entry = *src;

		//a zero value suppresses the pixel from processing entirely; it doesnt exist
		if ((palette_entry > 0) && (prio < prioTab[sprX]))
		{
			const u16 color = LE_TO_LOCAL_16(pal[palette_entry]);
			dst[sprX] = color;
			dst_alpha[sprX] = -1;
			typeTab[sprX] = (alpha ? 1 : 0);
			prioTab[sprX] = prio;
			gpu->sprNum[sprX] = spriteNum;
		}
	}
}

INLINE void render_sprite_16(GPU *gpu, const u16 l, u16 *dst, const u32 srcadr, const u16 *pal, u8 *dst_alpha, u8 *typeTab, u8 *prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
{
	for (size_t i = 0; i < lg; i++, ++sprX, x += xdir)
	{
		const u16 x1 = x >> 1;
		const u32 adr = srcadr + (x1 & 0x3) + ((x1 & 0xFFFC) << 3);
		const u8 *src = (u8 *)MMU_gpu_map(adr);//
		const u8 palette = *src;
		const u8 palette_entry = (x & 1) ? palette >> 4 : palette & 0xF;
		
		//a zero value suppresses the pixel from processing entirely; it doesnt exist
		if ((palette_entry > 0) && (prio < prioTab[sprX]))
		{
			const u16 color = LE_TO_LOCAL_16(pal[palette_entry]);
			dst[sprX] = color;
			dst_alpha[sprX] = -1;
			typeTab[sprX] = (alpha ? 1 : 0);
			prioTab[sprX] = prio;
		}
	}
}

INLINE void render_sprite_Win(const u8 *src, const bool col256, const size_t lg, size_t sprX, size_t x, const s32 xdir)
{
	if (col256)
	{
		for (size_t i = 0; i < lg; i++, sprX++, x += xdir)
		{
			//sprWin[sprX] = (src[x])?1:0;
			if (src[(x & 7) + ((x & 0xFFF8) << 3)])
			{
				sprWin[sprX] = 1;
			}
		}
	}
	else
	{
		for (size_t i = 0; i < lg; i++, sprX++, x += xdir)
		{
			const s32 x1 = x >> 1;
			const u8 palette = src[(x1 & 0x3) + ((x1 & 0xFFFC) << 3)];
			const u8 palette_entry = (x & 1) ? palette >> 4 : palette & 0xF;
			
			if (palette_entry)
			{
				sprWin[sprX] = 1;
			}
		}
	}
}

// return val means if the sprite is to be drawn or not
FORCEINLINE bool compute_sprite_vars(const OAMAttributes &spriteInfo, const u16 l,
	SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir)
{
	x = 0;
	// get sprite location and size
	sprX = (spriteInfo.X/*<<23*/)/*>>23*/;
	sprY = spriteInfo.Y;
	sprSize = sprSizeTab[spriteInfo.Size][spriteInfo.Shape];

	lg = sprSize.x;
	
	if (sprY >= GPU_FRAMEBUFFER_NATIVE_HEIGHT)
		sprY = (s32)((s8)(spriteInfo.Y));
	
// FIXME: for rot/scale, a list of entries into the sprite should be maintained,
// that tells us where the first pixel of a screenline starts in the sprite,
// and how a step to the right in a screenline translates within the sprite

	//this wasn't really tested by anything. very unlikely to get triggered
	y = (l - sprY) & 0xFF;                        /* get the y line within sprite coords */
	if (y >= sprSize.y)
		return false;

	if ((sprX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || (sprX+sprSize.x <= 0))	/* sprite pixels outside of line */
		return false;				/* not to be drawn */

	// sprite portion out of the screen (LEFT)
	if (sprX < 0)
	{
		lg += sprX;	
		x = -(sprX);
		sprX = 0;
	}
	// sprite portion out of the screen (RIGHT)
	if (sprX+sprSize.x >= GPU_FRAMEBUFFER_NATIVE_WIDTH)
		lg = GPU_FRAMEBUFFER_NATIVE_WIDTH - sprX;

	// switch TOP<-->BOTTOM
	if (spriteInfo.VFlip)
		y = sprSize.y - y - 1;
	
	// switch LEFT<-->RIGHT
	if (spriteInfo.HFlip)
	{
		x = sprSize.x - x - 1;
		xdir = -1;
	}
	else
	{
		xdir = 1;
	}
	
	return true;
}

/*****************************************************************************/
//			SPRITE RENDERING
/*****************************************************************************/


//TODO - refactor this so there isnt as much duped code between rotozoomed and non-rotozoomed versions

static u32 bmp_sprite_address(GPU *gpu, const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y)
{
	if (gpu->dispCnt().OBJ_BMP_mapping)
	{
		//tested by buffy sacrifice damage blood splatters in corner
		return gpu->sprMem + (spriteInfo.TileIndex << gpu->sprBMPBoundary) + (y * sprSize.x * 2);
	}
	else
	{
		//2d mapping:
		//verified in rotozoomed mode by knights in the nightmare intro

		if (gpu->dispCnt().OBJ_BMP_2D_dim)
			//256*256, verified by heroes of mana FMV intro
			return gpu->sprMem + (((spriteInfo.TileIndex&0x3E0) * 64 + (spriteInfo.TileIndex&0x1F) * 8 + (y << 8)) << 1);
		else 
			//128*512, verified by harry potter and the order of the phoenix conversation portraits
			return gpu->sprMem + (((spriteInfo.TileIndex&0x3F0) * 64 + (spriteInfo.TileIndex&0x0F) * 8 + (y << 7)) << 1);
	}
}


template<GPU::SpriteRenderMode MODE>
void GPU::_spriteRender(u16 *dst, u8 *dst_alpha, u8 *typeTab, u8 *prioTab)
{
	u16 l = currLine;
	GPU *gpu = this;

	size_t cost = 0;

	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	u8 block = gpu->sprBoundary;

	for (size_t i = 0; i < 128; i++)
	{
		const OAMAttributes &spriteInfo = gpu->oamList[i];

		//for each sprite:
		if (cost >= 2130)
		{
			//out of sprite rendering time
			//printf("sprite overflow!\n");
			//return;		
		}

		//do we incur a cost if a sprite is disabled?? we guess so.
		cost += 2;

		SpriteSize sprSize;
		s32 sprX, sprY, x, y, lg;
		s32 xdir;
		u8 prio;
		u8 *src;
		u32 srcadr;
		
		// Check if sprite is disabled before everything
		if (spriteInfo.RotScale == 2)
			continue;

		prio = spriteInfo.Priority;

		if (spriteInfo.RotScale & 1)
		{
			s32		fieldX, fieldY, auxX, auxY, realX, realY, offset;
			u8		blockparameter;
			u16		*pal;
			s16		dx, dmx, dy, dmy;
			u16		colour;

			// Get sprite positions and size
			sprX = (spriteInfo.X << 23) >> 23;
			sprY = spriteInfo.Y;
			sprSize = sprSizeTab[spriteInfo.Size][spriteInfo.Shape];

			lg = sprSize.x;
			
			if (sprY >= GPU_FRAMEBUFFER_NATIVE_HEIGHT)
				sprY = (s32)((s8)(spriteInfo.Y));

			// Copy sprite size, to check change it if needed
			fieldX = sprSize.x;
			fieldY = sprSize.y;

			// If we are using double size mode, double our control vars
			if (spriteInfo.RotScale & 2)
			{
				fieldX <<= 1;
				fieldY <<= 1;
				lg <<= 1;
			}

			//check if the sprite is visible y-wise. unfortunately our logic for x and y is different due to our scanline based rendering
			//tested thoroughly by many large sprites in Super Robot Wars K which wrap around the screen
			y = (l - sprY) & 0xFF;
			if (y >= fieldY)
				continue;

			//check if sprite is visible x-wise.
			if ((sprX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || (sprX + fieldX <= 0))
				continue;

			cost += sprSize.x*2 + 10;

			// Get which four parameter block is assigned to this sprite
			blockparameter = (spriteInfo.RotScalIndex + (spriteInfo.HFlip<< 3) + (spriteInfo.VFlip << 4))*4;

			// Get rotation/scale parameters
			dx  = LE_TO_LOCAL_16((s16)gpu->oamList[blockparameter+0].attr3);
			dmx = LE_TO_LOCAL_16((s16)gpu->oamList[blockparameter+1].attr3);
			dy  = LE_TO_LOCAL_16((s16)gpu->oamList[blockparameter+2].attr3);
			dmy = LE_TO_LOCAL_16((s16)gpu->oamList[blockparameter+3].attr3);
			
			// Calculate fixed poitn 8.8 start offsets
			realX = ((sprSize.x) << 7) - (fieldX >> 1)*dx - (fieldY>>1)*dmx + y * dmx;
			realY = ((sprSize.y) << 7) - (fieldX >> 1)*dy - (fieldY>>1)*dmy + y * dmy;

			if (sprX < 0)
			{
				// If sprite is not in the window
				if (sprX + fieldX <= 0)
					continue;

				// Otherwise, is partially visible
				lg += sprX;
				realX -= sprX*dx;
				realY -= sprX*dy;
				sprX = 0;
			}
			else
			{
				if (sprX + fieldX > GPU_FRAMEBUFFER_NATIVE_WIDTH)
					lg = GPU_FRAMEBUFFER_NATIVE_WIDTH - sprX;
			}

			// If we are using 1 palette of 256 colours
			if (spriteInfo.Depth)
			{
				src = (u8 *)MMU_gpu_map(gpu->sprMem + (spriteInfo.TileIndex << block));

				// If extended palettes are set, use them
				if (dispCnt->ExOBJPalette_Enable)
					pal = (u16 *)(MMU.ObjExtPal[gpu->core][0]+(spriteInfo.PaletteIndex*0x200));
				else
					pal = (u16 *)(MMU.ARM9_VMEM + 0x200 + gpu->core * ADDRESS_STEP_1KB);

				for (size_t j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX >> 8);
					auxY = (realY >> 8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						if (MODE == SPRITE_2D)
							offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*8);
						else
							offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)*sprSize.x*8) + ((auxY&0x7)*8);

						colour = src[offset];

						if (colour && (prio<prioTab[sprX]))
						{ 
							dst[sprX] = pal[colour];
							dst_alpha[sprX] = -1;
							typeTab[sprX] = spriteInfo.Mode;
							prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}
			}
			// Rotozoomed direct color
			else if (spriteInfo.Mode == 3)
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;

				srcadr = bmp_sprite_address(this,spriteInfo,sprSize,0);

				for (size_t j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;

					//this is all very slow, and so much dup code with other rotozoomed modes.
					//dont bother fixing speed until this whole thing gets reworked

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						if (dispCnt->OBJ_BMP_2D_dim)
							//tested by knights in the nightmare
							offset = (bmp_sprite_address(this,spriteInfo,sprSize,auxY)-srcadr)/2+auxX;
						else //tested by lego indiana jones (somehow?)
							//tested by buffy sacrifice damage blood splatters in corner
							offset = auxX + (auxY*sprSize.x);


						u16* mem = (u16*)MMU_gpu_map(srcadr + (offset<<1));
						colour = LE_TO_LOCAL_16(*mem);
						
						if ((colour & 0x8000) && (prio < prioTab[sprX]))
						{
							dst[sprX] = colour;
							dst_alpha[sprX] = spriteInfo.PaletteIndex;
							typeTab[sprX] = spriteInfo.Mode;
							prioTab[sprX] = prio;
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}
			}
			// Rotozoomed 16/16 palette
			else
			{
				if (MODE == SPRITE_2D)
				{
					src = (u8 *)MMU_gpu_map(gpu->sprMem + (spriteInfo.TileIndex<<5));
					pal = (u16 *)(MMU.ARM9_VMEM + 0x200 + (gpu->core*ADDRESS_STEP_1KB + (spriteInfo.PaletteIndex*32)));
				}
				else
				{
					src = (u8 *)MMU_gpu_map(gpu->sprMem + (spriteInfo.TileIndex<<gpu->sprBoundary));
					pal = (u16 *)(MMU.ARM9_VMEM + 0x200 + gpu->core*ADDRESS_STEP_1KB + (spriteInfo.PaletteIndex*32));
				}

				for (size_t j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.x && auxY < sprSize.y)
					{
						if (MODE == SPRITE_2D)
							offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*4);
						else
							offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)*sprSize.x)*4 + ((auxY&0x7)*4);
						
						colour = src[offset];

						// Get 4bits value from the readed 8bits
						if (auxX&1)	colour >>= 4;
						else		colour &= 0xF;

						if (colour && (prio<prioTab[sprX]))
						{
							if (spriteInfo.Mode == 2)
							{
								sprWin[sprX] = 1;
							}
							else
							{
								dst[sprX] = LE_TO_LOCAL_16(pal[colour]);
								dst_alpha[sprX] = -1;
								typeTab[sprX] = spriteInfo.Mode;
								prioTab[sprX] = prio;
							}
						}
					}

					//  Add the rotation/scale coeficients, here the rotation/scaling
					// is performed
					realX += dx;
					realY += dy;
				}
			}
		}
		else //NOT rotozoomed
		{
			if (!compute_sprite_vars(spriteInfo, l, sprSize, sprX, sprY, x, y, lg, xdir))
				continue;

			cost += sprSize.x;

			if (spriteInfo.Mode == 2)
			{
				if (MODE == SPRITE_2D)
				{
					if (spriteInfo.Depth)
						src = (u8 *)MMU_gpu_map(gpu->sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8));
					else
						src = (u8 *)MMU_gpu_map(gpu->sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4));
				}
				else
				{
					if (spriteInfo.Depth)
						src = (u8 *)MMU_gpu_map(gpu->sprMem + (spriteInfo.TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8));
					else
						src = (u8 *)MMU_gpu_map(gpu->sprMem + (spriteInfo.TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4));
				}

				render_sprite_Win(src, (spriteInfo.Depth != 0), lg, sprX, x, xdir);
			}
			else if (spriteInfo.Mode == 3) //sprite is in BMP format
			{
				srcadr = bmp_sprite_address(this, spriteInfo, sprSize, y);

				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;
				
				render_sprite_BMP(gpu, i, l, dst, srcadr, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo.PaletteIndex);
			}
			else if (spriteInfo.Depth) //256 colors
			{
				if (MODE == SPRITE_2D)
					srcadr = gpu->sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
				else
					srcadr = gpu->sprMem + (spriteInfo.TileIndex<<block) + ((y>>3)*sprSize.x*8) + ((y&0x7)*8);
				
				const u16 *pal = (dispCnt->ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[gpu->core][0]+(spriteInfo.PaletteIndex*0x200)) : (u16 *)(MMU.ARM9_VMEM + 0x200 + gpu->core * ADDRESS_STEP_1KB);
				render_sprite_256(gpu, i, l, dst, srcadr, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo.Mode == 1);
			}
			else // 16 colors
			{
				if (MODE == SPRITE_2D)
				{
					srcadr = gpu->sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
				}
				else
				{
					srcadr = gpu->sprMem + (spriteInfo.TileIndex<<block) + ((y>>3)*sprSize.x*4) + ((y&0x7)*4);
				}
				
				const u16 *pal = (u16 *)(MMU.ARM9_VMEM + 0x200 + gpu->core * ADDRESS_STEP_1KB) + (spriteInfo.PaletteIndex << 4);
				render_sprite_16(gpu, l, dst, srcadr, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo.Mode == 1);
			}
		}
	}
}

/*****************************************************************************/
//			SCREEN FUNCTIONS
/*****************************************************************************/

int Screen_Init()
{
	MainScreen.gpu = GPU_Init(GPUCOREID_MAIN);
	SubScreen.gpu = GPU_Init(GPUCOREID_SUB);
	
	gfx3d_init();
	
	disp_fifo.head = disp_fifo.tail = 0;
	
	OSDCLASS *previousOSD = osd;
	osd = new OSDCLASS(-1);
	delete previousOSD;
	
	GPU_SetFramebufferSize(GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	
#ifdef ENABLE_COMBINATION_RENDERING
	_displayInfo.masterNativeBuffer = GPU_NativeFramebuffer;
#else
	_displayInfo.masterNativeBuffer = NULL;
#endif
	
	return 0;
}

void Screen_Reset(void)
{
	gfx3d_reset();
	GPU_Reset(MainScreen.gpu);
	GPU_Reset(SubScreen.gpu);
	
	memset(GPU_CustomFramebuffer, 0xFF, _displayInfo.customWidth * _displayInfo.customHeight * 2 * sizeof(u16));
	memset(gfx3d_colorRGBA6665, 0, _displayInfo.customWidth * _displayInfo.customHeight * sizeof(FragmentColor));
	memset(gfx3d_colorRGBA5551, 0, _displayInfo.customWidth * _displayInfo.customHeight * sizeof(u16));
	
	vram3DUsage.blockIndexDisplayVRAM = VRAM_NO_3D_USAGE;
	vram3DUsage.isBlockUsed[0] = false;
	vram3DUsage.isBlockUsed[1] = false;
	vram3DUsage.isBlockUsed[2] = false;
	vram3DUsage.isBlockUsed[3] = false;
	
	disp_fifo.head = disp_fifo.tail = 0;
	osd->clear();
}

void Screen_DeInit(void)
{
	gfx3d_deinit();
	
	GPU_DeInit(MainScreen.gpu);
	MainScreen.gpu = NULL;
	GPU_DeInit(SubScreen.gpu);
	SubScreen.gpu = NULL;

	delete osd;
	osd = NULL;
	
	free_aligned(GPU_CustomFramebuffer);
	GPU_CustomFramebuffer = NULL;
	GPU_screen = NULL;
	
	free_aligned(_gpuDstToSrcIndex);
	_gpuDstToSrcIndex = NULL;
}

size_t GPU_GetFramebufferWidth()
{
	return _displayInfo.customWidth;
}

size_t GPU_GetFramebufferHeight()
{
	return _displayInfo.customHeight;
}

void GPU_SetFramebufferSize(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return;
	}
	
	// Check if we're calling this function from initialization.
	// If we're not initializing, we need to finish rendering first.
	if (gfx3d_colorRGBA6665 != NULL && gfx3d_colorRGBA5551 != NULL)
	{
		CurrentRenderer->RenderFinish();
	}
	
	const float customWidthScale = (float)w / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const float customHeightScale = (float)h / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	const float newGpuLargestDstLineCount = (size_t)ceilf(customHeightScale);
	
	u16 *oldCustomFramebuffer = GPU_CustomFramebuffer;
	u16 *oldGpuDstToSrcIndexPtr = _gpuDstToSrcIndex;
	FragmentColor *oldColorRGBA6665Buffer = gfx3d_colorRGBA6665;
	u16 *oldColorRGBA5551Buffer = gfx3d_colorRGBA5551;
	u16 *oldMainScreenTempScanlineBuffer = MainScreen.gpu->tempScanlineBuffer;
	u16 *oldSubScreenTempScanlineBuffer = SubScreen.gpu->tempScanlineBuffer;
	u8 *oldMainScreenBGPixels = MainScreen.gpu->bgPixels;
	u8 *oldSubScreenBGPixels = SubScreen.gpu->bgPixels;
	u16 *oldCustomVRAM = _gpuCustomVRAM;
	
	for (size_t srcX = 0, currentPitchCount = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; srcX++)
	{
		const size_t pitch = (size_t)ceilf((srcX+1) * customWidthScale) - currentPitchCount;
		_gpuDstPitchCount[srcX] = pitch;
		_gpuDstPitchIndex[srcX] = currentPitchCount;
		currentPitchCount += pitch;
	}
	
	for (size_t srcY = 0, currentLineCount = 0; srcY < GPU_FRAMEBUFFER_NATIVE_HEIGHT; srcY++)
	{
		const size_t lineCount = (size_t)ceilf((srcY+1) * customHeightScale) - currentLineCount;
		_gpuDstLineCount[srcY] = lineCount;
		_gpuDstLineIndex[srcY] = currentLineCount;
		currentLineCount += lineCount;
	}
	
	for (size_t srcY = 0, currentLineCount = 0; srcY < GPU_VRAM_BLOCK_LINES + 1; srcY++)
	{
		const size_t lineCount = (size_t)ceilf((srcY+1) * customHeightScale) - currentLineCount;
		_gpuCaptureLineIndex[srcY] = currentLineCount;
		currentLineCount += lineCount;
	}
	
	u16 *newGpuDstToSrcIndex = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
	{
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t l = 0; l < _gpuDstLineCount[y]; l++)
			{
				for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
				{
					newGpuDstToSrcIndex[((_gpuDstLineIndex[y] + l) * w) + (_gpuDstPitchIndex[x] + p)] = (y * GPU_FRAMEBUFFER_NATIVE_WIDTH) + x;
				}
			}
		}
	}
	
	u16 *newCustomFramebuffer = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16) * 2);
	memset_u16(newCustomFramebuffer, 0x7FFF, w * h * 2);
	
	FragmentColor *newColorRGBA6665Buffer = (FragmentColor *)malloc_alignedCacheLine(w * h * sizeof(FragmentColor));
	u16 *newColorRGBA5551 = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	
	u16 *newMainScreenTempScanlineBuffer = (u16 *)malloc_alignedCacheLine(w * newGpuLargestDstLineCount * sizeof(u16));
	u16 *newSubScreenTempScanlineBuffer = (u16 *)malloc_alignedCacheLine(w * newGpuLargestDstLineCount * sizeof(u16));
	u8 *newMainScreenBGPixels = (u8 *)malloc_alignedCacheLine(w * newGpuLargestDstLineCount * 4 * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	u8 *newSubScreenBGPixels = (u8 *)malloc_alignedCacheLine(w * newGpuLargestDstLineCount * 4 * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	
	const size_t newCustomVRAMBlockSize = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
	const size_t newCustomVRAMBlankSize = newGpuLargestDstLineCount * w;
	u16 *newCustomVRAM = (u16 *)malloc_alignedCacheLine(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
	memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
	
	_gpuIsNativeSize = ( (w == GPU_FRAMEBUFFER_NATIVE_WIDTH) && (h == GPU_FRAMEBUFFER_NATIVE_HEIGHT) );
	_gpuLargestDstLineCount = newGpuLargestDstLineCount;
	_gpuVRAMBlockOffset = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
	_gpuDstToSrcIndex = newGpuDstToSrcIndex;
	_gpuCustomVRAM = newCustomVRAM;
	_gpuCustomVRAMBlank = newCustomVRAM + (newCustomVRAMBlockSize * 4);
	
	MainScreen.gpu->tempScanlineBuffer = newMainScreenTempScanlineBuffer;
	SubScreen.gpu->tempScanlineBuffer = newSubScreenTempScanlineBuffer;
	MainScreen.gpu->bgPixels = newMainScreenBGPixels;
	SubScreen.gpu->bgPixels = newSubScreenBGPixels;
	GPU_CustomFramebuffer = newCustomFramebuffer;
	GPU_screen = GPU_CustomFramebuffer;
	gfx3d_colorRGBA6665 = newColorRGBA6665Buffer;
	gfx3d_colorRGBA5551 = newColorRGBA5551;
	
	MainScreen.gpu->VRAMaddrCustom = newCustomVRAM + (MainScreen.gpu->vramBlock * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	SubScreen.gpu->VRAMaddrCustom = newCustomVRAM + (SubScreen.gpu->vramBlock * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	MainScreen.gpu->customBuffer = (MainScreen.gpu->targetDisplayID == NDSDisplayID_Main) ? GPU_CustomFramebuffer : GPU_CustomFramebuffer + (w * h);
	SubScreen.gpu->customBuffer = (SubScreen.gpu->targetDisplayID == NDSDisplayID_Main) ? GPU_CustomFramebuffer : GPU_CustomFramebuffer + (w * h);
	
	CurrentRenderer->SetFramebufferSize(w, h);
	
	_displayInfo.isCustomSizeRequested = ( (w != GPU_FRAMEBUFFER_NATIVE_WIDTH) || (h != GPU_FRAMEBUFFER_NATIVE_HEIGHT) );
	_displayInfo.masterCustomBuffer = GPU_CustomFramebuffer;
	_displayInfo.customWidth = w;
	_displayInfo.customHeight = h;
	_displayInfo.customBuffer[NDSDisplayID_Main] = MainDisplay.GetEngine()->customBuffer;
	_displayInfo.customBuffer[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->customBuffer;
	
	if (_displayInfo.didPerformCustomRender[NDSDisplayID_Main])
	{
		_displayInfo.renderedBuffer[NDSDisplayID_Main] = _displayInfo.customBuffer[NDSDisplayID_Main];
		_displayInfo.renderedWidth[NDSDisplayID_Main] = _displayInfo.customWidth;
		_displayInfo.renderedHeight[NDSDisplayID_Main] = _displayInfo.customHeight;
	}
	
	if (_displayInfo.didPerformCustomRender[NDSDisplayID_Touch])
	{
		_displayInfo.renderedBuffer[NDSDisplayID_Touch] = _displayInfo.customBuffer[NDSDisplayID_Touch];
		_displayInfo.renderedWidth[NDSDisplayID_Touch] = _displayInfo.customWidth;
		_displayInfo.renderedHeight[NDSDisplayID_Touch] = _displayInfo.customHeight;
	}
	
	free_aligned(oldCustomFramebuffer);
	free_aligned(oldGpuDstToSrcIndexPtr);
	free_aligned(oldColorRGBA6665Buffer);
	free_aligned(oldColorRGBA5551Buffer);
	free_aligned(oldMainScreenTempScanlineBuffer);
	free_aligned(oldSubScreenTempScanlineBuffer);
	free_aligned(oldMainScreenBGPixels);
	free_aligned(oldSubScreenBGPixels);
	free_aligned(oldCustomVRAM);
}

/*****************************************************************************/
//			GPU_RenderLine
/*****************************************************************************/

void GPU_set_DISPCAPCNT(u32 val)
{
	GPU *gpu = MainScreen.gpu;
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;

	gpu->dispCapCnt.val = val;
	gpu->dispCapCnt.EVA = std::min((u32)16, (val & 0x1F));
	gpu->dispCapCnt.EVB = std::min((u32)16, ((val >> 8) & 0x1F));
	gpu->dispCapCnt.writeBlock =  (val >> 16) & 0x03;
	gpu->dispCapCnt.writeOffset = (val >> 18) & 0x03;
	gpu->dispCapCnt.readBlock = dispCnt->VRAM_Block;
	gpu->dispCapCnt.readOffset = (dispCnt->DisplayMode == 2) ? 0 : (val >> 26) & 0x03;
	gpu->dispCapCnt.srcA = (val >> 24) & 0x01;
	gpu->dispCapCnt.srcB = (val >> 25) & 0x01;
	gpu->dispCapCnt.capSrc = (val >> 29) & 0x03;

	switch ((val >> 20) & 0x03)
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
			
		default:
			break;
	}

	/*INFO("Capture 0x%X:\n EVA=%i, EVB=%i, wBlock=%i, wOffset=%i, capX=%i, capY=%i\n rBlock=%i, rOffset=%i, srcCap=%i, dst=0x%X, src=0x%X\n srcA=%i, srcB=%i\n\n",
			val, gpu->dispCapCnt.EVA, gpu->dispCapCnt.EVB, gpu->dispCapCnt.writeBlock, gpu->dispCapCnt.writeOffset,
			gpu->dispCapCnt.capx, gpu->dispCapCnt.capy, gpu->dispCapCnt.readBlock, gpu->dispCapCnt.readOffset, 
			gpu->dispCapCnt.capSrc, gpu->dispCapCnt.dst - MMU.ARM9_LCD, gpu->dispCapCnt.src - MMU.ARM9_LCD,
			gpu->dispCapCnt.srcA, gpu->dispCapCnt.srcB);*/
}

static void GPU_RenderLine_layer(GPU *gpu, const u16 l, u16 *dstLine, const size_t dstLineWidth, const size_t dstLineCount)
{
	const size_t pixCount = dstLineWidth * dstLineCount;
	const size_t dstLineIndex = _gpuDstLineIndex[l];
	struct _DISPCNT *dispCnt = &(gpu->dispx_st)->dispx_DISPCNT.bits;
	itemsForPriority_t *item;

	gpu->currentFadeInColors = &fadeInColors[gpu->BLDY_EVY][0];
	gpu->currentFadeOutColors = &fadeOutColors[gpu->BLDY_EVY][0];
	
	const u16 backdrop_color = T1ReadWord(MMU.ARM9_VMEM, gpu->core * ADDRESS_STEP_1KB) & 0x7FFF;

	//we need to write backdrop colors in the same way as we do BG pixels in order to do correct window processing
	//this is currently eating up 2fps or so. it is a reasonable candidate for optimization. 
	gpu->currBgNum = 5;
	switch (gpu->setFinalColorBck_funcNum)
	{
		//for backdrops, blend isnt applied (it's illogical, isnt it?)
		case 0:
		case 1:
PLAIN_CLEAR:
			memset_u16(dstLine, LE_TO_LOCAL_16(backdrop_color), pixCount);
			break;

		//for backdrops, fade in and fade out can be applied if it's a 1st target screen
		case 2:
			if(gpu->BLDCNT & 0x20) //backdrop is selected for color effect
				memset_u16(dstLine, LE_TO_LOCAL_16(gpu->currentFadeInColors[backdrop_color]), pixCount);
			else goto PLAIN_CLEAR;
			break;
		case 3:
			if(gpu->BLDCNT & 0x20) //backdrop is selected for color effect
				memset_u16(dstLine, LE_TO_LOCAL_16(gpu->currentFadeOutColors[backdrop_color]), pixCount);
			else goto PLAIN_CLEAR;
			break;

		//windowed cases apparently need special treatment? why? can we not render the backdrop? how would that even work?
		case 4: for(size_t x=0;x<GPU_FRAMEBUFFER_NATIVE_WIDTH;x++) gpu->___setFinalColorBck<false,true,false,4>(backdrop_color,x,true); break;
		case 5: for(size_t x=0;x<GPU_FRAMEBUFFER_NATIVE_WIDTH;x++) gpu->___setFinalColorBck<false,true,false,5>(backdrop_color,x,true); break;
		case 6: for(size_t x=0;x<GPU_FRAMEBUFFER_NATIVE_WIDTH;x++) gpu->___setFinalColorBck<false,true,false,6>(backdrop_color,x,true); break;
		case 7: for(size_t x=0;x<GPU_FRAMEBUFFER_NATIVE_WIDTH;x++) gpu->___setFinalColorBck<false,true,false,7>(backdrop_color,x,true); break;
	}
	
	memset(gpu->bgPixels, 5, pixCount);

	// init background color & priorities
	memset(gpu->sprAlpha, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(gpu->sprType, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(gpu->sprPrio, 0xFF, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(sprWin, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	// init pixels priorities
	assert(NB_PRIORITIES == 4);
	gpu->itemsForPriority[0].nbPixelsX = 0;
	gpu->itemsForPriority[1].nbPixelsX = 0;
	gpu->itemsForPriority[2].nbPixelsX = 0;
	gpu->itemsForPriority[3].nbPixelsX = 0;

	// for all the pixels in the line
	if (gpu->LayersEnable[4]) 
	{
		//n.b. - this is clearing the sprite line buffer to the background color,
		memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(gpu->sprColor, backdrop_color);
		
		//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
		//how it interacts with this. I wish we knew why we needed this
		
		gpu->spriteRender(gpu->sprColor, gpu->sprAlpha, gpu->sprType, gpu->sprPrio);
		mosaicSpriteLine(gpu, l, gpu->sprColor, gpu->sprAlpha, gpu->sprType, gpu->sprPrio);

		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			// assign them to the good priority item
			const size_t prio = gpu->sprPrio[i];
			if (prio >= 4) continue;
			
			item = &(gpu->itemsForPriority[prio]);
			item->PixelsX[item->nbPixelsX] = i;
			item->nbPixelsX++;
		}
	}

	for (size_t j = 0; j < 8; j++)
		gpu->blend2[j] = (gpu->BLDCNT & (0x100 << j)) != 0;
	
	const bool BG_enabled = gpu->LayersEnable[0] || gpu->LayersEnable[1] || gpu->LayersEnable[2] || gpu->LayersEnable[3];

	// paint lower priorities first
	// then higher priorities on top
	for (size_t prio = NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(gpu->itemsForPriority[prio]);
		// render BGs
		if (BG_enabled)
		{
			for (size_t i = 0; i < item->nbBGs; i++)
			{
				const size_t layerNum = item->BGs[i];
				if (gpu->LayersEnable[layerNum])
				{
					gpu->currBgNum = (u8)layerNum;
					gpu->blend1 = (gpu->BLDCNT & (1 << gpu->currBgNum)) != 0;

					struct _BGxCNT *bgCnt = &(gpu->dispx_st)->dispx_BGxCNT[layerNum].bits;
					gpu->curr_mosaic_enabled = bgCnt->Mosaic_Enable;

					if (gpu->core == GPUCOREID_MAIN && layerNum == 0 && dispCnt->BG0_3D)
					{
						const float customWidthScale = (float)_displayInfo.customWidth / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
						const FragmentColor *srcLine = gfx3d_GetLineDataRGBA6665(dstLineIndex);
						const u16 hofs = (u16)( ((float)gpu->getHOFS(layerNum) * customWidthScale) + 0.5f );
						u16 *render3DdstLine = dstLine;
						u8 *render3DbgLine = gpu->bgPixels;
						
						gpu->currBgNum = 0;
						
						for (size_t line = 0; line < dstLineCount; line++)
						{
							for (size_t dstX = 0; dstX < dstLineWidth; dstX++)
							{
								size_t srcX = dstX + hofs;
								if (srcX >= dstLineWidth * 2)
								{
									srcX -= dstLineWidth * 2;
								}
								
								if (srcX >= dstLineWidth || srcLine[srcX].a == 0)
									continue;
								
								gpu->setFinalColor3d(_gpuDstToSrcIndex[dstX],
													 dstX,
													 render3DdstLine,
													 render3DbgLine,
													 srcLine[srcX]);
							}
							
							srcLine += dstLineWidth;
							render3DdstLine += dstLineWidth;
							render3DbgLine += dstLineWidth;
						}
						
						continue;
					}

					//useful for debugging individual layers
					//if(gpu->core == GPUCOREID_SUB || layerNum != 2) continue;

#ifndef DISABLE_MOSAIC
					if (gpu->curr_mosaic_enabled)
						gpu->modeRender<true>(layerNum);
					else 
#endif
						gpu->modeRender<false>(layerNum);
				} //layer enabled
			}
		}

		// render sprite Pixels
		if (gpu->LayersEnable[4])
		{
			gpu->currBgNum = 4;
			gpu->blend1 = (gpu->BLDCNT & (1 << gpu->currBgNum)) != 0;
			
			if (_gpuIsNativeSize)
			{
				for (size_t i = 0; i < item->nbPixelsX; i++)
				{
					const size_t srcX = item->PixelsX[i];
					
					gpu->setFinalColorSpr(srcX,
										  srcX,
										  gpu->currDst,
										  gpu->bgPixels,
										  gpu->sprColor[srcX],
										  gpu->sprAlpha[srcX],
										  gpu->sprType[srcX]);
				}
			}
			else
			{
				u16 *sprDstLine = gpu->currDst;
				u8 *sprBgLine = gpu->bgPixels;
				
				const bool useCustomVRAM = gpu->isCustomRenderingNeeded && (gpu->vramBlockOBJIndex != VRAM_NO_3D_USAGE);
				const u16 *srcLine = (useCustomVRAM) ? _gpuCustomVRAM + (gpu->vramBlockOBJIndex * _gpuVRAMBlockOffset) + (dstLineIndex * dstLineWidth) : NULL;
				
				for (size_t line = 0; line < dstLineCount; line++)
				{
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = _gpuDstPitchIndex[srcX] + p;
							
							gpu->setFinalColorSpr(srcX,
												  dstX,
												  sprDstLine,
												  sprBgLine,
												  (useCustomVRAM) ? srcLine[dstX] : gpu->sprColor[srcX],
												  gpu->sprAlpha[srcX],
												  gpu->sprType[srcX]);
						}
					}
					
					srcLine += dstLineWidth;
					sprDstLine += dstLineWidth;
					sprBgLine += dstLineWidth;
				}
			}
		}
	}
}

static void GPU_RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer)
{
#ifdef ENABLE_SSE2
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
	{
		__m128i fifoColor = _mm_set_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
		_mm_store_si128((__m128i *)fifoLineBuffer + i, _mm_shuffle_epi32(fifoColor, 0x1B)); // We need to shuffle the four FIFO values back into the correct order, since they were originally loaded in reverse order.
	}
#else
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
	{
		((u32 *)fifoLineBuffer)[i] = LE_TO_LOCAL_32( DISP_FIFOrecv() );
	}
#endif
}

template<int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
static void GPU_RenderLine_DispCapture_Copy(const u16 *__restrict src, u16 *__restrict dst, const size_t captureLengthExt, const size_t captureLineCount)
{
	const u16 alphaBit = (SOURCESWITCH == 0) ? 0x8000 : 0x0000;
	
#ifdef ENABLE_SSE2
	const __m128i alpha_vec128 = _mm_set1_epi16(alphaBit);
#endif
	
	if (CAPTURETONATIVEDST)
	{
		if (CAPTUREFROMNATIVESRC)
		{
#ifdef ENABLE_SSE2
			MACRODO_N(CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_storeu_si128((__m128i *)dst + X, _mm_or_si128( _mm_loadu_si128( (__m128i *)src + X), alpha_vec128 ) ));
#else
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				dst[i] = LE_TO_LOCAL_16(src[i]) | alphaBit;
			}
#endif
		}
		else
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				dst[i] = LE_TO_LOCAL_16(src[_gpuDstPitchIndex[i]]) | alphaBit;
			}
		}
	}
	else
	{
		if (CAPTUREFROMNATIVESRC)
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				for (size_t p = 0; p < _gpuDstPitchCount[i]; p++)
				{
					dst[_gpuDstPitchIndex[i] + p] = LE_TO_LOCAL_16(src[i]) | alphaBit;
				}
			}
			
			for (size_t line = 1; line < captureLineCount; line++)
			{
				memcpy(dst + (line * _displayInfo.customWidth), dst, captureLengthExt * sizeof(u16));
			}
		}
		else
		{
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				const size_t pixCountExt = captureLengthExt * captureLineCount;
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				const size_t ssePixCount = pixCountExt - (pixCountExt % 8);
				for (; i < ssePixCount; i += 8)
				{
					_mm_store_si128((__m128i *)(dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)(src + i)), alpha_vec128 ) );
				}
#endif
				for (; i < pixCountExt; i++)
				{
					dst[i] = LE_TO_LOCAL_16(src[i]) | alphaBit;
				}
			}
			else
			{
				for (size_t line = 0; line < captureLineCount; line++)
				{
					size_t i = 0;
#ifdef ENABLE_SSE2
					const size_t ssePixCount = captureLengthExt - (captureLengthExt % 8);
					for (; i < ssePixCount; i += 8)
					{
						_mm_store_si128((__m128i *)(dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)(src + i)), alpha_vec128 ) );
					}
#endif
					for (; i < captureLengthExt; i++)
					{
						dst[i] = LE_TO_LOCAL_16(src[i]) | alphaBit;
					}
					
					src += _displayInfo.customWidth;
					dst += _displayInfo.customWidth;
				}
			}
		}
	}
}

static u16 GPU_RenderLine_DispCapture_BlendFunc(const u16 srcA, const u16 srcB, const u8 blendEVA, const u8 blendEVB)
{
	u16 a = 0;
	u16 r = 0;
	u16 g = 0;
	u16 b = 0;
	u16 a_alpha = srcA & 0x8000;
	u16 b_alpha = srcB & 0x8000;
	
	if (a_alpha)
	{
		a = 0x8000;
		r =  ((srcA        & 0x1F) * blendEVA);
		g = (((srcA >>  5) & 0x1F) * blendEVA);
		b = (((srcA >> 10) & 0x1F) * blendEVA);
	}
	
	if (b_alpha)
	{
		a = 0x8000;
		r +=  ((srcB        & 0x1F) * blendEVB);
		g += (((srcB >>  5) & 0x1F) * blendEVB);
		b += (((srcB >> 10) & 0x1F) * blendEVB);
	}
	
	r >>= 4;
	g >>= 4;
	b >>= 4;
	
	//freedom wings sky will overflow while doing some fsaa/motionblur effect without this
	r = std::min((u16)31,r);
	g = std::min((u16)31,g);
	b = std::min((u16)31,b);
	
	return (a | (b << 10) | (g << 5) | r);
}

#ifdef ENABLE_SSE2
static __m128i GPU_RenderLine_DispCapture_BlendFunc_SSE2(__m128i &srcA, __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB)
{
	const __m128i colorBitMask = _mm_set1_epi16(0x001F);
	const __m128i srcA_alpha = _mm_and_si128(srcA, _mm_set1_epi16(0x8000));
	const __m128i srcB_alpha = _mm_and_si128(srcB, _mm_set1_epi16(0x8000));
	
	srcA = _mm_andnot_si128( _mm_cmpeq_epi16(srcA_alpha, _mm_setzero_si128()), srcA );
	srcB = _mm_andnot_si128( _mm_cmpeq_epi16(srcB_alpha, _mm_setzero_si128()), srcB );
	
	__m128i srcB_r = _mm_and_si128(srcB, colorBitMask);
	srcB_r = _mm_mullo_epi16(srcB_r, blendEVB);
	
	__m128i srcB_g = _mm_srli_epi16(srcB, 5);
	srcB_g = _mm_and_si128(srcB_g, colorBitMask);
	srcB_g = _mm_mullo_epi16(srcB_g, blendEVB);
	
	__m128i srcB_b = _mm_srli_epi16(srcB, 10);
	srcB_b = _mm_and_si128(srcB_b, colorBitMask);
	srcB_b = _mm_mullo_epi16(srcB_b, blendEVB);
	
	__m128i r = _mm_and_si128(srcA, colorBitMask);
	r = _mm_mullo_epi16(r, blendEVA);
	r = _mm_add_epi16(r, srcB_r);
	r = _mm_srli_epi16(r, 4);
	r = _mm_min_epi16(r, colorBitMask);
	
	__m128i g = _mm_srli_epi16(srcA, 5);
	g = _mm_and_si128(g, colorBitMask);
	g = _mm_mullo_epi16(g, blendEVA);
	g = _mm_add_epi16(g, srcB_g);
	g = _mm_srli_epi16(g, 4);
	g = _mm_min_epi16(g, colorBitMask);
	g = _mm_slli_epi16(g, 5);
	
	__m128i b = _mm_srli_epi16(srcA, 10);
	b = _mm_and_si128(b, colorBitMask);
	b = _mm_mullo_epi16(b, blendEVA);
	b = _mm_add_epi16(b, srcB_b);
	b = _mm_srli_epi16(b, 4);
	b = _mm_min_epi16(b, colorBitMask);
	b = _mm_slli_epi16(b, 10);
	
	const __m128i a = _mm_or_si128(srcA_alpha, srcB_alpha);
	
	return _mm_or_si128(_mm_or_si128(_mm_or_si128(r, g), b), a);
}
#endif

template<bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB>
static void GPU_RenderLine_DispCapture_BlendToCustomDstBuffer(const u16 *__restrict srcA, const u16 *__restrict srcB, u16 *__restrict dst, const u8 blendEVA, const u8 blendEVB, const size_t length, size_t l)
{
#ifdef ENABLE_SSE2
	const __m128i blendEVA_vec128 = _mm_set1_epi16(blendEVA);
	const __m128i blendEVB_vec128 = _mm_set1_epi16(blendEVB);
#endif
	
	size_t offset = _gpuDstToSrcIndex[_gpuDstLineIndex[l] * _displayInfo.customWidth] - (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	
	const size_t ssePixCount = length - (length % 8);
	for (; i < ssePixCount; i += 8)
	{
		__m128i srcA_vec128 = (!CAPTUREFROMNATIVESRCA) ? _mm_load_si128((__m128i *)(srcA + i)) : _mm_set_epi16(srcA[offset + i + 7],
																											   srcA[offset + i + 6],
																											   srcA[offset + i + 5],
																											   srcA[offset + i + 4],
																											   srcA[offset + i + 3],
																											   srcA[offset + i + 2],
																											   srcA[offset + i + 1],
																											   srcA[offset + i + 0]);
		
		__m128i srcB_vec128 = (!CAPTUREFROMNATIVESRCB) ? _mm_load_si128((__m128i *)(srcB + i)) : _mm_set_epi16(srcB[offset + i + 7],
																											   srcB[offset + i + 6],
																											   srcB[offset + i + 5],
																											   srcB[offset + i + 4],
																											   srcB[offset + i + 3],
																											   srcB[offset + i + 2],
																											   srcB[offset + i + 1],
																											   srcB[offset + i + 0]);
		
		_mm_store_si128( (__m128i *)(dst + i), GPU_RenderLine_DispCapture_BlendFunc_SSE2(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
	}
#endif
	for (; i < length; i++)
	{
		const u16 colorA = (!CAPTUREFROMNATIVESRCA) ? srcA[i] : srcA[offset + i];
		const u16 colorB = (!CAPTUREFROMNATIVESRCB) ? srcB[i] : srcB[offset + i];
		
		dst[i] = GPU_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
	}
}

template<size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB, bool CAPTURETONATIVEDST>
static void GPU_RenderLine_DispCapture_Blend(const u16 *__restrict srcA, const u16 *__restrict srcB, u16 *__restrict dst, const size_t captureLengthExt, const size_t l)
{
	const u8 blendEVA = MainScreen.gpu->dispCapCnt.EVA;
	const u8 blendEVB = MainScreen.gpu->dispCapCnt.EVB;
	
	if (CAPTURETONATIVEDST)
	{
#ifdef ENABLE_SSE2
		const __m128i blendEVA_vec128 = _mm_set1_epi16(blendEVA);
		const __m128i blendEVB_vec128 = _mm_set1_epi16(blendEVB);
		
		for (size_t i = 0; i < CAPTURELENGTH; i += 8)
		{
			__m128i srcA_vec128 = (CAPTUREFROMNATIVESRCA) ? _mm_load_si128((__m128i *)(srcA + i)) : _mm_set_epi16(srcA[_gpuDstPitchIndex[i+7]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+6]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+5]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+4]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+3]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+2]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+1]],
			                                                                                                      srcA[_gpuDstPitchIndex[i+0]]);
			
			__m128i srcB_vec128 = (CAPTUREFROMNATIVESRCB) ? _mm_loadu_si128((__m128i *)(srcB + i)) : _mm_set_epi16(srcB[_gpuDstPitchIndex[i+7]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+6]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+5]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+4]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+3]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+2]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+1]],
			                                                                                                       srcB[_gpuDstPitchIndex[i+0]]);
			
			_mm_storeu_si128( (__m128i *)(dst + i), GPU_RenderLine_DispCapture_BlendFunc_SSE2(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
		}
#else
		for (size_t i = 0; i < CAPTURELENGTH; i++)
		{
			const u16 colorA = (CAPTUREFROMNATIVESRCA) ? srcA[i] : srcA[_gpuDstPitchIndex[i]];
			const u16 colorB = (CAPTUREFROMNATIVESRCB) ? srcB[i] : srcB[_gpuDstPitchIndex[i]];
			
			dst[i] = GPU_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
		}
#endif
	}
	else
	{
		const size_t captureLineCount = _gpuDstLineCount[l];
		
		if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			GPU_RenderLine_DispCapture_BlendToCustomDstBuffer<CAPTUREFROMNATIVESRCA, CAPTUREFROMNATIVESRCB>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt * captureLineCount, l);
		}
		else
		{
			for (size_t line = 0; line < captureLineCount; line++)
			{
				GPU_RenderLine_DispCapture_BlendToCustomDstBuffer<CAPTUREFROMNATIVESRCA, CAPTUREFROMNATIVESRCB>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt, l);
				srcA += _displayInfo.customWidth;
				srcB += _displayInfo.customWidth;
				dst += _displayInfo.customWidth;
			}
		}
	}
}

template<size_t CAPTURELENGTH>
static void GPU_RenderLine_DispCapture(const u16 l)
{
	assert( (CAPTURELENGTH == 0) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH/2) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) );
	
	GPU *gpu = MainScreen.gpu;
	
	if (l == 0)
	{
		if (gpu->dispCapCnt.val & 0x80000000)
		{
			gpu->dispCapCnt.enabled = TRUE;
			T1WriteLong(MMU.ARM9_REG, 0x64, gpu->dispCapCnt.val);
		}
	}
	
	if (!gpu->dispCapCnt.enabled)
	{
		return;
	}
	
	if (CAPTURELENGTH != 0)
	{
		const u8 vramWriteBlock = gpu->dispCapCnt.writeBlock;
		const u8 vramReadBlock = gpu->dispCapCnt.readBlock;
		
		//128-wide captures should write linearly into memory, with no gaps
		//this is tested by hotel dusk
		u32 cap_dst_adr = ( (gpu->dispCapCnt.writeOffset * 64 * GPU_FRAMEBUFFER_NATIVE_WIDTH) + (l * CAPTURELENGTH) ) * sizeof(u16);
		
		//Read/Write block wrap to 00000h when exceeding 1FFFFh (128k)
		//this has not been tested yet (I thought I needed it for hotel dusk, but it was fixed by the above)
		cap_dst_adr &= 0x1FFFF;
		cap_dst_adr += vramWriteBlock * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16);
		
		// TODO: Make MMU.blank_memory and MMU.ARM9_LCD 16-byte aligned so that we can use aligned load/store for better performance.
		const u16 *cap_src = (u16 *)MMU.blank_memory;
		u16 *cap_dst = (u16 *)(MMU.ARM9_LCD + cap_dst_adr);
		
		if (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC)
		{
			u32 cap_src_adr = ( (gpu->dispCapCnt.readOffset * 64 * GPU_FRAMEBUFFER_NATIVE_WIDTH) + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) ) * sizeof(u16);
			cap_src_adr &= 0x1FFFF;
			cap_src_adr += vramReadBlock * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16);
			cap_src = (u16 *)(MMU.ARM9_LCD + cap_src_adr);
		}
		
		static CACHE_ALIGN u16 fifoLine[GPU_FRAMEBUFFER_NATIVE_WIDTH];
		const u16 *srcA = (gpu->dispCapCnt.srcA == 0) ? gpu->currDst : gfx3d_GetLineDataRGBA5551(_gpuDstLineIndex[l]);
		const u16 *srcB = (gpu->dispCapCnt.srcB == 0) ? cap_src : fifoLine;
		
		switch (gpu->dispCapCnt.capSrc)
		{
			case 0: // Capture source is SourceA
			{
				//INFO("Capture source is SourceA\n");
				switch (gpu->dispCapCnt.srcA)
				{
					case 0: // Capture screen (BG + OBJ + 3D)
					{
						//INFO("Capture screen (BG + OBJ + 3D)\n");
						if (_gpuIsNativeSize)
						{
							GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, true>(srcA, cap_dst, CAPTURELENGTH, 1);
						}
						else
						{
							GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, true>(srcA, cap_dst, CAPTURELENGTH, 1);
						}
						break;
					}
						
					case 1: // Capture 3D
					{
						//INFO("Capture 3D\n");
						if (_gpuIsNativeSize)
						{
							GPU_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, true>(srcA, cap_dst, CAPTURELENGTH, 1);
						}
						else
						{
							GPU_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, false, true>(srcA, cap_dst, CAPTURELENGTH, 1);
						}
						break;
					}
				}
				
				vram3DUsage.isBlockUsed[vramWriteBlock] = true;
				break;
			}
				
			case 1: // Capture source is SourceB
			{
				//INFO("Capture source is SourceB\n");
				switch (gpu->dispCapCnt.srcB)
				{
					case 0: // Capture VRAM
					{
						if (_gpuIsNativeSize || !vram3DUsage.isBlockUsed[vramReadBlock])
						{
							GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, true>(srcB, cap_dst, CAPTURELENGTH, 1);
						}
						else
						{
							GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, true>(srcB, cap_dst, CAPTURELENGTH, 1);
						}
						
						vram3DUsage.isBlockUsed[vramWriteBlock] = vram3DUsage.isBlockUsed[vramReadBlock];
						break;
					}
						
					case 1: // Capture dispfifo (not yet tested)
					{
						GPU_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
						GPU_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, true>(srcB, cap_dst, CAPTURELENGTH, 1);
						vram3DUsage.isBlockUsed[vramWriteBlock] = false;
						break;
					}
				}
				
				break;
			}
				
			default: // Capture source is SourceA+B blended
			{
				//INFO("Capture source is SourceA+B blended\n");
				
				if (gpu->dispCapCnt.srcB == 1)
				{
					// fifo - tested by splinter cell chaos theory thermal view
					GPU_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
					
					if (_gpuIsNativeSize)
					{
						GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
				}
				else
				{
					if (_gpuIsNativeSize)
					{
						if (vram3DUsage.isBlockUsed[vramReadBlock])
						{
							GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, false, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
						}
						else
						{
							GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
						}
					}
					else
					{
						if (vram3DUsage.isBlockUsed[vramReadBlock])
						{
							GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, false, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
						}
						else
						{
							GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
						}
					}
				}
				
				vram3DUsage.isBlockUsed[vramWriteBlock] = true;
				break;
			}
		}
		
		if (!_gpuIsNativeSize)
		{
			const size_t captureLengthExt = (CAPTURELENGTH) ? _displayInfo.customWidth : _displayInfo.customWidth / 2;
			const size_t captureLineCount = _gpuDstLineCount[l];
			const size_t vramBlockOffsetExt = _gpuVRAMBlockOffset;
			const u32 ofsmulExt = (CAPTURELENGTH) ? _displayInfo.customWidth : _displayInfo.customWidth / 2;
			
			size_t cap_dst_adr_ext = (gpu->dispCapCnt.writeOffset * _gpuCaptureLineIndex[64] * _displayInfo.customWidth) + (_gpuCaptureLineIndex[l] * ofsmulExt);
			
			while (cap_dst_adr_ext >= vramBlockOffsetExt)
			{
				cap_dst_adr_ext -= vramBlockOffsetExt;
			}
			
			cap_dst_adr_ext += vramWriteBlock * vramBlockOffsetExt;
			
			const u16 *cap_src_ext = _gpuCustomVRAMBlank;
			u16 *cap_dst_ext = _gpuCustomVRAM + cap_dst_adr_ext;
			
			if (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC)
			{
				size_t cap_src_adr_ext = (gpu->dispCapCnt.readOffset * _gpuCaptureLineIndex[64] * _displayInfo.customWidth) + (_gpuCaptureLineIndex[l] * _displayInfo.customWidth);
				
				while (cap_src_adr_ext >= vramBlockOffsetExt)
				{
					cap_src_adr_ext -= vramBlockOffsetExt;
				}
				
				cap_src_adr_ext += vramReadBlock * vramBlockOffsetExt;
				cap_src_ext = _gpuCustomVRAM + cap_src_adr_ext;
			}
			
			srcB = (gpu->dispCapCnt.srcB == 0) ? cap_src_ext : fifoLine;
			
			switch (gpu->dispCapCnt.capSrc)
			{
				case 0: // Capture source is SourceA
				{
					switch (gpu->dispCapCnt.srcA)
					{
						case 0: // Capture screen (BG + OBJ + 3D)
							GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
							break;
							
						case 1: // Capture 3D
							GPU_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, false, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
							break;
					}
					break;
				}
					
				case 1: // Capture source is SourceB
				{
					switch (gpu->dispCapCnt.srcB)
					{
						case 0: // Capture VRAM
						{
							if (vram3DUsage.isBlockUsed[vramReadBlock])
							{
								GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
							}
							else
							{
								GPU_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
							}
							break;
						}
							
						case 1: // Capture dispfifo (not yet tested)
							GPU_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
							break;
					}
					
					break;
				}
					
				default: // Capture source is SourceA+B blended
				{
					if (vram3DUsage.isBlockUsed[vramReadBlock])
					{
						GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, false, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
					}
					else
					{
						GPU_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
					}
					
					break;
				}
			}
		}
	}
	
	if (l >= 191)
	{
		gpu->dispCapCnt.enabled = FALSE;
		gpu->dispCapCnt.val &= 0x7FFFFFFF;
		T1WriteLong(MMU.ARM9_REG, 0x64, gpu->dispCapCnt.val);
	}
}

static INLINE void GPU_RenderLine_MasterBrightness(const GPUMasterBrightMode mode, const u32 factor, u16 *dstLine, const size_t dstLineWidth, const size_t dstLineCount)
{
	//isn't it odd that we can set uselessly high factors here?
	//factors above 16 change nothing. curious.
	if (factor == 0) return;
	
	//Apply final brightness adjust (MASTER_BRIGHT)
	//http://nocash.emubase.de/gbatek.htm#dsvideo (Under MASTER_BRIGHTNESS)
	
	const size_t pixCount = dstLineWidth * dstLineCount;
	
	switch (mode)
	{
		case GPUMasterBrightMode_Disable:
			break;
			
		case GPUMasterBrightMode_Up:
		{
			if (factor < 16)
			{
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				const size_t ssePixCount = pixCount - (pixCount % 8);
				for (; i < ssePixCount; i += 8)
				{
					__m128i dstColor_vec128 = _mm_load_si128((__m128i *)(dstLine + i));
					dstColor_vec128 = _mm_and_si128(dstColor_vec128, _mm_set1_epi16(0x7FFF));
					
					dstLine[i+7] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 7) ];
					dstLine[i+6] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 6) ];
					dstLine[i+5] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 5) ];
					dstLine[i+4] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 4) ];
					dstLine[i+3] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 3) ];
					dstLine[i+2] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 2) ];
					dstLine[i+1] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 1) ];
					dstLine[i+0] = fadeInColors[factor][ _mm_extract_epi16(dstColor_vec128, 0) ];
				}
#endif
				for (; i < pixCount; i++)
				{
					dstLine[i] = fadeInColors[factor][ dstLine[i] & 0x7FFF ];
				}
			}
			else
			{
				// all white (optimization)
				memset_u16(dstLine, 0x7FFF, pixCount);
			}
			break;
		}
			
		case GPUMasterBrightMode_Down:
		{
			if (factor < 16)
			{
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				const size_t ssePixCount = pixCount - (pixCount % 8);
				for (; i < ssePixCount; i += 8)
				{
					__m128i dstColor_vec128 = _mm_load_si128((__m128i *)(dstLine + i));
					dstColor_vec128 = _mm_and_si128(dstColor_vec128, _mm_set1_epi16(0x7FFF));
					
					dstLine[i+7] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 7) ];
					dstLine[i+6] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 6) ];
					dstLine[i+5] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 5) ];
					dstLine[i+4] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 4) ];
					dstLine[i+3] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 3) ];
					dstLine[i+2] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 2) ];
					dstLine[i+1] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 1) ];
					dstLine[i+0] = fadeOutColors[factor][ _mm_extract_epi16(dstColor_vec128, 0) ];
				}
#endif
				for (; i < pixCount; i++)
				{
					dstLine[i] = fadeOutColors[factor][ dstLine[i] & 0x7FFF ];
				}
			}
			else
			{
				// all black (optimization)
				memset(dstLine, 0, pixCount * sizeof(u16));
			}
			break;
		}
			
		case GPUMasterBrightMode_Reserved:
			break;
	}
}

template<size_t WIN_NUM>
FORCEINLINE void GPU::setup_windows()
{
	const u8 y = currLine;
	const u16 startY = (WIN_NUM == 0) ? WIN0V0 : WIN1V0;
	const u16 endY   = (WIN_NUM == 0) ? WIN0V1 : WIN1V1;
	
	if(WIN_NUM == 0 && !WIN0_ENABLED) goto allout;
	if(WIN_NUM == 1 && !WIN1_ENABLED) goto allout;

	if (startY > endY)
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

template<size_t WIN_NUM>
void GPU::update_winh()
{
	//dont even waste any time in here if the window isnt enabled
	if (WIN_NUM == 0 && !WIN0_ENABLED) return;
	if (WIN_NUM == 1 && !WIN1_ENABLED) return;

	need_update_winh[WIN_NUM] = false;
	const size_t startX = (WIN_NUM == 0) ? WIN0H0 : WIN1H0;
	const size_t endX   = (WIN_NUM == 0) ? WIN0H1 : WIN1H1;

	//the original logic: if you doubt the window code, please check it against the newer implementation below
	//if(startX > endX)
	//{
	//	if((x < startX) && (x > endX)) return false;
	//}
	//else
	//{
	//	if((x < startX) || (x >= endX)) return false;
	//}

	if (startX > endX)
	{
		for (size_t i = 0; i <= endX; i++)
			h_win[WIN_NUM][i] = 1;
		for (size_t i = endX + 1; i < startX; i++)
			h_win[WIN_NUM][i] = 0;
		for (size_t i = startX; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			h_win[WIN_NUM][i] = 1;
	}
	else
	{
		for (size_t i = 0; i < startX; i++)
			h_win[WIN_NUM][i] = 0;
		for (size_t i = startX; i < endX; i++)
			h_win[WIN_NUM][i] = 1;
		for (size_t i = endX; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			h_win[WIN_NUM][i] = 0;
	}
}

void GPU_RenderLine(NDS_Screen *screen, const u16 l, bool skip)
{
	GPU *gpu = screen->gpu;
	
	//here is some setup which is only done on line 0
	if (l == 0)
	{
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
		
		if (gpu->core == GPUCOREID_MAIN)
		{
			GPU_UpdateVRAM3DUsageProperties(vram3DUsage);
		}
	}
	
	if (skip)
	{
		gpu->currLine = l;
		if (gpu->core == GPUCOREID_MAIN)
		{
			GPU_RenderLine_DispCapture<0>(l);
			if (l == 191) { disp_fifo.head = disp_fifo.tail = 0; }
		}
		return;
	}
	
	const size_t dstLineWidth = _displayInfo.customWidth;
	const size_t dstLineCount = _gpuDstLineCount[l];
	const size_t dstLineIndex = _gpuDstLineIndex[l];
	u16 *dstLine = gpu->customBuffer + (dstLineIndex * dstLineWidth);
	
	//blacken the screen if it is turned off by the user
	if (!CommonSettings.showGpu.screens[gpu->core])
	{
		memset(dstLine, 0, dstLineWidth * dstLineCount * sizeof(u16));
		return;
	}

	// skip some work if master brightness makes the screen completely white or completely black
	if (gpu->MasterBrightFactor >= 16 && (gpu->MasterBrightMode == GPUMasterBrightMode_Up || gpu->MasterBrightMode == GPUMasterBrightMode_Down))
	{
		// except if it could cause any side effects (for example if we're capturing), then don't skip anything
		if (!(gpu->core == GPUCOREID_MAIN && (gpu->dispCapCnt.enabled || l == 0 || l == 191)))
		{
			gpu->currLine = l;
			GPU_RenderLine_MasterBrightness(gpu->MasterBrightMode, gpu->MasterBrightFactor, dstLine, dstLineWidth, dstLineCount);
			return;
		}
	}

	//cache some parameters which are assumed to be stable throughout the rendering of the entire line
	gpu->currLine = l;
	const u16 mosaic_control = LE_TO_LOCAL_16(gpu->dispx_st->dispx_MISC.MOSAIC);
	const u16 mosaic_width = (mosaic_control & 0xF);
	const u16 mosaic_height = ((mosaic_control >> 4) & 0xF);

	//mosaic test hacks
	//mosaic_width = mosaic_height = 3;

	GPU::mosaicLookup.widthValue = mosaic_width;
	GPU::mosaicLookup.heightValue = mosaic_height;
	GPU::mosaicLookup.width = &GPU::mosaicLookup.table[mosaic_width][0];
	GPU::mosaicLookup.height = &GPU::mosaicLookup.table[mosaic_height][0];

	if(gpu->need_update_winh[0]) gpu->update_winh<0>();
	if(gpu->need_update_winh[1]) gpu->update_winh<1>();

	gpu->setup_windows<0>();
	gpu->setup_windows<1>();

	//generate the 2d engine output
	if (gpu->dispMode == GPUDisplayMode_Normal)
	{
		//optimization: render straight to the output buffer when thats what we are going to end up displaying anyway
		gpu->currDst = dstLine;
	}
	else
	{
		//otherwise, we need to go to a temp buffer
		gpu->currDst = gpu->tempScanlineBuffer;
	}

	GPU_RenderLine_layer(gpu, l, gpu->currDst, dstLineWidth, dstLineCount);

	switch (gpu->dispMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			memset_u16(dstLine, 0x7FFF, dstLineWidth * dstLineCount);
			break;
			
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			//do nothing: it has already been generated into the right place
			break;
			
		case GPUDisplayMode_VRAM: // Display vram framebuffer
		{
			if (_gpuIsNativeSize)
			{
				const u16 *src = gpu->VRAMaddrNative + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
#ifdef LOCAL_LE
				memcpy(dstLine, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
#else
				for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
				{
					dstLine[x] = LE_TO_LOCAL_16(src[x]);
				}
#endif
			}
			else
			{
				if (vram3DUsage.isBlockUsed[MainScreen.gpu->vramBlock] && (vram3DUsage.blockIndexDisplayVRAM == MainScreen.gpu->vramBlock))
				{
					const u16 *src = gpu->VRAMaddrCustom + (dstLineIndex * dstLineWidth);
#ifdef LOCAL_LE
					memcpy(dstLine, src, dstLineWidth * dstLineCount * sizeof(u16));
#else
					for (size_t x = 0; x < dstLineWidth * dstLineCount; x++)
					{
						dstLine[x] = LE_TO_LOCAL_16(src[x]);
					}
#endif
				}
				else
				{
					const u16 *src = gpu->VRAMaddrNative + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
					
					for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
					{
						const u16 color = LE_TO_LOCAL_16(src[x]);
						
						for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
						{
							dstLine[_gpuDstPitchIndex[x] + p] = color;
						}
					}
					
					for (size_t line = 1; line < dstLineCount; line++)
					{
						memcpy(dstLine + (line * dstLineWidth), dstLine, dstLineWidth * sizeof(u16));
					}
				}
			}
		}
			break;
			
		case GPUDisplayMode_MainMemory: // Display memory FIFO
			{
				//this has not been tested since the dma timing for dispfifo was changed around the time of
				//newemuloop. it may not work.
				
#ifdef ENABLE_SSE2
				const __m128i fifoMask = _mm_set1_epi32(0x7FFF7FFF);
				for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
				{
					__m128i fifoColor = _mm_set_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
					fifoColor = _mm_shuffle_epi32(fifoColor, 0x1B); // We need to shuffle the four FIFO values back into the correct order, since they were originally loaded in reverse order.
					_mm_store_si128((__m128i *)dstLine + i, _mm_and_si128(fifoColor, fifoMask));
				}
#else
				for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
				{
					((u32 *)dstLine)[i] = DISP_FIFOrecv() & 0x7FFF7FFF;
				}
#endif
				if (_displayInfo.customWidth != GPU_FRAMEBUFFER_NATIVE_WIDTH)
				{
					for (size_t i = GPU_FRAMEBUFFER_NATIVE_WIDTH - 1; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i--)
					{
						for (size_t p = _gpuDstPitchCount[i] - 1; p < _gpuDstPitchCount[i]; p--)
						{
							dstLine[_gpuDstPitchIndex[i] + p] = dstLine[i];
						}
					}
				}
				
				for (size_t line = 1; line < dstLineCount; line++)
				{
					memcpy(dstLine + (line * dstLineWidth), dstLine, dstLineWidth * sizeof(u16));
				}
			}
			break;
	}
	
	//capture after displaying so that we can safely display vram before overwriting it here
	if (gpu->core == GPUCOREID_MAIN)
	{
		//BUG!!! if someone is capturing and displaying both from the fifo, then it will have been 
		//consumed above by the display before we get here
		//(is that even legal? i think so)
		
		if ((vramConfiguration.banks[gpu->dispCapCnt.writeBlock].purpose == VramConfiguration::LCDC) && (l < gpu->dispCapCnt.capy))
		{
			if (gpu->dispCapCnt.capx == DISPCAPCNT::_128)
			{
				GPU_RenderLine_DispCapture<GPU_FRAMEBUFFER_NATIVE_WIDTH/2>(l);
			}
			else
			{
				GPU_RenderLine_DispCapture<GPU_FRAMEBUFFER_NATIVE_WIDTH>(l);
			}
		}
		else
		{
			GPU_RenderLine_DispCapture<0>(l);
		}
		
		if (l == 191) { disp_fifo.head = disp_fifo.tail = 0; }
	}
	
	GPU_RenderLine_MasterBrightness(gpu->MasterBrightMode, gpu->MasterBrightFactor, dstLine, dstLineWidth, dstLineCount);
}

void gpu_savestate(EMUFILE* os)
{
	//version
	write32le(1,os);
	
	os->fwrite((u8 *)GPU_CustomFramebuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);
	
	write32le(MainScreen.gpu->affineInfo[0].x,os);
	write32le(MainScreen.gpu->affineInfo[0].y,os);
	write32le(MainScreen.gpu->affineInfo[1].x,os);
	write32le(MainScreen.gpu->affineInfo[1].y,os);
	write32le(SubScreen.gpu->affineInfo[0].x,os);
	write32le(SubScreen.gpu->affineInfo[0].y,os);
	write32le(SubScreen.gpu->affineInfo[1].x,os);
	write32le(SubScreen.gpu->affineInfo[1].y,os);
}

bool gpu_loadstate(EMUFILE* is, int size)
{
	//read version
	u32 version;

	//sigh.. shouldve used a new version number
	if (size == GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2)
	{
		version = 0;
	}
	else if (size == 0x30024)
	{
		read32le(&version,is);
		version = 1;
	}
	else
	{
		if(read32le(&version,is) != 1) return false;
	}
	
	if (version > 1) return false;

	is->fread((u8 *)GPU_CustomFramebuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);

	if (version == 1)
	{
		read32le(&MainScreen.gpu->affineInfo[0].x,is);
		read32le(&MainScreen.gpu->affineInfo[0].y,is);
		read32le(&MainScreen.gpu->affineInfo[1].x,is);
		read32le(&MainScreen.gpu->affineInfo[1].y,is);
		read32le(&SubScreen.gpu->affineInfo[0].x,is);
		read32le(&SubScreen.gpu->affineInfo[0].y,is);
		read32le(&SubScreen.gpu->affineInfo[1].x,is);
		read32le(&SubScreen.gpu->affineInfo[1].y,is);
		//removed per nitsuja feedback. anyway, this same thing will happen almost immediately in gpu line=0
		//MainScreen.gpu->refreshAffineStartRegs(-1,-1);
		//SubScreen.gpu->refreshAffineStartRegs(-1,-1);
	}

	MainScreen.gpu->updateBLDALPHA();
	SubScreen.gpu->updateBLDALPHA();
	return !is->fail();
}

void GPU::UpdateVRAM3DUsageProperties_BGLayer(const size_t bankIndex, VRAM3DUsageProperties &outProperty)
{
	const bool bg2 = (this->dispCnt().BG2_Enable == 1) && (this->BGTypes[2] == BGType_AffineExt_Direct) && (this->BGSize[2][0] == 256) && (this->BGSize[2][1] == 256);
	const bool bg3 = (this->dispCnt().BG3_Enable == 1) && (this->BGTypes[3] == BGType_AffineExt_Direct) && (this->BGSize[3][0] == 256) && (this->BGSize[3][1] == 256);
	u8 selectedBGLayer = VRAM_NO_3D_USAGE;
	
	if (!bg2 && !bg3)
	{
		return;
	}
	
	if (bg3 && !bg2)
	{
		selectedBGLayer = (this->bgPrio[3] >= this->bgPrio[0]) ? 3 : VRAM_NO_3D_USAGE;
	}
	else if (!bg3 && bg2)
	{
		selectedBGLayer = (this->bgPrio[2] >= this->bgPrio[0]) ? 2 : VRAM_NO_3D_USAGE;
	}
	else if (bg3 && bg2)
	{
		selectedBGLayer = (this->bgPrio[3] >= this->bgPrio[2]) ? ((this->bgPrio[3] >= this->bgPrio[0]) ? 3 : VRAM_NO_3D_USAGE) : ((this->bgPrio[2] >= this->bgPrio[0]) ? 2 : VRAM_NO_3D_USAGE);
	}
	
	if (selectedBGLayer != VRAM_NO_3D_USAGE)
	{
		const BGxPARMS &bgParams = (selectedBGLayer == 3) ? (this->dispx_st)->dispx_BG3PARMS : (this->dispx_st)->dispx_BG2PARMS;
		const AffineInfo &affineParams = this->affineInfo[selectedBGLayer - 2];
		
		if ( (bgParams.BGxPA != 256) ||
		     (bgParams.BGxPB !=   0) ||
		     (bgParams.BGxPC !=   0) ||
		     (bgParams.BGxPD != 256) ||
		     (affineParams.x !=   0) ||
		     (affineParams.y !=   0) )
		{
			selectedBGLayer = VRAM_NO_3D_USAGE;
		}
	}
	
	this->vramBGLayer = selectedBGLayer;
	this->vramBlockBGIndex = bankIndex;
	this->isCustomRenderingNeeded = (selectedBGLayer != VRAM_NO_3D_USAGE);
}

void GPU::UpdateVRAM3DUsageProperties_OBJLayer(const size_t bankIndex, VRAM3DUsageProperties &outProperty)
{
	if ((this->dispCnt().OBJ_Enable != 1) || (this->dispCnt().OBJ_BMP_mapping != 0) || (this->dispCnt().OBJ_BMP_2D_dim != 1))
	{
		return;
	}
	
	for (size_t spriteIndex = 0; spriteIndex < 128; spriteIndex++)
	{
		const OAMAttributes &spriteInfo = this->oamList[spriteIndex];
		
		if ( (spriteInfo.RotScale != 2) && ((spriteInfo.RotScale & 1) == 0) && (spriteInfo.Mode == 3) && (spriteInfo.PaletteIndex != 0) )
		{
			const u32 vramAddress = ( (spriteInfo.TileIndex & 0x1F) * 0x10 ) + ( (spriteInfo.TileIndex & ~0x1F) * 0x80 );
			const SpriteSize sprSize = sprSizeTab[spriteInfo.Size][spriteInfo.Shape];
			
			if( (vramAddress == (MainScreen.gpu->dispCapCnt.writeOffset * ADDRESS_STEP_32KB)) && (sprSize.x == 64) && (sprSize.y == 64) )
			{
				this->vramBlockOBJIndex = bankIndex;
				this->isCustomRenderingNeeded = true;
				return;
			}
		}
	}
}

void GPU_UpdateVRAM3DUsageProperties(VRAM3DUsageProperties &outProperty)
{
	outProperty.blockIndexDisplayVRAM = VRAM_NO_3D_USAGE;
	
	MainScreen.gpu->is3DEnabled = (MainScreen.gpu->dispCnt().BG0_Enable == 1) && (MainScreen.gpu->dispCnt().BG0_3D == 1);
	MainScreen.gpu->isCustomRenderingNeeded = false;
	MainScreen.gpu->vramBlockBGIndex = VRAM_NO_3D_USAGE;
	MainScreen.gpu->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	MainScreen.gpu->vramBGLayer = VRAM_NO_3D_USAGE;
	
	SubScreen.gpu->is3DEnabled = false;
	SubScreen.gpu->isCustomRenderingNeeded = false;
	SubScreen.gpu->vramBlockBGIndex = VRAM_NO_3D_USAGE;
	SubScreen.gpu->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	SubScreen.gpu->vramBGLayer = VRAM_NO_3D_USAGE;
	
#ifdef ENABLE_COMBINATION_RENDERING
	MainScreen.gpu->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	MainScreen.gpu->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	MainScreen.gpu->renderedBuffer = MainScreen.gpu->nativeBuffer;
	
	SubScreen.gpu->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	SubScreen.gpu->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	SubScreen.gpu->renderedBuffer = SubScreen.gpu->nativeBuffer;
	
	_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = false;
	_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
#else
	MainScreen.gpu->renderedWidth = _displayInfo.customWidth;
	MainScreen.gpu->renderedHeight = _displayInfo.customHeight;
	MainScreen.gpu->renderedBuffer = MainScreen.gpu->customBuffer;
	
	SubScreen.gpu->renderedWidth = _displayInfo.customWidth;
	SubScreen.gpu->renderedHeight = _displayInfo.customHeight;
	SubScreen.gpu->renderedBuffer = SubScreen.gpu->customBuffer;
	
	_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = true;
	_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = true;
#endif
	
	_displayInfo.renderedBuffer[NDSDisplayID_Main] = MainDisplay.GetEngine()->renderedBuffer;
	_displayInfo.renderedWidth[NDSDisplayID_Main] = MainDisplay.GetEngine()->renderedWidth;
	_displayInfo.renderedHeight[NDSDisplayID_Main] = MainDisplay.GetEngine()->renderedHeight;
	
	_displayInfo.renderedBuffer[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->renderedBuffer;
	_displayInfo.renderedWidth[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->renderedWidth;
	_displayInfo.renderedHeight[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->renderedHeight;
	
	if (_gpuIsNativeSize)
	{
		return;
	}
	
	MainScreen.gpu->isCustomRenderingNeeded = MainScreen.gpu->is3DEnabled;
	
	// Iterate through VRAM banks A-D and determine if they will be used for this frame.
	for (size_t i = 0; i < 4; i++)
	{
		if (!outProperty.isBlockUsed[i])
		{
			continue;
		}
		
		switch (vramConfiguration.banks[i].purpose)
		{
			case VramConfiguration::ABG:
				MainScreen.gpu->UpdateVRAM3DUsageProperties_BGLayer(i, outProperty);
				break;
				
			case VramConfiguration::BBG:
				SubScreen.gpu->UpdateVRAM3DUsageProperties_BGLayer(i, outProperty);
				break;
				
			case VramConfiguration::AOBJ:
				MainScreen.gpu->UpdateVRAM3DUsageProperties_OBJLayer(i, outProperty);
				break;
				
			case VramConfiguration::BOBJ:
				SubScreen.gpu->UpdateVRAM3DUsageProperties_OBJLayer(i, outProperty);
				break;
				
			case VramConfiguration::LCDC:
			{
				if ((MainScreen.gpu->dispMode == GPUDisplayMode_VRAM) && (MainScreen.gpu->vramBlock == i))
				{
					outProperty.blockIndexDisplayVRAM = i;
				}
				break;
			}
				
			default:
				outProperty.isBlockUsed[i] = false;
				break;
		}
	}
	
#ifdef ENABLE_COMBINATION_RENDERING
	if (MainScreen.gpu->isCustomRenderingNeeded)
	{
		MainScreen.gpu->renderedWidth = DisplayInfo.customWidth;
		MainScreen.gpu->renderedHeight = DisplayInfo.customHeight;
		MainScreen.gpu->renderedBuffer = MainScreen.gpu->customBuffer;
	}
	
	if (SubScreen.gpu->isCustomRenderingNeeded)
	{
		SubScreen.gpu->renderedWidth = DisplayInfo.customWidth;
		SubScreen.gpu->renderedHeight = DisplayInfo.customHeight;
		SubScreen.gpu->renderedBuffer = SubScreen.gpu->customBuffer;
	}
	
	DisplayInfo.didPerformCustomRender[NDSDisplayID_Main] = MainDisplay.GetEngine()->isCustomRenderingNeeded;
	DisplayInfo.renderedBuffer[NDSDisplayID_Main] = MainDisplay.GetEngine()->renderedBuffer;
	DisplayInfo.renderedWidth[NDSDisplayID_Main] = MainDisplay.GetEngine()->renderedWidth;
	DisplayInfo.renderedHeight[NDSDisplayID_Main] = MainDisplay.GetEngine()->renderedHeight;
	
	DisplayInfo.didPerformCustomRender[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->isCustomRenderingNeeded;
	DisplayInfo.renderedBuffer[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->renderedBuffer;
	DisplayInfo.renderedWidth[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->renderedWidth;
	DisplayInfo.renderedHeight[NDSDisplayID_Touch] = TouchDisplay.GetEngine()->renderedHeight;
#endif
}

const NDSDisplayInfo& NDS_GetDisplayInfo()
{
	return _displayInfo;
}

u32 GPU::getAffineStart(const size_t layer, int xy)
{
	if (xy == 0)
		return affineInfo[layer-2].x;
	else
		return affineInfo[layer-2].y;
}

void GPU::setAffineStartWord(const size_t layer, int xy, u16 val, int word)
{
	u32 curr = getAffineStart(layer, xy);
	
	if (word == 0)
		curr = (curr & 0xFFFF0000) | val;
	else
		curr = (curr & 0x0000FFFF) | (((u32)val) << 16);
	
	setAffineStart(layer, xy, curr);
}

void GPU::setAffineStart(const size_t layer, int xy, u32 val)
{
	if (xy == 0)
		affineInfo[layer-2].x = val;
	else
		affineInfo[layer-2].y = val;
	
	refreshAffineStartRegs(layer, xy);
}

void GPU::refreshAffineStartRegs(const int num, const int xy)
{
	if (num == -1)
	{
		refreshAffineStartRegs(2, xy);
		refreshAffineStartRegs(3, xy);
		return;
	}

	if (xy == -1)
	{
		refreshAffineStartRegs(num, 0);
		refreshAffineStartRegs(num, 1);
		return;
	}

	BGxPARMS *params = (num == 2) ? &(dispx_st)->dispx_BG2PARMS : &(dispx_st)->dispx_BG3PARMS;
	
	if (xy == 0)
		params->BGxX = affineInfo[num-2].x;
	else
		params->BGxY = affineInfo[num-2].y;
}

template<bool MOSAIC>
void GPU::modeRender(const size_t layer)
{
	switch(GPU_mode2type[dispCnt().BG_Mode][layer])
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

u32 GPU::getHOFS(const size_t bg)
{
	return LE_TO_LOCAL_16(dispx_st->dispx_BGxOFS[bg].BGxHOFS) & 0x01FF;
}

u32 GPU::getVOFS(const size_t bg)
{
	return LE_TO_LOCAL_16(dispx_st->dispx_BGxOFS[bg].BGxVOFS) & 0x01FF;
}

void GPU::updateBLDALPHA()
{
	blendTable = (TBlendTable *)&gpuBlendTable555[BLDALPHA_EVA][BLDALPHA_EVB][0][0];
}

void GPU::setBLDALPHA(const u16 val)
{
	BLDALPHA_EVA = (  (val       & 0x1F) > 16 ) ? 16 :  (val       & 0x1F);
	BLDALPHA_EVB = ( ((val >> 8) & 0x1F) > 16 ) ? 16 : ((val >> 8) & 0x1F);
	this->updateBLDALPHA();
}

void GPU::setBLDALPHA_EVA(const u8 val)
{
	BLDALPHA_EVA = ( (val & 0x1F) > 16 ) ? 16 :  (val & 0x1F);
	this->updateBLDALPHA();
}

void GPU::setBLDALPHA_EVB(const u8 val)
{
	BLDALPHA_EVB = ( (val & 0x1F) > 16 ) ? 16 :  (val & 0x1F);
	this->updateBLDALPHA();
}

void GPU::SetDisplayByID(const NDSDisplayID theDisplayID)
{
	this->targetDisplayID = theDisplayID;
	
	if (theDisplayID == NDSDisplayID_Main)
	{
#ifdef ENABLE_COMBINATION_RENDERING
		this->nativeBuffer = GPU_NativeFramebuffer;
#endif
		this->customBuffer = GPU_CustomFramebuffer;
	}
	else
	{
#ifdef ENABLE_COMBINATION_RENDERING
		this->nativeBuffer = GPU_NativeFramebuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
#endif
		this->customBuffer = GPU_CustomFramebuffer + (_displayInfo.customWidth * _displayInfo.customHeight);
	}
}

NDSDisplay::NDSDisplay()
{
	_ID = NDSDisplayID_Main;
	SetEngineByID(GPUCOREID_MAIN);
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID)
{
	_ID = displayID;
	SetEngineByID(GPUCOREID_MAIN);
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID, const GPUCoreID coreID)
{
	_ID = displayID;
	SetEngineByID(coreID);
}

GPU* NDSDisplay::GetEngine()
{
	return this->_gpu;
}

GPUCoreID NDSDisplay::GetEngineID()
{
	return this->_gpu->core;
}

void NDSDisplay::SetEngineByID(const GPUCoreID theID)
{
	this->_gpu = (theID == GPUCOREID_MAIN) ? &GPU_main : &GPU_sub;
	this->_gpu->SetDisplayByID(this->_ID);
}
