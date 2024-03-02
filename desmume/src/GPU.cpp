/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 Theo Berkau
	Copyright (C) 2007 shash
	Copyright (C) 2008-2023 DeSmuME team

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
#include "NDSSystem.h"
#include "matrix.h"
#include "emufile.h"
#include "utils/task.h"


#ifdef FASTBUILD
	#undef FORCEINLINE
	#define FORCEINLINE
	//compilation speed hack (cuts time exactly in half by cutting out permutations)
	#define DISABLE_MOSAIC
	#define DISABLE_COMPOSITOR_FAST_PATHS
#endif

#include "GPU_Operations.cpp"

#if defined(ENABLE_AVX2)
	#define USEVECTORSIZE_256
	#define VECTORSIZE 32
#elif defined(ENABLE_SSE2)
	#define USEVECTORSIZE_128
	#define VECTORSIZE 16
#endif

#if defined(USEVECTORSIZE_512) || defined(USEVECTORSIZE_256) || defined(USEVECTORSIZE_128)
	#define VECELEMENTCOUNT8  (VECTORSIZE/sizeof(s8))
	#define VECELEMENTCOUNT16 (VECTORSIZE/sizeof(s16))
	#define VECELEMENTCOUNT32 (VECTORSIZE/sizeof(s32))
	// Comment out USEMANUALVECTORIZATION to disable the hand-coded vectorized code.
	#define USEMANUALVECTORIZATION
#endif

//instantiate static instance
GPUEngineBase::MosaicLookup GPUEngineBase::_mosaicLookup;

GPUSubsystem *GPU = NULL;

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

void gpu_savestate(EMUFILE &os)
{
	GPU->SaveState(os);
}

bool gpu_loadstate(EMUFILE &is, int size)
{
	return GPU->LoadState(is, size);
}

/*****************************************************************************/
//			INITIALIZATION
/*****************************************************************************/
GPUEngineBase::GPUEngineBase()
{
	_IORegisterMap = NULL;
	_paletteOBJ = NULL;
	_targetDisplay = NULL;
	
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
	
	_internalRenderLineTargetCustom = NULL;
	_renderLineLayerIDCustom = NULL;
	_deferredIndexCustom = NULL;
	_deferredColorCustom = NULL;
	
	_enableEngine = true;
	_enableBGLayer[GPULayerID_BG0] = true;
	_enableBGLayer[GPULayerID_BG1] = true;
	_enableBGLayer[GPULayerID_BG2] = true;
	_enableBGLayer[GPULayerID_BG3] = true;
	_enableBGLayer[GPULayerID_OBJ] = true;
	
	_needExpandSprColorCustom = false;
	_sprColorCustom = NULL;
	_sprAlphaCustom = NULL;
	_sprTypeCustom = NULL;
	
	if (CommonSettings.num_cores > 1)
	{
		_asyncClearTask = new Task;
		_asyncClearTask->start(false, 0, "async clear");
	}
	else
	{
		_asyncClearTask = NULL;
	}
	
	_asyncClearTransitionedLineFromBackdropCount = 0;
	_asyncClearLineCustom = 0;
	_asyncClearInterrupt = 0;
	_asyncClearBackdropColor16 = 0;
	_asyncClearBackdropColor32.value = 0;
	_asyncClearIsRunning = false;
	_asyncClearUseInternalCustomBuffer = false;
	
	_didPassWindowTestCustomMasterPtr = NULL;
	_didPassWindowTestCustom[GPULayerID_BG0] = NULL;
	_didPassWindowTestCustom[GPULayerID_BG1] = NULL;
	_didPassWindowTestCustom[GPULayerID_BG2] = NULL;
	_didPassWindowTestCustom[GPULayerID_BG3] = NULL;
	_didPassWindowTestCustom[GPULayerID_OBJ] = NULL;
	
	_enableColorEffectCustomMasterPtr = NULL;
	_enableColorEffectCustom[GPULayerID_BG0] = NULL;
	_enableColorEffectCustom[GPULayerID_BG1] = NULL;
	_enableColorEffectCustom[GPULayerID_BG2] = NULL;
	_enableColorEffectCustom[GPULayerID_BG3] = NULL;
	_enableColorEffectCustom[GPULayerID_OBJ] = NULL;
}

GPUEngineBase::~GPUEngineBase()
{
	if (this->_asyncClearTask != NULL)
	{
		this->RenderLineClearAsyncFinish();
		delete this->_asyncClearTask;
		this->_asyncClearTask = NULL;
	}
	
	free_aligned(this->_internalRenderLineTargetCustom);
	this->_internalRenderLineTargetCustom = NULL;
	free_aligned(this->_renderLineLayerIDCustom);
	this->_renderLineLayerIDCustom = NULL;
	free_aligned(this->_deferredIndexCustom);
	this->_deferredIndexCustom = NULL;
	free_aligned(this->_deferredColorCustom);
	this->_deferredColorCustom = NULL;
	
	free_aligned(this->_sprColorCustom);
	this->_sprColorCustom = NULL;
	free_aligned(this->_sprAlphaCustom);
	this->_sprAlphaCustom = NULL;
	free_aligned(this->_sprTypeCustom);
	this->_sprTypeCustom = NULL;
	
	free_aligned(this->_didPassWindowTestCustomMasterPtr);
	this->_didPassWindowTestCustomMasterPtr = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG0] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG1] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG2] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_BG3] = NULL;
	this->_didPassWindowTestCustom[GPULayerID_OBJ] = NULL;
	
	this->_enableColorEffectCustomMasterPtr = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG0] = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG1] = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG2] = NULL;
	this->_enableColorEffectCustom[GPULayerID_BG3] = NULL;
	this->_enableColorEffectCustom[GPULayerID_OBJ] = NULL;
}

void GPUEngineBase::Reset()
{
	this->SetupBuffers();
	
	memset(this->_sprColor, 0, sizeof(this->_sprColor));
	memset(this->_sprNum, 0, sizeof(this->_sprNum));
	
	memset(this->_didPassWindowTestNative, 0xFF, 5 * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	memset(this->_enableColorEffectNative, 0xFF, 5 * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	memset(this->_didPassWindowTestCustomMasterPtr, 0xFF, 10 * this->_targetDisplay->GetWidth() * sizeof(u8));
	
	memset(this->_h_win[0], 0, sizeof(this->_h_win[0]));
	memset(this->_h_win[1], 0, sizeof(this->_h_win[1]));
	memset(&this->_mosaicColors, 0, sizeof(MosaicColor));
	memset(this->_itemsForPriority, 0, sizeof(this->_itemsForPriority));
	
	memset(this->_internalRenderLineTargetNative, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	
	if (this->_internalRenderLineTargetCustom != NULL)
	{
		memset(this->_internalRenderLineTargetCustom, 0, this->_targetDisplay->GetWidth() * this->_targetDisplay->GetHeight() * this->_targetDisplay->GetPixelBytes());
	}
	
	this->_needExpandSprColorCustom = false;
	
	this->_isBGLayerShown[GPULayerID_BG0] = false;
	this->_isBGLayerShown[GPULayerID_BG1] = false;
	this->_isBGLayerShown[GPULayerID_BG2] = false;
	this->_isBGLayerShown[GPULayerID_BG3] = false;
	this->_isBGLayerShown[GPULayerID_OBJ] = false;
	this->_isAnyBGLayerShown = false;
	
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
	
	this->_prevWINState.value = 0xFFFF;
	this->_prevWINCoord.value = 0xFFFFFFFFFFFFFFFFULL;
	this->_prevWINCtrl.value = 0xFFFFFFFF;
	
	this->_vramBlockOBJAddress = 0;
	
	this->savedBG2X.value = 0;
	this->savedBG2Y.value = 0;
	this->savedBG3X.value = 0;
	this->savedBG3Y.value = 0;
	
	this->_asyncClearTransitionedLineFromBackdropCount = 0;
	this->_asyncClearLineCustom = 0;
	this->_asyncClearInterrupt = 0;
	this->_asyncClearIsRunning = false;
	this->_asyncClearUseInternalCustomBuffer = false;
	
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->_isLineRenderNative[l] = true;
	}
	
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.displayOutputMode = GPUDisplayMode_Off;
	renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	renderState.selectedLayerID = GPULayerID_BG0;
	renderState.selectedBGLayer = &this->_BGLayer[GPULayerID_BG0];
	renderState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	renderState.workingBackdropColor16 = renderState.backdropColor16;
	renderState.workingBackdropColor32.value = LOCAL_TO_LE_32( (this->_targetDisplay->GetColorFormat() == NDSColorFormat_BGR666_Rev) ? COLOR555TO666(LOCAL_TO_LE_16(renderState.workingBackdropColor16)) : COLOR555TO888(LOCAL_TO_LE_16(renderState.workingBackdropColor16)) );
	renderState.colorEffect = (ColorEffect)this->_IORegisterMap->BLDCNT.ColorEffect;
	renderState.blendEVA = 0;
	renderState.blendEVB = 0;
	renderState.blendEVY = 0;
	renderState.masterBrightnessMode = GPUMasterBrightMode_Disable;
	renderState.masterBrightnessIntensity = 0;
	renderState.masterBrightnessIsMaxOrMin = true;
	renderState.blendTable555 = (TBlendTable *)&PixelOperation::BlendTable555[renderState.blendEVA][renderState.blendEVB][0][0];
	renderState.brightnessUpTable555 = &PixelOperation::BrightnessUpTable555[renderState.blendEVY][0];
	renderState.brightnessUpTable666 = &PixelOperation::BrightnessUpTable666[renderState.blendEVY][0];
	renderState.brightnessUpTable888 = &PixelOperation::BrightnessUpTable888[renderState.blendEVY][0];
	renderState.brightnessDownTable555 = &PixelOperation::BrightnessDownTable555[renderState.blendEVY][0];
	renderState.brightnessDownTable666 = &PixelOperation::BrightnessDownTable666[renderState.blendEVY][0];
	renderState.brightnessDownTable888 = &PixelOperation::BrightnessDownTable888[renderState.blendEVY][0];
	
	memset(&renderState.WIN0_enable[0], 0, sizeof(renderState.WIN0_enable[0]) * 6);
	memset(&renderState.WIN1_enable[0], 0, sizeof(renderState.WIN1_enable[0]) * 6);
	memset(&renderState.WINOUT_enable[0], 0, sizeof(renderState.WINOUT_enable[0]) * 6);
	memset(&renderState.WINOBJ_enable[0], 0, sizeof(renderState.WINOBJ_enable[0]) * 6);
	memset(&renderState.dstBlendEnableVecLookup, 0, sizeof(renderState.dstBlendEnableVecLookup));
	
	renderState.windowState.value = 0;
	renderState.isAnyWindowEnabled = false;
	
	memset(&renderState.srcEffectEnable[0], 0, sizeof(renderState.srcEffectEnable[0]) * 6);
	memset(&renderState.dstBlendEnable[0], 0, sizeof(renderState.dstBlendEnable[0]) * 6);
	renderState.dstAnyBlendEnable = false;
	
	renderState.mosaicWidthBG = &this->_mosaicLookup.table[0];
	renderState.mosaicHeightBG = &this->_mosaicLookup.table[0];
	renderState.mosaicWidthOBJ = &this->_mosaicLookup.table[0];
	renderState.mosaicHeightOBJ = &this->_mosaicLookup.table[0];
	renderState.isBGMosaicSet = false;
	renderState.isOBJMosaicSet = false;
	
	renderState.spriteRenderMode = SpriteRenderMode_Sprite1D;
	renderState.spriteBoundary = 0;
	renderState.spriteBMPBoundary = 0;
	
	for (size_t line = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		this->_currentCompositorInfo[line].renderState = renderState;
	}
}

void GPUEngineBase::_ResortBGLayers()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	int i, prio;
	itemsForPriority_t *item;
	
	// we don't need to check for windows here...
	// if we tick boxes, invisible layers become invisible & vice versa
#define OP ^ !
	// if we untick boxes, layers become invisible
	//#define OP &&
	this->_isBGLayerShown[GPULayerID_BG0] = this->_enableBGLayer[GPULayerID_BG0] OP(this->_BGLayer[GPULayerID_BG0].isVisible);
	this->_isBGLayerShown[GPULayerID_BG1] = this->_enableBGLayer[GPULayerID_BG1] OP(this->_BGLayer[GPULayerID_BG1].isVisible);
	this->_isBGLayerShown[GPULayerID_BG2] = this->_enableBGLayer[GPULayerID_BG2] OP(this->_BGLayer[GPULayerID_BG2].isVisible);
	this->_isBGLayerShown[GPULayerID_BG3] = this->_enableBGLayer[GPULayerID_BG3] OP(this->_BGLayer[GPULayerID_BG3].isVisible);
	this->_isBGLayerShown[GPULayerID_OBJ] = this->_enableBGLayer[GPULayerID_OBJ] OP(DISPCNT.OBJ_Enable);
	
	this->_isAnyBGLayerShown = this->_isBGLayerShown[GPULayerID_BG0] || this->_isBGLayerShown[GPULayerID_BG1] || this->_isBGLayerShown[GPULayerID_BG2] || this->_isBGLayerShown[GPULayerID_BG3];
	
	renderState.windowState.BG0_Shown = (this->_isBGLayerShown[GPULayerID_BG0]) ? 1 : 0;
	renderState.windowState.BG1_Shown = (this->_isBGLayerShown[GPULayerID_BG1]) ? 1 : 0;
	renderState.windowState.BG2_Shown = (this->_isBGLayerShown[GPULayerID_BG2]) ? 1 : 0;
	renderState.windowState.BG3_Shown = (this->_isBGLayerShown[GPULayerID_BG3]) ? 1 : 0;
	renderState.windowState.OBJ_Shown = (this->_isBGLayerShown[GPULayerID_OBJ]) ? 1 : 0;
	
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
		if (!this->_isBGLayerShown[i]) continue;
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

void GPUEngineBase::ParseReg_MASTER_BRIGHT()
{
	const IOREG_MASTER_BRIGHT &MASTER_BRIGHT = this->_IORegisterMap->MASTER_BRIGHT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.masterBrightnessIntensity = (MASTER_BRIGHT.Intensity >= 16) ? 16 : MASTER_BRIGHT.Intensity;
	renderState.masterBrightnessMode = (GPUMasterBrightMode)MASTER_BRIGHT.Mode;
	renderState.masterBrightnessIsMaxOrMin = ( (MASTER_BRIGHT.Intensity >= 16) || (MASTER_BRIGHT.Intensity == 0) );
}

//Sets up LCD control variables for Display Engines A and B for quick reading
void GPUEngineBase::ParseReg_DISPCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.displayOutputMode = (this->_engineID == GPUEngineID_Main) ? (GPUDisplayMode)DISPCNT.DisplayMode : (GPUDisplayMode)(DISPCNT.DisplayMode & 0x01);
	
	renderState.windowState.WIN0_ENABLED = DISPCNT.Win0_Enable;
	renderState.windowState.WIN1_ENABLED = DISPCNT.Win1_Enable;
	renderState.windowState.WINOBJ_ENABLED = DISPCNT.WinOBJ_Enable;
	renderState.isAnyWindowEnabled = ( (DISPCNT.Win0_Enable != 0) || (DISPCNT.Win1_Enable != 0) || (DISPCNT.WinOBJ_Enable != 0) );
	
	if (DISPCNT.OBJ_Tile_mapping)
	{
		//1-d sprite mapping boundaries:
		//32k, 64k, 128k, 256k
		renderState.spriteBoundary = 5 + DISPCNT.OBJ_Tile_1D_Bound;
		
		//do not be deceived: even though a sprBoundary==8 (256KB region) is impossible to fully address
		//in GPU_SUB, it is still fully legal to address it with that granularity.
		//so don't do this: //if((gpu->core == GPU_SUB) && (cnt->OBJ_Tile_1D_Bound == 3)) gpu->sprBoundary = 7;

		renderState.spriteRenderMode = SpriteRenderMode_Sprite1D;
	}
	else
	{
		//2d sprite mapping
		//boundary : 32k
		renderState.spriteBoundary = 5;
		renderState.spriteRenderMode = SpriteRenderMode_Sprite2D;
	}
     
	if (DISPCNT.OBJ_BMP_1D_Bound && (this->_engineID == GPUEngineID_Main))
		renderState.spriteBMPBoundary = 8;
	else
		renderState.spriteBMPBoundary = 7;
	
	this->ParseReg_BGnCNT(GPULayerID_BG3);
	this->ParseReg_BGnCNT(GPULayerID_BG2);
	this->ParseReg_BGnCNT(GPULayerID_BG1);
	this->ParseReg_BGnCNT(GPULayerID_BG0);
}

void GPUEngineBase::ParseReg_BGnCNT(const GPULayerID layerID)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_BGnCNT &BGnCNT = this->_IORegisterMap->BGnCNT[layerID];
	this->_BGLayer[layerID].BGnCNT = BGnCNT;
	
	switch (layerID)
	{
		case GPULayerID_BG0: this->_BGLayer[layerID].isVisible = (DISPCNT.BG0_Enable != 0); break;
		case GPULayerID_BG1: this->_BGLayer[layerID].isVisible = (DISPCNT.BG1_Enable != 0); break;
		case GPULayerID_BG2: this->_BGLayer[layerID].isVisible = (DISPCNT.BG2_Enable != 0); break;
		case GPULayerID_BG3: this->_BGLayer[layerID].isVisible = (DISPCNT.BG3_Enable != 0); break;
			
		default:
			break;
	}
	
	if (this->_engineID == GPUEngineID_Main)
	{
		this->_BGLayer[layerID].largeBMPAddress  = MMU_ABG;
		this->_BGLayer[layerID].BMPAddress       = MMU_ABG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_16KB);
		this->_BGLayer[layerID].tileMapAddress   = MMU_ABG + (DISPCNT.ScreenBase_Block * ADDRESS_STEP_64KB) + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_2KB);
		this->_BGLayer[layerID].tileEntryAddress = MMU_ABG + (DISPCNT.CharacBase_Block * ADDRESS_STEP_64KB) + (BGnCNT.CharacBase_Block * ADDRESS_STEP_16KB);
	}
	else
	{
		this->_BGLayer[layerID].largeBMPAddress  = MMU_BBG;
		this->_BGLayer[layerID].BMPAddress       = MMU_BBG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_16KB);
		this->_BGLayer[layerID].tileMapAddress   = MMU_BBG + (BGnCNT.ScreenBase_Block * ADDRESS_STEP_2KB);
		this->_BGLayer[layerID].tileEntryAddress = MMU_BBG + (BGnCNT.CharacBase_Block * ADDRESS_STEP_16KB);
	}
	
	//clarify affine ext modes
	BGType mode = GPUEngineBase::_mode2type[DISPCNT.BG_Mode][layerID];
	this->_BGLayer[layerID].baseType = mode;
	
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
	if (layerID == GPULayerID_BG0 || layerID == GPULayerID_BG1)
	{
		this->_BGLayer[layerID].extPaletteSlot = (BGnCNT.PaletteSet_Wrap * 2) + layerID;
	}
	else
	{
		this->_BGLayer[layerID].isDisplayWrapped = (BGnCNT.PaletteSet_Wrap != 0);
	}
	
	this->_BGLayer[layerID].type = mode;
	this->_BGLayer[layerID].size = GPUEngineBase::_BGLayerSizeLUT[mode][BGnCNT.ScreenSize];
	this->_BGLayer[layerID].isMosaic = (BGnCNT.Mosaic != 0);
	this->_BGLayer[layerID].priority = BGnCNT.Priority;
	this->_BGLayer[layerID].extPalette = (u16 **)&MMU.ExtPal[this->_engineID][this->_BGLayer[layerID].extPaletteSlot];
	
	this->_ResortBGLayers();
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnHOFS()
{
	const IOREG_BGnHOFS &BGnHOFS = this->_IORegisterMap->BGnOFS[LAYERID].BGnHOFS;
	this->_BGLayer[LAYERID].BGnHOFS = BGnHOFS;
	this->_BGLayer[LAYERID].xOffset = BGnHOFS.value & 0x01FF;
}

template <GPULayerID LAYERID>
void GPUEngineBase::ParseReg_BGnVOFS()
{
	const IOREG_BGnVOFS &BGnVOFS = this->_IORegisterMap->BGnOFS[LAYERID].BGnVOFS;
	this->_BGLayer[LAYERID].BGnVOFS = BGnVOFS;
	this->_BGLayer[LAYERID].yOffset = BGnVOFS.value & 0x01FF;
}

template <GPULayerID LAYERID>
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

template <GPULayerID LAYERID>
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

void* GPUEngine_RunClearAsynchronous(void *arg)
{
	GPUEngineBase *gpuEngine = (GPUEngineBase *)arg;
	gpuEngine->RenderLineClearAsync();
	
	return NULL;
}

void GPUEngineBase::RenderLineClearAsync()
{
	s32 asyncClearLineCustom = atomic_and_barrier32(&this->_asyncClearLineCustom, 0x000000FF);
	
	if (this->_targetDisplay->IsCustomSizeRequested())
	{
		// Note that the following variables are not explicitly synchronized, and therefore are expected
		// to remain constant while this thread is running:
		//    _asyncClearUseInternalCustomBuffer
		//    _asyncClearBackdropColor16
		//    _asyncClearBackdropColor32
		//
		// These variables are automatically handled when calling RenderLineClearAsyncStart(),
		// and so there should be no need to modify these variables directly.
		//
		// Also note that this clearing relies on the target engine/display association. If this association
		// needs to be changed, then asynchronous clearing needs to be stopped before the change occurs.
		u8 *targetBufferHead = (this->_asyncClearUseInternalCustomBuffer) ? (u8 *)this->_internalRenderLineTargetCustom : (u8 *)this->_targetDisplay->GetCustomBuffer();
		
		while (asyncClearLineCustom < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
		{
			const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[asyncClearLineCustom].line;
			
			switch (this->_targetDisplay->GetColorFormat())
			{
				case NDSColorFormat_BGR555_Rev:
					memset_u16(targetBufferHead + (lineInfo.blockOffsetCustom * sizeof(u16)), this->_asyncClearBackdropColor16, lineInfo.pixelCount);
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					memset_u32(targetBufferHead + (lineInfo.blockOffsetCustom * sizeof(Color4u8)), this->_asyncClearBackdropColor32.value, lineInfo.pixelCount);
					break;
			}
			
			asyncClearLineCustom++;
			atomic_inc_barrier32(&this->_asyncClearLineCustom);
			
			if ( atomic_test_and_clear_barrier32(&this->_asyncClearInterrupt, 0x07) )
			{
				return;
			}
		}
	}
	else
	{
		atomic_add_32(&this->_asyncClearLineCustom, GPU_FRAMEBUFFER_NATIVE_HEIGHT - asyncClearLineCustom);
		asyncClearLineCustom = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	atomic_test_and_clear_barrier32(&this->_asyncClearInterrupt, 0x07);
}

void GPUEngineBase::RenderLineClearAsyncStart(bool willClearInternalCustomBuffer,
											  size_t startLineIndex,
											  u16 clearColor16,
											  Color4u8 clearColor32)
{
	if (this->_asyncClearTask == NULL)
	{
		return;
	}
	
	this->RenderLineClearAsyncFinish();
	
	this->_asyncClearLineCustom = (s32)startLineIndex;
	this->_asyncClearBackdropColor16 = clearColor16;
	this->_asyncClearBackdropColor32 = clearColor32;
	this->_asyncClearUseInternalCustomBuffer = willClearInternalCustomBuffer;
	
	this->_asyncClearTask->execute(&GPUEngine_RunClearAsynchronous, this);
	this->_asyncClearIsRunning = true;
}

void GPUEngineBase::RenderLineClearAsyncFinish()
{
	if (!this->_asyncClearIsRunning)
	{
		return;
	}
	
	atomic_test_and_set_barrier32(&this->_asyncClearInterrupt, 0x07);
	
	this->_asyncClearTask->finish();
	this->_asyncClearIsRunning = false;
	this->_asyncClearInterrupt = 0;
}

void GPUEngineBase::RenderLineClearAsyncWaitForCustomLine(const size_t l)
{
	const s32 lineCompare = (s32)l;
	while (lineCompare >= atomic_and_barrier32(&this->_asyncClearLineCustom, 0x000000FF))
	{
		// Do nothing -- just spin.
	}
}

void GPUEngineBase::_RenderLine_Clear(GPUEngineCompositorInfo &compInfo)
{
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(compInfo.target.lineColor16, compInfo.renderState.workingBackdropColor16);
	
	// init pixels priorities
	assert(NB_PRIORITIES == 4);
	this->_itemsForPriority[0].nbPixelsX = 0;
	this->_itemsForPriority[1].nbPixelsX = 0;
	this->_itemsForPriority[2].nbPixelsX = 0;
	this->_itemsForPriority[3].nbPixelsX = 0;
}

void GPUEngineBase::SetupBuffers()
{
	memset(this->_renderLineLayerIDNative, GPULayerID_Backdrop, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	
	memset(this->_sprAlpha, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	memset(this->_sprType, OBJMode_Normal, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	memset(this->_sprPrio, 0x7F, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	memset(this->_sprWin, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	
	if (this->_targetDisplay->IsCustomSizeRequested())
	{
		if (this->_renderLineLayerIDCustom != NULL)
		{
			memset(this->_renderLineLayerIDCustom, GPULayerID_Backdrop, this->_targetDisplay->GetWidth() * (this->_targetDisplay->GetHeight() + (_gpuLargestDstLineCount * 4)) * sizeof(u8));
		}
	}
}

void GPUEngineBase::SetupRenderStates()
{
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
	{
		this->_isLineRenderNative[l] = true;
	}
}

void GPUEngineBase::DisplayDrawBuffersUpdate()
{
	if (this->_targetDisplay == NULL)
	{
		return;
	}
	
	// For thread-safety, this method must be called whenever the following occurs:
	// 1. The engine/display association is changed
	// 2. The target display's custom buffer is changed
	
	if ( this->_targetDisplay->DidPerformCustomRender() && !this->_asyncClearUseInternalCustomBuffer && (this->_targetDisplay->GetCustomBuffer() != NULL) )
	{
		// So apparently, it is possible for some games to change the engine/display association
		// mid-frame. For example, "The Legend of Zelda: Phantom Hourglass" will do exactly this
		// whenever the player moves the map to or from the touch screen.
		//
		// Therefore, whenever a game changes this association mid-frame, we need to force any
		// asynchronous clearing to finish so that we can return control of the custom draw buffer
		// to this thread.
		this->RenderLineClearAsyncFinish();
		this->_asyncClearTransitionedLineFromBackdropCount = 0;
	}
}

void GPUEngineBase::UpdateRenderStates(const size_t l)
{
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	GPUEngineRenderState &currRenderState = this->_currentRenderState;
	
	// Get the current backdrop color.
	currRenderState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	if (currRenderState.srcEffectEnable[GPULayerID_Backdrop])
	{
		if (currRenderState.colorEffect == ColorEffect_IncreaseBrightness)
		{
			currRenderState.workingBackdropColor16 = currRenderState.brightnessUpTable555[currRenderState.backdropColor16];
		}
		else if (currRenderState.colorEffect == ColorEffect_DecreaseBrightness)
		{
			currRenderState.workingBackdropColor16 = currRenderState.brightnessDownTable555[currRenderState.backdropColor16];
		}
		else
		{
			currRenderState.workingBackdropColor16 = currRenderState.backdropColor16;
		}
	}
	else
	{
		currRenderState.workingBackdropColor16 = currRenderState.backdropColor16;
	}
	currRenderState.workingBackdropColor32.value = LOCAL_TO_LE_32( (this->_targetDisplay->GetColorFormat() == NDSColorFormat_BGR666_Rev) ? COLOR555TO666(LOCAL_TO_LE_16(currRenderState.workingBackdropColor16)) : COLOR555TO888(LOCAL_TO_LE_16(currRenderState.workingBackdropColor16)) );
	
	// Save the current render states to this line's compositor info.
	compInfo.renderState = currRenderState;
	
	// Handle the asynchronous custom line clearing thread.
	if (compInfo.line.indexNative == 0)
	{
		// If, in the last frame, we transitioned all 192 lines directly from the backdrop layer,
		// then we'll assume that this current frame will also do the same. Therefore, we will
		// attempt to asynchronously clear all of the custom lines with the backdrop color as an
		// optimization.
		const bool wasPreviousHDFrameFullyTransitionedFromBackdrop = (this->_asyncClearTransitionedLineFromBackdropCount >= GPU_FRAMEBUFFER_NATIVE_HEIGHT);
		this->_asyncClearTransitionedLineFromBackdropCount = 0;
		
		if (this->_targetDisplay->IsCustomSizeRequested() && wasPreviousHDFrameFullyTransitionedFromBackdrop)
		{
			this->RenderLineClearAsyncStart((compInfo.renderState.displayOutputMode != GPUDisplayMode_Normal),
			                                compInfo.line.indexNative,
			                                compInfo.renderState.workingBackdropColor16,
			                                compInfo.renderState.workingBackdropColor32);
		}
	}
	else if (this->_asyncClearIsRunning)
	{
		// If the backdrop color or the display output mode changes mid-frame, then we need to cancel
		// the asynchronous clear and then clear this and any other remaining lines synchronously.
		const bool isUsingInternalCustomBuffer = (compInfo.renderState.displayOutputMode != GPUDisplayMode_Normal);
		
		if ( (compInfo.renderState.workingBackdropColor16 != this->_asyncClearBackdropColor16) ||
			 (isUsingInternalCustomBuffer != this->_asyncClearUseInternalCustomBuffer) )
		{
			this->RenderLineClearAsyncFinish();
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::RenderLine(const size_t l)
{
	// By default, do nothing.
	this->UpdatePropertiesWithoutRender(l);
}

void GPUEngineBase::UpdatePropertiesWithoutRender(const u16 l)
{
	// Update BG2/BG3 parameters for Affine and AffineExt modes
	if (  this->_isBGLayerShown[GPULayerID_BG2] &&
		((this->_BGLayer[GPULayerID_BG2].baseType == BGType_Affine) || (this->_BGLayer[GPULayerID_BG2].baseType == BGType_AffineExt)) )
	{
		IOREG_BG2Parameter &BG2Param = this->_IORegisterMap->BG2Param;
		
		BG2Param.BG2X.value += BG2Param.BG2PB.value;
		BG2Param.BG2Y.value += BG2Param.BG2PD.value;
	}
	
	if (  this->_isBGLayerShown[GPULayerID_BG3] &&
		((this->_BGLayer[GPULayerID_BG3].baseType == BGType_Affine) || (this->_BGLayer[GPULayerID_BG3].baseType == BGType_AffineExt)) )
	{
		IOREG_BG3Parameter &BG3Param = this->_IORegisterMap->BG3Param;
		
		BG3Param.BG3X.value += BG3Param.BG3PB.value;
		BG3Param.BG3Y.value += BG3Param.BG3PD.value;
	}
}

void GPUEngineBase::LastLineProcess()
{
	this->RefreshAffineStartRegs();
}

const GPU_IOREG& GPUEngineBase::GetIORegisterMap() const
{
	return *this->_IORegisterMap;
}

bool GPUEngineBase::IsForceBlankSet() const
{
	return (this->_IORegisterMap->DISPCNT.ForceBlank != 0);
}

bool GPUEngineBase::IsMasterBrightMaxOrMin() const
{
	return this->_currentRenderState.masterBrightnessIsMaxOrMin;
}

/*****************************************************************************/
//			ENABLING / DISABLING LAYERS
/*****************************************************************************/

bool GPUEngineBase::GetEnableState()
{
	return CommonSettings.showGpu.screens[this->_engineID];
}

bool GPUEngineBase::GetEnableStateApplied()
{
	return this->_enableEngine;
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
}

void GPUEngineBase::ApplySettings()
{
	this->_enableEngine = CommonSettings.showGpu.screens[this->_engineID];
	
	bool needResortBGLayers = ( (this->_enableBGLayer[GPULayerID_BG0] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG0]) ||
							    (this->_enableBGLayer[GPULayerID_BG1] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG1]) ||
							    (this->_enableBGLayer[GPULayerID_BG2] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG2]) ||
							    (this->_enableBGLayer[GPULayerID_BG3] != CommonSettings.dispLayers[this->_engineID][GPULayerID_BG3]) ||
							    (this->_enableBGLayer[GPULayerID_OBJ] != CommonSettings.dispLayers[this->_engineID][GPULayerID_OBJ]) );
	
	if (needResortBGLayers)
	{
		this->_enableBGLayer[GPULayerID_BG0] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG0];
		this->_enableBGLayer[GPULayerID_BG1] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG1];
		this->_enableBGLayer[GPULayerID_BG2] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG2];
		this->_enableBGLayer[GPULayerID_BG3] = CommonSettings.dispLayers[this->_engineID][GPULayerID_BG3];
		this->_enableBGLayer[GPULayerID_OBJ] = CommonSettings.dispLayers[this->_engineID][GPULayerID_OBJ];
		
		this->_ResortBGLayers();
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_TransitionLineNativeToCustom(GPUEngineCompositorInfo &compInfo)
{
	if (!this->_isLineRenderNative[compInfo.line.indexNative])
	{
		return;
	}
	
	if (compInfo.renderState.previouslyRenderedLayerID == GPULayerID_Backdrop)
	{
		if (this->_asyncClearIsRunning)
		{
			this->RenderLineClearAsyncWaitForCustomLine(compInfo.line.indexNative);
		}
		else
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					memset_u16(compInfo.target.lineColorHeadCustom, compInfo.renderState.workingBackdropColor16, compInfo.line.pixelCount);
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					memset_u32(compInfo.target.lineColorHeadCustom, compInfo.renderState.workingBackdropColor32.value, compInfo.line.pixelCount);
					break;
			}
		}
		
		this->_asyncClearTransitionedLineFromBackdropCount++;
	}
	else
	{
		this->RenderLineClearAsyncFinish();
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			CopyLineExpandHinted<0x3FFF, true, false, false, 2>(compInfo.line, compInfo.target.lineColorHeadNative, compInfo.target.lineColorHeadCustom);
		}
		else
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR666_Rev:
				{
					if ( (compInfo.line.widthCustom == GPU_FRAMEBUFFER_NATIVE_WIDTH) && (compInfo.line.renderCount == 1) )
					{
						ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapDst>((u16 *)compInfo.target.lineColorHeadNative, (u32 *)compInfo.target.lineColorHeadCustom, GPU_FRAMEBUFFER_NATIVE_WIDTH);
					}
					else
					{
						u32 *workingNativeBuffer32 = this->_targetDisplay->GetWorkingNativeBuffer32();
						ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapDst>((u16 *)compInfo.target.lineColorHeadNative, workingNativeBuffer32 + compInfo.line.blockOffsetNative, GPU_FRAMEBUFFER_NATIVE_WIDTH);
						CopyLineExpandHinted<0x3FFF, true, false, false, 4>(compInfo.line, workingNativeBuffer32 + compInfo.line.blockOffsetNative, compInfo.target.lineColorHeadCustom);
					}
					break;
				}
					
				case NDSColorFormat_BGR888_Rev:
				{
					if ( (compInfo.line.widthCustom == GPU_FRAMEBUFFER_NATIVE_WIDTH) && (compInfo.line.renderCount == 1) )
					{
						ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>((u16 *)compInfo.target.lineColorHeadNative, (u32 *)compInfo.target.lineColorHeadCustom, GPU_FRAMEBUFFER_NATIVE_WIDTH);
					}
					else
					{
						u32 *workingNativeBuffer32 = this->_targetDisplay->GetWorkingNativeBuffer32();
						ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>((u16 *)compInfo.target.lineColorHeadNative, workingNativeBuffer32 + compInfo.line.blockOffsetNative, GPU_FRAMEBUFFER_NATIVE_WIDTH);
						CopyLineExpandHinted<0x3FFF, true, false, false, 4>(compInfo.line, workingNativeBuffer32 + compInfo.line.blockOffsetNative, compInfo.target.lineColorHeadCustom);
					}
					break;
				}
					
				default:
					break;
			}
		}
		
		CopyLineExpandHinted<0x3FFF, true, false, false, 1>(compInfo.line, compInfo.target.lineLayerIDHeadNative, compInfo.target.lineLayerIDHeadCustom);
	}
	
	compInfo.target.lineColorHead = compInfo.target.lineColorHeadCustom;
	compInfo.target.lineLayerIDHead = compInfo.target.lineLayerIDHeadCustom;
	this->_isLineRenderNative[compInfo.line.indexNative] = false;
}

//this is fantastically inaccurate.
//we do the early return even though it reduces the resulting accuracy
//because we need the speed, and because it is inaccurate anyway
void GPUEngineBase::_MosaicSpriteLinePixel(GPUEngineCompositorInfo &compInfo, const size_t x, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const bool enableMosaic = (this->_oamList[this->_sprNum[x]].Mosaic != 0);
	if (!enableMosaic)
		return;
	
	const bool opaque = prioTab[x] <= 4;

	GPUEngineBase::MosaicColor::Obj objColor;
	objColor.color = LE_TO_LOCAL_16(dst[x]);
	objColor.alpha = dst_alpha[x];
	objColor.opaque = opaque;

	const size_t y = compInfo.line.indexNative;
	
	if (!compInfo.renderState.mosaicWidthOBJ->begin[x] || !compInfo.renderState.mosaicHeightOBJ->begin[y])
	{
		objColor = this->_mosaicColors.obj[compInfo.renderState.mosaicWidthOBJ->trunc[x]];
	}
	
	this->_mosaicColors.obj[x] = objColor;
	
	dst[x] = LE_TO_LOCAL_16(objColor.color);
	dst_alpha[x] = objColor.alpha;
	if (!objColor.opaque) prioTab[x] = 0x7F;
}

void GPUEngineBase::_MosaicSpriteLine(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (!compInfo.renderState.isOBJMosaicSet)
	{
		return;
	}
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		this->_MosaicSpriteLinePixel(compInfo, i, dst, dst_alpha, typeTab, prioTab);
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_Final(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	const u16 lineWidth = (COMPOSITORMODE == GPUCompositorMode_Debug) ? compInfo.renderState.selectedBGLayer->size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const s32 wh = compInfo.renderState.selectedBGLayer->size.width;
	const s32 ht = compInfo.renderState.selectedBGLayer->size.height;
	const s32 wmask = wh - 1;
	const s32 hmask = ht - 1;
	
	IOREG_BGnX x = param.BGnX;
	IOREG_BGnY y = param.BGnY;
	
	u8 index;
	u16 srcColor;
	
	// as an optimization, specially handle the fairly common case of
	// "unrotated + unscaled + no boundary checking required"
	if ( (param.BGnPA.Integer == 1) && (param.BGnPA.Fraction == 0) && (param.BGnPC.value == 0) )
	{
		s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if ( WRAP || ((auxX >= 0) && (auxX + lineWidth <= wh) && (auxY >= 0) && (auxY < ht)) )
		{
			for (size_t i = 0; i < lineWidth; i++)
			{
				GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, srcColor);
				
				if (WILLDEFERCOMPOSITING)
				{
					this->_deferredIndexNative[i] = index;
					this->_deferredColorNative[i] = srcColor;
				}
				else
				{
					this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, i, srcColor, (index != 0));
				}
				
				auxX++;
				
				if (WRAP)
				{
					auxX &= wmask;
				}
			}
			
			return;
		}
	}
	
	const s16 dx = param.BGnPA.value;
	const s16 dy = param.BGnPC.value;
	
	for (size_t i = 0; i < lineWidth; i++, x.value+=dx, y.value+=dy)
	{
		const s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if (WRAP || ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht)))
		{
			GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, srcColor);
			
			if (WILLDEFERCOMPOSITING)
			{
				this->_deferredIndexNative[i] = index;
				this->_deferredColorNative[i] = srcColor;
			}
			else
			{
				this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, i, srcColor, (index != 0));
			}
		}
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_ApplyWrap(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	this->_RenderPixelIterate_Final<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, GetPixelFunc, WRAP>(compInfo, param, map, tile, pal);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING, PixelLookupFunc GetPixelFunc>
void GPUEngineBase::_RenderPixelIterate(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	if (compInfo.renderState.selectedBGLayer->isDisplayWrapped)
	{
		this->_RenderPixelIterate_ApplyWrap<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, GetPixelFunc, true>(compInfo, param, map, tile, pal);
	}
	else
	{
		this->_RenderPixelIterate_ApplyWrap<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, GetPixelFunc, false>(compInfo, param, map, tile, pal);
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

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_CompositePixelImmediate(GPUEngineCompositorInfo &compInfo, const size_t srcX, u16 srcColor16, bool isOpaque)
{
	if (MOSAIC)
	{
		//due to this early out, we will get incorrect behavior in cases where
		//we enable mosaic in the middle of a frame. this is deemed unlikely.
		
		u16 *mosaicColorBG = this->_mosaicColors.bg[compInfo.renderState.selectedLayerID];
		
		if (compInfo.renderState.mosaicHeightBG->begin[compInfo.line.indexNative] && compInfo.renderState.mosaicWidthBG->begin[srcX])
		{
			srcColor16 = (isOpaque) ? (srcColor16 & 0x7FFF) : 0xFFFF;
			mosaicColorBG[srcX] = srcColor16;
		}
		else
		{
			srcColor16 = mosaicColorBG[compInfo.renderState.mosaicWidthBG->trunc[srcX]];
		}
		
		isOpaque = (srcColor16 != 0xFFFF);
	}
	
	if (!isOpaque)
	{
		return;
	}
	
	if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID][srcX] == 0) )
	{
		return;
	}
	
	compInfo.target.xNative = srcX;
	compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHeadNative + srcX;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative + srcX;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHeadNative + srcX;
	
	const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compInfo.renderState.selectedLayerID][compInfo.target.xNative] != 0) : true;
	pixelop.Composite16<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, GPULayerType_BG>(compInfo, srcColor16, enableColorEffect, 0, 0);
}

template <bool MOSAIC>
void GPUEngineBase::_PrecompositeNativeToCustomLineBG(GPUEngineCompositorInfo &compInfo)
{
	if (MOSAIC)
	{
		if (compInfo.renderState.mosaicHeightBG->begin[compInfo.line.indexNative])
		{
			this->_MosaicLine<true>(compInfo);
		}
		else
		{
			this->_MosaicLine<false>(compInfo);
		}
	}
	
	CopyLineExpand<0x3FFF, false, false, 2>(this->_deferredColorCustom, this->_deferredColorNative, compInfo.line.widthCustom, 1);
	CopyLineExpand<0x3FFF, false, false, 1>(this->_deferredIndexCustom, this->_deferredIndexNative, compInfo.line.widthCustom, 1);
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeNativeLineOBJ(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorNative16, const Color4u8 *__restrict srcColorNative32)
{
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
#ifdef USEMANUALVECTORIZATION
	this->_CompositeNativeLineOBJ_LoopOp<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, WILLPERFORMWINDOWTEST>(compInfo, srcColorNative16, NULL);
#else
	if (srcColorNative32 != NULL)
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++, srcColorNative32++, compInfo.target.xNative++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][i] == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][i] != 0) : true;
			pixelop.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, *srcColorNative32, enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][i], this->_sprType[compInfo.line.indexNative][i]);
		}
	}
	else
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++, srcColorNative16++, compInfo.target.xNative++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][i] == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][i] != 0) : true;
			pixelop.Composite16<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, GPULayerType_OBJ>(compInfo, *srcColorNative16, enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][i], this->_sprType[compInfo.line.indexNative][i]);
		}
	}
#endif
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeLineDeferred(GPUEngineCompositorInfo &compInfo, const u16 *__restrict srcColorCustom16, const u8 *__restrict srcIndexCustom)
{
	const u8 *windowTest = (compInfo.line.widthCustom == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID] : this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID];
	const u8 *colorEffectEnable = (compInfo.line.widthCustom == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? this->_enableColorEffectNative[compInfo.renderState.selectedLayerID] : this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID];
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	size_t i = 0;
	
#ifdef USEMANUALVECTORIZATION
	i = this->_CompositeLineDeferred_LoopOp<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo, windowTest, colorEffectEnable, srcColorCustom16, srcIndexCustom);
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < compInfo.line.pixelCount; i++, compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] == 0) )
		{
			continue;
		}
		
		if ( (LAYERTYPE == GPULayerType_BG) && (srcIndexCustom[compInfo.target.xCustom] == 0) )
		{
			continue;
		}
		
		const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID][compInfo.target.xCustom] != 0) : true;
		pixelop.Composite16<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE>(compInfo, srcColorCustom16[compInfo.target.xCustom], enableColorEffect, this->_sprAlphaCustom[compInfo.target.xCustom], this->_sprTypeCustom[compInfo.target.xCustom]);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_CompositeVRAMLineDeferred(GPUEngineCompositorInfo &compInfo, const void *__restrict vramColorPtr)
{
	const u8 *windowTest = (compInfo.line.widthCustom == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? this->_didPassWindowTestNative[compInfo.renderState.selectedLayerID] : this->_didPassWindowTestCustom[compInfo.renderState.selectedLayerID];
	const u8 *colorEffectEnable = (compInfo.line.widthCustom == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? this->_enableColorEffectNative[compInfo.renderState.selectedLayerID] : this->_enableColorEffectCustom[compInfo.renderState.selectedLayerID];
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	size_t i = 0;
	
#ifdef USEMANUALVECTORIZATION
	i = this->_CompositeVRAMLineDeferred_LoopOp<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE, WILLPERFORMWINDOWTEST>(compInfo, windowTest, colorEffectEnable, vramColorPtr);
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < compInfo.line.pixelCount; i++, compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
	{
		if (compInfo.target.xCustom >= compInfo.line.widthCustom)
		{
			compInfo.target.xCustom -= compInfo.line.widthCustom;
		}
		
		if ( WILLPERFORMWINDOWTEST && (windowTest[compInfo.target.xCustom] == 0) )
		{
			continue;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			if ( (LAYERTYPE != GPULayerType_OBJ) && ((((u32 *)vramColorPtr)[i] & 0xFF000000) == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (colorEffectEnable[compInfo.target.xCustom] != 0) : true;
			pixelop.Composite32<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE>(compInfo, ((Color4u8 *)vramColorPtr)[i], enableColorEffect, this->_sprAlphaCustom[compInfo.target.xCustom], this->_sprTypeCustom[compInfo.target.xCustom]);
		}
		else
		{
			if ( (LAYERTYPE != GPULayerType_OBJ) && ((((u16 *)vramColorPtr)[i] & 0x8000) == 0) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (colorEffectEnable[compInfo.target.xCustom] != 0) : true;
			pixelop.Composite16<COMPOSITORMODE, OUTPUTFORMAT, LAYERTYPE>(compInfo, ((u16 *)vramColorPtr)[i], enableColorEffect, this->_sprAlphaCustom[compInfo.target.xCustom], this->_sprTypeCustom[compInfo.target.xCustom]);
		}
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_RenderLine_BGText(GPUEngineCompositorInfo &compInfo, const u16 XBG, const u16 YBG)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const u16 lineWidth = (COMPOSITORMODE == GPUCompositorMode_Debug) ? compInfo.renderState.selectedBGLayer->size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const u16 lg    = compInfo.renderState.selectedBGLayer->size.width;
	const u16 ht    = compInfo.renderState.selectedBGLayer->size.height;
	const u32 tile  = compInfo.renderState.selectedBGLayer->tileEntryAddress;
	const u16 wmask = lg - 1;
	const u16 hmask = ht - 1;
	
	const size_t pixCountLo = 8 - (XBG & 0x0007);
	size_t x = 0;
	size_t xoff = XBG;
	
	const u16 tmp = (YBG & hmask) >> 3;
	u32 map = compInfo.renderState.selectedBGLayer->tileMapAddress + (tmp & 31) * 64;
	if (tmp > 31)
		map += ADDRESS_STEP_512B << compInfo.renderState.selectedBGLayer->BGnCNT.ScreenSize;
	
	if (compInfo.renderState.selectedBGLayer->BGnCNT.PaletteMode == PaletteMode_16x16) // color: 16 palette entries
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
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx & 0x0F;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx & 0x0F;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					tileColorIdx--;
				}
				
				for (; x < xfin; tileColorIdx--)
				{
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx >> 4;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx >> 4;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					
					if (x < xfin)
					{
						if (WILLDEFERCOMPOSITING)
						{
							this->_deferredIndexNative[x] = *tileColorIdx & 0x0F;
							this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
						}
						else
						{
							index = *tileColorIdx & 0x0F;
							color = LE_TO_LOCAL_16(pal[index + tilePalette]);
							this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
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
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx >> 4;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx >> 4;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					tileColorIdx++;
				}
				
				for (; x < xfin; tileColorIdx++)
				{
					if (WILLDEFERCOMPOSITING)
					{
						this->_deferredIndexNative[x] = *tileColorIdx & 0x0F;
						this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
					}
					else
					{
						index = *tileColorIdx & 0x0F;
						color = LE_TO_LOCAL_16(pal[index + tilePalette]);
						this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
					}
					
					x++;
					xoff++;
					
					if (x < xfin)
					{
						if (WILLDEFERCOMPOSITING)
						{
							this->_deferredIndexNative[x] = *tileColorIdx >> 4;
							this->_deferredColorNative[x] = LE_TO_LOCAL_16(pal[this->_deferredIndexNative[x] + tilePalette]);
						}
						else
						{
							index = *tileColorIdx >> 4;
							color = LE_TO_LOCAL_16(pal[index + tilePalette]);
							this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
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
		const u16 *__restrict pal = (DISPCNT.ExBGxPalette_Enable) ? *(compInfo.renderState.selectedBGLayer->extPalette) : this->_paletteBG;
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
				if (WILLDEFERCOMPOSITING)
				{
					this->_deferredIndexNative[x] = *tileColorIdx;
					this->_deferredColorNative[x] = LE_TO_LOCAL_16(tilePal[this->_deferredIndexNative[x]]);
				}
				else
				{
					const u8 index = *tileColorIdx;
					const u16 color = LE_TO_LOCAL_16(tilePal[index]);
					this->_CompositePixelImmediate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (index != 0));
				}
			}
		}
	}
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_RenderLine_BGAffine(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param)
{
	this->_RenderPixelIterate<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_tiled_8bit_entry>(compInfo, param, compInfo.renderState.selectedBGLayer->tileMapAddress, compInfo.renderState.selectedBGLayer->tileEntryAddress, this->_paletteBG);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_RenderLine_BGExtended(GPUEngineCompositorInfo &compInfo, const IOREG_BGnParameter &param, bool &outUseCustomVRAM)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	switch (compInfo.renderState.selectedBGLayer->type)
	{
		case BGType_AffineExt_256x16: // 16  bit bgmap entries
		{
			if (DISPCNT.ExBGxPalette_Enable)
			{
				this->_RenderPixelIterate< COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_tiled_16bit_entry<true> >(compInfo, param, compInfo.renderState.selectedBGLayer->tileMapAddress, compInfo.renderState.selectedBGLayer->tileEntryAddress, *(compInfo.renderState.selectedBGLayer->extPalette));
			}
			else
			{
				this->_RenderPixelIterate< COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_tiled_16bit_entry<false> >(compInfo, param, compInfo.renderState.selectedBGLayer->tileMapAddress, compInfo.renderState.selectedBGLayer->tileEntryAddress, this->_paletteBG);
			}
			break;
		}
			
		case BGType_AffineExt_256x1: // 256 colors
			this->_RenderPixelIterate<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_256_map>(compInfo, param, compInfo.renderState.selectedBGLayer->BMPAddress, 0, this->_paletteBG);
			break;
			
		case BGType_AffineExt_Direct: // direct colors / BMP
		{
			outUseCustomVRAM = false;
			
			if (!MOSAIC)
			{
				const bool isRotationScaled = ( (param.BGnPA.Integer  != 1) ||
				                                (param.BGnPA.Fraction != 0) ||
				                                (param.BGnPC.value    != 0) ||
				                                (param.BGnX.value     != 0) ||
				                                (param.BGnY.Integer   != (s32)compInfo.line.indexNative) ||
				                                (param.BGnY.Fraction  != 0) );
				if (!isRotationScaled)
				{
					const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(compInfo.renderState.selectedBGLayer->BMPAddress) - MMU.ARM9_LCD) / sizeof(u16);
					
					if (vramPixel < (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
					{
						const size_t blockID   = vramPixel >> 16;
						const size_t blockLine = (vramPixel >> 8) & 0x000000FF;
						
						GPU->GetEngineMain()->VerifyVRAMLineDidChange(blockID, compInfo.line.indexNative + blockLine);
						outUseCustomVRAM = !GPU->GetEngineMain()->IsLineCaptureNative(blockID, compInfo.line.indexNative + blockLine);
					}
				}
			}
			
			if (!outUseCustomVRAM)
			{
				this->_RenderPixelIterate<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_BMP_map>(compInfo, param, compInfo.renderState.selectedBGLayer->BMPAddress, 0, this->_paletteBG);
			}
			else
			{
				this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
			}
			break;
		}
			
		case BGType_Large8bpp: // large screen 256 colors
			this->_RenderPixelIterate<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING, rot_256_map>(compInfo, param, compInfo.renderState.selectedBGLayer->largeBMPAddress, 0, this->_paletteBG);
			break;
			
		default:
			break;
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_LineText(GPUEngineCompositorInfo &compInfo)
{
	if (COMPOSITORMODE == GPUCompositorMode_Debug)
	{
		this->_RenderLine_BGText<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, 0, compInfo.line.indexNative);
	}
	else
	{
		this->_RenderLine_BGText<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, compInfo.renderState.selectedBGLayer->xOffset, compInfo.line.indexNative + compInfo.renderState.selectedBGLayer->yOffset);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_LineRot(GPUEngineCompositorInfo &compInfo)
{
	if (COMPOSITORMODE == GPUCompositorMode_Debug)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, (s32)compInfo.line.blockOffsetNative};
		this->_RenderLine_BGAffine<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, debugParams);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (compInfo.renderState.selectedLayerID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGAffine<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, *bgParams);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineBase::_LineExtRot(GPUEngineCompositorInfo &compInfo, bool &outUseCustomVRAM)
{
	if (COMPOSITORMODE == GPUCompositorMode_Debug)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, (s32)compInfo.line.blockOffsetNative};
		this->_RenderLine_BGExtended<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, debugParams, outUseCustomVRAM);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (compInfo.renderState.selectedLayerID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGExtended<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, *bgParams, outUseCustomVRAM);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

/*****************************************************************************/
//			SPRITE RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

template <bool ISDEBUGRENDER, bool ISOBJMODEBITMAP>
FORCEINLINE void GPUEngineBase::_RenderSpriteUpdatePixel(GPUEngineCompositorInfo &compInfo, size_t frameX,
														 const u16 *__restrict srcPalette, const u8 palIndex, const OBJMode objMode, const u8 prio, const u8 spriteNum,
														 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if ( (ISOBJMODEBITMAP && ((*srcPalette & 0x8000) == 0)) || (!ISOBJMODEBITMAP && (palIndex == 0)) )
	{
		return;
	}
	
	if (ISDEBUGRENDER)
	{
		//sprites draw in order, so EQUAL priority also trumps later sprites
		if(prioTab[frameX] <= prio)
			return;
		dst[frameX] = (ISOBJMODEBITMAP) ? *srcPalette : LE_TO_LOCAL_16(srcPalette[palIndex]);
		prioTab[frameX] = prio;
		return;
	}
	
	if ( !ISOBJMODEBITMAP && (objMode == OBJMode_Window) )
	{
		this->_sprWin[compInfo.line.indexNative][frameX] = 1;
		return;
	}
	
	if (prio < prioTab[frameX])
	{
		dst[frameX]           = (ISOBJMODEBITMAP) ? *srcPalette : LE_TO_LOCAL_16(srcPalette[palIndex]);
		dst_alpha[frameX]     = (ISOBJMODEBITMAP) ? palIndex : 0xFF;
		typeTab[frameX]       = (ISOBJMODEBITMAP) ? OBJMode_Bitmap : objMode;
		prioTab[frameX]       = prio;
		this->_sprNum[frameX] = spriteNum;
	}
}

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
template <bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSpriteBMP(GPUEngineCompositorInfo &compInfo,
									 const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									 const u8 spriteAlpha, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const u16 *__restrict vramBuffer = (u16 *)MMU_gpu_map(objAddress);
	size_t i = 0;
	
#ifdef USEMANUALVECTORIZATION
	if (readXStep == 1)
	{
		i = this->_RenderSpriteBMP_LoopOp<ISDEBUGRENDER>(length, spriteAlpha, prio, spriteNum, vramBuffer, frameX, spriteX, dst, dst_alpha, typeTab, prioTab);
	}
#endif
	
	for (; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u16 vramColor = LE_TO_LOCAL_16(vramBuffer[spriteX]);
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, true>(compInfo, frameX, &vramColor, spriteAlpha+1, OBJMode_Bitmap, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite256(GPUEngineCompositorInfo &compInfo,
									 const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									 const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									 u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	for (size_t i = 0; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u32 palIndexAddress = objAddress + (u32)( (spriteX & 0x0007) + ((spriteX & 0xFFF8) << 3) );
		const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(palIndexAddress);
		const u8 idx8 = *palIndexBuffer;
		
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx8, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

template<bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSprite16(GPUEngineCompositorInfo &compInfo,
									const u32 objAddress, const size_t length, size_t frameX, size_t spriteX, const s32 readXStep,
									const u16 *__restrict palColorBuffer, const OBJMode objMode, const u8 prio, const u8 spriteNum,
									u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	for (size_t i = 0; i < length; i++, frameX++, spriteX+=readXStep)
	{
		const u32 spriteX_word = (u32)spriteX >> 1;
		const u32 palIndexAddress = objAddress + (spriteX_word & 0x0003) + ((spriteX_word & 0xFFFC) << 3);
		const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(palIndexAddress);
		const u8 palIndex = *palIndexBuffer;
		const u8 idx4 = (spriteX & 1) ? palIndex >> 4 : palIndex & 0x0F;
		
		this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx4, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
	}
}

// return val means if the sprite is to be drawn or not
bool GPUEngineBase::_ComputeSpriteVars(GPUEngineCompositorInfo &compInfo, const OAMAttributes &spriteInfo, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir)
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
	y = (compInfo.line.indexNative - sprY) & 0xFF;                        /* get the y line within sprite coords */
	if (y >= sprSize.height)
		return false;

	if ((sprX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || ((sprX+sprSize.width) <= 0))	/* sprite pixels outside of line */
		return false;				/* not to be drawn */

	// sprite portion out of the screen (LEFT)
	if (sprX < 0)
	{
		lg += sprX;	
		x = -(sprX);
		sprX = 0;
	}
	// sprite portion out of the screen (RIGHT)
	if ((sprX+sprSize.width) >= GPU_FRAMEBUFFER_NATIVE_WIDTH)
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

u32 GPUEngineBase::_SpriteAddressBMP(GPUEngineCompositorInfo &compInfo, const OAMAttributes &spriteInfo, const SpriteSize sprSize, const s32 y)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	if (DISPCNT.OBJ_BMP_mapping)
	{
		//tested by buffy sacrifice damage blood splatters in corner
		return this->_sprMem + (spriteInfo.TileIndex << compInfo.renderState.spriteBMPBoundary) + (y * sprSize.width * 2);
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
void GPUEngineBase::_SpriteRender(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (compInfo.renderState.spriteRenderMode == SpriteRenderMode_Sprite1D)
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite1D, ISDEBUGRENDER>(compInfo, dst, dst_alpha, typeTab, prioTab);
	else
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite2D, ISDEBUGRENDER>(compInfo, dst, dst_alpha, typeTab, prioTab);
}

void GPUEngineBase::SpritePrepareRenderDebug(u16 *dst)
{
	memset(this->_sprPrio, 0x7F, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
}

void GPUEngineBase::SpriteRenderDebug(const u16 lineIndex, u16 *dst)
{
	GPUEngineCompositorInfo compInfo;
	memset(&compInfo, 0, sizeof(compInfo));
	
	compInfo.renderState.displayOutputMode = GPUDisplayMode_Normal;
	compInfo.renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	compInfo.renderState.selectedLayerID = GPULayerID_OBJ;
	compInfo.renderState.colorEffect = ColorEffect_Disable;
	compInfo.renderState.masterBrightnessMode = GPUMasterBrightMode_Disable;
	compInfo.renderState.masterBrightnessIsMaxOrMin = true;
	compInfo.renderState.spriteRenderMode = this->_currentRenderState.spriteRenderMode;
	compInfo.renderState.spriteBoundary = this->_currentRenderState.spriteBoundary;
	compInfo.renderState.spriteBMPBoundary = this->_currentRenderState.spriteBMPBoundary;
	
	compInfo.line.indexNative = lineIndex;
	compInfo.line.indexCustom = compInfo.line.indexNative;
	compInfo.line.widthCustom = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compInfo.line.renderCount = 1;
	compInfo.line.pixelCount = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compInfo.line.blockOffsetNative = compInfo.line.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compInfo.line.blockOffsetCustom = compInfo.line.blockOffsetNative;
	
	compInfo.target.lineColorHead = dst;
	compInfo.target.lineColorHeadNative = compInfo.target.lineColorHead;
	compInfo.target.lineColorHeadCustom = compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerIDHead = NULL;
	compInfo.target.lineLayerIDHeadNative = NULL;
	compInfo.target.lineLayerIDHeadCustom = NULL;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor = (void **)&compInfo.target.lineColor16;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = NULL;
	
	this->_SpriteRender<true>(compInfo, dst, NULL, NULL, &this->_sprPrio[lineIndex][0]);
}

template <SpriteRenderMode MODE, bool ISDEBUGRENDER>
void GPUEngineBase::_SpriteRenderPerform(GPUEngineCompositorInfo &compInfo, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	size_t cost = 0;
	
	for (size_t spriteNum = 0; spriteNum < 128; spriteNum++)
	{
		OAMAttributes spriteInfo = this->_oamList[spriteNum];
		
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
		s32 frameX;
		s32 frameY;
		s32 spriteX;
		s32 spriteY;
		s32 length;
		s32 readXStep;
		u8 prio = spriteInfo.Priority;
		
		if (spriteInfo.RotScale != 0)
		{
			s32		fieldX, fieldY, auxX, auxY, realX, realY;
			u8		blockparameter;
			s16		dx, dmx, dy, dmy;
			
			// Get sprite positions and size
			frameX = spriteInfo.X;
			frameY = spriteInfo.Y;
			sprSize = GPUEngineBase::_sprSizeTab[spriteInfo.Size][spriteInfo.Shape];
			
			// Copy sprite size, to check change it if needed
			fieldX = sprSize.width;
			fieldY = sprSize.height;
			length = sprSize.width;
			
			// If we are using double size mode, double our control vars
			if (spriteInfo.DoubleSize != 0)
			{
				fieldX <<= 1;
				fieldY <<= 1;
				length <<= 1;
			}
			
			//check if the sprite is visible y-wise. unfortunately our logic for x and y is different due to our scanline based rendering
			//tested thoroughly by many large sprites in Super Robot Wars K which wrap around the screen
			spriteY = (compInfo.line.indexNative - frameY) & 0xFF;
			if (spriteY >= fieldY)
				continue;
			
			//check if sprite is visible x-wise.
			if ((frameX == GPU_FRAMEBUFFER_NATIVE_WIDTH) || (frameX + fieldX <= 0))
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
			realX = (sprSize.width  << 7) - (fieldX >> 1)*dx - (fieldY >> 1)*dmx + spriteY*dmx;
			realY = (sprSize.height << 7) - (fieldX >> 1)*dy - (fieldY >> 1)*dmy + spriteY*dmy;
			
			if (frameX < 0)
			{
				// If sprite is not in the window
				if (frameX + fieldX <= 0)
					continue;

				// Otherwise, is partially visible
				length += frameX;
				realX -= frameX*dx;
				realY -= frameX*dy;
				frameX = 0;
			}
			else
			{
				if (frameX + fieldX > GPU_FRAMEBUFFER_NATIVE_WIDTH)
					length = GPU_FRAMEBUFFER_NATIVE_WIDTH - frameX;
			}
			
			if (objMode == OBJMode_Bitmap) // Rotozoomed direct color
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;
				
				const u32 objAddress = this->_SpriteAddressBMP(compInfo, spriteInfo, sprSize, 0);
				
				for (size_t j = 0; j < length; ++j, ++frameX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;
					
					//this is all very slow, and so much dup code with other rotozoomed modes.
					//dont bother fixing speed until this whole thing gets reworked
					
					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						u32 objOffset = 0;
						
						if (DISPCNT.OBJ_BMP_2D_dim)
						{
							//tested by knights in the nightmare
							objOffset = ((this->_SpriteAddressBMP(compInfo, spriteInfo, sprSize, auxY) - objAddress) / 2) + auxX;
						}
						else
						{
							//tested by lego indiana jones (somehow?)
							//tested by buffy sacrifice damage blood splatters in corner
							objOffset = (auxY * sprSize.width) + auxX;
						}
						
						const u32 vramAddress = objAddress + (objOffset << 1);
						const u16 *vramBuffer = (u16 *)MMU_gpu_map(vramAddress);
						const u16 vramColor = LE_TO_LOCAL_16(*vramBuffer);
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, true>(compInfo, frameX, &vramColor, spriteInfo.PaletteIndex, OBJMode_Bitmap, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
					}
					
					// Add the rotation/scale coefficients, here the rotation/scaling is performed
					realX += dx;
					realY += dy;
				}
			}
			else if (spriteInfo.PaletteMode == PaletteMode_1x256) // If we are using 1 palette of 256 colours
			{
				const u32 objAddress = this->_sprMem + (spriteInfo.TileIndex << compInfo.renderState.spriteBoundary);
				const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(objAddress);
				const u16 *__restrict palColorBuffer = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;
				
				for (size_t j = 0; j < length; ++j, ++frameX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = (realX >> 8);
					auxY = (realY >> 8);
					
					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						const size_t palOffset = (MODE == SpriteRenderMode_Sprite2D) ? (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*8) :
						                                                               (auxX&0x7) + ((auxX&0xFFF8)<<3) + ((auxY>>3)*sprSize.width*8) + ((auxY&0x7)*8);
						const u8 idx8 = palIndexBuffer[palOffset];
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx8, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
					}
					
					// Add the rotation/scale coefficients, here the rotation/scaling is performed
					realX += dx;
					realY += dy;
				}
			}
			else // Rotozoomed 16/16 palette
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + (spriteInfo.TileIndex << 5) : this->_sprMem + (spriteInfo.TileIndex << compInfo.renderState.spriteBoundary);
				const u8 *__restrict palIndexBuffer = (u8 *)MMU_gpu_map(objAddress);
				const u16 *__restrict palColorBuffer = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);
				
				for (size_t j = 0; j < length; ++j, ++frameX)
				{
					// Get the integer part of the fixed point 8.8, and check if it lies inside the sprite data
					auxX = realX >> 8;
					auxY = realY >> 8;

					if (auxX >= 0 && auxY >= 0 && auxX < sprSize.width && auxY < sprSize.height)
					{
						const size_t palOffset = (MODE == SpriteRenderMode_Sprite2D) ? ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)<<10) + ((auxY&0x7)*4) :
						                                                               ((auxX>>1)&0x3) + (((auxX>>1)&0xFFFC)<<3) + ((auxY>>3)*sprSize.width*4) + ((auxY&0x7)*4);
						const u8 palIndex = palIndexBuffer[palOffset];
						const u8 idx4 = (auxX & 1) ? palIndex >> 4 : palIndex & 0x0F;
						
						this->_RenderSpriteUpdatePixel<ISDEBUGRENDER, false>(compInfo, frameX, palColorBuffer, idx4, objMode, prio, spriteNum, dst, dst_alpha, typeTab, prioTab);
					}
					
					// Add the rotation/scale coeficients, here the rotation/scaling  is performed
					realX += dx;
					realY += dy;
				}
			}
		}
		else //NOT rotozoomed
		{
			if (!this->_ComputeSpriteVars(compInfo, spriteInfo, sprSize, frameX, frameY, spriteX, spriteY, length, readXStep))
				continue;
			
			cost += sprSize.width;
			
			if (objMode == OBJMode_Bitmap) //sprite is in BMP format
			{
				//transparent (i think, dont bother to render?) if alpha is 0
				if (spriteInfo.PaletteIndex == 0)
					continue;
				
				const u32 objAddress = this->_SpriteAddressBMP(compInfo, spriteInfo, sprSize, spriteY);
				
				this->_RenderSpriteBMP<ISDEBUGRENDER>(compInfo,
													  objAddress, length, frameX, spriteX, readXStep,
													  spriteInfo.PaletteIndex, OBJMode_Bitmap, prio, spriteNum,
													  dst, dst_alpha, typeTab, prioTab);
				
				// When rendering at a custom framebuffer size, save a copy of the OBJ address as a reference
				// for reading the custom VRAM.
				const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(objAddress) - MMU.ARM9_LCD) / sizeof(u16);
				if (vramPixel < (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
				{
					const size_t blockID   = vramPixel >> 16;
					const size_t blockLine = (vramPixel >> 8) & 0x000000FF;
					const size_t linePixel = vramPixel & 0x000000FF;
					
					if (!GPU->GetEngineMain()->IsLineCaptureNative(blockID, blockLine) && (linePixel == 0))
					{
						this->_vramBlockOBJAddress = objAddress;
					}
				}
			}
			else if (spriteInfo.PaletteMode == PaletteMode_1x256) //256 colors; handles OBJ windows too
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((spriteY>>3)<<10) + ((spriteY&0x7)*8) :
				                                                             this->_sprMem + (spriteInfo.TileIndex<<compInfo.renderState.spriteBoundary) + ((spriteY>>3)*sprSize.width*8) + ((spriteY&0x7)*8);
				
				const u16 *__restrict palColorBuffer = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;
				
				this->_RenderSprite256<ISDEBUGRENDER>(compInfo,
													  objAddress, length, frameX, spriteX, readXStep,
													  palColorBuffer, objMode, prio, spriteNum,
													  dst, dst_alpha, typeTab, prioTab);
			}
			else // 16 colors; handles OBJ windows too
			{
				const u32 objAddress = (MODE == SpriteRenderMode_Sprite2D) ? this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((spriteY>>3)<<10) + ((spriteY&0x7)*4) :
				                                                             this->_sprMem + (spriteInfo.TileIndex<<compInfo.renderState.spriteBoundary) + ((spriteY>>3)*sprSize.width*4) + ((spriteY&0x7)*4);
				
				const u16 *__restrict palColorBuffer = this->_paletteOBJ + (spriteInfo.PaletteIndex << 4);
				
				this->_RenderSprite16<ISDEBUGRENDER>(compInfo,
													 objAddress, length, frameX, spriteX, readXStep,
													 palColorBuffer, objMode, prio, spriteNum,
													 dst, dst_alpha, typeTab, prioTab);
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_Layers(GPUEngineCompositorInfo &compInfo)
{
	const size_t customPixelBytes = this->_targetDisplay->GetPixelBytes();
	itemsForPriority_t *item;
	
	// Optimization: For normal display mode, render straight to the output buffer when that is what we are going to end
	// up displaying anyway. Otherwise, we need to use the working buffer.
	compInfo.target.lineColorHeadNative = (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) ? this->_targetDisplay->GetNativeBuffer16() + compInfo.line.blockOffsetNative : this->_internalRenderLineTargetNative;
	compInfo.target.lineColorHeadCustom = (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->_targetDisplay->GetCustomBuffer() + (compInfo.line.blockOffsetCustom * customPixelBytes) : (u8 *)this->_internalRenderLineTargetCustom + (compInfo.line.blockOffsetCustom * customPixelBytes);
	compInfo.target.lineColorHead = compInfo.target.lineColorHeadNative;
	
	compInfo.target.lineLayerIDHeadNative = this->_renderLineLayerIDNative[compInfo.line.indexNative];
	compInfo.target.lineLayerIDHeadCustom = this->_renderLineLayerIDCustom + (compInfo.line.blockOffsetCustom * sizeof(u8));
	compInfo.target.lineLayerIDHead = compInfo.target.lineLayerIDHeadNative;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	compInfo.renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	
	this->_RenderLine_Clear(compInfo);
	
	// for all the pixels in the line
	if (this->_isBGLayerShown[GPULayerID_OBJ])
	{
		this->_vramBlockOBJAddress = 0;
		this->_RenderLine_SetupSprites(compInfo);
	}
	
	if (WILLPERFORMWINDOWTEST)
	{
		this->_PerformWindowTesting(compInfo);
	}
	
	// paint lower priorities first
	// then higher priorities on top
	for (size_t prio = NB_PRIORITIES; prio > 0; )
	{
		prio--;
		item = &(this->_itemsForPriority[prio]);
		// render BGs
		if (this->_isAnyBGLayerShown)
		{
			for (size_t i = 0; i < item->nbBGs; i++)
			{
				const GPULayerID layerID = (GPULayerID)item->BGs[i];
				
				if (this->_isBGLayerShown[layerID] && this->_BGLayer[layerID].isVisible)
				{
					compInfo.renderState.selectedLayerID = layerID;
					compInfo.renderState.selectedBGLayer = &this->_BGLayer[layerID];
					
					if (this->_engineID == GPUEngineID_Main)
					{
						if ( (layerID == GPULayerID_BG0) && ((GPUEngineA *)this)->WillRender3DLayer() )
						{
#ifndef DISABLE_COMPOSITOR_FAST_PATHS
							if ( !compInfo.renderState.dstAnyBlendEnable && (  (compInfo.renderState.colorEffect == ColorEffect_Disable) ||
																			   !compInfo.renderState.srcEffectEnable[GPULayerID_BG0] ||
																			 (((compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) || (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)) && (compInfo.renderState.blendEVY == 0)) ) )
							{
								((GPUEngineA *)this)->RenderLine_Layer3D<GPUCompositorMode_Copy, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_BG0] && (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) )
							{
								((GPUEngineA *)this)->RenderLine_Layer3D<GPUCompositorMode_BrightUp, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_BG0] && (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness) )
							{
								((GPUEngineA *)this)->RenderLine_Layer3D<GPUCompositorMode_BrightDown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							else
#endif
							{
								((GPUEngineA *)this)->RenderLine_Layer3D<GPUCompositorMode_Unknown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
							}
							continue;
						}
					}
										
#ifndef DISABLE_COMPOSITOR_FAST_PATHS
					if ( (compInfo.renderState.colorEffect == ColorEffect_Disable) ||
						 !compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID] ||
						((compInfo.renderState.colorEffect == ColorEffect_Blend) && !compInfo.renderState.dstAnyBlendEnable) ||
						(((compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) || (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)) && (compInfo.renderState.blendEVY == 0)) )
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_Copy, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					else if ( !WILLPERFORMWINDOWTEST && compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID] && (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) )
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_BrightUp, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					else if ( !WILLPERFORMWINDOWTEST && compInfo.renderState.srcEffectEnable[compInfo.renderState.selectedLayerID] && (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness) )
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_BrightDown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					else
#endif
					{
						this->_RenderLine_LayerBG<GPUCompositorMode_Unknown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo);
					}
					
					compInfo.renderState.previouslyRenderedLayerID = layerID;
				} //layer enabled
			}
		}
		
		// render sprite Pixels
		if ( this->_isBGLayerShown[GPULayerID_OBJ] && (item->nbPixelsX > 0) )
		{
			compInfo.renderState.selectedLayerID = GPULayerID_OBJ;
			compInfo.renderState.selectedBGLayer = NULL;
			
#ifndef DISABLE_COMPOSITOR_FAST_PATHS
			if ( !compInfo.renderState.dstAnyBlendEnable && (  (compInfo.renderState.colorEffect == ColorEffect_Disable) ||
															   !compInfo.renderState.srcEffectEnable[GPULayerID_OBJ] ||
															 (((compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) || (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness)) && (compInfo.renderState.blendEVY == 0)) ) )
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_Copy, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_OBJ] && (compInfo.renderState.colorEffect == ColorEffect_IncreaseBrightness) )
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_BrightUp, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			else if ( !WILLPERFORMWINDOWTEST && !compInfo.renderState.dstAnyBlendEnable && compInfo.renderState.srcEffectEnable[GPULayerID_OBJ] && (compInfo.renderState.colorEffect == ColorEffect_DecreaseBrightness) )
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_BrightDown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			else
#endif
			{
				this->_RenderLine_LayerOBJ<GPUCompositorMode_Unknown, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, item);
			}
			
			compInfo.renderState.previouslyRenderedLayerID = GPULayerID_OBJ;
		}
	}
}

void GPUEngineBase::_RenderLine_SetupSprites(GPUEngineCompositorInfo &compInfo)
{
	itemsForPriority_t *item;
	
	this->_needExpandSprColorCustom = false;
	
	//n.b. - this is clearing the sprite line buffer to the background color,
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_sprColor, compInfo.renderState.backdropColor16);
	
	//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
	//how it interacts with this. I wish we knew why we needed this
	
	this->_SpriteRender<false>(compInfo, this->_sprColor, this->_sprAlpha[compInfo.line.indexNative], this->_sprType[compInfo.line.indexNative], this->_sprPrio[compInfo.line.indexNative]);
	this->_MosaicSpriteLine(compInfo, this->_sprColor, this->_sprAlpha[compInfo.line.indexNative], this->_sprType[compInfo.line.indexNative], this->_sprPrio[compInfo.line.indexNative]);
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		// assign them to the good priority item
		const size_t prio = this->_sprPrio[compInfo.line.indexNative][i];
		if (prio >= 4) continue;
		
		item = &(this->_itemsForPriority[prio]);
		item->PixelsX[item->nbPixelsX] = i;
		item->nbPixelsX++;
	}
	
	if ( (compInfo.line.widthCustom > GPU_FRAMEBUFFER_NATIVE_WIDTH) || (this->_targetDisplay->GetColorFormat() != NDSColorFormat_BGR555_Rev) )
	{
		bool isLineComplete = false;
		
		for (size_t i = 0; i < NB_PRIORITIES; i++)
		{
			item = &(this->_itemsForPriority[i]);
			
			if (item->nbPixelsX == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				isLineComplete = true;
				break;
			}
		}
		
		if (isLineComplete)
		{
			this->_needExpandSprColorCustom = true;
			CopyLineExpandHinted<0x3FFF, false, false, false, 1>(compInfo.line, this->_sprAlpha[compInfo.line.indexNative], this->_sprAlphaCustom);
			CopyLineExpandHinted<0x3FFF, false, false, false, 1>(compInfo.line, this->_sprType[compInfo.line.indexNative], this->_sprTypeCustom);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_LayerOBJ(GPUEngineCompositorInfo &compInfo, itemsForPriority_t *__restrict item)
{
	bool useCustomVRAM = false;
	
	if (this->_vramBlockOBJAddress != 0)
	{
		const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(this->_vramBlockOBJAddress) - MMU.ARM9_LCD) / sizeof(u16);
		
		if (vramPixel < (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
		{
			const size_t blockID   = vramPixel >> 16;
			const size_t blockLine = (vramPixel >> 8) & 0x000000FF;
			
			GPU->GetEngineMain()->VerifyVRAMLineDidChange(blockID, blockLine);
			useCustomVRAM = !GPU->GetEngineMain()->IsLineCaptureNative(blockID, blockLine);
		}
	}
	
	if (useCustomVRAM)
	{
		this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
	}
	
	if (item->nbPixelsX == GPU_FRAMEBUFFER_NATIVE_WIDTH)
	{
		if (this->_isLineRenderNative[compInfo.line.indexNative])
		{
			this->_CompositeNativeLineOBJ<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, WILLPERFORMWINDOWTEST>(compInfo, this->_sprColor, NULL);
		}
		else
		{
			if (useCustomVRAM)
			{
				const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->_vramBlockOBJAddress, 0);
				this->_CompositeVRAMLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo, vramColorPtr);
			}
			else
			{
				// Lazily expand the sprite color line since there are also many instances where the OBJ layer will be
				// reading directly out of VRAM instead of reading out of the rendered sprite line.
				if (this->_needExpandSprColorCustom)
				{
					this->_needExpandSprColorCustom = false;
					CopyLineExpandHinted<0x3FFF, false, false, false, 2>(compInfo.line, this->_sprColor, this->_sprColorCustom);
				}
				
				this->_CompositeLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ, WILLPERFORMWINDOWTEST>(compInfo, this->_sprColorCustom, NULL);
			}
		}
	}
	else
	{
		if (this->_isLineRenderNative[compInfo.line.indexNative])
		{
			for (size_t i = 0; i < item->nbPixelsX; i++)
			{
				const size_t srcX = item->PixelsX[i];
				
				if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
				{
					continue;
				}
				
				compInfo.target.xNative = srcX;
				compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
				compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead + srcX;
				compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHead + srcX;
				compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead + srcX;
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
				pixelop.Composite16<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, GPULayerType_OBJ>(compInfo, this->_sprColor[srcX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
			}
		}
		else
		{
			void *__restrict dstColorPtr = compInfo.target.lineColorHead;
			u8 *__restrict dstLayerIDPtr = compInfo.target.lineLayerIDHead;
			
			if (useCustomVRAM)
			{
				const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(this->_vramBlockOBJAddress, 0);
				
				for (size_t line = 0; line < compInfo.line.renderCount; line++)
				{
					compInfo.target.lineColor16 = (u16 *)dstColorPtr;
					compInfo.target.lineColor32 = (Color4u8 *)dstColorPtr;
					compInfo.target.lineLayerID = dstLayerIDPtr;
					
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
						{
							continue;
						}
						
						compInfo.target.xNative = srcX;
						compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = compInfo.target.xCustom + p;
							
							compInfo.target.lineColor16 = (u16 *)dstColorPtr + dstX;
							compInfo.target.lineColor32 = (Color4u8 *)dstColorPtr + dstX;
							compInfo.target.lineLayerID = dstLayerIDPtr + dstX;
							
							const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
							
							if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
							{
								pixelop.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, ((Color4u8 *)vramColorPtr)[dstX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
							}
							else
							{
								pixelop.Composite16<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, ((u16 *)vramColorPtr)[dstX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
							}
						}
					}
					
					vramColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((Color4u8 *)vramColorPtr + compInfo.line.widthCustom) : (void *)((u16 *)vramColorPtr + compInfo.line.widthCustom);
					dstColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void *)((u16 *)dstColorPtr + compInfo.line.widthCustom) : (void *)((Color4u8 *)dstColorPtr + compInfo.line.widthCustom);
					dstLayerIDPtr += compInfo.line.widthCustom;
				}
			}
			else
			{
				for (size_t line = 0; line < compInfo.line.renderCount; line++)
				{
					compInfo.target.lineColor16 = (u16 *)dstColorPtr;
					compInfo.target.lineColor32 = (Color4u8 *)dstColorPtr;
					compInfo.target.lineLayerID = dstLayerIDPtr;
					
					for (size_t i = 0; i < item->nbPixelsX; i++)
					{
						const size_t srcX = item->PixelsX[i];
						
						if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[GPULayerID_OBJ][srcX] == 0) )
						{
							continue;
						}
						
						compInfo.target.xNative = srcX;
						compInfo.target.xCustom = _gpuDstPitchIndex[srcX];
						
						for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
						{
							const size_t dstX = compInfo.target.xCustom + p;
							
							compInfo.target.lineColor16 = (u16 *)dstColorPtr + dstX;
							compInfo.target.lineColor32 = (Color4u8 *)dstColorPtr + dstX;
							compInfo.target.lineLayerID = dstLayerIDPtr + dstX;
							
							const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[GPULayerID_OBJ][compInfo.target.xNative] != 0) : true;
							pixelop.Composite16<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_OBJ>(compInfo, this->_sprColor[srcX], enableColorEffect, this->_sprAlpha[compInfo.line.indexNative][srcX], this->_sprType[compInfo.line.indexNative][srcX]);
						}
					}
					
					dstColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void *)((u16 *)dstColorPtr + compInfo.line.widthCustom) : (void *)((Color4u8 *)dstColorPtr + compInfo.line.widthCustom);
					dstLayerIDPtr += compInfo.line.widthCustom;
				}
			}
		}
	}
}

void GPUEngineBase::TransitionRenderStatesToDisplayInfo(NDSDisplayInfo &mutableInfo)
{
	// The master brightness is normally handled at the GPU engine level as a scanline operation,
	// but since it is the last color operation that occurs on a scanline, it is possible to push
	// this work up to the display level instead. This method is used to transition the master
	// brightness states to the display level so it can be processed as a framebuffer operation.
	
	const GPUEngineCompositorInfo &compInfoZero = this->_currentCompositorInfo[0];
	bool needApplyMasterBrightness = false;
	bool masterBrightnessDiffersPerLine = false;
	
	for (size_t line = 0; line < GPU_FRAMEBUFFER_NATIVE_HEIGHT; line++)
	{
		const GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[line];
		
		if ( !needApplyMasterBrightness &&
			 (compInfo.renderState.masterBrightnessIntensity != 0) &&
			((compInfo.renderState.masterBrightnessMode == GPUMasterBrightMode_Up) || (compInfo.renderState.masterBrightnessMode == GPUMasterBrightMode_Down)) )
		{
			needApplyMasterBrightness = true;
		}
		
		mutableInfo.masterBrightnessMode[this->_targetDisplay->GetDisplayID()][line] = compInfo.renderState.masterBrightnessMode;
		mutableInfo.masterBrightnessIntensity[this->_targetDisplay->GetDisplayID()][line] = compInfo.renderState.masterBrightnessIntensity;
		
		// Most games maintain the exact same master brightness values for all 192 lines, so we
		// can easily apply the master brightness to the entire framebuffer at once, which is
		// faster than applying it per scanline.
		//
		// However, some games need to have the master brightness values applied on a per-scanline
		// basis since they can differ for each scanline. For example, Mega Man Zero Collection
		// purposely changes the master brightness intensity to 31 on line 0, 0 on line 16, and
		// then back to 31 on line 176. Since the MMZC are originally GBA games, the master
		// brightness intensity changes are done to disable the unused scanlines on the NDS.
		if ( !masterBrightnessDiffersPerLine &&
			((compInfo.renderState.masterBrightnessMode != compInfoZero.renderState.masterBrightnessMode) ||
			 (compInfo.renderState.masterBrightnessIntensity != compInfoZero.renderState.masterBrightnessIntensity)) )
		{
			masterBrightnessDiffersPerLine = true;
		}
	}
	
	mutableInfo.masterBrightnessDiffersPerLine[this->_targetDisplay->GetDisplayID()] = masterBrightnessDiffersPerLine;
	mutableInfo.needApplyMasterBrightness[this->_targetDisplay->GetDisplayID()] = needApplyMasterBrightness;
}

template <size_t WIN_NUM>
bool GPUEngineBase::_IsWindowInsideVerticalRange(GPUEngineCompositorInfo &compInfo)
{
	if (WIN_NUM == 0 && !compInfo.renderState.windowState.WIN0_ENABLED)
	{
		return false;
	}
	
	if (WIN_NUM == 1 && !compInfo.renderState.windowState.WIN1_ENABLED)
	{
		return false;
	}
	
	const u16 windowTop    = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Top : this->_IORegisterMap->WIN1V.Top;
	const u16 windowBottom = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Bottom : this->_IORegisterMap->WIN1V.Bottom;

	if (windowTop > windowBottom)
	{
		if ((compInfo.line.indexNative < windowTop) && (compInfo.line.indexNative > windowBottom))
		{
			return false;
		}
	}
	else
	{
		if ((compInfo.line.indexNative < windowTop) || (compInfo.line.indexNative >= windowBottom))
		{
			return false;
		}
	}

	//the x windows will apply for this scanline
	return true;
}

template <size_t WIN_NUM>
void GPUEngineBase::_UpdateWINH(GPUEngineCompositorInfo &compInfo)
{
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

void GPUEngineBase::_PerformWindowTesting(GPUEngineCompositorInfo &compInfo)
{
	compInfo.renderState.windowState.IsWithinVerticalRange_WIN0 = (this->_IsWindowInsideVerticalRange<0>(compInfo)) ? 1 : 0;
	compInfo.renderState.windowState.IsWithinVerticalRange_WIN1 = (this->_IsWindowInsideVerticalRange<1>(compInfo)) ? 1 : 0;
	
	// While window testing isn't too expensive to do, it might become a performance issue when it's
	// performed on every line when running large framebuffer widths on a system without SIMD
	// acceleration. Therefore, we're going to check all the relevant window states and only perform
	// window testing when one of those states actually changes.
	
	if ( (this->_prevWINState.value == compInfo.renderState.windowState.value) &&
		 (this->_prevWINCoord.value == this->_IORegisterMap->WIN_COORD.value) &&
		 (this->_prevWINCtrl.value == this->_IORegisterMap->WIN_CTRL.value) &&
		 (compInfo.renderState.windowState.WINOBJ_ENABLED == 0) ) // Sprite window states continually update. If WINOBJ is enabled, that means we need to continually perform window testing as well.
	{
		return;
	}
	
	const bool needUpdateWIN0H = (this->_prevWINCoord.WIN0H.value != this->_IORegisterMap->WIN0H.value);
	const bool needUpdateWIN1H = (this->_prevWINCoord.WIN1H.value != this->_IORegisterMap->WIN1H.value);
	
	this->_prevWINState.value = compInfo.renderState.windowState.value;
	this->_prevWINCoord.value = this->_IORegisterMap->WIN_COORD.value;
	this->_prevWINCtrl.value = this->_IORegisterMap->WIN_CTRL.value;
	
	if (needUpdateWIN0H)
	{
		this->_UpdateWINH<0>(compInfo);
	}
	
	if (needUpdateWIN1H)
	{
		this->_UpdateWINH<1>(compInfo);
	}
	
	const u8 *__restrict win0Ptr   = (compInfo.renderState.windowState.IsWithinVerticalRange_WIN0) ? this->_h_win[0] : NULL;
	const u8 *__restrict win1Ptr   = (compInfo.renderState.windowState.IsWithinVerticalRange_WIN1) ? this->_h_win[1] : NULL;
	const u8 *__restrict winObjPtr = (compInfo.renderState.windowState.WINOBJ_ENABLED) ? this->_sprWin[compInfo.line.indexNative] : NULL;
	
	for (size_t layerID = GPULayerID_BG0; layerID <= GPULayerID_OBJ; layerID++)
	{
		if (!this->_isBGLayerShown[layerID])
		{
			continue;
		}
		
		this->_PerformWindowTestingNative(compInfo, layerID, win0Ptr, win1Ptr, winObjPtr, this->_didPassWindowTestNative[layerID], this->_enableColorEffectNative[layerID]);
		
		if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 1))
		{
			CopyLineExpand<1, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 1, 1);
			CopyLineExpand<1, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 1, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 2))
		{
			CopyLineExpand<2, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 2, 1);
			CopyLineExpand<2, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 2, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 3))
		{
			CopyLineExpand<3, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 3, 1);
			CopyLineExpand<3, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 3, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 4))
		{
			CopyLineExpand<4, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 4, 1);
			CopyLineExpand<4, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 4, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 5))
		{
			CopyLineExpand<5, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 5, 1);
			CopyLineExpand<5, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 5, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 6))
		{
			CopyLineExpand<6, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 6, 1);
			CopyLineExpand<6, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 6, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 7))
		{
			CopyLineExpand<7, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 7, 1);
			CopyLineExpand<7, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 7, 1);
		}
		else if (compInfo.line.widthCustom == (GPU_FRAMEBUFFER_NATIVE_WIDTH * 8))
		{
			CopyLineExpand<8, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 8, 1);
			CopyLineExpand<8, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * 8, 1);
		}
		else if ((compInfo.line.widthCustom % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0)
		{
			CopyLineExpand<0x3FFF, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compInfo.line.widthCustom, 1);
			CopyLineExpand<0x3FFF, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compInfo.line.widthCustom, 1);
		}
		else
		{
			CopyLineExpand<-1, false, false, 1>(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compInfo.line.widthCustom, 1);
			CopyLineExpand<-1, false, false, 1>(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compInfo.line.widthCustom, 1);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
FORCEINLINE void GPUEngineBase::_RenderLine_LayerBG_Final(GPUEngineCompositorInfo &compInfo)
{
	bool useCustomVRAM = false;
	
	if (WILLDEFERCOMPOSITING)
	{
		// Because there is no guarantee for any given pixel to be written out, we need
		// to zero out the deferred index buffer so that unwritten pixels can properly
		// fail in _CompositeLineDeferred(). If we don't do this, then previously rendered
		// layers may leave garbage indices for the current layer to mistakenly use if
		// the current layer just so happens to have unwritten pixels.
		//
		// Test case: The score screen in Sonic Rush will be taken over by BG2, filling
		// the screen with blue, unless this initialization is done each time.
		memset(this->_deferredIndexNative, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	}
	
	switch (compInfo.renderState.selectedBGLayer->baseType)
	{
		case BGType_Text: this->_LineText<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo); break;
		case BGType_Affine: this->_LineRot<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo); break;
		case BGType_AffineExt: this->_LineExtRot<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, useCustomVRAM); break;
		case BGType_Large8bpp: this->_LineExtRot<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, WILLDEFERCOMPOSITING>(compInfo, useCustomVRAM); break;
		case BGType_Invalid:
			PROGINFO("Attempting to render an invalid BG type\n");
			break;
		default:
			break;
	}
	
	// If compositing at the native size, each pixel is composited immediately. However, if
	// compositing at a custom size, pixel gathering and pixel compositing are split up into
	// separate steps. If compositing at a custom size, composite the entire line now.
	if ( (COMPOSITORMODE != GPUCompositorMode_Debug) && (WILLDEFERCOMPOSITING || !this->_isLineRenderNative[compInfo.line.indexNative] || useCustomVRAM) )
	{
		if (useCustomVRAM)
		{
			const void *__restrict vramColorPtr = GPU->GetCustomVRAMAddressUsingMappedAddress<OUTPUTFORMAT>(compInfo.renderState.selectedBGLayer->BMPAddress, compInfo.line.blockOffsetCustom);
			this->_CompositeVRAMLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG, WILLPERFORMWINDOWTEST>(compInfo, vramColorPtr);
		}
		else
		{
			this->_PrecompositeNativeToCustomLineBG<MOSAIC>(compInfo);
			this->_CompositeLineDeferred<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_BG, WILLPERFORMWINDOWTEST>(compInfo, this->_deferredColorCustom, this->_deferredIndexCustom);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_RenderLine_LayerBG_ApplyMosaic(GPUEngineCompositorInfo &compInfo)
{
	if (this->_isLineRenderNative[compInfo.line.indexNative])
	{
		this->_RenderLine_LayerBG_Final<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, false>(compInfo);
	}
	else
	{
		this->_RenderLine_LayerBG_Final<COMPOSITORMODE, OUTPUTFORMAT, MOSAIC, WILLPERFORMWINDOWTEST, true>(compInfo);
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
FORCEINLINE void GPUEngineBase::_RenderLine_LayerBG(GPUEngineCompositorInfo &compInfo)
{
#ifndef DISABLE_MOSAIC
	if (compInfo.renderState.selectedBGLayer->isMosaic && compInfo.renderState.isBGMosaicSet)
	{
		this->_RenderLine_LayerBG_ApplyMosaic<COMPOSITORMODE, OUTPUTFORMAT, true, WILLPERFORMWINDOWTEST>(compInfo);
	}
	else
#endif
	{
		this->_RenderLine_LayerBG_ApplyMosaic<COMPOSITORMODE, OUTPUTFORMAT, false, WILLPERFORMWINDOWTEST>(compInfo);
	}
}

void GPUEngineBase::RenderLayerBG(const GPULayerID layerID, u16 *dstColorBuffer)
{
	GPUEngineCompositorInfo compInfo;
	memset(&compInfo, 0, sizeof(compInfo));
	
	compInfo.renderState.displayOutputMode = GPUDisplayMode_Normal;
	compInfo.renderState.previouslyRenderedLayerID = GPULayerID_Backdrop;
	compInfo.renderState.selectedLayerID = layerID;
	compInfo.renderState.selectedBGLayer = &this->_BGLayer[layerID];
	compInfo.renderState.colorEffect = ColorEffect_Disable;
	compInfo.renderState.masterBrightnessMode = GPUMasterBrightMode_Disable;
	compInfo.renderState.masterBrightnessIsMaxOrMin = true;
	compInfo.renderState.spriteRenderMode = this->_currentRenderState.spriteRenderMode;
	compInfo.renderState.spriteBoundary = this->_currentRenderState.spriteBoundary;
	compInfo.renderState.spriteBMPBoundary = this->_currentRenderState.spriteBMPBoundary;
	
	const size_t layerWidth = compInfo.renderState.selectedBGLayer->size.width;
	const size_t layerHeight = compInfo.renderState.selectedBGLayer->size.height;
	compInfo.line.widthCustom = layerWidth;
	compInfo.line.renderCount = 1;
	
	compInfo.target.lineLayerIDHead = NULL;
	compInfo.target.lineLayerIDHeadNative = NULL;
	compInfo.target.lineLayerIDHeadCustom = NULL;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = compInfo.target.xNative;
	compInfo.target.lineColor = (void **)&compInfo.target.lineColor16;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHeadNative;
	compInfo.target.lineLayerID = NULL;
	
	for (size_t lineIndex = 0; lineIndex < layerHeight; lineIndex++)
	{
		compInfo.line.indexNative = lineIndex;
		compInfo.line.indexCustom = compInfo.line.indexNative;
		compInfo.line.pixelCount = layerWidth;
		compInfo.line.blockOffsetNative = compInfo.line.indexNative * layerWidth;
		compInfo.line.blockOffsetCustom = compInfo.line.blockOffsetNative;
		
		compInfo.target.lineColorHead = (u16 *)dstColorBuffer + compInfo.line.blockOffsetNative;
		compInfo.target.lineColorHeadNative = compInfo.target.lineColorHead;
		compInfo.target.lineColorHeadCustom = compInfo.target.lineColorHeadNative;
		
		this->_RenderLine_LayerBG_Final<GPUCompositorMode_Debug, NDSColorFormat_BGR555_Rev, false, false, false>(compInfo);
	}
}

void GPUEngineBase::_RenderLineBlank(const size_t l)
{
	// Native rendering only.
	// Just clear the line using white pixels.
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_targetDisplay->GetNativeBuffer16() + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFF);
}

void GPUEngineBase::_HandleDisplayModeOff(const size_t l)
{
	// Native rendering only.
	// In this display mode, the line is cleared to white.
	this->_RenderLineBlank(l);
}

void GPUEngineBase::_HandleDisplayModeNormal(const size_t l)
{
	if (!this->_isLineRenderNative[l])
	{
		this->_targetDisplay->SetIsLineNative(l, false);
	}
}

void GPUEngineBase::ParseReg_WININ()
{
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.WIN0_enable[GPULayerID_BG0] = (this->_IORegisterMap->WIN0IN.BG0_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN0_enable[GPULayerID_BG1] = (this->_IORegisterMap->WIN0IN.BG1_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN0_enable[GPULayerID_BG2] = (this->_IORegisterMap->WIN0IN.BG2_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN0_enable[GPULayerID_BG3] = (this->_IORegisterMap->WIN0IN.BG3_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN0_enable[GPULayerID_OBJ] = (this->_IORegisterMap->WIN0IN.OBJ_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN0_enable[WINDOWCONTROL_EFFECTFLAG] = (this->_IORegisterMap->WIN0IN.Effect_Enable != 0) ? 0xFF : 0x00;
	
	renderState.WIN1_enable[GPULayerID_BG0] = (this->_IORegisterMap->WIN1IN.BG0_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN1_enable[GPULayerID_BG1] = (this->_IORegisterMap->WIN1IN.BG1_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN1_enable[GPULayerID_BG2] = (this->_IORegisterMap->WIN1IN.BG2_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN1_enable[GPULayerID_BG3] = (this->_IORegisterMap->WIN1IN.BG3_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN1_enable[GPULayerID_OBJ] = (this->_IORegisterMap->WIN1IN.OBJ_Enable != 0) ? 0xFF : 0x00;
	renderState.WIN1_enable[WINDOWCONTROL_EFFECTFLAG] = (this->_IORegisterMap->WIN1IN.Effect_Enable != 0) ? 0xFF : 0x00;
}

void GPUEngineBase::ParseReg_WINOUT()
{
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.WINOUT_enable[GPULayerID_BG0] = (this->_IORegisterMap->WINOUT.BG0_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOUT_enable[GPULayerID_BG1] = (this->_IORegisterMap->WINOUT.BG1_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOUT_enable[GPULayerID_BG2] = (this->_IORegisterMap->WINOUT.BG2_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOUT_enable[GPULayerID_BG3] = (this->_IORegisterMap->WINOUT.BG3_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOUT_enable[GPULayerID_OBJ] = (this->_IORegisterMap->WINOUT.OBJ_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOUT_enable[WINDOWCONTROL_EFFECTFLAG] = (this->_IORegisterMap->WINOUT.Effect_Enable != 0) ? 0xFF : 0x00;
	
	renderState.WINOBJ_enable[GPULayerID_BG0] = (this->_IORegisterMap->WINOBJ.BG0_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOBJ_enable[GPULayerID_BG1] = (this->_IORegisterMap->WINOBJ.BG1_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOBJ_enable[GPULayerID_BG2] = (this->_IORegisterMap->WINOBJ.BG2_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOBJ_enable[GPULayerID_BG3] = (this->_IORegisterMap->WINOBJ.BG3_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOBJ_enable[GPULayerID_OBJ] = (this->_IORegisterMap->WINOBJ.OBJ_Enable != 0) ? 0xFF : 0x00;
	renderState.WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG] = (this->_IORegisterMap->WINOBJ.Effect_Enable != 0) ? 0xFF : 0x00;
}

void GPUEngineBase::ParseReg_MOSAIC()
{
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.mosaicWidthBG = &this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.BG_MosaicH];
	renderState.mosaicHeightBG = &this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.BG_MosaicV];
	renderState.mosaicWidthOBJ = &this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.OBJ_MosaicH];
	renderState.mosaicHeightOBJ = &this->_mosaicLookup.table[this->_IORegisterMap->MOSAIC.OBJ_MosaicV];
	
	renderState.isBGMosaicSet = (this->_IORegisterMap->MOSAIC.BG_MosaicH != 0) || (this->_IORegisterMap->MOSAIC.BG_MosaicV != 0);
	renderState.isOBJMosaicSet = (this->_IORegisterMap->MOSAIC.OBJ_MosaicH != 0) || (this->_IORegisterMap->MOSAIC.OBJ_MosaicV != 0);
}

void GPUEngineBase::ParseReg_BLDCNT()
{
	const IOREG_BLDCNT &BLDCNT = this->_IORegisterMap->BLDCNT;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.colorEffect = (ColorEffect)BLDCNT.ColorEffect;
	
	renderState.srcEffectEnable[GPULayerID_BG0] = (BLDCNT.BG0_Target1 != 0) ? 0xFF : 0x00;
	renderState.srcEffectEnable[GPULayerID_BG1] = (BLDCNT.BG1_Target1 != 0) ? 0xFF : 0x00;
	renderState.srcEffectEnable[GPULayerID_BG2] = (BLDCNT.BG2_Target1 != 0) ? 0xFF : 0x00;
	renderState.srcEffectEnable[GPULayerID_BG3] = (BLDCNT.BG3_Target1 != 0) ? 0xFF : 0x00;
	renderState.srcEffectEnable[GPULayerID_OBJ] = (BLDCNT.OBJ_Target1 != 0) ? 0xFF : 0x00;
	renderState.srcEffectEnable[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target1 != 0) ? 0xFF : 0x00;
	
	renderState.dstBlendEnable[GPULayerID_BG0] = (BLDCNT.BG0_Target2 != 0) ? 0xFF : 0x00;
	renderState.dstBlendEnable[GPULayerID_BG1] = (BLDCNT.BG1_Target2 != 0) ? 0xFF : 0x00;
	renderState.dstBlendEnable[GPULayerID_BG2] = (BLDCNT.BG2_Target2 != 0) ? 0xFF : 0x00;
	renderState.dstBlendEnable[GPULayerID_BG3] = (BLDCNT.BG3_Target2 != 0) ? 0xFF : 0x00;
	renderState.dstBlendEnable[GPULayerID_OBJ] = (BLDCNT.OBJ_Target2 != 0) ? 0xFF : 0x00;
	renderState.dstBlendEnable[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target2 != 0) ? 0xFF : 0x00;
	
	renderState.dstAnyBlendEnable = renderState.dstBlendEnable[GPULayerID_BG0] ||
	                                renderState.dstBlendEnable[GPULayerID_BG1] ||
	                                renderState.dstBlendEnable[GPULayerID_BG2] ||
	                                renderState.dstBlendEnable[GPULayerID_BG3] ||
	                                renderState.dstBlendEnable[GPULayerID_OBJ] ||
	                                renderState.dstBlendEnable[GPULayerID_Backdrop];
	
	// For the vectorized rendering loops, create a lookup table for each 128-bit lane.
	for (size_t i = 0; i < sizeof(renderState.dstBlendEnableVecLookup); i+=16)
	{
		renderState.dstBlendEnableVecLookup[i+0] = renderState.dstBlendEnable[GPULayerID_BG0];
		renderState.dstBlendEnableVecLookup[i+1] = renderState.dstBlendEnable[GPULayerID_BG1];
		renderState.dstBlendEnableVecLookup[i+2] = renderState.dstBlendEnable[GPULayerID_BG2];
		renderState.dstBlendEnableVecLookup[i+3] = renderState.dstBlendEnable[GPULayerID_BG3];
		renderState.dstBlendEnableVecLookup[i+4] = renderState.dstBlendEnable[GPULayerID_OBJ];
		renderState.dstBlendEnableVecLookup[i+5] = renderState.dstBlendEnable[GPULayerID_Backdrop];
	}
}

void GPUEngineBase::ParseReg_BLDALPHA()
{
	const IOREG_BLDALPHA &BLDALPHA = this->_IORegisterMap->BLDALPHA;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.blendEVA = (BLDALPHA.EVA >= 16) ? 16 : BLDALPHA.EVA;
	renderState.blendEVB = (BLDALPHA.EVB >= 16) ? 16 : BLDALPHA.EVB;
	renderState.blendTable555 = (TBlendTable *)&PixelOperation::BlendTable555[renderState.blendEVA][renderState.blendEVB][0][0];
}

void GPUEngineBase::ParseReg_BLDY()
{
	const IOREG_BLDY &BLDY = this->_IORegisterMap->BLDY;
	GPUEngineRenderState &renderState = this->_currentRenderState;
	
	renderState.blendEVY = (BLDY.EVY >= 16) ? 16 : BLDY.EVY;
	renderState.brightnessUpTable555 = &PixelOperation::BrightnessUpTable555[renderState.blendEVY][0];
	renderState.brightnessUpTable666 = &PixelOperation::BrightnessUpTable666[renderState.blendEVY][0];
	renderState.brightnessUpTable888 = &PixelOperation::BrightnessUpTable888[renderState.blendEVY][0];
	renderState.brightnessDownTable555 = &PixelOperation::BrightnessDownTable555[renderState.blendEVY][0];
	renderState.brightnessDownTable666 = &PixelOperation::BrightnessDownTable666[renderState.blendEVY][0];
	renderState.brightnessDownTable888 = &PixelOperation::BrightnessDownTable888[renderState.blendEVY][0];
}

const BGLayerInfo& GPUEngineBase::GetBGLayerInfoByID(const GPULayerID layerID)
{
	return this->_BGLayer[layerID];
}

NDSDisplayID GPUEngineBase::GetTargetDisplayByID() const
{
	return this->_targetDisplay->GetDisplayID();
}

NDSDisplay* GPUEngineBase::GetTargetDisplay() const
{
	return this->_targetDisplay;
}

void GPUEngineBase::SetTargetDisplay(NDSDisplay *theDisplay)
{
	if (this->_targetDisplay == theDisplay)
	{
		return;
	}
	
	this->DisplayDrawBuffersUpdate();
	this->_targetDisplay = theDisplay;
}

GPUEngineID GPUEngineBase::GetEngineID() const
{
	return this->_engineID;
}

void GPUEngineBase::AllocateWorkingBuffers(NDSColorFormat requestedColorFormat, size_t w, size_t h)
{
	void *oldWorkingLineColor = this->_internalRenderLineTargetCustom;
	u8 *oldWorkingLineLayerID = this->_renderLineLayerIDCustom;
	u8 *oldDeferredIndexCustom = this->_deferredIndexCustom;
	u16 *oldDeferredColorCustom = this->_deferredColorCustom;
	u16 *oldSprColorCustom = this->_sprColorCustom;
	u8 *oldSprAlphaCustom = this->_sprAlphaCustom;
	u8 *oldSprTypeCustom = this->_sprTypeCustom;
	u8 *oldDidPassWindowTestCustomMasterPtr = this->_didPassWindowTestCustomMasterPtr;
	
	this->_internalRenderLineTargetCustom = malloc_alignedPage(w * h * this->_targetDisplay->GetPixelBytes());
	this->_renderLineLayerIDCustom = (u8 *)malloc_alignedPage(w * (h + (_gpuLargestDstLineCount * 4)) * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	this->_deferredIndexCustom = (u8 *)malloc_alignedPage(w * sizeof(u8));
	this->_deferredColorCustom = (u16 *)malloc_alignedPage(w * sizeof(u16));
	
	this->_sprColorCustom = (u16 *)malloc_alignedPage(w * sizeof(u16));
	this->_sprAlphaCustom = (u8 *)malloc_alignedPage(w * sizeof(u8));
	this->_sprTypeCustom = (u8 *)malloc_alignedPage(w * sizeof(u8));
	
	u8 *newDidPassWindowTestCustomMasterPtr = (u8 *)malloc_alignedPage(w * 10 * sizeof(u8));
	
	this->_didPassWindowTestCustomMasterPtr = newDidPassWindowTestCustomMasterPtr;
	this->_didPassWindowTestCustom[GPULayerID_BG0] = this->_didPassWindowTestCustomMasterPtr + (0 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_BG1] = this->_didPassWindowTestCustomMasterPtr + (1 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_BG2] = this->_didPassWindowTestCustomMasterPtr + (2 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_BG3] = this->_didPassWindowTestCustomMasterPtr + (3 * w * sizeof(u8));
	this->_didPassWindowTestCustom[GPULayerID_OBJ] = this->_didPassWindowTestCustomMasterPtr + (4 * w * sizeof(u8));
	
	this->_enableColorEffectCustomMasterPtr = newDidPassWindowTestCustomMasterPtr + (w * 5 * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG0] = this->_enableColorEffectCustomMasterPtr + (0 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG1] = this->_enableColorEffectCustomMasterPtr + (1 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG2] = this->_enableColorEffectCustomMasterPtr + (2 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_BG3] = this->_enableColorEffectCustomMasterPtr + (3 * w * sizeof(u8));
	this->_enableColorEffectCustom[GPULayerID_OBJ] = this->_enableColorEffectCustomMasterPtr + (4 * w * sizeof(u8));
	
	for (size_t line = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		this->_currentCompositorInfo[line].line = GPU->GetLineInfoAtIndex(line);
		this->_currentCompositorInfo[line].target.lineColor = (this->_targetDisplay->GetColorFormat() == NDSColorFormat_BGR555_Rev) ? (void **)&this->_currentCompositorInfo[line].target.lineColor16 : (void **)&this->_currentCompositorInfo[line].target.lineColor32;
	}
	
	free_aligned(oldWorkingLineColor);
	free_aligned(oldWorkingLineLayerID);
	free_aligned(oldDeferredIndexCustom);
	free_aligned(oldDeferredColorCustom);
	free_aligned(oldSprColorCustom);
	free_aligned(oldSprAlphaCustom);
	free_aligned(oldSprTypeCustom);
	free_aligned(oldDidPassWindowTestCustomMasterPtr);
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
	
	printf("%p %02x\n", r, (u32)((uintptr_t)(&r->DISPCNT) - (uintptr_t)r) );
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

void GPUEngineBase::ParseAllRegisters()
{
	this->ParseReg_DISPCNT();
	// No need to call ParseReg_BGnCNT(), since it is already called by ParseReg_DISPCNT().
	
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
	
	this->ParseReg_WININ();
	this->ParseReg_WINOUT();
	
	this->ParseReg_MOSAIC();
	this->ParseReg_BLDCNT();
	this->ParseReg_BLDALPHA();
	this->ParseReg_BLDY();
	this->ParseReg_MASTER_BRIGHT();
}

GPUEngineA::GPUEngineA()
{
	_engineID = GPUEngineID_Main;
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
	
	_nativeLineCaptureCount[0] = GPU_VRAM_BLOCK_LINES;
	_nativeLineCaptureCount[1] = GPU_VRAM_BLOCK_LINES;
	_nativeLineCaptureCount[2] = GPU_VRAM_BLOCK_LINES;
	_nativeLineCaptureCount[3] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		_isLineCaptureNative[0][l] = true;
		_isLineCaptureNative[1][l] = true;
		_isLineCaptureNative[2][l] = true;
		_isLineCaptureNative[3][l] = true;
	}
	
	_3DFramebufferMain = (Color4u8 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(Color4u8));
	_3DFramebuffer16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	_captureWorkingDisplay16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingA16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingB16 = (u16 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16));
	_captureWorkingA32 = (Color4u8 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(Color4u8));
	_captureWorkingB32 = (Color4u8 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(Color4u8));
}

GPUEngineA::~GPUEngineA()
{
	free_aligned(this->_3DFramebufferMain);
	free_aligned(this->_3DFramebuffer16);
	free_aligned(this->_captureWorkingDisplay16);
	free_aligned(this->_captureWorkingA16);
	free_aligned(this->_captureWorkingB16);
	free_aligned(this->_captureWorkingA32);
	free_aligned(this->_captureWorkingB32);
}

GPUEngineA* GPUEngineA::Allocate()
{
	return new(malloc_alignedPage(sizeof(GPUEngineA))) GPUEngineA();
}

void GPUEngineA::FinalizeAndDeallocate()
{
	this->~GPUEngineA();
	free_aligned(this);
}

void GPUEngineA::Reset()
{
	this->SetTargetDisplay( GPU->GetDisplayMain() );
	this->GPUEngineBase::Reset();
	
	const size_t customWidth  = this->_targetDisplay->GetWidth();
	const size_t customHeight = this->_targetDisplay->GetHeight();
	
	memset(this->_3DFramebufferMain, 0, customWidth * customHeight * sizeof(Color4u8));
	memset(this->_3DFramebuffer16, 0, customWidth * customHeight * sizeof(u16));
	memset(this->_captureWorkingDisplay16, 0, customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingA16, 0, customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingB16, 0, customWidth * _gpuLargestDstLineCount * sizeof(u16));
	memset(this->_captureWorkingA32, 0, customWidth * _gpuLargestDstLineCount * sizeof(Color4u8));
	memset(this->_captureWorkingB32, 0, customWidth * _gpuLargestDstLineCount * sizeof(Color4u8));
	
	memset(&this->_dispCapCnt, 0, sizeof(DISPCAPCNT_parsed));
	this->_displayCaptureEnable = false;
	
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
	
	this->ResetCaptureLineStates(0);
	this->ResetCaptureLineStates(1);
	this->ResetCaptureLineStates(2);
	this->ResetCaptureLineStates(3);
}

void GPUEngineA::ResetCaptureLineStates(const size_t blockID)
{
	if (this->_nativeLineCaptureCount[blockID] == GPU_VRAM_BLOCK_LINES)
	{
		return;
	}
	
	this->_nativeLineCaptureCount[blockID] = GPU_VRAM_BLOCK_LINES;
	
	for (size_t l = 0; l < GPU_VRAM_BLOCK_LINES; l++)
	{
		this->_isLineCaptureNative[blockID][l] = true;
	}
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

Color4u8* GPUEngineA::Get3DFramebufferMain() const
{
	return this->_3DFramebufferMain;
}

u16* GPUEngineA::Get3DFramebuffer16() const
{
	return this->_3DFramebuffer16;
}

bool GPUEngineA::IsLineCaptureNative(const size_t blockID, const size_t blockLine)
{
	return this->_isLineCaptureNative[blockID][blockLine];
}

void* GPUEngineA::GetCustomVRAMBlockPtr(const size_t blockID)
{
	return this->_VRAMCustomBlockPtr[blockID];
}

void GPUEngineA::AllocateWorkingBuffers(NDSColorFormat requestedColorFormat, size_t w, size_t h)
{
	this->GPUEngineBase::AllocateWorkingBuffers(requestedColorFormat, w, h);
	
	Color4u8 *old3DFramebufferMain = this->_3DFramebufferMain;
	u16 *old3DFramebuffer16 = this->_3DFramebuffer16;
	u16 *oldCaptureWorkingDisplay16 = this->_captureWorkingDisplay16;
	u16 *oldCaptureWorkingA16 = this->_captureWorkingA16;
	u16 *oldCaptureWorkingB16 = this->_captureWorkingB16;
	Color4u8 *oldCaptureWorkingA32 = this->_captureWorkingA32;
	Color4u8 *oldCaptureWorkingB32 = this->_captureWorkingB32;
	
	this->_3DFramebufferMain = (Color4u8 *)malloc_alignedPage(w * h * sizeof(Color4u8));
	this->_3DFramebuffer16 = (u16 *)malloc_alignedPage(w * h * sizeof(u16));
	this->_captureWorkingDisplay16 = (u16 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(u16));
	this->_captureWorkingA16 = (u16 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(u16));
	this->_captureWorkingB16 = (u16 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(u16));
	this->_captureWorkingA32 = (Color4u8 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(Color4u8));
	this->_captureWorkingB32 = (Color4u8 *)malloc_alignedPage(w * _gpuLargestDstLineCount * sizeof(Color4u8));
	
	const GPUEngineLineInfo &lineInfo = this->_currentCompositorInfo[GPU_VRAM_BLOCK_LINES].line;
	
	if (this->_targetDisplay->GetColorFormat() == NDSColorFormat_BGR888_Rev)
	{
		this->_VRAMCustomBlockPtr[0] = (Color4u8 *)GPU->GetCustomVRAMBuffer();
		this->_VRAMCustomBlockPtr[1] = (Color4u8 *)this->_VRAMCustomBlockPtr[0] + (1 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[2] = (Color4u8 *)this->_VRAMCustomBlockPtr[0] + (2 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[3] = (Color4u8 *)this->_VRAMCustomBlockPtr[0] + (3 * lineInfo.indexCustom * w);
	}
	else
	{
		this->_VRAMCustomBlockPtr[0] = (u16 *)GPU->GetCustomVRAMBuffer();
		this->_VRAMCustomBlockPtr[1] = (u16 *)this->_VRAMCustomBlockPtr[0] + (1 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[2] = (u16 *)this->_VRAMCustomBlockPtr[0] + (2 * lineInfo.indexCustom * w);
		this->_VRAMCustomBlockPtr[3] = (u16 *)this->_VRAMCustomBlockPtr[0] + (3 * lineInfo.indexCustom * w);
	}
	
	free_aligned(old3DFramebufferMain);
	free_aligned(old3DFramebuffer16);
	free_aligned(oldCaptureWorkingDisplay16);
	free_aligned(oldCaptureWorkingA16);
	free_aligned(oldCaptureWorkingB16);
	free_aligned(oldCaptureWorkingA32);
	free_aligned(oldCaptureWorkingB32);
}

bool GPUEngineA::WillRender3DLayer()
{
	return ( this->_isBGLayerShown[GPULayerID_BG0] && (this->_IORegisterMap->DISPCNT.BG0_3D != 0) );
}

bool GPUEngineA::WillCapture3DLayerDirect(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return ( this->WillDisplayCapture(l) && (DISPCAPCNT.SrcA != 0) && (DISPCAPCNT.CaptureSrc != 1) );
}

bool GPUEngineA::WillDisplayCapture(const size_t l)
{
	//we must block captures when the capture dest is not mapped to LCDC.
	//mario kart does this (maybe due to a programming bug, but maybe emulation timing error) when spamming confirm key during course intro and through black transition
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return this->_displayCaptureEnable && (vramConfiguration.banks[DISPCAPCNT.VRAMWriteBlock].purpose == VramConfiguration::LCDC) && (l < this->_dispCapCnt.capy);
}

void GPUEngineA::SetDisplayCaptureEnable()
{
	this->_displayCaptureEnable = (this->_IORegisterMap->DISPCAPCNT.CaptureEnable != 0);
}

void GPUEngineA::ResetDisplayCaptureEnable()
{
	IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	if (this->_displayCaptureEnable)
	{
		DISPCAPCNT.CaptureEnable = 0;
		this->_displayCaptureEnable = false;
	}
}

bool GPUEngineA::VerifyVRAMLineDidChange(const size_t blockID, const size_t l)
{
	// This method must be called for ALL instances where captured lines in VRAM may be read back.
	//
	// If a line is captured at a custom size, we need to ensure that the line hasn't been changed between
	// capture time and read time. If the captured line has changed, then we need to fallback to using the
	// native captured line instead.
	
	if (this->_isLineCaptureNative[blockID][l])
	{
		return false;
	}
	
	u16 *__restrict capturedNativeLine = this->_VRAMNativeBlockCaptureCopyPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	const u16 *__restrict currentNativeLine = this->_VRAMNativeBlockPtr[blockID] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	const bool didVRAMLineChange = (memcmp(currentNativeLine, capturedNativeLine, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) != 0);
	if (didVRAMLineChange)
	{
		CopyLineExpandHinted<1, true, true, false, 2>(this->_currentCompositorInfo[l].line, this->_VRAMNativeBlockPtr[blockID], this->_VRAMNativeBlockCaptureCopyPtr[blockID]);
		this->_isLineCaptureNative[blockID][l] = true;
		this->_nativeLineCaptureCount[blockID]++;
	}
	
	return didVRAMLineChange;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::RenderLine(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	const bool isDisplayCaptureNeeded = this->WillDisplayCapture(l);
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	
	// Render the line
	if ( (compInfo.renderState.displayOutputMode == GPUDisplayMode_Normal) || isDisplayCaptureNeeded )
	{
		if (compInfo.renderState.isAnyWindowEnabled)
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, true>(compInfo);
		}
		else
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, false>(compInfo);
		}
	}
	
	if (compInfo.line.indexNative >= 191)
	{
		this->RenderLineClearAsyncFinish();
	}
	
	// Fill the display output
	if ( this->IsForceBlankSet() )
	{
		this->_RenderLineBlank(l);
	}
	else
	{
		switch (compInfo.renderState.displayOutputMode)
		{
			case GPUDisplayMode_Off: // Display Off (clear line to white)
				this->_HandleDisplayModeOff(l);
				break;
				
			case GPUDisplayMode_Normal: // Display BG and OBJ layers
				this->_HandleDisplayModeNormal(l);
				break;
				
			case GPUDisplayMode_VRAM: // Display VRAM framebuffer
				this->_HandleDisplayModeVRAM<OUTPUTFORMAT>(compInfo.line);
				break;
				
			case GPUDisplayMode_MainMemory: // Display Memory FIFO
				this->_HandleDisplayModeMainMemory(compInfo.line);
				break;
		}
	}
	
	//capture after displaying so that we can safely display vram before overwriting it here
	
	//BUG!!! if someone is capturing and displaying both from the fifo, then it will have been
	//consumed above by the display before we get here
	//(is that even legal? i think so)
	if (isDisplayCaptureNeeded)
	{
		if (DISPCAPCNT.CaptureSize == DisplayCaptureSize_128x128)
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH/2>(compInfo);
		}
		else
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH>(compInfo);
		}
	}
}

template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineA::RenderLine_Layer3D(GPUEngineCompositorInfo &compInfo)
{
	const Color4u8 *__restrict framebuffer3D = CurrentRenderer->GetFramebuffer();
	if (framebuffer3D == NULL)
	{
		return;
	}
	
	if ( (OUTPUTFORMAT != NDSColorFormat_BGR555_Rev) || !CurrentRenderer->IsFramebufferNativeSize())
	{
		this->_TransitionLineNativeToCustom<OUTPUTFORMAT>(compInfo);
	}
	
	const u8 *windowTest = (CurrentRenderer->GetFramebufferWidth() == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? this->_didPassWindowTestNative[GPULayerID_BG0] : this->_didPassWindowTestCustom[GPULayerID_BG0];
	const u8 *colorEffectEnable = (CurrentRenderer->GetFramebufferWidth() == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? this->_enableColorEffectNative[GPULayerID_BG0] : this->_enableColorEffectCustom[GPULayerID_BG0];
	
	const float customWidthScale = (float)compInfo.line.widthCustom / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const Color4u8 *__restrict srcLinePtr = framebuffer3D + compInfo.line.blockOffsetCustom;
	
	compInfo.target.xNative = 0;
	compInfo.target.xCustom = 0;
	compInfo.target.lineColor16 = (u16 *)compInfo.target.lineColorHead;
	compInfo.target.lineColor32 = (Color4u8 *)compInfo.target.lineColorHead;
	compInfo.target.lineLayerID = compInfo.target.lineLayerIDHead;
	
	// Horizontally offset the 3D layer by this amount.
	// Test case: Blowing up large objects in Nanostray 2 will cause the main screen to shake horizontally.
	const u16 hofs = (u16)( ((float)compInfo.renderState.selectedBGLayer->xOffset * customWidthScale) + 0.5f );
	
	if (hofs == 0)
	{
		size_t i = 0;
		
#ifdef USEMANUALVECTORIZATION
		i = this->_RenderLine_Layer3D_LoopOp<COMPOSITORMODE, OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compInfo, windowTest, colorEffectEnable, srcLinePtr);
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; i < compInfo.line.pixelCount; i++, srcLinePtr++, compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
		{
			if (compInfo.target.xCustom >= compInfo.line.widthCustom)
			{
				compInfo.target.xCustom -= compInfo.line.widthCustom;
			}
			
			if ( (srcLinePtr->a == 0) || (WILLPERFORMWINDOWTEST && (windowTest[compInfo.target.xCustom] == 0)) )
			{
				continue;
			}
			
			const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (colorEffectEnable[compInfo.target.xCustom] != 0) : true;
			pixelop.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D>(compInfo, *srcLinePtr, enableColorEffect, 0, 0);
		}
	}
	else
	{
		for (size_t line = 0; line < compInfo.line.renderCount; line++)
		{
			for (compInfo.target.xCustom = 0; compInfo.target.xCustom < compInfo.line.widthCustom; compInfo.target.xCustom++, compInfo.target.lineColor16++, compInfo.target.lineColor32++, compInfo.target.lineLayerID++)
			{
				if ( WILLPERFORMWINDOWTEST && (windowTest[compInfo.target.xCustom] == 0) )
				{
					continue;
				}
				
				size_t srcX = compInfo.target.xCustom + hofs;
				if (srcX >= compInfo.line.widthCustom * 2)
				{
					srcX -= compInfo.line.widthCustom * 2;
				}
				
				if ( (srcX >= compInfo.line.widthCustom) || (srcLinePtr[srcX].a == 0) )
				{
					continue;
				}
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (colorEffectEnable[compInfo.target.xCustom] != 0) : true;
				pixelop.Composite32<COMPOSITORMODE, OUTPUTFORMAT, GPULayerType_3D>(compInfo, srcLinePtr[srcX], enableColorEffect, 0, 0);
			}
			
			srcLinePtr += compInfo.line.widthCustom;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCaptureCustom(const IOREG_DISPCAPCNT &DISPCAPCNT,
												  const GPUEngineLineInfo &lineInfo,
												  const bool isReadDisplayLineNative,
												  const bool isReadVRAMLineNative,
												  const void *srcAPtr,
												  const void *srcBPtr,
												  void *dstCustomPtr)
{
	const u32 captureSrcBits = LOCAL_TO_LE_32(DISPCAPCNT.value) & 0x63000000;
	const size_t captureLengthExt = (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? lineInfo.widthCustom : lineInfo.widthCustom / 2;
	
	switch (captureSrcBits)
	{
		case 0x00000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x02000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		{
			if (isReadDisplayLineNative)
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, true, false>(lineInfo, srcAPtr, dstCustomPtr, captureLengthExt);
			}
			else
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, false, false>(lineInfo, srcAPtr, dstCustomPtr, captureLengthExt);
			}
			break;
		}
			
		case 0x01000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x03000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 1, CAPTURELENGTH, false, false>(lineInfo, srcAPtr, dstCustomPtr, captureLengthExt);
			break;
			
		case 0x20000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x21000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		{
			if (isReadVRAMLineNative)
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, true, false>(lineInfo, srcBPtr, dstCustomPtr, captureLengthExt);
			}
			else
			{
				this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 0, CAPTURELENGTH, false, false>(lineInfo, srcBPtr, dstCustomPtr, captureLengthExt);
			}
			break;
		}
			
		case 0x22000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x23000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		{
			if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
			{
				ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(this->_fifoLine16, (u32 *)srcBPtr, GPU_FRAMEBUFFER_NATIVE_WIDTH);
			}
			
			this->_RenderLine_DispCapture_Copy<OUTPUTFORMAT, 1, CAPTURELENGTH, true, false>(lineInfo, srcBPtr, dstCustomPtr, captureLengthExt);
			break;
		}
			
		case 0x40000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x41000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x42000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x43000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		case 0x60000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x62000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x61000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x63000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		{
			if ((DISPCAPCNT.SrcA == 0) && isReadDisplayLineNative)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					CopyLineExpandHinted<0x3FFF, true, false, false, 2>(lineInfo, srcAPtr, this->_captureWorkingA16);
					srcAPtr = this->_captureWorkingA16;
				}
				else
				{
					u32 *workingNativeBuffer32 = this->_targetDisplay->GetWorkingNativeBuffer32();
					ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapNone>((u16 *)srcAPtr, workingNativeBuffer32 + lineInfo.blockOffsetNative, GPU_FRAMEBUFFER_NATIVE_WIDTH);
					CopyLineExpandHinted<0x3FFF, true, false, false, 4>(lineInfo, workingNativeBuffer32 + lineInfo.blockOffsetNative, this->_captureWorkingA32);
					srcAPtr = this->_captureWorkingA32;
				}
			}
			
			if ((DISPCAPCNT.SrcB != 0) || isReadVRAMLineNative)
			{
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					CopyLineExpandHinted<0x3FFF, true, false, false, 2>(lineInfo, srcBPtr, this->_captureWorkingB16);
					srcBPtr = this->_captureWorkingB16;
				}
				else
				{
					if ((OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && (DISPCAPCNT.SrcB != 0))
					{
						ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(this->_fifoLine16, (u32 *)srcBPtr, GPU_FRAMEBUFFER_NATIVE_WIDTH);
					}
					
					CopyLineExpandHinted<0x3FFF, true, false, false, 4>(lineInfo, srcBPtr, this->_captureWorkingB32);
					srcBPtr = this->_captureWorkingB32;
				}
			}
			
			this->_RenderLine_DispCapture_Blend<OUTPUTFORMAT, CAPTURELENGTH, false>(lineInfo, srcAPtr, srcBPtr, dstCustomPtr, captureLengthExt);
			break;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCapture(const GPUEngineCompositorInfo &compInfo)
{
	assert( (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH/2) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) );
	
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	const u32 captureSrcBits = LOCAL_TO_LE_32(DISPCAPCNT.value) & 0x63000000;
	
	const size_t writeLineIndexWithOffset = (DISPCAPCNT.VRAMWriteOffset * 64) + compInfo.line.indexNative;
	const size_t readLineIndexWithOffset = (this->_dispCapCnt.readOffset * 64) + compInfo.line.indexNative;
	
	const bool isReadDisplayLineNative = this->_isLineRenderNative[compInfo.line.indexNative];
	const bool isRead3DLineNative = (OUTPUTFORMAT != NDSColorFormat_BGR888_Rev) && CurrentRenderer->IsFramebufferNativeSize();
	const bool isReadVRAMLineNative = this->_isLineCaptureNative[DISPCNT.VRAM_Block][readLineIndexWithOffset];
	
	bool willReadNativeVRAM = isReadVRAMLineNative;
	bool willWriteVRAMLineNative = true;
	bool needCaptureNative = true;
	bool needConvertDisplayLine23 = false;
	bool needConvertDisplayLine32 = false;
	
	//128-wide captures should write linearly into memory, with no gaps
	//this is tested by hotel dusk
	size_t dstNativeOffset = (DISPCAPCNT.VRAMWriteOffset * 64 * GPU_FRAMEBUFFER_NATIVE_WIDTH) + (compInfo.line.indexNative * CAPTURELENGTH);
	
	//Read/Write block wrap to 00000h when exceeding 1FFFFh (128k)
	//this has not been tested yet (I thought I needed it for hotel dusk, but it was fixed by the above)
	dstNativeOffset &= 0x0000FFFF;
	
	// Convert VRAM for native VRAM capture.
	const u16 *vramNative16 = (u16 *)MMU.blank_memory;
	
	if ( (DISPCAPCNT.SrcB == 0) && (DISPCAPCNT.CaptureSrc != 0) && (vramConfiguration.banks[DISPCNT.VRAM_Block].purpose == VramConfiguration::LCDC) )
	{
		size_t vramNativeOffset = readLineIndexWithOffset * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		vramNativeOffset &= 0x0000FFFF;
		vramNative16 = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + vramNativeOffset;
		
		this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, readLineIndexWithOffset);
		
		willReadNativeVRAM = this->_isLineCaptureNative[DISPCNT.VRAM_Block][readLineIndexWithOffset];
	}
	
	switch (captureSrcBits)
	{
		case 0x00000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x02000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isReadDisplayLineNative;
			needConvertDisplayLine23 = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) && !isReadDisplayLineNative;
			needConvertDisplayLine32 = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && !isReadDisplayLineNative;
			break;
			
		case 0x01000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x03000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isRead3DLineNative;
			break;
			
		case 0x20000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x21000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			willWriteVRAMLineNative = willReadNativeVRAM;
			break;
			
		case 0x22000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x23000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			this->_RenderLine_DispCapture_FIFOToBuffer(this->_fifoLine16);
			willWriteVRAMLineNative = true;
			break;
			
		case 0x40000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
		case 0x60000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			willWriteVRAMLineNative = (isReadDisplayLineNative && willReadNativeVRAM);
			needConvertDisplayLine23 = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) && !isReadDisplayLineNative;
			needConvertDisplayLine32 = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && !isReadDisplayLineNative;
			break;
			
		case 0x42000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
		case 0x62000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isReadDisplayLineNative;
			needConvertDisplayLine23 = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) && !isReadDisplayLineNative;
			needConvertDisplayLine32 = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && !isReadDisplayLineNative;
			this->_RenderLine_DispCapture_FIFOToBuffer(this->_fifoLine16); // fifo - tested by splinter cell chaos theory thermal view
			break;
			
		case 0x41000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
		case 0x61000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			willWriteVRAMLineNative = (isRead3DLineNative && willReadNativeVRAM);
			break;
			
		case 0x43000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
		case 0x63000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			willWriteVRAMLineNative = isRead3DLineNative;
			this->_RenderLine_DispCapture_FIFOToBuffer(this->_fifoLine16); // fifo - tested by splinter cell chaos theory thermal view
			break;
	}
	
	const void *srcAPtr;
	const void *srcBPtr;
	u16 *dstNative16 = this->_VRAMNativeBlockPtr[DISPCAPCNT.VRAMWriteBlock] + dstNativeOffset;
	
	if (!willWriteVRAMLineNative)
	{
		void *dstCustomPtr;
		const size_t captureLengthExt = (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) ? compInfo.line.widthCustom : compInfo.line.widthCustom / 2;
		const GPUEngineLineInfo &lineInfoBlock = this->_currentCompositorInfo[DISPCAPCNT.VRAMWriteOffset * 64].line;
		
		size_t dstCustomOffset = lineInfoBlock.blockOffsetCustom + (compInfo.line.indexCustom * captureLengthExt);
		while (dstCustomOffset >= _gpuVRAMBlockOffset)
		{
			dstCustomOffset -= _gpuVRAMBlockOffset;
		}
		
		const u16 *vramCustom16 = (u16 *)GPU->GetCustomVRAMBlankBuffer();
		const Color4u8 *vramCustom32 = (Color4u8 *)GPU->GetCustomVRAMBlankBuffer();
		
		if (!willReadNativeVRAM)
		{
			size_t vramCustomOffset = (lineInfoBlock.indexCustom + compInfo.line.indexCustom) * compInfo.line.widthCustom;
			while (vramCustomOffset >= _gpuVRAMBlockOffset)
			{
				vramCustomOffset -= _gpuVRAMBlockOffset;
			}
			
			vramCustom16 = (u16 *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + vramCustomOffset;
			vramCustom32 = (Color4u8 *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + vramCustomOffset;
		}
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			if ( (DISPCAPCNT.SrcB == 0) && (DISPCAPCNT.CaptureSrc != 0) && (vramConfiguration.banks[DISPCNT.VRAM_Block].purpose == VramConfiguration::LCDC) )
			{
				if (willReadNativeVRAM)
				{
					ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(vramNative16, (u32 *)vramCustom32, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				}
			}
			
			srcAPtr = (DISPCAPCNT.SrcA == 0) ? (Color4u8 *)compInfo.target.lineColorHead : (Color4u8 *)CurrentRenderer->GetFramebuffer() + compInfo.line.blockOffsetCustom;
			srcBPtr = (DISPCAPCNT.SrcB == 0) ? vramCustom32 : this->_fifoLine32;
			dstCustomPtr = (Color4u8 *)this->_VRAMCustomBlockPtr[DISPCAPCNT.VRAMWriteBlock] + dstCustomOffset;
		}
		else
		{
			const u16 *vramPtr16 = (willReadNativeVRAM) ? vramNative16 : vramCustom16;
			
			srcAPtr = (DISPCAPCNT.SrcA == 0) ? (u16 *)compInfo.target.lineColorHead : this->_3DFramebuffer16 + compInfo.line.blockOffsetCustom;
			srcBPtr = (DISPCAPCNT.SrcB == 0) ? vramPtr16 : this->_fifoLine16;
			dstCustomPtr = (u16 *)this->_VRAMCustomBlockPtr[DISPCAPCNT.VRAMWriteBlock] + dstCustomOffset;
		}
		
		if ( (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) || (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) )
		{
			// Note that although RGB666 colors are 32-bit values, this particular mode uses 16-bit color depth for line captures.
			if ( (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) && needConvertDisplayLine23 )
			{
				ColorspaceConvertBuffer6665To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingDisplay16, compInfo.line.pixelCount);
				srcAPtr = this->_captureWorkingDisplay16;
				needConvertDisplayLine23 = false;
			}
			
			this->_RenderLine_DisplayCaptureCustom<NDSColorFormat_BGR555_Rev, CAPTURELENGTH>(DISPCAPCNT,
																							 compInfo.line,
																							 isReadDisplayLineNative,
																							 (srcBPtr == vramNative16),
																							 srcAPtr,
																							 srcBPtr,
																							 dstCustomPtr);
			
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				CopyLineReduceHinted<0x3FFF, false, false, 2>(compInfo.line, dstCustomPtr, dstNative16);
				needCaptureNative = false;
			}
		}
		else
		{
			this->_RenderLine_DisplayCaptureCustom<OUTPUTFORMAT, CAPTURELENGTH>(DISPCAPCNT,
																				compInfo.line,
																				isReadDisplayLineNative,
																				(srcBPtr == vramNative16),
																				srcAPtr,
																				srcBPtr,
																				dstCustomPtr);
			
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				u32 *dstNative32 = (u32 *)dstCustomPtr;
				
				if (compInfo.line.widthCustom > GPU_FRAMEBUFFER_NATIVE_WIDTH)
				{
					dstNative32 = (u32 *)this->_captureWorkingA32; // We're going to reuse _captureWorkingA32, since we should already be done with it by now.
					CopyLineReduceHinted<0x3FFF, false, false, 4>(compInfo.line, dstCustomPtr, dstNative32);
				}
				
				ColorspaceConvertBuffer8888To5551<false, false>(dstNative32, dstNative16, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				needCaptureNative = false;
			}
		}
	}
	
	if (needCaptureNative)
	{
		srcAPtr = (DISPCAPCNT.SrcA == 0) ? (u16 *)compInfo.target.lineColorHead : this->_3DFramebuffer16 + compInfo.line.blockOffsetCustom;
		srcBPtr = (DISPCAPCNT.SrcB == 0) ? vramNative16 : this->_fifoLine16;
		
		// Convert 18-bit and 24-bit framebuffers to 15-bit for native screen capture.
		if ((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) && needConvertDisplayLine23)
		{
			ColorspaceConvertBuffer6665To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingDisplay16, compInfo.line.pixelCount);
			srcAPtr = this->_captureWorkingDisplay16;
		}
		else if ((OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) && needConvertDisplayLine32)
		{
			ColorspaceConvertBuffer8888To5551<false, false>((u32 *)compInfo.target.lineColorHead, this->_captureWorkingDisplay16, compInfo.line.pixelCount);
			srcAPtr = this->_captureWorkingDisplay16;
		}
		
		switch (captureSrcBits)
		{
			case 0x00000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x02000000: // Display only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			{
				if (isReadDisplayLineNative)
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				else
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				break;
			}
				
			case 0x01000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			case 0x03000000: // 3D only - ((DISPCAPCNT.CaptureSrc == 0) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			{
				if (isRead3DLineNative)
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				else
				{
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, false, true>(compInfo.line, srcAPtr, dstNative16, CAPTURELENGTH);
				}
				break;
			}
				
			case 0x20000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x21000000: // VRAM only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
				this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(compInfo.line, srcBPtr, dstNative16, CAPTURELENGTH);
				break;
				
			case 0x22000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			case 0x23000000: // FIFO only - ((DISPCAPCNT.CaptureSrc == 1) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
				this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(compInfo.line, srcBPtr, dstNative16, CAPTURELENGTH);
				break;
				
			case 0x40000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x41000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			case 0x42000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			case 0x43000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 2) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			case 0x60000000: // Display + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 0))
			case 0x61000000: //      3D + VRAM - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 0))
			case 0x62000000: // Display + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 0) && (DISPCAPCNT.SrcB == 1))
			case 0x63000000: //      3D + FIFO - ((DISPCAPCNT.CaptureSrc == 3) && (DISPCAPCNT.SrcA == 1) && (DISPCAPCNT.SrcB == 1))
			{
				if ( ((DISPCAPCNT.SrcA == 0) && !isReadDisplayLineNative) || ((DISPCAPCNT.SrcA != 0) && !isRead3DLineNative) )
				{
					CopyLineReduceHinted<0x3FFF, false, false, 2>(srcAPtr, 0, CAPTURELENGTH, this->_captureWorkingA16, 0);
					srcAPtr = this->_captureWorkingA16;
				}
				
				this->_RenderLine_DispCapture_Blend<NDSColorFormat_BGR555_Rev, CAPTURELENGTH, true>(compInfo.line, srcAPtr, srcBPtr, dstNative16, CAPTURELENGTH);
				break;
			}
		}
	}
	
	// Save a fresh copy of the current native VRAM buffer so that we have something to compare against
	// in the case where something else decides to make direct changes to VRAM behind our back.
	stream_copy_fast<CAPTURELENGTH*sizeof(u16)>(this->_VRAMNativeBlockCaptureCopyPtr[DISPCAPCNT.VRAMWriteBlock] + dstNativeOffset, dstNative16);
	
	if (this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] && !willWriteVRAMLineNative)
	{
		this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] = false;
		this->_nativeLineCaptureCount[DISPCAPCNT.VRAMWriteBlock]--;
	}
	else if (!this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] && willWriteVRAMLineNative)
	{
		this->_isLineCaptureNative[DISPCAPCNT.VRAMWriteBlock][writeLineIndexWithOffset] = true;
		this->_nativeLineCaptureCount[DISPCAPCNT.VRAMWriteBlock]++;
	}
}

void GPUEngineA::_RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer)
{
	DISP_FIFOrecv_Line16(fifoLineBuffer);
}

template<NDSColorFormat COLORFORMAT, int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
void GPUEngineA::_RenderLine_DispCapture_Copy(const GPUEngineLineInfo &lineInfo, const void *src, void *dst, const size_t captureLengthExt)
{
	const u16 alphaBit16 = (SOURCESWITCH == 0) ? 0x8000 : 0x0000;
	const u32 alphaBit32 = (SOURCESWITCH == 0) ? ((COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF000000 : 0x1F000000) : 0x00000000;
	
	if (CAPTURETONATIVEDST)
	{
		if (CAPTUREFROMNATIVESRC)
		{
#ifdef USEMANUALVECTORIZATION
			buffer_copy_or_constant_s16_fast<CAPTURELENGTH * sizeof(u16), true>(dst, src, alphaBit16);
#else
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
			}
#endif
		}
		else
		{
			for (size_t i = 0; i < CAPTURELENGTH; i++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[_gpuDstPitchIndex[i]] | alphaBit16);
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[_gpuDstPitchIndex[i]] | alphaBit32);
						break;
				}
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
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
							((u16 *)dst)[_gpuDstPitchIndex[i] + p] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							break;
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
							((u32 *)dst)[_gpuDstPitchIndex[i] + p] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							break;
					}
				}
			}
			
			for (size_t l = 1; l < lineInfo.renderCount; l++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memcpy((u16 *)dst + (l * lineInfo.widthCustom), dst, captureLengthExt * sizeof(u16));
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						memcpy((u32 *)dst + (l * lineInfo.widthCustom), dst, captureLengthExt * sizeof(u32));
						break;
				}
			}
		}
		else
		{
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				const size_t pixCountExt = captureLengthExt * lineInfo.renderCount;
				size_t i = 0;
				
#ifdef USEMANUALVECTORIZATION
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
						const size_t vecLength = (pixCountExt * sizeof(u16)) - ((pixCountExt * sizeof(u16)) % VECTORSIZE);
						buffer_copy_or_constant_s16<true>(dst, src, vecLength, alphaBit16);
						i += vecLength / sizeof(u16);
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
						const size_t vecLength = (pixCountExt * sizeof(u32)) - ((pixCountExt * sizeof(u32)) % VECTORSIZE);
						buffer_copy_or_constant_s32<true>(dst, src, vecLength, alphaBit32);
						i += vecLength / sizeof(u32);
						break;
					}
				}
#pragma LOOPVECTORIZE_DISABLE
#endif
				for (; i < pixCountExt; i++)
				{
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
							((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							break;
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
							((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							break;
					}
				}
			}
			else
			{
				for (size_t l = 0; l < lineInfo.renderCount; l++)
				{
					size_t i = 0;
					
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
						{
#ifdef USEMANUALVECTORIZATION
							const size_t vecLength = (captureLengthExt * sizeof(u16)) - ((captureLengthExt * sizeof(u16)) % VECTORSIZE);
							buffer_copy_or_constant_s16<true>(dst, src, vecLength, alphaBit16);
							i += vecLength / sizeof(u16);
#pragma LOOPVECTORIZE_DISABLE
#endif
							for (; i < captureLengthExt; i++)
							{
								((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							}
							
							src = (u16 *)src + lineInfo.widthCustom;
							dst = (u16 *)dst + lineInfo.widthCustom;
							break;
						}
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
						{
#ifdef USEMANUALVECTORIZATION
							const size_t vecLength = (captureLengthExt * sizeof(u32)) - ((captureLengthExt * sizeof(u32)) % VECTORSIZE);
							buffer_copy_or_constant_s32<true>(dst, src, vecLength, alphaBit32);
							i += vecLength / sizeof(u32);
#pragma LOOPVECTORIZE_DISABLE
#endif
							for (; i < captureLengthExt; i++)
							{
								((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							}
							
							src = (u32 *)src + lineInfo.widthCustom;
							dst = (u32 *)dst + lineInfo.widthCustom;
							break;
						}
					}
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
		r =  ((srcA        & 0x001F) * blendEVA);
		g = (((srcA >>  5) & 0x001F) * blendEVA);
		b = (((srcA >> 10) & 0x001F) * blendEVA);
	}
	
	if (b_alpha)
	{
		a = 0x8000;
		r +=  ((srcB        & 0x001F) * blendEVB);
		g += (((srcB >>  5) & 0x001F) * blendEVB);
		b += (((srcB >> 10) & 0x001F) * blendEVB);
	}
	
	r >>= 4;
	g >>= 4;
	b >>= 4;
	
	//freedom wings sky will overflow while doing some fsaa/motionblur effect without this
	r = (r > 31) ? 31 : r;
	g = (g > 31) ? 31 : g;
	b = (b > 31) ? 31 : b;
	
	return LOCAL_TO_LE_16(a | (b << 10) | (g << 5) | r);
}

template<NDSColorFormat COLORFORMAT>
Color4u8 GPUEngineA::_RenderLine_DispCapture_BlendFunc(const Color4u8 srcA, const Color4u8 srcB, const u8 blendEVA, const u8 blendEVB)
{
	Color4u8 outColor;
	outColor.value = 0;
	
	u16 r = 0;
	u16 g = 0;
	u16 b = 0;
	
	if (srcA.a > 0)
	{
		outColor.a  = (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
		r  = srcA.r * blendEVA;
		g  = srcA.g * blendEVA;
		b  = srcA.b * blendEVA;
	}
	
	if (srcB.a > 0)
	{
		outColor.a  = (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
		r += srcB.r * blendEVB;
		g += srcB.g * blendEVB;
		b += srcB.b * blendEVB;
	}
	
	r >>= 4;
	g >>= 4;
	b >>= 4;
	
	//freedom wings sky will overflow while doing some fsaa/motionblur effect without this
	if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		outColor.r = (r > 255) ? 255 : r;
		outColor.g = (g > 255) ? 255 : g;
		outColor.b = (b > 255) ? 255 : b;
	}
	else
	{
		outColor.r = (r > 63) ? 63 : r;
		outColor.g = (g > 63) ? 63 : g;
		outColor.b = (b > 63) ? 63 : b;
	}
	
	return outColor;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_RenderLine_DispCapture_Blend_Buffer(const void *srcA, const void *srcB, void *dst, const u8 blendEVA, const u8 blendEVB, const size_t length)
{
	size_t i = 0;
	
#ifdef USEMANUALVECTORIZATION
	i = this->_RenderLine_DispCapture_Blend_VecLoop<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, length);
#endif
	if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
	{
		const Color4u8 *srcA_32 = (const Color4u8 *)srcA;
		const Color4u8 *srcB_32 = (const Color4u8 *)srcB;
		Color4u8 *dst32 = (Color4u8 *)dst;
		
#ifdef USEMANUALVECTORIZATION
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; i < length; i++)
		{
			const Color4u8 colorA = srcA_32[i];
			const Color4u8 colorB = srcB_32[i];
			
			dst32[i] = this->_RenderLine_DispCapture_BlendFunc<OUTPUTFORMAT>(colorA, colorB, blendEVA, blendEVB);
		}
	}
	else
	{
		const u16 *srcA_16 = (const u16 *)srcA;
		const u16 *srcB_16 = (const u16 *)srcB;
		u16 *dst16 = (u16 *)dst;
		
#ifdef USEMANUALVECTORIZATION
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; i < length; i++)
		{
			const u16 colorA = srcA_16[i];
			const u16 colorB = srcB_16[i];
			
			dst16[i] = this->_RenderLine_DispCapture_BlendFunc(colorA, colorB, blendEVA, blendEVB);
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH, bool ISCAPTURENATIVE>
void GPUEngineA::_RenderLine_DispCapture_Blend(const GPUEngineLineInfo &lineInfo, const void *srcA, const void *srcB, void *dst, const size_t captureLengthExt)
{
	const u8 blendEVA = this->_dispCapCnt.EVA;
	const u8 blendEVB = this->_dispCapCnt.EVB;
	
	if (ISCAPTURENATIVE)
	{
		this->_RenderLine_DispCapture_Blend_Buffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, CAPTURELENGTH);
	}
	else
	{
		if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
		{
			this->_RenderLine_DispCapture_Blend_Buffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt * lineInfo.renderCount);
		}
		else
		{
			for (size_t line = 0; line < lineInfo.renderCount; line++)
			{
				this->_RenderLine_DispCapture_Blend_Buffer<OUTPUTFORMAT>(srcA, srcB, dst, blendEVA, blendEVB, captureLengthExt);
				srcA = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((Color4u8 *)srcA + lineInfo.widthCustom) : (void *)((u16 *)srcA + lineInfo.widthCustom);
				srcB = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((Color4u8 *)srcB + lineInfo.widthCustom) : (void *)((u16 *)srcB + lineInfo.widthCustom);
				dst  = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((Color4u8 *)dst  + lineInfo.widthCustom) : (void *)((u16 *)dst  + lineInfo.widthCustom);
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeVRAM(const GPUEngineLineInfo &lineInfo)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, lineInfo.indexNative);
	
	if (this->_isLineCaptureNative[DISPCNT.VRAM_Block][lineInfo.indexNative])
	{
		CopyLineExpandHinted<1, true, true, true, 2>(lineInfo, this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block], this->_targetDisplay->GetNativeBuffer16());
	}
	else
	{
		void *customBuffer = this->_targetDisplay->GetCustomBuffer();
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				CopyLineExpandHinted<0, true, true, true, 2>(lineInfo, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], customBuffer);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = (u16 *)this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + lineInfo.blockOffsetCustom;
				u32 *dst = (u32 *)customBuffer + lineInfo.blockOffsetCustom;
				ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapSrcDst>(src, dst, lineInfo.pixelCount);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
				CopyLineExpandHinted<0, true, true, true, 4>(lineInfo, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], customBuffer);
				break;
		}
		
		this->_targetDisplay->SetIsLineNative(lineInfo.indexNative, false);
	}
}

void GPUEngineA::_HandleDisplayModeMainMemory(const GPUEngineLineInfo &lineInfo)
{
	// Native rendering only.
	// Displays video using color data directly read from main memory.
	u32 *__restrict dst = (u32 *__restrict)(this->_targetDisplay->GetNativeBuffer16() + (lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH));
	DISP_FIFOrecv_LineOpaque<NDSColorFormat_BGR555_Rev>(dst);
}

template<GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool WILLDEFERCOMPOSITING>
void GPUEngineA::_LineLarge8bpp(GPUEngineCompositorInfo &compInfo)
{
	u16 XBG = compInfo.renderState.selectedBGLayer->xOffset;
	u16 YBG = compInfo.line.indexNative + compInfo.renderState.selectedBGLayer->yOffset;
	u16 lg = compInfo.renderState.selectedBGLayer->size.width;
	u16 ht = compInfo.renderState.selectedBGLayer->size.height;
	u16 wmask = (lg-1);
	u16 hmask = (ht-1);
	YBG &= hmask;
	
	//TODO - handle wrapping / out of bounds correctly from rot_scale_op?
	
	u32 tmp_map = compInfo.renderState.selectedBGLayer->largeBMPAddress + lg * YBG;
	u8 *__restrict map = (u8 *)MMU_gpu_map(tmp_map);
	
	for (size_t x = 0; x < lg; ++x, ++XBG)
	{
		XBG &= wmask;
		
		if (WILLDEFERCOMPOSITING)
		{
			this->_deferredIndexNative[x] = map[XBG];
			this->_deferredColorNative[x] = LE_TO_LOCAL_16(this->_paletteBG[this->_deferredIndexNative[x]]);
		}
		else
		{
			const u8 index = map[XBG];
			const u16 color = LE_TO_LOCAL_16(this->_paletteBG[index]);
			this->_CompositePixelImmediate<COMPOSITORMODE, NDSColorFormat_BGR555_Rev, MOSAIC, WILLPERFORMWINDOWTEST>(compInfo, x, color, (color != 0));
		}
	}
}

void GPUEngineA::LastLineProcess()
{
	this->GPUEngineBase::LastLineProcess();
	DISP_FIFOreset();
}

GPUEngineB::GPUEngineB()
{
	_engineID = GPUEngineID_Sub;
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
	return new(malloc_alignedPage(sizeof(GPUEngineB))) GPUEngineB();
}

void GPUEngineB::FinalizeAndDeallocate()
{
	this->~GPUEngineB();
	free_aligned(this);
}

void GPUEngineB::Reset()
{
	this->SetTargetDisplay( GPU->GetDisplayTouch() );
	this->GPUEngineBase::Reset();
	
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
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineB::RenderLine(const size_t l)
{
	GPUEngineCompositorInfo &compInfo = this->_currentCompositorInfo[l];
	
	if ( this->IsForceBlankSet() )
	{
		this->_RenderLineBlank(l);
	}
	else
	{
		switch (compInfo.renderState.displayOutputMode)
		{
			case GPUDisplayMode_Off: // Display Off (clear line to white)
				this->_HandleDisplayModeOff(l);
				break;
			
			case GPUDisplayMode_Normal: // Display BG and OBJ layers
			{
				if (compInfo.renderState.isAnyWindowEnabled)
				{
					this->_RenderLine_Layers<OUTPUTFORMAT, true>(compInfo);
				}
				else
				{
					this->_RenderLine_Layers<OUTPUTFORMAT, false>(compInfo);
				}
				
				this->_HandleDisplayModeNormal(l);
				break;
			}
				
			default:
				break;
		}
	}
	
	if (compInfo.line.indexNative >= 191)
	{
		this->RenderLineClearAsyncFinish();
	}
}

GPUSubsystem::GPUSubsystem()
{
	ColorspaceHandlerInit();
	PixelOperation::InitLUTs();
	
	_defaultEventHandler = new GPUEventHandlerDefault;
	_event = _defaultEventHandler;
	
	for (size_t line = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		GPUEngineLineInfo &lineInfo = this->_lineInfo[line];
		
		lineInfo.indexNative = line;
		lineInfo.indexCustom = lineInfo.indexNative;
		lineInfo.widthCustom = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.renderCount = 1;
		lineInfo.pixelCount = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetNative = lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetCustom = lineInfo.indexCustom * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	}
	
	if (CommonSettings.num_cores > 1)
	{
		_asyncEngineBufferSetupTask = new Task;
		_asyncEngineBufferSetupTask->start(false, 0, "setup gpu bufs");
	}
	else
	{
		_asyncEngineBufferSetupTask = NULL;
	}
	
	_asyncEngineBufferSetupIsRunning = false;
	
	_pending3DRendererID = RENDERID_NULL;
	_needChange3DRenderer = false;
	
	_videoFrameIndex = 0;
	_render3DFrameCount = 0;
	_frameNeedsFinish = false;
	_willFrameSkip = false;
	_willPostprocessDisplays = true;
	_willAutoResolveToCustomBuffer = true;
	
	//TODO OSD
	//OSDCLASS *previousOSD = osd;
	//osd = new OSDCLASS(-1);
	//delete previousOSD;
	
	_customVRAM = NULL;
	_customVRAMBlank = NULL;
	
	_displayInfo.colorFormat = NDSColorFormat_BGR555_Rev;
	_displayInfo.pixelBytes = sizeof(u16);
	_displayInfo.isCustomSizeRequested = false;
	_displayInfo.customWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.customHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_displayInfo.framebufferPageSize = ((GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT) + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT)) * 2 * _displayInfo.pixelBytes;
	_displayInfo.framebufferPageCount = 1;
	_masterFramebuffer = malloc_alignedPage(_displayInfo.framebufferPageSize * _displayInfo.framebufferPageCount);
	_displayInfo.masterFramebufferHead = _masterFramebuffer;
	
	_masterWorkingNativeBuffer32 = NULL;
	
	_displayInfo.isDisplayEnabled[NDSDisplayID_Main]  = true;
	_displayInfo.isDisplayEnabled[NDSDisplayID_Touch] = true;
	
	_displayInfo.bufferIndex = 0;
	_displayInfo.sequenceNumber = 0;
	_displayInfo.masterNativeBuffer16 = (u16 *)_masterFramebuffer;
	_displayInfo.masterCustomBuffer = (u8 *)_masterFramebuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * sizeof(u16));
	
	_displayInfo.nativeBuffer16[NDSDisplayID_Main]  = _displayInfo.masterNativeBuffer16;
	_displayInfo.nativeBuffer16[NDSDisplayID_Touch] = _displayInfo.masterNativeBuffer16 + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	_displayInfo.customBuffer[NDSDisplayID_Main]  = _displayInfo.masterCustomBuffer;
	_displayInfo.customBuffer[NDSDisplayID_Touch] = (u8 *)_displayInfo.masterCustomBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * _displayInfo.pixelBytes);
	
	_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_displayInfo.renderedBuffer[NDSDisplayID_Main]  = _displayInfo.nativeBuffer16[NDSDisplayID_Main];
	_displayInfo.renderedBuffer[NDSDisplayID_Touch] = _displayInfo.nativeBuffer16[NDSDisplayID_Touch];
	
	_displayInfo.engineID[NDSDisplayID_Main]  = GPUEngineID_Main;
	_displayInfo.engineID[NDSDisplayID_Touch] = GPUEngineID_Sub;
	
	_displayInfo.didPerformCustomRender[NDSDisplayID_Main]  = false;
	_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	
	_displayInfo.masterBrightnessDiffersPerLine[NDSDisplayID_Main]  = false;
	_displayInfo.masterBrightnessDiffersPerLine[NDSDisplayID_Touch] = false;
	memset(_displayInfo.masterBrightnessMode[NDSDisplayID_Main],  GPUMasterBrightMode_Disable, sizeof(_displayInfo.masterBrightnessMode[NDSDisplayID_Main]));
	memset(_displayInfo.masterBrightnessMode[NDSDisplayID_Touch], GPUMasterBrightMode_Disable, sizeof(_displayInfo.masterBrightnessMode[NDSDisplayID_Touch]));
	memset(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Main],  0, sizeof(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Main]));
	memset(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Touch], 0, sizeof(_displayInfo.masterBrightnessIntensity[NDSDisplayID_Touch]));
	
	_displayInfo.backlightIntensity[NDSDisplayID_Main]  = 1.0f;
	_displayInfo.backlightIntensity[NDSDisplayID_Touch] = 1.0f;
	_displayInfo.needConvertColorFormat[NDSDisplayID_Main]  = false;
	_displayInfo.needConvertColorFormat[NDSDisplayID_Touch] = false;
	_displayInfo.needApplyMasterBrightness[NDSDisplayID_Main]  = false;
	_displayInfo.needApplyMasterBrightness[NDSDisplayID_Touch] = false;
	
	ClearWithColor(0x8000);
	
	_engineMain = GPUEngineA::Allocate();
	_engineSub = GPUEngineB::Allocate();
	
	_display[NDSDisplayID_Main]  = new NDSDisplay(NDSDisplayID_Main);
	_display[NDSDisplayID_Touch] = new NDSDisplay(NDSDisplayID_Touch);
	
	_display[NDSDisplayID_Main]->SetEngine(_engineMain);
	_display[NDSDisplayID_Main]->SetEngine(_engineSub);
	
	_display[NDSDisplayID_Main]->SetDrawBuffers(_displayInfo.nativeBuffer16[NDSDisplayID_Main], NULL, _displayInfo.customBuffer[NDSDisplayID_Main]);
	_display[NDSDisplayID_Touch]->SetDrawBuffers(_displayInfo.nativeBuffer16[NDSDisplayID_Touch], NULL, _displayInfo.customBuffer[NDSDisplayID_Touch]);
	
	gfx3d_init();
}

GPUSubsystem::~GPUSubsystem()
{
	//TODO OSD
	//delete osd;
	//osd = NULL;
	
	if (this->_asyncEngineBufferSetupTask != NULL)
	{
		this->AsyncSetupEngineBuffersFinish();
		delete this->_asyncEngineBufferSetupTask;
		this->_asyncEngineBufferSetupTask = NULL;
	}
	
	free_aligned(this->_masterFramebuffer);
	free_aligned(this->_masterWorkingNativeBuffer32);
	free_aligned(this->_customVRAM);
	
	free_aligned(_gpuDstToSrcIndex);
	_gpuDstToSrcIndex = NULL;
	
	free_aligned(_gpuDstToSrcSSSE3_u8_8e);
	_gpuDstToSrcSSSE3_u8_8e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u8_16e);
	_gpuDstToSrcSSSE3_u8_16e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u16_8e);
	_gpuDstToSrcSSSE3_u16_8e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u32_4e);
	_gpuDstToSrcSSSE3_u32_4e = NULL;
	
	delete _display[NDSDisplayID_Main];
	delete _display[NDSDisplayID_Touch];
	_engineMain->FinalizeAndDeallocate();
	_engineSub->FinalizeAndDeallocate();
	
	gfx3d_deinit();
	
	delete _defaultEventHandler;
}

void GPUSubsystem::_UpdateFPSRender3D()
{
	this->_videoFrameIndex++;
	if (this->_videoFrameIndex == 60)
	{
		this->_render3DFrameCount = GFX3D_GetRender3DFrameCount();
		GFX3D_ResetRender3DFrameCount();
		this->_videoFrameIndex = 0;
	}
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
	this->_engineMain->RenderLineClearAsyncFinish();
	this->_engineSub->RenderLineClearAsyncFinish();
	this->AsyncSetupEngineBuffersFinish();
	
	if (this->_customVRAM == NULL)
	{
		this->SetCustomFramebufferSize(this->_displayInfo.customWidth, this->_displayInfo.customHeight);
	}
	
	this->_willFrameSkip = false;
	this->_videoFrameIndex = 0;
	this->_render3DFrameCount = 0;
	
	this->ClearWithColor(0xFFFF);
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main]  = false;
	this->_displayInfo.nativeBuffer16[NDSDisplayID_Main]  = this->_displayInfo.masterNativeBuffer16;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = this->_displayInfo.nativeBuffer16[NDSDisplayID_Main];
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = false;
	this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch] = this->_displayInfo.masterNativeBuffer16 + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterCustomBuffer + (this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes);
	this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch];
	
	this->_displayInfo.engineID[NDSDisplayID_Main] = GPUEngineID_Main;
	this->_displayInfo.engineID[NDSDisplayID_Touch] = GPUEngineID_Sub;
	
	this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = 1.0f;
	this->_displayInfo.backlightIntensity[NDSDisplayID_Touch] = 1.0f;
	
	this->_display[NDSDisplayID_Main]->SetEngineByID(GPUEngineID_Main);
	this->_display[NDSDisplayID_Touch]->SetEngineByID(GPUEngineID_Sub);
	
	gfx3d_reset();
	
	this->_display[NDSDisplayID_Main]->SetBacklightIntensityTotal(0.0f);
	this->_display[NDSDisplayID_Touch]->SetBacklightIntensityTotal(0.0f);
	
	this->_display[NDSDisplayID_Main]->ClearAllLinesToNative();
 	this->_display[NDSDisplayID_Touch]->ClearAllLinesToNative();
	
	this->_engineMain->Reset();
	this->_engineSub->Reset();
	
	DISP_FIFOreset();

	//historically, we reset the OSD here. maybe because we would want a clean drawing surface? anyway this is not the right point to be doing OSD work
	//osd->clear();
}

void GPUSubsystem::ForceRender3DFinishAndFlush(bool willFlush)
{
	CurrentRenderer->RenderFinish();
	CurrentRenderer->RenderFlush(willFlush, willFlush);
}

void GPUSubsystem::ForceFrameStop()
{
	if (CurrentRenderer->GetRenderNeedsFinish())
	{
		this->ForceRender3DFinishAndFlush(true);
		CurrentRenderer->SetRenderNeedsFinish(false);
		this->_event->DidRender3DEnd();
	}
	
	if (this->_frameNeedsFinish)
	{
		this->_frameNeedsFinish = false;
		this->_displayInfo.sequenceNumber++;
		this->_event->DidFrameEnd(this->_willFrameSkip, this->_displayInfo);
	}
}

bool GPUSubsystem::GetWillFrameSkip() const
{
	return this->_willFrameSkip;
}

void GPUSubsystem::SetWillFrameSkip(const bool willFrameSkip)
{
	this->_willFrameSkip = willFrameSkip;
}

void GPUSubsystem::SetDisplayCaptureEnable()
{
	this->_engineMain->SetDisplayCaptureEnable();
}

void GPUSubsystem::ResetDisplayCaptureEnable()
{
	this->_engineMain->ResetDisplayCaptureEnable();
}

void GPUSubsystem::UpdateRenderProperties()
{
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16);
	const size_t customFramebufferSize = this->_displayInfo.customWidth * this->_displayInfo.customHeight * this->_displayInfo.pixelBytes;
	
	this->_displayInfo.masterNativeBuffer16 = (u16 *)((u8 *)this->_masterFramebuffer + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize));
	this->_displayInfo.masterCustomBuffer = (u8 *)this->_masterFramebuffer + (nativeFramebufferSize * 2) + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize);
	this->_displayInfo.nativeBuffer16[NDSDisplayID_Main]  = this->_displayInfo.masterNativeBuffer16;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch] = (u16 *)((u8 *)this->_displayInfo.masterNativeBuffer16 + nativeFramebufferSize);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterCustomBuffer + customFramebufferSize;
	
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = this->_displayInfo.nativeBuffer16[NDSDisplayID_Main];
	this->_displayInfo.renderedWidth[NDSDisplayID_Main]   = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch];
	this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->DidPerformCustomRender();
	this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->DidPerformCustomRender();
	
	this->_display[NDSDisplayID_Main]->SetDrawBuffers(this->_displayInfo.nativeBuffer16[NDSDisplayID_Main], this->_display[NDSDisplayID_Main]->GetWorkingNativeBuffer32(), this->_displayInfo.customBuffer[NDSDisplayID_Main]);
	this->_display[NDSDisplayID_Touch]->SetDrawBuffers(this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch], this->_display[NDSDisplayID_Touch]->GetWorkingNativeBuffer32(), this->_displayInfo.customBuffer[NDSDisplayID_Touch]);
	this->_engineMain->SetupRenderStates();
	this->_engineSub->SetupRenderStates();
	
	if (!this->_displayInfo.isCustomSizeRequested && (this->_displayInfo.colorFormat != NDSColorFormat_BGR888_Rev))
	{
		return;
	}
	
	// Iterate through VRAM banks A-D and determine if they will be used for this frame.
	for (size_t i = 0; i < 4; i++)
	{
		switch (vramConfiguration.banks[i].purpose)
		{
			case VramConfiguration::ABG:
			case VramConfiguration::BBG:
			case VramConfiguration::LCDC:
			case VramConfiguration::AOBJ:
			case VramConfiguration::BOBJ:
				break;
				
			default:
				this->_engineMain->ResetCaptureLineStates(i);
				break;
		}
	}
}

const NDSDisplayInfo& GPUSubsystem::GetDisplayInfo()
{
	return this->_displayInfo;
}

const GPUEngineLineInfo& GPUSubsystem::GetLineInfoAtIndex(size_t l)
{
	return this->_lineInfo[l];
}

u32 GPUSubsystem::GetFPSRender3D() const
{
	return this->_render3DFrameCount;
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
	return this->_display[NDSDisplayID_Main];
}

NDSDisplay* GPUSubsystem::GetDisplayTouch()
{
	return this->_display[NDSDisplayID_Touch];
}

size_t GPUSubsystem::GetFramebufferPageCount() const
{
	return this->_displayInfo.framebufferPageCount;
}

void GPUSubsystem::SetFramebufferPageCount(size_t pageCount)
{
	if (pageCount > MAX_FRAMEBUFFER_PAGES)
	{
		pageCount = MAX_FRAMEBUFFER_PAGES;
	}
	
	this->_displayInfo.framebufferPageCount = (u32)pageCount;
}

size_t GPUSubsystem::GetCustomFramebufferWidth() const
{
	return this->_displayInfo.customWidth;
}

size_t GPUSubsystem::GetCustomFramebufferHeight() const
{
	return this->_displayInfo.customHeight;
}

void GPUSubsystem::SetCustomFramebufferSize(size_t w, size_t h)
{
	if (w < GPU_FRAMEBUFFER_NATIVE_WIDTH || h < GPU_FRAMEBUFFER_NATIVE_HEIGHT)
	{
		return;
	}
	
	this->_engineMain->RenderLineClearAsyncFinish();
	this->_engineSub->RenderLineClearAsyncFinish();
	this->AsyncSetupEngineBuffersFinish();
	
	const float customWidthScale = (float)w / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const float customHeightScale = (float)h / (float)GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	const float newGpuLargestDstLineCount = (size_t)ceilf(customHeightScale);
	
	u16 *oldGpuDstToSrcIndexPtr = _gpuDstToSrcIndex;
	u8 *oldGpuDstToSrcSSSE3_u8_8e = _gpuDstToSrcSSSE3_u8_8e;
	u8 *oldGpuDstToSrcSSSE3_u8_16e = _gpuDstToSrcSSSE3_u8_16e;
	u8 *oldGpuDstToSrcSSSE3_u16_8e = _gpuDstToSrcSSSE3_u16_8e;
	u8 *oldGpuDstToSrcSSSE3_u32_4e = _gpuDstToSrcSSSE3_u32_4e;
	
	for (u32 srcX = 0, currentPitchCount = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; srcX++)
	{
		const u32 pitch = (u32)ceilf(((float)srcX+1.0f) * customWidthScale) - (float)currentPitchCount;
		_gpuDstPitchCount[srcX] = pitch;
		_gpuDstPitchIndex[srcX] = currentPitchCount;
		currentPitchCount += pitch;
	}
	
	for (size_t line = 0, currentLineCount = 0; line < GPU_VRAM_BLOCK_LINES + 1; line++)
	{
		const size_t lineCount = (size_t)ceilf((line+1) * customHeightScale) - currentLineCount;
		GPUEngineLineInfo &lineInfo = this->_lineInfo[line];
		
		lineInfo.indexNative = line;
		lineInfo.indexCustom = currentLineCount;
		lineInfo.widthCustom = w;
		lineInfo.renderCount = lineCount;
		lineInfo.pixelCount = lineInfo.widthCustom * lineInfo.renderCount;
		lineInfo.blockOffsetNative = lineInfo.indexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
		lineInfo.blockOffsetCustom = lineInfo.indexCustom * lineInfo.widthCustom;
		
		currentLineCount += lineCount;
	}
	
	u16 *newGpuDstToSrcIndex = (u16 *)malloc_alignedPage(w * h * sizeof(u16));
	u16 *newGpuDstToSrcPtr = newGpuDstToSrcIndex;
	for (size_t y = 0, dstIdx = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
	{
		if (this->_lineInfo[y].renderCount < 1)
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
		
		for (size_t l = 1; l < this->_lineInfo[y].renderCount; l++)
		{
			memcpy(newGpuDstToSrcPtr + (w * l), newGpuDstToSrcPtr, w * sizeof(u16));
		}
		
		newGpuDstToSrcPtr += (w * this->_lineInfo[y].renderCount);
		dstIdx += (w * (this->_lineInfo[y].renderCount - 1));
	}
	
	u8 *newGpuDstToSrcSSSE3_u8_8e = (u8 *)malloc_alignedPage(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u8_16e = (u8 *)malloc_alignedPage(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u16_8e = (u8 *)malloc_alignedPage(w * sizeof(u16));
	u8 *newGpuDstToSrcSSSE3_u32_4e = (u8 *)malloc_alignedPage(w * sizeof(u32));
	
	for (size_t i = 0; i < w; i++)
	{
		const u8 value_u8_4 = newGpuDstToSrcIndex[i] & 0x03;
		const u8 value_u8_8 = newGpuDstToSrcIndex[i] & 0x07;
		const u8 value_u8_16 = newGpuDstToSrcIndex[i] & 0x0F;
		const u8 value_u16 = (value_u8_8 << 1);
		const u8 value_u32 = (value_u8_4 << 2);
		
		newGpuDstToSrcSSSE3_u8_8e[i] = value_u8_8;
		newGpuDstToSrcSSSE3_u8_16e[i] = value_u8_16;
		
		newGpuDstToSrcSSSE3_u16_8e[(i << 1) + 0] = value_u16 + 0;
		newGpuDstToSrcSSSE3_u16_8e[(i << 1) + 1] = value_u16 + 1;
		
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 0] = value_u32 + 0;
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 1] = value_u32 + 1;
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 2] = value_u32 + 2;
		newGpuDstToSrcSSSE3_u32_4e[(i << 2) + 3] = value_u32 + 3;
	}
	
	_gpuLargestDstLineCount = newGpuLargestDstLineCount;
	_gpuVRAMBlockOffset = this->_lineInfo[GPU_VRAM_BLOCK_LINES].indexCustom * w;
	_gpuDstToSrcIndex = newGpuDstToSrcIndex;
	_gpuDstToSrcSSSE3_u8_8e = newGpuDstToSrcSSSE3_u8_8e;
	_gpuDstToSrcSSSE3_u8_16e = newGpuDstToSrcSSSE3_u8_16e;
	_gpuDstToSrcSSSE3_u16_8e = newGpuDstToSrcSSSE3_u16_8e;
	_gpuDstToSrcSSSE3_u32_4e = newGpuDstToSrcSSSE3_u32_4e;
	
	CurrentRenderer->RenderFinish();
	CurrentRenderer->SetRenderNeedsFinish(false);
	
	this->_display[NDSDisplayID_Main]->SetDisplaySize(w, h);
	this->_display[NDSDisplayID_Touch]->SetDisplaySize(w, h);
	
	this->_displayInfo.isCustomSizeRequested = ( (w != GPU_FRAMEBUFFER_NATIVE_WIDTH) || (h != GPU_FRAMEBUFFER_NATIVE_HEIGHT) );
	this->_displayInfo.customWidth  = (u32)w;
	this->_displayInfo.customHeight = (u32)h;
	
	if (!this->_display[NDSDisplayID_Main]->IsCustomSizeRequested())
	{
		this->_engineMain->ResetCaptureLineStates(0);
		this->_engineMain->ResetCaptureLineStates(1);
		this->_engineMain->ResetCaptureLineStates(2);
		this->_engineMain->ResetCaptureLineStates(3);
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, w, h, this->_displayInfo.framebufferPageCount);
	
	free_aligned(oldGpuDstToSrcIndexPtr);
	free_aligned(oldGpuDstToSrcSSSE3_u8_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u8_16e);
	free_aligned(oldGpuDstToSrcSSSE3_u16_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u32_4e);
}

NDSColorFormat GPUSubsystem::GetColorFormat() const
{
	return this->_displayInfo.colorFormat;
}

void GPUSubsystem::SetColorFormat(const NDSColorFormat outputFormat)
{
	if (this->_displayInfo.colorFormat == outputFormat)
	{
		return;
	}
	
	this->_engineMain->RenderLineClearAsyncFinish();
	this->_engineSub->RenderLineClearAsyncFinish();
	this->AsyncSetupEngineBuffersFinish();
	
	CurrentRenderer->RenderFinish();
	CurrentRenderer->SetRenderNeedsFinish(false);
	
	this->_display[NDSDisplayID_Main]->SetColorFormat(outputFormat);
	this->_display[NDSDisplayID_Touch]->SetColorFormat(outputFormat);
	
	this->_displayInfo.colorFormat = this->_display[NDSDisplayID_Main]->GetColorFormat();
	this->_displayInfo.pixelBytes  = (u32)this->_display[NDSDisplayID_Main]->GetPixelBytes();
	
	if (!this->_displayInfo.isCustomSizeRequested)
	{
		this->_engineMain->ResetCaptureLineStates(0);
		this->_engineMain->ResetCaptureLineStates(1);
		this->_engineMain->ResetCaptureLineStates(2);
		this->_engineMain->ResetCaptureLineStates(3);
	}
	
	this->_AllocateFramebuffers(this->_displayInfo.colorFormat, this->_displayInfo.customWidth, this->_displayInfo.customHeight, this->_displayInfo.framebufferPageCount);
}

void GPUSubsystem::_AllocateFramebuffers(NDSColorFormat outputFormat, size_t w, size_t h, size_t pageCount)
{
	void *oldMasterFramebuffer = this->_masterFramebuffer;
	void *oldCustomVRAM = this->_customVRAM;
	
	const size_t pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(Color4u8);
	const size_t newCustomVRAMBlockSize = this->_lineInfo[GPU_VRAM_BLOCK_LINES].indexCustom * w;
	const size_t newCustomVRAMBlankSize = _gpuLargestDstLineCount * GPU_VRAM_BLANK_REGION_LINES * w;
	const size_t nativeFramebufferSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16);
	const size_t customFramebufferSize = w * h * pixelBytes;
	
	void *newCustomVRAM = NULL;
	
	this->_displayInfo.framebufferPageCount = (u32)pageCount;
	this->_displayInfo.framebufferPageSize  = (u32)( (nativeFramebufferSize * 2) + (customFramebufferSize * 2) );
	this->_masterFramebuffer = malloc_alignedPage(this->_displayInfo.framebufferPageSize * this->_displayInfo.framebufferPageCount);
	
	if (outputFormat != NDSColorFormat_BGR555_Rev)
	{
		if (this->_masterWorkingNativeBuffer32 == NULL)
		{
			this->_masterWorkingNativeBuffer32 = (u32 *)malloc_alignedPage(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * sizeof(u32));
		}
	}
	else
	{
		free_aligned(this->_masterWorkingNativeBuffer32);
		this->_masterWorkingNativeBuffer32 = NULL;
	}
	
	this->_displayInfo.masterFramebufferHead = this->_masterFramebuffer;
	this->_displayInfo.masterNativeBuffer16 = (u16 *)((u8 *)this->_masterFramebuffer + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize));
	this->_displayInfo.masterCustomBuffer   = (u8 *)this->_masterFramebuffer + (nativeFramebufferSize * 2) + (this->_displayInfo.bufferIndex * this->_displayInfo.framebufferPageSize);
	
	this->_displayInfo.nativeBuffer16[NDSDisplayID_Main]  = this->_displayInfo.masterNativeBuffer16;
	this->_displayInfo.customBuffer[NDSDisplayID_Main]    = this->_displayInfo.masterCustomBuffer;
	this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch] = (u16 *)((u8 *)this->_displayInfo.masterNativeBuffer16 + nativeFramebufferSize);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_displayInfo.masterCustomBuffer + customFramebufferSize;
	
	this->ClearWithColor(0x8000);
	
	if (this->_display[NDSDisplayID_Main]->DidPerformCustomRender())
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_displayInfo.customBuffer[NDSDisplayID_Main];
		this->_displayInfo.renderedWidth[NDSDisplayID_Main]  = this->_displayInfo.customWidth;
		this->_displayInfo.renderedHeight[NDSDisplayID_Main] = this->_displayInfo.customHeight;
	}
	else
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_displayInfo.nativeBuffer16[NDSDisplayID_Main];
		this->_displayInfo.renderedWidth[NDSDisplayID_Main]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_displayInfo.renderedHeight[NDSDisplayID_Main] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	if (this->_display[NDSDisplayID_Touch]->DidPerformCustomRender())
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.customBuffer[NDSDisplayID_Touch];
		this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = this->_displayInfo.customWidth;
		this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = this->_displayInfo.customHeight;
	}
	else
	{
		this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch];
		this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
		this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	}
	
	switch (outputFormat)
	{
		case NDSColorFormat_BGR555_Rev:
			newCustomVRAM = (void *)malloc_alignedPage(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (u16 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			newCustomVRAM = (void *)malloc_alignedPage(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(u16));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (u16 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			newCustomVRAM = (void *)malloc_alignedPage(((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(Color4u8));
			memset(newCustomVRAM, 0, ((newCustomVRAMBlockSize * 4) + newCustomVRAMBlankSize) * sizeof(Color4u8));
			this->_customVRAM = newCustomVRAM;
			this->_customVRAMBlank = (Color4u8 *)newCustomVRAM + (newCustomVRAMBlockSize * 4);
			break;
			
		default:
			break;
	}
	
	this->_display[NDSDisplayID_Main]->SetDrawBuffers(this->_displayInfo.nativeBuffer16[NDSDisplayID_Main], this->_masterWorkingNativeBuffer32, this->_displayInfo.customBuffer[NDSDisplayID_Main]);
	this->_display[NDSDisplayID_Touch]->SetDrawBuffers(this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch], this->_masterWorkingNativeBuffer32 + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT), this->_displayInfo.customBuffer[NDSDisplayID_Touch]);
	
	this->_engineMain->AllocateWorkingBuffers(outputFormat, w, h);
	this->_engineSub->AllocateWorkingBuffers(outputFormat, w, h);
	
	BaseRenderer->SetFramebufferSize(w, h); // Since BaseRenderer is persistent, we need to update this manually.
	if (CurrentRenderer != BaseRenderer)
	{
		CurrentRenderer->RequestColorFormat(outputFormat);
		CurrentRenderer->SetFramebufferSize(w, h);
	}
	
	free_aligned(oldMasterFramebuffer);
	free_aligned(oldCustomVRAM);
}

int GPUSubsystem::Get3DRendererID()
{
	return this->_pending3DRendererID;
}

void GPUSubsystem::Set3DRendererByID(int rendererID)
{
	Render3DInterface *newRenderInterface = core3DList[rendererID];
	if ( (newRenderInterface == NULL) || (newRenderInterface->NDS_3D_Init == NULL) )
	{
		return;
	}
	
	this->_pending3DRendererID = rendererID;
	this->_needChange3DRenderer = true;
}

bool GPUSubsystem::Change3DRendererByID(int rendererID)
{
	bool result = false;
	
	// Whether pass or fail, the 3D renderer will have only one chance to be
	// lazily changed via the flag set by Set3DRendererByID().
	this->_needChange3DRenderer = false;
	
	Render3DInterface *newRenderInterface = core3DList[rendererID];
	if ( (newRenderInterface == NULL) || (newRenderInterface->NDS_3D_Init == NULL) )
	{
		return result;
	}
	
	// Some resources are shared between renderers, such as the texture cache,
	// so we need to shut down the current renderer now to ensure that any
	// shared resources aren't in use.
	const bool didRenderBegin = CurrentRenderer->GetRenderNeedsFinish();
	CurrentRenderer->RenderFinish();
	gpu3D->NDS_3D_Close();
	gpu3D = &gpu3DNull;
	cur3DCore = RENDERID_NULL;
	BaseRenderer->SetRenderNeedsFinish(didRenderBegin);
	CurrentRenderer = BaseRenderer;
	
	Render3D *newRenderer = newRenderInterface->NDS_3D_Init();
	if (newRenderer == NULL)
	{
		return result;
	}
	
	newRenderer->RequestColorFormat(this->_displayInfo.colorFormat);
	
	Render3DError error = newRenderer->SetFramebufferSize(this->_displayInfo.customWidth, this->_displayInfo.customHeight);
	if (error != RENDER3DERROR_NOERR)
	{
		newRenderInterface->NDS_3D_Close();
		printf("GPU: 3D framebuffer resize error. 3D rendering will be disabled for this renderer. (Error code = %d)\n", (int)error);
		return result;
	}
	
	gpu3D = newRenderInterface;
	cur3DCore = rendererID;
	newRenderer->SetRenderNeedsFinish( BaseRenderer->GetRenderNeedsFinish() );
	CurrentRenderer = newRenderer;
	
	result = true;
	return result;
}

bool GPUSubsystem::Change3DRendererIfNeeded()
{
	if (!this->_needChange3DRenderer)
	{
		return true;
	}
	
	return this->Change3DRendererByID(this->_pending3DRendererID);
}

void* GPUSubsystem::GetCustomVRAMBuffer()
{
	return this->_customVRAM;
}

void* GPUSubsystem::GetCustomVRAMBlankBuffer()
{
	return this->_customVRAMBlank;
}

template <NDSColorFormat COLORFORMAT>
void* GPUSubsystem::GetCustomVRAMAddressUsingMappedAddress(const u32 mappedAddr, const size_t offset)
{
	const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(mappedAddr) - MMU.ARM9_LCD) / sizeof(u16);
	if (vramPixel >= (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
	{
		return this->_customVRAMBlank;
	}
	
	const size_t blockID   = vramPixel >> 16;				// blockID   = vramPixel / (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES)
	const size_t blockLine = (vramPixel >> 8) & 0x000000FF;	// blockLine = (vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES)) / GPU_FRAMEBUFFER_NATIVE_WIDTH
	const size_t linePixel = vramPixel & 0x000000FF;		// linePixel = (vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES)) % GPU_FRAMEBUFFER_NATIVE_WIDTH
	
	return (COLORFORMAT == NDSColorFormat_BGR888_Rev) ? (void *)((Color4u8 *)this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (this->_lineInfo[blockLine].indexCustom * this->_lineInfo[blockLine].widthCustom) + _gpuDstPitchIndex[linePixel] + offset) : (void *)((u16 *)this->GetEngineMain()->GetCustomVRAMBlockPtr(blockID) + (this->_lineInfo[blockLine].indexCustom * this->_lineInfo[blockLine].widthCustom) + _gpuDstPitchIndex[linePixel] + offset);
}

bool GPUSubsystem::GetWillPostprocessDisplays() const
{
	return this->_willPostprocessDisplays;
}

void GPUSubsystem::SetWillPostprocessDisplays(const bool willPostprocess)
{
	this->_willPostprocessDisplays = willPostprocess;
}

void GPUSubsystem::PostprocessDisplay(const NDSDisplayID displayID, NDSDisplayInfo &mutableInfo)
{
	this->_display[displayID]->Postprocess(mutableInfo);
}

void GPUSubsystem::ResolveDisplayToCustomFramebuffer(const NDSDisplayID displayID, NDSDisplayInfo &mutableInfo)
{
	this->_display[displayID]->ResolveFramebufferToCustom(mutableInfo);
}

bool GPUSubsystem::GetWillAutoResolveToCustomBuffer() const
{
	return this->_willAutoResolveToCustomBuffer;
}

void GPUSubsystem::SetWillAutoResolveToCustomBuffer(const bool willAutoResolve)
{
	this->_willAutoResolveToCustomBuffer = willAutoResolve;
}

void GPUSubsystem::SetupEngineBuffers()
{
	this->_engineMain->SetupBuffers();
	this->_engineSub->SetupBuffers();
}

void* GPUSubsystem_AsyncSetupEngineBuffers(void *arg)
{
	GPUSubsystem *gpuSubystem = (GPUSubsystem *)arg;
	gpuSubystem->SetupEngineBuffers();
	
	return NULL;
}

void GPUSubsystem::AsyncSetupEngineBuffersStart()
{
	if (this->_asyncEngineBufferSetupTask == NULL)
	{
		return;
	}
	
	this->AsyncSetupEngineBuffersFinish();
	this->_asyncEngineBufferSetupTask->execute(&GPUSubsystem_AsyncSetupEngineBuffers, this);
	this->_asyncEngineBufferSetupIsRunning = true;
}

void GPUSubsystem::AsyncSetupEngineBuffersFinish()
{
	if (!this->_asyncEngineBufferSetupIsRunning)
	{
		return;
	}
	
	this->_asyncEngineBufferSetupTask->finish();
	this->_asyncEngineBufferSetupIsRunning = false;
}

void GPUSubsystem::RenderLine(const size_t l)
{
	if (!this->_frameNeedsFinish)
	{
		this->_event->DidApplyGPUSettingsBegin();
		this->_engineMain->ApplySettings();
		this->_engineSub->ApplySettings();
		this->_event->DidApplyGPUSettingsEnd();
		
		this->_display[NDSDisplayID_Main]->SetIsEnabled( this->_display[NDSDisplayID_Main]->GetEngine()->GetEnableStateApplied() );
		this->_display[NDSDisplayID_Touch]->SetIsEnabled( this->_display[NDSDisplayID_Touch]->GetEngine()->GetEnableStateApplied() );
		this->_displayInfo.isDisplayEnabled[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->IsEnabled();
		this->_displayInfo.isDisplayEnabled[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->IsEnabled();
		
		this->_event->DidFrameBegin(l, this->_willFrameSkip, this->_displayInfo.framebufferPageCount, this->_displayInfo.bufferIndex);
		this->_frameNeedsFinish = true;
	}
	
	const bool isDisplayCaptureNeeded = this->_engineMain->WillDisplayCapture(l);
	const bool isFramebufferRenderNeeded[2]	= {
		this->_engineMain->GetEnableStateApplied(),
		this->_engineSub->GetEnableStateApplied()
	};
	
	if (l == 0)
	{
		if (!this->_willFrameSkip)
		{
			if (this->_asyncEngineBufferSetupIsRunning)
			{
				this->AsyncSetupEngineBuffersFinish();
			}
			else
			{
				this->SetupEngineBuffers();
			}
			
			this->_display[NDSDisplayID_Main]->ClearAllLinesToNative();
 			this->_display[NDSDisplayID_Touch]->ClearAllLinesToNative();
			this->UpdateRenderProperties();
		}
	}
	
	if (!this->_willFrameSkip)
	{
		this->_engineMain->UpdateRenderStates(l);
		this->_engineSub->UpdateRenderStates(l);
	}
	
	if ( (isFramebufferRenderNeeded[GPUEngineID_Main] || this->_engineMain->IsForceBlankSet() || isDisplayCaptureNeeded) && !this->_willFrameSkip )
	{
		// GPUEngineA:WillRender3DLayer() and GPUEngineA:WillCapture3DLayerDirect() both rely on register
		// states that might change on a per-line basis. Therefore, we need to check these states on a
		// per-line basis as well. While most games will set up these states by line 0 and keep these
		// states constant all the way through line 191, this may not always be the case.
		//
		// Test case: If a conversation occurs in Advance Wars: Dual Strike where the conversation
		// originates from the top of the screen, the BG0 layer will only be enabled at line 46. This
		// means that we need to check the states at that particular time to ensure that the 3D renderer
		// finishes before we read the 3D framebuffer. Otherwise, the map will render incorrectly.
		
		const bool need3DCaptureFramebuffer = this->_engineMain->WillCapture3DLayerDirect(l);
		const bool need3DDisplayFramebuffer = this->_engineMain->WillRender3DLayer() || ((this->_engineMain->GetTargetDisplay()->GetColorFormat() == NDSColorFormat_BGR888_Rev) && need3DCaptureFramebuffer);
		
		if (need3DCaptureFramebuffer || need3DDisplayFramebuffer)
		{
			if (CurrentRenderer->GetRenderNeedsFinish())
			{
				CurrentRenderer->RenderFinish();
				CurrentRenderer->SetRenderNeedsFinish(false);
				this->_event->DidRender3DEnd();
			}
			
			CurrentRenderer->RenderFlush(need3DDisplayFramebuffer && CurrentRenderer->GetRenderNeedsFlushMain(),
			                             need3DCaptureFramebuffer && CurrentRenderer->GetRenderNeedsFlush16());
		}
		
		switch (this->_engineMain->GetTargetDisplay()->GetColorFormat())
		{
			case NDSColorFormat_BGR555_Rev:
				this->_engineMain->RenderLine<NDSColorFormat_BGR555_Rev>(l);
				break;
				
			case NDSColorFormat_BGR666_Rev:
				this->_engineMain->RenderLine<NDSColorFormat_BGR666_Rev>(l);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				this->_engineMain->RenderLine<NDSColorFormat_BGR888_Rev>(l);
				break;
		}
	}
	else
	{
		this->_engineMain->UpdatePropertiesWithoutRender(l);
	}
	
	if ( (isFramebufferRenderNeeded[GPUEngineID_Sub] || this->_engineSub->IsForceBlankSet()) && !this->_willFrameSkip)
	{
		switch (this->_engineSub->GetTargetDisplay()->GetColorFormat())
		{
			case NDSColorFormat_BGR555_Rev:
				this->_engineSub->RenderLine<NDSColorFormat_BGR555_Rev>(l);
				break;
				
			case NDSColorFormat_BGR666_Rev:
				this->_engineSub->RenderLine<NDSColorFormat_BGR666_Rev>(l);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				this->_engineSub->RenderLine<NDSColorFormat_BGR888_Rev>(l);
				break;
		}
	}
	else
	{
		this->_engineSub->UpdatePropertiesWithoutRender(l);
	}
	
	if (l == 191)
	{
		this->_engineMain->LastLineProcess();
		this->_engineSub->LastLineProcess();
		
		this->_UpdateFPSRender3D();
		
		if (!this->_willFrameSkip)
		{
			this->_display[NDSDisplayID_Main]->ResolveLinesDisplayedNative();
			this->_display[NDSDisplayID_Touch]->ResolveLinesDisplayedNative();
			
			this->_engineMain->TransitionRenderStatesToDisplayInfo(this->_displayInfo);
			this->_engineSub->TransitionRenderStatesToDisplayInfo(this->_displayInfo);
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->DidPerformCustomRender();
			this->_displayInfo.renderedBuffer[NDSDisplayID_Main] = this->_display[NDSDisplayID_Main]->GetRenderedBuffer();
			this->_displayInfo.renderedWidth[NDSDisplayID_Main]  = (u32)this->_display[NDSDisplayID_Main]->GetRenderedWidth();
			this->_displayInfo.renderedHeight[NDSDisplayID_Main] = (u32)this->_display[NDSDisplayID_Main]->GetRenderedHeight();
			
			this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->DidPerformCustomRender();
			this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetRenderedBuffer();
			this->_displayInfo.renderedWidth[NDSDisplayID_Touch]  = (u32)this->_display[NDSDisplayID_Touch]->GetRenderedWidth();
			this->_displayInfo.renderedHeight[NDSDisplayID_Touch] = (u32)this->_display[NDSDisplayID_Touch]->GetRenderedHeight();
			
			this->_displayInfo.engineID[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->GetEngineID();
			this->_displayInfo.engineID[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetEngineID();
			
			this->_displayInfo.needConvertColorFormat[NDSDisplayID_Main]  = (this->_display[NDSDisplayID_Main]->GetColorFormat()  == NDSColorFormat_BGR666_Rev);
			this->_displayInfo.needConvertColorFormat[NDSDisplayID_Touch] = (this->_display[NDSDisplayID_Touch]->GetColorFormat() == NDSColorFormat_BGR666_Rev);
			
			// Set the average backlight intensity over 263 H-blanks.
			this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->GetBacklightIntensityTotal()  / 263.0f;
			this->_displayInfo.backlightIntensity[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetBacklightIntensityTotal() / 263.0f;
			
			if (this->_willPostprocessDisplays)
			{
				this->PostprocessDisplay(NDSDisplayID_Main,  this->_displayInfo);
				this->PostprocessDisplay(NDSDisplayID_Touch, this->_displayInfo);
			}
			
			if (this->_willAutoResolveToCustomBuffer)
			{
				this->ResolveDisplayToCustomFramebuffer(NDSDisplayID_Main,  this->_displayInfo);
				this->ResolveDisplayToCustomFramebuffer(NDSDisplayID_Touch, this->_displayInfo);
			}
			
			this->AsyncSetupEngineBuffersStart();
		}
		
		// Reset the current backlight intensity total.
		this->_display[NDSDisplayID_Main]->SetBacklightIntensityTotal(0.0f);
		this->_display[NDSDisplayID_Touch]->SetBacklightIntensityTotal(0.0f);
		
		if (this->_frameNeedsFinish)
		{
			this->_frameNeedsFinish = false;
			this->_displayInfo.sequenceNumber++;
			this->_event->DidFrameEnd(this->_willFrameSkip, this->_displayInfo);
		}
	}
}

void GPUSubsystem::UpdateAverageBacklightIntensityTotal()
{
	// The values in this table are, more or less, arbitrarily chosen.
	//
	// It would be nice to get some real testing done on a hardware NDS to have some more
	// accurate intensity values for each backlight level.
	static const float backlightLevelToIntensityTable[] = {0.100, 0.300, 0.600, 1.000};
	
	IOREG_POWERMANCTL POWERMANCTL;
	IOREG_BACKLIGHTCTL BACKLIGHTCTL;
	POWERMANCTL.value = MMU.powerMan_Reg[0];
	BACKLIGHTCTL.value = MMU.powerMan_Reg[4];
	
	if (POWERMANCTL.MainBacklight_Enable != 0)
	{
		const BacklightLevel level = ((BACKLIGHTCTL.ExternalPowerState != 0) && (BACKLIGHTCTL.ForceMaxBrightnessWithExtPower_Enable != 0)) ? BacklightLevel_Maximum : (BacklightLevel)BACKLIGHTCTL.Level;
		this->_display[NDSDisplayID_Main]->SetBacklightIntensityTotal(this->_display[NDSDisplayID_Main]->GetBacklightIntensityTotal() + backlightLevelToIntensityTable[level]);
	}
	
	if (POWERMANCTL.TouchBacklight_Enable != 0)
	{
		const BacklightLevel level = ((BACKLIGHTCTL.ExternalPowerState != 0) && (BACKLIGHTCTL.ForceMaxBrightnessWithExtPower_Enable != 0)) ? BacklightLevel_Maximum : (BacklightLevel)BACKLIGHTCTL.Level;
		this->_display[NDSDisplayID_Touch]->SetBacklightIntensityTotal(this->_display[NDSDisplayID_Touch]->GetBacklightIntensityTotal() + backlightLevelToIntensityTable[level]);
	}
}

void GPUSubsystem::ClearWithColor(const u16 colorBGRA5551)
{
	const u16 color16 = colorBGRA5551 | 0x8000;
	const size_t nativeFramebufferPixCount = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2;
	const size_t customFramebufferPixCount = this->_displayInfo.customWidth * this->_displayInfo.customHeight * 2;
	
	if (this->_displayInfo.colorFormat == NDSColorFormat_BGR555_Rev)
	{
		if (this->_displayInfo.isCustomSizeRequested)
		{
			for (size_t i = 0; i < this->_displayInfo.framebufferPageCount; i++)
			{
				memset_u16((u8 *)this->_masterFramebuffer + (this->_displayInfo.framebufferPageSize * i), color16, nativeFramebufferPixCount);
				memset_u16((u8 *)this->_masterFramebuffer + (this->_displayInfo.framebufferPageSize * i) + (nativeFramebufferPixCount * sizeof(u16)), color16, customFramebufferPixCount);
			}
		}
		else
		{
			for (size_t i = 0; i < this->_displayInfo.framebufferPageCount; i++)
			{
				memset_u16((u8 *)this->_masterFramebuffer + (this->_displayInfo.framebufferPageSize * i), color16, nativeFramebufferPixCount);
			}
		}
	}
	else
	{
		Color4u8 color32;
		
		switch (this->_displayInfo.colorFormat)
		{
			case NDSColorFormat_BGR666_Rev:
				color32.value = LE_TO_LOCAL_32( ColorspaceConvert555To6665Opaque<false>(colorBGRA5551 & 0x7FFF) );
				break;
				
			case NDSColorFormat_BGR888_Rev:
				color32.value = LE_TO_LOCAL_32( ColorspaceConvert555To8888Opaque<false>(colorBGRA5551 & 0x7FFF) );
				break;
				
			default:
				break;
		}
		
		for (size_t i = 0; i < this->_displayInfo.framebufferPageCount; i++)
		{
			memset_u16((u8 *)this->_masterFramebuffer + (this->_displayInfo.framebufferPageSize * i), color16, nativeFramebufferPixCount);
			memset_u32((u8 *)this->_masterFramebuffer + (this->_displayInfo.framebufferPageSize * i) + (nativeFramebufferPixCount * sizeof(u16)), color32.value, customFramebufferPixCount);
		}
	}
}

void GPUSubsystem::_DownscaleAndConvertForSavestate(const NDSDisplayID displayID, const void *srcBuffer, u16 *dstBuffer)
{
	if ( (srcBuffer == NULL) || (dstBuffer == NULL) || !this->_display[displayID]->DidPerformCustomRender() )
	{
		return;
	}
	
	if (!this->_display[displayID]->IsEnabled())
	{
		memset(dstBuffer, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	}
	else if (this->_display[displayID]->GetColorFormat() == NDSColorFormat_BGR555_Rev)
	{
		const u16 *__restrict src = (u16 *)srcBuffer;
		u16 *__restrict dst = dstBuffer;
		
		for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
		{
			CopyLineReduceHinted<0x3FFF, false, true, 2>(this->_lineInfo[l], src, dst);
			src += this->_lineInfo[l].pixelCount;
			dst += GPU_FRAMEBUFFER_NATIVE_WIDTH;
		}
	}
	else
	{
		const u32 *__restrict src = (u32 *)srcBuffer;
		u32 *__restrict working = this->_display[displayID]->GetWorkingNativeBuffer32();
		u16 *__restrict dst = dstBuffer;
		
		for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
		{
			CopyLineReduceHinted<0x3FFF, false, true, 4>(this->_lineInfo[l], src, working);
			src += this->_lineInfo[l].pixelCount;
			working += GPU_FRAMEBUFFER_NATIVE_WIDTH;
		}
		
		switch (this->_display[displayID]->GetColorFormat())
		{
			case NDSColorFormat_BGR666_Rev:
				ColorspaceConvertBuffer6665To5551<false, false>(this->_display[displayID]->GetWorkingNativeBuffer32(), dst, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				ColorspaceConvertBuffer8888To5551<false, false>(this->_display[displayID]->GetWorkingNativeBuffer32(), dst, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				break;
				
			default:
				break;
		}
	}
}

void GPUSubsystem::_ConvertAndUpscaleForLoadstate(const NDSDisplayID displayID, const u16 *srcBuffer, void *dstBuffer)
{
	if ( (srcBuffer == NULL) || (dstBuffer == NULL) || !this->_display[displayID]->DidPerformCustomRender() )
	{
		return;
	}
	
	const u16 *__restrict src = srcBuffer;
	
	if (!this->_display[displayID]->IsEnabled())
	{
		memset(dstBuffer, 0, this->_display[displayID]->GetWidth() * this->_display[displayID]->GetHeight() * this->_display[displayID]->GetPixelBytes());
	}
	else if (this->_display[displayID]->GetColorFormat() == NDSColorFormat_BGR555_Rev)
	{
		u16 *__restrict dst = (u16 *)dstBuffer;
		
		for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
		{
			CopyLineExpandHinted<0x3FFF, true, false, true, 2>(this->_lineInfo[l], src, dst);
			src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
			dst += this->_lineInfo[l].pixelCount;
		}
	}
	else
	{
		u32 *__restrict dst = (u32 *)dstBuffer;
		u32 *__restrict working = (this->_display[displayID]->IsCustomSizeRequested()) ? this->_display[displayID]->GetWorkingNativeBuffer32() : dst;
		
		switch (this->_display[displayID]->GetColorFormat())
		{
			case NDSColorFormat_BGR666_Rev:
				ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapDst>(src, working, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				break;

			case NDSColorFormat_BGR888_Rev:
				ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(src, working, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				break;
				
			default:
				break;
		}
		
		if (this->_display[displayID]->IsCustomSizeRequested())
		{
			for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
			{
				CopyLineExpandHinted<0x3FFF, true, false, true, 4>(this->_lineInfo[l], working, dst);
				working += GPU_FRAMEBUFFER_NATIVE_WIDTH;
				dst += this->_lineInfo[l].pixelCount;
			}
		}
	}
}

void GPUSubsystem::SaveState(EMUFILE &os)
{
	// Savestate chunk version
	os.write_32LE(2);
	
	// Version 0
	
	// Downscale and color convert the display framebuffers.
	this->_DownscaleAndConvertForSavestate(NDSDisplayID_Main,  this->_display[NDSDisplayID_Main]->GetCustomBuffer(),  this->_display[NDSDisplayID_Main]->GetNativeBuffer16());
	os.fwrite(this->_display[NDSDisplayID_Main]->GetNativeBuffer16(),  GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	
	this->_DownscaleAndConvertForSavestate(NDSDisplayID_Touch, this->_display[NDSDisplayID_Touch]->GetCustomBuffer(), this->_display[NDSDisplayID_Touch]->GetNativeBuffer16());
	os.fwrite(this->_display[NDSDisplayID_Touch]->GetNativeBuffer16(), GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	
	// Version 1
	os.write_32LE(this->_engineMain->savedBG2X.value);
	os.write_32LE(this->_engineMain->savedBG2Y.value);
	os.write_32LE(this->_engineMain->savedBG3X.value);
	os.write_32LE(this->_engineMain->savedBG3Y.value);
	os.write_32LE(this->_engineSub->savedBG2X.value);
	os.write_32LE(this->_engineSub->savedBG2Y.value);
	os.write_32LE(this->_engineSub->savedBG3X.value);
	os.write_32LE(this->_engineSub->savedBG3Y.value);
	
	// Version 2
	os.write_floatLE( this->_display[NDSDisplayID_Main]->GetBacklightIntensityTotal() );
	os.write_floatLE( this->_display[NDSDisplayID_Touch]->GetBacklightIntensityTotal() );
}

bool GPUSubsystem::LoadState(EMUFILE &is, int size)
{
	u32 version;
	
	//sigh.. shouldve used a new version number
	if (size == GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16) * 2)
	{
		version = 0;
	}
	else if (size == 0x30024)
	{
		is.read_32LE(version);
		version = 1;
	}
	else
	{
		if (is.read_32LE(version) < 1) return false;
	}
	
	if (version > 2) return false;
	
	// Version 0
	is.fread((u8 *)this->_displayInfo.nativeBuffer16[NDSDisplayID_Main],  GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	is.fread((u8 *)this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch], GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
	
	this->_ConvertAndUpscaleForLoadstate(NDSDisplayID_Main,  this->_displayInfo.nativeBuffer16[NDSDisplayID_Main],  this->_displayInfo.customBuffer[NDSDisplayID_Main]);
	this->_ConvertAndUpscaleForLoadstate(NDSDisplayID_Touch, this->_displayInfo.nativeBuffer16[NDSDisplayID_Touch], this->_displayInfo.customBuffer[NDSDisplayID_Touch]);
	
	// Version 1
	if (version >= 1)
	{
		is.read_32LE(this->_engineMain->savedBG2X.value);
		is.read_32LE(this->_engineMain->savedBG2Y.value);
		is.read_32LE(this->_engineMain->savedBG3X.value);
		is.read_32LE(this->_engineMain->savedBG3Y.value);
		is.read_32LE(this->_engineSub->savedBG2X.value);
		is.read_32LE(this->_engineSub->savedBG2Y.value);
		is.read_32LE(this->_engineSub->savedBG3X.value);
		is.read_32LE(this->_engineSub->savedBG3Y.value);
		//removed per nitsuja feedback. anyway, this same thing will happen almost immediately in gpu line=0
		//this->_engineMain->refreshAffineStartRegs(-1,-1);
		//this->_engineSub->refreshAffineStartRegs(-1,-1);
	}
	
	// Version 2
	if (version >= 2)
	{
		float readIntensity = 0.0f;
		
		is.read_floatLE(readIntensity);
		this->_display[NDSDisplayID_Main]->SetBacklightIntensityTotal(readIntensity / 71.0f);
		this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->GetBacklightIntensityTotal();
		
		is.read_floatLE(readIntensity);
		this->_display[NDSDisplayID_Touch]->SetBacklightIntensityTotal(readIntensity / 71.0f);
		this->_displayInfo.backlightIntensity[NDSDisplayID_Touch]  = this->_display[NDSDisplayID_Touch]->GetBacklightIntensityTotal();
	}
	else
	{
		// UpdateAverageBacklightIntensityTotal() adds to _backlightIntensityTotal, and is called 263 times per frame.
		// Of these, 71 calls are after _displayInfo.backlightIntensity is set.
		// This emulates those calls as a way of guessing what the backlight values were in a savestate which doesn't contain that information.
		this->_display[NDSDisplayID_Main]->SetBacklightIntensityTotal(0.0f);
		this->_display[NDSDisplayID_Touch]->SetBacklightIntensityTotal(0.0f);
		this->UpdateAverageBacklightIntensityTotal();
		this->_displayInfo.backlightIntensity[NDSDisplayID_Main]  = this->_display[NDSDisplayID_Main]->GetBacklightIntensityTotal();
		this->_displayInfo.backlightIntensity[NDSDisplayID_Touch] = this->_display[NDSDisplayID_Touch]->GetBacklightIntensityTotal();
		this->_display[NDSDisplayID_Main]->SetBacklightIntensityTotal(this->_display[NDSDisplayID_Main]->GetBacklightIntensityTotal() * 71.0f);
		this->_display[NDSDisplayID_Touch]->SetBacklightIntensityTotal(this->_display[NDSDisplayID_Touch]->GetBacklightIntensityTotal() * 71.0f);
	}
	
	// Parse all GPU engine related registers based on a previously read MMU savestate chunk.
	this->_engineMain->ParseAllRegisters();
	this->_engineSub->ParseAllRegisters();
	
	return !is.fail();
}

void GPUEventHandlerDefault::DidFrameBegin(const size_t line, const bool isFrameSkipRequested, const size_t pageCount, u8 &selectedBufferIndexInOut)
{
	if ( (pageCount > 1) && (line == 0) && !isFrameSkipRequested )
	{
		selectedBufferIndexInOut = ((selectedBufferIndexInOut + 1) % pageCount);
	}
}

GPUClientFetchObject::GPUClientFetchObject()
{
	_id = 0;
	
	memset(_name, 0, sizeof(_name));
	strncpy(_name, "Generic Video", sizeof(_name) - 1);
	
	memset(_description, 0, sizeof(_description));
	strncpy(_description, "No description.", sizeof(_description) - 1);
	
	_clientData = NULL;
	_lastFetchIndex = 0;
	
	for (size_t i = 0; i < MAX_FRAMEBUFFER_PAGES; i++)
	{
		memset(&_fetchDisplayInfo[i], 0, sizeof(NDSDisplayInfo));
	}
}

void GPUClientFetchObject::Init()
{
	// Do nothing. This is implementation dependent.
}

void GPUClientFetchObject::SetFetchBuffers(const NDSDisplayInfo &currentDisplayInfo)
{
	const size_t nativeSize = GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16);
	const size_t customSize = currentDisplayInfo.customWidth * currentDisplayInfo.customHeight * currentDisplayInfo.pixelBytes;
	
	for (size_t i = 0; i < currentDisplayInfo.framebufferPageCount; i++)
	{
		this->_fetchDisplayInfo[i] = currentDisplayInfo;
		
		if (i == 0)
		{
			this->_fetchDisplayInfo[0].nativeBuffer16[NDSDisplayID_Main]  = (u16 *)currentDisplayInfo.masterFramebufferHead;
			this->_fetchDisplayInfo[0].nativeBuffer16[NDSDisplayID_Touch] = (u16 *)((u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 1));
			this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Main]    = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 2);
			this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Touch]   = (u8 *)currentDisplayInfo.masterFramebufferHead + (nativeSize * 2) + customSize;
		}
		else
		{
			this->_fetchDisplayInfo[i].nativeBuffer16[NDSDisplayID_Main]  = (u16 *)((u8 *)this->_fetchDisplayInfo[0].nativeBuffer16[NDSDisplayID_Main]  + (currentDisplayInfo.framebufferPageSize * i));
			this->_fetchDisplayInfo[i].nativeBuffer16[NDSDisplayID_Touch] = (u16 *)((u8 *)this->_fetchDisplayInfo[0].nativeBuffer16[NDSDisplayID_Touch] + (currentDisplayInfo.framebufferPageSize * i));
			this->_fetchDisplayInfo[i].customBuffer[NDSDisplayID_Main]    = (u8 *)this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Main]    + (currentDisplayInfo.framebufferPageSize * i);
			this->_fetchDisplayInfo[i].customBuffer[NDSDisplayID_Touch]   = (u8 *)this->_fetchDisplayInfo[0].customBuffer[NDSDisplayID_Touch]   + (currentDisplayInfo.framebufferPageSize * i);
		}
	}
}

void GPUClientFetchObject::FetchFromBufferIndex(const u8 index)
{
	if (this->_fetchDisplayInfo[index].isDisplayEnabled[NDSDisplayID_Main])
	{
		if (!this->_fetchDisplayInfo[index].didPerformCustomRender[NDSDisplayID_Main])
		{
			this->_FetchNativeDisplayByID(NDSDisplayID_Main, index);
		}
		else
		{
			this->_FetchCustomDisplayByID(NDSDisplayID_Main, index);
		}
	}
	
	if (this->_fetchDisplayInfo[index].isDisplayEnabled[NDSDisplayID_Touch])
	{
		if (!this->_fetchDisplayInfo[index].didPerformCustomRender[NDSDisplayID_Touch])
		{
			this->_FetchNativeDisplayByID(NDSDisplayID_Touch, index);
		}
		else
		{
			this->_FetchCustomDisplayByID(NDSDisplayID_Touch, index);
		}
	}
	
	this->SetLastFetchIndex(index);
}

void GPUClientFetchObject::_FetchNativeDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// Do nothing. This is implementation dependent.
}

void GPUClientFetchObject::_FetchCustomDisplayByID(const NDSDisplayID displayID, const u8 bufferIndex)
{
	// Do nothing. This is implementation dependent.
}

const NDSDisplayInfo& GPUClientFetchObject::GetFetchDisplayInfoForBufferIndex(const u8 bufferIndex) const
{
	return this->_fetchDisplayInfo[bufferIndex];
}

void GPUClientFetchObject::SetFetchDisplayInfo(const NDSDisplayInfo &displayInfo)
{
	this->_fetchDisplayInfo[displayInfo.bufferIndex] = displayInfo;
}

const s32 GPUClientFetchObject::GetID() const
{
	return this->_id;
}

const char* GPUClientFetchObject::GetName() const
{
	return this->_name;
}

const char* GPUClientFetchObject::GetDescription() const
{
	return this->_description;
}

u8 GPUClientFetchObject::GetLastFetchIndex() const
{
	return this->_lastFetchIndex;
}

void GPUClientFetchObject::SetLastFetchIndex(const u8 fetchIndex)
{
	this->_lastFetchIndex = fetchIndex;
}

void* GPUClientFetchObject::GetClientData() const
{
	return this->_clientData;
}

void GPUClientFetchObject::SetClientData(void *clientData)
{
	this->_clientData = clientData;
}

NDSDisplay::NDSDisplay()
{
	__constructor(NDSDisplayID_Main, NULL);
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID)
{
	__constructor(displayID, NULL);
}

NDSDisplay::NDSDisplay(const NDSDisplayID displayID, GPUEngineBase *theEngine)
{
	__constructor(displayID, theEngine);
}

void NDSDisplay::__constructor(const NDSDisplayID displayID, GPUEngineBase *theEngine)
{
	_ID = displayID;
	_gpuEngine = theEngine;
	
	for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
 	{
 		_isLineDisplayNative[l] = true;
 	}

 	_nativeLineDisplayCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_nativeBuffer16 = NULL;
	_customBuffer = NULL;
	
	_customColorFormat = NDSColorFormat_BGR555_Rev;
	_customPixelBytes = sizeof(u16);
	_customWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_customHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	_isCustomSizeRequested = false;
	
	_renderedBuffer = this->_nativeBuffer16;
 	_renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
 	_renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_isEnabled = true;
}

NDSDisplayID NDSDisplay::GetDisplayID() const
{
	return this->_ID;
}

GPUEngineBase* NDSDisplay::GetEngine()
{
	return this->_gpuEngine;
}

void NDSDisplay::SetEngine(GPUEngineBase *theEngine)
{
	this->_gpuEngine = theEngine;
	this->_gpuEngine->SetTargetDisplay(this);
}

GPUEngineID NDSDisplay::GetEngineID()
{
	return this->_gpuEngine->GetEngineID();
}

void NDSDisplay::SetEngineByID(const GPUEngineID theID)
{
	this->SetEngine( (theID == GPUEngineID_Main) ? (GPUEngineBase *)GPU->GetEngineMain() : (GPUEngineBase *)GPU->GetEngineSub() );
}

size_t NDSDisplay::GetNativeLineCount()
{
   return this->_nativeLineDisplayCount;
}

bool NDSDisplay::GetIsLineNative(const size_t l)
{
   return this->_isLineDisplayNative[l];
}

void NDSDisplay::SetIsLineNative(const size_t l, const bool isNative)
{
   if (this->_isLineDisplayNative[l] != isNative)
   {
	   if (isNative)
	   {
		   this->_isLineDisplayNative[l] = isNative;
		   this->_nativeLineDisplayCount++;
	   }
	   else
	   {
		   this->_isLineDisplayNative[l] = isNative;
		   this->_nativeLineDisplayCount--;
	   }
   }
}

void NDSDisplay::ClearAllLinesToNative()
{
   for (size_t l = 0; l < GPU_FRAMEBUFFER_NATIVE_HEIGHT; l++)
   {
	   this->_isLineDisplayNative[l] = true;
   }

   this->_nativeLineDisplayCount = GPU_FRAMEBUFFER_NATIVE_HEIGHT;

   this->_renderedBuffer = this->_nativeBuffer16;
   this->_renderedWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
   this->_renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
}

void NDSDisplay::ResolveLinesDisplayedNative()
{
	if (this->_nativeLineDisplayCount == GPU_FRAMEBUFFER_NATIVE_HEIGHT)
 	{
 		return;
 	}
 	else if (this->_nativeLineDisplayCount == 0)
 	{
 		this->_renderedWidth  = this->_customWidth;
 		this->_renderedHeight = this->_customHeight;
 		this->_renderedBuffer = this->_customBuffer;
 		return;
 	}

	const u16 *__restrict src = this->_nativeBuffer16;
	
	// Rendering should consist of either all native-sized lines or all custom-sized lines.
	// But if there is a mix of both native-sized and custom-sized lines, then we need to
	// resolve any remaining native lines to the custom buffer.
	if (this->_customColorFormat == NDSColorFormat_BGR555_Rev)
	{
		u16 *__restrict dst = (u16 *__restrict)this->_customBuffer;
		
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(y);
			
			if (this->_isLineDisplayNative[y])
			{
				CopyLineExpandHinted<0x3FFF, true, false, false, 2>(lineInfo, src, dst);
				this->_isLineDisplayNative[y] = false;
			}
			
			src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
			dst += lineInfo.pixelCount;
		}
	}
	else
	{
		u32 *__restrict working = this->_workingNativeBuffer32;
		u32 *__restrict dst = (u32 *__restrict)this->_customBuffer;
		
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(y);
			
			if (this->_isLineDisplayNative[y])
			{
				if (this->_customColorFormat == NDSColorFormat_BGR888_Rev)
				{
					ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(src, working, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				}
				else
				{
					ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapDst>(src, working, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				}
				
				CopyLineExpandHinted<0x3FFF, true, false, false, 4>(lineInfo, working, dst);
				this->_isLineDisplayNative[y] = false;
			}
			
			src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
			working += GPU_FRAMEBUFFER_NATIVE_WIDTH;
			dst += lineInfo.pixelCount;
		}
	}
	
	this->_nativeLineDisplayCount = 0;
 	this->_renderedWidth  = this->_customWidth;
 	this->_renderedHeight = this->_customHeight;
 	this->_renderedBuffer = this->_customBuffer;
}

void NDSDisplay::ResolveFramebufferToCustom(NDSDisplayInfo &mutableInfo)
{
	// All lines should be 15-bit native-sized lines.
	//
	// This method is called to transfer these lines into the customBuffer portion of the current
	// framebuffer page so that clients can access a single continuous buffer.
	
	if (this->DidPerformCustomRender())
	{
		return;
	}
	
	if (mutableInfo.isCustomSizeRequested)
	{
		const u16 *__restrict src = this->_nativeBuffer16;
		u32 *__restrict working = this->_workingNativeBuffer32;
		
		switch (mutableInfo.colorFormat)
		{
			case NDSColorFormat_BGR666_Rev:
			case NDSColorFormat_BGR888_Rev:
				ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(src, working, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				break;
				
			default:
				break;
		}
		
		if (mutableInfo.pixelBytes == 2)
		{
			u16 *__restrict dst = (u16 *__restrict)this->_customBuffer;
			
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(y);
				CopyLineExpandHinted<0x3FFF, true, false, false, 2>(lineInfo, src, dst);
				src += GPU_FRAMEBUFFER_NATIVE_WIDTH;
				dst += lineInfo.pixelCount;
			}
		}
		else if (mutableInfo.pixelBytes == 4)
		{
			u32 *__restrict dst = (u32 *__restrict)this->_customBuffer;
			
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(y);
				CopyLineExpandHinted<0x3FFF, true, false, false, 4>(lineInfo, working, dst);
				working += GPU_FRAMEBUFFER_NATIVE_WIDTH;
				dst += lineInfo.pixelCount;
			}
		}
	}
	else
	{
		switch (mutableInfo.colorFormat)
		{
			case NDSColorFormat_BGR555_Rev:
				memcpy(this->_customBuffer, this->_nativeBuffer16, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(u16));
				break;
				
			case NDSColorFormat_BGR666_Rev:
			case NDSColorFormat_BGR888_Rev:
				ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>(this->_nativeBuffer16, (u32 *)this->_customBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT);
				break;
		}
	}
	
	mutableInfo.didPerformCustomRender[this->_ID] = true;
}

bool NDSDisplay::DidPerformCustomRender() const
{
	return (this->_nativeLineDisplayCount < GPU_FRAMEBUFFER_NATIVE_HEIGHT);
}

u16* NDSDisplay::GetNativeBuffer16() const
{
	return this->_nativeBuffer16;
}

u32* NDSDisplay::GetWorkingNativeBuffer32() const
{
	return this->_workingNativeBuffer32;
}

void* NDSDisplay::GetCustomBuffer() const
{
	return this->_customBuffer;
}

void NDSDisplay::SetDrawBuffers(u16 *nativeBuffer16, u32 *workingNativeBuffer32, void *customBuffer)
{
	this->_nativeBuffer16 = nativeBuffer16;
	this->_workingNativeBuffer32 = workingNativeBuffer32;
	this->_customBuffer = customBuffer;
	this->_renderedBuffer = (this->_nativeLineDisplayCount == GPU_FRAMEBUFFER_NATIVE_HEIGHT) ? (u8 *)nativeBuffer16 : (u8 *)customBuffer;
	
	if (this->_gpuEngine != NULL)
	{
		this->_gpuEngine->DisplayDrawBuffersUpdate();
	}
}

NDSColorFormat NDSDisplay::GetColorFormat() const
{
	return this->_customColorFormat;
}

void NDSDisplay::SetColorFormat(NDSColorFormat colorFormat)
{
	this->_customColorFormat = colorFormat;
	this->_customPixelBytes = (colorFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(Color4u8);
}

size_t NDSDisplay::GetPixelBytes() const
{
	return this->_customPixelBytes;
}

size_t NDSDisplay::GetWidth() const
{
	return this->_customWidth;
}

size_t NDSDisplay::GetHeight() const
{
	return this->_customHeight;
}

void NDSDisplay::SetDisplaySize(size_t w, size_t h)
{
	this->_customWidth = w;
	this->_customHeight = h;
	this->_isCustomSizeRequested = (this->_customWidth != GPU_FRAMEBUFFER_NATIVE_WIDTH) || (this->_customHeight != GPU_FRAMEBUFFER_NATIVE_HEIGHT);
}

bool NDSDisplay::IsCustomSizeRequested() const
{
	return this->_isCustomSizeRequested;
}

void* NDSDisplay::GetRenderedBuffer() const
{
   return this->_renderedBuffer;
}

size_t NDSDisplay::GetRenderedWidth() const
{
   return this->_renderedWidth;
}

size_t NDSDisplay::GetRenderedHeight() const
{
   return this->_renderedHeight;
}

bool NDSDisplay::IsEnabled() const
{
	return this->_isEnabled;
}

void NDSDisplay::SetIsEnabled(bool stateIsEnabled)
{
	this->_isEnabled = stateIsEnabled;
}

float NDSDisplay::GetBacklightIntensityTotal() const
{
	return this->_backlightIntensityTotal;
}

void NDSDisplay::SetBacklightIntensityTotal(float intensity)
{
	this->_backlightIntensityTotal = intensity;
}

template <NDSColorFormat OUTPUTFORMAT>
void NDSDisplay::ApplyMasterBrightness(const NDSDisplayInfo &displayInfo)
{
	if (!displayInfo.masterBrightnessDiffersPerLine[this->_ID])
	{
		// Apply the master brightness as a framebuffer operation.
		this->ApplyMasterBrightness<OUTPUTFORMAT>(this->_renderedBuffer,
			                                      this->_renderedWidth * this->_renderedHeight,
			                                      (GPUMasterBrightMode)displayInfo.masterBrightnessMode[this->_ID][0],
			                                      displayInfo.masterBrightnessIntensity[this->_ID][0]);
	}
	else
	{
		// Apply the master brightness as a scanline operation.
		for (size_t line = 0; line < GPU_FRAMEBUFFER_NATIVE_HEIGHT; line++)
		{
			const GPUEngineLineInfo &lineInfo = GPU->GetLineInfoAtIndex(line);
			void *dstColorLine = (!this->DidPerformCustomRender()) ? ((u8 *)this->_nativeBuffer16 + (lineInfo.blockOffsetNative * sizeof(u16))) : ((u8 *)this->_customBuffer + (lineInfo.blockOffsetCustom * this->_customPixelBytes));
			const size_t pixCount = (!this->DidPerformCustomRender()) ? GPU_FRAMEBUFFER_NATIVE_WIDTH : lineInfo.pixelCount;
			
			this->ApplyMasterBrightness<OUTPUTFORMAT>(dstColorLine,
			                                          pixCount,
			                                          (GPUMasterBrightMode)displayInfo.masterBrightnessMode[this->_ID][line],
			                                          displayInfo.masterBrightnessIntensity[this->_ID][line]);
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void NDSDisplay::ApplyMasterBrightness(void *dst, const size_t pixCount, const GPUMasterBrightMode mode, const u8 intensity)
{
	if (intensity == 0)
	{
		return;
	}
	
	const bool isFullIntensity = (intensity >= 16);
	const u8 intensityClamped = (isFullIntensity) ? 16 : intensity;
	
	switch (mode)
	{
		case GPUMasterBrightMode_Disable:
			// Do nothing.
			break;
			
		case GPUMasterBrightMode_Up:
		{
			if (!isFullIntensity)
			{
				size_t i = 0;
				
#ifdef USEMANUALVECTORIZATION
				i = this->_ApplyMasterBrightnessUp_LoopOp<OUTPUTFORMAT>(dst, pixCount, intensityClamped);
#pragma LOOPVECTORIZE_DISABLE
#endif
				for (; i < pixCount; i++)
				{
					if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
					{
						((u16 *)dst)[i] = PixelOperation::BrightnessUpTable555[intensityClamped][ ((u16 *)dst)[i] & 0x7FFF ] | 0x8000;
					}
					else
					{
						((Color4u8 *)dst)[i] = colorop.increase<OUTPUTFORMAT>(((Color4u8 *)dst)[i], intensityClamped);
						((Color4u8 *)dst)[i].a = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F : 0xFF;
					}
				}
			}
			else
			{
				// all white (optimization)
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memset_u16(dst, 0xFFFF, pixCount);
						break;
						
					case NDSColorFormat_BGR666_Rev:
						memset_u32(dst, 0x1F3F3F3F, pixCount);
						break;
						
					case NDSColorFormat_BGR888_Rev:
						memset_u32(dst, 0xFFFFFFFF, pixCount);
						break;
						
					default:
						break;
				}
			}
			break;
		}
			
		case GPUMasterBrightMode_Down:
		{
			if (!isFullIntensity)
			{
				size_t i = 0;
				
#ifdef USEMANUALVECTORIZATION
				i = this->_ApplyMasterBrightnessDown_LoopOp<OUTPUTFORMAT>(dst, pixCount, intensityClamped);
#pragma LOOPVECTORIZE_DISABLE
#endif
				for (; i < pixCount; i++)
				{
					if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
					{
						((u16 *)dst)[i] = PixelOperation::BrightnessDownTable555[intensityClamped][ ((u16 *)dst)[i] & 0x7FFF ] | 0x8000;
					}
					else
					{
						((Color4u8 *)dst)[i] = colorop.decrease<OUTPUTFORMAT>(((Color4u8 *)dst)[i], intensityClamped);
						((Color4u8 *)dst)[i].a = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F : 0xFF;
					}
				}
			}
			else
			{
				// all black (optimization)
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memset_u16(dst, 0x8000, pixCount);
						break;
						
					case NDSColorFormat_BGR666_Rev:
						memset_u32(dst, 0x1F000000, pixCount);
						break;
						
					case NDSColorFormat_BGR888_Rev:
						memset_u32(dst, 0xFF000000, pixCount);
						break;
						
					default:
						break;
				}
			}
			break;
		}
			
		case GPUMasterBrightMode_Reserved:
			// Do nothing.
			break;
	}
}

void NDSDisplay::Postprocess(NDSDisplayInfo &mutableDisplayInfo)
{
	if (this->_isEnabled)
	{
		if ( (this->_customColorFormat == NDSColorFormat_BGR666_Rev) && this->DidPerformCustomRender() && mutableDisplayInfo.needConvertColorFormat[this->_ID] )
		{
			ColorspaceConvertBuffer6665To8888<false, false>((u32 *)this->_renderedBuffer, (u32 *)this->_renderedBuffer, this->_renderedWidth * this->_renderedHeight);
		}
		
		if (mutableDisplayInfo.needApplyMasterBrightness[this->_ID])
		{
			if ( (this->_customColorFormat == NDSColorFormat_BGR555_Rev) || !this->DidPerformCustomRender() )
			{
				this->ApplyMasterBrightness<NDSColorFormat_BGR555_Rev>(mutableDisplayInfo);
			}
			else
			{
				this->ApplyMasterBrightness<NDSColorFormat_BGR888_Rev>(mutableDisplayInfo);
			}
		}
	}
	else
	{
		memset(this->_renderedBuffer, 0, this->_renderedWidth * this->_renderedHeight * this->_customPixelBytes);
	}
	
	mutableDisplayInfo.needConvertColorFormat[this->_ID] = false;
	mutableDisplayInfo.needApplyMasterBrightness[this->_ID] = false;
}

template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnHOFS<GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG0>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG1>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnVOFS<GPULayerID_BG3>();

template void GPUEngineBase::ParseReg_BGnX<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnY<GPULayerID_BG2>();
template void GPUEngineBase::ParseReg_BGnX<GPULayerID_BG3>();
template void GPUEngineBase::ParseReg_BGnY<GPULayerID_BG3>();
