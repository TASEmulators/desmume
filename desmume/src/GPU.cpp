/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2008-2016 DeSmuME team

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

u32 Render3DFramesPerSecond;

//instantiate static instance
u16 GPUEngineBase::_fadeInColors[17][0x8000];
u16 GPUEngineBase::_fadeOutColors[17][0x8000];
u8 GPUEngineBase::_blendTable555[17][17][32][32];
GPUEngineBase::MosaicLookup GPUEngineBase::_mosaicLookup;

GPUSubsystem *GPU = NULL;

static size_t _gpuLargestDstLineCount = 1;
static size_t _gpuVRAMBlockOffset = GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH;

static u16 *_gpuDstToSrcIndex = NULL; // Key: Destination pixel index / Value: Source pixel index
static u8 *_gpuDstToSrcSSSE3_u8 = NULL;
static u8 *_gpuDstToSrcSSSE3_u16 = NULL;

static CACHE_ALIGN size_t _gpuDstPitchCount[GPU_FRAMEBUFFER_NATIVE_WIDTH];	// Key: Source pixel index in x-dimension / Value: Number of x-dimension destination pixels for the source pixel
static CACHE_ALIGN size_t _gpuDstPitchIndex[GPU_FRAMEBUFFER_NATIVE_WIDTH];	// Key: Source pixel index in x-dimension / Value: First destination pixel that maps to the source pixel
static CACHE_ALIGN size_t _gpuDstLineCount[GPU_FRAMEBUFFER_NATIVE_HEIGHT];	// Key: Source line index / Value: Number of destination lines for the source line
static CACHE_ALIGN size_t _gpuDstLineIndex[GPU_FRAMEBUFFER_NATIVE_HEIGHT];	// Key: Source line index / Value: First destination line that maps to the source line
static CACHE_ALIGN size_t _gpuCaptureLineCount[GPU_VRAM_BLOCK_LINES + 1];	// Key: Source line index / Value: Number of destination lines for the source line
static CACHE_ALIGN size_t _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES + 1];	// Key: Source line index / Value: First destination line that maps to the source line

const CACHE_ALIGN SpriteSize GPUEngineBase::_sprSizeTab[4][4] = {
     {{8, 8}, {16, 8}, {8, 16}, {8, 8}},
     {{16, 16}, {32, 8}, {8, 32}, {8, 8}},
     {{32, 32}, {32, 16}, {16, 32}, {8, 8}},
     {{64, 64}, {64, 32}, {32, 64}, {8, 8}},
};

const CACHE_ALIGN BGType GPUEngineBase::_mode2type[8][4] = {
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
const CACHE_ALIGN BGLayerSize GPUEngineBase::_BGLayerSizeLUT[8][4] = {
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //Invalid
    {{256,256}, {512,256}, {256,512}, {512,512}}, //text
    {{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine
    {{512,1024}, {1024,512}, {0,0}, {0,0}}, //large 8bpp
	{{0, 0}, {0, 0}, {0, 0}, {0, 0}}, //affine ext (to be elaborated with another value)
	{{128,128}, {256,256}, {512,512}, {1024,1024}}, //affine ext 256x16
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext 256x1
	{{128,128}, {256,256}, {512,256}, {512,512}}, //affine ext direct
};

const CACHE_ALIGN u8 GPUEngineBase::_winEmpty[GPU_FRAMEBUFFER_NATIVE_WIDTH] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
};

/*****************************************************************************/
//			BACKGROUND RENDERING -ROTOSCALE-
/*****************************************************************************/

FORCEINLINE void rot_tiled_8bit_entry(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	const u16 tileindex = *(u8*)MMU_gpu_map(map + ((auxX>>3) + (auxY>>3) * (lg>>3)));
	const u16 x = auxX & 0x0007;
	const u16 y = auxY & 0x0007;
	
	outIndex = *(u8*)MMU_gpu_map(tile + ((tileindex<<6)+(y<<3)+x));
	outColor = LE_TO_LOCAL_16(pal[outIndex]);
}

template<bool EXTPAL>
FORCEINLINE void rot_tiled_16bit_entry(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	TILEENTRY tileentry;
	tileentry.val = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map(map + (((auxX>>3) + (auxY>>3) * (lg>>3))<<1)) );
	
	const u16 x = ((tileentry.bits.HFlip) ? 7 - (auxX) : (auxX)) & 0x0007;
	const u16 y = ((tileentry.bits.VFlip) ? 7 - (auxY) : (auxY)) & 0x0007;
	
	outIndex = *(u8*)MMU_gpu_map(tile + ((tileentry.bits.TileNum<<6)+(y<<3)+x));
	outColor = LE_TO_LOCAL_16(pal[(outIndex + (EXTPAL ? (tileentry.bits.Palette<<8) : 0))]);
}

FORCEINLINE void rot_256_map(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	outIndex = *(u8*)MMU_gpu_map(map + ((auxX + auxY * lg)));
	outColor = LE_TO_LOCAL_16(pal[outIndex]);
}

FORCEINLINE void rot_BMP_map(const s32 auxX, const s32 auxY, const int lg, const u32 map, const u32 tile, const u16 *__restrict pal, u8 &outIndex, u16 &outColor)
{
	outColor = LE_TO_LOCAL_16( *(u16 *)MMU_gpu_map(map + ((auxX + auxY * lg) << 1)) );
	outIndex = ((outColor & 0x8000) == 0) ? 0 : 1;
}

void gpu_savestate(EMUFILE* os)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const GPUEngineA *mainEngine = GPU->GetEngineMain();
	const GPUEngineB *subEngine = GPU->GetEngineSub();
	
	//version
	write32le(1,os);
	
	os->fwrite((u8 *)dispInfo.masterCustomBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);
	
	write32le(mainEngine->savedBG2X.value, os);
	write32le(mainEngine->savedBG2Y.value, os);
	write32le(mainEngine->savedBG3X.value, os);
	write32le(mainEngine->savedBG3Y.value, os);
	write32le(subEngine->savedBG2X.value, os);
	write32le(subEngine->savedBG2Y.value, os);
	write32le(subEngine->savedBG3X.value, os);
	write32le(subEngine->savedBG3Y.value, os);
}

bool gpu_loadstate(EMUFILE* is, int size)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	GPUEngineA *mainEngine = GPU->GetEngineMain();
	GPUEngineB *subEngine = GPU->GetEngineSub();
	
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
	
	is->fread((u8 *)dispInfo.masterCustomBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);
	
	if (version == 1)
	{
		read32le((u32 *)&mainEngine->savedBG2X, is);
		read32le((u32 *)&mainEngine->savedBG2Y, is);
		read32le((u32 *)&mainEngine->savedBG3X, is);
		read32le((u32 *)&mainEngine->savedBG3Y, is);
		read32le((u32 *)&subEngine->savedBG2X, is);
		read32le((u32 *)&subEngine->savedBG2Y, is);
		read32le((u32 *)&subEngine->savedBG3X, is);
		read32le((u32 *)&subEngine->savedBG3Y, is);
		//removed per nitsuja feedback. anyway, this same thing will happen almost immediately in gpu line=0
		//mainEngine->refreshAffineStartRegs(-1,-1);
		//subEngine->refreshAffineStartRegs(-1,-1);
	}
	
	mainEngine->ParseAllRegisters<GPUEngineID_Main>();
	subEngine->ParseAllRegisters<GPUEngineID_Sub>();
	
	return !is->fail();
}

/*****************************************************************************/
//			INITIALIZATION
/*****************************************************************************/
void GPUEngineBase::_InitLUTs()
{
	static bool didInit = false;
	
	if (didInit)
	{
		return;
	}
	
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
	
	for (size_t i = 0; i <= 16; i++)
	{
		for (size_t j = 0x0000; j < 0x8000; j++)
		{
			COLOR cur;

			cur.val = j;
			cur.bits.red = (cur.bits.red + ((31 - cur.bits.red) * i / 16));
			cur.bits.green = (cur.bits.green + ((31 - cur.bits.green) * i / 16));
			cur.bits.blue = (cur.bits.blue + ((31 - cur.bits.blue) * i / 16));
			cur.bits.alpha = 0;
			GPUEngineBase::_fadeInColors[i][j] = cur.val;
			
			cur.val = j;
			cur.bits.red = (cur.bits.red - (cur.bits.red * i / 16));
			cur.bits.green = (cur.bits.green - (cur.bits.green * i / 16));
			cur.bits.blue = (cur.bits.blue - (cur.bits.blue * i / 16));
			cur.bits.alpha = 0;
			GPUEngineBase::_fadeOutColors[i][j] = cur.val;
		}
	}
	
	for(int c0=0;c0<=31;c0++) 
		for(int c1=0;c1<=31;c1++) 
			for(int eva=0;eva<=16;eva++)
				for(int evb=0;evb<=16;evb++)
				{
					int blend = ((c0 * eva) + (c1 * evb) ) / 16;
					int final = std::min<int>(31,blend);
					GPUEngineBase::_blendTable555[eva][evb][c0][c1] = final;
				}
	
	didInit = true;
}

GPUEngineBase::GPUEngineBase()
{
	_IORegisterMap = NULL;
	_paletteOBJ = NULL;
	
	_BGLayer[GPULayerID_BG0].layerID = GPULayerID_BG0;
	_BGLayer[GPULayerID_BG1].layerID = GPULayerID_BG1;
	_BGLayer[GPULayerID_BG2].layerID = GPULayerID_BG2;
	_BGLayer[GPULayerID_BG3].layerID = GPULayerID_BG3;
	
	_BGLayer[GPULayerID_BG0].extPaletteSlot = GPULayerID_BG0;
	_BGLayer[GPULayerID_BG1].extPaletteSlot = GPULayerID_BG1;
	_BGLayer[GPULayerID_BG2].extPaletteSlot = GPULayerID_BG2;
	_BGLayer[GPULayerID_BG3].extPaletteSlot = GPULayerID_BG3;
	
	_BGLayer[GPULayerID_BG0].extPalette = NULL;
	_BGLayer[GPULayerID_BG1].extPalette = NULL;
	_BGLayer[GPULayerID_BG2].extPalette = NULL;
	_BGLayer[GPULayerID_BG3].extPalette = NULL;
	
	_InitLUTs();
	_internalRenderLineTargetCustom = NULL;
	_renderLineLayerIDCustom = NULL;
	_bgLayerIndexCustom = NULL;
	_bgLayerColorCustom = NULL;
}

GPUEngineBase::~GPUEngineBase()
{
	free_aligned(this->_internalRenderLineTargetCustom);
	this->_internalRenderLineTargetCustom = NULL;
	free_aligned(this->_renderLineLayerIDCustom);
	this->_renderLineLayerIDCustom = NULL;
	free_aligned(this->_bgLayerIndexCustom);
	this->_bgLayerIndexCustom = NULL;
	free_aligned(this->_bgLayerColorCustom);
	this->_bgLayerColorCustom = NULL;
}

void GPUEngineBase::_Reset_Base()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	memset(this->_sprColor, 0, sizeof(this->_sprColor));
	memset(this->_sprAlpha, 0, sizeof(this->_sprAlpha));
	memset(this->_sprType, OBJMode_Normal, sizeof(this->_sprType));
	memset(this->_sprPrio, 0x7F, sizeof(this->_sprPrio));
	memset(this->_sprNum, 0, sizeof(this->_sprNum));
	
	memset(this->_h_win[0], 0, sizeof(this->_h_win[0]));
	memset(this->_h_win[1], 0, sizeof(this->_h_win[1]));
	memset(&this->_mosaicColors, 0, sizeof(MosaicColor));
	memset(this->_itemsForPriority, 0, sizeof(this->_itemsForPriority));
	
	memset(this->_internalRenderLineTargetNative, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	
	if (this->_internalRenderLineTargetCustom != NULL)
	{
		memset(this->_internalRenderLineTargetCustom, 0, dispInfo.customWidth * _gpuLargestDstLineCount * sizeof(u16));
	}
	if (this->_renderLineLayerIDCustom != NULL)
	{
		memset(this->_renderLineLayerIDCustom, 0, dispInfo.customWidth * _gpuLargestDstLineCount * 4 * sizeof(u8));
	}
	
	this->_displayOutputMode = GPUDisplayMode_Off;
	
	this->_enableLayer[GPULayerID_BG0] = false;
	this->_enableLayer[GPULayerID_BG1] = false;
	this->_enableLayer[GPULayerID_BG2] = false;
	this->_enableLayer[GPULayerID_BG3] = false;
	this->_enableLayer[GPULayerID_OBJ] = false;
	this->_isAnyBGLayerEnabled = false;
	
	this->_BGLayer[GPULayerID_BG0].BGnCNT = this->_IORegisterMap->BG0CNT;
	this->_BGLayer[GPULayerID_BG1].BGnCNT = this->_IORegisterMap->BG1CNT;
	this->_BGLayer[GPULayerID_BG2].BGnCNT = this->_IORegisterMap->BG2CNT;
	this->_BGLayer[GPULayerID_BG3].BGnCNT = this->_IORegisterMap->BG3CNT;
	
	this->_BGLayer[GPULayerID_BG0].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	this->_BGLayer[GPULayerID_BG1].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	this->_BGLayer[GPULayerID_BG2].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	this->_BGLayer[GPULayerID_BG3].size = GPUEngineBase::_BGLayerSizeLUT[BGType_Affine][1];
	
	this->_BGLayer[GPULayerID_BG0].baseType = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG1].baseType = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG2].baseType = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG3].baseType = BGType_Invalid;
	
	this->_BGLayer[GPULayerID_BG0].type = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG1].type = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG2].type = BGType_Invalid;
	this->_BGLayer[GPULayerID_BG3].type = BGType_Invalid;
	
	this->_BGLayer[GPULayerID_BG0].priority = 0;
	this->_BGLayer[GPULayerID_BG1].priority = 0;
	this->_BGLayer[GPULayerID_BG2].priority = 0;
	this->_BGLayer[GPULayerID_BG3].priority = 0;
	
	this->_BGLayer[GPULayerID_BG0].isVisible = false;
	this->_BGLayer[GPULayerID_BG1].isVisible = false;
	this->_BGLayer[GPULayerID_BG2].isVisible = false;
	this->_BGLayer[GPULayerID_BG3].isVisible = false;
	
	this->_BGLayer[GPULayerID_BG0].isMosaic = false;
	this->_BGLayer[GPULayerID_BG1].isMosaic = false;
	this->_BGLayer[GPULayerID_BG2].isMosaic = false;
	this->_BGLayer[GPULayerID_BG3].isMosaic = false;
	
	this->_BGLayer[GPULayerID_BG0].isDisplayWrapped = false;
	this->_BGLayer[GPULayerID_BG1].isDisplayWrapped = false;
	this->_BGLayer[GPULayerID_BG2].isDisplayWrapped = false;
	this->_BGLayer[GPULayerID_BG3].isDisplayWrapped = false;
	
	this->_BGLayer[GPULayerID_BG0].extPaletteSlot = GPULayerID_BG0;
	this->_BGLayer[GPULayerID_BG1].extPaletteSlot = GPULayerID_BG1;
	this->_BGLayer[GPULayerID_BG0].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG0];
	this->_BGLayer[GPULayerID_BG1].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG1];
	this->_BGLayer[GPULayerID_BG2].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG2];
	this->_BGLayer[GPULayerID_BG3].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][GPULayerID_BG3];
	
	this->_mosaicWidthBG = &GPUEngineBase::_mosaicLookup.table[0][0];
	this->_mosaicHeightBG = &GPUEngineBase::_mosaicLookup.table[0][0];
	this->_mosaicWidthOBJ = &GPUEngineBase::_mosaicLookup.table[0][0];
	this->_mosaicHeightOBJ = &GPUEngineBase::_mosaicLookup.table[0][0];
	this->_isBGMosaicSet = false;
	this->_isOBJMosaicSet = false;
	
	this->_curr_win[0] = GPUEngineBase::_winEmpty;
	this->_curr_win[1] = GPUEngineBase::_winEmpty;
	this->_needUpdateWINH[0] = true;
	this->_needUpdateWINH[1] = true;
	
	this->isCustomRenderingNeeded = false;
	this->vramBGLayer = VRAM_NO_3D_USAGE;
	this->vramBlockBGIndex = VRAM_NO_3D_USAGE;
	this->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	
	this->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->isLineRenderNative[l] = true;
		this->isLineOutputNative[l] = true;
	}
	
	this->_sprBoundary = 0;
	this->_sprBMPBoundary = 0;
	
	this->_WIN0_ENABLED = false;
	this->_WIN1_ENABLED = false;
	this->_WINOBJ_ENABLED = false;
	this->_isAnyWindowEnabled = false;
	
	this->_BLDALPHA_EVA = 0;
	this->_BLDALPHA_EVB = 0;
	this->_BLDALPHA_EVY = 0;
	this->_blendTable = (TBlendTable *)&GPUEngineBase::_blendTable555[this->_BLDALPHA_EVA][this->_BLDALPHA_EVB][0][0];
	this->_currentFadeInColors = &GPUEngineBase::_fadeInColors[this->_BLDALPHA_EVY][0];
	this->_currentFadeOutColors = &GPUEngineBase::_fadeOutColors[this->_BLDALPHA_EVY][0];
	
	this->_blend2[GPULayerID_BG0] = false;
	this->_blend2[GPULayerID_BG1] = false;
	this->_blend2[GPULayerID_BG2] = false;
	this->_blend2[GPULayerID_BG3] = false;
	this->_blend2[GPULayerID_OBJ] = false;
	this->_blend2[GPULayerID_Backdrop] = false;
	
#ifdef ENABLE_SSSE3
	this->_blend2_SSSE3 = _mm_setzero_si128();
#endif
	
	this->_isMasterBrightFullIntensity = false;
	
	this->_spriteRenderMode = SpriteRenderMode_Sprite1D;
	
	this->savedBG2X.value = 0;
	this->savedBG2Y.value = 0;
	this->savedBG3X.value = 0;
	this->savedBG3Y.value = 0;
	
	this->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->renderedBuffer = this->nativeBuffer;
}

void GPUEngineBase::Reset()
{
	this->_Reset_Base();
}

void GPUEngineBase::_ResortBGLayers()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	int i, prio;
	itemsForPriority_t *item;
	
	// we don't need to check for windows here...
	// if we tick boxes, invisible layers become invisible & vice versa
#define OP ^ !
	// if we untick boxes, layers become invisible
	//#define OP &&
	this->_enableLayer[GPULayerID_BG0] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG0] OP(this->_BGLayer[GPULayerID_BG0].isVisible);
	this->_enableLayer[GPULayerID_BG1] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG1] OP(this->_BGLayer[GPULayerID_BG1].isVisible);
	this->_enableLayer[GPULayerID_BG2] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG2] OP(this->_BGLayer[GPULayerID_BG2].isVisible);
	this->_enableLayer[GPULayerID_BG3] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG3] OP(this->_BGLayer[GPULayerID_BG3].isVisible);
	this->_enableLayer[GPULayerID_OBJ] = CommonSettings.dispLayers[this->_engineID][GPULayerID_OBJ] OP(DISPCNT.OBJ_Enable);
	
	this->_isAnyBGLayerEnabled = this->_enableLayer[GPULayerID_BG0] || this->_enableLayer[GPULayerID_BG1] || this->_enableLayer[GPULayerID_BG2] || this->_enableLayer[GPULayerID_BG3];
	
	// KISS ! lower priority first, if same then lower num
	for (i = 0; i < NB_PRIORITIES; i++)
	{
		item = &(this->_itemsForPriority[i]);
		item->nbBGs = 0;
		item->nbPixelsX = 0;
	}
	
	for (i = NB_BG; i > 0; )
	{
		i--;
		if (!this->_enableLayer[i]) continue;
		prio = this->_BGLayer[i].priority;
		item = &(this->_itemsForPriority[prio]);
		item->BGs[item->nbBGs]=i;
		item->nbBGs++;
	}
	
#if 0
	//debug
	for (i = 0; i < NB_PRIORITIES; i++)
	{
		item = &(this->_itemsForPriority[i]);
		printf("%d : ", i);
		for (j=0; j<NB_PRIORITIES; j++)
		{
			if (j < item->nbBGs)
				printf("BG%d ", item->BGs[j]);
			else
				printf("... ", item->BGs[j]);
		}
	}
	printf("\n");
#endif
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectBlend(const u16 colA, const u16 colB, const u16 blendEVA, const u16 blendEVB)
{
	u16 ra = (colA & 0x001F);
	u16 ga = (colA & 0x03E0) >>  5;
	u16 ba = (colA & 0x7C00) >> 10;
	u16 rb = (colB & 0x001F);
	u16 gb = (colB & 0x03E0) >>  5;
	u16 bb = (colB & 0x7C00) >> 10;
	
	ra = ( (ra * blendEVA) + (rb * blendEVB) ) / 16;
	if (ra > 31) ra = 31;
	ga = ( (ga * blendEVA) + (gb * blendEVB) ) / 16;
	if (ga > 31) ga = 31;
	ba = ( (ba * blendEVA) + (bb * blendEVB) ) / 16;
	if (ba > 31) ba = 31;
	
	return ra | (ga << 5) | (ba << 10);
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectBlend(const u16 colA, const u16 colB, const TBlendTable *blendTable)
{
	const u8 r = (*blendTable)[ colA        & 0x1F][ colB        & 0x1F];
	const u8 g = (*blendTable)[(colA >>  5) & 0x1F][(colB >>  5) & 0x1F];
	const u8 b = (*blendTable)[(colA >> 10) & 0x1F][(colB >> 10) & 0x1F];

	return r | (g << 5) | (b << 10);
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectBlend3D(const FragmentColor colA, const u16 colB)
{
	const u8 alpha = colA.a + 1;
	COLOR c2;
	COLOR cfinal;
	
	c2.val = colB;
	
	cfinal.bits.red   = ((colA.r * alpha) + ((c2.bits.red   << 1) * (32 - alpha))) >> 6;
	cfinal.bits.green = ((colA.g * alpha) + ((c2.bits.green << 1) * (32 - alpha))) >> 6;
	cfinal.bits.blue  = ((colA.b * alpha) + ((c2.bits.blue  << 1) * (32 - alpha))) >> 6;
	cfinal.bits.alpha = 0;
	
	return cfinal.val;
}

FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectBlend3D(const FragmentColor colA, const FragmentColor colB)
{
	FragmentColor blendedColor;
	const u8 alpha = colA.a + 1;
	
	blendedColor.r = ((colA.r * alpha) + (colB.r * (32 - alpha))) >> 6;
	blendedColor.g = ((colA.g * alpha) + (colB.g * (32 - alpha))) >> 6;
	blendedColor.b = ((colA.b * alpha) + (colB.b * (32 - alpha))) >> 6;
	blendedColor.a = 0;
	
	return blendedColor;
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectIncreaseBrightness(const u16 col)
{
	return this->_currentFadeInColors[col];
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectIncreaseBrightness(const u16 col, const u16 blendEVY)
{
	u16 r = (col & 0x001F);
	u16 g = (col & 0x03E0) >>  5;
	u16 b = (col & 0x7C00) >> 10;
	
	r = (r + ((31 - r) * blendEVY / 16));
	g = (g + ((31 - g) * blendEVY / 16));
	b = (b + ((31 - b) * blendEVY / 16));
	
	return r | (g << 5) | (b << 10);
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectDecreaseBrightness(const u16 col)
{
	return this->_currentFadeOutColors[col];
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectDecreaseBrightness(const u16 col, const u16 blendEVY)
{
	u16 r = (col & 0x001F);
	u16 g = (col & 0x03E0) >>  5;
	u16 b = (col & 0x7C00) >> 10;
	
	r = (r - (r * blendEVY / 16));
	g = (g - (g * blendEVY / 16));
	b = (b - (b * blendEVY / 16));
	
	return r | (g << 5) | (b << 10);
}

#ifdef ENABLE_SSE2

FORCEINLINE __m128i GPUEngineBase::_ColorEffectIncreaseBrightness(const __m128i &col, const __m128i &blendEVY)
{
	__m128i r_vec128 = _mm_and_si128( col, _mm_set1_epi16(0x001F) );
	__m128i g_vec128 = _mm_srli_epi16( _mm_and_si128(col, _mm_set1_epi16(0x03E0)),  5 );
	__m128i b_vec128 = _mm_srli_epi16( _mm_and_si128(col, _mm_set1_epi16(0x7C00)), 10 );
	
	r_vec128 = _mm_add_epi16( r_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), r_vec128), blendEVY), 4) );
	g_vec128 = _mm_add_epi16( g_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), g_vec128), blendEVY), 4) );
	b_vec128 = _mm_add_epi16( b_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), b_vec128), blendEVY), 4) );
	
	return _mm_or_si128(r_vec128, _mm_or_si128( _mm_slli_epi16(g_vec128, 5), _mm_slli_epi16(b_vec128, 10)) );
}

FORCEINLINE __m128i GPUEngineBase::_ColorEffectDecreaseBrightness(const __m128i &col, const __m128i &blendEVY)
{
	__m128i r_vec128 = _mm_and_si128( col, _mm_set1_epi16(0x001F) );
	__m128i g_vec128 = _mm_srli_epi16( _mm_and_si128(col, _mm_set1_epi16(0x03E0)),  5 );
	__m128i b_vec128 = _mm_srli_epi16( _mm_and_si128(col, _mm_set1_epi16(0x7C00)), 10 );
	
	r_vec128 = _mm_sub_epi16( r_vec128, _mm_srli_epi16(_mm_mullo_epi16(r_vec128, blendEVY), 4) );
	g_vec128 = _mm_sub_epi16( g_vec128, _mm_srli_epi16(_mm_mullo_epi16(g_vec128, blendEVY), 4) );
	b_vec128 = _mm_sub_epi16( b_vec128, _mm_srli_epi16(_mm_mullo_epi16(b_vec128, blendEVY), 4) );
	
	return _mm_or_si128(r_vec128, _mm_or_si128( _mm_slli_epi16(g_vec128, 5), _mm_slli_epi16(b_vec128, 10)) );
}

FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend(const __m128i &colA, const __m128i &colB, const __m128i &blendEVA, const __m128i &blendEVB)
{
	__m128i ra_vec128 = _mm_and_si128( colA, _mm_set1_epi16(0x001F) );
	__m128i ga_vec128 = _mm_srli_epi16( _mm_and_si128(colA, _mm_set1_epi16(0x03E0)),  5 );
	__m128i ba_vec128 = _mm_srli_epi16( _mm_and_si128(colA, _mm_set1_epi16(0x7C00)), 10 );
	__m128i rb_vec128 = _mm_and_si128( colB, _mm_set1_epi16(0x001F) );
	__m128i gb_vec128 = _mm_srli_epi16( _mm_and_si128(colB, _mm_set1_epi16(0x03E0)),  5 );
	__m128i bb_vec128 = _mm_srli_epi16( _mm_and_si128(colB, _mm_set1_epi16(0x7C00)), 10 );
	
	ra_vec128 = _mm_srli_epi16( _mm_add_epi16( _mm_mullo_epi16(ra_vec128, blendEVA), _mm_mullo_epi16(rb_vec128, blendEVB)), 4 );
	ra_vec128 = _mm_min_epi16(ra_vec128, _mm_set1_epi16(31));
	
	ga_vec128 = _mm_srli_epi16( _mm_add_epi16( _mm_mullo_epi16(ga_vec128, blendEVA), _mm_mullo_epi16(gb_vec128, blendEVB)), 4 );
	ga_vec128 = _mm_min_epi16(ga_vec128, _mm_set1_epi16(31));
	
	ba_vec128 = _mm_srli_epi16( _mm_add_epi16( _mm_mullo_epi16(ba_vec128, blendEVA), _mm_mullo_epi16(bb_vec128, blendEVB)), 4 );
	ba_vec128 = _mm_min_epi16(ba_vec128, _mm_set1_epi16(31));
	
	return _mm_or_si128(ra_vec128, _mm_or_si128( _mm_slli_epi16(ga_vec128, 5), _mm_slli_epi16(ba_vec128, 10)) );
}

FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend3D(const __m128i &colA_Lo, const __m128i &colA_Hi, const __m128i &colB)
{
	__m128i rb = _mm_slli_epi16( _mm_and_si128(_mm_set1_epi16(0x001F), colB), 1);
	__m128i gb = _mm_srli_epi16( _mm_and_si128(_mm_set1_epi16(0x03E0), colB), 4);
	__m128i bb = _mm_srli_epi16( _mm_and_si128(_mm_set1_epi16(0x7C00), colB), 9);
	
	__m128i ra_lo = _mm_and_si128(_mm_set1_epi32(0x000000FF), colA_Lo);
	__m128i ga_lo = _mm_srli_epi32( _mm_and_si128(_mm_set1_epi32(0x0000FF00), colA_Lo), 8 );
	__m128i ba_lo = _mm_srli_epi32( _mm_and_si128(_mm_set1_epi32(0x00FF0000), colA_Lo), 16 );
	__m128i aa_lo = _mm_srli_epi32( _mm_and_si128(_mm_set1_epi32(0xFF000000), colA_Lo), 24 );
	
	__m128i ra_hi = _mm_and_si128(_mm_set1_epi32(0x000000FF), colA_Hi);
	__m128i ga_hi = _mm_srli_epi32( _mm_and_si128(_mm_set1_epi32(0x0000FF00), colA_Hi), 8 );
	__m128i ba_hi = _mm_srli_epi32( _mm_and_si128(_mm_set1_epi32(0x00FF0000), colA_Hi), 16 );
	__m128i aa_hi = _mm_srli_epi32( _mm_and_si128(_mm_set1_epi32(0xFF000000), colA_Hi), 24 );
	
	__m128i ra = _mm_packs_epi32(ra_lo, ra_hi);
	__m128i ga = _mm_packs_epi32(ga_lo, ga_hi);
	__m128i ba = _mm_packs_epi32(ba_lo, ba_hi);
	__m128i aa = _mm_packs_epi32(aa_lo, aa_hi);
	
	aa = _mm_add_epi16(aa, _mm_set1_epi16(1));
	
	ra = _mm_srli_epi16( _mm_add_epi16( _mm_mullo_epi16(ra, aa), _mm_mullo_epi16(rb, _mm_sub_epi16(_mm_set1_epi16(32), aa)) ), 6 );
	ga = _mm_srli_epi16( _mm_add_epi16( _mm_mullo_epi16(ga, aa), _mm_mullo_epi16(gb, _mm_sub_epi16(_mm_set1_epi16(32), aa)) ), 6 );
	ba = _mm_srli_epi16( _mm_add_epi16( _mm_mullo_epi16(ba, aa), _mm_mullo_epi16(bb, _mm_sub_epi16(_mm_set1_epi16(32), aa)) ), 6 );
	
	return _mm_or_si128( _mm_or_si128(ra, _mm_slli_epi16(ga, 5)), _mm_slli_epi16(ba, 10) );
}

#endif

void GPUEngineBase::ParseReg_MASTER_BRIGHT()
{
	if (!nds.isInVblank())
	{
		PROGINFO("Changing master brightness outside of vblank, line=%d\n", nds.VCount);
	}
	
	const IOREG_MASTER_BRIGHT &MASTER_BRIGHT = this->_IORegisterMap->MASTER_BRIGHT;
	this->_isMasterBrightFullIntensity = ( (MASTER_BRIGHT.Intensity >= 16) && ((MASTER_BRIGHT.Mode == GPUMasterBrightMode_Up) || (MASTER_BRIGHT.Mode == GPUMasterBrightMode_Down)) );
	//printf("MASTER BRIGHTNESS %d to %d at %d\n",this->core,MASTER_BRIGHT.Intensity,nds.VCount);
}

//Sets up LCD control variables for Display Engines A and B for quick reading
template<GPUEngineID ENGINEID>
void GPUEngineBase::ParseReg_DISPCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->_displayOutputMode = (ENGINEID == GPUEngineID_Main) ? (GPUDisplayMode)DISPCNT.DisplayMode : (GPUDisplayMode)(DISPCNT.DisplayMode & 0x01);
	
	this->_WIN0_ENABLED	= (DISPCNT.Win0_Enable != 0);
	this->_WIN1_ENABLED	= (DISPCNT.Win1_Enable != 0);
	this->_WINOBJ_ENABLED = (DISPCNT.WinOBJ_Enable != 0);
	this->_isAnyWindowEnabled = (this->_WIN0_ENABLED || this->_WIN1_ENABLED || this->_WINOBJ_ENABLED);
	
	if (DISPCNT.OBJ_Tile_mapping)
	{
		//1-d sprite mapping boundaries:
		//32k, 64k, 128k, 256k
		this->_sprBoundary = 5 + DISPCNT.OBJ_Tile_1D_Bound;
		
		//do not be deceived: even though a sprBoundary==8 (256KB region) is impossible to fully address
		//in GPU_SUB, it is still fully legal to address it with that granularity.
		//so don't do this: //if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3)) gpu->sprBoundary = 7;

		this->_spriteRenderMode = SpriteRenderMode_Sprite1D;
	}
	else
	{
		//2d sprite mapping
		//boundary : 32k
		this->_sprBoundary = 5;
		this->_spriteRenderMode = SpriteRenderMode_Sprite2D;
	}
     
	if (DISPCNT.OBJ_BMP_1D_Bound && (ENGINEID == GPUEngineID_Main))
		this->_sprBMPBoundary = 8;
	else
		this->_sprBMPBoundary = 7;
	
	this->ParseReg_BGnCNT<ENGINEID, GPULayerID_BG3>();
	this->ParseReg_BGnCNT<ENGINEID, GPULayerID_BG2>();
	this->ParseReg_BGnCNT<ENGINEID, GPULayerID_BG1>();
	this->ParseReg_BGnCNT<ENGINEID, GPULayerID_BG0>();
}

template <GPUEngineID ENGINEID, GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_BGnCNT &BGnCNT = this->_IORegisterMap->BGnCNT[LAYERID];
	this->_BGLayer[LAYERID].BGnCNT = BGnCNT;
	
	switch (LAYERID)
	{
		case GPULayerID_BG0: this->_BGLayer[LAYERID].isVisible = (DISPCNT.BG0_Enable != 0); break;
		case GPULayerID_BG1: this->_BGLayer[LAYERID].isVisible = (DISPCNT.BG1_Enable != 0); break;
		case GPULayerID_BG2: this->_BGLayer[LAYERID].isVisible = (DISPCNT.BG2_Enable != 0); break;
		case GPULayerID_BG3: this->_BGLayer[LAYERID].isVisible = (DISPCNT.BG3_Enable != 0); break;
			
		default:
			break;
	}
	
	if (ENGINEID == GPUEngineID_Main)
	{
		this->_BGLayer[LAYERID].largeBMPAddress  = MMU_ABG;
		this->_BGLayer[LAYERID].BMPAddress       = MMU_ABG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_16KB);
		this->_BGLayer[LAYERID].tileMapAddress   = MMU_ABG + (DISPCNT.ScreenBase_Block * ADDRESS_STEP_64KB) + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_2KB);
		this->_BGLayer[LAYERID].tileEntryAddress = MMU_ABG + (DISPCNT.CharacBase_Block * ADDRESS_STEP_64KB) + (BGnCNT.CharacBase_Block * ADDRESS_STEP_16KB);
	}
	else
	{
		this->_BGLayer[LAYERID].largeBMPAddress  = MMU_BBG;
		this->_BGLayer[LAYERID].BMPAddress       = MMU_BBG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_16KB);
		this->_BGLayer[LAYERID].tileMapAddress   = MMU_BBG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_2KB);
		this->_BGLayer[LAYERID].tileEntryAddress = MMU_BBG + (BGnCNT.CharacBase_Block * ADDRESS_STEP_16KB);
	}
	
	//clarify affine ext modes
	BGType mode = GPUEngineBase::_mode2type[DISPCNT.BG_Mode][LAYERID];
	this->_BGLayer[LAYERID].baseType = mode;
	
	if (mode == BGType_AffineExt)
	{
		//see: http://nocash.emubase.de/gbatek.htm#dsvideobgmodescontrol
		const u8 affineModeSelection = (BGnCNT.PaletteMode << 1) | (BGnCNT.CharacBase_Block & 1);
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
	
	// Extended palette slots can be changed for BG0 and BG1, but BG2 and BG3 remain constant.
	// Display wrapping can be changed for BG2 and BG3, but BG0 and BG1 cannot wrap.
	if (LAYERID == GPULayerID_BG0 || LAYERID == GPULayerID_BG1)
	{
		this->_BGLayer[LAYERID].extPaletteSlot = (BGnCNT.PaletteSet_Wrap * 2) + LAYERID;
	}
	else
	{
		this->_BGLayer[LAYERID].isDisplayWrapped = (BGnCNT.PaletteSet_Wrap != 0);
	}
	
	this->_BGLayer[LAYERID].type = mode;
	this->_BGLayer[LAYERID].size = GPUEngineBase::_BGLayerSizeLUT[mode][BGnCNT.ScreenSize];
	this->_BGLayer[LAYERID].isMosaic = (BGnCNT.Mosaic != 0);
	this->_BGLayer[LAYERID].priority = BGnCNT.Priority;
	this->_BGLayer[LAYERID].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][this->_BGLayer[LAYERID].extPaletteSlot];
	
	this->_ResortBGLayers();
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnHOFS()
{
	const IOREG_BGnHOFS &BGnHOFS = this->_IORegisterMap->BGnOFS[LAYERID].BGnHOFS;
	this->_BGLayer[LAYERID].BGnHOFS = BGnHOFS;
	
#ifdef LOCAL_LE
	this->_BGLayer[LAYERID].xOffset = BGnHOFS.Offset;
#else
	this->_BGLayer[LAYERID].xOffset = LOCAL_TO_LE_16(BGnHOFS.value) & 0x01FF;
#endif
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnVOFS()
{
	const IOREG_BGnVOFS &BGnVOFS = this->_IORegisterMap->BGnOFS[LAYERID].BGnVOFS;
	this->_BGLayer[LAYERID].BGnVOFS = BGnVOFS;
	
#ifdef LOCAL_LE
	this->_BGLayer[LAYERID].yOffset = BGnVOFS.Offset;
#else
	this->_BGLayer[LAYERID].yOffset = LOCAL_TO_LE_16(BGnVOFS.value) & 0x01FF;
#endif
}

template<GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnX()
{
	if (LAYERID == GPULayerID_BG2)
	{
		this->savedBG2X = this->_IORegisterMap->BG2X;
	}
	else if (LAYERID == GPULayerID_BG3)
	{
		this->savedBG3X = this->_IORegisterMap->BG3X;
	}
}

template<GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnY()
{
	if (LAYERID == GPULayerID_BG2)
	{
		this->savedBG2Y = this->_IORegisterMap->BG2Y;
	}
	else if (LAYERID == GPULayerID_BG3)
	{
		this->savedBG3Y = this->_IORegisterMap->BG3Y;
	}
}

void GPUEngineBase::_RenderLine_Clear(const u16 clearColor, const u16 l, u16 *renderLineTarget)
{
	// Clear the current line with the clear color
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	
	u16 dstClearColor = clearColor;
	
	if (BLDCNT.Backdrop_Target1 != 0)
	{
		if (BLDCNT.ColorEffect == ColorEffect_IncreaseBrightness)
		{
			dstClearColor = this->_currentFadeInColors[clearColor];
		}
		else if (BLDCNT.ColorEffect == ColorEffect_DecreaseBrightness)
		{
			dstClearColor = this->_currentFadeOutColors[clearColor];
		}
	}
	
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(renderLineTarget, dstClearColor);
	memset(this->_renderLineLayerIDNative, GPULayerID_Backdrop, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	// init background color & priorities
	memset(this->_sprAlpha, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprType, OBJMode_Normal, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprPrio, 0x7F, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprWin, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	// init pixels priorities
	assert(NB_PRIORITIES == 4);
	this->_itemsForPriority[0].nbPixelsX = 0;
	this->_itemsForPriority[1].nbPixelsX = 0;
	this->_itemsForPriority[2].nbPixelsX = 0;
	this->_itemsForPriority[3].nbPixelsX = 0;
}

void GPUEngineBase::RenderLine(const u16 l)
{
	// By default, do nothing.
	this->UpdatePropertiesWithoutRender(l);
}

void GPUEngineBase::UpdatePropertiesWithoutRender(const u16 l)
{
	// Update BG2/BG3 parameters for Affine and AffineExt modes
	if (  this->_enableLayer[GPULayerID_BG2] &&
		((this->_BGLayer[GPULayerID_BG2].baseType == BGType_Affine) || (this->_BGLayer[GPULayerID_BG2].baseType == BGType_AffineExt)) )
	{
		IOREG_BG2Parameter &BG2Param = this->_IORegisterMap->BG2Param;
		
		BG2Param.BG2X.value += BG2Param.BG2PB.value;
		BG2Param.BG2Y.value += BG2Param.BG2PD.value;
	}
	
	if (  this->_enableLayer[GPULayerID_BG3] &&
		((this->_BGLayer[GPULayerID_BG3].baseType == BGType_Affine) || (this->_BGLayer[GPULayerID_BG3].baseType == BGType_AffineExt)) )
	{
		IOREG_BG3Parameter &BG3Param = this->_IORegisterMap->BG3Param;
		
		BG3Param.BG3X.value += BG3Param.BG3PB.value;
		BG3Param.BG3Y.value += BG3Param.BG3PD.value;
	}
}

void GPUEngineBase::FramebufferPostprocess()
{
	this->RefreshAffineStartRegs();
}

const GPU_IOREG& GPUEngineBase::GetIORegisterMap() const
{
	return *this->_IORegisterMap;
}

bool GPUEngineBase::GetIsMasterBrightFullIntensity() const
{
	return this->_isMasterBrightFullIntensity;
}

/*****************************************************************************/
//			ENABLING / DISABLING LAYERS
/*****************************************************************************/

bool GPUEngineBase::GetEnableState()
{
	return CommonSettings.showGpu.screens[this->_engineID];
}

void GPUEngineBase::SetEnableState(bool theState)
{
	CommonSettings.showGpu.screens[this->_engineID] = theState;
}

bool GPUEngineBase::GetLayerEnableState(const size_t layerIndex)
{
	return CommonSettings.dispLayers[this->_engineID][layerIndex];
}

void GPUEngineBase::SetLayerEnableState(const size_t layerIndex, bool theState)
{
	CommonSettings.dispLayers[this->_engineID][layerIndex] = theState;
	this->_ResortBGLayers();
}

template <bool NATIVEDST, bool NATIVESRC, bool USELINEINDEX, bool NEEDENDIANSWAP>
void GPUEngineBase::_LineColorCopy(u16 *__restrict dstBuffer, const u16 *__restrict srcBuffer, const size_t l)
{
	if (NATIVEDST && NATIVESRC)
	{
		u16 *__restrict dst = (USELINEINDEX) ? dstBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : dstBuffer;
		const u16 *__restrict src = (USELINEINDEX) ? srcBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : srcBuffer;
		
#if defined(ENABLE_SSE2)
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / sizeof(u16)), _mm_stream_si128((__m128i *)dst + (X), _mm_load_si128((__m128i *)src + (X))) );
#elif LOCAL_LE
		memcpy(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
#else
		if (NEEDENDIANSWAP)
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			{
				dst[i] = LE_TO_LOCAL_16(src[i]);
			}
		}
		else
		{
			memcpy(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
		}
#endif
	}
	else if (!NATIVEDST && !NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineIndex = _gpuCaptureLineIndex[l];
		const size_t lineCount = _gpuCaptureLineCount[l];
		
		u16 *__restrict dst = (USELINEINDEX) ? dstBuffer + (lineIndex * lineWidth) : dstBuffer;
		const u16 *__restrict src = (USELINEINDEX) ? srcBuffer + (lineIndex * lineWidth) : srcBuffer;
		
#if defined(LOCAL_LE)
		memcpy(dst, src, lineWidth * lineCount * sizeof(u16));
#else
		if (NEEDENDIANSWAP)
		{
			for (size_t i = 0; i < lineWidth * lineCount; i++)
			{
				dst[i] = LE_TO_LOCAL_16(src[i]);
			}
		}
		else
		{
			memcpy(dst, src, lineWidth * lineCount * sizeof(u16));
		}
#endif
	}
	else if (NATIVEDST && !NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineIndex = _gpuCaptureLineIndex[l];
		
		u16 *__restrict dst = (USELINEINDEX) ? dstBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : dstBuffer;
		const u16 *__restrict src = (USELINEINDEX) ? srcBuffer + (lineIndex * lineWidth) : srcBuffer;
		
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			dst[i] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16(src[_gpuDstPitchIndex[i]]) : src[_gpuDstPitchIndex[i]];
		}
	}
	else // (!NATIVEDST && NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineIndex = _gpuCaptureLineIndex[l];
		const size_t lineCount = _gpuCaptureLineCount[l];
		
		u16 *dstLinePtr = (USELINEINDEX) ? dstBuffer + (lineIndex * lineWidth) : dstBuffer;
		u16 *dst = dstLinePtr;
		const u16 *src = (USELINEINDEX) ? srcBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : srcBuffer;
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				dst[_gpuDstPitchIndex[x] + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16(src[x]) : src[x];
			}
		}
		
		dst = dstLinePtr + lineWidth;
		
		for (size_t line = 1; line < lineCount; line++)
		{
			memcpy(dst, dstLinePtr, lineWidth * sizeof(u16));
			dst += lineWidth;
		}
	}
}

template <bool NATIVEDST, bool NATIVESRC>
void GPUEngineBase::_LineLayerIDCopy(u8 *__restrict dstBuffer, const u8 *__restrict srcBuffer, const size_t l)
{
	if (NATIVEDST && NATIVESRC)
	{
#if defined(ENABLE_SSE2)
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / sizeof(u8)), _mm_stream_si128((__m128i *)dstBuffer + (X), _mm_load_si128((__m128i *)srcBuffer + (X))) );
#else
		memcpy(dstBuffer, srcBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
#endif
	}
	else if (!NATIVEDST && !NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineCount = _gpuDstLineCount[l];
		
		memcpy(dstBuffer, srcBuffer, lineWidth * lineCount * sizeof(u8));
	}
	else if (NATIVEDST && !NATIVESRC)
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			dstBuffer[i] = srcBuffer[_gpuDstPitchIndex[i]];
		}
	}
	else // (!NATIVEDST && NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineCount = _gpuDstLineCount[l];
		
		u8 *dstLinePtr = dstBuffer;
		u8 *dst = dstLinePtr;
		const u8 *src = srcBuffer;
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				dst[_gpuDstPitchIndex[x] + p] = src[x];
			}
		}
		
		dst = dstLinePtr + lineWidth;
		
		for (size_t line = 1; line < lineCount; line++)
		{
			memcpy(dst, dstLinePtr, lineWidth * sizeof(u8));
			dst += lineWidth;
		}
	}
}

/*****************************************************************************/
//		ROUTINES FOR INSIDE / OUTSIDE WINDOW CHECKS
/*****************************************************************************/

template <GPULayerID LAYERID>
FORCEINLINE void GPUEngineBase::_RenderPixel_CheckWindows(const size_t srcX, bool &didPassWindowTest, bool &enableColorEffect) const
{
	// If no windows are enabled, then we don't need to perform any window tests.
	// In this case, the pixel always passes and the color effect is always processed.
	if (!this->_isAnyWindowEnabled)
	{
		didPassWindowTest = true;
		enableColorEffect = true;
		return;
	}
	
	// Window 0 has the highest priority, so always check this first.
	if (this->_WIN0_ENABLED)
	{
		if (this->_curr_win[0][srcX] == 1)
		{
			//INFO("bg%i passed win0 : (%i %i) was within (%i %i)(%i %i)\n", bgnum, x, gpu->_currentScanline, gpu->WIN0H0, gpu->WIN0V0, gpu->WIN0H1, gpu->WIN0V1);
			switch (LAYERID)
			{
				case GPULayerID_BG0: didPassWindowTest = (this->_IORegisterMap->WIN0IN.BG0_Enable != 0); break;
				case GPULayerID_BG1: didPassWindowTest = (this->_IORegisterMap->WIN0IN.BG1_Enable != 0); break;
				case GPULayerID_BG2: didPassWindowTest = (this->_IORegisterMap->WIN0IN.BG2_Enable != 0); break;
				case GPULayerID_BG3: didPassWindowTest = (this->_IORegisterMap->WIN0IN.BG3_Enable != 0); break;
				case GPULayerID_OBJ: didPassWindowTest = (this->_IORegisterMap->WIN0IN.OBJ_Enable != 0); break;
					
				default:
					break;
			}
			
			enableColorEffect = (this->_IORegisterMap->WIN0IN.Effect_Enable != 0);
			return;
		}
	}

	// Window 1 has medium priority, and is checked after Window 0.
	if (this->_WIN1_ENABLED)
	{
		if (this->_curr_win[1][srcX] == 1)
		{
			//INFO("bg%i passed win1 : (%i %i) was within (%i %i)(%i %i)\n", bgnum, x, gpu->_currentScanline, gpu->WIN1H0, gpu->WIN1V0, gpu->WIN1H1, gpu->WIN1V1);
			switch (LAYERID)
			{
				case GPULayerID_BG0: didPassWindowTest = (this->_IORegisterMap->WIN1IN.BG0_Enable != 0); break;
				case GPULayerID_BG1: didPassWindowTest = (this->_IORegisterMap->WIN1IN.BG1_Enable != 0); break;
				case GPULayerID_BG2: didPassWindowTest = (this->_IORegisterMap->WIN1IN.BG2_Enable != 0); break;
				case GPULayerID_BG3: didPassWindowTest = (this->_IORegisterMap->WIN1IN.BG3_Enable != 0); break;
				case GPULayerID_OBJ: didPassWindowTest = (this->_IORegisterMap->WIN1IN.OBJ_Enable != 0); break;
					
				default:
					break;
			}
			
			enableColorEffect = (this->_IORegisterMap->WIN1IN.Effect_Enable != 0);
			return;
		}
	}
	
	// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
	if (this->_WINOBJ_ENABLED)
	{
		if (this->_sprWin[srcX] == 1)
		{
			switch (LAYERID)
			{
				case GPULayerID_BG0: didPassWindowTest = (this->_IORegisterMap->WINOBJ.BG0_Enable != 0); break;
				case GPULayerID_BG1: didPassWindowTest = (this->_IORegisterMap->WINOBJ.BG1_Enable != 0); break;
				case GPULayerID_BG2: didPassWindowTest = (this->_IORegisterMap->WINOBJ.BG2_Enable != 0); break;
				case GPULayerID_BG3: didPassWindowTest = (this->_IORegisterMap->WINOBJ.BG3_Enable != 0); break;
				case GPULayerID_OBJ: didPassWindowTest = (this->_IORegisterMap->WINOBJ.OBJ_Enable != 0); break;
					
				default:
					break;
			}
			
			enableColorEffect = (this->_IORegisterMap->WINOBJ.Effect_Enable != 0);
			return;
		}
	}
	
	// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
	// This has the lowest priority, and is always checked last.
	switch (LAYERID)
	{
		case GPULayerID_BG0: didPassWindowTest = (this->_IORegisterMap->WINOUT.BG0_Enable != 0); break;
		case GPULayerID_BG1: didPassWindowTest = (this->_IORegisterMap->WINOUT.BG1_Enable != 0); break;
		case GPULayerID_BG2: didPassWindowTest = (this->_IORegisterMap->WINOUT.BG2_Enable != 0); break;
		case GPULayerID_BG3: didPassWindowTest = (this->_IORegisterMap->WINOUT.BG3_Enable != 0); break;
		case GPULayerID_OBJ: didPassWindowTest = (this->_IORegisterMap->WINOUT.OBJ_Enable != 0); break;
			
		default:
			break;
	}
	
	enableColorEffect = (this->_IORegisterMap->WINOUT.Effect_Enable != 0);
}

#ifdef ENABLE_SSE2

template <GPULayerID LAYERID, bool ISCUSTOMRENDERINGNEEDED>
FORCEINLINE void GPUEngineBase::_RenderPixel_CheckWindows16_SSE2(const size_t dstX, __m128i &didPassWindowTest, __m128i &enableColorEffect) const
{
	// If no windows are enabled, then we don't need to perform any window tests.
	// In this case, the pixel always passes and the color effect is always processed.
	if (!this->_isAnyWindowEnabled)
	{
		didPassWindowTest = _mm_set1_epi8(1);
		enableColorEffect = _mm_set1_epi8(1);
		return;
	}
	
	u8 didPassValue;
	__m128i win_vec128;
	
	__m128i win0HandledMask = _mm_setzero_si128();
	__m128i win1HandledMask = _mm_setzero_si128();
	__m128i winOBJHandledMask = _mm_setzero_si128();
	__m128i winOUTHandledMask = _mm_setzero_si128();
	
	// Window 0 has the highest priority, so always check this first.
	if (this->_WIN0_ENABLED)
	{
		switch (LAYERID)
		{
			case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WIN0IN.BG0_Enable; break;
			case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WIN0IN.BG1_Enable; break;
			case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WIN0IN.BG2_Enable; break;
			case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WIN0IN.BG3_Enable; break;
			case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WIN0IN.OBJ_Enable; break;
				
			default:
				didPassValue = 1;
				break;
		}
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			win_vec128 = _mm_set_epi8(this->_curr_win[0][_gpuDstToSrcIndex[dstX+15]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+14]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+13]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+12]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+11]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+10]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 9]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 8]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 7]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 6]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 5]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 4]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 3]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 2]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 1]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+ 0]]);
		}
		else
		{
			win_vec128 = _mm_loadu_si128((__m128i *)(this->_curr_win[0] + dstX));
		}
		
		win0HandledMask = _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1));
		didPassWindowTest = _mm_and_si128(win0HandledMask, _mm_set1_epi8(didPassValue));
		enableColorEffect = _mm_and_si128(win0HandledMask, _mm_set1_epi8(this->_IORegisterMap->WIN0IN.Effect_Enable));
	}
	
	// Window 1 has medium priority, and is checked after Window 0.
	if (this->_WIN1_ENABLED)
	{
		switch (LAYERID)
		{
			case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WIN1IN.BG0_Enable; break;
			case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WIN1IN.BG1_Enable; break;
			case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WIN1IN.BG2_Enable; break;
			case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WIN1IN.BG3_Enable; break;
			case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WIN1IN.OBJ_Enable; break;
				
			default:
				didPassValue = 1;
				break;
		}
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			win_vec128 = _mm_set_epi8(this->_curr_win[1][_gpuDstToSrcIndex[dstX+15]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+14]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+13]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+12]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+11]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+10]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 9]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 8]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 7]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 6]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 5]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 4]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 3]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 2]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 1]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+ 0]]);
		}
		else
		{
			win_vec128 = _mm_loadu_si128((__m128i *)(this->_curr_win[1] + dstX));
		}
		
		win1HandledMask = _mm_andnot_si128( win0HandledMask, _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)) );
		didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(win1HandledMask, _mm_set1_epi8(didPassValue)) );
		enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(win1HandledMask, _mm_set1_epi8(this->_IORegisterMap->WIN1IN.Effect_Enable)) );
	}
	
	// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
	if (this->_WINOBJ_ENABLED)
	{
		switch (LAYERID)
		{
			case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WINOBJ.BG0_Enable; break;
			case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WINOBJ.BG1_Enable; break;
			case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WINOBJ.BG2_Enable; break;
			case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WINOBJ.BG3_Enable; break;
			case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WINOBJ.OBJ_Enable; break;
				
			default:
				didPassValue = 1;
				break;
		}
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			win_vec128 = _mm_set_epi8(this->_sprWin[_gpuDstToSrcIndex[dstX+15]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+14]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+13]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+12]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+11]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+10]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 9]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 8]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 7]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 6]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 5]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 4]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 3]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 2]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 1]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+ 0]]);
		}
		else
		{
			win_vec128 = _mm_loadu_si128((__m128i *)(this->_sprWin + dstX));
		}
		
		winOBJHandledMask = _mm_andnot_si128( _mm_or_si128(win0HandledMask, win1HandledMask), _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)) );
		didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOBJHandledMask, _mm_set1_epi8(didPassValue)) );
		enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOBJHandledMask, _mm_set1_epi8(this->_IORegisterMap->WINOBJ.Effect_Enable)) );
	}
	
	// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
	// This has the lowest priority, and is always checked last.
	switch (LAYERID)
	{
		case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WINOUT.BG0_Enable; break;
		case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WINOUT.BG1_Enable; break;
		case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WINOUT.BG2_Enable; break;
		case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WINOUT.BG3_Enable; break;
		case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WINOUT.OBJ_Enable; break;
			
		default:
			break;
	}
	
	winOUTHandledMask = _mm_xor_si128( _mm_or_si128(win0HandledMask, _mm_or_si128(win1HandledMask, winOBJHandledMask)), _mm_set1_epi32(0xFFFFFFFF) );
	didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOUTHandledMask, _mm_set1_epi8(didPassValue)) );
	enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOUTHandledMask, _mm_set1_epi8(this->_IORegisterMap->WINOUT.Effect_Enable)) );
}

template <GPULayerID LAYERID, bool ISCUSTOMRENDERINGNEEDED>
FORCEINLINE void GPUEngineBase::_RenderPixel_CheckWindows8_SSE2(const size_t dstX, __m128i &didPassWindowTest, __m128i &enableColorEffect) const
{
	// If no windows are enabled, then we don't need to perform any window tests.
	// In this case, the pixel always passes and the color effect is always processed.
	if (!this->_isAnyWindowEnabled)
	{
		didPassWindowTest = _mm_set_epi32(0, 0, 0x01010101, 0x01010101);
		enableColorEffect = _mm_set_epi32(0, 0, 0x01010101, 0x01010101);
		return;
	}
	
	u8 didPassValue;
	__m128i win_vec128;
	
	__m128i win0HandledMask = _mm_setzero_si128();
	__m128i win1HandledMask = _mm_setzero_si128();
	__m128i winOBJHandledMask = _mm_setzero_si128();
	__m128i winOUTHandledMask = _mm_setzero_si128();
	
	// Window 0 has the highest priority, so always check this first.
	if (this->_WIN0_ENABLED)
	{
		switch (LAYERID)
		{
			case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WIN0IN.BG0_Enable; break;
			case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WIN0IN.BG1_Enable; break;
			case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WIN0IN.BG2_Enable; break;
			case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WIN0IN.BG3_Enable; break;
			case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WIN0IN.OBJ_Enable; break;
				
			default:
				didPassValue = 1;
				break;
		}
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			win_vec128 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+7]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+6]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+5]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+4]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+3]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+2]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+1]],
									  this->_curr_win[0][_gpuDstToSrcIndex[dstX+0]]);
		}
		else
		{
			win_vec128 = _mm_loadl_epi64((__m128i *)(this->_curr_win[0] + dstX));
		}
		
		win0HandledMask = _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1));
		didPassWindowTest = _mm_and_si128(win0HandledMask, _mm_set1_epi8(didPassValue));
		enableColorEffect = _mm_and_si128(win0HandledMask, _mm_set1_epi8(this->_IORegisterMap->WIN0IN.Effect_Enable));
	}
	
	// Window 1 has medium priority, and is checked after Window 0.
	if (this->_WIN1_ENABLED)
	{
		switch (LAYERID)
		{
			case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WIN1IN.BG0_Enable; break;
			case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WIN1IN.BG1_Enable; break;
			case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WIN1IN.BG2_Enable; break;
			case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WIN1IN.BG3_Enable; break;
			case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WIN1IN.OBJ_Enable; break;
				
			default:
				didPassValue = 1;
				break;
		}
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			win_vec128 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+7]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+6]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+5]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+4]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+3]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+2]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+1]],
									  this->_curr_win[1][_gpuDstToSrcIndex[dstX+0]]);
		}
		else
		{
			win_vec128 = _mm_loadl_epi64((__m128i *)(this->_curr_win[1] + dstX));
		}
		
		win1HandledMask = _mm_andnot_si128( win0HandledMask, _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)) );
		didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(win1HandledMask, _mm_set1_epi8(didPassValue)) );
		enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(win1HandledMask, _mm_set1_epi8(this->_IORegisterMap->WIN1IN.Effect_Enable)) );
	}
	
	// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
	if (this->_WINOBJ_ENABLED)
	{
		switch (LAYERID)
		{
			case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WINOBJ.BG0_Enable; break;
			case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WINOBJ.BG1_Enable; break;
			case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WINOBJ.BG2_Enable; break;
			case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WINOBJ.BG3_Enable; break;
			case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WINOBJ.OBJ_Enable; break;
				
			default:
				didPassValue = 1;
				break;
		}
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			win_vec128 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0,
									  this->_sprWin[_gpuDstToSrcIndex[dstX+7]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+6]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+5]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+4]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+3]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+2]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+1]],
									  this->_sprWin[_gpuDstToSrcIndex[dstX+0]]);
		}
		else
		{
			win_vec128 = _mm_loadl_epi64((__m128i *)(this->_sprWin + dstX));
		}
		
		winOBJHandledMask = _mm_andnot_si128( _mm_or_si128(win0HandledMask, win1HandledMask), _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)) );
		didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOBJHandledMask, _mm_set1_epi8(didPassValue)) );
		enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOBJHandledMask, _mm_set1_epi8(this->_IORegisterMap->WINOBJ.Effect_Enable)) );
	}
	
	// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
	// This has the lowest priority, and is always checked last.
	switch (LAYERID)
	{
		case GPULayerID_BG0: didPassValue = this->_IORegisterMap->WINOUT.BG0_Enable; break;
		case GPULayerID_BG1: didPassValue = this->_IORegisterMap->WINOUT.BG1_Enable; break;
		case GPULayerID_BG2: didPassValue = this->_IORegisterMap->WINOUT.BG2_Enable; break;
		case GPULayerID_BG3: didPassValue = this->_IORegisterMap->WINOUT.BG3_Enable; break;
		case GPULayerID_OBJ: didPassValue = this->_IORegisterMap->WINOUT.OBJ_Enable; break;
			
		default:
			break;
	}
	
	winOUTHandledMask = _mm_and_si128( _mm_set_epi32(0, 0, 0xFFFFFFFF, 0xFFFFFFFF), _mm_xor_si128(_mm_or_si128(win0HandledMask, _mm_or_si128(win1HandledMask, winOBJHandledMask)), _mm_set1_epi32(0xFFFFFFFF)) );
	didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOUTHandledMask, _mm_set1_epi8(didPassValue)) );
	enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOUTHandledMask, _mm_set1_epi8(this->_IORegisterMap->WINOUT.Effect_Enable)) );
}

#endif

/*****************************************************************************/
//			PIXEL RENDERING
/*****************************************************************************/
template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT>
FORCEINLINE void GPUEngineBase::_RenderPixel(const size_t srcX, const u16 src, const u8 srcAlpha, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine)
{
	if (ISDEBUGRENDER)
	{
		// If we're rendering pixels to a debugging context, then assume that the pixel
		// always passes the window test and that the color effect is always disabled.
		*dstColorLine = src | 0x8000;
		*dstLayerIDLine = LAYERID;
		return;
	}
	
	bool enableColorEffect = true;
	
	if (!NOWINDOWSENABLEDHINT)
	{
		bool didPassWindowTest = true;
		this->_RenderPixel_CheckWindows<LAYERID>(srcX, didPassWindowTest, enableColorEffect);
		
		if (!didPassWindowTest)
		{
			return;
		}
	}
	
	if ((LAYERID != GPULayerID_OBJ) && COLOREFFECTDISABLEDHINT)
	{
		*dstColorLine = src | 0x8000;
		*dstLayerIDLine = LAYERID;
		return;
	}
	
	ColorEffect selectedEffect = ColorEffect_Disable;
	TBlendTable *selectedBlendTable = this->_blendTable;
	
	if (enableColorEffect)
	{
		const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
		const GPULayerID dstLayerID = (GPULayerID)*dstLayerIDLine;
		bool srcEffectEnable = false;
		const bool dstEffectEnable = (dstLayerID != LAYERID) && this->_blend2[dstLayerID];
		
		switch (LAYERID)
		{
			case GPULayerID_BG0:
				srcEffectEnable = (BLDCNT.BG0_Target1 != 0);
				break;
				
			case GPULayerID_BG1:
				srcEffectEnable = (BLDCNT.BG1_Target1 != 0);
				break;
				
			case GPULayerID_BG2:
				srcEffectEnable = (BLDCNT.BG2_Target1 != 0);
				break;
				
			case GPULayerID_BG3:
				srcEffectEnable = (BLDCNT.BG3_Target1 != 0);
				break;
				
			case GPULayerID_OBJ:
				srcEffectEnable = (BLDCNT.OBJ_Target1 != 0);
				break;
				
			default:
				break;
		}
		
		// Select the color effect based on the BLDCNT target flags.
		bool forceBlendEffect = false;
		
		if (LAYERID == GPULayerID_OBJ)
		{
			//translucent-capable OBJ are forcing the function to blend when the second target is satisfied
			const OBJMode objMode = (OBJMode)this->_sprType[srcX];
			const bool isObjTranslucentType = (objMode == OBJMode_Transparent) || (objMode == OBJMode_Bitmap);
			if (isObjTranslucentType && dstEffectEnable)
			{
				//obj without fine-grained alpha are using EVA/EVB for blending. this is signified by receiving 0xFF in the alpha
				//it's tested by the spriteblend demo and the glory of heracles title screen
				if (srcAlpha != 0xFF)
				{
					const u8 BLDALPHA_EVA = srcAlpha;
					const u8 BLDALPHA_EVB = 16 - srcAlpha;
					selectedBlendTable = &GPUEngineBase::_blendTable555[BLDALPHA_EVA][BLDALPHA_EVB];
				}
				
				forceBlendEffect = true;
			}
		}
		
		if (forceBlendEffect)
		{
			selectedEffect = ColorEffect_Blend;
		}
		else if (srcEffectEnable)
		{
			switch ((ColorEffect)BLDCNT.ColorEffect)
			{
				// For the Blend effect, both first and second target flags must be checked.
				case ColorEffect_Blend:
				{
					if (dstEffectEnable) selectedEffect = (ColorEffect)BLDCNT.ColorEffect;
					break;
				}
					
				// For the Increase/Decrease Brightness effects, only the first target flag needs to be checked.
				// Test case: Bomberman Land Touch! dialog boxes will render too dark without this check.
				case ColorEffect_IncreaseBrightness:
				case ColorEffect_DecreaseBrightness:
					selectedEffect = (ColorEffect)BLDCNT.ColorEffect;
					break;
					
				default:
					break;
			}
		}
	}
	
	// Render the pixel using the selected color effect.
	u16 finalDstColor;
	
	switch (selectedEffect)
	{
		case ColorEffect_Disable:
			finalDstColor = src;
			break;
			
		case ColorEffect_IncreaseBrightness:
			finalDstColor = this->_ColorEffectIncreaseBrightness(src & 0x7FFF);
			break;
			
		case ColorEffect_DecreaseBrightness:
			finalDstColor = this->_ColorEffectDecreaseBrightness(src & 0x7FFF);
			break;
			
		case ColorEffect_Blend:
			finalDstColor = this->_ColorEffectBlend(src, *dstColorLine, selectedBlendTable);
			break;
	}
	
	*dstColorLine = finalDstColor | 0x8000;
	*dstLayerIDLine = LAYERID;
}

#ifdef ENABLE_SSE2

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
FORCEINLINE void GPUEngineBase::_RenderPixel16_SSE2(const size_t dstX,
													const __m128i &srcColorHi_vec128,
													const __m128i &srcColorLo_vec128,
													const __m128i &srcOpaqueMask,
													const u8 *__restrict srcAlpha,
													u16 *__restrict dstColorLine,
													u8 *__restrict dstLayerIDLine)
{
	const __m128i dstColorLo_vec128 = _mm_load_si128((__m128i *)dstColorLine);
	const __m128i dstColorHi_vec128 = _mm_load_si128((__m128i *)(dstColorLine + 8));
	const __m128i dstLayerID_vec128 = _mm_load_si128((__m128i *)dstLayerIDLine);
	
	const __m128i srcOpaqueMaskLo = _mm_cmpeq_epi16( _mm_unpacklo_epi8(srcOpaqueMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF));
	const __m128i srcOpaqueMaskHi = _mm_cmpeq_epi16( _mm_unpackhi_epi8(srcOpaqueMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF));
	
	if (ISDEBUGRENDER)
	{
		// If we're rendering pixels to a debugging context, then assume that the pixel
		// always passes the window test and that the color effect is always disabled.
		_mm_store_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(srcOpaqueMaskLo, _mm_or_si128(srcColorLo_vec128, _mm_set1_epi16(0x8000))), _mm_andnot_si128(srcOpaqueMaskLo, dstColorLo_vec128)) );
		_mm_store_si128( (__m128i *)(dstColorLine + 8), _mm_or_si128(_mm_and_si128(srcOpaqueMaskHi, _mm_or_si128(srcColorHi_vec128, _mm_set1_epi16(0x8000))), _mm_andnot_si128(srcOpaqueMaskHi, dstColorHi_vec128)) );
		_mm_store_si128( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(srcOpaqueMask, _mm_set1_epi8(LAYERID)), _mm_andnot_si128(srcOpaqueMask, dstLayerID_vec128)) );
		return;
	}
	
	__m128i passMaskLo = srcOpaqueMaskLo;
	__m128i passMaskHi = srcOpaqueMaskHi;
	__m128i passMask8 = srcOpaqueMask;
	__m128i enableColorEffect = _mm_set1_epi8(1);
	
	if (!NOWINDOWSENABLEDHINT)
	{
		// Do the window test.
		__m128i didPassWindowTest = _mm_set1_epi8(1);
		this->_RenderPixel_CheckWindows16_SSE2<LAYERID, ISCUSTOMRENDERINGNEEDED>(dstX, didPassWindowTest, enableColorEffect);
		
		passMaskLo = _mm_and_si128( passMaskLo, _mm_cmpeq_epi16(_mm_unpacklo_epi8(didPassWindowTest, _mm_setzero_si128()), _mm_set1_epi16(1)) );
		passMaskHi = _mm_and_si128( passMaskHi, _mm_cmpeq_epi16(_mm_unpackhi_epi8(didPassWindowTest, _mm_setzero_si128()), _mm_set1_epi16(1)) );
		passMask8 = _mm_and_si128( passMask8, _mm_packs_epi16(passMaskLo, passMaskHi) );
	}
	
	if ((LAYERID != GPULayerID_OBJ) && COLOREFFECTDISABLEDHINT)
	{
		_mm_store_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(passMaskLo, _mm_or_si128(srcColorLo_vec128, _mm_set1_epi16(0x8000))), _mm_andnot_si128(passMaskLo, dstColorLo_vec128)) );
		_mm_store_si128( (__m128i *)(dstColorLine + 8), _mm_or_si128(_mm_and_si128(passMaskHi, _mm_or_si128(srcColorHi_vec128, _mm_set1_epi16(0x8000))), _mm_andnot_si128(passMaskHi, dstColorHi_vec128)) );
		_mm_store_si128( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(passMask8, _mm_set1_epi8(LAYERID)), _mm_andnot_si128(passMask8, dstLayerID_vec128)) );
		return;
	}
	
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	u8 srcEffectEnableValue;
	
	switch (LAYERID)
	{
		case GPULayerID_BG0:
			srcEffectEnableValue = BLDCNT.BG0_Target1;
			break;
			
		case GPULayerID_BG1:
			srcEffectEnableValue = BLDCNT.BG1_Target1;
			break;
			
		case GPULayerID_BG2:
			srcEffectEnableValue = BLDCNT.BG2_Target1;
			break;
			
		case GPULayerID_BG3:
			srcEffectEnableValue = BLDCNT.BG3_Target1;
			break;
			
		case GPULayerID_OBJ:
			srcEffectEnableValue = BLDCNT.OBJ_Target1;
			break;
			
		default:
			srcEffectEnableValue = 0;
			break;
	}
	const __m128i srcEffectEnableMask = _mm_cmpeq_epi8(_mm_set1_epi8(srcEffectEnableValue), _mm_set1_epi8(1));
	
#ifdef ENABLE_SSSE3
	__m128i dstEffectEnableMask = _mm_shuffle_epi8(this->_blend2_SSSE3, dstLayerID_vec128);
#else
	__m128i dstEffectEnableMask = _mm_set_epi8(this->_blend2[dstLayerIDLine[15]],
											   this->_blend2[dstLayerIDLine[14]],
											   this->_blend2[dstLayerIDLine[13]],
											   this->_blend2[dstLayerIDLine[12]],
											   this->_blend2[dstLayerIDLine[11]],
											   this->_blend2[dstLayerIDLine[10]],
											   this->_blend2[dstLayerIDLine[ 9]],
											   this->_blend2[dstLayerIDLine[ 8]],
											   this->_blend2[dstLayerIDLine[ 7]],
											   this->_blend2[dstLayerIDLine[ 6]],
											   this->_blend2[dstLayerIDLine[ 5]],
											   this->_blend2[dstLayerIDLine[ 4]],
											   this->_blend2[dstLayerIDLine[ 3]],
											   this->_blend2[dstLayerIDLine[ 2]],
											   this->_blend2[dstLayerIDLine[ 1]],
											   this->_blend2[dstLayerIDLine[ 0]]);
#endif
	
	dstEffectEnableMask = _mm_and_si128(_mm_xor_si128(_mm_cmpeq_epi8(dstLayerID_vec128, _mm_set1_epi8(LAYERID)), _mm_set1_epi32(0xFFFFFFFF)),
										_mm_xor_si128(_mm_cmpeq_epi8(dstEffectEnableMask, _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF)) );
	
	// Select the color effect based on the BLDCNT target flags.
	__m128i forceBlendEffectMask = _mm_setzero_si128();
	__m128i colorEffect_vec128;
	
	if (NOWINDOWSENABLEDHINT)
	{
		colorEffect_vec128 = _mm_set1_epi8(BLDCNT.ColorEffect);
	}
	else
	{
		const __m128i enableColorEffectMask = _mm_cmpeq_epi8(enableColorEffect, _mm_set1_epi8(1));
		colorEffect_vec128 = _mm_or_si128( _mm_and_si128(enableColorEffectMask, _mm_set1_epi8(BLDCNT.ColorEffect)), _mm_andnot_si128(enableColorEffectMask, _mm_set1_epi8(ColorEffect_Disable)) );
	}
	
	__m128i eva_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVA);
	__m128i evb_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVB);
	const __m128i evy_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVY);
	
	if (LAYERID == GPULayerID_OBJ)
	{
		const __m128i objMode_vec128 = _mm_loadu_si128((__m128i *)(this->_sprType + dstX));
		const __m128i isObjTranslucentMask = _mm_and_si128( dstEffectEnableMask, _mm_or_si128(_mm_cmpeq_epi8(objMode_vec128, _mm_set1_epi8(OBJMode_Transparent)), _mm_cmpeq_epi8(objMode_vec128, _mm_set1_epi8(OBJMode_Bitmap))) );
		forceBlendEffectMask = isObjTranslucentMask;
		
		const __m128i srcAlpha_vec128 = _mm_loadu_si128((__m128i *)(srcAlpha + dstX));
		const __m128i srcAlphaMask = _mm_and_si128( isObjTranslucentMask, _mm_xor_si128(_mm_cmpeq_epi8(srcAlpha_vec128, _mm_set1_epi8(0xFF)), _mm_set1_epi32(0xFFFFFFFF)) );
		
		eva_vec128 = _mm_or_si128( _mm_and_si128(srcAlphaMask, srcAlpha_vec128), _mm_andnot_si128(srcAlphaMask, eva_vec128) );
		evb_vec128 = _mm_or_si128( _mm_and_si128(srcAlphaMask, _mm_sub_epi8(_mm_set1_epi8(16), srcAlpha_vec128)), _mm_andnot_si128(srcAlphaMask, evb_vec128) );
	}
	
	__m128i brightnessMask = _mm_setzero_si128();
	__m128i brightnessPixelsLo = _mm_setzero_si128();
	__m128i brightnessPixelsHi = _mm_setzero_si128();
	
	switch (BLDCNT.ColorEffect)
	{
		case ColorEffect_IncreaseBrightness:
			brightnessMask = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			brightnessPixelsLo = _mm_and_si128( this->_ColorEffectIncreaseBrightness(srcColorLo_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpacklo_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			brightnessPixelsHi = _mm_and_si128( this->_ColorEffectIncreaseBrightness(srcColorHi_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpackhi_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			break;
			
		case ColorEffect_DecreaseBrightness:
			brightnessMask = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			brightnessPixelsLo = _mm_and_si128( this->_ColorEffectDecreaseBrightness(srcColorLo_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpacklo_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			brightnessPixelsHi = _mm_and_si128( this->_ColorEffectDecreaseBrightness(srcColorHi_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpackhi_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			break;
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const __m128i blendMask = _mm_or_si128( forceBlendEffectMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstEffectEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const __m128i blendPixelsLo = _mm_and_si128( this->_ColorEffectBlend(srcColorLo_vec128, dstColorLo_vec128, eva_vec128, evb_vec128), _mm_cmpeq_epi16(_mm_unpacklo_epi8(blendMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	const __m128i blendPixelsHi = _mm_and_si128( this->_ColorEffectBlend(srcColorHi_vec128, dstColorHi_vec128, eva_vec128, evb_vec128), _mm_cmpeq_epi16(_mm_unpackhi_epi8(blendMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	
	const __m128i disableMask = _mm_xor_si128( _mm_or_si128(brightnessMask, blendMask), _mm_set1_epi32(0xFFFFFFFF) );
	const __m128i disablePixelsLo = _mm_and_si128( srcColorLo_vec128, _mm_cmpeq_epi16(_mm_unpacklo_epi8(disableMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	const __m128i disablePixelsHi = _mm_and_si128( srcColorHi_vec128, _mm_cmpeq_epi16(_mm_unpackhi_epi8(disableMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	
	// Combine the final colors.
	const __m128i combinedSrcColorLo_vec128 = _mm_or_si128( _mm_or_si128(_mm_or_si128(brightnessPixelsLo, blendPixelsLo), disablePixelsLo), _mm_set1_epi16(0x8000) );
	const __m128i combinedSrcColorHi_vec128 = _mm_or_si128( _mm_or_si128(_mm_or_si128(brightnessPixelsHi, blendPixelsHi), disablePixelsHi), _mm_set1_epi16(0x8000) );
	
	_mm_store_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(passMaskLo, combinedSrcColorLo_vec128), _mm_andnot_si128(passMaskLo, dstColorLo_vec128)) );
	_mm_store_si128( (__m128i *)(dstColorLine + 8), _mm_or_si128(_mm_and_si128(passMaskHi, combinedSrcColorHi_vec128), _mm_andnot_si128(passMaskHi, dstColorHi_vec128)) );
	_mm_store_si128( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(passMask8, _mm_set1_epi8(LAYERID)), _mm_andnot_si128(passMask8, dstLayerID_vec128)) );
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
FORCEINLINE void GPUEngineBase::_RenderPixel8_SSE2(const size_t dstX,
												   const __m128i &srcColor_vec128,
												   const __m128i &srcOpaqueMask,
												   const u8 *__restrict srcAlpha,
												   u16 *__restrict dstColorLine,
												   u8 *__restrict dstLayerIDLine)
{
	const __m128i dstColor_vec128 = _mm_loadu_si128((__m128i *)dstColorLine);
	const __m128i dstLayerID_vec128 = _mm_loadl_epi64((__m128i *)dstLayerIDLine);
	
	if (ISDEBUGRENDER)
	{
		// If we're rendering pixels to a debugging context, then assume that the pixel
		// always passes the window test and that the color effect is always disabled.
		const __m128i srcOpaqueMask16 = _mm_cmpeq_epi16( _mm_unpacklo_epi8(srcOpaqueMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF));
		_mm_storeu_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(srcOpaqueMask16, _mm_or_si128(srcColor_vec128, _mm_set1_epi16(0x8000))), _mm_andnot_si128(srcOpaqueMask16, dstColor_vec128)) );
		_mm_storel_epi64( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(srcOpaqueMask, _mm_set1_epi8(LAYERID)), _mm_andnot_si128(srcOpaqueMask, dstLayerID_vec128)) );
		return;
	}
	
	const __m128i srcOpaqueMask16 = _mm_cmpeq_epi16( _mm_unpacklo_epi8(srcOpaqueMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF));
	__m128i passMask16 = srcOpaqueMask16;
	__m128i passMask8 = srcOpaqueMask;
	__m128i enableColorEffect = _mm_set_epi32(0, 0, 0x01010101, 0x01010101);
	
	if (!NOWINDOWSENABLEDHINT)
	{
		// Do the window test.
		__m128i didPassWindowTest = _mm_set_epi32(0, 0, 0x01010101, 0x01010101);
		this->_RenderPixel_CheckWindows8_SSE2<LAYERID, ISCUSTOMRENDERINGNEEDED>(dstX, didPassWindowTest, enableColorEffect);
		
		passMask16 = _mm_and_si128( passMask16, _mm_cmpeq_epi16(_mm_unpacklo_epi8(didPassWindowTest, _mm_setzero_si128()), _mm_set1_epi16(1)) );
		passMask8 = _mm_and_si128( passMask8, _mm_cmpeq_epi8(didPassWindowTest, _mm_set1_epi8(1)) );
	}
	
	if ((LAYERID != GPULayerID_OBJ) && COLOREFFECTDISABLEDHINT)
	{
		_mm_storeu_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(passMask16, _mm_or_si128(srcColor_vec128, _mm_set1_epi16(0x8000))), _mm_andnot_si128(passMask16, dstColor_vec128)) );
		_mm_storel_epi64( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(passMask8, _mm_set1_epi8(LAYERID)), _mm_andnot_si128(passMask8, dstLayerID_vec128)) );
		return;
	}
	
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	u8 srcEffectEnableValue;
	
	switch (LAYERID)
	{
		case GPULayerID_BG0:
			srcEffectEnableValue = BLDCNT.BG0_Target1;
			break;
			
		case GPULayerID_BG1:
			srcEffectEnableValue = BLDCNT.BG1_Target1;
			break;
			
		case GPULayerID_BG2:
			srcEffectEnableValue = BLDCNT.BG2_Target1;
			break;
			
		case GPULayerID_BG3:
			srcEffectEnableValue = BLDCNT.BG3_Target1;
			break;
			
		case GPULayerID_OBJ:
			srcEffectEnableValue = BLDCNT.OBJ_Target1;
			break;
			
		default:
			srcEffectEnableValue = 0;
			break;
	}
	const __m128i srcEffectEnableMask = _mm_cmpeq_epi16(_mm_set1_epi16(srcEffectEnableValue), _mm_set1_epi16(1));
	
#ifdef ENABLE_SSSE3
	__m128i dstEffectEnableMask = _mm_unpacklo_epi8( _mm_shuffle_epi8(this->_blend2_SSSE3, dstLayerID_vec128), _mm_setzero_si128() );
#else
	__m128i dstEffectEnableMask = _mm_set_epi16(this->_blend2[dstLayerIDLine[7]],
												this->_blend2[dstLayerIDLine[6]],
												this->_blend2[dstLayerIDLine[5]],
												this->_blend2[dstLayerIDLine[4]],
												this->_blend2[dstLayerIDLine[3]],
												this->_blend2[dstLayerIDLine[2]],
												this->_blend2[dstLayerIDLine[1]],
												this->_blend2[dstLayerIDLine[0]]);
#endif
	
	dstEffectEnableMask = _mm_and_si128( _mm_xor_si128(_mm_cmpeq_epi16(_mm_unpacklo_epi8(dstLayerID_vec128, _mm_setzero_si128()), _mm_set1_epi16(LAYERID)), _mm_set1_epi32(0xFFFFFFFF)),
										 _mm_xor_si128(_mm_cmpeq_epi16(dstEffectEnableMask, _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF)) );
	
	// Select the color effect based on the BLDCNT target flags.
	__m128i forceBlendEffectMask = _mm_setzero_si128();
	__m128i colorEffect_vec128;
	
	if (NOWINDOWSENABLEDHINT)
	{
		colorEffect_vec128 = _mm_set1_epi16(BLDCNT.ColorEffect);
	}
	else
	{
		const __m128i enableColorEffectMask = _mm_cmpeq_epi16(_mm_unpacklo_epi8(enableColorEffect, _mm_setzero_si128()), _mm_set1_epi16(1));
		colorEffect_vec128 = _mm_or_si128( _mm_and_si128(enableColorEffectMask, _mm_set1_epi16(BLDCNT.ColorEffect)), _mm_andnot_si128(enableColorEffectMask, _mm_set1_epi16(ColorEffect_Disable)) );
	}
	
	__m128i eva_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVA);
	__m128i evb_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVB);
	const __m128i evy_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVY);
	
	if (LAYERID == GPULayerID_OBJ)
	{
		const __m128i objMode_vec128 = _mm_unpacklo_epi8( _mm_loadl_epi64((__m128i *)(this->_sprType + dstX)), _mm_setzero_si128() );
		const __m128i isObjTranslucentMask = _mm_and_si128( dstEffectEnableMask, _mm_or_si128(_mm_cmpeq_epi16(objMode_vec128, _mm_set1_epi16(OBJMode_Transparent)), _mm_cmpeq_epi16(objMode_vec128, _mm_set1_epi16(OBJMode_Bitmap))) );
		forceBlendEffectMask = isObjTranslucentMask;
		
		const __m128i srcAlpha_vec128 = _mm_unpacklo_epi8( _mm_loadl_epi64((__m128i *)(srcAlpha + dstX)), _mm_setzero_si128() );
		const __m128i srcAlphaMask = _mm_and_si128( isObjTranslucentMask, _mm_xor_si128(_mm_cmpeq_epi16(srcAlpha_vec128, _mm_set1_epi16(0x00FF)), _mm_set1_epi32(0xFFFFFFFF)) );
		
		eva_vec128 = _mm_or_si128( _mm_and_si128(srcAlphaMask, srcAlpha_vec128), _mm_andnot_si128(srcAlphaMask, eva_vec128) );
		evb_vec128 = _mm_or_si128( _mm_and_si128(srcAlphaMask, _mm_sub_epi16(_mm_set1_epi16(16), srcAlpha_vec128)), _mm_andnot_si128(srcAlphaMask, evb_vec128) );
	}
	
	__m128i brightnessMask = _mm_setzero_si128();
	__m128i brightnessPixels = _mm_setzero_si128();
	
	switch (BLDCNT.ColorEffect)
	{
		case ColorEffect_IncreaseBrightness:
			brightnessMask = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi16(colorEffect_vec128, _mm_set1_epi16(ColorEffect_IncreaseBrightness))) );
			brightnessPixels = _mm_and_si128( brightnessMask, this->_ColorEffectIncreaseBrightness(srcColor_vec128, evy_vec128) );
			break;
			
		case ColorEffect_DecreaseBrightness:
			brightnessMask = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi16(colorEffect_vec128, _mm_set1_epi16(ColorEffect_DecreaseBrightness))) );
			brightnessPixels = _mm_and_si128( brightnessMask, this->_ColorEffectDecreaseBrightness(srcColor_vec128, evy_vec128) );
			break;
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const __m128i blendMask = _mm_or_si128( forceBlendEffectMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstEffectEnableMask), _mm_cmpeq_epi16(colorEffect_vec128, _mm_set1_epi16(ColorEffect_Blend))) );
	const __m128i blendPixels = _mm_and_si128( blendMask, this->_ColorEffectBlend(srcColor_vec128, dstColor_vec128, eva_vec128, evb_vec128) );
	
	const __m128i disableMask = _mm_xor_si128( _mm_or_si128(brightnessMask, blendMask), _mm_set1_epi32(0xFFFFFFFF) );
	const __m128i disablePixels = _mm_and_si128(disableMask, srcColor_vec128);
	
	// Combine the final colors.
	const __m128i combinedSrcColorLo_vec128 = _mm_or_si128( _mm_or_si128(_mm_or_si128(brightnessPixels, blendPixels), disablePixels), _mm_set1_epi16(0x8000) );
	
	_mm_storeu_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(passMask16, combinedSrcColorLo_vec128), _mm_andnot_si128(passMask16, dstColor_vec128)) );
	_mm_storel_epi64( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(passMask8, _mm_set1_epi8(LAYERID)), _mm_andnot_si128(passMask8, dstLayerID_vec128)) );
}

#endif

// TODO: Unify this method with GPUEngineBase::_RenderPixel().
// We can't unify this yet because the output framebuffer is in RGBA5551, but the 3D source pixels are in RGBA6665.
// However, GPUEngineBase::_RenderPixel() takes source pixels in RGB555. In order to unify the methods, all pixels
// must be processed in RGBA6665.
FORCEINLINE void GPUEngineBase::_RenderPixel3D(const size_t srcX, const FragmentColor src, u16 *__restrict dstColorLine, u8 *__restrict dstLayerIDLine)
{
	if (src.a == 0)
	{
		return;
	}
	
	bool didPassWindowTest = true;
	bool enableColorEffect = true;
	
	this->_RenderPixel_CheckWindows<GPULayerID_BG0>(srcX, didPassWindowTest, enableColorEffect);
	
	if (!didPassWindowTest)
	{
		return;
	}
	
	ColorEffect selectedEffect = ColorEffect_Disable;
	
	if (enableColorEffect)
	{
		const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
		const GPULayerID dstLayerID = (GPULayerID)*dstLayerIDLine;
		const bool srcEffectEnable = (BLDCNT.BG0_Target1 != 0);
		const bool dstEffectEnable = (dstLayerID != GPULayerID_BG0) && this->_blend2[dstLayerID];
		
		// Select the color effect based on the BLDCNT target flags.
		bool forceBlendEffect = false;
		
		// 3D rendering has a special override: If the destination pixel is set to blend, then always blend.
		// Test case: When starting a stage in Super Princess Peach, the screen will be solid black unless
		// blending is forced here.
		forceBlendEffect = dstEffectEnable;
		
		if (forceBlendEffect)
		{
			selectedEffect = ColorEffect_Blend;
		}
		else if (srcEffectEnable)
		{
			switch ((ColorEffect)BLDCNT.ColorEffect)
			{
				// For the Blend effect, both first and second target flags must be checked.
				case ColorEffect_Blend:
				{
					if (dstEffectEnable) selectedEffect = (ColorEffect)BLDCNT.ColorEffect;
					break;
				}
					
				// For the Increase/Decrease Brightness effects, only the first target flag needs to be checked.
				// Test case: Bomberman Land Touch! dialog boxes will render too dark without this check.
				case ColorEffect_IncreaseBrightness:
				case ColorEffect_DecreaseBrightness:
					selectedEffect = (ColorEffect)BLDCNT.ColorEffect;
					break;
					
				default:
					break;
			}
		}
	}
	
	// Render the pixel using the selected color effect.
	const u16 srcRGB555 = R6G6B6TORGB15(src.r, src.g, src.b);
	u16 finalDstColor;
	
	switch (selectedEffect)
	{
		case ColorEffect_Disable:
			finalDstColor = srcRGB555;
			break;
			
		case ColorEffect_IncreaseBrightness:
			finalDstColor = this->_ColorEffectIncreaseBrightness(srcRGB555);
			break;
			
		case ColorEffect_DecreaseBrightness:
			finalDstColor = this->_ColorEffectDecreaseBrightness(srcRGB555);
			break;
			
		case ColorEffect_Blend:
			finalDstColor = this->_ColorEffectBlend3D(src, *dstColorLine);
			break;
	}
	
	*dstColorLine = finalDstColor | 0x8000;
	*dstLayerIDLine = GPULayerID_BG0;
}

#ifdef ENABLE_SSE2

template <bool ISCUSTOMRENDERINGNEEDED>
FORCEINLINE void GPUEngineBase::_RenderPixel3D_SSE2(const size_t dstX,
													const FragmentColor *__restrict src,
													u16 *__restrict dstColorLine,
													u8 *__restrict dstLayerIDLine)
{
	const __m128i srcColor0 = _mm_load_si128((__m128i *)src);
	const __m128i srcColor1 = _mm_load_si128((__m128i *)(src + 4));
	const __m128i srcColor2 = _mm_load_si128((__m128i *)(src + 8));
	const __m128i srcColor3 = _mm_load_si128((__m128i *)(src + 12));
	
	__m128i srcColorLo_vec128 = _mm_packs_epi32( _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(srcColor0, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(srcColor0, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(srcColor0, _mm_set1_epi32(0x003E0000)), 7)),
												 _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(srcColor1, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(srcColor1, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(srcColor1, _mm_set1_epi32(0x003E0000)), 7)) );
	__m128i srcColorHi_vec128 = _mm_packs_epi32( _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(srcColor2, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(srcColor2, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(srcColor2, _mm_set1_epi32(0x003E0000)), 7)),
												 _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(srcColor3, _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(srcColor3, _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(srcColor3, _mm_set1_epi32(0x003E0000)), 7)) );
	
	const __m128i srcAlphaLo_vec128 = _mm_cmpgt_epi16( _mm_packs_epi32( _mm_srli_epi32(_mm_and_si128(srcColor0, _mm_set1_epi32(0xFF000000)), 24), _mm_srli_epi32(_mm_and_si128(srcColor1, _mm_set1_epi32(0xFF000000)), 24) ), _mm_setzero_si128() );
	const __m128i srcAlphaHi_vec128 = _mm_cmpgt_epi16( _mm_packs_epi32( _mm_srli_epi32(_mm_and_si128(srcColor2, _mm_set1_epi32(0xFF000000)), 24), _mm_srli_epi32(_mm_and_si128(srcColor3, _mm_set1_epi32(0xFF000000)), 24) ), _mm_setzero_si128() );
	const __m128i srcAlphaLayerID_vec128 = _mm_packs_epi16(srcAlphaLo_vec128, srcAlphaHi_vec128);
	
	const __m128i dstColorLo_vec128 = _mm_load_si128((__m128i *)dstColorLine);
	const __m128i dstColorHi_vec128 = _mm_load_si128((__m128i *)(dstColorLine + 8));
	const __m128i dstLayerID_vec128 = _mm_load_si128((__m128i *)dstLayerIDLine);
	
	// Do the window test.
	__m128i didPassWindowTest = _mm_set1_epi8(1);
	__m128i enableColorEffect = _mm_set1_epi8(1);
	this->_RenderPixel_CheckWindows16_SSE2<GPULayerID_BG0, ISCUSTOMRENDERINGNEEDED>(dstX, didPassWindowTest, enableColorEffect);
	
	const __m128i passedWindowTestMaskLo = _mm_and_si128( srcAlphaLo_vec128, _mm_cmpeq_epi16(_mm_unpacklo_epi8(didPassWindowTest, _mm_setzero_si128()), _mm_set1_epi16(1)) );
	const __m128i passedWindowTestMaskHi = _mm_and_si128( srcAlphaHi_vec128, _mm_cmpeq_epi16(_mm_unpackhi_epi8(didPassWindowTest, _mm_setzero_si128()), _mm_set1_epi16(1)) );
	const __m128i passedWindowTestLayerID = _mm_and_si128( srcAlphaLayerID_vec128, _mm_packs_epi16(passedWindowTestMaskLo, passedWindowTestMaskHi) );
	
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	const __m128i srcEffectEnableMask = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG0_Target1), _mm_set1_epi8(1));
	
#ifdef ENABLE_SSSE3
	__m128i dstEffectEnableMask = _mm_shuffle_epi8(this->_blend2_SSSE3, dstLayerID_vec128);
#else
	__m128i dstEffectEnableMask = _mm_set_epi8(this->_blend2[dstLayerIDLine[15]],
											   this->_blend2[dstLayerIDLine[14]],
											   this->_blend2[dstLayerIDLine[13]],
											   this->_blend2[dstLayerIDLine[12]],
											   this->_blend2[dstLayerIDLine[11]],
											   this->_blend2[dstLayerIDLine[10]],
											   this->_blend2[dstLayerIDLine[ 9]],
											   this->_blend2[dstLayerIDLine[ 8]],
											   this->_blend2[dstLayerIDLine[ 7]],
											   this->_blend2[dstLayerIDLine[ 6]],
											   this->_blend2[dstLayerIDLine[ 5]],
											   this->_blend2[dstLayerIDLine[ 4]],
											   this->_blend2[dstLayerIDLine[ 3]],
											   this->_blend2[dstLayerIDLine[ 2]],
											   this->_blend2[dstLayerIDLine[ 1]],
											   this->_blend2[dstLayerIDLine[ 0]]);
#endif
	
	dstEffectEnableMask = _mm_and_si128(_mm_xor_si128(_mm_cmpeq_epi8(dstLayerID_vec128, _mm_set1_epi8(GPULayerID_BG0)), _mm_set1_epi32(0xFFFFFFFF)),
										_mm_xor_si128(_mm_cmpeq_epi8(dstEffectEnableMask, _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF)) );
	
	// Select the color effect based on the BLDCNT target flags.
	const __m128i enableColorEffectMask = _mm_cmpeq_epi8(enableColorEffect, _mm_set1_epi8(1));
	const __m128i colorEffect_vec128 = _mm_or_si128( _mm_and_si128(enableColorEffectMask, _mm_set1_epi8(BLDCNT.ColorEffect)), _mm_andnot_si128(enableColorEffectMask, _mm_set1_epi8(ColorEffect_Disable)) );
	const __m128i forceBlendEffectMask = _mm_and_si128(enableColorEffectMask, dstEffectEnableMask);
	const __m128i evy_vec128 = _mm_set1_epi16(this->_BLDALPHA_EVY);
	
	__m128i brightnessMask = _mm_setzero_si128();
	__m128i brightnessPixelsLo = _mm_setzero_si128();
	__m128i brightnessPixelsHi = _mm_setzero_si128();
	
	switch (BLDCNT.ColorEffect)
	{
		case ColorEffect_IncreaseBrightness:
			brightnessMask = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			brightnessPixelsLo = _mm_and_si128( this->_ColorEffectIncreaseBrightness(srcColorLo_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpacklo_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			brightnessPixelsHi = _mm_and_si128( this->_ColorEffectIncreaseBrightness(srcColorHi_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpackhi_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			break;
			
		case ColorEffect_DecreaseBrightness:
			brightnessMask = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			brightnessPixelsLo = _mm_and_si128( this->_ColorEffectDecreaseBrightness(srcColorLo_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpacklo_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			brightnessPixelsHi = _mm_and_si128( this->_ColorEffectDecreaseBrightness(srcColorHi_vec128, evy_vec128), _mm_cmpeq_epi16(_mm_unpackhi_epi8(brightnessMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
			break;
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const __m128i blendMask = _mm_or_si128( forceBlendEffectMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstEffectEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const __m128i blendPixelsLo = _mm_and_si128( this->_ColorEffectBlend3D(srcColor0, srcColor1, dstColorLo_vec128), _mm_cmpeq_epi16(_mm_unpacklo_epi8(blendMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	const __m128i blendPixelsHi = _mm_and_si128( this->_ColorEffectBlend3D(srcColor2, srcColor3, dstColorHi_vec128), _mm_cmpeq_epi16(_mm_unpackhi_epi8(blendMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	
	const __m128i disableMask = _mm_xor_si128( _mm_or_si128(brightnessMask, blendMask), _mm_set1_epi32(0xFFFFFFFF) );
	const __m128i disablePixelsLo = _mm_and_si128( srcColorLo_vec128, _mm_cmpeq_epi16(_mm_unpacklo_epi8(disableMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	const __m128i disablePixelsHi = _mm_and_si128( srcColorHi_vec128, _mm_cmpeq_epi16(_mm_unpackhi_epi8(disableMask, _mm_setzero_si128()), _mm_set1_epi16(0x00FF)) );
	
	// Combine the final colors.
	srcColorLo_vec128 = _mm_or_si128( _mm_or_si128(_mm_or_si128(brightnessPixelsLo, blendPixelsLo), disablePixelsLo), _mm_set1_epi16(0x8000) );
	srcColorHi_vec128 = _mm_or_si128( _mm_or_si128(_mm_or_si128(brightnessPixelsHi, blendPixelsHi), disablePixelsHi), _mm_set1_epi16(0x8000) );
	
	_mm_store_si128( (__m128i *)dstColorLine, _mm_or_si128(_mm_and_si128(passedWindowTestMaskLo, srcColorLo_vec128), _mm_andnot_si128(passedWindowTestMaskLo, dstColorLo_vec128)) );
	_mm_store_si128( (__m128i *)(dstColorLine + 8), _mm_or_si128(_mm_and_si128(passedWindowTestMaskHi, srcColorHi_vec128), _mm_andnot_si128(passedWindowTestMaskHi, dstColorHi_vec128)) );
	_mm_store_si128( (__m128i *)dstLayerIDLine, _mm_or_si128(_mm_and_si128(passedWindowTestLayerID, _mm_set1_epi8(GPULayerID_BG0)), _mm_andnot_si128(passedWindowTestLayerID, dstLayerID_vec128)) );
}

#endif

//this is fantastically inaccurate.
//we do the early return even though it reduces the resulting accuracy
//because we need the speed, and because it is inaccurate anyway
void GPUEngineBase::_MosaicSpriteLinePixel(const size_t x, u16 l, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const bool enableMosaic = (this->_oamList[this->_sprNum[x]].Mosaic != 0);
	if (!enableMosaic)
		return;
	
	const bool opaque = prioTab[x] <= 4;

	GPUEngineBase::MosaicColor::Obj objColor;
	objColor.color = LE_TO_LOCAL_16(dst[x]);
	objColor.alpha = dst_alpha[x];
	objColor.opaque = opaque;

	const size_t y = l;
	
	if (!this->_mosaicWidthOBJ[x].begin || !this->_mosaicHeightOBJ[y].begin)
	{
		objColor = this->_mosaicColors.obj[this->_mosaicWidthOBJ[x].trunc];
	}
	
	this->_mosaicColors.obj[x] = objColor;
	
	dst[x] = LE_TO_LOCAL_16(objColor.color);
	dst_alpha[x] = objColor.alpha;
	if (!objColor.opaque) prioTab[x] = 0x7F;
}

void GPUEngineBase::_MosaicSpriteLine(u16 l, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (!this->_isOBJMosaicSet)
	{
		return;
	}
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		this->_MosaicSpriteLinePixel(i, l, dst, dst_alpha, typeTab, prioTab);
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_Final(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	const u16 lineWidth = (ISDEBUGRENDER) ? this->_BGLayer[LAYERID].size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	IOREG_BGnX x = param.BGnX;
	IOREG_BGnY y = param.BGnY;
	const s32 dx = (s32)param.BGnPA.value;
	const s32 dy = (s32)param.BGnPC.value;
	const s32 wh = this->_BGLayer[LAYERID].size.width;
	const s32 ht = this->_BGLayer[LAYERID].size.height;
	const s32 wmask = wh - 1;
	const s32 hmask = ht - 1;
	
	u8 index;
	u16 color;
	
	// as an optimization, specially handle the fairly common case of
	// "unrotated + unscaled + no boundary checking required"
	if (dx == GPU_FRAMEBUFFER_NATIVE_WIDTH && dy == 0)
	{
		s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if (WRAP || (auxX + lineWidth < wh && auxX >= 0 && auxY < ht && auxY >= 0))
		{
			for (size_t i = 0; i < lineWidth; i++)
			{
				GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, color);
				
				if (ISCUSTOMRENDERINGNEEDED)
				{
					this->_bgLayerIndex[i] = index;
					this->_bgLayerColor[i] = color;
				}
				else
				{
					this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, i, (index != 0));
				}
				
				auxX++;
				
				if (WRAP)
					auxX = auxX & wmask;
			}
			
			return;
		}
	}
	
	for (size_t i = 0; i < lineWidth; i++, x.value += dx, y.value += dy)
	{
		const s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if (WRAP || ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht)))
		{
			GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, color);
			
			if (ISCUSTOMRENDERINGNEEDED)
			{
				this->_bgLayerIndex[i] = index;
				this->_bgLayerColor[i] = color;
			}
			else
			{
				this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, i, (index != 0));
			}
		}
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_ApplyWrap(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	this->_RenderPixelIterate_Final<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, GetPixelFunc, WRAP>(dstColorLine, lineIndex, param, map, tile, pal);
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc>
void GPUEngineBase::_RenderPixelIterate(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	if (this->_BGLayer[LAYERID].isDisplayWrapped)
	{
		this->_RenderPixelIterate_ApplyWrap<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, GetPixelFunc, true>(dstColorLine, lineIndex, param, map, tile, pal);
	}
	else
	{
		this->_RenderPixelIterate_ApplyWrap<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, GetPixelFunc, false>(dstColorLine, lineIndex, param, map, tile, pal);
	}
}

TILEENTRY GPUEngineBase::_GetTileEntry(const u32 tileMapAddress, const u16 xOffset, const u16 layerWidthMask)
{
	TILEENTRY theTileEntry;
	
	const u16 tmp = (xOffset & layerWidthMask) >> 3;
	u32 mapinfo = tileMapAddress + (tmp & 0x1F) * 2;
	if (tmp > 31) mapinfo += 32*32*2;
	theTileEntry.val = LOCAL_TO_LE_16( *(u16 *)MMU_gpu_map(mapinfo) );
	
	return theTileEntry;
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT>
FORCEINLINE void GPUEngineBase::_RenderPixelSingle(u16 *__restrict dstColorLine, u8 *__restrict dstLayerID, const size_t lineIndex, u16 color, const size_t srcX, const bool opaque)
{
	bool willRenderColor = opaque;
	
	if (MOSAIC)
	{
		//due to this early out, we will get incorrect behavior in cases where
		//we enable mosaic in the middle of a frame. this is deemed unlikely.
		
		if (!opaque) color = 0xFFFF;
		else color &= 0x7FFF;
		
		if (!this->_mosaicWidthBG[srcX].begin || !this->_mosaicHeightBG[lineIndex].begin)
		{
			color = this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[srcX].trunc];
		}
		
		this->_mosaicColors.bg[LAYERID][srcX] = color;
		
		willRenderColor = (color != 0xFFFF);
	}
	
	if (willRenderColor)
	{
		this->_RenderPixel<LAYERID, ISDEBUGRENDER, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(srcX,
																								  color,
																								  0,
																								  dstColorLine + srcX,
																								  dstLayerID + srcX);
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT>
void GPUEngineBase::_RenderPixelsCustom(u16 *__restrict dstColorLine, u8 *__restrict dstLayerID, const size_t lineIndex)
{
	const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
	
#ifdef ENABLE_SSE2
	
	#ifdef ENABLE_SSSE3
	const bool isIntegerScale = ((lineWidth % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0);
	const size_t scale = lineWidth / GPU_FRAMEBUFFER_NATIVE_WIDTH;
	#endif
	
	for (size_t x = 0, dstIdx = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=8)
	{
		if (MOSAIC)
		{
			const __m128i index_vec128 = _mm_loadl_epi64((__m128i *)(this->_bgLayerIndex + x));
			const __m128i col_vec128 = _mm_load_si128((__m128i *)(this->_bgLayerColor + x));
			
			const __m128i idxMask = _mm_cmpeq_epi16(_mm_unpacklo_epi8(index_vec128, _mm_setzero_si128()), _mm_setzero_si128());
			const __m128i tmpColor_vec128 = _mm_or_si128( _mm_and_si128(idxMask, _mm_set1_epi16(0xFFFF)), _mm_andnot_si128(idxMask, _mm_and_si128(_mm_set1_epi16(0x7FFF), col_vec128)) );
			
			const __m128i mosaicWidthMask = _mm_cmpeq_epi16( _mm_and_si128(_mm_set1_epi16(0x00FF), _mm_loadu_si128((__m128i *)(this->_mosaicWidthBG + x))), _mm_setzero_si128() );
			const __m128i mosaicHeightMask = _mm_cmpeq_epi16(_mm_set1_epi16(this->_mosaicHeightBG[lineIndex].begin), _mm_setzero_si128());
			const __m128i mosaicMask = _mm_or_si128(mosaicWidthMask, mosaicHeightMask);
			
			this->_mosaicColors.bg[LAYERID][x+0] = (_mm_extract_epi16(mosaicMask, 0) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+0].trunc] : _mm_extract_epi16(tmpColor_vec128, 0);
			this->_mosaicColors.bg[LAYERID][x+1] = (_mm_extract_epi16(mosaicMask, 1) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+1].trunc] : _mm_extract_epi16(tmpColor_vec128, 1);
			this->_mosaicColors.bg[LAYERID][x+2] = (_mm_extract_epi16(mosaicMask, 2) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+2].trunc] : _mm_extract_epi16(tmpColor_vec128, 2);
			this->_mosaicColors.bg[LAYERID][x+3] = (_mm_extract_epi16(mosaicMask, 3) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+3].trunc] : _mm_extract_epi16(tmpColor_vec128, 3);
			this->_mosaicColors.bg[LAYERID][x+4] = (_mm_extract_epi16(mosaicMask, 4) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+4].trunc] : _mm_extract_epi16(tmpColor_vec128, 4);
			this->_mosaicColors.bg[LAYERID][x+5] = (_mm_extract_epi16(mosaicMask, 5) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+5].trunc] : _mm_extract_epi16(tmpColor_vec128, 5);
			this->_mosaicColors.bg[LAYERID][x+6] = (_mm_extract_epi16(mosaicMask, 6) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+6].trunc] : _mm_extract_epi16(tmpColor_vec128, 6);
			this->_mosaicColors.bg[LAYERID][x+7] = (_mm_extract_epi16(mosaicMask, 7) != 0) ? this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x+7].trunc] : _mm_extract_epi16(tmpColor_vec128, 7);
			
			const __m128i mosaicColor_vec128 = _mm_loadu_si128((__m128i *)(this->_mosaicColors.bg[LAYERID] + x));
			const __m128i mosaicColorMask = _mm_cmpeq_epi16(mosaicColor_vec128, _mm_set1_epi16(0xFFFF));
			_mm_storel_epi64( (__m128i *)(this->_bgLayerIndex + x), _mm_andnot_si128(_mm_packs_epi16(mosaicColorMask, _mm_setzero_si128()), index_vec128) );
			_mm_store_si128( (__m128i *)(this->_bgLayerColor + x), _mm_or_si128(_mm_and_si128(mosaicColorMask, col_vec128), _mm_andnot_si128(mosaicColorMask, mosaicColor_vec128)) );
		}
		
	#ifdef ENABLE_SSSE3
		if (isIntegerScale)
		{
			const __m128i index_vec128 = _mm_loadl_epi64((__m128i *)(this->_bgLayerIndex + x));
			const __m128i col_vec128 = _mm_load_si128((__m128i *)(this->_bgLayerColor + x));
			
			for (size_t s = 0; s < scale; s++)
			{
				const __m128i ssse3idx_u8 = _mm_loadl_epi64((__m128i *)(_gpuDstToSrcSSSE3_u8 + (s * 8)));
				const __m128i ssse3idx_u16 = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u16 + (s * 16)));
				
				_mm_storel_epi64( (__m128i *)(this->_bgLayerIndexCustom + dstIdx + (s * 8)), _mm_shuffle_epi8(index_vec128, ssse3idx_u8) );
				_mm_store_si128( (__m128i *)(this->_bgLayerColorCustom + dstIdx + (s * 8)), _mm_shuffle_epi8(col_vec128, ssse3idx_u16) );
			}
			
			dstIdx += (scale * 8);
		}
		else
	#endif
		{
			for (size_t i = 0; i < 8; i++)
			{
				for (size_t j = 0; j < _gpuDstPitchCount[x]; j++, dstIdx++)
				{
					this->_bgLayerIndexCustom[dstIdx] = this->_bgLayerIndex[x+i];
					this->_bgLayerColorCustom[dstIdx] = this->_bgLayerColor[x+i];
				}
			}
		}
	}
#else
	for (size_t x = 0, dstIdx = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
	{
		if (MOSAIC)
		{
			u16 tmpColor = (this->_bgLayerIndex[x] == 0) ? 0xFFFF : this->_bgLayerColor[x] & 0x7FFF;
			
			if (!this->_mosaicWidthBG[x].begin || !this->_mosaicHeightBG[lineIndex].begin)
			{
				tmpColor = this->_mosaicColors.bg[LAYERID][this->_mosaicWidthBG[x].trunc];
			}
			
			this->_mosaicColors.bg[LAYERID][x] = tmpColor;
			
			if (tmpColor == 0xFFFF)
			{
				this->_bgLayerIndex[x] = 0;
			}
			else
			{
				this->_bgLayerColor[x] = tmpColor;
			}
		}
		
		for (size_t j = 0; j < _gpuDstPitchCount[x]; j++, dstIdx++)
		{
			this->_bgLayerIndexCustom[dstIdx] = this->_bgLayerIndex[x];
			this->_bgLayerColorCustom[dstIdx] = this->_bgLayerColor[x];
		}
	}
#endif
	
	const size_t dstPixCount = lineWidth;
	const size_t ssePixCount = (dstPixCount - (dstPixCount % 8));
	const size_t lineCount = _gpuDstLineCount[lineIndex];
	
	for (size_t l = 0; l < lineCount; l++)
	{
		size_t i = 0;
#ifdef ENABLE_SSE2
		for (; i < ssePixCount; i+=16)
		{
			const __m128i srcColorLo_vec128 = _mm_load_si128((__m128i *)(this->_bgLayerColorCustom + i));
			const __m128i srcColorHi_vec128 = _mm_load_si128((__m128i *)(this->_bgLayerColorCustom + i + 8));
			const __m128i srcOpaqueMask = _mm_xor_si128( _mm_cmpeq_epi8(_mm_load_si128((__m128i *)(this->_bgLayerIndexCustom + i)), _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF) );
			
			this->_RenderPixel16_SSE2<LAYERID, ISDEBUGRENDER, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, true>(i,
																												   srcColorHi_vec128,
																												   srcColorLo_vec128,
																												   srcOpaqueMask,
																												   NULL,
																												   dstColorLine + i,
																												   dstLayerID + i);
		}
#endif
		for (; i < dstPixCount; i++)
		{
			if (this->_bgLayerIndexCustom[i] == 0)
			{
				continue;
			}
			
			this->_RenderPixel<LAYERID, ISDEBUGRENDER, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(_gpuDstToSrcIndex[i],
																									  this->_bgLayerColorCustom[i],
																									  0,
																									  dstColorLine + i,
																									  dstLayerID + i);
		}
		
		dstColorLine += dstPixCount;
		dstLayerID += dstPixCount;
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT>
void GPUEngineBase::_RenderPixelsCustomVRAM(u16 *__restrict dstColorLine, u8 *__restrict dstLayerID, const size_t lineIndex)
{
	const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
	const size_t lineCount = _gpuDstLineCount[lineIndex];
	const size_t dstPixCount = lineWidth * lineCount;
	const size_t ssePixCount = (dstPixCount - (dstPixCount % 8));
	const u16 *__restrict srcLine = GPU->GetCustomVRAMAddressUsingMappedAddress(this->_BGLayer[LAYERID].BMPAddress) + (_gpuDstLineIndex[lineIndex] * lineWidth);
	
	size_t i = 0;
#ifdef ENABLE_SSE2
	for (; i < ssePixCount; i+=16)
	{
		const __m128i srcColorLo_vec128 = _mm_load_si128((__m128i *)(srcLine + i));
		const __m128i srcColorHi_vec128 = _mm_load_si128((__m128i *)(srcLine + i + 8));
		
		const __m128i srcOpaqueMaskLo = _mm_cmpeq_epi16(_mm_and_si128(_mm_set1_epi16(0x8000), srcColorLo_vec128), _mm_setzero_si128());
		const __m128i srcOpaqueMaskHi = _mm_cmpeq_epi16(_mm_and_si128(_mm_set1_epi16(0x8000), srcColorHi_vec128), _mm_setzero_si128());
		const __m128i srcOpaqueMask = _mm_xor_si128( _mm_packs_epi16(srcOpaqueMaskLo, srcOpaqueMaskHi), _mm_set1_epi32(0xFFFFFFFF) );
		
		this->_RenderPixel16_SSE2<LAYERID, ISDEBUGRENDER, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, true>(i,
																											   srcColorHi_vec128,
																											   srcColorLo_vec128,
																											   srcOpaqueMask,
																											   NULL,
																											   dstColorLine + i,
																											   dstLayerID + i);
	}
#endif
	for (; i < dstPixCount; i++)
	{
		if ((srcLine[i] & 0x8000) == 0)
		{
			continue;
		}
		
		this->_RenderPixel<LAYERID, ISDEBUGRENDER, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(_gpuDstToSrcIndex[i],
																								  srcLine[i],
																								  0,
																								  dstColorLine + i,
																								  dstLayerID + i);
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_BGText(u16 *__restrict dstColorLine, const u16 lineIndex, const u16 XBG, const u16 YBG)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const u16 lineWidth = (ISDEBUGRENDER) ? this->_BGLayer[LAYERID].size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const u16 lg    = this->_BGLayer[LAYERID].size.width;
	const u16 ht    = this->_BGLayer[LAYERID].size.height;
	const u32 tile  = this->_BGLayer[LAYERID].tileEntryAddress;
	const u16 wmask = lg - 1;
	const u16 hmask = ht - 1;
	
	const size_t pixCountLo = 8 - (XBG & 0x0007);
	size_t x = 0;
	size_t xoff = XBG;
	
	const u16 tmp = (YBG & hmask) >> 3;
	u32 map = this->_BGLayer[LAYERID].tileMapAddress + (tmp & 31) * 64;
	if (tmp > 31)
		map += ADDRESS_STEP_512B << this->_BGLayer[LAYERID].BGnCNT.ScreenSize;
	
	if (this->_BGLayer[LAYERID].BGnCNT.PaletteMode == PaletteMode_16x16) // color: 16 palette entries
	{
		const u16 *__restrict pal = this->_paletteBG;
		const u16 yoff = (YBG & 0x0007) << 2;
		u8 index;
		u16 color;
		
		for (size_t xfin = pixCountLo; x < lineWidth; xfin = std::min<u16>(x+8, lineWidth))
		{
			const TILEENTRY tileEntry = this->_GetTileEntry(map, xoff, wmask);
			const u16 tilePalette = tileEntry.bits.Palette * 16;
			u8 *__restrict tileColorIdx = (u8 *)MMU_gpu_map(tile + (tileEntry.bits.TileNum * 0x20) + ((tileEntry.bits.VFlip) ? (7*4)-yoff : yoff));
			
			if (tileEntry.bits.HFlip)
			{
				tileColorIdx += 3 - ((xoff & 0x0007) >> 1);
				
				if (xoff & 1)
				{
					if (ISCUSTOMRENDERINGNEEDED)
					{
						this->_bgLayerIndex[x] = *tileColorIdx & 0x0F;
						this->_bgLayerColor[x] = LE_TO_LOCAL_16(pal[this->_bgLayerIndex[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx & 0x0F;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
					}
					
					x++;
					xoff++;
					tileColorIdx--;
				}
				
				for (; x < xfin; tileColorIdx--)
				{
					if (ISCUSTOMRENDERINGNEEDED)
					{
						this->_bgLayerIndex[x] = *tileColorIdx >> 4;
						this->_bgLayerColor[x] = LE_TO_LOCAL_16(pal[this->_bgLayerIndex[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx >> 4;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
					}
					
					x++;
					xoff++;
					
					if (x < xfin)
					{
						if (ISCUSTOMRENDERINGNEEDED)
						{
							this->_bgLayerIndex[x] = *tileColorIdx & 0x0F;
							this->_bgLayerColor[x] = LE_TO_LOCAL_16(pal[this->_bgLayerIndex[x] + tilePalette]);
						}
						else
						{
							index = *tileColorIdx & 0x0F;
							color = LE_TO_LOCAL_16(pal[index + tilePalette]);
							this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
						}
						
						x++;
						xoff++;
					}
				}
			}
			else
			{
				tileColorIdx += ((xoff & 0x0007) >> 1);
				
				if (xoff & 1)
				{
					if (ISCUSTOMRENDERINGNEEDED)
					{
						this->_bgLayerIndex[x] = *tileColorIdx >> 4;
						this->_bgLayerColor[x] = LE_TO_LOCAL_16(pal[this->_bgLayerIndex[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx >> 4;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
					}
					
					x++;
					xoff++;
					tileColorIdx++;
				}
				
				for (; x < xfin; tileColorIdx++)
				{
					if (ISCUSTOMRENDERINGNEEDED)
					{
						this->_bgLayerIndex[x] = *tileColorIdx & 0x0F;
						this->_bgLayerColor[x] = LE_TO_LOCAL_16(pal[this->_bgLayerIndex[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx & 0x0F;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
					}
					
					x++;
					xoff++;
					
					if (x < xfin)
					{
						if (ISCUSTOMRENDERINGNEEDED)
						{
							this->_bgLayerIndex[x] = *tileColorIdx >> 4;
							this->_bgLayerColor[x] = LE_TO_LOCAL_16(pal[this->_bgLayerIndex[x] + tilePalette]);
						}
						else
						{
							index = *tileColorIdx >> 4;
							color = LE_TO_LOCAL_16(pal[index + tilePalette]);
							this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
						}
						
						x++;
						xoff++;
					}
				}
			}
		}
	}
	else //256-color BG
	{
		const u16 *__restrict pal = (DISPCNT.ExBGxPalette_Enable) ? *(this->_BGLayer[LAYERID].extPalette) : this->_paletteBG;
		const u32 extPalMask = -DISPCNT.ExBGxPalette_Enable;
		const u16 yoff = (YBG & 0x0007) << 3;
		size_t line_dir;
		
		for (size_t xfin = pixCountLo; x < lineWidth; xfin = std::min<u16>(x+8, lineWidth))
		{
			const TILEENTRY tileEntry = this->_GetTileEntry(map, xoff, wmask);
			const u16 *__restrict tilePal = (u16 *)((u8 *)pal + ((tileEntry.bits.Palette<<9) & extPalMask));
			const u8 *__restrict tileColorIdx = (u8 *)MMU_gpu_map(tile + (tileEntry.bits.TileNum * 0x40) + ((tileEntry.bits.VFlip) ? (7*8)-yoff : yoff));
			
			if (tileEntry.bits.HFlip)
			{
				tileColorIdx += (7 - (xoff & 0x0007));
				line_dir = -1;
			}
			else
			{
				tileColorIdx += (xoff & 0x0007);
				line_dir = 1;
			}
			
			for (; x < xfin; x++, xoff++, tileColorIdx += line_dir)
			{
				if (ISCUSTOMRENDERINGNEEDED)
				{
					this->_bgLayerIndex[x] = *tileColorIdx;
					this->_bgLayerColor[x] = LE_TO_LOCAL_16(tilePal[this->_bgLayerIndex[x]]);
				}
				else
				{
					const u8 index = *tileColorIdx;
					const u16 color = LE_TO_LOCAL_16(tilePal[index]);
					this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (index != 0));
				}
			}
		}
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_BGAffine(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param)
{
	this->_RenderPixelIterate<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_tiled_8bit_entry>(dstColorLine, lineIndex, param, this->_BGLayer[LAYERID].tileMapAddress, this->_BGLayer[LAYERID].tileEntryAddress, this->_paletteBG);
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_RenderLine_BGExtended(u16 *__restrict dstColorLine, const u16 lineIndex, const IOREG_BGnParameter &param, bool &outUseCustomVRAM)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	switch (this->_BGLayer[LAYERID].type)
	{
		case BGType_AffineExt_256x16: // 16  bit bgmap entries
		{
			if (DISPCNT.ExBGxPalette_Enable)
			{
				this->_RenderPixelIterate< LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_tiled_16bit_entry<true> >(dstColorLine, lineIndex, param, this->_BGLayer[LAYERID].tileMapAddress, this->_BGLayer[LAYERID].tileEntryAddress, *(this->_BGLayer[LAYERID].extPalette));
			}
			else
			{
				this->_RenderPixelIterate< LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_tiled_16bit_entry<false> >(dstColorLine, lineIndex, param, this->_BGLayer[LAYERID].tileMapAddress, this->_BGLayer[LAYERID].tileEntryAddress, this->_paletteBG);
			}
			break;
		}
			
		case BGType_AffineExt_256x1: // 256 colors
			this->_RenderPixelIterate<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_256_map>(dstColorLine, lineIndex, param, this->_BGLayer[LAYERID].BMPAddress, 0, this->_paletteBG);
			break;
			
		case BGType_AffineExt_Direct: // direct colors / BMP
		{
			if (!MOSAIC)
			{
				const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(this->_BGLayer[LAYERID].BMPAddress) - MMU.ARM9_LCD) / sizeof(u16);
				
				if (vramPixel > (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
				{
					outUseCustomVRAM = false;
				}
				else
				{
					const size_t blockID = vramPixel / (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES);
					const size_t blockPixel = vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES);
					const size_t blockLine = blockPixel / GPU_FRAMEBUFFER_NATIVE_WIDTH;
					
					if (GPU->GetEngineMain()->VerifyVRAMLineDidChange(blockID, lineIndex + blockLine))
					{
						u16 *newRenderLineTarget = (this->_displayOutputMode == GPUDisplayMode_Normal) ? this->nativeBuffer + (lineIndex * GPU_FRAMEBUFFER_NATIVE_WIDTH) : this->_internalRenderLineTargetNative;
						this->_LineColorCopy<true, false, false, false>(newRenderLineTarget, dstColorLine, lineIndex);
						this->_LineLayerIDCopy<true, false>(this->_renderLineLayerIDNative, this->_renderLineLayerIDCustom, lineIndex);
						dstColorLine = newRenderLineTarget;
					}
					
					outUseCustomVRAM = !GPU->GetEngineMain()->isLineCaptureNative[blockID][lineIndex + blockLine];
				}
			}
			else
			{
				outUseCustomVRAM = false;
			}
			
			if (!outUseCustomVRAM)
			{
				this->_RenderPixelIterate<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_BMP_map>(dstColorLine, lineIndex, param, this->_BGLayer[LAYERID].BMPAddress, 0, this->_paletteBG);
			}
			else
			{
				if (this->isLineRenderNative[lineIndex])
				{
					const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
					const size_t customLineWidth = dispInfo.customWidth;
					const size_t customLineIndex = _gpuDstLineIndex[lineIndex];
					
					u16 *newRenderLineTarget = (this->_displayOutputMode == GPUDisplayMode_Normal) ? this->customBuffer + (customLineIndex * customLineWidth) : this->_internalRenderLineTargetCustom;
					this->_LineColorCopy<false, true, false, false>(newRenderLineTarget, dstColorLine, lineIndex);
					this->_LineLayerIDCopy<false, true>(this->_renderLineLayerIDCustom, this->_renderLineLayerIDNative, lineIndex);
					dstColorLine = newRenderLineTarget;
					
					this->isLineRenderNative[lineIndex] = false;
					this->nativeLineRenderCount--;
				}
			}
			break;
		}
			
		case BGType_Large8bpp: // large screen 256 colors
			this->_RenderPixelIterate<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_256_map>(dstColorLine, lineIndex, param, this->_BGLayer[LAYERID].largeBMPAddress, 0, this->_paletteBG);
			break;
			
		default:
			break;
	}
	
	return dstColorLine;
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_LineText(u16 *__restrict dstColorLine, const u16 lineIndex)
{
	if (ISDEBUGRENDER)
	{
		this->_RenderLine_BGText<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, 0, lineIndex);
	}
	else
	{
		this->_RenderLine_BGText<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, this->_BGLayer[LAYERID].xOffset, lineIndex + this->_BGLayer[LAYERID].yOffset);
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_LineRot(u16 *__restrict dstColorLine, const u16 lineIndex)
{
	if (ISDEBUGRENDER)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, lineIndex*GPU_FRAMEBUFFER_NATIVE_WIDTH};
		this->_RenderLine_BGAffine<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, debugParams);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (LAYERID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGAffine<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, *bgParams);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_LineExtRot(u16 *__restrict dstColorLine, const u16 lineIndex, bool &outUseCustomVRAM)
{
	if (ISDEBUGRENDER)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, lineIndex*GPU_FRAMEBUFFER_NATIVE_WIDTH};
		return this->_RenderLine_BGExtended<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, debugParams, outUseCustomVRAM);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (LAYERID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		dstColorLine = this->_RenderLine_BGExtended<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, *bgParams, outUseCustomVRAM);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
	
	return dstColorLine;
}

/*****************************************************************************/
//			SPRITE RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSpriteBMP(const u8 spriteNum, const u16 l, u16 *__restrict dst, const u32 srcadr, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
{
	const u16 *__restrict bmpBuffer = (u16 *)MMU_gpu_map(srcadr);
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	if (xdir == 1)
	{
		if (ISDEBUGRENDER)
		{
			const size_t ssePixCount = lg - (lg % 8);
			for (; i < ssePixCount; i += 8, x += 8, sprX += 8)
			{
				__m128i color_vec128 = _mm_loadu_si128((__m128i *)(bmpBuffer + x));
				const __m128i colorAlpha_vec128 = _mm_and_si128(color_vec128, _mm_set1_epi16(0x8000));
				const __m128i colorAlphaCompare = _mm_cmpeq_epi16(colorAlpha_vec128, _mm_set1_epi16(0x8000));
				color_vec128 = _mm_or_si128( _mm_and_si128(colorAlphaCompare, color_vec128), _mm_andnot_si128(colorAlphaCompare, _mm_loadu_si128((__m128i *)(dst + sprX))) );
				
				_mm_storeu_si128((__m128i *)(dst + sprX), color_vec128);
			}
		}
		else
		{
			const __m128i prio_vec128 = _mm_set1_epi8(prio);
			
			const size_t ssePixCount = lg - (lg % 16);
			for (; i < ssePixCount; i += 16, x += 16, sprX += 16)
			{
				__m128i prioTab_vec128 = _mm_loadu_si128((__m128i *)(prioTab + sprX));
				const __m128i prioCompare = _mm_cmplt_epi8(prio_vec128, prioTab_vec128);
				
				__m128i colorLo_vec128 = _mm_loadu_si128((__m128i *)(bmpBuffer + x));
				__m128i colorHi_vec128 = _mm_loadu_si128((__m128i *)(bmpBuffer + x + 8));
				
				const __m128i colorAlphaLo_vec128 = _mm_and_si128(colorLo_vec128, _mm_set1_epi16(0x8000));
				const __m128i colorAlphaHi_vec128 = _mm_and_si128(colorHi_vec128, _mm_set1_epi16(0x8000));
				
				const __m128i colorAlphaLoCompare = _mm_cmpeq_epi16(colorAlphaLo_vec128, _mm_set1_epi16(0x8000));
				const __m128i colorAlphaHiCompare = _mm_cmpeq_epi16(colorAlphaHi_vec128, _mm_set1_epi16(0x8000));
				const __m128i colorAlphaPackedCompare = _mm_cmpeq_epi8( _mm_packs_epi16(colorAlphaLoCompare, colorAlphaHiCompare), _mm_set1_epi8(0xFF) );
				
				const __m128i combinedPackedCompare = _mm_and_si128(prioCompare, colorAlphaPackedCompare);
				const __m128i combinedLoCompare = _mm_cmpeq_epi16( _mm_unpacklo_epi8(combinedPackedCompare, _mm_setzero_si128()), _mm_set1_epi16(0x00FF) );
				const __m128i combinedHiCompare = _mm_cmpeq_epi16( _mm_unpackhi_epi8(combinedPackedCompare, _mm_setzero_si128()), _mm_set1_epi16(0x00FF) );
				
				colorLo_vec128 = _mm_or_si128( _mm_and_si128(combinedLoCompare, colorLo_vec128), _mm_andnot_si128(combinedLoCompare, _mm_loadu_si128((__m128i *)(dst + sprX))) );
				colorHi_vec128 = _mm_or_si128( _mm_and_si128(combinedHiCompare, colorHi_vec128), _mm_andnot_si128(combinedHiCompare, _mm_loadu_si128((__m128i *)(dst + sprX + 8))) );
				const __m128i dstAlpha_vec128 = _mm_or_si128( _mm_and_si128(combinedPackedCompare, _mm_set1_epi8(alpha + 1)), _mm_andnot_si128(combinedPackedCompare, _mm_loadu_si128((__m128i *)(dst_alpha + sprX))) );
				const __m128i dstTypeTab_vec128 = _mm_or_si128( _mm_and_si128(combinedPackedCompare, _mm_set1_epi8(OBJMode_Bitmap)), _mm_andnot_si128(combinedPackedCompare, _mm_loadu_si128((__m128i *)(typeTab + sprX))) );
				prioTab_vec128 = _mm_or_si128( _mm_and_si128(combinedPackedCompare, prio_vec128), _mm_andnot_si128(combinedPackedCompare, prioTab_vec128) );
				const __m128i sprNum_vec128 = _mm_or_si128( _mm_and_si128(combinedPackedCompare, _mm_set1_epi8(spriteNum)), _mm_andnot_si128(combinedPackedCompare, _mm_loadu_si128((__m128i *)(this->_sprNum + sprX))) );
				
				_mm_storeu_si128((__m128i *)(dst + sprX), colorLo_vec128);
				_mm_storeu_si128((__m128i *)(dst + sprX + 8), colorHi_vec128);
				_mm_storeu_si128((__m128i *)(dst_alpha + sprX), dstAlpha_vec128);
				_mm_storeu_si128((__m128i *)(typeTab + sprX), dstTypeTab_vec128);
				_mm_storeu_si128((__m128i *)(prioTab + sprX), prioTab_vec128);
				_mm_storeu_si128((__m128i *)(this->_sprNum + sprX), sprNum_vec128);
			}
		}
	}
#endif
	
	for (; i < lg; i++, sprX++, x += xdir)
	{
		const u16 color = LE_TO_LOCAL_16(bmpBuffer[x]);
		
		//a cleared alpha bit suppresses the pixel from processing entirely; it doesnt exist
		if (ISDEBUGRENDER)
		{
			if (color & 0x8000)
			{
				dst[sprX] = color;
			}
		}
		else
		{
			if ((color & 0x8000) && (prio < prioTab[sprX]))
			{
				dst[sprX] = color;
				dst_alpha[sprX] = alpha+1;
				typeTab[sprX] = OBJMode_Bitmap;
				prioTab[sprX] = prio;
				this->_sprNum[sprX] = spriteNum;
			}
		}
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite256(const u8 spriteNum, const u16 l, u16 *__restrict dst, const u32 srcadr, const u16 *__restrict pal, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
{
	for (size_t i = 0; i < lg; i++, ++sprX, x += xdir)
	{
		const u32 adr = srcadr + (x & 0x7) + ((x & 0xFFF8) << 3);
		const u8 *__restrict src = (u8 *)MMU_gpu_map(adr);
		const u8 palette_entry = *src;

		//a zero value suppresses the pixel from processing entirely; it doesnt exist
		if (ISDEBUGRENDER)
		{
			if (palette_entry > 0)
			{
				dst[sprX] = LE_TO_LOCAL_16(pal[palette_entry]);
			}
		}
		else
		{
			if ((palette_entry > 0) && (prio < prioTab[sprX]))
			{
				dst[sprX] = LE_TO_LOCAL_16(pal[palette_entry]);
				dst_alpha[sprX] = 0xFF;
				typeTab[sprX] = (alpha ? OBJMode_Transparent : OBJMode_Normal);
				prioTab[sprX] = prio;
				this->_sprNum[sprX] = spriteNum;
			}
		}
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite16(const u8 spriteNum, const u16 l, u16 *__restrict dst, const u32 srcadr, const u16 *__restrict pal, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
{
	for (size_t i = 0; i < lg; i++, ++sprX, x += xdir)
	{
		const u16 x1 = x >> 1;
		const u32 adr = srcadr + (x1 & 0x3) + ((x1 & 0xFFFC) << 3);
		const u8 *__restrict src = (u8 *)MMU_gpu_map(adr);
		const u8 palette = *src;
		const u8 palette_entry = (x & 1) ? palette >> 4 : palette & 0xF;
		
		//a zero value suppresses the pixel from processing entirely; it doesnt exist
		if (ISDEBUGRENDER)
		{
			if (palette_entry > 0)
			{
				dst[sprX] = LE_TO_LOCAL_16(pal[palette_entry]);
			}
		}
		else
		{
			if ((palette_entry > 0) && (prio < prioTab[sprX]))
			{
				dst[sprX] = LE_TO_LOCAL_16(pal[palette_entry]);
				dst_alpha[sprX] = 0xFF;
				typeTab[sprX] = (alpha ? OBJMode_Transparent : OBJMode_Normal);
				prioTab[sprX] = prio;
				this->_sprNum[sprX] = spriteNum;
			}
		}
	}
}

void GPUEngineBase::_RenderSpriteWin(const u8 *src, const bool col256, const size_t lg, size_t sprX, size_t x, const s32 xdir)
{
	if (col256)
	{
		for (size_t i = 0; i < lg; i++, sprX++, x += xdir)
		{
			//_gpuSprWin[sprX] = (src[x])?1:0;
			if (src[(x & 7) + ((x & 0xFFF8) << 3)])
			{
				this->_sprWin[sprX] = 1;
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
				this->_sprWin[sprX] = 1;
			}
		}
	}
}

// return val means if the sprite is to be drawn or not
bool GPUEngineBase::_ComputeSpriteVars(const OAMAttributes &spriteInfo, const u16 l, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir)
{
	x = 0;
	// get sprite location and size
	sprX = spriteInfo.X;
	sprY = spriteInfo.Y;
	sprSize = GPUEngineBase::_sprSizeTab[spriteInfo.Size][spriteInfo.Shape];
	lg = sprSize.width;
	
// FIXME: for rot/scale, a list of entries into the sprite should be maintained,
// that tells us where the first pixel of a screenline starts in the sprite,
// and how a step to the right in a screenline translates within the sprite

	//this wasn't really tested by anything. very unlikely to get triggered
	y = (l - sprY) & 0xFF;                        /* get the y line within sprite coords */
	if (y >= sprSize.height)
		return false;

	if ((sprX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || (sprX+sprSize.width <= 0))	/* sprite pixels outside of line */
		return false;				/* not to be drawn */

	// sprite portion out of the screen (LEFT)
	if (sprX < 0)
	{
		lg += sprX;	
		x = -(sprX);
		sprX = 0;
	}
	// sprite portion out of the screen (RIGHT)
	if (sprX+sprSize.width >= GPU_FRAMEBUFFER_NATIVE_WIDTH)
		lg = GPU_FRAMEBUFFER_NATIVE_WIDTH - sprX;

	// switch TOP<-->BOTTOM
	if (spriteInfo.VFlip)
		y = sprSize.height - y - 1;
	
	// switch LEFT<-->RIGHT
	if (spriteInfo.HFlip)
	{
		x = sprSize.width - x - 1;
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

u32 GPUEngineBase::_SpriteAddressBMP(const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	if (DISPCNT.OBJ_BMP_mapping)
	{
		//tested by buffy sacrifice damage blood splatters in corner
		return this->_sprMem + (spriteInfo.TileIndex << this->_sprBMPBoundary) + (y * sprSize.width * 2);
	}
	else
	{
		//2d mapping:
		//verified in rotozoomed mode by knights in the nightmare intro

		if (DISPCNT.OBJ_BMP_2D_dim)
			//256*256, verified by heroes of mana FMV intro
			return this->_sprMem + (((spriteInfo.TileIndex&0x3E0) * 64 + (spriteInfo.TileIndex&0x1F) * 8 + (y << 8)) << 1);
		else 
			//128*512, verified by harry potter and the order of the phoenix conversation portraits
			return this->_sprMem + (((spriteInfo.TileIndex&0x3F0) * 64 + (spriteInfo.TileIndex&0x0F) * 8 + (y << 7)) << 1);
	}
}

template <bool ISDEBUGRENDER>
void GPUEngineBase::_SpriteRender(const u16 lineIndex, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (this->_spriteRenderMode == SpriteRenderMode_Sprite1D)
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite1D, ISDEBUGRENDER>(lineIndex, dst, dst_alpha, typeTab, prioTab);
	else
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite2D, ISDEBUGRENDER>(lineIndex, dst, dst_alpha, typeTab, prioTab);
}

void GPUEngineBase::SpriteRenderDebug(const u16 lineIndex, u16 *dst)
{
	this->_SpriteRender<true>(lineIndex, dst, NULL, NULL, NULL);
}

template <SpriteRenderMode MODE, bool ISDEBUGRENDER>
void GPUEngineBase::_SpriteRenderPerform(const u16 lineIndex, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	size_t cost = 0;
	
	for (size_t i = 0; i < 128; i++)
	{
		OAMAttributes spriteInfo = this->_oamList[i];

		//for each sprite:
		if (cost >= 2130)
		{
			//out of sprite rendering time
			//printf("sprite overflow!\n");
			//return;		
		}

		//do we incur a cost if a sprite is disabled?? we guess so.
		cost += 2;
		
		// Check if sprite is disabled before everything
		if (spriteInfo.RotScale == 0 && spriteInfo.Disable != 0)
			continue;
		
		// Must explicitly convert endianness with attributes 1 and 2.
		spriteInfo.attr[1] = LOCAL_TO_LE_16(spriteInfo.attr[1]);
		spriteInfo.attr[2] = LOCAL_TO_LE_16(spriteInfo.attr[2]);
		
		const OBJMode objMode = (OBJMode)spriteInfo.Mode;

		SpriteSize sprSize;
		s32 sprX;
		s32 sprY;
		s32 x;
		s32 y;
		s32 lg;
		s32 xdir;
		u8 prio = spriteInfo.Priority;
		u16 *__restrict pal;
		u8 *__restrict src;
		u32 srcadr;
		
		if (spriteInfo.RotScale != 0)
		{
			s32		fieldX, fieldY, auxX, auxY, realX, realY, offset;
			u8		blockparameter;
			s16		dx, dmx, dy, dmy;
			u16		colour;

			// Get sprite positions and size
			sprX = spriteInfo.X;
			sprY = spriteInfo.Y;
			sprSize = GPUEngineBase::_sprSizeTab[spriteInfo.Size][spriteInfo.Shape];

			// Copy sprite size, to check change it if needed
			fieldX = sprSize.width;
			fieldY = sprSize.height;
			lg = sprSize.width;

			// If we are using double size mode, double our control vars
			if (spriteInfo.DoubleSize != 0)
			{
				fieldX <<= 1;
				fieldY <<= 1;
				lg <<= 1;
			}

			//check if the sprite is visible y-wise. unfortunately our logic for x and y is different due to our scanline based rendering
			//tested thoroughly by many large sprites in Super Robot Wars K which wrap around the screen
			y = (lineIndex - sprY) & 0xFF;
			if (y >= fieldY)
				continue;

			//check if sprite is visible x-wise.
			if ((sprX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || (sprX + fieldX <= 0))
				continue;

			cost += (sprSize.width * 2) + 10;

			// Get which four parameter block is assigned to this sprite
			blockparameter = (spriteInfo.RotScaleIndex + (spriteInfo.HFlip << 3) + (spriteInfo.VFlip << 4)) * 4;

			// Get rotation/scale parameters
			dx  = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+0].attr3);
			dmx = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+1].attr3);
			dy  = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+2].attr3);
			dmy = LE_TO_LOCAL_16((s16)this->_oamList[blockparameter+3].attr3);
			
			// Calculate fixed point 8.8 start offsets
			realX = (sprSize.width  << 7) - (fieldX >> 1)*dx - (fieldY >> 1)*dmx + y*dmx;
			realY = (sprSize.height << 7) - (fieldX >> 1)*dy - (fieldY >> 1)*dmy + y*dmy;

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
			if (spriteInfo.PaletteMode == PaletteMode_1x256)
			{
				src = (u8 *)MMU_gpu_map(this->_sprMem + (spriteInfo.TileIndex << this->_sprBoundary));

				// If extended palettes are set, use them
				pal = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;

				for (size_t j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX >> 8);
					auxY = (realY >> 8);

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						if (MODE == SpriteRenderMode_Sprite2D)
							offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*8);
						else
							offset = (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)*sprSize.width*8) + ((auxY&0x7)*8);

						colour = src[offset];
						
						if (ISDEBUGRENDER)
						{
							if (colour)
							{
								dst[sprX] = LE_TO_LOCAL_16(pal[colour]);
							}
						}
						else
						{
							if (colour && (prio < prioTab[sprX]))
							{
								dst[sprX] = LE_TO_LOCAL_16(pal[colour]);
								dst_alpha[sprX] = 0xFF;
								typeTab[sprX] = objMode;
								prioTab[sprX] = prio;
							}
						}
					}

					// Add the rotation/scale coefficients, here the rotation/scaling is performed
					realX += dx;
					realY += dy;
				}
			}
			// Rotozoomed direct color
			else if (objMode == OBJMode_Bitmap)
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;

				srcadr = this->_SpriteAddressBMP(spriteInfo, sprSize, 0);

				for (size_t j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;

					//this is all very slow, and so much dup code with other rotozoomed modes.
					//dont bother fixing speed until this whole thing gets reworked

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						if (DISPCNT.OBJ_BMP_2D_dim)
							//tested by knights in the nightmare
							offset = (this->_SpriteAddressBMP(spriteInfo, sprSize, auxY)-srcadr)/2+auxX;
						else //tested by lego indiana jones (somehow?)
							//tested by buffy sacrifice damage blood splatters in corner
							offset = auxX + (auxY*sprSize.width);


						u16* mem = (u16*)MMU_gpu_map(srcadr + (offset<<1));
						colour = LE_TO_LOCAL_16(*mem);
						
						if (ISDEBUGRENDER)
						{
							if (colour & 0x8000)
							{
								dst[sprX] = colour;
							}
						}
						else
						{
							if ((colour & 0x8000) && (prio < prioTab[sprX]))
							{
								dst[sprX] = colour;
								dst_alpha[sprX] = spriteInfo.PaletteIndex;
								typeTab[sprX] = objMode;
								prioTab[sprX] = prio;
							}
						}
					}

					// Add the rotation/scale coefficients, here the rotation/scaling is performed
					realX += dx;
					realY += dy;
				}
			}
			// Rotozoomed 16/16 palette
			else
			{
				if (MODE == SpriteRenderMode_Sprite2D)
				{
					src = (u8 *)MMU_gpu_map(this->_sprMem + (spriteInfo.TileIndex << 5));
				}
				else
				{
					src = (u8 *)MMU_gpu_map(this->_sprMem + (spriteInfo.TileIndex << this->_sprBoundary));
				}
				
				pal = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);

				for (size_t j = 0; j < lg; ++j, ++sprX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						if (MODE == SpriteRenderMode_Sprite2D)
							offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*4);
						else
							offset = ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)*sprSize.width)*4 + ((auxY&0x7)*4);
						
						colour = src[offset];

						// Get 4bits value from the readed 8bits
						if (auxX&1)	colour >>= 4;
						else		colour &= 0xF;
						
						if (ISDEBUGRENDER)
						{
							if (colour)
							{
								dst[sprX] = LE_TO_LOCAL_16(pal[colour]);
							}
						}
						else
						{
							if (colour && (prio < prioTab[sprX]))
							{
								if (objMode == OBJMode_Window)
								{
									this->_sprWin[sprX] = 1;
								}
								else
								{
									dst[sprX] = LE_TO_LOCAL_16(pal[colour]);
									dst_alpha[sprX] = 0xFF;
									typeTab[sprX] = objMode;
									prioTab[sprX] = prio;
								}
							}
						}
					}

					// Add the rotation/scale coeficients, here the rotation/scaling  is performed
					realX += dx;
					realY += dy;
				}
			}
		}
		else //NOT rotozoomed
		{
			if (!this->_ComputeSpriteVars(spriteInfo, lineIndex, sprSize, sprX, sprY, x, y, lg, xdir))
				continue;

			cost += sprSize.width;

			if (objMode == OBJMode_Window)
			{
				if (MODE == SpriteRenderMode_Sprite2D)
				{
					if (spriteInfo.PaletteMode == PaletteMode_1x256)
						src = (u8 *)MMU_gpu_map(this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8));
					else
						src = (u8 *)MMU_gpu_map(this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4));
				}
				else
				{
					if (spriteInfo.PaletteMode == PaletteMode_1x256)
						src = (u8 *)MMU_gpu_map(this->_sprMem + (spriteInfo.TileIndex<<this->_sprBoundary) + ((y>>3)*sprSize.width*8) + ((y&0x7)*8));
					else
						src = (u8 *)MMU_gpu_map(this->_sprMem + (spriteInfo.TileIndex<<this->_sprBoundary) + ((y>>3)*sprSize.width*4) + ((y&0x7)*4));
				}

				this->_RenderSpriteWin(src, (spriteInfo.PaletteMode == PaletteMode_1x256), lg, sprX, x, xdir);
			}
			else if (objMode == OBJMode_Bitmap) //sprite is in BMP format
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;
				
				srcadr = this->_SpriteAddressBMP(spriteInfo, sprSize, y);
				this->_RenderSpriteBMP<ISDEBUGRENDER>(i, lineIndex, dst, srcadr, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo.PaletteIndex);
			}
			else if (spriteInfo.PaletteMode == PaletteMode_1x256) //256 colors
			{
				if (MODE == SpriteRenderMode_Sprite2D)
					srcadr = this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
				else
					srcadr = this->_sprMem + (spriteInfo.TileIndex<<this->_sprBoundary) + ((y>>3)*sprSize.width*8) + ((y&0x7)*8);
				
				pal = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;
				this->_RenderSprite256<ISDEBUGRENDER>(i, lineIndex, dst, srcadr, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, (objMode == OBJMode_Transparent));
			}
			else // 16 colors
			{
				if (MODE == SpriteRenderMode_Sprite2D)
				{
					srcadr = this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*4);
				}
				else
				{
					srcadr = this->_sprMem + (spriteInfo.TileIndex<<this->_sprBoundary) + ((y>>3)*sprSize.width*4) + ((y&0x7)*4);
				}
				
				pal = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);
				this->_RenderSprite16<ISDEBUGRENDER>(i, lineIndex, dst, srcadr, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, (objMode == OBJMode_Transparent));
			}
		}
	}
}

u16* GPUEngineBase::_RenderLine_Layers(const u16 l)
{
	return (this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH));
}

template <bool ISFULLINTENSITYHINT>
void GPUEngineBase::ApplyMasterBrightness()
{
	const IOREG_MASTER_BRIGHT &MASTER_BRIGHT = this->_IORegisterMap->MASTER_BRIGHT;
	const u32 intensity = MASTER_BRIGHT.Intensity;
	
	if (!ISFULLINTENSITYHINT && (intensity == 0)) return;
	
	u16 *dst = this->renderedBuffer;
	const size_t pixCount = this->renderedWidth * this->renderedHeight;
	
	switch (MASTER_BRIGHT.Mode)
	{
		case GPUMasterBrightMode_Disable:
			break;
			
		case GPUMasterBrightMode_Up:
		{
			if (!ISFULLINTENSITYHINT && !this->_isMasterBrightFullIntensity)
			{
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				const __m128i intensity_vec128 = _mm_set1_epi16(intensity);
				
				const size_t ssePixCount = pixCount - (pixCount % 8);
				for (; i < ssePixCount; i += 8)
				{
					const __m128i dstColor_vec128 = _mm_load_si128((__m128i *)(dst + i));
					_mm_store_si128( (__m128i *)(dst + i), this->_ColorEffectIncreaseBrightness(dstColor_vec128, intensity_vec128) );
				}
#endif
				for (; i < pixCount; i++)
				{
					dst[i] = GPUEngineBase::_fadeInColors[intensity][ dst[i] & 0x7FFF ];
				}
			}
			else
			{
				// all white (optimization)
				memset_u16(dst, 0x7FFF, pixCount);
			}
			break;
		}
			
		case GPUMasterBrightMode_Down:
		{
			if (!ISFULLINTENSITYHINT && !this->_isMasterBrightFullIntensity)
			{
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				const __m128i intensity_vec128 = _mm_set1_epi16(intensity);
				
				const size_t ssePixCount = pixCount - (pixCount % 8);
				for (; i < ssePixCount; i += 8)
				{
					const __m128i dstColor_vec128 = _mm_load_si128((__m128i *)(dst + i));
					_mm_store_si128( (__m128i *)(dst + i), this->_ColorEffectDecreaseBrightness(dstColor_vec128, intensity_vec128) );
				}
#endif
				for (; i < pixCount; i++)
				{
					dst[i] = GPUEngineBase::_fadeOutColors[intensity][ dst[i] & 0x7FFF ];
				}
			}
			else
			{
				// all black (optimization)
				memset(dst, 0, pixCount * sizeof(u16));
			}
			break;
		}
			
		case GPUMasterBrightMode_Reserved:
			break;
	}
}

template<size_t WIN_NUM>
void GPUEngineBase::_SetupWindows(const u16 lineIndex)
{
	const u16 windowTop    = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Top : this->_IORegisterMap->WIN1V.Top;
	const u16 windowBottom = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Bottom : this->_IORegisterMap->WIN1V.Bottom;
	
	if (WIN_NUM == 0 && !this->_WIN0_ENABLED) goto allout;
	if (WIN_NUM == 1 && !this->_WIN1_ENABLED) goto allout;

	if (windowTop > windowBottom)
	{
		if((lineIndex < windowTop) && (lineIndex > windowBottom)) goto allout;
	}
	else
	{
		if((lineIndex < windowTop) || (lineIndex >= windowBottom)) goto allout;
	}

	//the x windows will apply for this scanline
	this->_curr_win[WIN_NUM] = this->_h_win[WIN_NUM];
	return;
	
allout:
	this->_curr_win[WIN_NUM] = GPUEngineBase::_winEmpty;
}

template<size_t WIN_NUM>
void GPUEngineBase::_UpdateWINH()
{
	//dont even waste any time in here if the window isnt enabled
	if (WIN_NUM == 0 && !this->_WIN0_ENABLED) return;
	if (WIN_NUM == 1 && !this->_WIN1_ENABLED) return;

	this->_needUpdateWINH[WIN_NUM] = false;
	const size_t windowLeft  = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0H.Left  : this->_IORegisterMap->WIN1H.Left;
	const size_t windowRight = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0H.Right : this->_IORegisterMap->WIN1H.Right;

	//the original logic: if you doubt the window code, please check it against the newer implementation below
	//if(windowLeft > windowRight)
	//{
	//	if((x < windowLeft) && (x > windowRight)) return false;
	//}
	//else
	//{
	//	if((x < windowLeft) || (x >= windowRight)) return false;
	//}

	if (windowLeft > windowRight)
	{
		memset(this->_h_win[WIN_NUM], 1, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
		memset(this->_h_win[WIN_NUM] + windowRight + 1, 0, (windowLeft - (windowRight + 1)) * sizeof(u8));
	}
	else
	{
		memset(this->_h_win[WIN_NUM], 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
		memset(this->_h_win[WIN_NUM] + windowLeft, 1, (windowRight - windowLeft) * sizeof(u8));
	}
}

void GPUEngineBase::UpdateVRAM3DUsageProperties_BGLayer(const size_t bankIndex)
{
	const bool isBG2UsingVRAM = this->_enableLayer[GPULayerID_BG2] && (this->_BGLayer[GPULayerID_BG2].type == BGType_AffineExt_Direct) && (this->_BGLayer[GPULayerID_BG2].size.width == 256) && (this->_BGLayer[GPULayerID_BG2].size.height == 256);
	const bool isBG3UsingVRAM = this->_enableLayer[GPULayerID_BG3] && (this->_BGLayer[GPULayerID_BG3].type == BGType_AffineExt_Direct) && (this->_BGLayer[GPULayerID_BG3].size.width == 256) && (this->_BGLayer[GPULayerID_BG3].size.height == 256);
	u8 selectedBGLayer = VRAM_NO_3D_USAGE;
	
	if (!isBG2UsingVRAM && !isBG3UsingVRAM)
	{
		return;
	}
	else if (!isBG2UsingVRAM && isBG3UsingVRAM)
	{
		selectedBGLayer = GPULayerID_BG3;
	}
	else if (isBG2UsingVRAM && !isBG3UsingVRAM)
	{
		selectedBGLayer = GPULayerID_BG2;
	}
	else if (isBG2UsingVRAM && isBG3UsingVRAM)
	{
		selectedBGLayer = (this->_BGLayer[GPULayerID_BG3].priority <= this->_BGLayer[GPULayerID_BG2].priority) ? GPULayerID_BG3 : GPULayerID_BG2;
	}
	
	if (selectedBGLayer != VRAM_NO_3D_USAGE)
	{
		const IOREG_BGnParameter *bgParams = (selectedBGLayer == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		const IOREG_BGnX &savedBGnX = (selectedBGLayer == GPULayerID_BG2) ? this->savedBG2X : this->savedBG3X;
		const IOREG_BGnY &savedBGnY = (selectedBGLayer == GPULayerID_BG2) ? this->savedBG2Y : this->savedBG3Y;
		
		if ( (bgParams->BGnPA.value != 0x100) ||
		     (bgParams->BGnPB.value !=     0) ||
		     (bgParams->BGnPC.value !=     0) ||
		     (bgParams->BGnPD.value != 0x100) ||
		     (savedBGnX.value       !=     0) ||
		     (savedBGnY.value       !=     0) )
		{
			selectedBGLayer = VRAM_NO_3D_USAGE;
		}
	}
	
	this->vramBGLayer = selectedBGLayer;
	this->vramBlockBGIndex = (selectedBGLayer != VRAM_NO_3D_USAGE) ? bankIndex : VRAM_NO_3D_USAGE;
}

void GPUEngineBase::UpdateVRAM3DUsageProperties_OBJLayer(const size_t bankIndex)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	if (!this->_enableLayer[GPULayerID_OBJ] || (DISPCNT.OBJ_BMP_mapping != 0) || (DISPCNT.OBJ_BMP_2D_dim == 0))
	{
		return;
	}
	
	const GPUEngineA *mainEngine = GPU->GetEngineMain();
	const IOREG_DISPCAPCNT &DISPCAPCNT = mainEngine->GetIORegisterMap().DISPCAPCNT;
	
	for (size_t spriteIndex = 0; spriteIndex < 128; spriteIndex++)
	{
		const OAMAttributes &spriteInfo = this->_oamList[spriteIndex];
		
		if ( ((spriteInfo.RotScale != 0) || (spriteInfo.Disable == 0)) && (spriteInfo.Mode == OBJMode_Bitmap) && (spriteInfo.PaletteIndex != 0) )
		{
			const u32 vramAddress = ((spriteInfo.TileIndex & 0x1F) << 5) + ((spriteInfo.TileIndex & ~0x1F) << 7);
			const SpriteSize sprSize = GPUEngineBase::_sprSizeTab[spriteInfo.Size][spriteInfo.Shape];
			
			if( (vramAddress == (DISPCAPCNT.VRAMWriteOffset * ADDRESS_STEP_32KB)) && (sprSize.width == 64) && (sprSize.height == 64) )
			{
				this->vramBlockOBJIndex = bankIndex;
				return;
			}
		}
	}
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_RenderLine_LayerBG_Final(u16 *dstColorLine, const u16 lineIndex)
{
	bool useCustomVRAM = false;
	
	switch (this->_BGLayer[LAYERID].baseType)
	{
		case BGType_Text: this->_LineText<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex); break;
		case BGType_Affine: this->_LineRot<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex); break;
		case BGType_AffineExt: dstColorLine = this->_LineExtRot<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, useCustomVRAM); break;
		case BGType_Large8bpp: dstColorLine = this->_LineExtRot<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex, useCustomVRAM); break;
		case BGType_Invalid:
			PROGINFO("Attempting to render an invalid BG type\n");
			break;
		default:
			break;
	}
	
	// If rendering at the native size, each pixel is rendered the moment it is gathered.
	// However, if rendering at a custom size, pixel gathering and pixel rendering are split
	// up into separate steps. If rendering at a custom size, do the pixel rendering step now.
	if ( !ISDEBUGRENDER && (ISCUSTOMRENDERINGNEEDED || !this->isLineRenderNative[lineIndex]) )
	{
		if (useCustomVRAM)
		{
			this->_RenderPixelsCustomVRAM<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDCustom, lineIndex);
		}
		else
		{
			this->_RenderPixelsCustom<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDCustom, lineIndex);
		}
	}
	
	return dstColorLine;
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_RenderLine_LayerBG_ApplyColorEffectDisabledHint(u16 *dstColorLine, const u16 lineIndex)
{
	return this->_RenderLine_LayerBG_Final<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_RenderLine_LayerBG_ApplyNoWindowsEnabledHint(u16 *dstColorLine, const u16 lineIndex)
{
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	
	if (BLDCNT.ColorEffect == ColorEffect_Disable)
	{
		dstColorLine = this->_RenderLine_LayerBG_ApplyColorEffectDisabledHint<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, true, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
	}
	else
	{
		dstColorLine = this->_RenderLine_LayerBG_ApplyColorEffectDisabledHint<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, false, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
	}
	
	return dstColorLine;
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_RenderLine_LayerBG_ApplyMosaic(u16 *dstColorLine, const u16 lineIndex)
{
	if (this->_isAnyWindowEnabled)
	{
		dstColorLine = this->_RenderLine_LayerBG_ApplyNoWindowsEnabledHint<LAYERID, ISDEBUGRENDER, MOSAIC, false, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
	}
	else
	{
		dstColorLine = this->_RenderLine_LayerBG_ApplyNoWindowsEnabledHint<LAYERID, ISDEBUGRENDER, MOSAIC, true, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
	}
	
	return dstColorLine;
}

template <GPULayerID LAYERID, bool ISDEBUGRENDER, bool ISCUSTOMRENDERINGNEEDED>
u16* GPUEngineBase::_RenderLine_LayerBG(u16 *dstColorLine, const u16 lineIndex)
{
	if (ISDEBUGRENDER)
	{
		return this->_RenderLine_LayerBG_Final<LAYERID, ISDEBUGRENDER, false, true, true, false>(dstColorLine, lineIndex);
	}
	else
	{
#ifndef DISABLE_MOSAIC
		if (this->_BGLayer[LAYERID].isMosaic && this->_isBGMosaicSet)
		{
			dstColorLine = this->_RenderLine_LayerBG_ApplyMosaic<LAYERID, ISDEBUGRENDER, true, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
		}
		else
#endif
		{
			dstColorLine = this->_RenderLine_LayerBG_ApplyMosaic<LAYERID, ISDEBUGRENDER, false, ISCUSTOMRENDERINGNEEDED>(dstColorLine, lineIndex);
		}
	}
	
	return dstColorLine;
}

template <GPULayerID LAYERID>
void GPUEngineBase::RenderLayerBG(u16 *dstColorBuffer)
{
	u16 *dstColorLine = dstColorBuffer;
	const size_t layerWidth = this->_BGLayer[LAYERID].size.width;
	const size_t layerHeight = this->_BGLayer[LAYERID].size.height;
	
	for (size_t lineIndex = 0; lineIndex < layerHeight; lineIndex++)
	{
		this->_RenderLine_LayerBG<LAYERID, true, false>(dstColorLine, lineIndex);
		dstColorLine += layerWidth;
	}
}

void GPUEngineBase::_HandleDisplayModeOff(const size_t l)
{
	// Native rendering only.
	// In this display mode, the display is cleared to white.
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0x7FFF);
}

void GPUEngineBase::_HandleDisplayModeNormal(const size_t l)
{
	if (!this->isLineRenderNative[l])
	{
		this->isLineOutputNative[l] = false;
		this->nativeLineOutputCount--;
	}
}

void GPUEngineBase::ParseReg_MOSAIC()
{
	this->_mosaicWidthBG = &GPUEngineBase::_mosaicLookup.table[this->_IORegisterMap->MOSAIC.BG_MosaicH][0];
	this->_mosaicHeightBG = &GPUEngineBase::_mosaicLookup.table[this->_IORegisterMap->MOSAIC.BG_MosaicV][0];
	this->_mosaicWidthOBJ = &GPUEngineBase::_mosaicLookup.table[this->_IORegisterMap->MOSAIC.OBJ_MosaicH][0];
	this->_mosaicHeightOBJ = &GPUEngineBase::_mosaicLookup.table[this->_IORegisterMap->MOSAIC.OBJ_MosaicV][0];
	
	this->_isBGMosaicSet = (this->_IORegisterMap->MOSAIC.BG_MosaicH != 0) || (this->_IORegisterMap->MOSAIC.BG_MosaicV != 0);
	this->_isOBJMosaicSet = (this->_IORegisterMap->MOSAIC.OBJ_MosaicH != 0) || (this->_IORegisterMap->MOSAIC.OBJ_MosaicV != 0);
}

void GPUEngineBase::ParseReg_BLDCNT()
{
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	
	this->_blend2[GPULayerID_BG0] = (BLDCNT.BG0_Target2 != 0);
	this->_blend2[GPULayerID_BG1] = (BLDCNT.BG1_Target2 != 0);
	this->_blend2[GPULayerID_BG2] = (BLDCNT.BG2_Target2 != 0);
	this->_blend2[GPULayerID_BG3] = (BLDCNT.BG3_Target2 != 0);
	this->_blend2[GPULayerID_OBJ] = (BLDCNT.OBJ_Target2 != 0);
	this->_blend2[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target2 != 0);
	
#ifdef ENABLE_SSSE3
	this->_blend2_SSSE3 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
									   BLDCNT.Backdrop_Target2,
									   BLDCNT.OBJ_Target2,
									   BLDCNT.BG3_Target2,
									   BLDCNT.BG2_Target2,
									   BLDCNT.BG1_Target2,
									   BLDCNT.BG0_Target2);
#endif
}

void GPUEngineBase::ParseReg_BLDALPHA()
{
	const IOREG_BLDALPHA &BLDALPHA = this->_IORegisterMap->BLDALPHA;
	
	this->_BLDALPHA_EVA = (BLDALPHA.EVA >= 16) ? 16 : BLDALPHA.EVA;
	this->_BLDALPHA_EVB = (BLDALPHA.EVB >= 16) ? 16 : BLDALPHA.EVB;
	this->_blendTable = (TBlendTable *)&GPUEngineBase::_blendTable555[this->_BLDALPHA_EVA][this->_BLDALPHA_EVB][0][0];
}

void GPUEngineBase::ParseReg_BLDY()
{
	const IOREG_BLDY &BLDY = this->_IORegisterMap->BLDY;
	
	this->_BLDALPHA_EVY = (BLDY.EVY >= 16) ? 16 : BLDY.EVY;
	this->_currentFadeInColors = &GPUEngineBase::_fadeInColors[this->_BLDALPHA_EVY][0];
	this->_currentFadeOutColors = &GPUEngineBase::_fadeOutColors[this->_BLDALPHA_EVY][0];
}

template<size_t WINNUM>
void GPUEngineBase::ParseReg_WINnH()
{
	this->_needUpdateWINH[WINNUM] = true;
}

const BGLayerInfo& GPUEngineBase::GetBGLayerInfoByID(const GPULayerID layerID)
{
	return this->_BGLayer[layerID];
}

NDSDisplayID GPUEngineBase::GetDisplayByID() const
{
	return this->_targetDisplayID;
}

void GPUEngineBase::SetDisplayByID(const NDSDisplayID theDisplayID)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->_targetDisplayID = theDisplayID;
	this->nativeBuffer = (u16 *)dispInfo.nativeBuffer[theDisplayID];
	this->customBuffer = (u16 *)dispInfo.customBuffer[theDisplayID];
}

GPUEngineID GPUEngineBase::GetEngineID() const
{
	return this->_engineID;
}

void GPUEngineBase::SetCustomFramebufferSize(size_t w, size_t h)
{
	u16 *oldWorkingScanline = this->_internalRenderLineTargetCustom;
	u8 *oldBGPixels = this->_renderLineLayerIDCustom;
	u8 *oldBGLayerIndexCustom = this->_bgLayerIndexCustom;
	u16 *oldBGLayerColorCustom = this->_bgLayerColorCustom;
	
	u16 *newWorkingScanline = (u16 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(u16));
	u8 *newBGPixels = (u8 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * 4 * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	u8 *newBGLayerIndexCustom = (u8 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(u8));
	u16 *newBGLayerColorCustom = (u16 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * sizeof(u16));
	
	this->_internalRenderLineTargetCustom = newWorkingScanline;
	this->_renderLineLayerIDCustom = newBGPixels;
	this->nativeBuffer = (u16 *)GPU->GetDisplayInfo().nativeBuffer[this->_targetDisplayID];
	this->customBuffer = (u16 *)GPU->GetDisplayInfo().customBuffer[this->_targetDisplayID];
	this->_bgLayerIndexCustom = newBGLayerIndexCustom;
	this->_bgLayerColorCustom = newBGLayerColorCustom;
	
	free_aligned(oldWorkingScanline);
	free_aligned(oldBGPixels);
	free_aligned(oldBGLayerIndexCustom);
	free_aligned(oldBGLayerColorCustom);
}

void GPUEngineBase::ResolveCustomRendering()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	if (this->nativeLineOutputCount == GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return;
	}
	else if (this->nativeLineOutputCount == 0)
	{
		this->renderedWidth = dispInfo.customWidth;
		this->renderedHeight = dispInfo.customHeight;
		this->renderedBuffer = this->customBuffer;
		return;
	}
	
	// Resolve any remaining native lines to the custom buffer
	for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
	{
		if (this->isLineOutputNative[y])
		{
			this->_LineColorCopy<false, true, true, false>(this->customBuffer, this->nativeBuffer, y);
			this->isLineOutputNative[y] = false;
		}
	}
	
	this->nativeLineOutputCount = 0;
	this->renderedWidth = dispInfo.customWidth;
	this->renderedHeight = dispInfo.customHeight;
	this->renderedBuffer = this->customBuffer;
}

void GPUEngineBase::ResolveToCustomFramebuffer()
{
	if (this->nativeLineOutputCount == 0)
	{
		return;
	}
	
	if (GPU->GetDisplayInfo().isCustomSizeRequested)
	{
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			this->_LineColorCopy<false, true, true, false>(this->customBuffer, this->nativeBuffer, y);
		}
	}
	else
	{
		memcpy(this->customBuffer, this->nativeBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	}
	
	GPU->SetDisplayDidCustomRender(this->_targetDisplayID, true);
}

void GPUEngineBase::RefreshAffineStartRegs()
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
	//I am REALLY unsatisfied with this logic now. But it seems to be working.
	
	this->_IORegisterMap->BG2X = this->savedBG2X;
	this->_IORegisterMap->BG2Y = this->savedBG2Y;
	this->_IORegisterMap->BG3X = this->savedBG3X;
	this->_IORegisterMap->BG3Y = this->savedBG3Y;
}

// normally should have same addresses
void GPUEngineBase::REG_DISPx_pack_test()
{
	const GPU_IOREG *r = this->_IORegisterMap;
	
	printf("%08lx %02x\n", (uintptr_t)r, (u32)((uintptr_t)(&r->DISPCNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISPSTAT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->VCOUNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BGnCNT[GPULayerID_BG0]) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BGnOFS[GPULayerID_BG0]) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BG2Param) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->BG3Param) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISP3DCNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISPCAPCNT) - (uintptr_t)r) );
	printf("\t%02x\n", (u32)((uintptr_t)(&r->DISP_MMEM_FIFO) - (uintptr_t)r) );
}

template<GPUEngineID ENGINEID>
void GPUEngineBase::ParseAllRegisters()
{
	this->ParseReg_DISPCNT<ENGINEID>();
	// No need to call ParseReg_BGnCNT<GPUEngineID, GPULayerID>(), since it is
	// already called by ParseReg_DISPCNT().
	
	this->ParseReg_BGnHOFS<GPULayerID_BG0>();
	this->ParseReg_BGnHOFS<GPULayerID_BG1>();
	this->ParseReg_BGnHOFS<GPULayerID_BG2>();
	this->ParseReg_BGnHOFS<GPULayerID_BG3>();
	this->ParseReg_BGnVOFS<GPULayerID_BG0>();
	this->ParseReg_BGnVOFS<GPULayerID_BG1>();
	this->ParseReg_BGnVOFS<GPULayerID_BG2>();
	this->ParseReg_BGnVOFS<GPULayerID_BG3>();
	
	this->ParseReg_BGnX<GPULayerID_BG2>();
	this->ParseReg_BGnY<GPULayerID_BG2>();
	this->ParseReg_BGnX<GPULayerID_BG3>();
	this->ParseReg_BGnY<GPULayerID_BG3>();
	
	this->ParseReg_WINnH<0>();
	this->ParseReg_WINnH<1>();
	
	this->ParseReg_MOSAIC();
	this->ParseReg_BLDCNT();
	this->ParseReg_BLDALPHA();
	this->ParseReg_BLDY();
	this->ParseReg_MASTER_BRIGHT();
}

GPUEngineA::GPUEngineA()
{
	_engineID = GPUEngineID_Main;
	_targetDisplayID = NDSDisplayID_Main;
	_IORegisterMap = (GPU_IOREG *)MMU.ARM9_REG;
	_paletteBG = (u16 *)MMU.ARM9_VMEM;
	_paletteOBJ = (u16 *)(MMU.ARM9_VMEM + ADDRESS_STEP_512B);
	_oamList = (OAMAttributes *)(MMU.ARM9_OAM);
	_sprMem = MMU_AOBJ;
	
	_VRAMNativeBlockPtr[0] = (u16 *)MMU.ARM9_LCD;
	_VRAMNativeBlockPtr[1] = _VRAMNativeBlockPtr[0] + (1 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockPtr[2] = _VRAMNativeBlockPtr[0] + (2 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockPtr[3] = _VRAMNativeBlockPtr[0] + (3 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	memset(this->_VRAMNativeBlockCaptureCopy, 0, GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
	_VRAMNativeBlockCaptureCopyPtr[0] = this->_VRAMNativeBlockCaptureCopy;
	_VRAMNativeBlockCaptureCopyPtr[1] = _VRAMNativeBlockCaptureCopyPtr[0] + (1 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockCaptureCopyPtr[2] = _VRAMNativeBlockCaptureCopyPtr[0] + (2 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	_VRAMNativeBlockCaptureCopyPtr[3] = _VRAMNativeBlockCaptureCopyPtr[0] + (3 * GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
	nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
	nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
	nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		isLineCaptureNative[0][l] = true;
		isLineCaptureNative[1][l] = true;
		isLineCaptureNative[2][l] = true;
		isLineCaptureNative[3][l] = true;
	}
	
	_3DFramebufferRGBA6665 = (FragmentColor *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(FragmentColor));
	_3DFramebufferRGBA5551 = (u16 *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	gfx3d_Update3DFramebuffers(_3DFramebufferRGBA6665, _3DFramebufferRGBA5551);
}

GPUEngineA::~GPUEngineA()
{
	free_aligned(this->_3DFramebufferRGBA6665);
	free_aligned(this->_3DFramebufferRGBA5551);
	gfx3d_Update3DFramebuffers(NULL, NULL);
}

GPUEngineA* GPUEngineA::Allocate()
{
	return new(malloc_aligned64(sizeof(GPUEngineA))) GPUEngineA();
}

void GPUEngineA::FinalizeAndDeallocate()
{
	this->~GPUEngineA();
	free_aligned(this);
}

void GPUEngineA::Reset()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	this->_Reset_Base();
	
	memset(&this->_dispCapCnt, 0, sizeof(DISPCAPCNT_parsed));
	
	this->_BGLayer[GPULayerID_BG0].BMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].BMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].BMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].BMPAddress = MMU_ABG;
	
	this->_BGLayer[GPULayerID_BG0].largeBMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].largeBMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].largeBMPAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].largeBMPAddress = MMU_ABG;
	
	this->_BGLayer[GPULayerID_BG0].tileMapAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].tileMapAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].tileMapAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].tileMapAddress = MMU_ABG;
	
	this->_BGLayer[GPULayerID_BG0].tileEntryAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG1].tileEntryAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG2].tileEntryAddress = MMU_ABG;
	this->_BGLayer[GPULayerID_BG3].tileEntryAddress = MMU_ABG;
	
	memset(this->_VRAMNativeBlockCaptureCopy, 0, GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH * 4);
	
	this->nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
	this->nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
	this->nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
	this->nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		this->isLineCaptureNative[0][l] = true;
		this->isLineCaptureNative[1][l] = true;
		this->isLineCaptureNative[2][l] = true;
		this->isLineCaptureNative[3][l] = true;
	}
	
	this->SetDisplayByID(NDSDisplayID_Main);
	
	memset(this->_3DFramebufferRGBA6665, 0, dispInfo.customWidth * dispInfo.customHeight * sizeof(FragmentColor));
	memset(this->_3DFramebufferRGBA5551, 0, dispInfo.customWidth * dispInfo.customHeight * sizeof(u16));
}

void GPUEngineA::ParseReg_DISPCAPCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	this->_dispCapCnt.EVA = (DISPCAPCNT.EVA >= 16) ? 16 : DISPCAPCNT.EVA;
	this->_dispCapCnt.EVB = (DISPCAPCNT.EVB >= 16) ? 16 : DISPCAPCNT.EVB;
	this->_dispCapCnt.readOffset = (DISPCNT.DisplayMode == GPUDisplayMode_VRAM) ? 0 : DISPCAPCNT.VRAMReadOffset;
	
	switch (DISPCAPCNT.CaptureSize)
	{
		case DisplayCaptureSize_128x128:
			this->_dispCapCnt.capy = 128;
			break;
			
		case DisplayCaptureSize_256x64:
			this->_dispCapCnt.capy = 64;
			break;
			
		case DisplayCaptureSize_256x128:
			this->_dispCapCnt.capy = 128;
			break;
			
		case DisplayCaptureSize_256x192:
			this->_dispCapCnt.capy = 192;
			break;
			
		default:
			break;
	}
	
	/*INFO("Capture 0x%X:\n EVA=%i, EVB=%i, wBlock=%i, wOffset=%i, capX=%i, capY=%i\n rBlock=%i, rOffset=%i, srcCap=%i, dst=0x%X, src=0x%X\n srcA=%i, srcB=%i\n\n",
	 val, this->_dispCapCnt.EVA, this->_dispCapCnt.EVB, this->_dispCapCnt.writeBlock, this->_dispCapCnt.writeOffset,
	 this->_dispCapCnt.capy, this->_dispCapCnt.readBlock, this->_dispCapCnt.readOffset,
	 this->_dispCapCnt.capSrc, this->_dispCapCnt.dst - MMU.ARM9_LCD, this->_dispCapCnt.src - MMU.ARM9_LCD,
	 this->_dispCapCnt.srcA, this->_dispCapCnt.srcB);*/
}

FragmentColor* GPUEngineA::Get3DFramebufferRGBA6665() const
{
	return this->_3DFramebufferRGBA6665;
}

u16* GPUEngineA::Get3DFramebufferRGBA5551() const
{
	return this->_3DFramebufferRGBA5551;
}

u16* GPUEngineA::GetCustomVRAMBlockPtr(const size_t blockID)
{
	return this->_VRAMCustomBlockPtr[blockID];
}

void GPUEngineA::SetCustomFramebufferSize(size_t w, size_t h)
{
	this->GPUEngineBase::SetCustomFramebufferSize(w, h);
	
	FragmentColor *oldColorRGBA6665Buffer = this->_3DFramebufferRGBA6665;
	u16 *oldColorRGBA5551Buffer = this->_3DFramebufferRGBA5551;
	FragmentColor *newColorRGBA6665Buffer = (FragmentColor *)malloc_alignedCacheLine(w * h * sizeof(FragmentColor));
	u16 *newColorRGBA5551 = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	
	this->_3DFramebufferRGBA6665 = newColorRGBA6665Buffer;
	this->_3DFramebufferRGBA5551 = newColorRGBA5551;
	gfx3d_Update3DFramebuffers(this->_3DFramebufferRGBA6665, this->_3DFramebufferRGBA5551);
	
	this->_VRAMCustomBlockPtr[0] = GPU->GetCustomVRAMBuffer();
	this->_VRAMCustomBlockPtr[1] = this->_VRAMCustomBlockPtr[0] + (1 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	this->_VRAMCustomBlockPtr[2] = this->_VRAMCustomBlockPtr[0] + (2 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	this->_VRAMCustomBlockPtr[3] = this->_VRAMCustomBlockPtr[0] + (3 * _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w);
	
	free_aligned(oldColorRGBA6665Buffer);
	free_aligned(oldColorRGBA5551Buffer);
}

bool GPUEngineA::WillRender3DLayer()
{
	return ( this->_enableLayer[GPULayerID_BG0] && (this->_IORegisterMap->DISPCNT.BG0_3D != 0) );
}

bool GPUEngineA::WillCapture3DLayerDirect()
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return ( (DISPCAPCNT.CaptureEnable != 0) && (DISPCAPCNT.SrcA != 0) && (DISPCAPCNT.CaptureSrc != 1) && (vramConfiguration.banks[DISPCAPCNT.VRAMWriteBlock].purpose == VramConfiguration::LCDC) );
}

bool GPUEngineA::VerifyVRAMLineDidChange(const size_t blockID, const size_t l)
{
	// This method must be called for ALL instances where captured lines in VRAM may be read back.
	//
	// If a line is captured at a custom size, we need to ensure that the line hasn't been changed between
	// capture time and read time. If the captured line has changed, then we need to fallback to using the
	// native captured line instead.
	
	if (this->isLineCaptureNative[blockID][l])
	{
		return false;
	}
	
	u16 *__restrict capturedNativeLine = this->_VRAMNativeBlockCaptureCopyPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	const u16 *__restrict currentNativeLine = this->_VRAMNativeBlockPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	const bool didVRAMLineChange = (memcmp(currentNativeLine, capturedNativeLine, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) != 0);
	if (didVRAMLineChange)
	{
		this->_LineColorCopy<true, true, true, false>(this->_VRAMNativeBlockCaptureCopyPtr[blockID], this->_VRAMNativeBlockPtr[blockID], l);
		this->isLineCaptureNative[blockID][l] = true;
		this->nativeLineCaptureCount[blockID]++;
	}
	
	return didVRAMLineChange;
}

void GPUEngineA::RenderLine(const u16 l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	//cache some parameters which are assumed to be stable throughout the rendering of the entire line
	if (this->_needUpdateWINH[0]) this->_UpdateWINH<0>();
	if (this->_needUpdateWINH[1]) this->_UpdateWINH<1>();
	
	this->_SetupWindows<0>(l);
	this->_SetupWindows<1>(l);
	
	// Render the line
	u16 *renderLineTarget = this->_RenderLine_Layers(l);
	
	// Fill the display output
	switch (this->_displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff(l);
			break;
			
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			this->_HandleDisplayModeNormal(l);
			break;
			
		case GPUDisplayMode_VRAM: // Display vram framebuffer
			this->_HandleDisplayModeVRAM(l);
			break;
			
		case GPUDisplayMode_MainMemory: // Display memory FIFO
			this->_HandleDisplayModeMainMemory(l);
			break;
	}
	
	//capture after displaying so that we can safely display vram before overwriting it here
	
	//BUG!!! if someone is capturing and displaying both from the fifo, then it will have been
	//consumed above by the display before we get here
	//(is that even legal? i think so)
	if ((DISPCAPCNT.CaptureEnable != 0) && (vramConfiguration.banks[DISPCAPCNT.VRAMWriteBlock].purpose == VramConfiguration::LCDC) && (l < this->_dispCapCnt.capy))
	{
		if (DISPCAPCNT.CaptureSize == DisplayCaptureSize_128x128)
		{
			this->_RenderLine_DisplayCapture<GPU_FRAMEBUFFER_NATIVE_WIDTH/2>(renderLineTarget, l);
		}
		else
		{
			this->_RenderLine_DisplayCapture<GPU_FRAMEBUFFER_NATIVE_WIDTH>(renderLineTarget, l);
		}
	}
}

u16* GPUEngineA::_RenderLine_Layers(const u16 l)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const size_t customLineWidth = dispInfo.customWidth;
	const size_t customLineCount = _gpuDstLineCount[l];
	const size_t customLineIndex = _gpuDstLineIndex[l];
	
	// Optimization: For normal display mode, render straight to the output buffer when that is what we are going to end
	// up displaying anyway. Otherwise, we need to use the working buffer.
	const bool isDisplayModeNormal = (this->_displayOutputMode == GPUDisplayMode_Normal);
	
	u16 *renderLineTargetNative = (isDisplayModeNormal) ? this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : this->_internalRenderLineTargetNative;
	u16 *renderLineTargetCustom = (isDisplayModeNormal) ? this->customBuffer + (customLineIndex * customLineWidth) : this->_internalRenderLineTargetCustom;
	u16 *currentRenderLineTarget = renderLineTargetNative;
	
	const u16 backdropColor = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	this->_RenderLine_Clear(backdropColor, l, renderLineTargetNative);
	
	itemsForPriority_t *__restrict item;
	
	// for all the pixels in the line
	if (this->_enableLayer[GPULayerID_OBJ])
	{
		//n.b. - this is clearing the sprite line buffer to the background color,
		memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_sprColor, backdropColor);
		
		//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
		//how it interacts with this. I wish we knew why we needed this
		
		this->_SpriteRender<false>(l, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
		this->_MosaicSpriteLine(l, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
		
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			// assign them to the good priority item
			const size_t prio = this->_sprPrio[i];
			if (prio >= 4) continue;
			
			item = &(this->_itemsForPriority[prio]);
			item->PixelsX[item->nbPixelsX] = i;
			item->nbPixelsX++;
		}
	}
	
	// paint lower priorities first
	// then higher priorities on top
	for (size_t prio = NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(this->_itemsForPriority[prio]);
		// render BGs
		if (this->_isAnyBGLayerEnabled)
		{
			for (size_t i = 0; i < item->nbBGs; i++)
			{
				const GPULayerID layerID = (GPULayerID)item->BGs[i];
				if (this->_enableLayer[layerID])
				{
					if ( (layerID == GPULayerID_BG0) && this->WillRender3DLayer() )
					{
						const FragmentColor *__restrict framebuffer3D = CurrentRenderer->GetFramebuffer();
						if (framebuffer3D == NULL)
						{
							continue;
						}
						
						if (this->isLineRenderNative[l] && !CurrentRenderer->IsFramebufferNativeSize())
						{
							u16 *newRenderLineTarget = renderLineTargetCustom;
							this->_LineColorCopy<false, true, false, false>(newRenderLineTarget, currentRenderLineTarget, l);
							this->_LineLayerIDCopy<false, true>(this->_renderLineLayerIDCustom, this->_renderLineLayerIDNative, l);
							currentRenderLineTarget = newRenderLineTarget;
							
							this->isLineRenderNative[l] = false;
							this->nativeLineRenderCount--;
						}
						
						const float customWidthScale = (float)customLineWidth / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
						const FragmentColor *__restrict srcLine = framebuffer3D + (customLineIndex * customLineWidth);
						u16 *__restrict dstColorLinePtr = currentRenderLineTarget;
						u8 *__restrict dstLayerIDPtr = (this->isLineRenderNative[l]) ? this->_renderLineLayerIDNative : this->_renderLineLayerIDCustom;
						
						// Horizontally offset the 3D layer by this amount.
						// Test case: Blowing up large objects in Nanostray 2 will cause the main screen to shake horizontally.
						const u16 hofs = (u16)( ((float)this->_BGLayer[GPULayerID_BG0].xOffset * customWidthScale) + 0.5f );
						
						if (hofs == 0)
						{
							for (size_t line = 0; line < customLineCount; line++)
							{
								size_t dstX = 0;
#ifdef ENABLE_SSE2
								const size_t ssePixCount = customLineWidth - (customLineWidth % 16);
								for (; dstX < ssePixCount; dstX += 16)
								{
									this->_RenderPixel3D_SSE2<true>(dstX,
																	srcLine + dstX,
																	dstColorLinePtr + dstX,
																	dstLayerIDPtr + dstX);
								}
#endif
								for (; dstX < customLineWidth; dstX++)
								{
									const size_t srcX = dstX;
									
									this->_RenderPixel3D(_gpuDstToSrcIndex[dstX],
														 srcLine[srcX],
														 dstColorLinePtr + dstX,
														 dstLayerIDPtr + dstX);
								}
								
								srcLine += customLineWidth;
								dstColorLinePtr += customLineWidth;
								dstLayerIDPtr += customLineWidth;
							}
						}
						else
						{
							for (size_t line = 0; line < customLineCount; line++)
							{
								for (size_t dstX = 0; dstX < customLineWidth; dstX++)
								{
									size_t srcX = dstX + hofs;
									if (srcX >= customLineWidth * 2)
									{
										srcX -= customLineWidth * 2;
									}
									
									if (srcX >= customLineWidth || srcLine[srcX].a == 0)
										continue;
									
									this->_RenderPixel3D(_gpuDstToSrcIndex[dstX],
														 srcLine[srcX],
														 dstColorLinePtr + dstX,
														 dstLayerIDPtr + dstX);
								}
								
								srcLine += customLineWidth;
								dstColorLinePtr += customLineWidth;
								dstLayerIDPtr += customLineWidth;
							}
						}
						
						continue;
					}
					
					if (this->isLineRenderNative[l])
					{
						switch (layerID)
						{
							case GPULayerID_BG0: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG0, false, false>(currentRenderLineTarget, l); break;
							case GPULayerID_BG1: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG1, false, false>(currentRenderLineTarget, l); break;
							case GPULayerID_BG2: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG2, false, false>(currentRenderLineTarget, l); break;
							case GPULayerID_BG3: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG3, false, false>(currentRenderLineTarget, l); break;
								
							default:
								break;
						}
					}
					else
					{
						switch (layerID)
						{
							case GPULayerID_BG0: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG0, false, true>(currentRenderLineTarget, l); break;
							case GPULayerID_BG1: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG1, false, true>(currentRenderLineTarget, l); break;
							case GPULayerID_BG2: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG2, false, true>(currentRenderLineTarget, l); break;
							case GPULayerID_BG3: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG3, false, true>(currentRenderLineTarget, l); break;
								
							default:
								break;
						}
					}
				} //layer enabled
			}
		}
		
		// render sprite Pixels
		if ( this->_enableLayer[GPULayerID_OBJ] && (item->nbPixelsX > 0) )
		{
			if (this->vramBlockOBJIndex != VRAM_NO_3D_USAGE)
			{
				if (GPU->GetEngineMain()->VerifyVRAMLineDidChange(this->vramBlockOBJIndex, l))
				{
					u16 *newRenderLineTarget = (isDisplayModeNormal) ? this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : this->_internalRenderLineTargetNative;
					this->_LineColorCopy<true, false, false, false>(newRenderLineTarget, currentRenderLineTarget, l);
					this->_LineLayerIDCopy<true, false>(this->_renderLineLayerIDNative, this->_renderLineLayerIDCustom, l);
					currentRenderLineTarget = newRenderLineTarget;
				}
			}
			
			const bool useCustomVRAM = (this->vramBlockOBJIndex != VRAM_NO_3D_USAGE) && !GPU->GetEngineMain()->isLineCaptureNative[this->vramBlockOBJIndex][l];
			const u16 *__restrict srcLine = (useCustomVRAM) ? GPU->GetEngineMain()->GetCustomVRAMBlockPtr(this->vramBlockOBJIndex) + (customLineIndex * customLineWidth) : NULL;
			if (this->isLineRenderNative[l] && useCustomVRAM)
			{
				u16 *newRenderLineTarget = renderLineTargetCustom;
				this->_LineColorCopy<false, true, false, false>(newRenderLineTarget, currentRenderLineTarget, l);
				this->_LineLayerIDCopy<false, true>(this->_renderLineLayerIDCustom, this->_renderLineLayerIDNative, l);
				currentRenderLineTarget = newRenderLineTarget;
				
				this->isLineRenderNative[l] = false;
				this->nativeLineRenderCount--;
			}
			
			u16 *__restrict dstColorLinePtr = currentRenderLineTarget;
			
			if (this->isLineRenderNative[l])
			{
				u8 *__restrict dstLayerIDPtr = this->_renderLineLayerIDNative;
				
				for (size_t i = 0; i < item->nbPixelsX; i++)
				{
					const size_t srcX = item->PixelsX[i];
					
					this->_RenderPixel<GPULayerID_OBJ, false, false, false>(srcX,
																			this->_sprColor[srcX],
																			this->_sprAlpha[srcX],
																			dstColorLinePtr + srcX,
																			dstLayerIDPtr + srcX);
				}
			}
			else
			{
				u8 *__restrict dstLayerIDPtr = this->_renderLineLayerIDCustom;
				
				for (size_t line = 0; line < customLineCount; line++)
				{
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = _gpuDstPitchIndex[srcX] + p;
							
							this->_RenderPixel<GPULayerID_OBJ, false, false, false>(srcX,
																					(useCustomVRAM) ? srcLine[dstX] : this->_sprColor[srcX],
																					this->_sprAlpha[srcX],
																					dstColorLinePtr + dstX,
																					dstLayerIDPtr + dstX);
						}
					}
					
					srcLine += customLineWidth;
					dstColorLinePtr += customLineWidth;
					dstLayerIDPtr += customLineWidth;
				}
			}
		}
	}
	
	return currentRenderLineTarget;
}

template<size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCapture(const u16 *renderedLineSrcA, const u16 l)
{
	assert( (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH/2) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) );
	
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const bool is3DFramebufferNativeSize = CurrentRenderer->IsFramebufferNativeSize();
	const u8 vramWriteBlock = DISPCAPCNT.VRAMWriteBlock;
	const u8 vramReadBlock = DISPCNT.VRAM_Block;
	const size_t writeLineIndexWithOffset = (DISPCAPCNT.VRAMWriteOffset * 64) + l;
	const size_t readLineIndexWithOffset = (this->_dispCapCnt.readOffset * 64) + l;
	bool newCaptureLineNativeState = true;
	
	//128-wide captures should write linearly into memory, with no gaps
	//this is tested by hotel dusk
	size_t cap_dst_adr = (DISPCAPCNT.VRAMWriteOffset * 64 * GPU_FRAMEBUFFER_NATIVE_WIDTH) + (l * CAPTURELENGTH);
	
	//Read/Write block wrap to 00000h when exceeding 1FFFFh (128k)
	//this has not been tested yet (I thought I needed it for hotel dusk, but it was fixed by the above)
	cap_dst_adr &= 0x0000FFFF;
	
	const u16 *cap_src = (this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset]) ? (u16 *)MMU.blank_memory : GPU->GetCustomVRAMBlankBuffer();
	u16 *cap_dst = this->_VRAMNativeBlockPtr[vramWriteBlock] + cap_dst_adr;
	
	if (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC)
	{
		if (this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset])
		{
			u32 cap_src_adr = readLineIndexWithOffset * GPU_FRAMEBUFFER_NATIVE_WIDTH;
			cap_src_adr &= 0x0000FFFF;
			cap_src = this->_VRAMNativeBlockPtr[vramReadBlock] + cap_src_adr;
		}
		else
		{
			size_t cap_src_adr_ext = ((this->_dispCapCnt.readOffset * _gpuCaptureLineIndex[64]) + _gpuCaptureLineIndex[l]) * dispInfo.customWidth;
			while (cap_src_adr_ext >= _gpuVRAMBlockOffset)
			{
				cap_src_adr_ext -= _gpuVRAMBlockOffset;
			}
			
			cap_src = this->_VRAMCustomBlockPtr[vramReadBlock] + cap_src_adr_ext;
		}
	}
	
	static CACHE_ALIGN u16 fifoLine[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	const u16 *srcA = (DISPCAPCNT.SrcA == 0) ? renderedLineSrcA : this->_3DFramebufferRGBA5551 + (_gpuDstLineIndex[l] * dispInfo.customWidth);
	const u16 *srcB = (DISPCAPCNT.SrcB == 0) ? cap_src : fifoLine;
	
	switch (DISPCAPCNT.CaptureSrc)
	{
		case 0: // Capture source is SourceA
		{
			//INFO("Capture source is SourceA\n");
			switch (DISPCAPCNT.SrcA)
			{
				case 0: // Capture screen (BG + OBJ + 3D)
				{
					//INFO("Capture screen (BG + OBJ + 3D)\n");
					if (this->isLineRenderNative[l])
					{
						this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = this->isLineRenderNative[l];
					break;
				}
					
				case 1: // Capture 3D
				{
					//INFO("Capture 3D\n");
					if (is3DFramebufferNativeSize)
					{
						this->_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, false, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = is3DFramebufferNativeSize;
					break;
				}
			}
			break;
		}
			
		case 1: // Capture source is SourceB
		{
			//INFO("Capture source is SourceB\n");
			switch (DISPCAPCNT.SrcB)
			{
				case 0: // Capture VRAM
				{
					this->VerifyVRAMLineDidChange(vramReadBlock, readLineIndexWithOffset);
					
					if (this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset])
					{
						this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, true>(srcB, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, true>(srcB, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset];
					break;
				}
					
				case 1: // Capture dispfifo (not yet tested)
				{
					this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
					this->_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, true>(srcB, cap_dst, CAPTURELENGTH, 1);
					newCaptureLineNativeState = true;
					break;
				}
			}
			break;
		}
			
		default: // Capture source is SourceA+B blended
		{
			//INFO("Capture source is SourceA+B blended\n");
			this->VerifyVRAMLineDidChange(vramReadBlock, readLineIndexWithOffset);
			
			if (DISPCAPCNT.SrcA == 0)
			{
				if ( (DISPCAPCNT.SrcB == 0) && !this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
				{
					if (this->isLineRenderNative[l])
					{
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, false, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, false, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = false;
				}
				else
				{
					if (DISPCAPCNT.SrcB != 0)
					{
						// fifo - tested by splinter cell chaos theory thermal view
						this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
					}
					
					if (this->isLineRenderNative[l])
					{
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = this->isLineRenderNative[l];
				}
			}
			else
			{
				if (is3DFramebufferNativeSize)
				{
					if ( (DISPCAPCNT.SrcB == 0) && !this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
					{
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, false, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
						newCaptureLineNativeState = false;
					}
					else
					{
						if (DISPCAPCNT.SrcB != 0)
						{
							// fifo - tested by splinter cell chaos theory thermal view
							this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
						}
						
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
						newCaptureLineNativeState = true;
					}
				}
				else
				{
					if ( (DISPCAPCNT.SrcB == 0) && !this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
					{
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, false, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						if (DISPCAPCNT.SrcB != 0)
						{
							// fifo - tested by splinter cell chaos theory thermal view
							this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
						}
						
						this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, true>(srcA, srcB, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = false;
				}
			}
			break;
		}
	}
	
#ifdef ENABLE_SSE2
	MACRODO_N( CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_stream_si128((__m128i *)(this->_VRAMNativeBlockCaptureCopyPtr[vramWriteBlock] + cap_dst_adr) + (X), _mm_load_si128((__m128i *)cap_dst + (X))) );
#else
	memcpy(this->_VRAMNativeBlockCaptureCopyPtr[vramWriteBlock] + cap_dst_adr, cap_dst, CAPTURELENGTH * sizeof(u16));
#endif
	
	if (this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] && !newCaptureLineNativeState)
	{
		this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] = false;
		this->nativeLineCaptureCount[vramWriteBlock]--;
	}
	else if (!this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] && newCaptureLineNativeState)
	{
		this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset] = true;
		this->nativeLineCaptureCount[vramWriteBlock]++;
	}
	
	if (!this->isLineCaptureNative[vramWriteBlock][writeLineIndexWithOffset])
	{
		const size_t captureLengthExt = (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? dispInfo.customWidth : dispInfo.customWidth / 2;
		const size_t captureLineCount = _gpuCaptureLineCount[l];
		
		size_t cap_dst_adr_ext = (DISPCAPCNT.VRAMWriteOffset * _gpuCaptureLineIndex[64] * dispInfo.customWidth) + (_gpuCaptureLineIndex[l] * captureLengthExt);
		while (cap_dst_adr_ext >= _gpuVRAMBlockOffset)
		{
			cap_dst_adr_ext -= _gpuVRAMBlockOffset;
		}
		
		u16 *cap_dst_ext = this->_VRAMCustomBlockPtr[vramWriteBlock] + cap_dst_adr_ext;
		
		switch (DISPCAPCNT.CaptureSrc)
		{
			case 0: // Capture source is SourceA
			{
				switch (DISPCAPCNT.SrcA)
				{
					case 0: // Capture screen (BG + OBJ + 3D)
					{
						if (this->isLineRenderNative[l])
						{
							this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						break;
					}
						
					case 1: // Capture 3D
					{
						if (is3DFramebufferNativeSize)
						{
							this->_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, false, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						break;
					}
				}
				break;
			}
				
			case 1: // Capture source is SourceB
			{
				switch (DISPCAPCNT.SrcB)
				{
					case 0: // Capture VRAM
					{
						if (this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset])
						{
							this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, true, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Copy<0, CAPTURELENGTH, false, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						break;
					}
						
					case 1: // Capture dispfifo (not yet tested)
						this->_RenderLine_DispCapture_Copy<1, CAPTURELENGTH, true, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						break;
				}
				break;
			}
				
			default: // Capture source is SourceA+B blended
			{
				if (DISPCAPCNT.SrcA == 0)
				{
					if ( (DISPCAPCNT.SrcB == 0) && !this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
					{
						if (this->isLineRenderNative[l])
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, false, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, false, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
					}
					else
					{
						if (this->isLineRenderNative[l])
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, true, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
					}
				}
				else
				{
					if (is3DFramebufferNativeSize)
					{
						if ( (DISPCAPCNT.SrcB == 0) && !this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, false, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, true, true, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
					}
					else
					{
						if ( (DISPCAPCNT.SrcB == 0) && !this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset] )
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, false, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Blend<CAPTURELENGTH, false, true, false>(srcA, srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
					}
				}
				break;
			}
		}
	}
}

void GPUEngineA::_RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer)
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
void GPUEngineA::_RenderLine_DispCapture_Copy(const u16 *src, u16 *dst, const size_t captureLengthExt, const size_t captureLineCount)
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
			MACRODO_N(CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_store_si128((__m128i *)dst + (X), _mm_or_si128( _mm_load_si128( (__m128i *)src + (X)), alpha_vec128 ) ));
#else
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				dst[i] = LE_TO_LOCAL_16(src[i] | alphaBit);
			}
#endif
		}
		else
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				dst[i] = LE_TO_LOCAL_16(src[_gpuDstPitchIndex[i]] | alphaBit);
			}
		}
	}
	else
	{
		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		
		if (CAPTUREFROMNATIVESRC)
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				for (size_t p = 0; p < _gpuDstPitchCount[i]; p++)
				{
					dst[_gpuDstPitchIndex[i] + p] = LE_TO_LOCAL_16(src[i] | alphaBit);
				}
			}
			
			for (size_t line = 1; line < captureLineCount; line++)
			{
				memcpy(dst + (line * dispInfo.customWidth), dst, captureLengthExt * sizeof(u16));
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
					dst[i] = LE_TO_LOCAL_16(src[i] | alphaBit);
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
						dst[i] = LE_TO_LOCAL_16(src[i] | alphaBit);
					}
					
					src += dispInfo.customWidth;
					dst += dispInfo.customWidth;
				}
			}
		}
	}
}

u16 GPUEngineA::_RenderLine_DispCapture_BlendFunc(const u16 srcA, const u16 srcB, const u8 blendEVA, const u8 blendEVB)
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
	
	return LOCAL_TO_LE_16(a | (b << 10) | (g << 5) | r);
}

#ifdef ENABLE_SSE2
__m128i GPUEngineA::_RenderLine_DispCapture_BlendFunc_SSE2(__m128i &srcA, __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB)
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
void GPUEngineA::_RenderLine_DispCapture_BlendToCustomDstBuffer(const u16 *srcA, const u16 *srcB, u16 *dst, const u8 blendEVA, const u8 blendEVB, const size_t length, size_t l)
{
#ifdef ENABLE_SSE2
	const __m128i blendEVA_vec128 = _mm_set1_epi16(blendEVA);
	const __m128i blendEVB_vec128 = _mm_set1_epi16(blendEVB);
#endif
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	size_t offset = _gpuDstToSrcIndex[_gpuDstLineIndex[l] * dispInfo.customWidth] - (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
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
		
		_mm_store_si128( (__m128i *)(dst + i), this->_RenderLine_DispCapture_BlendFunc_SSE2(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
	}
#endif
	for (; i < length; i++)
	{
		const u16 colorA = (!CAPTUREFROMNATIVESRCA) ? srcA[i] : srcA[offset + i];
		const u16 colorB = (!CAPTUREFROMNATIVESRCB) ? srcB[i] : srcB[offset + i];
		
		dst[i] = this->_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
	}
}

template<size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRCA, bool CAPTUREFROMNATIVESRCB, bool CAPTURETONATIVEDST>
void GPUEngineA::_RenderLine_DispCapture_Blend(const u16 *srcA, const u16 *srcB, u16 *dst, const size_t captureLengthExt, const size_t l)
{
	const u8 blendEVA = GPU->GetEngineMain()->_dispCapCnt.EVA;
	const u8 blendEVB = GPU->GetEngineMain()->_dispCapCnt.EVB;
	
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
			
			__m128i srcB_vec128 = (CAPTUREFROMNATIVESRCB) ? _mm_load_si128((__m128i *)(srcB + i)) : _mm_set_epi16(srcB[_gpuDstPitchIndex[i+7]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+6]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+5]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+4]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+3]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+2]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+1]],
			                                                                                                      srcB[_gpuDstPitchIndex[i+0]]);
			
			_mm_store_si128( (__m128i *)(dst + i), this->_RenderLine_DispCapture_BlendFunc_SSE2(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
		}
#else
		for (size_t i = 0; i < CAPTURELENGTH; i++)
		{
			const u16 colorA = (CAPTUREFROMNATIVESRCA) ? srcA[i] : srcA[_gpuDstPitchIndex[i]];
			const u16 colorB = (CAPTUREFROMNATIVESRCB) ? srcB[i] : srcB[_gpuDstPitchIndex[i]];
			
			dst[i] = this->_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
		}
#endif
	}
	else
	{
		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		const size_t captureLineCount = _gpuCaptureLineCount[l];
		
		if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			this->_RenderLine_DispCapture_BlendToCustomDstBuffer<CAPTUREFROMNATIVESRCA, CAPTUREFROMNATIVESRCB>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt * captureLineCount, l);
		}
		else
		{
			for (size_t line = 0; line < captureLineCount; line++)
			{
				this->_RenderLine_DispCapture_BlendToCustomDstBuffer<CAPTUREFROMNATIVESRCA, CAPTUREFROMNATIVESRCB>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt, l);
				srcA += dispInfo.customWidth;
				srcB += dispInfo.customWidth;
				dst += dispInfo.customWidth;
			}
		}
	}
}

void GPUEngineA::_HandleDisplayModeVRAM(const size_t l)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, l);
	
	if (this->isLineCaptureNative[DISPCNT.VRAM_Block][l])
	{
		this->_LineColorCopy<true, true, true, true>(this->nativeBuffer, this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block], l);
	}
	else
	{
		this->_LineColorCopy<false, false, true, true>(this->customBuffer, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], l);
		this->isLineOutputNative[l] = false;
		this->nativeLineOutputCount--;
	}
}

void GPUEngineA::_HandleDisplayModeMainMemory(const size_t l)
{
	// Native rendering only.
	//
	//this has not been tested since the dma timing for dispfifo was changed around the time of
	//newemuloop. it may not work.
	
	u32 *dstColorLine = (u32 *)(this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH));
	
#ifdef ENABLE_SSE2
	const __m128i fifoMask = _mm_set1_epi32(0x7FFF7FFF);
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
	{
		__m128i fifoColor = _mm_set_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
		fifoColor = _mm_shuffle_epi32(fifoColor, 0x1B); // We need to shuffle the four FIFO values back into the correct order, since they were originally loaded in reverse order.
		_mm_store_si128((__m128i *)dstColorLine + i, _mm_and_si128(fifoColor, fifoMask));
	}
#else
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
	{
		dstColorLine[i] = DISP_FIFOrecv() & 0x7FFF7FFF;
	}
#endif
}

template<GPULayerID LAYERID, bool ISDEBUGRENDER, bool MOSAIC, bool NOWINDOWSENABLEDHINT, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineA::_LineLarge8bpp(u16 *__restrict dstColorLine, const u16 lineIndex)
{
	u16 XBG = this->_IORegisterMap->BGnOFS[LAYERID].BGnHOFS.Offset;
	u16 YBG = lineIndex + this->_IORegisterMap->BGnOFS[LAYERID].BGnVOFS.Offset;
	u16 lg = this->_BGLayer[LAYERID].size.width;
	u16 ht = this->_BGLayer[LAYERID].size.height;
	u16 wmask = (lg-1);
	u16 hmask = (ht-1);
	YBG &= hmask;
	
	//TODO - handle wrapping / out of bounds correctly from rot_scale_op?
	
	u32 tmp_map = this->_BGLayer[LAYERID].largeBMPAddress + lg * YBG;
	u8 *__restrict map = (u8 *)MMU_gpu_map(tmp_map);
	
	for (size_t x = 0; x < lg; ++x, ++XBG)
	{
		XBG &= wmask;
		
		if (ISCUSTOMRENDERINGNEEDED)
		{
			this->_bgLayerIndex[x] = map[XBG];
			this->_bgLayerColor[x] = LE_TO_LOCAL_16(this->_paletteBG[this->_bgLayerIndex[x]]);
		}
		else
		{
			const u8 index = map[XBG];
			const u16 color = LE_TO_LOCAL_16(this->_paletteBG[index]);
			this->_RenderPixelSingle<LAYERID, ISDEBUGRENDER, MOSAIC, NOWINDOWSENABLEDHINT, COLOREFFECTDISABLEDHINT>(dstColorLine, this->_renderLineLayerIDNative, lineIndex, color, x, (color != 0));
		}
	}
}

void GPUEngineA::FramebufferPostprocess()
{
	this->GPUEngineBase::FramebufferPostprocess();
	
	this->_IORegisterMap->DISPCAPCNT.CaptureEnable = 0;
	DISP_FIFOreset();
}

GPUEngineB::GPUEngineB()
{
	_engineID = GPUEngineID_Sub;
	_targetDisplayID = NDSDisplayID_Touch;
	_IORegisterMap = (GPU_IOREG *)(&MMU.ARM9_REG[REG_DISPB]);
	_paletteBG = (u16 *)(MMU.ARM9_VMEM + ADDRESS_STEP_1KB);
	_paletteOBJ = (u16 *)(MMU.ARM9_VMEM + ADDRESS_STEP_1KB + ADDRESS_STEP_512B);
	_oamList = (OAMAttributes *)(MMU.ARM9_OAM + ADDRESS_STEP_1KB);
	_sprMem = MMU_BOBJ;
}

GPUEngineB::~GPUEngineB()
{
}

GPUEngineB* GPUEngineB::Allocate()
{
	return new(malloc_aligned64(sizeof(GPUEngineB))) GPUEngineB();
}

void GPUEngineB::FinalizeAndDeallocate()
{
	this->~GPUEngineB();
	free_aligned(this);
}

void GPUEngineB::Reset()
{
	this->_Reset_Base();
	
	this->_BGLayer[GPULayerID_BG0].BMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].BMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].BMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].BMPAddress = MMU_BBG;
	
	this->_BGLayer[GPULayerID_BG0].largeBMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].largeBMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].largeBMPAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].largeBMPAddress = MMU_BBG;
	
	this->_BGLayer[GPULayerID_BG0].tileMapAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].tileMapAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].tileMapAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].tileMapAddress = MMU_BBG;
	
	this->_BGLayer[GPULayerID_BG0].tileEntryAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG1].tileEntryAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG2].tileEntryAddress = MMU_BBG;
	this->_BGLayer[GPULayerID_BG3].tileEntryAddress = MMU_BBG;
	
	this->SetDisplayByID(NDSDisplayID_Touch);
}

void GPUEngineB::RenderLine(const u16 l)
{
	//cache some parameters which are assumed to be stable throughout the rendering of the entire line
	if (this->_needUpdateWINH[0]) this->_UpdateWINH<0>();
	if (this->_needUpdateWINH[1]) this->_UpdateWINH<1>();
	
	this->_SetupWindows<0>(l);
	this->_SetupWindows<1>(l);
	
	switch (this->_displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff(l);
			break;
		
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			this->_RenderLine_Layers(l);
			this->_HandleDisplayModeNormal(l);
			break;
		
		default:
			break;
	}
}

u16* GPUEngineB::_RenderLine_Layers(const u16 l)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const size_t customLineWidth = dispInfo.customWidth;
	const size_t customLineCount = _gpuDstLineCount[l];
	const size_t customLineIndex = _gpuDstLineIndex[l];
	
	u16 *currentRenderLineTarget = this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	itemsForPriority_t *__restrict item;
	
	const u16 backdropColor = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	this->_RenderLine_Clear(backdropColor, l, currentRenderLineTarget);
	
	// for all the pixels in the line
	if (this->_enableLayer[GPULayerID_OBJ])
	{
		//n.b. - this is clearing the sprite line buffer to the background color,
		memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_sprColor, backdropColor);
		
		//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
		//how it interacts with this. I wish we knew why we needed this
		
		this->_SpriteRender<false>(l, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
		this->_MosaicSpriteLine(l, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
		
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			// assign them to the good priority item
			const size_t prio = this->_sprPrio[i];
			if (prio >= 4) continue;
			
			item = &(this->_itemsForPriority[prio]);
			item->PixelsX[item->nbPixelsX] = i;
			item->nbPixelsX++;
		}
	}
	
	// paint lower priorities first
	// then higher priorities on top
	for (size_t prio = NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(this->_itemsForPriority[prio]);
		// render BGs
		if (this->_isAnyBGLayerEnabled)
		{
			for (size_t i = 0; i < item->nbBGs; i++)
			{
				const GPULayerID layerID = (GPULayerID)item->BGs[i];
				if (this->_enableLayer[layerID])
				{
					if (this->isLineRenderNative[l])
					{
						switch (layerID)
						{
							case GPULayerID_BG0: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG0, false, false>(currentRenderLineTarget, l); break;
							case GPULayerID_BG1: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG1, false, false>(currentRenderLineTarget, l); break;
							case GPULayerID_BG2: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG2, false, false>(currentRenderLineTarget, l); break;
							case GPULayerID_BG3: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG3, false, false>(currentRenderLineTarget, l); break;
								
							default:
								break;
						}
					}
					else
					{
						switch (layerID)
						{
							case GPULayerID_BG0: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG0, false, true>(currentRenderLineTarget, l); break;
							case GPULayerID_BG1: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG1, false, true>(currentRenderLineTarget, l); break;
							case GPULayerID_BG2: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG2, false, true>(currentRenderLineTarget, l); break;
							case GPULayerID_BG3: currentRenderLineTarget = this->_RenderLine_LayerBG<GPULayerID_BG3, false, true>(currentRenderLineTarget, l); break;
								
							default:
								break;
						}
					}
				} //layer enabled
			}
		}
		
		// render sprite Pixels
		if ( this->_enableLayer[GPULayerID_OBJ] && (item->nbPixelsX > 0) )
		{
			if (this->vramBlockOBJIndex != VRAM_NO_3D_USAGE)
			{
				if (GPU->GetEngineMain()->VerifyVRAMLineDidChange(this->vramBlockOBJIndex, l))
				{
					u16 *newRenderLineTarget = (this->_displayOutputMode == GPUDisplayMode_Normal) ? this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH) : this->_internalRenderLineTargetNative;
					this->_LineColorCopy<true, false, false, false>(newRenderLineTarget, currentRenderLineTarget, l);
					this->_LineLayerIDCopy<true, false>(this->_renderLineLayerIDNative, this->_renderLineLayerIDCustom, l);
					currentRenderLineTarget = newRenderLineTarget;
				}
			}
			
			const bool useCustomVRAM = (this->vramBlockOBJIndex != VRAM_NO_3D_USAGE) && !GPU->GetEngineMain()->isLineCaptureNative[this->vramBlockOBJIndex][l];
			const u16 *__restrict srcLine = (useCustomVRAM) ? GPU->GetEngineMain()->GetCustomVRAMBlockPtr(this->vramBlockOBJIndex) + (customLineIndex * customLineWidth) : NULL;
			if (this->isLineRenderNative[l] && useCustomVRAM)
			{
				u16 *newRenderLineTarget = this->customBuffer + (customLineIndex * customLineWidth);
				this->_LineColorCopy<false, true, false, false>(newRenderLineTarget, currentRenderLineTarget, l);
				this->_LineLayerIDCopy<false, true>(this->_renderLineLayerIDCustom, this->_renderLineLayerIDNative, l);
				currentRenderLineTarget = newRenderLineTarget;
				
				this->isLineRenderNative[l] = false;
				this->nativeLineRenderCount--;
			}
			
			u16 *__restrict dstColorLinePtr = currentRenderLineTarget;
			
			if (this->isLineRenderNative[l])
			{
				u8 *__restrict dstLayerIDPtr = this->_renderLineLayerIDNative;
				
				for (size_t i = 0; i < item->nbPixelsX; i++)
				{
					const size_t srcX = item->PixelsX[i];
					
					this->_RenderPixel<GPULayerID_OBJ, false, false, false>(srcX,
																			this->_sprColor[srcX],
																			this->_sprAlpha[srcX],
																			dstColorLinePtr + srcX,
																			dstLayerIDPtr + srcX);
				}
			}
			else
			{
				u8 *__restrict dstLayerIDPtr = this->_renderLineLayerIDCustom;
				
				for (size_t line = 0; line < customLineCount; line++)
				{
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = _gpuDstPitchIndex[srcX] + p;
							
							this->_RenderPixel<GPULayerID_OBJ, false, false, false>(srcX,
																					(useCustomVRAM) ? srcLine[dstX] : this->_sprColor[srcX],
																					this->_sprAlpha[srcX],
																					dstColorLinePtr + dstX,
																					dstLayerIDPtr + dstX);
						}
					}
					
					srcLine += customLineWidth;
					dstColorLinePtr += customLineWidth;
					dstLayerIDPtr += customLineWidth;
				}
			}
		}
	}
	
	return currentRenderLineTarget;
}

GPUSubsystem::GPUSubsystem()
{
	_defaultEventHandler = new GPUEventHandlerDefault;
	_event = _defaultEventHandler;
	
	gfx3d_init();
	
	_engineMain = GPUEngineA::Allocate();
	_engineSub = GPUEngineB::Allocate();
	
	_displayMain = new NDSDisplay(NDSDisplayID_Main);
	_displayMain->SetEngine(_engineMain);
	_displayTouch = new NDSDisplay(NDSDisplayID_Touch);
	_displayTouch->SetEngine(_engineSub);
	
	_willAutoResolveToCustomBuffer = true;
	
	OSDCLASS *previousOSD = osd;
	osd = new OSDCLASS(-1);
	delete previousOSD;
	
	_customVRAM = NULL;
	_customVRAMBlank = NULL;
	_customFramebuffer = (u16 *)malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2);
	
	_displayInfo.colorFormat = NDSColorFormat_BGR555_Rev;
	_displayInfo.pixelBytes = sizeof(u16);
	_displayInfo.isCustomSizeRequested = false;
	_displayInfo.customWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.customHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_displayInfo.masterNativeBuffer = _nativeFramebuffer;
	_displayInfo.nativeBuffer[NDSDisplayID_Main] = _displayInfo.masterNativeBuffer;
	_displayInfo.nativeBuffer[NDSDisplayID_Touch] = (u8 *)_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * _displayInfo.pixelBytes);
	
	_displayInfo.masterCustomBuffer = _customFramebuffer;
	_displayInfo.customBuffer[NDSDisplayID_Main] = _displayInfo.masterCustomBuffer;
	_displayInfo.customBuffer[NDSDisplayID_Touch] = (u8 *)_displayInfo.masterCustomBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * _displayInfo.pixelBytes);
	
	_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = false;
	_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	_displayInfo.renderedWidth[NDSDisplayID_Main] = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.renderedWidth[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.renderedHeight[NDSDisplayID_Main] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_displayInfo.renderedBuffer[NDSDisplayID_Main] = _displayInfo.nativeBuffer[NDSDisplayID_Main];
	_displayInfo.renderedBuffer[NDSDisplayID_Touch] = _displayInfo.nativeBuffer[NDSDisplayID_Touch];
	
	ClearWithColor(0x8000);
}

GPUSubsystem::~GPUSubsystem()
{
	delete osd;
	osd = NULL;
	
	free_aligned(this->_customFramebuffer);
	free_aligned(this->_customVRAM);
	
	free_aligned(_gpuDstToSrcIndex);
	_gpuDstToSrcIndex = NULL;
	
	free_aligned(_gpuDstToSrcSSSE3_u8);
	_gpuDstToSrcSSSE3_u8 = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u16);
	_gpuDstToSrcSSSE3_u16 = NULL;
	
	delete _displayMain;
	delete _displayTouch;
	_engineMain->FinalizeAndDeallocate();
	_engineSub->FinalizeAndDeallocate();
	
	gfx3d_deinit();
	
	delete _defaultEventHandler;
}

GPUSubsystem* GPUSubsystem::Allocate()
{
	return new(malloc_aligned64(sizeof(GPUSubsystem))) GPUSubsystem();
}

void GPUSubsystem::FinalizeAndDeallocate()
{
	this->~GPUSubsystem();
	free_aligned(this);
}

void GPUSubsystem::SetEventHandler(GPUEventHandler *eventHandler)
{
	this->_event = eventHandler;
}

GPUEventHandler* GPUSubsystem::GetEventHandler()
{
	return this->_event;
}

void GPUSubsystem::Reset()
{
	if (this->_customVRAM == NULL)
	{
		this->SetCustomFramebufferSize(this->_displayInfo.customWidth, this->_displayInfo.customHeight);
	}
	
	this->ClearWithColor(0xFFFF);
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main]  = false;
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterNativeBuffer;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterCustomBuffer + (this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes);
	this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	
	this->_displayMain->SetEngineByID(GPUEngineID_Main);
	this->_displayTouch->SetEngineByID(GPUEngineID_Sub);
	
	gfx3d_reset();
	this->_engineMain->Reset();
	this->_engineSub->Reset();
	
	DISP_FIFOreset();
	osd->clear();
}

void GPUSubsystem::UpdateRenderProperties()
{
	this->_engineMain->isCustomRenderingNeeded = false;
	this->_engineMain->vramBlockBGIndex = VRAM_NO_3D_USAGE;
	this->_engineMain->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	this->_engineMain->vramBGLayer = VRAM_NO_3D_USAGE;
	this->_engineMain->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_engineMain->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineMain->renderedBuffer = this->_engineMain->nativeBuffer;
	
	this->_engineSub->isCustomRenderingNeeded = false;
	this->_engineSub->vramBlockBGIndex = VRAM_NO_3D_USAGE;
	this->_engineSub->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	this->_engineSub->vramBGLayer = VRAM_NO_3D_USAGE;
	this->_engineSub->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_engineSub->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineSub->renderedBuffer = this->_engineSub->nativeBuffer;
	
	this->_engineMain->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineMain->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineSub->nativeLineRenderCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineSub->nativeLineOutputCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->_engineMain->isLineRenderNative[l] = true;
		this->_engineMain->isLineOutputNative[l] = true;
		this->_engineSub->isLineRenderNative[l] = true;
		this->_engineSub->isLineOutputNative[l] = true;
	}
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = false;
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main] = this->_displayMain->GetEngine()->nativeBuffer;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_displayMain->GetEngine()->renderedBuffer;
	this->_displayInfo.renderedWidth[NDSDisplayID_Main] = this->_displayMain->GetEngine()->renderedWidth;
	this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_displayMain->GetEngine()->renderedHeight;
	
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->nativeBuffer;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->renderedBuffer;
	this->_displayInfo.renderedWidth[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->renderedWidth;
	this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->renderedHeight;
	
	if (!this->_displayInfo.isCustomSizeRequested)
	{
		return;
	}
	
	// Iterate through VRAM banks A-D and determine if they will be used for this frame.
	for (size_t i = 0; i < 4; i++)
	{
		if (this->_engineMain->nativeLineCaptureCount[i] == GPU_VRAM_BLOCK_LINES)
		{
			continue;
		}
		
		switch (vramConfiguration.banks[i].purpose)
		{
			case VramConfiguration::ABG:
				this->_engineMain->UpdateVRAM3DUsageProperties_BGLayer(i);
				break;
				
			case VramConfiguration::BBG:
				this->_engineSub->UpdateVRAM3DUsageProperties_BGLayer(i);
				break;
				
			case VramConfiguration::AOBJ:
				this->_engineMain->UpdateVRAM3DUsageProperties_OBJLayer(i);
				break;
				
			case VramConfiguration::BOBJ:
				this->_engineSub->UpdateVRAM3DUsageProperties_OBJLayer(i);
				break;
				
			case VramConfiguration::LCDC:
				break;
				
			default:
			{
				this->_engineMain->nativeLineCaptureCount[i] = GPU_VRAM_BLOCK_LINES;
				for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
				{
					this->_engineMain->isLineCaptureNative[i][l] = true;
				}
				break;
			}
		}
	}
	
	this->_engineMain->isCustomRenderingNeeded	= (this->_engineMain->WillRender3DLayer() && !CurrentRenderer->IsFramebufferNativeSize()) ||
												  (this->_engineMain->vramBlockBGIndex != VRAM_NO_3D_USAGE) ||
												  (this->_engineMain->vramBlockOBJIndex != VRAM_NO_3D_USAGE);
	
	this->_engineSub->isCustomRenderingNeeded	= (this->_engineSub->vramBlockBGIndex != VRAM_NO_3D_USAGE) ||
												  (this->_engineSub->vramBlockOBJIndex != VRAM_NO_3D_USAGE);
}

const NDSDisplayInfo& GPUSubsystem::GetDisplayInfo()
{
	return this->_displayInfo;
}

void GPUSubsystem::SetDisplayDidCustomRender(NDSDisplayID displayID, bool theState)
{
	this->_displayInfo.didPerformCustomRender[displayID] = theState;
}

GPUEngineA* GPUSubsystem::GetEngineMain()
{
	return this->_engineMain;
}

GPUEngineB* GPUSubsystem::GetEngineSub()
{
	return this->_engineSub;
}

NDSDisplay* GPUSubsystem::GetDisplayMain()
{
	return this->_displayMain;
}

NDSDisplay* GPUSubsystem::GetDisplayTouch()
{
	return this->_displayTouch;
}

size_t GPUSubsystem::GetCustomFramebufferWidth() const
{
	return this->_displayInfo.customWidth;
}

size_t GPUSubsystem::GetCustomFramebufferHeight() const
{
	return this->_displayInfo.customHeight;
}

void GPUSubsystem::SetCustomFramebufferSize(size_t w, size_t h, void *clientNativeBuffer, void *clientCustomBuffer)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return;
	}
	
	CurrentRenderer->RenderFinish();
	
	const float customWidthScale = (float)w / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const float customHeightScale = (float)h / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	const float newGpuLargestDstLineCount = (size_t)ceilf(customHeightScale);
	
	u16 *oldGpuDstToSrcIndexPtr = _gpuDstToSrcIndex;
	u8 *oldGpuDstToSrcSSSE3_u8 = _gpuDstToSrcSSSE3_u8;
	u8 *oldGpuDstToSrcSSSE3_u16 = _gpuDstToSrcSSSE3_u16;
	
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
		_gpuCaptureLineCount[srcY] = lineCount;
		_gpuCaptureLineIndex[srcY] = currentLineCount;
		currentLineCount += lineCount;
	}
	
	u16 *newGpuDstToSrcIndex = (u16 *)malloc_alignedCacheLine(w * h * sizeof(u16));
	u16 *newGpuDstToSrcPtr = newGpuDstToSrcIndex;
	for (size_t y = 0, dstIdx = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
	{
		if (_gpuDstLineCount[y] < 1)
		{
			continue;
		}
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				newGpuDstToSrcIndex[dstIdx++] = (y * GPU_FRAMEBUFFER_NATIVE_WIDTH) + x;
			}
		}
		
		for (size_t l = 1; l < _gpuDstLineCount[y]; l++)
		{
			memcpy(newGpuDstToSrcPtr + (w * l), newGpuDstToSrcPtr, w * sizeof(u16));
		}
		
		newGpuDstToSrcPtr += (w * _gpuDstLineCount[y]);
		dstIdx += (w * (_gpuDstLineCount[y] - 1));
	}
	
	u8 *newGpuDstToSrcSSSE3_u8 = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u16 = (u8 *)malloc_alignedCacheLine(w * 2 * sizeof(u8));
	
	for (size_t i = 0; i < w; i++)
	{
		const u8 value_u8 = newGpuDstToSrcIndex[i] & 0x0007;
		const u8 value_u16 = (value_u8 << 1);
		
		newGpuDstToSrcSSSE3_u8[i] = value_u8;
		newGpuDstToSrcSSSE3_u16[(i << 1)] = value_u16;
		newGpuDstToSrcSSSE3_u16[(i << 1) + 1] = value_u16 + 1;
	}
	
	_gpuLargestDstLineCount = newGpuLargestDstLineCount;
	_gpuVRAMBlockOffset = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
	_gpuDstToSrcIndex = newGpuDstToSrcIndex;
	_gpuDstToSrcSSSE3_u8 = newGpuDstToSrcSSSE3_u8;
	_gpuDstToSrcSSSE3_u16 = newGpuDstToSrcSSSE3_u16;
	
	this->_displayInfo.isCustomSizeRequested = ( (w != GPU_FRAMEBUFFER_NATIVE_WIDTH) || (h != GPU_FRAMEBUFFER_NATIVE_HEIGHT) );
	this->_displayInfo.customWidth = w;
	this->_displayInfo.customHeight = h;
	
	if (!this->_displayInfo.isCustomSizeRequested)
	{
		this->_engineMain->nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
		this->_engineMain->nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
		this->_engineMain->nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
		this->_engineMain->nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
		
		for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
		{
			this->_engineMain->isLineCaptureNative[0][l] = true;
			this->_engineMain->isLineCaptureNative[1][l] = true;
			this->_engineMain->isLineCaptureNative[2][l] = true;
			this->_engineMain->isLineCaptureNative[3][l] = true;
		}
	}
	
	if (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main])
	{
		this->_displayInfo.renderedWidth[NDSDisplayID_Main] = this->_displayInfo.customWidth;
		this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_displayInfo.customHeight;
	}
	else
	{
		this->_displayInfo.renderedWidth[NDSDisplayID_Main] = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_displayInfo.renderedHeight[NDSDisplayID_Main] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	if (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch])
	{
		this->_displayInfo.renderedWidth[NDSDisplayID_Touch] = this->_displayInfo.customWidth;
		this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_displayInfo.customHeight;
	}
	else
	{
		this->_displayInfo.renderedWidth[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, w, h, clientNativeBuffer, clientCustomBuffer);
	
	free_aligned(oldGpuDstToSrcIndexPtr);
	free_aligned(oldGpuDstToSrcSSSE3_u8);
	free_aligned(oldGpuDstToSrcSSSE3_u16);
}

void GPUSubsystem::SetCustomFramebufferSize(size_t w, size_t h)
{
	this->SetCustomFramebufferSize(w, h, NULL, NULL);
}

void GPUSubsystem::SetColorFormat(const NDSColorFormat outputFormat, void *clientNativeBuffer, void *clientCustomBuffer)
{
	// TBD: Multiple color formats aren't supported in the renderer yet. Force the color format to NDSColorFormat_BGR555_Rev until then.
	//this->_displayInfo.colorFormat = outputFormat;
	this->_displayInfo.colorFormat = NDSColorFormat_BGR555_Rev;
	
	this->_displayInfo.pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(u32);
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, this->_displayInfo.customWidth, this->_displayInfo.customHeight, clientNativeBuffer, clientCustomBuffer);
}

void GPUSubsystem::SetColorFormat(const NDSColorFormat outputFormat)
{
	this->SetColorFormat(outputFormat, NULL, NULL);
}

void GPUSubsystem::_AllocateFramebuffers(NDSColorFormat outputFormat, size_t w, size_t h, void *clientNativeBuffer, void *clientCustomBuffer)
{
	void *oldCustomFramebuffer = this->_customFramebuffer;
	void *oldCustomVRAM = this->_customVRAM;
	
	const size_t pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(FragmentColor);
	const size_t newCustomVRAMBlockSize = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
	const size_t newCustomVRAMBlankSize = _gpuLargestDstLineCount * GPU_VRAM_BLANK_REGION_LINES * w;
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * pixelBytes;
	
	u16 *newCustomVRAM = (u16 *)malloc_alignedCacheLine(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * pixelBytes);
	memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * pixelBytes);
	
	if (clientNativeBuffer != NULL)
	{
		if (this->_displayInfo.masterNativeBuffer != clientNativeBuffer)
		{
			memcpy(clientNativeBuffer, this->_displayInfo.masterNativeBuffer, nativeFramebufferSize);
		}
		
		this->_displayInfo.masterNativeBuffer = clientNativeBuffer;
	}
	else
	{
		if (this->_displayInfo.masterNativeBuffer != this->_nativeFramebuffer)
		{
			memcpy(this->_nativeFramebuffer, this->_displayInfo.masterNativeBuffer, nativeFramebufferSize);
		}
		
		this->_displayInfo.masterNativeBuffer = this->_nativeFramebuffer;
	}
	
	if (clientCustomBuffer != NULL)
	{
		memset_u16(clientCustomBuffer, 0x8000, w * h * 2);
		this->_customFramebuffer = NULL;
		this->_displayInfo.masterCustomBuffer = clientCustomBuffer;
	}
	else
	{
		u16 *newCustomFramebuffer = (u16 *)malloc_alignedCacheLine(w * h * 2 * pixelBytes);
		memset_u16(newCustomFramebuffer, 0x8000, w * h * 2);
		this->_customFramebuffer = newCustomFramebuffer;
		this->_displayInfo.masterCustomBuffer = newCustomFramebuffer;
	}
	
	this->_customVRAM = newCustomVRAM;
	this->_customVRAMBlank = newCustomVRAM + (newCustomVRAMBlockSize * 4);
	
	this->_engineMain->SetCustomFramebufferSize(w, h);
	this->_engineSub->SetCustomFramebufferSize(w, h);
	BaseRenderer->SetFramebufferSize(w, h); // Since BaseRenderer is persistent, we need to update this manually.
	
	if (CurrentRenderer != BaseRenderer)
	{
		CurrentRenderer->SetFramebufferSize(w, h);
	}
	
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main]  = (this->_displayMain->GetEngine()->GetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.masterNativeBuffer : (u8 *)this->_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch] = (this->_displayTouch->GetEngine()->GetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.masterNativeBuffer : (u8 *)this->_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
	this->_displayInfo.customBuffer[NDSDisplayID_Main]  = (this->_displayMain->GetEngine()->GetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.masterCustomBuffer : (u8 *)this->_displayInfo.masterCustomBuffer + (w * h * this->_displayInfo.pixelBytes);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch] = (this->_displayTouch->GetEngine()->GetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.masterCustomBuffer : (u8 *)this->_displayInfo.masterCustomBuffer + (w * h * this->_displayInfo.pixelBytes);
	
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main])  ? this->_displayInfo.customBuffer[NDSDisplayID_Main]  : this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch]) ? this->_displayInfo.customBuffer[NDSDisplayID_Touch] : this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	
	free_aligned(oldCustomFramebuffer);
	free_aligned(oldCustomVRAM);
}

u16* GPUSubsystem::GetCustomVRAMBuffer()
{
	return this->_customVRAM;
}

u16* GPUSubsystem::GetCustomVRAMBlankBuffer()
{
	return this->_customVRAMBlank;
}

u16* GPUSubsystem::GetCustomVRAMAddressUsingMappedAddress(const u32 mappedAddr)
{
	const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(mappedAddr) - MMU.ARM9_LCD) / sizeof(u16);
	if (vramPixel >= (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
	{
		return this->_customVRAMBlank;
	}
	
	const size_t blockID = vramPixel / (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES);
	const size_t blockPixel = vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES);
	const size_t blockLine = blockPixel / GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const size_t linePixel = blockPixel % GPU_FRAMEBUFFER_NATIVE_WIDTH;
	
	return (this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (_gpuCaptureLineIndex[blockLine] * this->_displayInfo.customWidth) + _gpuDstPitchIndex[linePixel]);
}

bool GPUSubsystem::GetWillAutoResolveToCustomBuffer() const
{
	return this->_willAutoResolveToCustomBuffer;
}

void GPUSubsystem::SetWillAutoResolveToCustomBuffer(const bool willAutoResolve)
{
	this->_willAutoResolveToCustomBuffer = willAutoResolve;
}

void GPUSubsystem::RenderLine(const u16 l, bool isFrameSkipRequested)
{
	const bool isFramebufferRenderNeeded[2]	= { CommonSettings.showGpu.main && !(this->_engineMain->GetIsMasterBrightFullIntensity() && (this->_engineMain->GetIORegisterMap().DISPCAPCNT.CaptureEnable == 0)),
											    CommonSettings.showGpu.sub && !this->_engineSub->GetIsMasterBrightFullIntensity() };
	
	if (l == 0)
	{
		this->_event->DidFrameBegin(isFrameSkipRequested);
		
		// Clear displays to black if they are turned off by the user.
		if (!isFrameSkipRequested)
		{
			if (CurrentRenderer->GetRenderNeedsFinish())
			{
				CurrentRenderer->SetFramebufferFlushStates(this->_engineMain->WillRender3DLayer(), this->_engineMain->WillCapture3DLayerDirect());
				CurrentRenderer->RenderFinish();
				CurrentRenderer->SetFramebufferFlushStates(true, true);
				CurrentRenderer->SetRenderNeedsFinish(false);
				this->_event->DidRender3DEnd();
			}
			
			this->UpdateRenderProperties();
			
			if (!CommonSettings.showGpu.main)
			{
				memset(this->_engineMain->renderedBuffer, 0, this->_engineMain->renderedWidth * this->_engineMain->renderedHeight * sizeof(u16));
			}
			else if (this->_engineMain->GetIsMasterBrightFullIntensity() && (this->_engineMain->GetIORegisterMap().DISPCAPCNT.CaptureEnable == 0))
			{
				this->_engineMain->ApplyMasterBrightness<true>();
			}
			
			if (!CommonSettings.showGpu.sub)
			{
				memset(this->_engineSub->renderedBuffer, 0, this->_engineSub->renderedWidth * this->_engineSub->renderedHeight * sizeof(u16));
			}
			else if (this->_engineSub->GetIsMasterBrightFullIntensity())
			{
				this->_engineSub->ApplyMasterBrightness<true>();
			}
		}
	}
	
	if (isFramebufferRenderNeeded[GPUEngineID_Main] && !isFrameSkipRequested)
	{
		this->_engineMain->RenderLine(l);
	}
	else
	{
		this->_engineMain->UpdatePropertiesWithoutRender(l);
	}
	
	if (isFramebufferRenderNeeded[GPUEngineID_Sub] && !isFrameSkipRequested)
	{
		this->_engineSub->RenderLine(l);
	}
	else
	{
		this->_engineSub->UpdatePropertiesWithoutRender(l);
	}
	
	if (l == 191)
	{
		if (!isFrameSkipRequested)
		{
			if (this->_displayInfo.isCustomSizeRequested)
			{
				this->_engineMain->ResolveCustomRendering();
				this->_engineSub->ResolveCustomRendering();
			}
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = (this->_displayMain->GetEngine()->nativeLineOutputCount < GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_displayMain->GetEngine()->renderedBuffer;
			this->_displayInfo.renderedWidth[NDSDisplayID_Main] = this->_displayMain->GetEngine()->renderedWidth;
			this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_displayMain->GetEngine()->renderedHeight;
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = (this->_displayTouch->GetEngine()->nativeLineOutputCount < GPU_FRAMEBUFFER_NATIVE_HEIGHT);
			this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->renderedBuffer;
			this->_displayInfo.renderedWidth[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->renderedWidth;
			this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_displayTouch->GetEngine()->renderedHeight;
			
			if (isFramebufferRenderNeeded[GPUEngineID_Main])
			{
				this->_engineMain->ApplyMasterBrightness<false>();
			}
			
			if (isFramebufferRenderNeeded[GPUEngineID_Sub])
			{
				this->_engineSub->ApplyMasterBrightness<false>();
			}
			
			if (this->_willAutoResolveToCustomBuffer)
			{
				this->_engineMain->ResolveToCustomFramebuffer();
				this->_engineSub->ResolveToCustomFramebuffer();
			}
		}
		
		this->_engineMain->FramebufferPostprocess();
		this->_engineSub->FramebufferPostprocess();
		
		gfx3d._videoFrameCount++;
		if (gfx3d._videoFrameCount == 60)
		{
			Render3DFramesPerSecond = gfx3d.render3DFrameCount;
			gfx3d.render3DFrameCount = 0;
			gfx3d._videoFrameCount = 0;
		}
		
		this->_event->DidFrameEnd(isFrameSkipRequested);
	}
}

void GPUSubsystem::ClearWithColor(const u16 colorBGRA5551)
{
	u16 color16 = colorBGRA5551;
	FragmentColor color32;
	
	switch (this->_displayInfo.colorFormat)
	{
		case NDSColorFormat_BGR555_Rev:
			color16 = colorBGRA5551 | 0x8000;
			break;
			
		case NDSColorFormat_BGR666_Rev:
			color32.r = material_5bit_to_6bit[(colorBGRA5551 & 0x001F)];
			color32.g = material_5bit_to_6bit[(colorBGRA5551 & 0x03E0) >> 5];
			color32.b = material_5bit_to_6bit[(colorBGRA5551 & 0x7C00) >> 10];
			color32.a = 0xFF;
			break;
			
		case NDSColorFormat_BGR888_Rev:
			color32.r = material_5bit_to_8bit[(colorBGRA5551 & 0x001F)];
			color32.g = material_5bit_to_8bit[(colorBGRA5551 & 0x03E0) >> 5];
			color32.b = material_5bit_to_8bit[(colorBGRA5551 & 0x7C00) >> 10];
			color32.a = 0xFF;
			break;
			
		default:
			break;
	}
	
	switch (this->_displayInfo.pixelBytes)
	{
		case 2:
			memset_u16(this->_displayInfo.masterNativeBuffer, color16, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
			memset_u16(this->_displayInfo.masterCustomBuffer, color16, this->_displayInfo.customWidth * this->_displayInfo.customHeight * 2);
			break;
			
		case 4:
			memset_u32(this->_displayInfo.masterNativeBuffer, color32.color, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2);
			memset_u32(this->_displayInfo.masterCustomBuffer, color32.color, this->_displayInfo.customWidth * this->_displayInfo.customHeight * 2);
			break;
			
		default:
			break;
	}
}

NDSDisplay::NDSDisplay()
{
	_ID = NDSDisplayID_Main;
	_gpu = NULL;
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID)
{
	_ID = displayID;
	_gpu = NULL;
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID, GPUEngineBase *theEngine)
{
	_ID = displayID;
	_gpu = theEngine;
}

GPUEngineBase* NDSDisplay::GetEngine()
{
	return this->_gpu;
}

void NDSDisplay::SetEngine(GPUEngineBase *theEngine)
{
	this->_gpu = theEngine;
}

GPUEngineID NDSDisplay::GetEngineID()
{
	return this->_gpu->GetEngineID();
}

void NDSDisplay::SetEngineByID(const GPUEngineID theID)
{
	this->_gpu = (theID == GPUEngineID_Main) ? (GPUEngineBase *)GPU->GetEngineMain() : (GPUEngineBase *)GPU->GetEngineSub();
	this->_gpu->SetDisplayByID(this->_ID);
}

template void GPUEngineBase::ParseReg_DISPCNT<GPUEngineID_Main>();
template void GPUEngineBase::ParseReg_DISPCNT<GPUEngineID_Sub>();

template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Main, GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Main, GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Main, GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Main, GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Sub, GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Sub, GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Sub, GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnCNT<GPUEngineID_Sub, GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_WINnH<0>();
template void GPUEngineBase::ParseReg_WINnH<1>();

template void GPUEngineBase::ParseReg_BGnX<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnY<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnX<GPULayerID_BG3>();
template void GPUEngineBase::ParseReg_BGnY<GPULayerID_BG3>();

template void GPUEngineBase::RenderLayerBG<GPULayerID_BG0>(u16 *dstColorBuffer);
template void GPUEngineBase::RenderLayerBG<GPULayerID_BG1>(u16 *dstColorBuffer);
template void GPUEngineBase::RenderLayerBG<GPULayerID_BG2>(u16 *dstColorBuffer);
template void GPUEngineBase::RenderLayerBG<GPULayerID_BG3>(u16 *dstColorBuffer);
