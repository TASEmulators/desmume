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

CACHE_ALIGN u32 color_555_to_6665_opaque[32768];
CACHE_ALIGN u32 color_555_to_6665_opaque_swap_rb[32768];
CACHE_ALIGN u32 color_555_to_666[32768];
CACHE_ALIGN u32 color_555_to_8888_opaque[32768];
CACHE_ALIGN u32 color_555_to_8888_opaque_swap_rb[32768];
CACHE_ALIGN u32 color_555_to_888[32768];

//is this a crazy idea? this table spreads 5 bits evenly over 31 from exactly 0 to INT_MAX
CACHE_ALIGN const u32 material_5bit_to_31bit[] = {
	0x00000000, 0x04210842, 0x08421084, 0x0C6318C6,
	0x10842108, 0x14A5294A, 0x18C6318C, 0x1CE739CE,
	0x21084210, 0x25294A52, 0x294A5294, 0x2D6B5AD6,
	0x318C6318, 0x35AD6B5A, 0x39CE739C, 0x3DEF7BDE,
	0x42108421, 0x46318C63, 0x4A5294A5, 0x4E739CE7,
	0x5294A529, 0x56B5AD6B, 0x5AD6B5AD, 0x5EF7BDEF,
	0x6318C631, 0x6739CE73, 0x6B5AD6B5, 0x6F7BDEF7,
	0x739CE739, 0x77BDEF7B, 0x7BDEF7BD, 0x7FFFFFFF
};

// 5-bit to 6-bit conversions use this formula -- dst = (src == 0) ? 0 : (2*src) + 1
// Reference GBATEK: http://problemkaputt.de/gbatek.htm#ds3dtextureblending
CACHE_ALIGN const u8 material_5bit_to_6bit[] = {
	0x00, 0x03, 0x05, 0x07, 0x09, 0x0B, 0x0D, 0x0F,
	0x11, 0x13, 0x15, 0x17, 0x19, 0x1B, 0x1D, 0x1F,
	0x21, 0x23, 0x25, 0x27, 0x29, 0x2B, 0x2D, 0x2F,
	0x31, 0x33, 0x35, 0x37, 0x39, 0x3B, 0x3D, 0x3F
};

CACHE_ALIGN const u8 material_5bit_to_8bit[] = {
	0x00, 0x08, 0x10, 0x18, 0x21, 0x29, 0x31, 0x39,
	0x42, 0x4A, 0x52, 0x5A, 0x63, 0x6B, 0x73, 0x7B,
	0x84, 0x8C, 0x94, 0x9C, 0xA5, 0xAD, 0xB5, 0xBD,
	0xC6, 0xCE, 0xD6, 0xDE, 0xE7, 0xEF, 0xF7, 0xFF
};

CACHE_ALIGN const u8 material_6bit_to_8bit[] = {
	0x00, 0x04, 0x08, 0x0C, 0x10, 0x14, 0x18, 0x1C,
	0x20, 0x24, 0x28, 0x2C, 0x30, 0x34, 0x38, 0x3C,
	0x41, 0x45, 0x49, 0x4D, 0x51, 0x55, 0x59, 0x5D,
	0x61, 0x65, 0x69, 0x6D, 0x71, 0x75, 0x79, 0x7D,
	0x82, 0x86, 0x8A, 0x8E, 0x92, 0x96, 0x9A, 0x9E,
	0xA2, 0xA6, 0xAA, 0xAE, 0xB2, 0xB6, 0xBA, 0xBE,
	0xC3, 0xC7, 0xCB, 0xCF, 0xD3, 0xD7, 0xDB, 0xDF,
	0xE3, 0xE7, 0xEB, 0xEF, 0xF3, 0xF7, 0xFB, 0xFF
};

CACHE_ALIGN const u8 material_3bit_to_8bit[] = {
	0x00, 0x24, 0x49, 0x6D, 0x92, 0xB6, 0xDB, 0xFF
};

//maybe not very precise
CACHE_ALIGN const u8 material_3bit_to_5bit[] = {
	0, 4, 8, 13, 17, 22, 26, 31
};

//TODO - generate this in the static init method more accurately
CACHE_ALIGN const u8 material_3bit_to_6bit[] = {
	0, 8, 16, 26, 34, 44, 52, 63
};

//instantiate static instance
u16 GPUEngineBase::_brightnessUpTable555[17][0x8000];
FragmentColor GPUEngineBase::_brightnessUpTable666[17][0x8000];
FragmentColor GPUEngineBase::_brightnessUpTable888[17][0x8000];
u16 GPUEngineBase::_brightnessDownTable555[17][0x8000];
FragmentColor GPUEngineBase::_brightnessDownTable666[17][0x8000];
FragmentColor GPUEngineBase::_brightnessDownTable888[17][0x8000];
u8 GPUEngineBase::_blendTable555[17][17][32][32];
GPUEngineBase::MosaicLookup GPUEngineBase::_mosaicLookup;

GPUSubsystem *GPU = NULL;

static size_t _gpuLargestDstLineCount = 1;
static size_t _gpuVRAMBlockOffset = GPU_VRAM_BLOCK_LINES * GPU_FRAMEBUFFER_NATIVE_WIDTH;

static u16 *_gpuDstToSrcIndex = NULL; // Key: Destination pixel index / Value: Source pixel index
static u8 *_gpuDstToSrcSSSE3_u8_8e = NULL;
static u8 *_gpuDstToSrcSSSE3_u8_16e = NULL;
static u8 *_gpuDstToSrcSSSE3_u16_8e = NULL;

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

static void ExpandLine8(u8 *__restrict dst, const u8 *__restrict src, size_t dstLength)
{
#ifdef ENABLE_SSSE3
	const bool isIntegerScale = ((dstLength % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0);
	if (isIntegerScale)
	{
		const size_t scale = dstLength / GPU_FRAMEBUFFER_NATIVE_WIDTH;
		
		for (size_t srcX = 0, dstX = 0; srcX < GPU_FRAMEBUFFER_NATIVE_WIDTH; srcX+=16, dstX+=(scale*16))
		{
			const __m128i src_vec128 = _mm_load_si128((__m128i *)(src + srcX));
			
			for (size_t s = 0; s < scale; s++)
			{
				const __m128i ssse3idx_u8 = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u8_16e + (s * 16)));
				_mm_store_si128( (__m128i *)(dst + dstX + (s * 16)), _mm_shuffle_epi8(src_vec128, ssse3idx_u8) );
			}
		}
	}
	else
#endif
	{
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				dst[_gpuDstPitchIndex[x] + p] = src[x];
			}
		}
	}
}

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
	
	mainEngine->ParseAllRegisters();
	subEngine->ParseAllRegisters();
	
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
			GPUEngineBase::_brightnessUpTable555[i][j] = cur.val;
			GPUEngineBase::_brightnessUpTable666[i][j].color = COLOR555TO666(cur.val);
			GPUEngineBase::_brightnessUpTable888[i][j].color = COLOR555TO888(cur.val);
			
			cur.val = j;
			cur.bits.red = (cur.bits.red - (cur.bits.red * i / 16));
			cur.bits.green = (cur.bits.green - (cur.bits.green * i / 16));
			cur.bits.blue = (cur.bits.blue - (cur.bits.blue * i / 16));
			cur.bits.alpha = 0;
			GPUEngineBase::_brightnessDownTable555[i][j] = cur.val;
			GPUEngineBase::_brightnessDownTable666[i][j].color = COLOR555TO666(cur.val);
			GPUEngineBase::_brightnessDownTable888[i][j].color = COLOR555TO888(cur.val);
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
	free_aligned(this->_internalRenderLineTargetCustom);
	this->_internalRenderLineTargetCustom = NULL;
	free_aligned(this->_renderLineLayerIDCustom);
	this->_renderLineLayerIDCustom = NULL;
	free_aligned(this->_bgLayerIndexCustom);
	this->_bgLayerIndexCustom = NULL;
	free_aligned(this->_bgLayerColorCustom);
	this->_bgLayerColorCustom = NULL;
	
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

void GPUEngineBase::_Reset_Base()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	memset(this->_sprColor, 0, sizeof(this->_sprColor));
	memset(this->_sprAlpha, 0, sizeof(this->_sprAlpha));
	memset(this->_sprType, OBJMode_Normal, sizeof(this->_sprType));
	memset(this->_sprPrio, 0x7F, sizeof(this->_sprPrio));
	memset(this->_sprNum, 0, sizeof(this->_sprNum));
	
	memset(this->_didPassWindowTestNative, 1, 5 * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	memset(this->_enableColorEffectNative, 1, 5 * GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
	memset(this->_didPassWindowTestCustomMasterPtr, 1, 10 * dispInfo.customWidth * sizeof(u8));
	
	memset(this->_h_win[0], 0, sizeof(this->_h_win[0]));
	memset(this->_h_win[1], 0, sizeof(this->_h_win[1]));
	memset(&this->_mosaicColors, 0, sizeof(MosaicColor));
	memset(this->_itemsForPriority, 0, sizeof(this->_itemsForPriority));
	
	memset(this->_internalRenderLineTargetNative, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(FragmentColor));
	
	if (this->_internalRenderLineTargetCustom != NULL)
	{
		memset(this->_internalRenderLineTargetCustom, 0, dispInfo.customWidth * _gpuLargestDstLineCount * dispInfo.pixelBytes);
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
	
	this->_needUpdateWINH[0] = true;
	this->_needUpdateWINH[1] = true;
	
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
	this->_selectedBlendTable555 = (TBlendTable *)&GPUEngineBase::_blendTable555[this->_BLDALPHA_EVA][this->_BLDALPHA_EVB][0][0];
	this->_selectedBrightnessUpTable555 = &GPUEngineBase::_brightnessUpTable555[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessUpTable666 = &GPUEngineBase::_brightnessUpTable666[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessUpTable888 = &GPUEngineBase::_brightnessUpTable888[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessDownTable555 = &GPUEngineBase::_brightnessDownTable555[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessDownTable666 = &GPUEngineBase::_brightnessDownTable666[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessDownTable888 = &GPUEngineBase::_brightnessDownTable888[this->_BLDALPHA_EVY][0];
	
	this->_srcBlendEnable[GPULayerID_BG0] = false;
	this->_srcBlendEnable[GPULayerID_BG1] = false;
	this->_srcBlendEnable[GPULayerID_BG2] = false;
	this->_srcBlendEnable[GPULayerID_BG3] = false;
	this->_srcBlendEnable[GPULayerID_OBJ] = false;
	this->_srcBlendEnable[GPULayerID_Backdrop] = false;
	
	this->_dstBlendEnable[GPULayerID_BG0] = false;
	this->_dstBlendEnable[GPULayerID_BG1] = false;
	this->_dstBlendEnable[GPULayerID_BG2] = false;
	this->_dstBlendEnable[GPULayerID_BG3] = false;
	this->_dstBlendEnable[GPULayerID_OBJ] = false;
	this->_dstBlendEnable[GPULayerID_Backdrop] = false;
	
#ifdef ENABLE_SSE2
	this->_srcBlendEnable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	this->_srcBlendEnable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	this->_srcBlendEnable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	this->_srcBlendEnable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	this->_srcBlendEnable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	this->_srcBlendEnable_SSE2[GPULayerID_Backdrop] = _mm_setzero_si128();
#ifdef ENABLE_SSSE3
	this->_dstBlendEnable_SSSE3 = _mm_setzero_si128();
#else
	this->_dstBlendEnable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	this->_dstBlendEnable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	this->_dstBlendEnable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	this->_dstBlendEnable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	this->_dstBlendEnable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	this->_dstBlendEnable_SSE2[GPULayerID_Backdrop] = _mm_setzero_si128();
#endif
#endif
	
	this->_WIN0_enable[GPULayerID_BG0] = 0;
	this->_WIN0_enable[GPULayerID_BG1] = 0;
	this->_WIN0_enable[GPULayerID_BG2] = 0;
	this->_WIN0_enable[GPULayerID_BG3] = 0;
	this->_WIN0_enable[GPULayerID_OBJ] = 0;
	this->_WIN0_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
	this->_WIN1_enable[GPULayerID_BG0] = 0;
	this->_WIN1_enable[GPULayerID_BG1] = 0;
	this->_WIN1_enable[GPULayerID_BG2] = 0;
	this->_WIN1_enable[GPULayerID_BG3] = 0;
	this->_WIN1_enable[GPULayerID_OBJ] = 0;
	this->_WIN1_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
	this->_WINOUT_enable[GPULayerID_BG0] = 0;
	this->_WINOUT_enable[GPULayerID_BG1] = 0;
	this->_WINOUT_enable[GPULayerID_BG2] = 0;
	this->_WINOUT_enable[GPULayerID_BG3] = 0;
	this->_WINOUT_enable[GPULayerID_OBJ] = 0;
	this->_WINOUT_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
	this->_WINOBJ_enable[GPULayerID_BG0] = 0;
	this->_WINOBJ_enable[GPULayerID_BG1] = 0;
	this->_WINOBJ_enable[GPULayerID_BG2] = 0;
	this->_WINOBJ_enable[GPULayerID_BG3] = 0;
	this->_WINOBJ_enable[GPULayerID_OBJ] = 0;
	this->_WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG] = 0;
	
#if defined(ENABLE_SSE2)
	this->_WIN0_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	this->_WIN0_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	this->_WIN0_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	this->_WIN0_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	this->_WIN0_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	this->_WIN0_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
	
	this->_WIN1_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	this->_WIN1_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	this->_WIN1_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	this->_WIN1_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	this->_WIN1_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	this->_WIN1_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
	
	this->_WINOUT_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	this->_WINOUT_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	this->_WINOUT_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	this->_WINOUT_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	this->_WINOUT_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	this->_WINOUT_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
	
	this->_WINOBJ_enable_SSE2[GPULayerID_BG0] = _mm_setzero_si128();
	this->_WINOBJ_enable_SSE2[GPULayerID_BG1] = _mm_setzero_si128();
	this->_WINOBJ_enable_SSE2[GPULayerID_BG2] = _mm_setzero_si128();
	this->_WINOBJ_enable_SSE2[GPULayerID_BG3] = _mm_setzero_si128();
	this->_WINOBJ_enable_SSE2[GPULayerID_OBJ] = _mm_setzero_si128();
	this->_WINOBJ_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_setzero_si128();
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
	
	GPUEngineCompositorInfo &compState = this->_currentCompositorState;
	compState.lineIndexNative = 0;
	compState.lineIndexCustom = 0;
	compState.lineWidthCustom = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compState.lineRenderCount = 1;
	compState.linePixelCount = compState.lineWidthCustom * compState.lineRenderCount;
	compState.blockOffsetNative = compState.lineIndexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compState.blockOffsetCustom = compState.lineIndexCustom * compState.lineWidthCustom;
	
	compState.selectedLayerID = GPULayerID_BG0;
	compState.selectedBGLayer = &this->_BGLayer[GPULayerID_BG0];
	compState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	compState.colorEffect = (ColorEffect)this->_IORegisterMap->BLDCNT.ColorEffect;
	compState.blendEVA = this->_BLDALPHA_EVA;
	compState.blendEVB = this->_BLDALPHA_EVB;
	compState.blendEVY = this->_BLDALPHA_EVY;
	compState.blendTable555 = this->_selectedBlendTable555;
	compState.brightnessUpTable555 = this->_selectedBrightnessUpTable555;
	compState.brightnessUpTable666 = this->_selectedBrightnessUpTable666;
	compState.brightnessUpTable888 = this->_selectedBrightnessUpTable888;
	compState.brightnessDownTable555 = this->_selectedBrightnessDownTable555;
	compState.brightnessDownTable666 = this->_selectedBrightnessDownTable666;
	compState.brightnessDownTable888 = this->_selectedBrightnessDownTable888;
	
	compState.lineColorHeadNative = this->_internalRenderLineTargetNative;
	compState.lineColorHeadCustom = this->_internalRenderLineTargetCustom;
	compState.lineColorHead = compState.lineColorHeadNative;
	compState.lineLayerIDHeadNative = this->_renderLineLayerIDNative;
	compState.lineLayerIDHeadCustom = this->_renderLineLayerIDCustom;
	compState.lineLayerIDHead = compState.lineLayerIDHeadNative;
	
	compState.xNative = 0;
	compState.xCustom = 0;
	compState.lineColorTarget = (void **)&compState.lineColorTarget16;
	compState.lineColorTarget16 = (u16 *)compState.lineColorHeadNative;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHeadNative;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
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
	u16 ra =  colA        & 0x001F;
	u16 ga = (colA >>  5) & 0x001F;
	u16 ba = (colA >> 10) & 0x001F;
	u16 rb =  colB        & 0x001F;
	u16 gb = (colB >>  5) & 0x001F;
	u16 bb = (colB >> 10) & 0x001F;
	
	ra = ( (ra * blendEVA) + (rb * blendEVB) ) / 16;
	ga = ( (ga * blendEVA) + (gb * blendEVB) ) / 16;
	ba = ( (ba * blendEVA) + (bb * blendEVB) ) / 16;
	
	ra = (ra > 31) ? 31 : ra;
	ga = (ga > 31) ? 31 : ga;
	ba = (ba > 31) ? 31 : ba;
	
	return ra | (ga << 5) | (ba << 10);
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectBlend(const FragmentColor colA, const FragmentColor colB, const u16 blendEVA, const u16 blendEVB)
{
	FragmentColor outColor;
	
	u16 r16 = ( (colA.r * blendEVA) + (colB.r * blendEVB) ) / 16;
	u16 g16 = ( (colA.g * blendEVA) + (colB.g * blendEVB) ) / 16;
	u16 b16 = ( (colA.b * blendEVA) + (colB.b * blendEVB) ) / 16;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		outColor.r = (r16 > 63) ? 63 : r16;
		outColor.g = (g16 > 63) ? 63 : g16;
		outColor.b = (b16 > 63) ? 63 : b16;
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		outColor.r = (r16 > 255) ? 255 : r16;
		outColor.g = (g16 > 255) ? 255 : g16;
		outColor.b = (b16 > 255) ? 255 : b16;
	}
	
	outColor.a = 0;
	return outColor;
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
	const u16 alpha = colA.a + 1;
	COLOR c2;
	COLOR cfinal;
	
	c2.val = colB;
	
	cfinal.bits.red   = ((colA.r * alpha) + ((c2.bits.red   << 1) * (32 - alpha))) >> 6;
	cfinal.bits.green = ((colA.g * alpha) + ((c2.bits.green << 1) * (32 - alpha))) >> 6;
	cfinal.bits.blue  = ((colA.b * alpha) + ((c2.bits.blue  << 1) * (32 - alpha))) >> 6;
	cfinal.bits.alpha = 0;
	
	return cfinal.val;
}

template <NDSColorFormat COLORFORMATB>
FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectBlend3D(const FragmentColor colA, const FragmentColor colB)
{
	FragmentColor blendedColor;
	const u16 alpha = colA.a + 1;
	
	if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
	{
		blendedColor.r = ((colA.r * alpha) + (colB.r * (32 - alpha))) >> 5;
		blendedColor.g = ((colA.g * alpha) + (colB.g * (32 - alpha))) >> 5;
		blendedColor.b = ((colA.b * alpha) + (colB.b * (32 - alpha))) >> 5;
	}
	else if (COLORFORMATB == NDSColorFormat_BGR888_Rev)
	{
		blendedColor.r = ((colA.r * alpha) + (colB.r * (256 - alpha))) >> 8;
		blendedColor.g = ((colA.g * alpha) + (colB.g * (256 - alpha))) >> 8;
		blendedColor.b = ((colA.b * alpha) + (colB.b * (256 - alpha))) >> 8;
	}
	
	blendedColor.a = 0;
	return blendedColor;
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectIncreaseBrightness(const u16 col, const u16 blendEVY)
{
	u16 r =  col        & 0x001F;
	u16 g = (col >>  5) & 0x001F;
	u16 b = (col >> 10) & 0x001F;
	
	r = (r + ((31 - r) * blendEVY / 16));
	g = (g + ((31 - g) * blendEVY / 16));
	b = (b + ((31 - b) * blendEVY / 16));
	
	return r | (g << 5) | (b << 10);
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectIncreaseBrightness(const FragmentColor col, const u16 blendEVY)
{
	FragmentColor newColor;
	newColor.color = 0;
	
	u32 r = col.r;
	u32 g = col.g;
	u32 b = col.b;
	
	if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
	{
		newColor.r = (r + ((63 - r) * blendEVY / 16));
		newColor.g = (g + ((63 - g) * blendEVY / 16));
		newColor.b = (b + ((63 - b) * blendEVY / 16));
	}
	else if (COLORFORMAT == NDSColorFormat_BGR888_Rev)
	{
		newColor.r = (r + ((255 - r) * blendEVY / 16));
		newColor.g = (g + ((255 - g) * blendEVY / 16));
		newColor.b = (b + ((255 - b) * blendEVY / 16));
	}
	
	return newColor;
}

FORCEINLINE u16 GPUEngineBase::_ColorEffectDecreaseBrightness(const u16 col, const u16 blendEVY)
{
	u16 r =  col        & 0x001F;
	u16 g = (col >>  5) & 0x001F;
	u16 b = (col >> 10) & 0x001F;
	
	r = (r - (r * blendEVY / 16));
	g = (g - (g * blendEVY / 16));
	b = (b - (b * blendEVY / 16));
	
	return r | (g << 5) | (b << 10);
}

FORCEINLINE FragmentColor GPUEngineBase::_ColorEffectDecreaseBrightness(const FragmentColor col, const u16 blendEVY)
{
	FragmentColor newColor;
	newColor.color = 0;
	
	u32 r = col.r;
	u32 g = col.g;
	u32 b = col.b;
	
	newColor.r = (r - (r * blendEVY / 16));
	newColor.g = (g - (g * blendEVY / 16));
	newColor.b = (b - (b * blendEVY / 16));
	
	return newColor;
}

#ifdef ENABLE_SSE2

template <NDSColorFormat COLORFORMAT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectIncreaseBrightness(const __m128i &col, const __m128i &blendEVY)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i r_vec128 = _mm_and_si128(                col,      _mm_set1_epi16(0x001F) );
		__m128i g_vec128 = _mm_and_si128( _mm_srli_epi16(col,  5), _mm_set1_epi16(0x001F) );
		__m128i b_vec128 = _mm_and_si128( _mm_srli_epi16(col, 10), _mm_set1_epi16(0x001F) );
		
		r_vec128 = _mm_add_epi16( r_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), r_vec128), blendEVY), 4) );
		g_vec128 = _mm_add_epi16( g_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), g_vec128), blendEVY), 4) );
		b_vec128 = _mm_add_epi16( b_vec128, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16(31), b_vec128), blendEVY), 4) );
		
		return _mm_or_si128(r_vec128, _mm_or_si128( _mm_slli_epi16(g_vec128, 5), _mm_slli_epi16(b_vec128, 10)) );
	}
	else
	{
		__m128i rgbLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
		__m128i rgbHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
		
		rgbLo = _mm_add_epi16( rgbLo, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbLo), blendEVY), 4) );
		rgbHi = _mm_add_epi16( rgbHi, _mm_srli_epi16(_mm_mullo_epi16(_mm_sub_epi16(_mm_set1_epi16((COLORFORMAT == NDSColorFormat_BGR666_Rev) ? 63 : 255), rgbHi), blendEVY), 4) );
		
		return _mm_and_si128( _mm_packus_epi16(rgbLo, rgbHi), _mm_set1_epi32(0x00FFFFFF) );
	}
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectDecreaseBrightness(const __m128i &col, const __m128i &blendEVY)
{
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i r_vec128 = _mm_and_si128(                col,      _mm_set1_epi16(0x001F) );
		__m128i g_vec128 = _mm_and_si128( _mm_srli_epi16(col,  5), _mm_set1_epi16(0x001F) );
		__m128i b_vec128 = _mm_and_si128( _mm_srli_epi16(col, 10), _mm_set1_epi16(0x001F) );
		
		r_vec128 = _mm_sub_epi16( r_vec128, _mm_srli_epi16(_mm_mullo_epi16(r_vec128, blendEVY), 4) );
		g_vec128 = _mm_sub_epi16( g_vec128, _mm_srli_epi16(_mm_mullo_epi16(g_vec128, blendEVY), 4) );
		b_vec128 = _mm_sub_epi16( b_vec128, _mm_srli_epi16(_mm_mullo_epi16(b_vec128, blendEVY), 4) );
		
		return _mm_or_si128(r_vec128, _mm_or_si128( _mm_slli_epi16(g_vec128, 5), _mm_slli_epi16(b_vec128, 10)) );
	}
	else
	{
		__m128i rgbLo = _mm_unpacklo_epi8(col, _mm_setzero_si128());
		__m128i rgbHi = _mm_unpackhi_epi8(col, _mm_setzero_si128());
		
		rgbLo = _mm_sub_epi16( rgbLo, _mm_srli_epi16(_mm_mullo_epi16(rgbLo, blendEVY), 4) );
		rgbHi = _mm_sub_epi16( rgbHi, _mm_srli_epi16(_mm_mullo_epi16(rgbHi, blendEVY), 4) );
		
		return _mm_and_si128( _mm_packus_epi16(rgbLo, rgbHi), _mm_set1_epi32(0x00FFFFFF) );
	}
}

template <NDSColorFormat COLORFORMAT>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend(const __m128i &colA, const __m128i &colB, const __m128i &blendEVA, const __m128i &blendEVB)
{
#ifdef ENABLE_SSSE3
	__m128i blendAB = _mm_or_si128(blendEVA, _mm_slli_epi16(blendEVB, 8));
#endif
	
	if (COLORFORMAT == NDSColorFormat_BGR555_Rev)
	{
		__m128i ra;
		__m128i ga;
		__m128i ba;
		__m128i colorBitMask = _mm_set1_epi16(0x001F);
		
#ifdef ENABLE_SSSE3
		ra = _mm_or_si128( _mm_and_si128(               colA,      colorBitMask), _mm_and_si128(_mm_slli_epi16(colB, 8), _mm_set1_epi16(0x1F00)) );
		ga = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(colA,  5), colorBitMask), _mm_and_si128(_mm_slli_epi16(colB, 3), _mm_set1_epi16(0x1F00)) );
		ba = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(colA, 10), colorBitMask), _mm_and_si128(_mm_srli_epi16(colB, 2), _mm_set1_epi16(0x1F00)) );
		
		ra = _mm_maddubs_epi16(ra, blendAB);
		ga = _mm_maddubs_epi16(ga, blendAB);
		ba = _mm_maddubs_epi16(ba, blendAB);
#else
		ra = _mm_and_si128(               colA,      colorBitMask);
		ga = _mm_and_si128(_mm_srli_epi16(colA,  5), colorBitMask);
		ba = _mm_and_si128(_mm_srli_epi16(colA, 10), colorBitMask);
		
		__m128i rb = _mm_and_si128(               colB,      colorBitMask);
		__m128i gb = _mm_and_si128(_mm_srli_epi16(colB,  5), colorBitMask);
		__m128i bb = _mm_and_si128(_mm_srli_epi16(colB, 10), colorBitMask);
		
		ra = _mm_add_epi16( _mm_mullo_epi16(ra, blendEVA), _mm_mullo_epi16(rb, blendEVB) );
		ga = _mm_add_epi16( _mm_mullo_epi16(ga, blendEVA), _mm_mullo_epi16(gb, blendEVB) );
		ba = _mm_add_epi16( _mm_mullo_epi16(ba, blendEVA), _mm_mullo_epi16(bb, blendEVB) );
#endif
		
		ra = _mm_srli_epi16(ra, 4);
		ga = _mm_srli_epi16(ga, 4);
		ba = _mm_srli_epi16(ba, 4);
		
		ra = _mm_min_epi16(ra, colorBitMask);
		ga = _mm_min_epi16(ga, colorBitMask);
		ba = _mm_min_epi16(ba, colorBitMask);
		
		return _mm_or_si128(ra, _mm_or_si128( _mm_slli_epi16(ga, 5), _mm_slli_epi16(ba, 10)) );
	}
	else
	{
		__m128i outColorLo;
		__m128i outColorHi;
		__m128i outColor;
		
#ifdef ENABLE_SSSE3
		outColorLo = _mm_unpacklo_epi8(colA, colB);
		outColorHi = _mm_unpackhi_epi8(colA, colB);
		
		outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
		outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
#else
		__m128i colALo = _mm_unpacklo_epi8(colA, _mm_setzero_si128());
		__m128i colAHi = _mm_unpackhi_epi8(colA, _mm_setzero_si128());
		__m128i colBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
		__m128i colBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
		
		outColorLo = _mm_add_epi16( _mm_mullo_epi16(colALo, blendEVA), _mm_mullo_epi16(colBLo, blendEVB) );
		outColorHi = _mm_add_epi16( _mm_mullo_epi16(colAHi, blendEVA), _mm_mullo_epi16(colBHi, blendEVB) );
#endif
		
		outColorLo = _mm_srli_epi16(outColorLo, 4);
		outColorHi = _mm_srli_epi16(outColorHi, 4);
		outColor = _mm_packus_epi16(outColorLo, outColorHi);
		
		// When the color format is 888, the packuswb instruction will naturally clamp
		// the color component values to 255. However, when the color format is 666, the
		// color component values must be clamped to 63. In this case, we must call pminub
		// to do the clamp.
		if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
		{
			outColor = _mm_min_epu8(outColor, _mm_set1_epi8(63));
		}
		
		outColor = _mm_and_si128(outColor, _mm_set1_epi32(0x00FFFFFF));
		
		return outColor;
	}
}

template <NDSColorFormat COLORFORMATB>
FORCEINLINE __m128i GPUEngineBase::_ColorEffectBlend3D(const __m128i &colA_Lo, const __m128i &colA_Hi, const __m128i &colB)
{
	if (COLORFORMATB == NDSColorFormat_BGR555_Rev)
	{
		// If the color format of B is 555, then the colA_Hi parameter is required.
		// The color format of A is assumed to be RGB666.
		__m128i ra_lo = _mm_and_si128(                colA_Lo,      _mm_set1_epi32(0x000000FF) );
		__m128i ga_lo = _mm_and_si128( _mm_srli_epi32(colA_Lo,  8), _mm_set1_epi32(0x000000FF) );
		__m128i ba_lo = _mm_and_si128( _mm_srli_epi32(colA_Lo, 16), _mm_set1_epi32(0x000000FF) );
		__m128i aa_lo =                _mm_srli_epi32(colA_Lo, 24);
		
		__m128i ra_hi = _mm_and_si128(                colA_Hi,      _mm_set1_epi32(0x000000FF) );
		__m128i ga_hi = _mm_and_si128( _mm_srli_epi32(colA_Hi,  8), _mm_set1_epi32(0x000000FF) );
		__m128i ba_hi = _mm_and_si128( _mm_srli_epi32(colA_Hi, 16), _mm_set1_epi32(0x000000FF) );
		__m128i aa_hi =                _mm_srli_epi32(colA_Hi, 24);
		
		__m128i ra = _mm_packs_epi32(ra_lo, ra_hi);
		__m128i ga = _mm_packs_epi32(ga_lo, ga_hi);
		__m128i ba = _mm_packs_epi32(ba_lo, ba_hi);
		__m128i aa = _mm_packs_epi32(aa_lo, aa_hi);
		
#ifdef ENABLE_SSSE3
		ra = _mm_or_si128( ra, _mm_and_si128(_mm_slli_epi16(colB, 9), _mm_set1_epi16(0x3E00)) );
		ga = _mm_or_si128( ga, _mm_and_si128(_mm_slli_epi16(colB, 4), _mm_set1_epi16(0x3E00)) );
		ba = _mm_or_si128( ba, _mm_and_si128(_mm_srli_epi16(colB, 1), _mm_set1_epi16(0x3E00)) );
		
		aa = _mm_adds_epu8(aa, _mm_set1_epi16(1));
		aa = _mm_or_si128( aa, _mm_slli_epi16(_mm_subs_epu16(_mm_set1_epi8(32), aa), 8) );
		
		ra = _mm_maddubs_epi16(ra, aa);
		ga = _mm_maddubs_epi16(ga, aa);
		ba = _mm_maddubs_epi16(ba, aa);
#else
		aa = _mm_adds_epu16(aa, _mm_set1_epi16(1));
		__m128i rb = _mm_and_si128( _mm_slli_epi16(colB, 1), _mm_set1_epi16(0x003E) );
		__m128i gb = _mm_and_si128( _mm_srli_epi16(colB, 4), _mm_set1_epi16(0x003E) );
		__m128i bb = _mm_and_si128( _mm_srli_epi16(colB, 9), _mm_set1_epi16(0x003E) );
		__m128i ab = _mm_subs_epu16( _mm_set1_epi16(32), aa );
		
		ra = _mm_add_epi16( _mm_mullo_epi16(ra, aa), _mm_mullo_epi16(rb, ab) );
		ga = _mm_add_epi16( _mm_mullo_epi16(ga, aa), _mm_mullo_epi16(gb, ab) );
		ba = _mm_add_epi16( _mm_mullo_epi16(ba, aa), _mm_mullo_epi16(bb, ab) );
#endif
		
		ra = _mm_srli_epi16(ra, 6);
		ga = _mm_srli_epi16(ga, 6);
		ba = _mm_srli_epi16(ba, 6);
		
		return _mm_or_si128( _mm_or_si128(ra, _mm_slli_epi16(ga, 5)), _mm_slli_epi16(ba, 10) );
	}
	else
	{
		// If the color format of B is 666 or 888, then the colA_Hi parameter is ignored.
		// The color format of A is assumed to match the color format of B.
		__m128i rgbALo;
		__m128i rgbAHi;
		
#ifdef ENABLE_SSSE3
		if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
		{
			// Does not work for RGBA8888 color format. The reason is because this
			// algorithm depends on the pmaddubsw instruction, which multiplies
			// two unsigned 8-bit integers into an intermediate signed 16-bit
			// integer. This means that we can overrun the signed 16-bit value
			// range, which would be limited to [-32767 - 32767]. For example, a
			// color component of value 255 multiplied by an alpha value of 255
			// would equal 65025, which is greater than the upper range of a signed
			// 16-bit value.
			rgbALo = _mm_unpacklo_epi8(colA_Lo, colB);
			rgbAHi = _mm_unpackhi_epi8(colA_Lo, colB);
			
			__m128i alpha = _mm_and_si128( _mm_srli_epi32(colA_Lo, 24), _mm_set1_epi32(0x0000001F) );
			alpha = _mm_or_si128( alpha, _mm_or_si128(_mm_slli_epi32(alpha, 8), _mm_slli_epi32(alpha, 16)) );
			alpha = _mm_adds_epu8(alpha, _mm_set1_epi8(1));
			
			__m128i invAlpha = _mm_subs_epu8(_mm_set1_epi8(32), alpha);
			__m128i alphaLo = _mm_unpacklo_epi8(alpha, invAlpha);
			__m128i alphaHi = _mm_unpackhi_epi8(alpha, invAlpha);
			
			rgbALo = _mm_maddubs_epi16(rgbALo, alphaLo);
			rgbAHi = _mm_maddubs_epi16(rgbAHi, alphaHi);
		}
		else
#endif
		{
			rgbALo = _mm_unpacklo_epi8(colA_Lo, _mm_setzero_si128());
			rgbAHi = _mm_unpackhi_epi8(colA_Lo, _mm_setzero_si128());
			__m128i rgbBLo = _mm_unpacklo_epi8(colB, _mm_setzero_si128());
			__m128i rgbBHi = _mm_unpackhi_epi8(colB, _mm_setzero_si128());
			
			__m128i alpha = _mm_and_si128( _mm_srli_epi32(colA_Lo, 24), _mm_set1_epi32(0x000000FF) );
			alpha = _mm_or_si128( alpha, _mm_or_si128(_mm_slli_epi32(alpha, 8), _mm_slli_epi32(alpha, 16)) );
			
			__m128i alphaLo = _mm_unpacklo_epi8(alpha, _mm_setzero_si128());
			__m128i alphaHi = _mm_unpackhi_epi8(alpha, _mm_setzero_si128());
			alphaLo = _mm_add_epi16(alphaLo, _mm_set1_epi16(1));
			alphaHi = _mm_add_epi16(alphaHi, _mm_set1_epi16(1));
			
			if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
			{
				rgbALo = _mm_add_epi16( _mm_mullo_epi16(rgbALo, alphaLo), _mm_mullo_epi16(rgbBLo, _mm_sub_epi16(_mm_set1_epi16(32), alphaLo)) );
				rgbAHi = _mm_add_epi16( _mm_mullo_epi16(rgbAHi, alphaHi), _mm_mullo_epi16(rgbBHi, _mm_sub_epi16(_mm_set1_epi16(32), alphaHi)) );
			}
			else if (COLORFORMATB == NDSColorFormat_BGR888_Rev)
			{
				rgbALo = _mm_add_epi16( _mm_mullo_epi16(rgbALo, alphaLo), _mm_mullo_epi16(rgbBLo, _mm_sub_epi16(_mm_set1_epi16(256), alphaLo)) );
				rgbAHi = _mm_add_epi16( _mm_mullo_epi16(rgbAHi, alphaHi), _mm_mullo_epi16(rgbBHi, _mm_sub_epi16(_mm_set1_epi16(256), alphaHi)) );
			}
		}
		
		if (COLORFORMATB == NDSColorFormat_BGR666_Rev)
		{
			rgbALo = _mm_srli_epi16(rgbALo, 5);
			rgbAHi = _mm_srli_epi16(rgbAHi, 5);
		}
		else if (COLORFORMATB == NDSColorFormat_BGR888_Rev)
		{
			rgbALo = _mm_srli_epi16(rgbALo, 8);
			rgbAHi = _mm_srli_epi16(rgbAHi, 8);
		}
		
		return _mm_and_si128( _mm_packus_epi16(rgbALo, rgbAHi), _mm_set1_epi32(0x00FFFFFF) );
	}
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
void GPUEngineBase::ParseReg_DISPCNT()
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->_displayOutputMode = (this->_engineID == GPUEngineID_Main) ? (GPUDisplayMode)DISPCNT.DisplayMode : (GPUDisplayMode)(DISPCNT.DisplayMode & 0x01);
	
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
     
	if (DISPCNT.OBJ_BMP_1D_Bound && (this->_engineID == GPUEngineID_Main))
		this->_sprBMPBoundary = 8;
	else
		this->_sprBMPBoundary = 7;
	
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

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_RenderLine_Clear(GPUEngineCompositorInfo &compState)
{
	// Clear the current line with the clear color
	u16 dstClearColor16 = compState.backdropColor16;
	
	if (this->_srcBlendEnable[GPULayerID_Backdrop])
	{
		if (compState.colorEffect == ColorEffect_IncreaseBrightness)
		{
			dstClearColor16 = compState.brightnessUpTable555[compState.backdropColor16];
		}
		else if (compState.colorEffect == ColorEffect_DecreaseBrightness)
		{
			dstClearColor16 = compState.brightnessDownTable555[compState.backdropColor16];
		}
	}
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(compState.lineColorTarget16, dstClearColor16);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(compState.lineColorTarget32, COLOR555TO666(dstClearColor16));
			break;
			
		case NDSColorFormat_BGR888_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(compState.lineColorTarget32, COLOR555TO888(dstClearColor16));
			break;
	}
	
	memset(this->_renderLineLayerIDNative, GPULayerID_Backdrop, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprWin, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	// init pixels priorities
	assert(NB_PRIORITIES == 4);
	this->_itemsForPriority[0].nbPixelsX = 0;
	this->_itemsForPriority[1].nbPixelsX = 0;
	this->_itemsForPriority[2].nbPixelsX = 0;
	this->_itemsForPriority[3].nbPixelsX = 0;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::RenderLine(const u16 l)
{
	this->_currentCompositorState.lineIndexNative = l;
	this->_currentCompositorState.lineIndexCustom = _gpuDstLineIndex[l];
	
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

template <bool NATIVEDST, bool NATIVESRC, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t PIXELBYTES>
void GPUEngineBase::_LineColorCopy(void *__restrict dstBuffer, const void *__restrict srcBuffer, const size_t l)
{
	if (NATIVEDST && NATIVESRC)
	{
		void *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * PIXELBYTES) : (u8 *)dstBuffer;
		const void *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * PIXELBYTES) : (u8 *)srcBuffer;
		
#if defined(ENABLE_SSE2)
		MACRODO_N( GPU_FRAMEBUFFER_NATIVE_WIDTH / (sizeof(__m128i) / PIXELBYTES), _mm_stream_si128((__m128i *)dst + (X), _mm_load_si128((__m128i *)src + (X))) );
#elif LOCAL_LE
		memcpy(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * PIXELBYTES);
#else
		if (NEEDENDIANSWAP)
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
			{
				if (PIXELBYTES == 2)
				{
					((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
				}
				else if (PIXELBYTES == 4)
				{
					((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
				}
			}
		}
		else
		{
			memcpy(dst, src, GPU_FRAMEBUFFER_NATIVE_WIDTH * PIXELBYTES);
		}
#endif
	}
	else if (!NATIVEDST && !NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineIndex = _gpuCaptureLineIndex[l];
		const size_t lineCount = _gpuCaptureLineCount[l];
		
		void *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (lineIndex * lineWidth * PIXELBYTES) : (u8 *)dstBuffer;
		const void *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (lineIndex * lineWidth * PIXELBYTES) : (u8 *)srcBuffer;
		
#if defined(LOCAL_LE)
		memcpy(dst, src, lineWidth * lineCount * PIXELBYTES);
#else
		if (NEEDENDIANSWAP)
		{
			for (size_t i = 0; i < lineWidth * lineCount; i++)
			{
				if (PIXELBYTES == 2)
				{
					((u16 *)dst)[i] = LE_TO_LOCAL_16( ((u16 *)src)[i] );
				}
				else if (PIXELBYTES == 4)
				{
					((u32 *)dst)[i] = LE_TO_LOCAL_32( ((u32 *)src)[i] );
				}
			}
		}
		else
		{
			memcpy(dst, src, lineWidth * lineCount * PIXELBYTES);
		}
#endif
	}
	else if (NATIVEDST && !NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineIndex = _gpuCaptureLineIndex[l];
		
		void *__restrict dst = (USELINEINDEX) ? (u8 *)dstBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * PIXELBYTES) : (u8 *)dstBuffer;
		const void *__restrict src = (USELINEINDEX) ? (u8 *)srcBuffer + (lineIndex * lineWidth * PIXELBYTES) : (u8 *)srcBuffer;
		
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			if (PIXELBYTES == 2)
			{
				((u16 *)dst)[i] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[_gpuDstPitchIndex[i]] ) : ((u16 *)src)[_gpuDstPitchIndex[i]];
			}
			else if (PIXELBYTES == 4)
			{
				((u32 *)dst)[i] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[_gpuDstPitchIndex[i]] ) : ((u32 *)src)[_gpuDstPitchIndex[i]];
			}
		}
	}
	else // (!NATIVEDST && NATIVESRC)
	{
		const size_t lineWidth = GPU->GetDisplayInfo().customWidth;
		const size_t lineIndex = _gpuCaptureLineIndex[l];
		const size_t lineCount = _gpuCaptureLineCount[l];
		
		u8 *dstLinePtr = (USELINEINDEX) ? (u8 *)dstBuffer + (lineIndex * lineWidth * PIXELBYTES) : (u8 *)dstBuffer;
		u8 *dst = dstLinePtr;
		const void *src = (USELINEINDEX) ? (u8 *)srcBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH * PIXELBYTES) : (u8 *)srcBuffer;
		
		for (size_t x = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x++)
		{
			for (size_t p = 0; p < _gpuDstPitchCount[x]; p++)
			{
				if (PIXELBYTES == 2)
				{
					((u16 *)dst)[_gpuDstPitchIndex[x] + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_16( ((u16 *)src)[x] ) : ((u16 *)src)[x];
				}
				else if (PIXELBYTES == 4)
				{
					((u32 *)dst)[_gpuDstPitchIndex[x] + p] = (NEEDENDIANSWAP) ? LE_TO_LOCAL_32( ((u32 *)src)[x] ) : ((u32 *)src)[x];
				}
			}
		}
		
		dst = dstLinePtr + (lineWidth * PIXELBYTES);
		
		for (size_t line = 1; line < lineCount; line++)
		{
			memcpy(dst, dstLinePtr, lineWidth * PIXELBYTES);
			dst += (lineWidth * PIXELBYTES);
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
		
		ExpandLine8(dstBuffer, srcBuffer, lineWidth);
		
		u8 *__restrict dstLineInc = dstBuffer + lineWidth;
		for (size_t line = 1; line < lineCount; line++)
		{
			memcpy(dstLineInc, dstBuffer, lineWidth * sizeof(u8));
			dstLineInc += lineWidth;
		}
	}
}

/*****************************************************************************/
//			PIXEL RENDERING
/*****************************************************************************/
template <NDSColorFormat OUTPUTFORMAT, bool ISSRCLAYEROBJ, bool ISDEBUGRENDER, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT>
FORCEINLINE void GPUEngineBase::_RenderPixel(GPUEngineCompositorInfo &compState, const u16 srcColor16, const u8 srcAlpha)
{
	u16 &dstColor16 = *compState.lineColorTarget16;
	FragmentColor &dstColor32 = *compState.lineColorTarget32;
	u8 &dstLayerID = *compState.lineLayerIDTarget;
	
	if (ISDEBUGRENDER)
	{
		// If we're rendering pixels to a debugging context, then assume that the pixel
		// always passes the window test and that the color effect is always disabled.
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				dstColor16 = srcColor16 | 0x8000;
				break;
				
			case NDSColorFormat_BGR666_Rev:
				dstColor32.color = ConvertColor555To6665Opaque<false>(srcColor16);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				dstColor32.color = ConvertColor555To8888Opaque<false>(srcColor16);
				break;
		}
		
		return;
	}
	
	if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestNative[compState.selectedLayerID][compState.xNative] == 0) )
	{
		return;
	}
	
	const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectNative[compState.selectedLayerID][compState.xNative] != 0) : true;
	
	if (!ISSRCLAYEROBJ && COLOREFFECTDISABLEDHINT)
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				dstColor16 = srcColor16 | 0x8000;
				break;
				
			case NDSColorFormat_BGR666_Rev:
				dstColor32.color = ConvertColor555To6665Opaque<false>(srcColor16);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				dstColor32.color = ConvertColor555To8888Opaque<false>(srcColor16);
				break;
		}
		
		dstLayerID = compState.selectedLayerID;
		return;
	}
	
	ColorEffect selectedEffect = ColorEffect_Disable;
	TBlendTable *selectedBlendTable = compState.blendTable555;
	u8 blendEVA = compState.blendEVA;
	u8 blendEVB = compState.blendEVB;
	
	if (enableColorEffect)
	{
		const bool dstEffectEnable = (dstLayerID != compState.selectedLayerID) && this->_dstBlendEnable[dstLayerID];
		
		// Select the color effect based on the BLDCNT target flags.
		bool forceBlendEffect = false;
		
		if (ISSRCLAYEROBJ)
		{
			//translucent-capable OBJ are forcing the function to blend when the second target is satisfied
			const OBJMode objMode = (OBJMode)this->_sprType[compState.xNative];
			const bool isObjTranslucentType = (objMode == OBJMode_Transparent) || (objMode == OBJMode_Bitmap);
			if (isObjTranslucentType && dstEffectEnable)
			{
				//obj without fine-grained alpha are using EVA/EVB for blending. this is signified by receiving 0xFF in the alpha
				//it's tested by the spriteblend demo and the glory of heracles title screen
				if (srcAlpha != 0xFF)
				{
					blendEVA = srcAlpha;
					blendEVB = 16 - srcAlpha;
					selectedBlendTable = &GPUEngineBase::_blendTable555[blendEVA][blendEVB];
				}
				
				forceBlendEffect = true;
			}
		}
		
		if (forceBlendEffect)
		{
			selectedEffect = ColorEffect_Blend;
		}
		else if (this->_srcBlendEnable[compState.selectedLayerID])
		{
			switch (compState.colorEffect)
			{
				// For the Blend effect, both first and second target flags must be checked.
				case ColorEffect_Blend:
				{
					if (dstEffectEnable) selectedEffect = compState.colorEffect;
					break;
				}
					
				// For the Increase/Decrease Brightness effects, only the first target flag needs to be checked.
				// Test case: Bomberman Land Touch! dialog boxes will render too dark without this check.
				case ColorEffect_IncreaseBrightness:
				case ColorEffect_DecreaseBrightness:
					selectedEffect = compState.colorEffect;
					break;
					
				default:
					break;
			}
		}
	}
	
	// Render the pixel using the selected color effect.
	switch (selectedEffect)
	{
		case ColorEffect_Disable:
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = srcColor16;
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					dstColor32.color = ConvertColor555To6665Opaque<false>(srcColor16);
					break;
					
				case NDSColorFormat_BGR888_Rev:
					dstColor32.color = ConvertColor555To8888Opaque<false>(srcColor16);
					break;
			}
			break;
		}
			
		case ColorEffect_IncreaseBrightness:
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = compState.brightnessUpTable555[srcColor16 & 0x7FFF];
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					dstColor32 = compState.brightnessUpTable666[srcColor16 & 0x7FFF];
					dstColor32.a = 0x1F;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					dstColor32 = compState.brightnessUpTable888[srcColor16 & 0x7FFF];
					dstColor32.a = 0xFF;
					break;
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = compState.brightnessDownTable555[srcColor16 & 0x7FFF];
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					dstColor32 = compState.brightnessDownTable666[srcColor16 & 0x7FFF];
					dstColor32.a = 0x1F;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					dstColor32 = compState.brightnessDownTable888[srcColor16 & 0x7FFF];
					dstColor32.a = 0xFF;
					break;
			}
			break;
		}
			
		case ColorEffect_Blend:
		{
			FragmentColor srcColor32;
			
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					dstColor16 = this->_ColorEffectBlend(srcColor16, dstColor16, selectedBlendTable);
					dstColor16 |= 0x8000;
					break;
					
				case NDSColorFormat_BGR666_Rev:
					srcColor32.color = ConvertColor555To6665Opaque<false>(srcColor16);
					dstColor32 = this->_ColorEffectBlend<OUTPUTFORMAT>(srcColor32, dstColor32, blendEVA, blendEVB);
					dstColor32.a = 0x1F;
					break;
					
				case NDSColorFormat_BGR888_Rev:
					srcColor32.color = ConvertColor555To8888Opaque<false>(srcColor16);
					dstColor32 = this->_ColorEffectBlend<OUTPUTFORMAT>(srcColor32, dstColor32, blendEVA, blendEVB);
					dstColor32.a = 0xFF;
					break;
			}
			break;
		}
	}
	
	dstLayerID = compState.selectedLayerID;
}

#ifdef ENABLE_SSE2

template <NDSColorFormat OUTPUTFORMAT, bool ISSRCLAYEROBJ, bool ISDEBUGRENDER, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
FORCEINLINE void GPUEngineBase::_RenderPixel16_SSE2(GPUEngineCompositorInfo &compState,
													const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
													const __m128i &srcAlpha,
													const __m128i &srcEffectEnableMask,
													__m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
													__m128i &dstLayerID,
													__m128i &passMask8)
{
	const __m128i srcLayerID_vec128 = _mm_set1_epi8(compState.selectedLayerID);
	__m128i passMask16[2]	= { _mm_unpacklo_epi8(passMask8, passMask8),
							    _mm_unpackhi_epi8(passMask8, passMask8) };
	
	__m128i passMask32[4] = { _mm_unpacklo_epi16(passMask16[0], passMask16[0]),
	                          _mm_unpackhi_epi16(passMask16[0], passMask16[0]),
	                          _mm_unpacklo_epi16(passMask16[1], passMask16[1]),
	                          _mm_unpackhi_epi16(passMask16[1], passMask16[1]) };
	
	if (ISDEBUGRENDER)
	{
		// If we're rendering pixels to a debugging context, then assume that the pixel
		// always passes the window test and that the color effect is always disabled.
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			const __m128i alphaBits = _mm_set1_epi16(0x8000);
			dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(src0, alphaBits), passMask16[0]);
			dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(src1, alphaBits), passMask16[1]);
		}
		else
		{
			const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
			dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(src0, alphaBits), passMask32[0]);
			dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(src1, alphaBits), passMask32[1]);
			dst2 = _mm_blendv_epi8(dst2, _mm_or_si128(src2, alphaBits), passMask32[2]);
			dst3 = _mm_blendv_epi8(dst3, _mm_or_si128(src3, alphaBits), passMask32[3]);
		}
		
		return;
	}
	
	__m128i enableColorEffectMask;
	
	if (WILLPERFORMWINDOWTEST)
	{
		// Do the window test.
		__m128i didPassWindowTest = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compState.selectedLayerID] + compState.xCustom)), _mm_set1_epi8(1) );
		passMask8 = _mm_and_si128(passMask8, didPassWindowTest);
		passMask16[0] = _mm_unpacklo_epi8(passMask8, passMask8);
		passMask16[1] = _mm_unpackhi_epi8(passMask8, passMask8);
		
		enableColorEffectMask = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_enableColorEffectCustom[compState.selectedLayerID] + compState.xCustom)), _mm_set1_epi8(1) );
	}
	else
	{
		enableColorEffectMask = _mm_set1_epi8(0xFF);
	}
	
	if ( (!ISSRCLAYEROBJ && COLOREFFECTDISABLEDHINT) || (_mm_movemask_epi8(srcEffectEnableMask) == 0) )
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			const __m128i alphaBits = _mm_set1_epi16(0x8000);
			dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(src0, alphaBits), passMask16[0]);
			dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(src1, alphaBits), passMask16[1]);
		}
		else
		{
			const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
			dst0 = _mm_blendv_epi8(dst0, _mm_or_si128(src0, alphaBits), passMask32[0]);
			dst1 = _mm_blendv_epi8(dst1, _mm_or_si128(src1, alphaBits), passMask32[1]);
			dst2 = _mm_blendv_epi8(dst2, _mm_or_si128(src2, alphaBits), passMask32[2]);
			dst3 = _mm_blendv_epi8(dst3, _mm_or_si128(src3, alphaBits), passMask32[3]);
		}
		
		dstLayerID = _mm_blendv_epi8(dstLayerID, srcLayerID_vec128, passMask8);
		return;
	}
	
	__m128i dstEffectEnableMask;
	
#ifdef ENABLE_SSSE3
	dstEffectEnableMask = _mm_shuffle_epi8(this->_dstBlendEnable_SSSE3, dstLayerID);
	dstEffectEnableMask = _mm_xor_si128( _mm_cmpeq_epi8(dstEffectEnableMask, _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF) );
#else
	dstEffectEnableMask =                                   _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0)), this->_dstBlendEnable_SSE2[GPULayerID_BG0]);
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG1)), this->_dstBlendEnable_SSE2[GPULayerID_BG1]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG2)), this->_dstBlendEnable_SSE2[GPULayerID_BG2]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG3)), this->_dstBlendEnable_SSE2[GPULayerID_BG3]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_OBJ)), this->_dstBlendEnable_SSE2[GPULayerID_OBJ]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_Backdrop)), this->_dstBlendEnable_SSE2[GPULayerID_Backdrop]) );
#endif
	
	dstEffectEnableMask = _mm_andnot_si128( _mm_cmpeq_epi8(dstLayerID, srcLayerID_vec128), dstEffectEnableMask );
	
	// Select the color effect based on the BLDCNT target flags.
	__m128i forceBlendEffectMask = _mm_setzero_si128();
	const __m128i colorEffect_vec128 = (WILLPERFORMWINDOWTEST) ? _mm_blendv_epi8(_mm_set1_epi8(ColorEffect_Disable), _mm_set1_epi8(compState.colorEffect), enableColorEffectMask) : _mm_set1_epi8(compState.colorEffect);
	
	__m128i eva_vec128 = _mm_set1_epi16(compState.blendEVA);
	__m128i evb_vec128 = _mm_set1_epi16(compState.blendEVB);
	const __m128i evy_vec128 = _mm_set1_epi16(compState.blendEVY);
	
	if (ISSRCLAYEROBJ)
	{
		const __m128i objMode_vec128 = _mm_loadu_si128((__m128i *)(this->_sprType + compState.xNative));
		const __m128i isObjTranslucentMask = _mm_and_si128( dstEffectEnableMask, _mm_or_si128(_mm_cmpeq_epi8(objMode_vec128, _mm_set1_epi8(OBJMode_Transparent)), _mm_cmpeq_epi8(objMode_vec128, _mm_set1_epi8(OBJMode_Bitmap))) );
		forceBlendEffectMask = isObjTranslucentMask;
		
		const __m128i srcAlphaMask = _mm_andnot_si128(_mm_cmpeq_epi8(srcAlpha, _mm_set1_epi8(0xFF)), isObjTranslucentMask);
		
		eva_vec128 = _mm_blendv_epi8(eva_vec128, srcAlpha, srcAlphaMask);
		evb_vec128 = _mm_blendv_epi8(evb_vec128, _mm_sub_epi8(_mm_set1_epi8(16), srcAlpha), srcAlphaMask);
	}
	
	__m128i tmpSrc[4] = {src0, src1, src2, src3};
	
	switch (compState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const __m128i brightnessMask8 = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const __m128i brightnessMask16[2] = {_mm_unpacklo_epi8(brightnessMask8, brightnessMask8), _mm_unpackhi_epi8(brightnessMask8, brightnessMask8)};
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask16[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask16[1] );
			}
			else
			{
				const __m128i brightnessMask32[4] = { _mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
				                                      _mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1]) };
				
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask32[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask32[1] );
				tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[2], evy_vec128), brightnessMask32[2] );
				tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[3], evy_vec128), brightnessMask32[3] );
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const __m128i brightnessMask8 = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const __m128i brightnessMask16[2] = {_mm_unpacklo_epi8(brightnessMask8, brightnessMask8), _mm_unpackhi_epi8(brightnessMask8, brightnessMask8)};
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask16[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask16[1] );
			}
			else
			{
				const __m128i brightnessMask32[4] = { _mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
				                                      _mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1]) };
				
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask32[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask32[1] );
				tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[2], evy_vec128), brightnessMask32[2] );
				tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[3], evy_vec128), brightnessMask32[3] );
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const __m128i blendMask8 = _mm_or_si128( forceBlendEffectMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstEffectEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const __m128i blendMask16[2] = {_mm_unpacklo_epi8(blendMask8, blendMask8), _mm_unpackhi_epi8(blendMask8, blendMask8)};
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i blendSrc16[2] = { this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[0], dst0, eva_vec128, evb_vec128), this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[1], dst1, eva_vec128, evb_vec128) };
		tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
		tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		
		// Combine the final colors.
		tmpSrc[0] = _mm_or_si128(tmpSrc[0], _mm_set1_epi16(0x8000));
		tmpSrc[1] = _mm_or_si128(tmpSrc[1], _mm_set1_epi16(0x8000));
		
		dst0 = _mm_blendv_epi8(dst0, tmpSrc[0], passMask16[0]);
		dst1 = _mm_blendv_epi8(dst1, tmpSrc[1], passMask16[1]);
	}
	else
	{
		const __m128i blendSrc32[4] = { this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[0], dst0, eva_vec128, evb_vec128),
		                                this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[1], dst1, eva_vec128, evb_vec128),
		                                this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[2], dst2, eva_vec128, evb_vec128),
		                                this->_ColorEffectBlend<OUTPUTFORMAT>(tmpSrc[3], dst3, eva_vec128, evb_vec128) };
		
		const __m128i blendMask32[4] = { _mm_unpacklo_epi16(blendMask16[0], blendMask16[0]),
		                                 _mm_unpackhi_epi16(blendMask16[0], blendMask16[0]),
		                                 _mm_unpacklo_epi16(blendMask16[1], blendMask16[1]),
		                                 _mm_unpackhi_epi16(blendMask16[1], blendMask16[1]) };
		
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		
		tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
		tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
		tmpSrc[2] = _mm_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
		tmpSrc[3] = _mm_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		
		tmpSrc[0] = _mm_or_si128(tmpSrc[0], alphaBits);
		tmpSrc[1] = _mm_or_si128(tmpSrc[1], alphaBits);
		tmpSrc[2] = _mm_or_si128(tmpSrc[2], alphaBits);
		tmpSrc[3] = _mm_or_si128(tmpSrc[3], alphaBits);
		
		dst0 = _mm_blendv_epi8(dst0, tmpSrc[0], passMask32[0]);
		dst1 = _mm_blendv_epi8(dst1, tmpSrc[1], passMask32[1]);
		dst2 = _mm_blendv_epi8(dst2, tmpSrc[2], passMask32[2]);
		dst3 = _mm_blendv_epi8(dst3, tmpSrc[3], passMask32[3]);
	}
	
	dstLayerID = _mm_blendv_epi8(dstLayerID, srcLayerID_vec128, passMask8);
}

#endif

// TODO: Unify this method with GPUEngineBase::_RenderPixel().
// We can't unify this yet because the output framebuffer is in RGBA5551, but the 3D source pixels are in RGBA6665.
// However, GPUEngineBase::_RenderPixel() takes source pixels in RGB555. In order to unify the methods, all pixels
// must be processed in RGBA6665.
template<NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_RenderPixel3D(GPUEngineCompositorInfo &compState, const bool enableColorEffect, const FragmentColor srcColor32)
{
	u16 &dstColor16 = *compState.lineColorTarget16;
	FragmentColor &dstColor32 = *compState.lineColorTarget32;
	u8 &dstLayerID = *compState.lineLayerIDTarget;
	ColorEffect selectedEffect = ColorEffect_Disable;
	
	if (enableColorEffect)
	{
		const bool dstEffectEnable = (dstLayerID != GPULayerID_BG0) && this->_dstBlendEnable[dstLayerID];
		
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
		else if (this->_srcBlendEnable[GPULayerID_BG0])
		{
			switch (compState.colorEffect)
			{
				// For the Blend effect, both first and second target flags must be checked.
				case ColorEffect_Blend:
				{
					if (dstEffectEnable) selectedEffect = compState.colorEffect;
					break;
				}
					
				// For the Increase/Decrease Brightness effects, only the first target flag needs to be checked.
				// Test case: Bomberman Land Touch! dialog boxes will render too dark without this check.
				case ColorEffect_IncreaseBrightness:
				case ColorEffect_DecreaseBrightness:
					selectedEffect = compState.colorEffect;
					break;
					
				default:
					break;
			}
		}
	}
	
	// Render the pixel using the selected color effect.
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const u16 srcColor16 = ConvertColor6665To5551<false>(srcColor32);
		
		switch (selectedEffect)
		{
			case ColorEffect_Disable:
				dstColor16 = srcColor16;
				break;
				
			case ColorEffect_IncreaseBrightness:
				dstColor16 = compState.brightnessUpTable555[srcColor16];
				break;
				
			case ColorEffect_DecreaseBrightness:
				dstColor16 = compState.brightnessDownTable555[srcColor16];
				break;
				
			case ColorEffect_Blend:
				dstColor16 = this->_ColorEffectBlend3D(srcColor32, dstColor16);
				break;
		}
		
		dstColor16 |= 0x8000;
	}
	else
	{
		switch (selectedEffect)
		{
			case ColorEffect_Disable:
				dstColor32 = srcColor32;
				break;
				
			case ColorEffect_IncreaseBrightness:
				dstColor32 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(srcColor32, compState.blendEVY);
				break;
				
			case ColorEffect_DecreaseBrightness:
				dstColor32 = this->_ColorEffectDecreaseBrightness(srcColor32, compState.blendEVY);
				break;
				
			case ColorEffect_Blend:
				dstColor32 = this->_ColorEffectBlend3D<OUTPUTFORMAT>(srcColor32, dstColor32);
				break;
		}
		
		dstColor32.a = (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF : 0x1F;
	}
	
	dstLayerID = GPULayerID_BG0;
}

#ifdef ENABLE_SSE2

template <NDSColorFormat OUTPUTFORMAT>
FORCEINLINE void GPUEngineBase::_RenderPixel3D_SSE2(GPUEngineCompositorInfo &compState,
													const __m128i &passMask8,
													const __m128i &enableColorEffectMask,
													const __m128i &src3, const __m128i &src2, const __m128i &src1, const __m128i &src0,
													__m128i &dst3, __m128i &dst2, __m128i &dst1, __m128i &dst0,
													__m128i &dstLayerID)
{
	const __m128i passMask16[2]	= { _mm_unpacklo_epi8(passMask8, passMask8), _mm_unpackhi_epi8(passMask8, passMask8) };
	
	const __m128i passMask32[4] = { _mm_unpacklo_epi16(passMask16[0], passMask16[0]),
									_mm_unpackhi_epi16(passMask16[0], passMask16[0]),
									_mm_unpacklo_epi16(passMask16[1], passMask16[1]),
									_mm_unpackhi_epi16(passMask16[1], passMask16[1]) };
	
	if (_mm_movemask_epi8(enableColorEffectMask) == 0)
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			dst0 = _mm_blendv_epi8(dst0, dst2, passMask16[0]);
			dst1 = _mm_blendv_epi8(dst1, dst3, passMask16[1]);
		}
		else
		{
			dst0 = _mm_blendv_epi8(dst0, src0, passMask32[0]);
			dst1 = _mm_blendv_epi8(dst1, src1, passMask32[1]);
			dst2 = _mm_blendv_epi8(dst2, src2, passMask32[2]);
			dst3 = _mm_blendv_epi8(dst3, src3, passMask32[3]);
		}
		
		dstLayerID = _mm_blendv_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0), passMask8);
		return;
	}
	
	__m128i tmpSrc[4];
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		// Rename our converted 16-bit src colors to reflect what they actually are.
		tmpSrc[0] = dst2;
		tmpSrc[1] = dst3;
		tmpSrc[2] = _mm_setzero_si128();
		tmpSrc[3] = _mm_setzero_si128();
	}
	else
	{
		tmpSrc[0] = src0;
		tmpSrc[1] = src1;
		tmpSrc[2] = src2;
		tmpSrc[3] = src3;
	}
	
	const __m128i srcEffectEnableMask = this->_srcBlendEnable_SSE2[GPULayerID_BG0];
	__m128i dstEffectEnableMask;
	
#ifdef ENABLE_SSSE3
	dstEffectEnableMask = _mm_shuffle_epi8(this->_dstBlendEnable_SSSE3, dstLayerID);
	dstEffectEnableMask = _mm_xor_si128( _mm_cmpeq_epi8(dstEffectEnableMask, _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF) );
#else
	dstEffectEnableMask =                                   _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0)), this->_dstBlendEnable_SSE2[GPULayerID_BG0]);
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG1)), this->_dstBlendEnable_SSE2[GPULayerID_BG1]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG2)), this->_dstBlendEnable_SSE2[GPULayerID_BG2]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG3)), this->_dstBlendEnable_SSE2[GPULayerID_BG3]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_OBJ)), this->_dstBlendEnable_SSE2[GPULayerID_OBJ]) );
	dstEffectEnableMask = _mm_or_si128(dstEffectEnableMask, _mm_and_si128(_mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_Backdrop)), this->_dstBlendEnable_SSE2[GPULayerID_Backdrop]) );
#endif
	
	dstEffectEnableMask = _mm_andnot_si128( _mm_cmpeq_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0)), dstEffectEnableMask );
	
	// Select the color effect based on the BLDCNT target flags.
	const __m128i colorEffect_vec128 = _mm_blendv_epi8(_mm_set1_epi8(ColorEffect_Disable), _mm_set1_epi8(compState.colorEffect), enableColorEffectMask);
	const __m128i forceBlendEffectMask = _mm_and_si128(enableColorEffectMask, dstEffectEnableMask);
	const __m128i evy_vec128 = _mm_set1_epi16(compState.blendEVY);
	
	switch (compState.colorEffect)
	{
		case ColorEffect_IncreaseBrightness:
		{
			const __m128i brightnessMask8 = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_IncreaseBrightness))) );
			const __m128i brightnessMask16[2] = { _mm_unpacklo_epi8(brightnessMask8, brightnessMask8), _mm_unpackhi_epi8(brightnessMask8, brightnessMask8) };
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask16[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask16[1] );
			}
			else
			{
				const __m128i brightnessMask32[4] = { _mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
				                                      _mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1]) };
				
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask32[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask32[1] );
				tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[2], evy_vec128), brightnessMask32[2] );
				tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(tmpSrc[3], evy_vec128), brightnessMask32[3] );
			}
			break;
		}
			
		case ColorEffect_DecreaseBrightness:
		{
			const __m128i brightnessMask8 = _mm_andnot_si128( forceBlendEffectMask, _mm_and_si128(srcEffectEnableMask, _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_DecreaseBrightness))) );
			const __m128i brightnessMask16[2] = { _mm_unpacklo_epi8(brightnessMask8, brightnessMask8), _mm_unpackhi_epi8(brightnessMask8, brightnessMask8) };
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask16[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask16[1] );
			}
			else
			{
				const __m128i brightnessMask32[4] = { _mm_unpacklo_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpackhi_epi16(brightnessMask16[0], brightnessMask16[0]),
				                                      _mm_unpacklo_epi16(brightnessMask16[1], brightnessMask16[1]),
				                                      _mm_unpackhi_epi16(brightnessMask16[1], brightnessMask16[1]) };
				
				tmpSrc[0] = _mm_blendv_epi8( tmpSrc[0], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[0], evy_vec128), brightnessMask32[0] );
				tmpSrc[1] = _mm_blendv_epi8( tmpSrc[1], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[1], evy_vec128), brightnessMask32[1] );
				tmpSrc[2] = _mm_blendv_epi8( tmpSrc[2], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[2], evy_vec128), brightnessMask32[2] );
				tmpSrc[3] = _mm_blendv_epi8( tmpSrc[3], this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(tmpSrc[3], evy_vec128), brightnessMask32[3] );
			}
			break;
		}
			
		default:
			break;
	}
	
	// Render the pixel using the selected color effect.
	const __m128i blendMask8 = _mm_or_si128( forceBlendEffectMask, _mm_and_si128(_mm_and_si128(srcEffectEnableMask, dstEffectEnableMask), _mm_cmpeq_epi8(colorEffect_vec128, _mm_set1_epi8(ColorEffect_Blend))) );
	const __m128i blendMask16[2] = { _mm_unpacklo_epi8(blendMask8, blendMask8), _mm_unpackhi_epi8(blendMask8, blendMask8) };
	
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		const __m128i blendSrc16[2] = { this->_ColorEffectBlend3D<OUTPUTFORMAT>(src0, src1, dst0), this->_ColorEffectBlend3D<OUTPUTFORMAT>(src2, src3, dst1) };
		tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc16[0], blendMask16[0]);
		tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc16[1], blendMask16[1]);
		
		// Combine the final colors.
		tmpSrc[0] = _mm_or_si128(tmpSrc[0], _mm_set1_epi16(0x8000));
		tmpSrc[1] = _mm_or_si128(tmpSrc[1], _mm_set1_epi16(0x8000));
		
		dst0 = _mm_blendv_epi8(dst0, tmpSrc[0], passMask16[0]);
		dst1 = _mm_blendv_epi8(dst1, tmpSrc[1], passMask16[1]);
	}
	else
	{
		const __m128i blendSrc32[4] = { this->_ColorEffectBlend3D<OUTPUTFORMAT>(src0, src0, dst0),
		                                this->_ColorEffectBlend3D<OUTPUTFORMAT>(src1, src1, dst1),
		                                this->_ColorEffectBlend3D<OUTPUTFORMAT>(src2, src2, dst2),
		                                this->_ColorEffectBlend3D<OUTPUTFORMAT>(src3, src3, dst3) };
		
		const __m128i blendMask32[4] = { _mm_unpacklo_epi16(blendMask16[0], blendMask16[0]),
		                                 _mm_unpackhi_epi16(blendMask16[0], blendMask16[0]),
		                                 _mm_unpacklo_epi16(blendMask16[1], blendMask16[1]),
		                                 _mm_unpackhi_epi16(blendMask16[1], blendMask16[1]) };
		
		const __m128i alphaBits = _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000);
		
		tmpSrc[0] = _mm_blendv_epi8(tmpSrc[0], blendSrc32[0], blendMask32[0]);
		tmpSrc[1] = _mm_blendv_epi8(tmpSrc[1], blendSrc32[1], blendMask32[1]);
		tmpSrc[2] = _mm_blendv_epi8(tmpSrc[2], blendSrc32[2], blendMask32[2]);
		tmpSrc[3] = _mm_blendv_epi8(tmpSrc[3], blendSrc32[3], blendMask32[3]);
		
		tmpSrc[0] = _mm_or_si128(tmpSrc[0], alphaBits);
		tmpSrc[1] = _mm_or_si128(tmpSrc[1], alphaBits);
		tmpSrc[2] = _mm_or_si128(tmpSrc[2], alphaBits);
		tmpSrc[3] = _mm_or_si128(tmpSrc[3], alphaBits);
		
		dst0 = _mm_blendv_epi8(dst0, tmpSrc[0], passMask32[0]);
		dst1 = _mm_blendv_epi8(dst1, tmpSrc[1], passMask32[1]);
		dst2 = _mm_blendv_epi8(dst2, tmpSrc[2], passMask32[2]);
		dst3 = _mm_blendv_epi8(dst3, tmpSrc[3], passMask32[3]);
	}
	
	dstLayerID = _mm_blendv_epi8(dstLayerID, _mm_set1_epi8(GPULayerID_BG0), passMask8);
}

#endif

//this is fantastically inaccurate.
//we do the early return even though it reduces the resulting accuracy
//because we need the speed, and because it is inaccurate anyway
void GPUEngineBase::_MosaicSpriteLinePixel(GPUEngineCompositorInfo &compState, const size_t x, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	const bool enableMosaic = (this->_oamList[this->_sprNum[x]].Mosaic != 0);
	if (!enableMosaic)
		return;
	
	const bool opaque = prioTab[x] <= 4;

	GPUEngineBase::MosaicColor::Obj objColor;
	objColor.color = LE_TO_LOCAL_16(dst[x]);
	objColor.alpha = dst_alpha[x];
	objColor.opaque = opaque;

	const size_t y = compState.lineIndexNative;
	
	if (!this->_mosaicWidthOBJ[x].begin || !this->_mosaicHeightOBJ[y].begin)
	{
		objColor = this->_mosaicColors.obj[this->_mosaicWidthOBJ[x].trunc];
	}
	
	this->_mosaicColors.obj[x] = objColor;
	
	dst[x] = LE_TO_LOCAL_16(objColor.color);
	dst_alpha[x] = objColor.alpha;
	if (!objColor.opaque) prioTab[x] = 0x7F;
}

void GPUEngineBase::_MosaicSpriteLine(GPUEngineCompositorInfo &compState, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (!this->_isOBJMosaicSet)
	{
		return;
	}
	
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
	{
		this->_MosaicSpriteLinePixel(compState, i, dst, dst_alpha, typeTab, prioTab);
	}
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_Final(GPUEngineCompositorInfo &compState, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	const u16 lineWidth = (ISDEBUGRENDER) ? compState.selectedBGLayer->size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const s16 dx = (s16)LOCAL_TO_LE_16(param.BGnPA.value);
	const s16 dy = (s16)LOCAL_TO_LE_16(param.BGnPC.value);
	const s32 wh = compState.selectedBGLayer->size.width;
	const s32 ht = compState.selectedBGLayer->size.height;
	const s32 wmask = wh - 1;
	const s32 hmask = ht - 1;
	
	IOREG_BGnX x = param.BGnX;
	IOREG_BGnY y = param.BGnY;
	
#ifdef LOCAL_BE
	// This only seems to work in the unrotated/unscaled case. I'm not too sure
	// about how these bits should really be arranged on big-endian, but at
	// least this arrangement fixes a bunch of games that use affine or extended
	// layers, just as long as they don't perform any rotation/scaling.
	// - rogerman, 2016-07-05
	x.value = ((x.value & 0x00FFFFFF) << 8) | ((x.value & 0xFF000000) >> 24);
	y.value = ((y.value & 0x00FFFFFF) << 8) | ((y.value & 0xFF000000) >> 24);
#endif
	
	u8 index;
	u16 srcColor;
	
	// as an optimization, specially handle the fairly common case of
	// "unrotated + unscaled + no boundary checking required"
	if (dx == GPU_FRAMEBUFFER_NATIVE_WIDTH && dy == 0)
	{
		s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if ( WRAP || ((auxX >= 0) && (auxX + lineWidth <= wh) && (auxY >= 0) && (auxY < ht)) )
		{
			for (size_t i = 0; i < lineWidth; i++)
			{
				GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, srcColor);
				
				if (ISCUSTOMRENDERINGNEEDED)
				{
					this->_bgLayerIndex[i] = index;
					this->_bgLayerColor[i] = srcColor;
				}
				else
				{
					this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, i, srcColor, (index != 0));
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
	
	for (size_t i = 0; i < lineWidth; i++, x.value+=dx, y.value+=dy)
	{
		const s32 auxX = (WRAP) ? (x.Integer & wmask) : x.Integer;
		const s32 auxY = (WRAP) ? (y.Integer & hmask) : y.Integer;
		
		if (WRAP || ((auxX >= 0) && (auxX < wh) && (auxY >= 0) && (auxY < ht)))
		{
			GetPixelFunc(auxX, auxY, wh, map, tile, pal, index, srcColor);
			
			if (ISCUSTOMRENDERINGNEEDED)
			{
				this->_bgLayerIndex[i] = index;
				this->_bgLayerColor[i] = srcColor;
			}
			else
			{
				this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, i, srcColor, (index != 0));
			}
		}
	}
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc, bool WRAP>
void GPUEngineBase::_RenderPixelIterate_ApplyWrap(GPUEngineCompositorInfo &compState, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	this->_RenderPixelIterate_Final<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, GetPixelFunc, WRAP>(compState, param, map, tile, pal);
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED, PixelLookupFunc GetPixelFunc>
void GPUEngineBase::_RenderPixelIterate(GPUEngineCompositorInfo &compState, const IOREG_BGnParameter &param, const u32 map, const u32 tile, const u16 *__restrict pal)
{
	if (compState.selectedBGLayer->isDisplayWrapped)
	{
		this->_RenderPixelIterate_ApplyWrap<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, GetPixelFunc, true>(compState, param, map, tile, pal);
	}
	else
	{
		this->_RenderPixelIterate_ApplyWrap<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, GetPixelFunc, false>(compState, param, map, tile, pal);
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

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT>
FORCEINLINE void GPUEngineBase::_RenderPixelSingle(GPUEngineCompositorInfo &compState, const size_t srcX, u16 srcColor16, const bool opaque)
{
	bool willRenderColor = opaque;
	
	compState.xNative = srcX;
	compState.xCustom = _gpuDstPitchIndex[srcX];
	compState.lineLayerIDTarget = compState.lineLayerIDHeadNative + srcX;
	compState.lineColorTarget16 = (u16 *)compState.lineColorHeadNative + srcX;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHeadNative + srcX;
	
	if (MOSAIC)
	{
		//due to this early out, we will get incorrect behavior in cases where
		//we enable mosaic in the middle of a frame. this is deemed unlikely.
		
		if (!opaque) srcColor16 = 0xFFFF;
		else srcColor16 &= 0x7FFF;
		
		if (!this->_mosaicWidthBG[srcX].begin || !this->_mosaicHeightBG[compState.lineIndexNative].begin)
		{
			srcColor16 = this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[srcX].trunc];
		}
		
		this->_mosaicColors.bg[compState.selectedLayerID][srcX] = srcColor16;
		
		willRenderColor = (srcColor16 != 0xFFFF);
	}
	
	if (willRenderColor)
	{
		this->_RenderPixel<OUTPUTFORMAT, false, ISDEBUGRENDER, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState,
																											  srcColor16,
																											  0);
	}
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT>
void GPUEngineBase::_RenderPixelsCustom(GPUEngineCompositorInfo &compState)
{
#ifdef ENABLE_SSE2
	
	#ifdef ENABLE_SSSE3
	const bool isIntegerScale = ((compState.lineWidthCustom % GPU_FRAMEBUFFER_NATIVE_WIDTH) == 0);
	const size_t scale = compState.lineWidthCustom / GPU_FRAMEBUFFER_NATIVE_WIDTH;
	#endif
	
	for (size_t x = 0, dstIdx = 0; x < GPU_FRAMEBUFFER_NATIVE_WIDTH; x+=8)
	{
		if (MOSAIC)
		{
			const __m128i index_vec128 = _mm_loadl_epi64((__m128i *)(this->_bgLayerIndex + x));
			const __m128i col_vec128 = _mm_load_si128((__m128i *)(this->_bgLayerColor + x));
			
			const __m128i idxMask = _mm_cmpeq_epi16(_mm_unpacklo_epi8(index_vec128, _mm_setzero_si128()), _mm_setzero_si128());
			const __m128i tmpColor_vec128 = _mm_blendv_epi8(_mm_and_si128(col_vec128, _mm_set1_epi16(0x7FFF)), _mm_set1_epi16(0xFFFF), idxMask);
			
			const __m128i mosaicWidthMask = _mm_cmpeq_epi16( _mm_and_si128(_mm_set1_epi16(0x00FF), _mm_loadu_si128((__m128i *)(this->_mosaicWidthBG + x))), _mm_setzero_si128() );
			const __m128i mosaicHeightMask = _mm_cmpeq_epi16(_mm_set1_epi16(this->_mosaicHeightBG[compState.lineIndexNative].begin), _mm_setzero_si128());
			const __m128i mosaicMask = _mm_or_si128(mosaicWidthMask, mosaicHeightMask);
			
			this->_mosaicColors.bg[compState.selectedLayerID][x+0] = (_mm_extract_epi16(mosaicMask, 0) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+0].trunc] : _mm_extract_epi16(tmpColor_vec128, 0);
			this->_mosaicColors.bg[compState.selectedLayerID][x+1] = (_mm_extract_epi16(mosaicMask, 1) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+1].trunc] : _mm_extract_epi16(tmpColor_vec128, 1);
			this->_mosaicColors.bg[compState.selectedLayerID][x+2] = (_mm_extract_epi16(mosaicMask, 2) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+2].trunc] : _mm_extract_epi16(tmpColor_vec128, 2);
			this->_mosaicColors.bg[compState.selectedLayerID][x+3] = (_mm_extract_epi16(mosaicMask, 3) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+3].trunc] : _mm_extract_epi16(tmpColor_vec128, 3);
			this->_mosaicColors.bg[compState.selectedLayerID][x+4] = (_mm_extract_epi16(mosaicMask, 4) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+4].trunc] : _mm_extract_epi16(tmpColor_vec128, 4);
			this->_mosaicColors.bg[compState.selectedLayerID][x+5] = (_mm_extract_epi16(mosaicMask, 5) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+5].trunc] : _mm_extract_epi16(tmpColor_vec128, 5);
			this->_mosaicColors.bg[compState.selectedLayerID][x+6] = (_mm_extract_epi16(mosaicMask, 6) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+6].trunc] : _mm_extract_epi16(tmpColor_vec128, 6);
			this->_mosaicColors.bg[compState.selectedLayerID][x+7] = (_mm_extract_epi16(mosaicMask, 7) != 0) ? this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x+7].trunc] : _mm_extract_epi16(tmpColor_vec128, 7);
			
			const __m128i mosaicColor_vec128 = _mm_loadu_si128((__m128i *)(this->_mosaicColors.bg[compState.selectedLayerID] + x));
			const __m128i mosaicColorMask = _mm_cmpeq_epi16(mosaicColor_vec128, _mm_set1_epi16(0xFFFF));
			_mm_storel_epi64( (__m128i *)(this->_bgLayerIndex + x), _mm_andnot_si128(_mm_packs_epi16(mosaicColorMask, _mm_setzero_si128()), index_vec128) );
			_mm_store_si128( (__m128i *)(this->_bgLayerColor + x), _mm_blendv_epi8(mosaicColor_vec128, col_vec128, mosaicColorMask) );
		}
		
	#ifdef ENABLE_SSSE3
		if (isIntegerScale)
		{
			const __m128i index_vec128 = _mm_loadl_epi64((__m128i *)(this->_bgLayerIndex + x));
			const __m128i col_vec128 = _mm_load_si128((__m128i *)(this->_bgLayerColor + x));
			
			for (size_t s = 0; s < scale; s++)
			{
				const __m128i ssse3idx_u8 = _mm_loadl_epi64((__m128i *)(_gpuDstToSrcSSSE3_u8_8e + (s * 8)));
				const __m128i ssse3idx_u16 = _mm_load_si128((__m128i *)(_gpuDstToSrcSSSE3_u16_8e + (s * 16)));
				
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
			
			if (!this->_mosaicWidthBG[x].begin || !this->_mosaicHeightBG[compState.lineIndexNative].begin)
			{
				tmpColor = this->_mosaicColors.bg[compState.selectedLayerID][this->_mosaicWidthBG[x].trunc];
			}
			
			this->_mosaicColors.bg[compState.selectedLayerID][x] = tmpColor;
			
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
	
	compState.lineColorTarget16 = (u16 *)compState.lineColorHead;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHead;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = (compState.lineWidthCustom - (compState.lineWidthCustom % 16));
	const __m128i srcEffectEnableMask = this->_srcBlendEnable_SSE2[compState.selectedLayerID];
#endif
	
	for (size_t l = 0; l < compState.lineRenderCount; l++)
	{
		compState.xNative = 0;
		compState.xCustom = 0;
		
#ifdef ENABLE_SSE2
		for (; compState.xCustom < ssePixCount; compState.xCustom+=16, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16+=16, compState.lineColorTarget32+=16, compState.lineLayerIDTarget+=16)
		{
			__m128i src[4];
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				src[0] = _mm_load_si128((__m128i *)(this->_bgLayerColorCustom + compState.xCustom + 0));
				src[1] = _mm_load_si128((__m128i *)(this->_bgLayerColorCustom + compState.xCustom + 8));
				src[2] = _mm_setzero_si128();
				src[3] = _mm_setzero_si128();
			}
			else
			{
				const __m128i src16[2] = { _mm_load_si128((__m128i *)(this->_bgLayerColorCustom + compState.xCustom + 0)),
										   _mm_load_si128((__m128i *)(this->_bgLayerColorCustom + compState.xCustom + 8)) };
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
				{
					ConvertColor555To6665Opaque<false>(src16[0], src[0], src[1]);
					ConvertColor555To6665Opaque<false>(src16[1], src[2], src[3]);
				}
				else
				{
					ConvertColor555To8888Opaque<false>(src16[0], src[0], src[1]);
					ConvertColor555To8888Opaque<false>(src16[1], src[2], src[3]);
				}
			}
			
			const __m128i srcAlpha = _mm_setzero_si128();
			
			__m128i dstLayerID_vec128 = _mm_load_si128((__m128i *)compState.lineLayerIDTarget);
			__m128i passMask8 = _mm_xor_si128( _mm_cmpeq_epi8(_mm_load_si128((__m128i *)(this->_bgLayerIndexCustom + compState.xCustom)), _mm_setzero_si128()), _mm_set1_epi32(0xFFFFFFFF) );
			
			__m128i dst[4];
			dst[0] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 0);
			dst[1] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 1);
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
			{
				dst[2] = _mm_setzero_si128();
				dst[3] = _mm_setzero_si128();
			}
			else
			{
				dst[2] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 2);
				dst[3] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 3);
			}
			
			this->_RenderPixel16_SSE2<OUTPUTFORMAT, false, ISDEBUGRENDER, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, true>(compState,
																															   src[3], src[2], src[1], src[0],
																															   srcAlpha,
																															   srcEffectEnableMask,
																															   dst[3], dst[2], dst[1], dst[0],
																															   dstLayerID_vec128,
																															   passMask8);
			_mm_store_si128((__m128i *)*compState.lineColorTarget + 0, dst[0]);
			_mm_store_si128((__m128i *)*compState.lineColorTarget + 1, dst[1]);
			
			if (OUTPUTFORMAT != NDSColorFormat_BGR555_Rev)
			{
				_mm_store_si128((__m128i *)*compState.lineColorTarget + 2, dst[2]);
				_mm_store_si128((__m128i *)*compState.lineColorTarget + 3, dst[3]);
			}
			
			_mm_store_si128((__m128i *)compState.lineLayerIDTarget, dstLayerID_vec128);
		}
#endif
		
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
		for (; compState.xCustom < compState.lineWidthCustom; compState.xCustom++, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16++, compState.lineColorTarget32++, compState.lineLayerIDTarget++)
		{
			if (this->_bgLayerIndexCustom[compState.xCustom] == 0)
			{
				continue;
			}
			
			this->_RenderPixel<OUTPUTFORMAT, false, ISDEBUGRENDER, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState,
																												  this->_bgLayerColorCustom[compState.xCustom],
																												  0);
		}
	}
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT>
void GPUEngineBase::_RenderPixelsCustomVRAM(GPUEngineCompositorInfo &compState)
{
	const u16 *__restrict srcLine = GPU->GetCustomVRAMAddressUsingMappedAddress(compState.selectedBGLayer->BMPAddress) + compState.blockOffsetCustom;
	
	compState.xNative = 0;
	compState.xCustom = 0;
	compState.lineColorTarget16 = (u16 *)compState.lineColorHead;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHead;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
	
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const __m128i srcEffectEnableMask = this->_srcBlendEnable_SSE2[compState.selectedLayerID];
	
	const size_t ssePixCount = (compState.linePixelCount - (compState.linePixelCount % 16));
	for (; i < ssePixCount; i+=16, compState.xCustom+=16, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16+=16, compState.lineColorTarget32+=16, compState.lineLayerIDTarget+=16)
	{
		const __m128i src16[2] = { _mm_load_si128((__m128i *)(srcLine + i + 0)),
		                           _mm_load_si128((__m128i *)(srcLine + i + 8)) };
		__m128i src[4];
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			src[0] = src16[0];
			src[1] = src16[1];
			src[2] = _mm_setzero_si128();
			src[3] = _mm_setzero_si128();
		}
		else
		{
			if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
			{
				ConvertColor555To6665Opaque<false>(src16[0], src[0], src[1]);
				ConvertColor555To6665Opaque<false>(src16[1], src[2], src[3]);
			}
			else
			{
				ConvertColor555To8888Opaque<false>(src16[0], src[0], src[1]);
				ConvertColor555To8888Opaque<false>(src16[1], src[2], src[3]);
			}
		}
		
		const __m128i srcAlpha = _mm_setzero_si128();
		
		__m128i dstLayerID_vec128 = _mm_load_si128((__m128i *)compState.lineLayerIDTarget);
		__m128i passMask8 = _mm_packs_epi16( _mm_srli_epi16(src16[0], 15), _mm_srli_epi16(src16[1], 15) );
		passMask8 = _mm_cmpeq_epi8(passMask8, _mm_set1_epi8(1));
		
		__m128i dst[4];
		dst[0] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 0);
		dst[1] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 1);
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			dst[2] = _mm_setzero_si128();
			dst[3] = _mm_setzero_si128();
		}
		else
		{
			dst[2] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 2);
			dst[3] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 3);
		}
		
		this->_RenderPixel16_SSE2<OUTPUTFORMAT, false, ISDEBUGRENDER, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, true>(compState,
																														   src[3], src[2], src[1], src[0],
																														   srcAlpha,
																														   srcEffectEnableMask,
																														   dst[3], dst[2], dst[1], dst[0],
																														   dstLayerID_vec128,
																														   passMask8);
		_mm_store_si128((__m128i *)*compState.lineColorTarget + 0, dst[0]);
		_mm_store_si128((__m128i *)*compState.lineColorTarget + 1, dst[1]);
		
		if (OUTPUTFORMAT != NDSColorFormat_BGR555_Rev)
		{
			_mm_store_si128((__m128i *)*compState.lineColorTarget + 2, dst[2]);
			_mm_store_si128((__m128i *)*compState.lineColorTarget + 3, dst[3]);
		}
		
		_mm_store_si128((__m128i *)compState.lineLayerIDTarget, dstLayerID_vec128);
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < compState.linePixelCount; i++, compState.xCustom++, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16++, compState.lineColorTarget32++, compState.lineLayerIDTarget++)
	{
		if ((srcLine[i] & 0x8000) == 0)
		{
			continue;
		}
		
		this->_RenderPixel<OUTPUTFORMAT, false, ISDEBUGRENDER, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState,
																											  srcLine[i],
																											  0);
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -TEXT-
/*****************************************************************************/
// render a text background to the combined pixelbuffer
template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_BGText(GPUEngineCompositorInfo &compState, const u16 XBG, const u16 YBG)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const u16 lineWidth = (ISDEBUGRENDER) ? compState.selectedBGLayer->size.width : GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const u16 lg    = compState.selectedBGLayer->size.width;
	const u16 ht    = compState.selectedBGLayer->size.height;
	const u32 tile  = compState.selectedBGLayer->tileEntryAddress;
	const u16 wmask = lg - 1;
	const u16 hmask = ht - 1;
	
	const size_t pixCountLo = 8 - (XBG & 0x0007);
	size_t x = 0;
	size_t xoff = XBG;
	
	const u16 tmp = (YBG & hmask) >> 3;
	u32 map = compState.selectedBGLayer->tileMapAddress + (tmp & 31) * 64;
	if (tmp > 31)
		map += ADDRESS_STEP_512B << compState.selectedBGLayer->BGnCNT.ScreenSize;
	
	if (compState.selectedBGLayer->BGnCNT.PaletteMode == PaletteMode_16x16) // color: 16 palette entries
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
						this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
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
						this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
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
							this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
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
						this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
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
						this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
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
							this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
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
		const u16 *__restrict pal = (DISPCNT.ExBGxPalette_Enable) ? *(compState.selectedBGLayer->extPalette) : this->_paletteBG;
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
					this->_RenderPixelSingle<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (index != 0));
				}
			}
		}
	}
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_BGAffine(GPUEngineCompositorInfo &compState, const IOREG_BGnParameter &param)
{
	this->_RenderPixelIterate<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_tiled_8bit_entry>(compState, param, compState.selectedBGLayer->tileMapAddress, compState.selectedBGLayer->tileEntryAddress, this->_paletteBG);
}

template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_BGExtended(GPUEngineCompositorInfo &compState, const IOREG_BGnParameter &param, bool &outUseCustomVRAM)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	
	switch (compState.selectedBGLayer->type)
	{
		case BGType_AffineExt_256x16: // 16  bit bgmap entries
		{
			if (DISPCNT.ExBGxPalette_Enable)
			{
				this->_RenderPixelIterate< OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_tiled_16bit_entry<true> >(compState, param, compState.selectedBGLayer->tileMapAddress, compState.selectedBGLayer->tileEntryAddress, *(compState.selectedBGLayer->extPalette));
			}
			else
			{
				this->_RenderPixelIterate< OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_tiled_16bit_entry<false> >(compState, param, compState.selectedBGLayer->tileMapAddress, compState.selectedBGLayer->tileEntryAddress, this->_paletteBG);
			}
			break;
		}
			
		case BGType_AffineExt_256x1: // 256 colors
			this->_RenderPixelIterate<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_256_map>(compState, param, compState.selectedBGLayer->BMPAddress, 0, this->_paletteBG);
			break;
			
		case BGType_AffineExt_Direct: // direct colors / BMP
		{
			if (!MOSAIC)
			{
				const size_t vramPixel = (size_t)((u8 *)MMU_gpu_map(compState.selectedBGLayer->BMPAddress) - MMU.ARM9_LCD) / sizeof(u16);
				
				if (vramPixel > (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES * 4))
				{
					outUseCustomVRAM = false;
				}
				else
				{
					const size_t blockID = vramPixel / (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES);
					const size_t blockPixel = vramPixel % (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_VRAM_BLOCK_LINES);
					const size_t blockLine = blockPixel / GPU_FRAMEBUFFER_NATIVE_WIDTH;
					
					if (GPU->GetEngineMain()->VerifyVRAMLineDidChange(blockID, compState.lineIndexNative + blockLine))
					{
						switch (OUTPUTFORMAT)
						{
							case NDSColorFormat_BGR555_Rev:
								this->_LineColorCopy<true, false, false, false, 2>(compState.lineColorHeadNative, compState.lineColorHeadCustom, compState.lineIndexNative);
								break;
								
							case NDSColorFormat_BGR666_Rev:
							case NDSColorFormat_BGR888_Rev:
								this->_LineColorCopy<true, false, false, false, 4>(compState.lineColorHeadNative, compState.lineColorHeadCustom, compState.lineIndexNative);
								break;
						}
						
						this->_LineLayerIDCopy<true, false>(compState.lineLayerIDHeadNative, compState.lineLayerIDHeadCustom, compState.lineIndexNative);
						
						compState.lineColorHead = compState.lineColorHeadNative;
						compState.lineLayerIDHead = compState.lineLayerIDHeadNative;
					}
					
					outUseCustomVRAM = !GPU->GetEngineMain()->isLineCaptureNative[blockID][compState.lineIndexNative + blockLine];
				}
			}
			else
			{
				outUseCustomVRAM = false;
			}
			
			if (!outUseCustomVRAM)
			{
				this->_RenderPixelIterate<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_BMP_map>(compState, param, compState.selectedBGLayer->BMPAddress, 0, this->_paletteBG);
			}
			else
			{
				if (this->isLineRenderNative[compState.lineIndexNative])
				{
					switch (OUTPUTFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
							this->_LineColorCopy<false, true, false, false, 2>(compState.lineColorHeadCustom, compState.lineColorHeadNative, compState.lineIndexNative);
							break;
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
							this->_LineColorCopy<false, true, false, false, 4>(compState.lineColorHeadCustom, compState.lineColorHeadNative, compState.lineIndexNative);
							break;
					}
					
					this->_LineLayerIDCopy<false, true>(compState.lineLayerIDHeadCustom, compState.lineLayerIDHeadNative, compState.lineIndexNative);
					
					compState.lineColorHead = compState.lineColorHeadCustom;
					compState.lineLayerIDHead = compState.lineLayerIDHeadCustom;
					
					this->isLineRenderNative[compState.lineIndexNative] = false;
					this->nativeLineRenderCount--;
				}
			}
			break;
		}
			
		case BGType_Large8bpp: // large screen 256 colors
			this->_RenderPixelIterate<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED, rot_256_map>(compState, param, compState.selectedBGLayer->largeBMPAddress, 0, this->_paletteBG);
			break;
			
		default:
			break;
	}
}

/*****************************************************************************/
//			BACKGROUND RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_LineText(GPUEngineCompositorInfo &compState)
{
	if (ISDEBUGRENDER)
	{
		this->_RenderLine_BGText<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, 0, compState.lineIndexNative);
	}
	else
	{
		this->_RenderLine_BGText<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, compState.selectedBGLayer->xOffset, compState.lineIndexNative + compState.selectedBGLayer->yOffset);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_LineRot(GPUEngineCompositorInfo &compState)
{
	if (ISDEBUGRENDER)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, compState.blockOffsetNative};
		this->_RenderLine_BGAffine<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, debugParams);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (compState.selectedLayerID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGAffine<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, *bgParams);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_LineExtRot(GPUEngineCompositorInfo &compState, bool &outUseCustomVRAM)
{
	if (ISDEBUGRENDER)
	{
		static const IOREG_BGnParameter debugParams = {256, 0, 0, -77, 0, compState.blockOffsetNative};
		this->_RenderLine_BGExtended<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, debugParams, outUseCustomVRAM);
	}
	else
	{
		IOREG_BGnParameter *__restrict bgParams = (compState.selectedLayerID == GPULayerID_BG2) ? (IOREG_BGnParameter *)&this->_IORegisterMap->BG2Param : (IOREG_BGnParameter *)&this->_IORegisterMap->BG3Param;
		this->_RenderLine_BGExtended<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, *bgParams, outUseCustomVRAM);
		
		bgParams->BGnX.value += bgParams->BGnPB.value;
		bgParams->BGnY.value += bgParams->BGnPD.value;
	}
}

/*****************************************************************************/
//			SPRITE RENDERING -HELPER FUNCTIONS-
/*****************************************************************************/

/* if i understand it correct, and it fixes some sprite problems in chameleon shot */
/* we have a 15 bit color, and should use the pal entry bits as alpha ?*/
/* http://nocash.emubase.de/gbatek.htm#dsvideoobjs */
template <bool ISDEBUGRENDER>
void GPUEngineBase::_RenderSpriteBMP(GPUEngineCompositorInfo &compState, const u8 spriteNum, u16 *__restrict dst, const u32 srcadr, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
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
				const __m128i color_vec128 = _mm_loadu_si128((__m128i *)(bmpBuffer + x));
				const __m128i alphaCompare = _mm_cmpeq_epi16( _mm_srli_epi16(color_vec128, 15), _mm_set1_epi16(0x0001) );
				_mm_storeu_si128( (__m128i *)(dst + sprX), _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst + sprX)), color_vec128, alphaCompare) );
			}
		}
		else
		{
			const __m128i prio_vec128 = _mm_set1_epi8(prio);
			
			const size_t ssePixCount = lg - (lg % 16);
			for (; i < ssePixCount; i += 16, x += 16, sprX += 16)
			{
				const __m128i prioTab_vec128 = _mm_loadu_si128((__m128i *)(prioTab + sprX));
				const __m128i colorLo_vec128 = _mm_loadu_si128((__m128i *)(bmpBuffer + x));
				const __m128i colorHi_vec128 = _mm_loadu_si128((__m128i *)(bmpBuffer + x + 8));
				
				const __m128i prioCompare = _mm_cmplt_epi8(prio_vec128, prioTab_vec128);
				const __m128i alphaCompare = _mm_cmpeq_epi8( _mm_packs_epi16(_mm_srli_epi16(colorLo_vec128, 15), _mm_srli_epi16(colorHi_vec128, 15)), _mm_set1_epi8(0x01) );
				
				const __m128i combinedPackedCompare = _mm_and_si128(prioCompare, alphaCompare);
				const __m128i combinedLoCompare = _mm_unpacklo_epi8(combinedPackedCompare, combinedPackedCompare);
				const __m128i combinedHiCompare = _mm_unpackhi_epi8(combinedPackedCompare, combinedPackedCompare);
				
				// Just in case you're wondering why we're not using maskmovdqu, but instead using movdqu+pblendvb+movdqu, it's because
				// maskmovdqu won't keep the data in cache, and we really need the data in cache since we're about to render the sprite
				// to the framebuffer. In addition, the maskmovdqu instruction can be brutally slow on many non-Intel CPUs.
				_mm_storeu_si128( (__m128i *)(dst + sprX + 0),       _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst + sprX + 0)), colorLo_vec128, combinedLoCompare) );
				_mm_storeu_si128( (__m128i *)(dst + sprX + 8),       _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst + sprX + 8)), colorHi_vec128, combinedHiCompare) );
				_mm_storeu_si128( (__m128i *)(dst_alpha + sprX),     _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(dst_alpha + sprX)), _mm_set1_epi8(alpha + 1), combinedPackedCompare) );
				_mm_storeu_si128( (__m128i *)(typeTab + sprX),       _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(typeTab + sprX)), _mm_set1_epi8(OBJMode_Bitmap), combinedPackedCompare) );
				_mm_storeu_si128( (__m128i *)(prioTab + sprX),       _mm_blendv_epi8(prioTab_vec128, prio_vec128, combinedPackedCompare) );
				_mm_storeu_si128( (__m128i *)(this->_sprNum + sprX), _mm_blendv_epi8(_mm_loadu_si128((__m128i *)(this->_sprNum + sprX)), _mm_set1_epi8(spriteNum), combinedPackedCompare) );
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
void GPUEngineBase::_RenderSprite256(GPUEngineCompositorInfo &compState, const u8 spriteNum, u16 *__restrict dst, const u32 srcadr, const u16 *__restrict pal, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
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
void GPUEngineBase::_RenderSprite16(GPUEngineCompositorInfo &compState, const u8 spriteNum, u16 *__restrict dst, const u32 srcadr, const u16 *__restrict pal, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab, const u8 prio, const size_t lg, size_t sprX, size_t x, const s32 xdir, const u8 alpha)
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
bool GPUEngineBase::_ComputeSpriteVars(GPUEngineCompositorInfo &compState, const OAMAttributes &spriteInfo, SpriteSize &sprSize, s32 &sprX, s32 &sprY, s32 &x, s32 &y, s32 &lg, s32 &xdir)
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
	y = (compState.lineIndexNative - sprY) & 0xFF;                        /* get the y line within sprite coords */
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
void GPUEngineBase::_SpriteRender(GPUEngineCompositorInfo &compState, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
{
	if (this->_spriteRenderMode == SpriteRenderMode_Sprite1D)
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite1D, ISDEBUGRENDER>(compState, dst, dst_alpha, typeTab, prioTab);
	else
		this->_SpriteRenderPerform<SpriteRenderMode_Sprite2D, ISDEBUGRENDER>(compState, dst, dst_alpha, typeTab, prioTab);
}

void GPUEngineBase::SpriteRenderDebug(const u16 lineIndex, u16 *dst)
{
	GPUEngineCompositorInfo compState;
	
	compState.lineIndexNative = lineIndex;
	compState.lineIndexCustom = _gpuDstLineIndex[lineIndex];
	compState.lineWidthCustom = GPU->GetDisplayInfo().customWidth;
	compState.lineRenderCount = _gpuDstLineCount[lineIndex];
	compState.linePixelCount = compState.lineWidthCustom * compState.lineRenderCount;
	compState.blockOffsetNative = compState.lineIndexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compState.blockOffsetCustom = compState.lineIndexCustom * compState.lineWidthCustom;
	
	compState.selectedLayerID = GPULayerID_OBJ;
	compState.selectedBGLayer = NULL;
	compState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	compState.colorEffect = ColorEffect_Disable;
	compState.blendEVA = this->_BLDALPHA_EVA;
	compState.blendEVB = this->_BLDALPHA_EVB;
	compState.blendEVY = this->_BLDALPHA_EVY;
	compState.blendTable555 = this->_selectedBlendTable555;
	compState.brightnessUpTable555 = this->_selectedBrightnessUpTable555;
	compState.brightnessUpTable666 = this->_selectedBrightnessUpTable666;
	compState.brightnessUpTable888 = this->_selectedBrightnessUpTable888;
	compState.brightnessDownTable555 = this->_selectedBrightnessDownTable555;
	compState.brightnessDownTable666 = this->_selectedBrightnessDownTable666;
	compState.brightnessDownTable888 = this->_selectedBrightnessDownTable888;
	
	compState.lineColorHead = dst;
	compState.lineColorHeadNative = compState.lineColorHead;
	compState.lineColorHeadCustom = compState.lineColorHead;
	compState.lineLayerIDHead = NULL;
	compState.lineLayerIDHeadNative = NULL;
	compState.lineLayerIDHeadCustom = NULL;
	
	compState.xNative = 0;
	compState.xCustom = 0;
	compState.lineColorTarget = (void **)&compState.lineColorTarget16;
	compState.lineColorTarget16 = (u16 *)compState.lineColorHeadNative;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHeadNative;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
	
	this->_SpriteRender<true>(compState, dst, NULL, NULL, NULL);
}

template <SpriteRenderMode MODE, bool ISDEBUGRENDER>
void GPUEngineBase::_SpriteRenderPerform(GPUEngineCompositorInfo &compState, u16 *__restrict dst, u8 *__restrict dst_alpha, u8 *__restrict typeTab, u8 *__restrict prioTab)
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
			y = (compState.lineIndexNative - sprY) & 0xFF;
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
			if (!this->_ComputeSpriteVars(compState, spriteInfo, sprSize, sprX, sprY, x, y, lg, xdir))
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
				this->_RenderSpriteBMP<ISDEBUGRENDER>(compState, i, dst, srcadr, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, spriteInfo.PaletteIndex);
			}
			else if (spriteInfo.PaletteMode == PaletteMode_1x256) //256 colors
			{
				if (MODE == SpriteRenderMode_Sprite2D)
					srcadr = this->_sprMem + ((spriteInfo.TileIndex)<<5) + ((y>>3)<<10) + ((y&0x7)*8);
				else
					srcadr = this->_sprMem + (spriteInfo.TileIndex<<this->_sprBoundary) + ((y>>3)*sprSize.width*8) + ((y&0x7)*8);
				
				pal = (DISPCNT.ExOBJPalette_Enable) ? (u16 *)(MMU.ObjExtPal[this->_engineID][0]+(spriteInfo.PaletteIndex*ADDRESS_STEP_512B)) : this->_paletteOBJ;
				this->_RenderSprite256<ISDEBUGRENDER>(compState, i, dst, srcadr, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, (objMode == OBJMode_Transparent));
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
				this->_RenderSprite16<ISDEBUGRENDER>(compState, i, dst, srcadr, pal, dst_alpha, typeTab, prioTab, prio, lg, sprX, x, xdir, (objMode == OBJMode_Transparent));
			}
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_Layers(const size_t l)
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	GPUEngineCompositorInfo &compState = this->_currentCompositorState;
	itemsForPriority_t *item;
	
	compState.lineIndexNative = l;
	compState.lineIndexCustom = _gpuDstLineIndex[l];
	compState.lineWidthCustom = dispInfo.customWidth;
	compState.lineRenderCount = _gpuDstLineCount[l];
	compState.linePixelCount = compState.lineWidthCustom * compState.lineRenderCount;
	compState.blockOffsetNative = compState.lineIndexNative * GPU_FRAMEBUFFER_NATIVE_WIDTH;
	compState.blockOffsetCustom = compState.lineIndexCustom * compState.lineWidthCustom;
	
	compState.selectedLayerID = GPULayerID_BG0;
	compState.selectedBGLayer = &this->_BGLayer[GPULayerID_BG0];
	compState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	compState.colorEffect = (ColorEffect)this->_IORegisterMap->BLDCNT.ColorEffect;;
	compState.blendEVA = this->_BLDALPHA_EVA;
	compState.blendEVB = this->_BLDALPHA_EVB;
	compState.blendEVY = this->_BLDALPHA_EVY;
	compState.blendTable555 = this->_selectedBlendTable555;
	compState.brightnessUpTable555 = this->_selectedBrightnessUpTable555;
	compState.brightnessUpTable666 = this->_selectedBrightnessUpTable666;
	compState.brightnessUpTable888 = this->_selectedBrightnessUpTable888;
	compState.brightnessDownTable555 = this->_selectedBrightnessDownTable555;
	compState.brightnessDownTable666 = this->_selectedBrightnessDownTable666;
	compState.brightnessDownTable888 = this->_selectedBrightnessDownTable888;
	
	// Optimization: For normal display mode, render straight to the output buffer when that is what we are going to end
	// up displaying anyway. Otherwise, we need to use the working buffer.
	compState.lineColorHeadNative = (this->_displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->nativeBuffer + (compState.blockOffsetNative * dispInfo.pixelBytes) : (u8 *)this->_internalRenderLineTargetNative;
	compState.lineColorHeadCustom = (this->_displayOutputMode == GPUDisplayMode_Normal) ? (u8 *)this->customBuffer + (compState.blockOffsetCustom * dispInfo.pixelBytes) : (u8 *)this->_internalRenderLineTargetCustom;
	compState.lineColorHead = compState.lineColorHeadNative;
	
	compState.lineLayerIDHeadNative = this->_renderLineLayerIDNative;
	compState.lineLayerIDHeadCustom = this->_renderLineLayerIDCustom;
	compState.lineLayerIDHead = compState.lineLayerIDHeadNative;
	
	compState.xNative = 0;
	compState.xCustom = 0;
	compState.lineColorTarget = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void **)&compState.lineColorTarget16 : (void **)&compState.lineColorTarget32;
	compState.lineColorTarget16 = (u16 *)compState.lineColorHeadNative;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHeadNative;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
	
	this->_RenderLine_Clear<OUTPUTFORMAT>(compState);
	
	// for all the pixels in the line
	if (this->_enableLayer[GPULayerID_OBJ])
	{
		this->_RenderLine_SetupSprites(compState);
	}
	
	if (WILLPERFORMWINDOWTEST)
	{
		this->_PerformWindowTesting(compState);
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
					compState.selectedLayerID = layerID;
					compState.selectedBGLayer = &this->_BGLayer[layerID];
					
					if (this->_engineID == GPUEngineID_Main)
					{
						if ( (layerID == GPULayerID_BG0) && GPU->GetEngineMain()->WillRender3DLayer() )
						{
							GPU->GetEngineMain()->RenderLine_Layer3D<OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compState);
							continue;
						}
					}
					
					if (this->isLineRenderNative[compState.lineIndexNative])
					{
						this->_RenderLine_LayerBG<OUTPUTFORMAT, false, WILLPERFORMWINDOWTEST, false>(compState);
					}
					else
					{
						this->_RenderLine_LayerBG<OUTPUTFORMAT, false, WILLPERFORMWINDOWTEST, true>(compState);
					}
				} //layer enabled
			}
		}
		
		// render sprite Pixels
		if ( this->_enableLayer[GPULayerID_OBJ] && (item->nbPixelsX > 0) )
		{
			compState.selectedLayerID = GPULayerID_OBJ;
			compState.selectedBGLayer = NULL;
			this->_RenderLine_LayerOBJ<OUTPUTFORMAT, WILLPERFORMWINDOWTEST>(compState, item);
		}
	}
}

void GPUEngineBase::_RenderLine_SetupSprites(GPUEngineCompositorInfo &compState)
{
	itemsForPriority_t *item;
	
	//n.b. - this is clearing the sprite line buffer to the background color,
	memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>(this->_sprColor, compState.backdropColor16);
	memset(this->_sprAlpha, 0, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprType, OBJMode_Normal, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	memset(this->_sprPrio, 0x7F, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	
	//zero 06-may-09: I properly supported window color effects for backdrop, but I am not sure
	//how it interacts with this. I wish we knew why we needed this
	
	this->_SpriteRender<false>(compState, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
	this->_MosaicSpriteLine(compState, this->_sprColor, this->_sprAlpha, this->_sprType, this->_sprPrio);
	
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

template <NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineBase::_RenderLine_LayerOBJ(GPUEngineCompositorInfo &compState, itemsForPriority_t *__restrict item)
{
	if (this->vramBlockOBJIndex != VRAM_NO_3D_USAGE)
	{
		if (GPU->GetEngineMain()->VerifyVRAMLineDidChange(this->vramBlockOBJIndex, compState.lineIndexNative))
		{
			switch (OUTPUTFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					this->_LineColorCopy<true, false, false, false, 2>(compState.lineColorHeadNative, compState.lineColorHeadCustom, compState.lineIndexNative);
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					this->_LineColorCopy<true, false, false, false, 4>(compState.lineColorHeadNative, compState.lineColorHeadCustom, compState.lineIndexNative);
					break;
			}
			
			this->_LineLayerIDCopy<true, false>(compState.lineLayerIDHeadNative, compState.lineLayerIDHeadCustom, compState.lineIndexNative);
			
			compState.lineColorHead = compState.lineColorHeadNative;
			compState.lineLayerIDHead = compState.lineLayerIDHeadNative;
		}
	}
	
	const bool useCustomVRAM = (this->vramBlockOBJIndex != VRAM_NO_3D_USAGE) && !GPU->GetEngineMain()->isLineCaptureNative[this->vramBlockOBJIndex][compState.lineIndexNative];
	const u16 *__restrict srcLine = (useCustomVRAM) ? GPU->GetEngineMain()->GetCustomVRAMBlockPtr(this->vramBlockOBJIndex) + compState.blockOffsetCustom : NULL;
	if (this->isLineRenderNative[compState.lineIndexNative] && useCustomVRAM)
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineColorCopy<false, true, false, false, 2>(compState.lineColorHeadCustom, compState.lineColorHeadNative, compState.lineIndexNative);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			case NDSColorFormat_BGR888_Rev:
				this->_LineColorCopy<false, true, false, false, 4>(compState.lineColorHeadCustom, compState.lineColorHeadNative, compState.lineIndexNative);
				break;
		}
		
		this->_LineLayerIDCopy<false, true>(compState.lineLayerIDHeadCustom, compState.lineLayerIDHeadNative, compState.lineIndexNative);
		
		compState.lineColorHead = compState.lineColorHeadCustom;
		compState.lineLayerIDHead = compState.lineLayerIDHeadCustom;
		
		this->isLineRenderNative[compState.lineIndexNative] = false;
		this->nativeLineRenderCount--;
	}
	
	if (this->isLineRenderNative[compState.lineIndexNative])
	{
		for (size_t i = 0; i < item->nbPixelsX; i++)
		{
			const size_t srcX = item->PixelsX[i];
			
			compState.xNative = srcX;
			compState.xCustom = _gpuDstPitchIndex[srcX];
			compState.lineColorTarget16 = (u16 *)compState.lineColorHead + srcX;
			compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHead + srcX;
			compState.lineLayerIDTarget = compState.lineLayerIDHead + srcX;
			
			this->_RenderPixel<OUTPUTFORMAT, true, false, WILLPERFORMWINDOWTEST, false>(compState,
																					   this->_sprColor[srcX],
																					   this->_sprAlpha[srcX]);
		}
	}
	else
	{
		void *__restrict dstColorPtr = compState.lineColorHead;
		u8 *__restrict dstLayerIDPtr = compState.lineLayerIDHead;
		
		for (size_t line = 0; line < compState.lineRenderCount; line++)
		{
			compState.lineColorTarget16 = (u16 *)dstColorPtr;
			compState.lineColorTarget32 = (FragmentColor *)dstColorPtr;
			compState.lineLayerIDTarget = dstLayerIDPtr;
			
			for (size_t i = 0; i < item->nbPixelsX; i++)
			{
				const size_t srcX = item->PixelsX[i];
				compState.xNative = srcX;
				compState.xCustom = _gpuDstPitchIndex[srcX];
				
				for (size_t p = 0; p < _gpuDstPitchCount[srcX]; p++)
				{
					const size_t dstX = compState.xCustom + p;
					
					compState.lineColorTarget16 = (u16 *)dstColorPtr + dstX;
					compState.lineColorTarget32 = (FragmentColor *)dstColorPtr + dstX;
					compState.lineLayerIDTarget = dstLayerIDPtr + dstX;
					
					this->_RenderPixel<OUTPUTFORMAT, true, false, WILLPERFORMWINDOWTEST, false>(compState,
																							   (useCustomVRAM) ? srcLine[dstX] : this->_sprColor[srcX],
																							   this->_sprAlpha[srcX]);
				}
			}
			
			srcLine += compState.lineWidthCustom;
			dstColorPtr = (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev) ? (void *)((u16 *)dstColorPtr + compState.lineWidthCustom) : (void *)((FragmentColor *)dstColorPtr + compState.lineWidthCustom);
			dstLayerIDPtr += compState.lineWidthCustom;
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISFULLINTENSITYHINT>
void GPUEngineBase::ApplyMasterBrightness()
{
	const IOREG_MASTER_BRIGHT &MASTER_BRIGHT = this->_IORegisterMap->MASTER_BRIGHT;
	const u32 intensity = MASTER_BRIGHT.Intensity;
	
	if (!ISFULLINTENSITYHINT && (intensity == 0)) return;
	
	void *dst = this->renderedBuffer;
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
				
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensity);
						
						const size_t ssePixCount = pixCount - (pixCount % 8);
						for (; i < ssePixCount; i += 8)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((u16 *)dst + i));
							dstColor_vec128 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi16(0x8000));
							_mm_store_si128((__m128i *)((u16 *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((u16 *)dst)[i] = GPUEngineBase::_brightnessUpTable555[intensity][ ((u16 *)dst)[i] & 0x7FFF ] | 0x8000;
						}
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensity);
						
						const size_t ssePixCount = pixCount - (pixCount % 4);
						for (; i < ssePixCount; i += 4)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((FragmentColor *)dst + i));
							dstColor_vec128 = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000));
							_mm_store_si128((__m128i *)((FragmentColor *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((FragmentColor *)dst)[i] = this->_ColorEffectIncreaseBrightness<OUTPUTFORMAT>(((FragmentColor *)dst)[i], intensity);
							((FragmentColor *)dst)[i].a = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F : 0xFF;
						}
						break;
					}
						
					default:
						break;
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
			if (!ISFULLINTENSITYHINT && !this->_isMasterBrightFullIntensity)
			{
				size_t i = 0;
				
				switch (OUTPUTFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensity);
						
						const size_t ssePixCount = pixCount - (pixCount % 8);
						for (; i < ssePixCount; i += 8)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((u16 *)dst + i));
							dstColor_vec128 = this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi16(0x8000));
							_mm_store_si128((__m128i *)((u16 *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((u16 *)dst)[i] = GPUEngineBase::_brightnessDownTable555[intensity][ ((u16 *)dst)[i] & 0x7FFF ] | 0x8000;
						}
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
#ifdef ENABLE_SSE2
						const __m128i intensity_vec128 = _mm_set1_epi16(intensity);
						
						const size_t ssePixCount = pixCount - (pixCount % 4);
						for (; i < ssePixCount; i += 4)
						{
							__m128i dstColor_vec128 = _mm_load_si128((__m128i *)((FragmentColor *)dst + i));
							dstColor_vec128 = this->_ColorEffectDecreaseBrightness<OUTPUTFORMAT>(dstColor_vec128, intensity_vec128);
							dstColor_vec128 = _mm_or_si128(dstColor_vec128, _mm_set1_epi32((OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F000000 : 0xFF000000));
							_mm_store_si128((__m128i *)((FragmentColor *)dst + i), dstColor_vec128);
						}
#endif
						
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
						for (; i < pixCount; i++)
						{
							((FragmentColor *)dst)[i] = this->_ColorEffectDecreaseBrightness(((FragmentColor *)dst)[i], intensity);
							((FragmentColor *)dst)[i].a = (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev) ? 0x1F : 0xFF;
						}
						break;
					}
						
					default:
						break;
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
			break;
	}
}

template <size_t WIN_NUM>
bool GPUEngineBase::_IsWindowInsideVerticalRange(GPUEngineCompositorInfo &compState)
{
	const u16 windowTop    = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Top : this->_IORegisterMap->WIN1V.Top;
	const u16 windowBottom = (WIN_NUM == 0) ? this->_IORegisterMap->WIN0V.Bottom : this->_IORegisterMap->WIN1V.Bottom;
	
	if (WIN_NUM == 0 && !this->_WIN0_ENABLED) goto allout;
	if (WIN_NUM == 1 && !this->_WIN1_ENABLED) goto allout;

	if (windowTop > windowBottom)
	{
		if ((compState.lineIndexNative < windowTop) && (compState.lineIndexNative > windowBottom)) goto allout;
	}
	else
	{
		if ((compState.lineIndexNative < windowTop) || (compState.lineIndexNative >= windowBottom)) goto allout;
	}

	//the x windows will apply for this scanline
	return true;
	
allout:
	return false;
}

template <size_t WIN_NUM>
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

void GPUEngineBase::_PerformWindowTesting(GPUEngineCompositorInfo &compState)
{
	if (this->_needUpdateWINH[0]) this->_UpdateWINH<0>();
	if (this->_needUpdateWINH[1]) this->_UpdateWINH<1>();
	
	for (size_t layerID = GPULayerID_BG0; layerID <= GPULayerID_OBJ; layerID++)
	{
		if (!this->_enableLayer[layerID])
		{
			continue;
		}
		
#ifdef ENABLE_SSE2
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=16)
		{
			__m128i win_vec128;
			
			__m128i didPassWindowTest = _mm_setzero_si128();
			__m128i enableColorEffect = _mm_setzero_si128();
			
			__m128i win0HandledMask = _mm_setzero_si128();
			__m128i win1HandledMask = _mm_setzero_si128();
			__m128i winOBJHandledMask = _mm_setzero_si128();
			__m128i winOUTHandledMask = _mm_setzero_si128();
			
			// Window 0 has the highest priority, so always check this first.
			if (this->_WIN0_ENABLED && this->_IsWindowInsideVerticalRange<0>(compState))
			{
				win_vec128 = _mm_load_si128((__m128i *)(this->_h_win[0] + i));
				win0HandledMask = _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1));
				
				didPassWindowTest = _mm_and_si128(win0HandledMask, this->_WIN0_enable_SSE2[layerID]);
				enableColorEffect = _mm_and_si128(win0HandledMask, this->_WIN0_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]);
			}
			
			// Window 1 has medium priority, and is checked after Window 0.
			if (this->_WIN1_ENABLED && this->_IsWindowInsideVerticalRange<1>(compState))
			{
				win_vec128 = _mm_load_si128((__m128i *)(this->_h_win[1] + i));
				win1HandledMask = _mm_andnot_si128(win0HandledMask, _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)));
				
				didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(win1HandledMask, this->_WIN1_enable_SSE2[layerID]) );
				enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(win1HandledMask, this->_WIN1_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]) );
			}
			
			// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
			if (this->_WINOBJ_ENABLED)
			{
				win_vec128 = _mm_load_si128((__m128i *)(this->_sprWin + i));
				winOBJHandledMask = _mm_andnot_si128( _mm_or_si128(win0HandledMask, win1HandledMask), _mm_cmpeq_epi8(win_vec128, _mm_set1_epi8(1)) );
				
				didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOBJHandledMask, this->_WINOBJ_enable_SSE2[layerID]) );
				enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOBJHandledMask, this->_WINOBJ_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]) );
			}
			
			// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
			// This has the lowest priority, and is always checked last.
			winOUTHandledMask = _mm_xor_si128( _mm_or_si128(win0HandledMask, _mm_or_si128(win1HandledMask, winOBJHandledMask)), _mm_set1_epi32(0xFFFFFFFF) );
			didPassWindowTest = _mm_or_si128( didPassWindowTest, _mm_and_si128(winOUTHandledMask, this->_WINOUT_enable_SSE2[layerID]) );
			enableColorEffect = _mm_or_si128( enableColorEffect, _mm_and_si128(winOUTHandledMask, this->_WINOUT_enable_SSE2[WINDOWCONTROL_EFFECTFLAG]) );
			
			_mm_store_si128((__m128i *)(this->_didPassWindowTestNative[layerID] + i), _mm_and_si128(didPassWindowTest, _mm_set1_epi8(0x01)));
			_mm_store_si128((__m128i *)(this->_enableColorEffectNative[layerID] + i), _mm_and_si128(enableColorEffect, _mm_set1_epi8(0x01)));
		}
#else
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i++)
		{
			// Window 0 has the highest priority, so always check this first.
			if (this->_WIN0_ENABLED && this->_IsWindowInsideVerticalRange<0>(compState))
			{
				if (this->_h_win[0][i] != 0)
				{
					this->_didPassWindowTestNative[layerID][i] = this->_WIN0_enable[layerID];
					this->_enableColorEffectNative[layerID][i] = this->_WIN0_enable[WINDOWCONTROL_EFFECTFLAG];
					continue;
				}
			}
			
			// Window 1 has medium priority, and is checked after Window 0.
			if (this->_WIN1_ENABLED && this->_IsWindowInsideVerticalRange<1>(compState))
			{
				if (this->_h_win[1][i] != 0)
				{
					this->_didPassWindowTestNative[layerID][i] = this->_WIN1_enable[layerID];
					this->_enableColorEffectNative[layerID][i] = this->_WIN1_enable[WINDOWCONTROL_EFFECTFLAG];
					continue;
				}
			}
			
			// Window OBJ has low priority, and is checked after both Window 0 and Window 1.
			if (this->_WINOBJ_ENABLED)
			{
				if (this->_sprWin[i] != 0)
				{
					this->_didPassWindowTestNative[layerID][i] = this->_WINOBJ_enable[layerID];
					this->_enableColorEffectNative[layerID][i] = this->_WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG];
					continue;
				}
			}
			
			// If the pixel isn't inside any windows, then the pixel is outside, and therefore uses the WINOUT flags.
			// This has the lowest priority, and is always checked last.
			this->_didPassWindowTestNative[layerID][i] = this->_WINOUT_enable[layerID];
			this->_enableColorEffectNative[layerID][i] = this->_WINOUT_enable[WINDOWCONTROL_EFFECTFLAG];
		}
#endif
		
		if (GPU->GetDisplayInfo().isCustomSizeRequested)
		{
			ExpandLine8(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], compState.lineWidthCustom);
			ExpandLine8(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], compState.lineWidthCustom);
		}
		else
		{
			memcpy(this->_didPassWindowTestCustom[layerID], this->_didPassWindowTestNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
			memcpy(this->_enableColorEffectCustom[layerID], this->_enableColorEffectNative[layerID], GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u8));
		}
	}
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

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_LayerBG_Final(GPUEngineCompositorInfo &compState)
{
	bool useCustomVRAM = false;
	
	switch (compState.selectedBGLayer->baseType)
	{
		case BGType_Text: this->_LineText<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState); break;
		case BGType_Affine: this->_LineRot<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState); break;
		case BGType_AffineExt: this->_LineExtRot<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, useCustomVRAM); break;
		case BGType_Large8bpp: this->_LineExtRot<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState, useCustomVRAM); break;
		case BGType_Invalid:
			PROGINFO("Attempting to render an invalid BG type\n");
			break;
		default:
			break;
	}
	
	// If rendering at the native size, each pixel is rendered the moment it is gathered.
	// However, if rendering at a custom size, pixel gathering and pixel rendering are split
	// up into separate steps. If rendering at a custom size, do the pixel rendering step now.
	if ( !ISDEBUGRENDER && (ISCUSTOMRENDERINGNEEDED || !this->isLineRenderNative[compState.lineIndexNative]) )
	{
		compState.lineLayerIDTarget = compState.lineLayerIDHeadCustom;
		
		if (useCustomVRAM)
		{
			this->_RenderPixelsCustomVRAM<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState);
		}
		else
		{
			this->_RenderPixelsCustom<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState);
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_LayerBG_ApplyColorEffectDisabledHint(GPUEngineCompositorInfo &compState)
{
	this->_RenderLine_LayerBG_Final<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT, ISCUSTOMRENDERINGNEEDED>(compState);
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_LayerBG_ApplyMosaic(GPUEngineCompositorInfo &compState)
{
	if (compState.colorEffect == ColorEffect_Disable)
	{
		this->_RenderLine_LayerBG_ApplyColorEffectDisabledHint<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, true, ISCUSTOMRENDERINGNEEDED>(compState);
	}
	else
	{
		this->_RenderLine_LayerBG_ApplyColorEffectDisabledHint<OUTPUTFORMAT, ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, false, ISCUSTOMRENDERINGNEEDED>(compState);
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER, bool WILLPERFORMWINDOWTEST, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineBase::_RenderLine_LayerBG(GPUEngineCompositorInfo &compState)
{
	if (ISDEBUGRENDER)
	{
		this->_RenderLine_LayerBG_Final<OUTPUTFORMAT, ISDEBUGRENDER, false, true, true, false>(compState);
	}
	else
	{
#ifndef DISABLE_MOSAIC
		if (compState.selectedBGLayer->isMosaic && this->_isBGMosaicSet)
		{
			this->_RenderLine_LayerBG_ApplyMosaic<OUTPUTFORMAT, ISDEBUGRENDER, true, WILLPERFORMWINDOWTEST, ISCUSTOMRENDERINGNEEDED>(compState);
		}
		else
#endif
		{
			this->_RenderLine_LayerBG_ApplyMosaic<OUTPUTFORMAT, ISDEBUGRENDER, false, WILLPERFORMWINDOWTEST, ISCUSTOMRENDERINGNEEDED>(compState);
		}
	}
}

void GPUEngineBase::RenderLayerBG(const GPULayerID layerID, u16 *dstColorBuffer)
{
	GPUEngineCompositorInfo compState;
	
	compState.selectedLayerID = layerID;
	compState.selectedBGLayer = &this->_BGLayer[layerID];
	compState.backdropColor16 = LE_TO_LOCAL_16(this->_paletteBG[0]) & 0x7FFF;
	compState.colorEffect = ColorEffect_Disable;
	compState.blendEVA = this->_BLDALPHA_EVA;
	compState.blendEVB = this->_BLDALPHA_EVB;
	compState.blendEVY = this->_BLDALPHA_EVY;
	compState.blendTable555 = this->_selectedBlendTable555;
	compState.brightnessUpTable555 = this->_selectedBrightnessUpTable555;
	compState.brightnessUpTable666 = this->_selectedBrightnessUpTable666;
	compState.brightnessUpTable888 = this->_selectedBrightnessUpTable888;
	compState.brightnessDownTable555 = this->_selectedBrightnessDownTable555;
	compState.brightnessDownTable666 = this->_selectedBrightnessDownTable666;
	compState.brightnessDownTable888 = this->_selectedBrightnessDownTable888;
	
	compState.lineLayerIDHead = NULL;
	compState.lineLayerIDHeadNative = NULL;
	compState.lineLayerIDHeadCustom = NULL;
	
	compState.xNative = 0;
	compState.xCustom = 0;
	compState.lineColorTarget = (void **)&compState.lineColorTarget16;
	compState.lineColorTarget16 = (u16 *)compState.lineColorHeadNative;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHeadNative;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
	
	const size_t layerWidth = compState.selectedBGLayer->size.width;
	const size_t layerHeight = compState.selectedBGLayer->size.height;
	
	for (size_t lineIndex = 0; lineIndex < layerHeight; lineIndex++)
	{
		compState.lineIndexNative = lineIndex;
		compState.lineIndexCustom = lineIndex;
		compState.lineWidthCustom = layerWidth;
		compState.lineRenderCount = 1;
		compState.linePixelCount = compState.lineWidthCustom * compState.lineRenderCount;
		compState.blockOffsetNative = compState.lineIndexNative * layerWidth;
		compState.blockOffsetCustom = compState.lineIndexCustom * compState.lineWidthCustom;
		
		compState.lineColorHead = (u16 *)dstColorBuffer + compState.blockOffsetNative;
		compState.lineColorHeadNative = compState.lineColorHead;
		compState.lineColorHeadCustom = compState.lineColorHead;
		
		this->_RenderLine_LayerBG<NDSColorFormat_BGR555_Rev, true, true, false>(compState);
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_HandleDisplayModeOff(const size_t l)
{
	// Native rendering only.
	// In this display mode, the display is cleared to white.
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
			memset_u16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u16 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFF);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u32 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0x1F3F3F3F);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			memset_u32_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH>((u32 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH), 0xFFFFFFFF);
			break;
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineBase::_HandleDisplayModeNormal(const size_t l)
{
	if (!this->isLineRenderNative[l])
	{
		this->isLineOutputNative[l] = false;
		this->nativeLineOutputCount--;
	}
}

template <size_t WINNUM>
void GPUEngineBase::ParseReg_WINnH()
{
	this->_needUpdateWINH[WINNUM] = true;
}

void GPUEngineBase::ParseReg_WININ()
{
	this->_WIN0_enable[GPULayerID_BG0] = this->_IORegisterMap->WIN0IN.BG0_Enable;
	this->_WIN0_enable[GPULayerID_BG1] = this->_IORegisterMap->WIN0IN.BG1_Enable;
	this->_WIN0_enable[GPULayerID_BG2] = this->_IORegisterMap->WIN0IN.BG2_Enable;
	this->_WIN0_enable[GPULayerID_BG3] = this->_IORegisterMap->WIN0IN.BG3_Enable;
	this->_WIN0_enable[GPULayerID_OBJ] = this->_IORegisterMap->WIN0IN.OBJ_Enable;
	this->_WIN0_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WIN0IN.Effect_Enable;
	
	this->_WIN1_enable[GPULayerID_BG0] = this->_IORegisterMap->WIN1IN.BG0_Enable;
	this->_WIN1_enable[GPULayerID_BG1] = this->_IORegisterMap->WIN1IN.BG1_Enable;
	this->_WIN1_enable[GPULayerID_BG2] = this->_IORegisterMap->WIN1IN.BG2_Enable;
	this->_WIN1_enable[GPULayerID_BG3] = this->_IORegisterMap->WIN1IN.BG3_Enable;
	this->_WIN1_enable[GPULayerID_OBJ] = this->_IORegisterMap->WIN1IN.OBJ_Enable;
	this->_WIN1_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WIN1IN.Effect_Enable;
	
#if defined(ENABLE_SSE2)
	this->_WIN0_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG0_Enable != 0) ? 0xFF : 0x00);
	this->_WIN0_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG1_Enable != 0) ? 0xFF : 0x00);
	this->_WIN0_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG2_Enable != 0) ? 0xFF : 0x00);
	this->_WIN0_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.BG3_Enable != 0) ? 0xFF : 0x00);
	this->_WIN0_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.OBJ_Enable != 0) ? 0xFF : 0x00);
	this->_WIN0_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WIN0IN.Effect_Enable != 0) ? 0xFF : 0x00);
	
	this->_WIN1_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG0_Enable != 0) ? 0xFF : 0x00);
	this->_WIN1_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG1_Enable != 0) ? 0xFF : 0x00);
	this->_WIN1_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG2_Enable != 0) ? 0xFF : 0x00);
	this->_WIN1_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.BG3_Enable != 0) ? 0xFF : 0x00);
	this->_WIN1_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.OBJ_Enable != 0) ? 0xFF : 0x00);
	this->_WIN1_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WIN1IN.Effect_Enable != 0) ? 0xFF : 0x00);
#endif
}

void GPUEngineBase::ParseReg_WINOUT()
{
	this->_WINOUT_enable[GPULayerID_BG0] = this->_IORegisterMap->WINOUT.BG0_Enable;
	this->_WINOUT_enable[GPULayerID_BG1] = this->_IORegisterMap->WINOUT.BG1_Enable;
	this->_WINOUT_enable[GPULayerID_BG2] = this->_IORegisterMap->WINOUT.BG2_Enable;
	this->_WINOUT_enable[GPULayerID_BG3] = this->_IORegisterMap->WINOUT.BG3_Enable;
	this->_WINOUT_enable[GPULayerID_OBJ] = this->_IORegisterMap->WINOUT.OBJ_Enable;
	this->_WINOUT_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WINOUT.Effect_Enable;
	
	this->_WINOBJ_enable[GPULayerID_BG0] = this->_IORegisterMap->WINOBJ.BG0_Enable;
	this->_WINOBJ_enable[GPULayerID_BG1] = this->_IORegisterMap->WINOBJ.BG1_Enable;
	this->_WINOBJ_enable[GPULayerID_BG2] = this->_IORegisterMap->WINOBJ.BG2_Enable;
	this->_WINOBJ_enable[GPULayerID_BG3] = this->_IORegisterMap->WINOBJ.BG3_Enable;
	this->_WINOBJ_enable[GPULayerID_OBJ] = this->_IORegisterMap->WINOBJ.OBJ_Enable;
	this->_WINOBJ_enable[WINDOWCONTROL_EFFECTFLAG] = this->_IORegisterMap->WINOBJ.Effect_Enable;
	
#if defined(ENABLE_SSE2)
	this->_WINOUT_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG0_Enable != 0) ? 0xFF : 0x00);
	this->_WINOUT_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG1_Enable != 0) ? 0xFF : 0x00);
	this->_WINOUT_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG2_Enable != 0) ? 0xFF : 0x00);
	this->_WINOUT_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.BG3_Enable != 0) ? 0xFF : 0x00);
	this->_WINOUT_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.OBJ_Enable != 0) ? 0xFF : 0x00);
	this->_WINOUT_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WINOUT.Effect_Enable != 0) ? 0xFF : 0x00);
	
	this->_WINOBJ_enable_SSE2[GPULayerID_BG0] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG0_Enable != 0) ? 0xFF : 0x00);
	this->_WINOBJ_enable_SSE2[GPULayerID_BG1] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG1_Enable != 0) ? 0xFF : 0x00);
	this->_WINOBJ_enable_SSE2[GPULayerID_BG2] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG2_Enable != 0) ? 0xFF : 0x00);
	this->_WINOBJ_enable_SSE2[GPULayerID_BG3] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.BG3_Enable != 0) ? 0xFF : 0x00);
	this->_WINOBJ_enable_SSE2[GPULayerID_OBJ] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.OBJ_Enable != 0) ? 0xFF : 0x00);
	this->_WINOBJ_enable_SSE2[WINDOWCONTROL_EFFECTFLAG] = _mm_set1_epi8((this->_IORegisterMap->WINOBJ.Effect_Enable != 0) ? 0xFF : 0x00);
#endif
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
	
	this->_srcBlendEnable[GPULayerID_BG0] = (BLDCNT.BG0_Target1 != 0);
	this->_srcBlendEnable[GPULayerID_BG1] = (BLDCNT.BG1_Target1 != 0);
	this->_srcBlendEnable[GPULayerID_BG2] = (BLDCNT.BG2_Target1 != 0);
	this->_srcBlendEnable[GPULayerID_BG3] = (BLDCNT.BG3_Target1 != 0);
	this->_srcBlendEnable[GPULayerID_OBJ] = (BLDCNT.OBJ_Target1 != 0);
	this->_srcBlendEnable[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target1 != 0);
	
	this->_dstBlendEnable[GPULayerID_BG0] = (BLDCNT.BG0_Target2 != 0);
	this->_dstBlendEnable[GPULayerID_BG1] = (BLDCNT.BG1_Target2 != 0);
	this->_dstBlendEnable[GPULayerID_BG2] = (BLDCNT.BG2_Target2 != 0);
	this->_dstBlendEnable[GPULayerID_BG3] = (BLDCNT.BG3_Target2 != 0);
	this->_dstBlendEnable[GPULayerID_OBJ] = (BLDCNT.OBJ_Target2 != 0);
	this->_dstBlendEnable[GPULayerID_Backdrop] = (BLDCNT.Backdrop_Target2 != 0);
	
#ifdef ENABLE_SSE2
	const __m128i one_vec128 = _mm_set1_epi8(1);
	
	this->_srcBlendEnable_SSE2[GPULayerID_BG0] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG0_Target1), one_vec128);
	this->_srcBlendEnable_SSE2[GPULayerID_BG1] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG1_Target1), one_vec128);
	this->_srcBlendEnable_SSE2[GPULayerID_BG2] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG2_Target1), one_vec128);
	this->_srcBlendEnable_SSE2[GPULayerID_BG3] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG3_Target1), one_vec128);
	this->_srcBlendEnable_SSE2[GPULayerID_OBJ] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.OBJ_Target1), one_vec128);
	this->_srcBlendEnable_SSE2[GPULayerID_Backdrop] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.Backdrop_Target1), one_vec128);
	
#ifdef ENABLE_SSSE3
	this->_dstBlendEnable_SSSE3 = _mm_set_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
											   BLDCNT.Backdrop_Target2,
											   BLDCNT.OBJ_Target2,
											   BLDCNT.BG3_Target2,
											   BLDCNT.BG2_Target2,
											   BLDCNT.BG1_Target2,
											   BLDCNT.BG0_Target2);
#else
	this->_dstBlendEnable_SSE2[GPULayerID_BG0] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG0_Target2), one_vec128);
	this->_dstBlendEnable_SSE2[GPULayerID_BG1] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG1_Target2), one_vec128);
	this->_dstBlendEnable_SSE2[GPULayerID_BG2] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG2_Target2), one_vec128);
	this->_dstBlendEnable_SSE2[GPULayerID_BG3] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.BG3_Target2), one_vec128);
	this->_dstBlendEnable_SSE2[GPULayerID_OBJ] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.OBJ_Target2), one_vec128);
	this->_dstBlendEnable_SSE2[GPULayerID_Backdrop] = _mm_cmpeq_epi8(_mm_set1_epi8(BLDCNT.Backdrop_Target2), one_vec128);
#endif
	
#endif // ENABLE_SSE2
}

void GPUEngineBase::ParseReg_BLDALPHA()
{
	const IOREG_BLDALPHA &BLDALPHA = this->_IORegisterMap->BLDALPHA;
	
	this->_BLDALPHA_EVA = (BLDALPHA.EVA >= 16) ? 16 : BLDALPHA.EVA;
	this->_BLDALPHA_EVB = (BLDALPHA.EVB >= 16) ? 16 : BLDALPHA.EVB;
	this->_selectedBlendTable555 = (TBlendTable *)&GPUEngineBase::_blendTable555[this->_BLDALPHA_EVA][this->_BLDALPHA_EVB][0][0];
}

void GPUEngineBase::ParseReg_BLDY()
{
	const IOREG_BLDY &BLDY = this->_IORegisterMap->BLDY;
	
	this->_BLDALPHA_EVY = (BLDY.EVY >= 16) ? 16 : BLDY.EVY;
	this->_selectedBrightnessUpTable555 = &GPUEngineBase::_brightnessUpTable555[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessUpTable666 = &GPUEngineBase::_brightnessUpTable666[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessUpTable888 = &GPUEngineBase::_brightnessUpTable888[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessDownTable555 = &GPUEngineBase::_brightnessDownTable555[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessDownTable666 = &GPUEngineBase::_brightnessDownTable666[this->_BLDALPHA_EVY][0];
	this->_selectedBrightnessDownTable888 = &GPUEngineBase::_brightnessDownTable888[this->_BLDALPHA_EVY][0];
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
	this->nativeBuffer = dispInfo.nativeBuffer[theDisplayID];
	this->customBuffer = dispInfo.customBuffer[theDisplayID];
}

GPUEngineID GPUEngineBase::GetEngineID() const
{
	return this->_engineID;
}

void GPUEngineBase::SetCustomFramebufferSize(size_t w, size_t h)
{
	void *oldWorkingLineColor = this->_internalRenderLineTargetCustom;
	u8 *oldWorkingLineLayerID = this->_renderLineLayerIDCustom;
	u8 *oldBGLayerIndexCustom = this->_bgLayerIndexCustom;
	u16 *oldBGLayerColorCustom = this->_bgLayerColorCustom;
	u8 *oldDidPassWindowTestCustomMasterPtr = this->_didPassWindowTestCustomMasterPtr;
	
	void *newWorkingLineColor = malloc_alignedCacheLine(w * _gpuLargestDstLineCount * GPU->GetDisplayInfo().pixelBytes);
	u8 *newWorkingLineLayerID = (u8 *)malloc_alignedCacheLine(w * _gpuLargestDstLineCount * 4 * sizeof(u8)); // yes indeed, this is oversized. map debug tools try to write to it
	u8 *newBGLayerIndexCustom = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u16 *newBGLayerColorCustom = (u16 *)malloc_alignedCacheLine(w * sizeof(u16));
	u8 *newDidPassWindowTestCustomMasterPtr = (u8 *)malloc_alignedCacheLine(w * 10 * sizeof(u8));
	
	this->_internalRenderLineTargetCustom = newWorkingLineColor;
	this->_renderLineLayerIDCustom = newWorkingLineLayerID;
	this->nativeBuffer = GPU->GetDisplayInfo().nativeBuffer[this->_targetDisplayID];
	this->customBuffer = GPU->GetDisplayInfo().customBuffer[this->_targetDisplayID];
	this->_bgLayerIndexCustom = newBGLayerIndexCustom;
	this->_bgLayerColorCustom = newBGLayerColorCustom;
	
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
	
	this->_needUpdateWINH[0] = true;
	this->_needUpdateWINH[1] = true;
	
	free_aligned(oldWorkingLineColor);
	free_aligned(oldWorkingLineLayerID);
	free_aligned(oldBGLayerIndexCustom);
	free_aligned(oldBGLayerColorCustom);
	free_aligned(oldDidPassWindowTestCustomMasterPtr);
}

template <NDSColorFormat OUTPUTFORMAT>
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
	if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
	{
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			if (this->isLineOutputNative[y])
			{
				this->_LineColorCopy<false, true, true, false, 2>(this->customBuffer, this->nativeBuffer, y);
				this->isLineOutputNative[y] = false;
			}
		}
	}
	else
	{
		for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
		{
			if (this->isLineOutputNative[y])
			{
				this->_LineColorCopy<false, true, true, false, 4>(this->customBuffer, this->nativeBuffer, y);
				this->isLineOutputNative[y] = false;
			}
		}
	}
	
	this->nativeLineOutputCount = 0;
	this->renderedWidth = dispInfo.customWidth;
	this->renderedHeight = dispInfo.customHeight;
	this->renderedBuffer = this->customBuffer;
}

void GPUEngineBase::ResolveRGB666ToRGB888()
{
	ConvertColorBuffer6665To8888<false>((u32 *)this->renderedBuffer, (u32 *)this->renderedBuffer, this->renderedWidth * this->renderedHeight);
}

void GPUEngineBase::ResolveToCustomFramebuffer()
{
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	
	if (this->nativeLineOutputCount == 0)
	{
		return;
	}
	
	if (dispInfo.isCustomSizeRequested)
	{
		if (dispInfo.pixelBytes == 2)
		{
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				this->_LineColorCopy<false, true, true, false, 2>(this->customBuffer, this->nativeBuffer, y);
			}
		}
		else if (dispInfo.pixelBytes == 4)
		{
			for (size_t y = 0; y < GPU_FRAMEBUFFER_NATIVE_HEIGHT; y++)
			{
				this->_LineColorCopy<false, true, true, false, 4>(this->customBuffer, this->nativeBuffer, y);
			}
		}
	}
	else
	{
		memcpy(this->customBuffer, this->nativeBuffer, GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * dispInfo.pixelBytes);
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
	
	this->ParseReg_WINnH<0>();
	this->ParseReg_WINnH<1>();
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

bool GPUEngineA::WillCapture3DLayerDirect(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return ( this->WillDisplayCapture(l) && (DISPCAPCNT.SrcA != 0) && (DISPCAPCNT.CaptureSrc != 1) );
}

bool GPUEngineA::WillDisplayCapture(const size_t l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	return (DISPCAPCNT.CaptureEnable != 0) && (vramConfiguration.banks[DISPCAPCNT.VRAMWriteBlock].purpose == VramConfiguration::LCDC) && (l < this->_dispCapCnt.capy);
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
		this->_LineColorCopy<true, true, true, false, 2>(this->_VRAMNativeBlockCaptureCopyPtr[blockID], this->_VRAMNativeBlockPtr[blockID], l);
		this->isLineCaptureNative[blockID][l] = true;
		this->nativeLineCaptureCount[blockID]++;
	}
	
	return didVRAMLineChange;
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::RenderLine(const u16 l)
{
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	const bool isDisplayCaptureNeeded = this->WillDisplayCapture(l);
	
	// Render the line
	if ( (this->_displayOutputMode == GPUDisplayMode_Normal) || isDisplayCaptureNeeded )
	{
		if (this->_isAnyWindowEnabled)
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, true>(l);
		}
		else
		{
			this->_RenderLine_Layers<OUTPUTFORMAT, false>(l);
		}
	}
	
	// Fill the display output
	switch (this->_displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
			this->_HandleDisplayModeNormal<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_VRAM: // Display vram framebuffer
			this->_HandleDisplayModeVRAM<OUTPUTFORMAT>(l);
			break;
			
		case GPUDisplayMode_MainMemory: // Display memory FIFO
			this->_HandleDisplayModeMainMemory<OUTPUTFORMAT>(l);
			break;
	}
	
	//capture after displaying so that we can safely display vram before overwriting it here
	
	//BUG!!! if someone is capturing and displaying both from the fifo, then it will have been
	//consumed above by the display before we get here
	//(is that even legal? i think so)
	if (isDisplayCaptureNeeded)
	{
		if (DISPCAPCNT.CaptureSize == DisplayCaptureSize_128x128)
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH/2>(l);
		}
		else
		{
			this->_RenderLine_DisplayCapture<OUTPUTFORMAT, GPU_FRAMEBUFFER_NATIVE_WIDTH>(l);
		}
	}
}

template <NDSColorFormat OUTPUTFORMAT, bool WILLPERFORMWINDOWTEST>
void GPUEngineA::RenderLine_Layer3D(GPUEngineCompositorInfo &compState)
{
	const FragmentColor *__restrict framebuffer3D = CurrentRenderer->GetFramebuffer();
	if (framebuffer3D == NULL)
	{
		return;
	}
	
	if (this->isLineRenderNative[compState.lineIndexNative] && !CurrentRenderer->IsFramebufferNativeSize())
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineColorCopy<false, true, false, false, 2>(compState.lineColorHeadCustom, compState.lineColorHeadNative, compState.lineIndexNative);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			case NDSColorFormat_BGR888_Rev:
				this->_LineColorCopy<false, true, false, false, 4>(compState.lineColorHeadCustom, compState.lineColorHeadNative, compState.lineIndexNative);
				break;
		}
		
		this->_LineLayerIDCopy<false, true>(compState.lineLayerIDHeadCustom, compState.lineLayerIDHeadNative, compState.lineIndexNative);
		
		compState.lineColorHead = compState.lineColorHeadCustom;
		compState.lineLayerIDHead = compState.lineLayerIDHeadCustom;
		this->isLineRenderNative[compState.lineIndexNative] = false;
		this->nativeLineRenderCount--;
	}
	
	const float customWidthScale = (float)compState.lineWidthCustom / (float)GPU_FRAMEBUFFER_NATIVE_WIDTH;
	const FragmentColor *__restrict srcLinePtr = framebuffer3D + compState.blockOffsetCustom;
	
	compState.lineColorTarget16 = (u16 *)compState.lineColorHead;
	compState.lineColorTarget32 = (FragmentColor *)compState.lineColorHead;
	compState.lineLayerIDTarget = compState.lineLayerIDHead;
	
	// Horizontally offset the 3D layer by this amount.
	// Test case: Blowing up large objects in Nanostray 2 will cause the main screen to shake horizontally.
	const u16 hofs = (u16)( ((float)compState.selectedBGLayer->xOffset * customWidthScale) + 0.5f );
	
	if (hofs == 0)
	{
		for (size_t line = 0; line < compState.lineRenderCount; line++)
		{
			compState.xNative = 0;
			compState.xCustom = 0;
#ifdef ENABLE_SSE2
			const size_t ssePixCount = compState.lineWidthCustom - (compState.lineWidthCustom % 16);
			
			for (; compState.xCustom < ssePixCount; srcLinePtr+=16, compState.xCustom+=16, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16+=16, compState.lineColorTarget32+=16, compState.lineLayerIDTarget+=16)
			{
				const __m128i src[4]	= { _mm_load_si128((__m128i *)srcLinePtr + 0),
										    _mm_load_si128((__m128i *)srcLinePtr + 1),
										    _mm_load_si128((__m128i *)srcLinePtr + 2),
										    _mm_load_si128((__m128i *)srcLinePtr + 3) };
				
				// Determine which pixels pass by doing the alpha test and the window test.
				const __m128i srcAlpha = _mm_packs_epi16( _mm_packs_epi32(_mm_srli_epi32(src[0], 24), _mm_srli_epi32(src[1], 24)),
														  _mm_packs_epi32(_mm_srli_epi32(src[2], 24), _mm_srli_epi32(src[3], 24)) );
				__m128i passMask8;
				__m128i enableColorEffectMask;
				
				if (WILLPERFORMWINDOWTEST)
				{
					// Do the window test.
					passMask8 = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_didPassWindowTestCustom[compState.selectedLayerID] + compState.xCustom)), _mm_set1_epi8(1) );
					enableColorEffectMask = _mm_cmpeq_epi8( _mm_load_si128((__m128i *)(this->_enableColorEffectCustom[compState.selectedLayerID] + compState.xCustom)), _mm_set1_epi8(1) );
				}
				else
				{
					passMask8 = _mm_set1_epi8(0xFF);
					enableColorEffectMask = _mm_set1_epi8(0xFF);
				}
				
				// Do the alpha test. Pixels with an alpha value of 0 are rejected.
				passMask8 = _mm_andnot_si128(_mm_cmpeq_epi8(srcAlpha, _mm_setzero_si128()), passMask8);
				
				// If none of the pixels within the vector pass, then reject them all at once.
				if (_mm_movemask_epi8(passMask8) == 0)
				{
					continue;
				}
				
				// Perform the blending function.
				__m128i dstLayerID_vec128 = _mm_load_si128((__m128i *)compState.lineLayerIDTarget);
				
				__m128i dst[4];
				dst[0] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 0);
				dst[1] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 1);
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
				{
					// Instead of letting these vectors go to waste, let's convert the src colors to 16-bit now and
					// then pack the converted 16-bit colors into these vectors.
					dst[2] = _mm_packs_epi32( _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src[0], _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src[0], _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src[0], _mm_set1_epi32(0x003E0000)), 7)),
											  _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src[1], _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src[1], _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src[1], _mm_set1_epi32(0x003E0000)), 7)) );
					dst[3] = _mm_packs_epi32( _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src[2], _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src[2], _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src[2], _mm_set1_epi32(0x003E0000)), 7)),
											  _mm_or_si128(_mm_or_si128(_mm_srli_epi32(_mm_and_si128(src[3], _mm_set1_epi32(0x0000003E)), 1), _mm_srli_epi32(_mm_and_si128(src[3], _mm_set1_epi32(0x00003E00)), 4)), _mm_srli_epi32(_mm_and_si128(src[3], _mm_set1_epi32(0x003E0000)), 7)) );
				}
				else
				{
					dst[2] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 2);
					dst[3] = _mm_load_si128((__m128i *)*compState.lineColorTarget + 3);
				}
				
				this->_RenderPixel3D_SSE2<OUTPUTFORMAT>(compState,
														passMask8,
														enableColorEffectMask,
														src[3], src[2], src[1], src[0],
														dst[3], dst[2], dst[1], dst[0],
														dstLayerID_vec128);
				
				_mm_store_si128((__m128i *)*compState.lineColorTarget + 0, dst[0]);
				_mm_store_si128((__m128i *)*compState.lineColorTarget + 1, dst[1]);
				
				if (OUTPUTFORMAT != NDSColorFormat_BGR555_Rev)
				{
					_mm_store_si128((__m128i *)*compState.lineColorTarget + 2, dst[2]);
					_mm_store_si128((__m128i *)*compState.lineColorTarget + 3, dst[3]);
				}
				
				_mm_store_si128((__m128i *)compState.lineLayerIDTarget, dstLayerID_vec128);
			}
#endif
			
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
			for (; compState.xCustom < compState.lineWidthCustom; srcLinePtr++, compState.xCustom++, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16++, compState.lineColorTarget32++, compState.lineLayerIDTarget++)
			{
				if ( (srcLinePtr->a == 0) || (WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compState.selectedLayerID][compState.xCustom] == 0)) )
				{
					continue;
				}
				
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compState.selectedLayerID][compState.xCustom] != 0) : true;
				
				this->_RenderPixel3D<OUTPUTFORMAT>(compState,
												   enableColorEffect,
												   *srcLinePtr);
			}
		}
	}
	else
	{
		for (size_t line = 0; line < compState.lineRenderCount; line++)
		{
			for (compState.xNative = 0, compState.xCustom = 0; compState.xCustom < compState.lineWidthCustom; compState.xCustom++, compState.xNative = _gpuDstToSrcIndex[compState.xCustom], compState.lineColorTarget16++, compState.lineColorTarget32++, compState.lineLayerIDTarget++)
			{
				if ( WILLPERFORMWINDOWTEST && (this->_didPassWindowTestCustom[compState.selectedLayerID][compState.xCustom] == 0) )
				{
					continue;
				}
				
				size_t srcX = compState.xCustom + hofs;
				if (srcX >= compState.lineWidthCustom * 2)
				{
					srcX -= compState.lineWidthCustom * 2;
				}
				
				if ( (srcX >= compState.lineWidthCustom) || (srcLinePtr[srcX].a == 0) )
				{
					continue;
				}
				
				compState.xNative = _gpuDstToSrcIndex[compState.xCustom];
				const bool enableColorEffect = (WILLPERFORMWINDOWTEST) ? (this->_enableColorEffectCustom[compState.selectedLayerID][compState.xCustom] != 0) : true;
				
				this->_RenderPixel3D<OUTPUTFORMAT>(compState,
												   enableColorEffect,
												   srcLinePtr[srcX]);
			}
			
			srcLinePtr += compState.lineWidthCustom;
		}
	}
}

template<NDSColorFormat OUTPUTFORMAT, size_t CAPTURELENGTH>
void GPUEngineA::_RenderLine_DisplayCapture(const u16 l)
{
	assert( (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH/2) || (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH) );
	
	GPUEngineCompositorInfo &compState = this->_currentCompositorState;
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	const IOREG_DISPCAPCNT &DISPCAPCNT = this->_IORegisterMap->DISPCAPCNT;
	
	const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
	const bool is3DFramebufferNativeSize = CurrentRenderer->IsFramebufferNativeSize();
	const u8 vramWriteBlock = DISPCAPCNT.VRAMWriteBlock;
	const u8 vramReadBlock = DISPCNT.VRAM_Block;
	const size_t writeLineIndexWithOffset = (DISPCAPCNT.VRAMWriteOffset * 64) + l;
	const size_t readLineIndexWithOffset = (this->_dispCapCnt.readOffset * 64) + l;
	bool newCaptureLineNativeState = true;
	u16 *renderedLineSrcA16 = NULL;
	
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
	
	if (DISPCAPCNT.SrcA == 0)
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				break;
				
			case NDSColorFormat_BGR666_Rev:
				renderedLineSrcA16 = (u16 *)malloc_alignedCacheLine(compState.linePixelCount * sizeof(u16));
				ConvertColorBuffer6665To5551<false, false>((u32 *)compState.lineColorHead, renderedLineSrcA16, compState.linePixelCount);
				break;
				
			case NDSColorFormat_BGR888_Rev:
				renderedLineSrcA16 = (u16 *)malloc_alignedCacheLine(compState.linePixelCount * sizeof(u16));
				ConvertColorBuffer8888To5551<false, false>((u32 *)compState.lineColorHead, renderedLineSrcA16, compState.linePixelCount);
				break;
		}
	}
	
	static CACHE_ALIGN u16 fifoLine[GPU_FRAMEBUFFER_NATIVE_WIDTH];
	const u16 *srcA = (DISPCAPCNT.SrcA == 0) ? ((renderedLineSrcA16 != NULL) ? renderedLineSrcA16 : (u16 *)compState.lineColorHead) : this->_3DFramebufferRGBA5551 + compState.blockOffsetCustom;
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
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = this->isLineRenderNative[l];
					break;
				}
					
				case 1: // Capture 3D
				{
					//INFO("Capture 3D\n");
					if (is3DFramebufferNativeSize)
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(srcA, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, false, true>(srcA, cap_dst, CAPTURELENGTH, 1);
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
					const bool didVRAMLineChange = this->VerifyVRAMLineDidChange(vramReadBlock, readLineIndexWithOffset);
					if (didVRAMLineChange)
					{
						if (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC)
						{
							u32 cap_src_adr = readLineIndexWithOffset * GPU_FRAMEBUFFER_NATIVE_WIDTH;
							cap_src_adr &= 0x0000FFFF;
							cap_src = this->_VRAMNativeBlockPtr[vramReadBlock] + cap_src_adr;
						}
						else
						{
							cap_src = (u16 *)MMU.blank_memory;
						}
						
						srcB = cap_src;
					}
					
					if (this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset])
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, true>(srcB, cap_dst, CAPTURELENGTH, 1);
					}
					else
					{
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, true>(srcB, cap_dst, CAPTURELENGTH, 1);
					}
					
					newCaptureLineNativeState = this->isLineCaptureNative[vramReadBlock][readLineIndexWithOffset];
					break;
				}
					
				case 1: // Capture dispfifo (not yet tested)
				{
					this->_RenderLine_DispCapture_FIFOToBuffer(fifoLine);
					this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, true>(srcB, cap_dst, CAPTURELENGTH, 1);
					newCaptureLineNativeState = true;
					break;
				}
			}
			break;
		}
			
		default: // Capture source is SourceA+B blended
		{
			//INFO("Capture source is SourceA+B blended\n");
			if (DISPCAPCNT.SrcB == 0)
			{
				const bool didVRAMLineChange = this->VerifyVRAMLineDidChange(vramReadBlock, readLineIndexWithOffset);
				if (didVRAMLineChange)
				{
					if (vramConfiguration.banks[vramReadBlock].purpose == VramConfiguration::LCDC)
					{
						u32 cap_src_adr = readLineIndexWithOffset * GPU_FRAMEBUFFER_NATIVE_WIDTH;
						cap_src_adr &= 0x0000FFFF;
						cap_src = this->_VRAMNativeBlockPtr[vramReadBlock] + cap_src_adr;
					}
					else
					{
						cap_src = (u16 *)MMU.blank_memory;
					}
					
					srcB = cap_src;
				}
			}
			
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
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						break;
					}
						
					case 1: // Capture 3D
					{
						if (is3DFramebufferNativeSize)
						{
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, false, false>(srcA, cap_dst_ext, captureLengthExt, captureLineCount);
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
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, true, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						else
						{
							this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 0, CAPTURELENGTH, false, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
						}
						break;
					}
						
					case 1: // Capture dispfifo (not yet tested)
						this->_RenderLine_DispCapture_Copy<NDSColorFormat_BGR555_Rev, 1, CAPTURELENGTH, true, false>(srcB, cap_dst_ext, captureLengthExt, captureLineCount);
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
	
	free_aligned(renderedLineSrcA16);
}

void GPUEngineA::_RenderLine_DispCapture_FIFOToBuffer(u16 *fifoLineBuffer)
{
#ifdef ENABLE_SSE2
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
	{
		const __m128i fifoColor = _mm_setr_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
		_mm_store_si128((__m128i *)fifoLineBuffer + i, fifoColor);
	}
#else
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
	{
		((u32 *)fifoLineBuffer)[i] = LE_TO_LOCAL_32( DISP_FIFOrecv() );
	}
#endif
}

template<NDSColorFormat COLORFORMAT, int SOURCESWITCH, size_t CAPTURELENGTH, bool CAPTUREFROMNATIVESRC, bool CAPTURETONATIVEDST>
void GPUEngineA::_RenderLine_DispCapture_Copy(const void *src, void *dst, const size_t captureLengthExt, const size_t captureLineCount)
{
	const u16 alphaBit16 = (SOURCESWITCH == 0) ? 0x8000 : 0x0000;
	const u32 alphaBit32 = (SOURCESWITCH == 0) ? ((COLORFORMAT == NDSColorFormat_BGR888_Rev) ? 0xFF000000 : 0x1F000000) : 0x00000000;
	
#ifdef ENABLE_SSE2
	const __m128i alpha_vec128 = (COLORFORMAT == NDSColorFormat_BGR555_Rev) ? _mm_set1_epi16(alphaBit16) : _mm_set1_epi32(alphaBit32);
#endif
	
	if (CAPTURETONATIVEDST)
	{
		if (CAPTUREFROMNATIVESRC)
		{
#ifdef ENABLE_SSE2
			switch (COLORFORMAT)
			{
				case NDSColorFormat_BGR555_Rev:
					MACRODO_N(CAPTURELENGTH / (sizeof(__m128i) / sizeof(u16)), _mm_store_si128((__m128i *)dst + (X), _mm_or_si128( _mm_load_si128( (__m128i *)src + (X)), alpha_vec128 ) ));
					break;
					
				case NDSColorFormat_BGR666_Rev:
				case NDSColorFormat_BGR888_Rev:
					MACRODO_N(CAPTURELENGTH / (sizeof(__m128i) / sizeof(u32)), _mm_store_si128((__m128i *)dst + (X), _mm_or_si128( _mm_load_si128( (__m128i *)src + (X)), alpha_vec128 ) ));
					break;
			}
#else
			for (size_t i = 0; i < CAPTURELENGTH; i++)
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
		const NDSDisplayInfo &dispInfo = GPU->GetDisplayInfo();
		
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
			
			for (size_t line = 1; line < captureLineCount; line++)
			{
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
						memcpy((u16 *)dst + (line * dispInfo.customWidth), dst, captureLengthExt * sizeof(u16));
						break;
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
						memcpy((u32 *)dst + (line * dispInfo.customWidth), dst, captureLengthExt * sizeof(u32));
						break;
				}
			}
		}
		else
		{
			if (CAPTURELENGTH == GPU_FRAMEBUFFER_NATIVE_WIDTH)
			{
				const size_t pixCountExt = captureLengthExt * captureLineCount;
				size_t i = 0;
				
#ifdef ENABLE_SSE2
				switch (COLORFORMAT)
				{
					case NDSColorFormat_BGR555_Rev:
					{
						const size_t ssePixCount = pixCountExt - (pixCountExt % 8);
						for (; i < ssePixCount; i += 8)
						{
							_mm_store_si128((__m128i *)((u16 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u16 *)src + i)), alpha_vec128 ) );
						}
						break;
					}
						
					case NDSColorFormat_BGR666_Rev:
					case NDSColorFormat_BGR888_Rev:
					{
						const size_t ssePixCount = pixCountExt - (pixCountExt % 4);
						for (; i < ssePixCount; i += 4)
						{
							_mm_store_si128((__m128i *)((u32 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u32 *)src + i)), alpha_vec128 ) );
						}
						break;
					}
				}
#endif
				
#ifdef ENABLE_SSE2
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
				for (size_t line = 0; line < captureLineCount; line++)
				{
					size_t i = 0;
					
					switch (COLORFORMAT)
					{
						case NDSColorFormat_BGR555_Rev:
						{
#ifdef ENABLE_SSE2
							const size_t ssePixCount = captureLengthExt - (captureLengthExt % 8);
							for (; i < ssePixCount; i += 8)
							{
								_mm_store_si128((__m128i *)((u16 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u16 *)src + i)), alpha_vec128 ) );
							}
#endif
							
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
							for (; i < captureLengthExt; i++)
							{
								((u16 *)dst)[i] = LE_TO_LOCAL_16(((u16 *)src)[i] | alphaBit16);
							}
							
							src = (u16 *)src + dispInfo.customWidth;
							dst = (u16 *)dst + dispInfo.customWidth;
							break;
						}
							
						case NDSColorFormat_BGR666_Rev:
						case NDSColorFormat_BGR888_Rev:
						{
#ifdef ENABLE_SSE2
							const size_t ssePixCount = captureLengthExt - (captureLengthExt % 4);
							for (; i < ssePixCount; i += 4)
							{
								_mm_store_si128((__m128i *)((u32 *)dst + i), _mm_or_si128( _mm_load_si128( (__m128i *)((u32 *)src + i)), alpha_vec128 ) );
							}
#endif
							
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
							for (; i < captureLengthExt; i++)
							{
								((u32 *)dst)[i] = LE_TO_LOCAL_32(((u32 *)src)[i] | alphaBit32);
							}
							
							src = (u32 *)src + dispInfo.customWidth;
							dst = (u32 *)dst + dispInfo.customWidth;
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
FragmentColor GPUEngineA::_RenderLine_DispCapture_BlendFunc(const FragmentColor srcA, const FragmentColor srcB, const u8 blendEVA, const u8 blendEVB)
{
	FragmentColor outColor;
	outColor.color = 0;
	
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

#ifdef ENABLE_SSE2
template<NDSColorFormat COLORFORMAT>
__m128i GPUEngineA::_RenderLine_DispCapture_BlendFunc_SSE2(const __m128i &srcA, const __m128i &srcB, const __m128i &blendEVA, const __m128i &blendEVB)
{
#ifdef ENABLE_SSSE3
	__m128i blendAB = _mm_or_si128( blendEVA, _mm_slli_epi16(blendEVB, 8) );
#endif
	
	switch (COLORFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
		{
			__m128i srcA_alpha = _mm_and_si128(srcA, _mm_set1_epi16(0x8000));
			__m128i srcB_alpha = _mm_and_si128(srcB, _mm_set1_epi16(0x8000));
			__m128i srcA_masked = _mm_andnot_si128( _mm_cmpeq_epi16(srcA_alpha, _mm_setzero_si128()), srcA );
			__m128i srcB_masked = _mm_andnot_si128( _mm_cmpeq_epi16(srcB_alpha, _mm_setzero_si128()), srcB );
			__m128i colorBitMask = _mm_set1_epi16(0x001F);
			
			__m128i ra;
			__m128i ga;
			__m128i ba;
			
#ifdef ENABLE_SSSE3
			ra = _mm_or_si128( _mm_and_si128(               srcA_masked,      colorBitMask), _mm_and_si128(_mm_slli_epi16(srcB_masked, 8), _mm_set1_epi16(0x1F00)) );
			ga = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(srcA_masked,  5), colorBitMask), _mm_and_si128(_mm_slli_epi16(srcB_masked, 3), _mm_set1_epi16(0x1F00)) );
			ba = _mm_or_si128( _mm_and_si128(_mm_srli_epi16(srcA_masked, 10), colorBitMask), _mm_and_si128(_mm_srli_epi16(srcB_masked, 2), _mm_set1_epi16(0x1F00)) );
			
			ra = _mm_maddubs_epi16(ra, blendAB);
			ga = _mm_maddubs_epi16(ga, blendAB);
			ba = _mm_maddubs_epi16(ba, blendAB);
#else
			ra = _mm_and_si128(               srcA_masked,      colorBitMask);
			ga = _mm_and_si128(_mm_srli_epi16(srcA_masked,  5), colorBitMask);
			ba = _mm_and_si128(_mm_srli_epi16(srcA_masked, 10), colorBitMask);
			
			__m128i rb = _mm_and_si128(               srcB_masked,      colorBitMask);
			__m128i gb = _mm_and_si128(_mm_srli_epi16(srcB_masked,  5), colorBitMask);
			__m128i bb = _mm_and_si128(_mm_srli_epi16(srcB_masked, 10), colorBitMask);
			
			ra = _mm_add_epi16( _mm_mullo_epi16(ra, blendEVA), _mm_mullo_epi16(rb, blendEVB) );
			ga = _mm_add_epi16( _mm_mullo_epi16(ga, blendEVA), _mm_mullo_epi16(gb, blendEVB) );
			ba = _mm_add_epi16( _mm_mullo_epi16(ba, blendEVA), _mm_mullo_epi16(bb, blendEVB) );
#endif
			
			ra = _mm_srli_epi16(ra, 4);
			ga = _mm_srli_epi16(ga, 4);
			ba = _mm_srli_epi16(ba, 4);
			
			ra = _mm_min_epi16(ra, colorBitMask);
			ga = _mm_min_epi16(ga, colorBitMask);
			ba = _mm_min_epi16(ba, colorBitMask);
			
			return _mm_or_si128( _mm_or_si128(_mm_or_si128(ra, _mm_slli_epi16(ga,  5)), _mm_slli_epi16(ba, 10)), _mm_or_si128(srcA_alpha, srcB_alpha) );
		}
			
		case NDSColorFormat_BGR666_Rev:
		case NDSColorFormat_BGR888_Rev:
		{
			// Get color masks based on if the alpha value is 0. Colors with an alpha value
			// equal to 0 are rejected.
			__m128i srcA_alpha = _mm_and_si128(srcA, _mm_set1_epi32(0xFF000000));
			__m128i srcB_alpha = _mm_and_si128(srcB, _mm_set1_epi32(0xFF000000));
			__m128i srcA_masked = _mm_andnot_si128(_mm_cmpeq_epi32(srcA_alpha, _mm_setzero_si128()), srcA);
			__m128i srcB_masked = _mm_andnot_si128(_mm_cmpeq_epi32(srcB_alpha, _mm_setzero_si128()), srcB);
			
			__m128i outColorLo;
			__m128i outColorHi;
			__m128i outColor;
			
			// Temporarily convert the color component values from 8-bit to 16-bit, and then
			// do the blend calculation.
#ifdef ENABLE_SSSE3
			outColorLo = _mm_unpacklo_epi8(srcA_masked, srcB_masked);
			outColorHi = _mm_unpackhi_epi8(srcA_masked, srcB_masked);
			
			outColorLo = _mm_maddubs_epi16(outColorLo, blendAB);
			outColorHi = _mm_maddubs_epi16(outColorHi, blendAB);
#else
			__m128i srcA_maskedLo = _mm_unpacklo_epi8(srcA_masked, _mm_setzero_si128());
			__m128i srcA_maskedHi = _mm_unpackhi_epi8(srcA_masked, _mm_setzero_si128());
			__m128i srcB_maskedLo = _mm_unpacklo_epi8(srcB_masked, _mm_setzero_si128());
			__m128i srcB_maskedHi = _mm_unpackhi_epi8(srcB_masked, _mm_setzero_si128());
			
			outColorLo = _mm_add_epi16( _mm_mullo_epi16(srcA_maskedLo, blendEVA), _mm_mullo_epi16(srcB_maskedLo, blendEVB) );
			outColorHi = _mm_add_epi16( _mm_mullo_epi16(srcA_maskedHi, blendEVA), _mm_mullo_epi16(srcB_maskedHi, blendEVB) );
#endif
			
			outColorLo = _mm_srli_epi16(outColorLo, 4);
			outColorHi = _mm_srli_epi16(outColorHi, 4);
			
			// Convert the color components back from 16-bit to 8-bit using a saturated pack.
			outColor = _mm_packus_epi16(outColorLo, outColorHi);
			
			// When the color format is 8888, the packuswb instruction will naturally clamp
			// the color component values to 255. However, when the color format is 6665, the
			// color component values must be clamped to 63. In this case, we must call pminub
			// to do the clamp.
			if (COLORFORMAT == NDSColorFormat_BGR666_Rev)
			{
				outColor = _mm_min_epu8(outColor, _mm_set1_epi8(63));
			}
			
			// Add the alpha components back in.
			outColor = _mm_and_si128(outColor, _mm_set1_epi32(0x00FFFFFF));
			outColor = _mm_or_si128(outColor, srcA_alpha);
			outColor = _mm_or_si128(outColor, srcB_alpha);
			
			return outColor;
		}
	}
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
		
		_mm_store_si128( (__m128i *)(dst + i), this->_RenderLine_DispCapture_BlendFunc_SSE2<NDSColorFormat_BGR555_Rev>(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
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
			
			_mm_store_si128( (__m128i *)(dst + i), this->_RenderLine_DispCapture_BlendFunc_SSE2<NDSColorFormat_BGR555_Rev>(srcA_vec128, srcB_vec128, blendEVA_vec128, blendEVB_vec128) );
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

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeVRAM(const size_t l)
{
	const IOREG_DISPCNT &DISPCNT = this->_IORegisterMap->DISPCNT;
	this->VerifyVRAMLineDidChange(DISPCNT.VRAM_Block, l);
	
	if (this->isLineCaptureNative[DISPCNT.VRAM_Block][l])
	{
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineColorCopy<true, true, true, true, 2>(this->nativeBuffer, this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block], l);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				FragmentColor *dst = (FragmentColor *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				ConvertColorBuffer555To6665Opaque<false, false>(src, (u32 *)dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				const u16 *src = this->_VRAMNativeBlockPtr[DISPCNT.VRAM_Block] + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				FragmentColor *dst = (FragmentColor *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH);
				ConvertColorBuffer555To8888Opaque<false, false>(src, (u32 *)dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
				break;
			}
		}
	}
	else
	{
		const size_t customWidth = GPU->GetDisplayInfo().customWidth;
		const size_t customPixCount = customWidth * _gpuDstLineCount[l];
		
		switch (OUTPUTFORMAT)
		{
			case NDSColorFormat_BGR555_Rev:
				this->_LineColorCopy<false, false, true, true, 2>((u16 *)this->customBuffer, this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block], l);
				break;
				
			case NDSColorFormat_BGR666_Rev:
			{
				const u16 *src = this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + (_gpuDstLineIndex[l] * customWidth);
				FragmentColor *dst = (FragmentColor *)this->customBuffer + (_gpuDstLineIndex[l] * customWidth);
				ConvertColorBuffer555To6665Opaque<false, false>(src, (u32 *)dst, customPixCount);
				break;
			}
				
			case NDSColorFormat_BGR888_Rev:
			{
				const u16 *src = this->_VRAMCustomBlockPtr[DISPCNT.VRAM_Block] + (_gpuDstLineIndex[l] * customWidth);
				FragmentColor *dst = (FragmentColor *)this->customBuffer + (_gpuDstLineIndex[l] * customWidth);
				ConvertColorBuffer555To8888Opaque<false, false>(src, (u32 *)dst, customPixCount);
				break;
			}
		}
		
		this->isLineOutputNative[l] = false;
		this->nativeLineOutputCount--;
	}
}

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineA::_HandleDisplayModeMainMemory(const size_t l)
{
	// Native rendering only.
	//
	//this has not been tested since the dma timing for dispfifo was changed around the time of
	//newemuloop. it may not work.
	
	u32 *dstColorLine = (u32 *)((u16 *)this->nativeBuffer + (l * GPU_FRAMEBUFFER_NATIVE_WIDTH));
	
	switch (OUTPUTFORMAT)
	{
		case NDSColorFormat_BGR555_Rev:
		{
			u32 *dst = dstColorLine;
			
#ifdef ENABLE_SSE2
			const __m128i alphaBit = _mm_set1_epi16(0x8000);
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(__m128i); i++)
			{
				const __m128i fifoColor = _mm_setr_epi32(DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv(), DISP_FIFOrecv());
				_mm_store_si128((__m128i *)dst + i, _mm_or_si128(fifoColor, alphaBit));
			}
#else
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
			{
				dst[i] = DISP_FIFOrecv() | 0x80008000;
			}
#endif
			break;
		}
			
		case NDSColorFormat_BGR666_Rev:
		{
			FragmentColor *dst = (FragmentColor *)dstColorLine;
			
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				u32 src = DISP_FIFOrecv();
				dst[i+0].color = COLOR555TO6665_OPAQUE((src >>  0) & 0x7FFF);
				dst[i+1].color = COLOR555TO6665_OPAQUE((src >> 16) & 0x7FFF);
			}
			break;
		}
			
		case NDSColorFormat_BGR888_Rev:
		{
			FragmentColor *dst = (FragmentColor *)dstColorLine;
			
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				u32 src = DISP_FIFOrecv();
				dst[i+0].color = COLOR555TO8888_OPAQUE((src >>  0) & 0x7FFF);
				dst[i+1].color = COLOR555TO8888_OPAQUE((src >> 16) & 0x7FFF);
			}
			break;
		}
	}
}

template<bool ISDEBUGRENDER, bool MOSAIC, bool WILLPERFORMWINDOWTEST, bool COLOREFFECTDISABLEDHINT, bool ISCUSTOMRENDERINGNEEDED>
void GPUEngineA::_LineLarge8bpp(GPUEngineCompositorInfo &compState)
{
	u16 XBG = compState.selectedBGLayer->xOffset;
	u16 YBG = compState.lineIndexNative + compState.selectedBGLayer->yOffset;
	u16 lg = compState.selectedBGLayer->size.width;
	u16 ht = compState.selectedBGLayer->size.height;
	u16 wmask = (lg-1);
	u16 hmask = (ht-1);
	YBG &= hmask;
	
	//TODO - handle wrapping / out of bounds correctly from rot_scale_op?
	
	u32 tmp_map = compState.selectedBGLayer->largeBMPAddress + lg * YBG;
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
			this->_RenderPixelSingle<ISDEBUGRENDER, MOSAIC, WILLPERFORMWINDOWTEST, COLOREFFECTDISABLEDHINT>(compState, x, color, (color != 0));
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

template <NDSColorFormat OUTPUTFORMAT>
void GPUEngineB::RenderLine(const u16 l)
{
	switch (this->_displayOutputMode)
	{
		case GPUDisplayMode_Off: // Display Off(Display white)
			this->_HandleDisplayModeOff<OUTPUTFORMAT>(l);
			break;
		
		case GPUDisplayMode_Normal: // Display BG and OBJ layers
		{
			if (this->_isAnyWindowEnabled)
			{
				this->_RenderLine_Layers<OUTPUTFORMAT, true>(l);
			}
			else
			{
				this->_RenderLine_Layers<OUTPUTFORMAT, false>(l);
			}
			
			this->_HandleDisplayModeNormal<OUTPUTFORMAT>(l);
			break;
		}
			
		default:
			break;
	}
}

GPUSubsystem::GPUSubsystem()
{
	static bool needInitTables = true;
	
	if (needInitTables)
	{
#define RGB15TO18_BITLOGIC(col)         ( (material_5bit_to_6bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_6bit[((col)>>5)&0x1F]<<8) |  material_5bit_to_6bit[(col)&0x1F] )
#define RGB15TO18_SWAP_RB_BITLOGIC(col) (  material_5bit_to_6bit[((col)>>10)&0x1F]      | (material_5bit_to_6bit[((col)>>5)&0x1F]<<8) | (material_5bit_to_6bit[(col)&0x1F]<<16) )
#define RGB15TO24_BITLOGIC(col)         ( (material_5bit_to_8bit[((col)>>10)&0x1F]<<16) | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) |  material_5bit_to_8bit[(col)&0x1F] )
#define RGB15TO24_SWAP_RB_BITLOGIC(col) (  material_5bit_to_8bit[((col)>>10)&0x1F]      | (material_5bit_to_8bit[((col)>>5)&0x1F]<<8) | (material_5bit_to_8bit[(col)&0x1F]<<16) )
		
		for (size_t i = 0; i < 32768; i++)
		{
			color_555_to_666[i]					= LE_TO_LOCAL_32( RGB15TO18_BITLOGIC(i) );
			color_555_to_6665_opaque[i]			= LE_TO_LOCAL_32( RGB15TO18_BITLOGIC(i) | 0x1F000000 );
			color_555_to_6665_opaque_swap_rb[i]	= LE_TO_LOCAL_32( RGB15TO18_SWAP_RB_BITLOGIC(i) | 0x1F000000 );
			
			color_555_to_888[i]					= LE_TO_LOCAL_32( RGB15TO24_BITLOGIC(i) );
			color_555_to_8888_opaque[i]			= LE_TO_LOCAL_32( RGB15TO24_BITLOGIC(i) | 0xFF000000 );
			color_555_to_8888_opaque_swap_rb[i]	= LE_TO_LOCAL_32( RGB15TO24_SWAP_RB_BITLOGIC(i) | 0xFF000000 );
		}
		
		needInitTables = false;
	}
	
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
	
	_displayInfo.colorFormat = NDSColorFormat_BGR555_Rev;
	_displayInfo.pixelBytes = sizeof(u16);
	_displayInfo.isCustomSizeRequested = false;
	_displayInfo.customWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	_displayInfo.customHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	
	_customVRAM = NULL;
	_customVRAMBlank = NULL;
	_customFramebuffer = malloc_alignedCacheLine(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2 * _displayInfo.pixelBytes);
	
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
	
	free_aligned(_gpuDstToSrcSSSE3_u8_8e);
	_gpuDstToSrcSSSE3_u8_8e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u8_16e);
	_gpuDstToSrcSSSE3_u8_16e = NULL;
	free_aligned(_gpuDstToSrcSSSE3_u16_8e);
	_gpuDstToSrcSSSE3_u16_8e = NULL;
	
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
	this->_engineMain->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
	this->_engineMain->renderedWidth = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	this->_engineMain->renderedHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;
	this->_engineMain->renderedBuffer = this->_engineMain->nativeBuffer;
	
	this->_engineSub->vramBlockOBJIndex = VRAM_NO_3D_USAGE;
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
			case VramConfiguration::BBG:
			case VramConfiguration::LCDC:
				break;
				
			case VramConfiguration::AOBJ:
				this->_engineMain->UpdateVRAM3DUsageProperties_OBJLayer(i);
				break;
				
			case VramConfiguration::BOBJ:
				this->_engineSub->UpdateVRAM3DUsageProperties_OBJLayer(i);
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
	u8 *oldGpuDstToSrcSSSE3_u8_8e = _gpuDstToSrcSSSE3_u8_8e;
	u8 *oldGpuDstToSrcSSSE3_u8_16e = _gpuDstToSrcSSSE3_u8_16e;
	u8 *oldGpuDstToSrcSSSE3_u16_8e = _gpuDstToSrcSSSE3_u16_8e;
	
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
	
	u8 *newGpuDstToSrcSSSE3_u8_8e = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u8_16e = (u8 *)malloc_alignedCacheLine(w * sizeof(u8));
	u8 *newGpuDstToSrcSSSE3_u16_8e = (u8 *)malloc_alignedCacheLine(w * sizeof(u16));
	
	for (size_t i = 0; i < w; i++)
	{
		const u8 value_u8_8 = newGpuDstToSrcIndex[i] & 0x07;
		const u8 value_u8_16 = newGpuDstToSrcIndex[i] & 0x0F;
		const u8 value_u16 = (value_u8_8 << 1);
		
		newGpuDstToSrcSSSE3_u8_8e[i] = value_u8_8;
		newGpuDstToSrcSSSE3_u8_16e[i] = value_u8_16;
		
		newGpuDstToSrcSSSE3_u16_8e[(i << 1)] = value_u16;
		newGpuDstToSrcSSSE3_u16_8e[(i << 1) + 1] = value_u16 + 1;
	}
	
	_gpuLargestDstLineCount = newGpuLargestDstLineCount;
	_gpuVRAMBlockOffset = _gpuCaptureLineIndex[GPU_VRAM_BLOCK_LINES] * w;
	_gpuDstToSrcIndex = newGpuDstToSrcIndex;
	_gpuDstToSrcSSSE3_u8_8e = newGpuDstToSrcSSSE3_u8_8e;
	_gpuDstToSrcSSSE3_u8_16e = newGpuDstToSrcSSSE3_u8_16e;
	_gpuDstToSrcSSSE3_u16_8e = newGpuDstToSrcSSSE3_u16_8e;
	
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
	free_aligned(oldGpuDstToSrcSSSE3_u8_8e);
	free_aligned(oldGpuDstToSrcSSSE3_u8_16e);
	free_aligned(oldGpuDstToSrcSSSE3_u16_8e);
}

void GPUSubsystem::SetCustomFramebufferSize(size_t w, size_t h)
{
	this->SetCustomFramebufferSize(w, h, NULL, NULL);
}

void GPUSubsystem::SetColorFormat(const NDSColorFormat outputFormat, void *clientNativeBuffer, void *clientCustomBuffer)
{
	CurrentRenderer->RenderFinish();
	
	this->_displayInfo.colorFormat = outputFormat;
	this->_displayInfo.pixelBytes = (outputFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(FragmentColor);
	
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
		this->_customFramebuffer = NULL;
		this->_displayInfo.masterCustomBuffer = clientCustomBuffer;
	}
	else
	{
		void *newCustomFramebuffer = malloc_alignedCacheLine(w * h * 2 * pixelBytes);
		this->_customFramebuffer = newCustomFramebuffer;
		this->_displayInfo.masterCustomBuffer = newCustomFramebuffer;
	}
	
	switch (outputFormat)
	{
		case NDSColorFormat_BGR555_Rev:
			memset_u16(this->_displayInfo.masterCustomBuffer, 0x8000, w * h * 2);
			break;
			
		case NDSColorFormat_BGR666_Rev:
			memset_u32(this->_displayInfo.masterCustomBuffer, 0x1F000000, w * h * 2);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			memset_u32(this->_displayInfo.masterCustomBuffer, 0xFF000000, w * h * 2);
			break;
			
		default:
			break;
	}
	
	this->_customVRAM = newCustomVRAM;
	this->_customVRAMBlank = newCustomVRAM + (newCustomVRAMBlockSize * 4);
	
	this->_displayInfo.nativeBuffer[NDSDisplayID_Main]  = (this->_displayMain->GetEngine()->GetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.masterNativeBuffer : (u8 *)this->_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
	this->_displayInfo.nativeBuffer[NDSDisplayID_Touch] = (this->_displayTouch->GetEngine()->GetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.masterNativeBuffer : (u8 *)this->_displayInfo.masterNativeBuffer + (GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * this->_displayInfo.pixelBytes);
	this->_displayInfo.customBuffer[NDSDisplayID_Main]  = (this->_displayMain->GetEngine()->GetDisplayByID()  == NDSDisplayID_Main) ? this->_displayInfo.masterCustomBuffer : (u8 *)this->_displayInfo.masterCustomBuffer + (w * h * this->_displayInfo.pixelBytes);
	this->_displayInfo.customBuffer[NDSDisplayID_Touch] = (this->_displayTouch->GetEngine()->GetDisplayByID() == NDSDisplayID_Main) ? this->_displayInfo.masterCustomBuffer : (u8 *)this->_displayInfo.masterCustomBuffer + (w * h * this->_displayInfo.pixelBytes);
	
	this->_displayInfo.renderedBuffer[NDSDisplayID_Main]  = (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Main])  ? this->_displayInfo.customBuffer[NDSDisplayID_Main]  : this->_displayInfo.nativeBuffer[NDSDisplayID_Main];
	this->_displayInfo.renderedBuffer[NDSDisplayID_Touch] = (this->_displayInfo.didPerformCustomRender[NDSDisplayID_Touch]) ? this->_displayInfo.customBuffer[NDSDisplayID_Touch] : this->_displayInfo.nativeBuffer[NDSDisplayID_Touch];
	
	this->_engineMain->SetCustomFramebufferSize(w, h);
	this->_engineSub->SetCustomFramebufferSize(w, h);
	BaseRenderer->SetFramebufferSize(w, h); // Since BaseRenderer is persistent, we need to update this manually.
	
	if (CurrentRenderer != BaseRenderer)
	{
		CurrentRenderer->RequestColorFormat(outputFormat);
		CurrentRenderer->SetFramebufferSize(w, h);
	}
	
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

template <NDSColorFormat OUTPUTFORMAT>
void GPUSubsystem::RenderLine(const u16 l, bool isFrameSkipRequested)
{
	const bool isDisplayCaptureNeeded = this->_engineMain->WillDisplayCapture(l);
	const bool isFramebufferRenderNeeded[2]	= {(CommonSettings.showGpu.main && !this->_engineMain->GetIsMasterBrightFullIntensity()) || isDisplayCaptureNeeded,
											    CommonSettings.showGpu.sub && !this->_engineSub->GetIsMasterBrightFullIntensity() };
	
	if (l == 0)
	{
		this->_event->DidFrameBegin(isFrameSkipRequested);
		
		// Clear displays to black if they are turned off by the user.
		if (!isFrameSkipRequested)
		{
			this->UpdateRenderProperties();
			
			if (!isFramebufferRenderNeeded[GPUEngineID_Main])
			{
				if (!CommonSettings.showGpu.main)
				{
					memset(this->_engineMain->renderedBuffer, 0, this->_engineMain->renderedWidth * this->_engineMain->renderedHeight * this->_displayInfo.pixelBytes);
				}
				else if (this->_engineMain->GetIsMasterBrightFullIntensity())
				{
					this->_engineMain->ApplyMasterBrightness<OUTPUTFORMAT, true>();
				}
			}
			
			if (!isFramebufferRenderNeeded[GPUEngineID_Sub])
			{
				if (!CommonSettings.showGpu.sub)
				{
					memset(this->_engineSub->renderedBuffer, 0, this->_engineSub->renderedWidth * this->_engineSub->renderedHeight * this->_displayInfo.pixelBytes);
				}
				else if (this->_engineSub->GetIsMasterBrightFullIntensity())
				{
					this->_engineSub->ApplyMasterBrightness<OUTPUTFORMAT, true>();
				}
			}
		}
	}
	
	if (isFramebufferRenderNeeded[GPUEngineID_Main] && !isFrameSkipRequested)
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
		
		if (CurrentRenderer->GetRenderNeedsFinish())
		{
			const bool need3DDisplayFramebuffer = this->_engineMain->WillRender3DLayer();
			const bool need3DCaptureFramebuffer = this->_engineMain->WillCapture3DLayerDirect(l);
			
			if (need3DDisplayFramebuffer || need3DCaptureFramebuffer)
			{
				CurrentRenderer->SetFramebufferFlushStates(need3DDisplayFramebuffer, need3DCaptureFramebuffer);
				CurrentRenderer->RenderFinish();
				CurrentRenderer->SetRenderNeedsFinish(false);
				this->_event->DidRender3DEnd();
			}
		}
		
		this->_engineMain->RenderLine<OUTPUTFORMAT>(l);
	}
	else
	{
		this->_engineMain->UpdatePropertiesWithoutRender(l);
	}
	
	if (isFramebufferRenderNeeded[GPUEngineID_Sub] && !isFrameSkipRequested)
	{
		this->_engineSub->RenderLine<OUTPUTFORMAT>(l);
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
				this->_engineMain->ResolveCustomRendering<OUTPUTFORMAT>();
				this->_engineSub->ResolveCustomRendering<OUTPUTFORMAT>();
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
				this->_engineMain->ApplyMasterBrightness<OUTPUTFORMAT, false>();
			}
			
			if (isFramebufferRenderNeeded[GPUEngineID_Sub])
			{
				this->_engineSub->ApplyMasterBrightness<OUTPUTFORMAT, false>();
			}
			
			if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
			{
				this->_engineMain->ResolveRGB666ToRGB888();
				this->_engineSub->ResolveRGB666ToRGB888();
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
			color32.color = COLOR555TO6665_OPAQUE(colorBGRA5551 & 0x7FFF);
			break;
			
		case NDSColorFormat_BGR888_Rev:
			color32.color = COLOR555TO8888_OPAQUE(colorBGRA5551 & 0x7FFF);
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

template <bool SWAP_RB, bool IS_UNALIGNED>
void ConvertColorBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount)
{
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = pixCount - (pixCount % 8);
	for (; i < ssePixCount; i += 8)
	{
		__m128i src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((__m128i *)(src + i)) : _mm_load_si128((__m128i *)(src + i));
		__m128i dstConvertedLo, dstConvertedHi;
		ConvertColor555To8888Opaque<SWAP_RB>(src_vec128, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((__m128i *)(dst + i + 0), dstConvertedLo);
			_mm_storeu_si128((__m128i *)(dst + i + 4), dstConvertedHi);
		}
		else
		{
			_mm_store_si128((__m128i *)(dst + i + 0), dstConvertedLo);
			_mm_store_si128((__m128i *)(dst + i + 4), dstConvertedHi);
		}
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < pixCount; i++)
	{
		dst[i] = ConvertColor555To8888Opaque<SWAP_RB>(src[i]);
	}
}

template <bool SWAP_RB, bool IS_UNALIGNED>
void ConvertColorBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount)
{
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = pixCount - (pixCount % 8);
	for (; i < ssePixCount; i += 8)
	{
		__m128i src_vec128 = (IS_UNALIGNED) ? _mm_loadu_si128((__m128i *)(src + i)) : _mm_load_si128((__m128i *)(src + i));
		__m128i dstConvertedLo, dstConvertedHi;
		ConvertColor555To6665Opaque<SWAP_RB>(src_vec128, dstConvertedLo, dstConvertedHi);
		
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128((__m128i *)(dst + i + 0), dstConvertedLo);
			_mm_storeu_si128((__m128i *)(dst + i + 4), dstConvertedHi);
		}
		else
		{
			_mm_store_si128((__m128i *)(dst + i + 0), dstConvertedLo);
			_mm_store_si128((__m128i *)(dst + i + 4), dstConvertedHi);
		}
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < pixCount; i++)
	{
		dst[i] = ConvertColor555To6665Opaque<SWAP_RB>(src[i]);
	}
}

template <bool SWAP_RB>
void ConvertColorBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount)
{
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = pixCount - (pixCount % 4);
	for (; i < ssePixCount; i += 4)
	{
		_mm_store_si128( (__m128i *)(dst + i), ConvertColor8888To6665<SWAP_RB>(_mm_load_si128((__m128i *)(src + i))) );
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < pixCount; i++)
	{
		dst[i] = ConvertColor8888To6665<SWAP_RB>(src[i]);
	}
}

template <bool SWAP_RB>
void ConvertColorBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount)
{
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = pixCount - (pixCount % 4);
	for (; i < ssePixCount; i += 4)
	{
		_mm_store_si128( (__m128i *)(dst + i), ConvertColor6665To8888<SWAP_RB>(_mm_load_si128((__m128i *)(src + i))) );
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < pixCount; i++)
	{
		dst[i] = ConvertColor6665To8888<SWAP_RB>(src[i]);
	}
}

template <bool SWAP_RB, bool IS_UNALIGNED>
void ConvertColorBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount)
{
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = pixCount - (pixCount % 8);
	for (; i < ssePixCount; i += 8)
	{
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (__m128i *)(dst + i), ConvertColor8888To5551<SWAP_RB>(_mm_loadu_si128((__m128i *)(src + i)), _mm_loadu_si128((__m128i *)(src + i + 4))) );
		}
		else
		{
			_mm_store_si128( (__m128i *)(dst + i), ConvertColor8888To5551<SWAP_RB>(_mm_load_si128((__m128i *)(src + i)), _mm_load_si128((__m128i *)(src + i + 4))) );
		}
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < pixCount; i++)
	{
		dst[i] = ConvertColor8888To5551<SWAP_RB>(src[i]);
	}
}

template <bool SWAP_RB, bool IS_UNALIGNED>
void ConvertColorBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount)
{
	size_t i = 0;
	
#ifdef ENABLE_SSE2
	const size_t ssePixCount = pixCount - (pixCount % 8);
	for (; i < ssePixCount; i += 8)
	{
		if (IS_UNALIGNED)
		{
			_mm_storeu_si128( (__m128i *)(dst + i), ConvertColor6665To5551<SWAP_RB>(_mm_loadu_si128((__m128i *)(src + i)), _mm_loadu_si128((__m128i *)(src + i + 4))) );
		}
		else
		{
			_mm_store_si128( (__m128i *)(dst + i), ConvertColor6665To5551<SWAP_RB>(_mm_load_si128((__m128i *)(src + i)), _mm_load_si128((__m128i *)(src + i + 4))) );
		}
	}
#endif
	
#ifdef ENABLE_SSE2
#pragma LOOPVECTORIZE_DISABLE
#endif
	for (; i < pixCount; i++)
	{
		dst[i] = ConvertColor6665To5551<SWAP_RB>(src[i]);
	}
}

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

template void GPUSubsystem::RenderLine<NDSColorFormat_BGR555_Rev>(const u16 l, bool skip);
template void GPUSubsystem::RenderLine<NDSColorFormat_BGR666_Rev>(const u16 l, bool skip);
template void GPUSubsystem::RenderLine<NDSColorFormat_BGR888_Rev>(const u16 l, bool skip);

template void ConvertColorBuffer555To8888Opaque<true, true>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer555To8888Opaque<true, false>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer555To8888Opaque<false, true>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer555To8888Opaque<false, false>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);

template void ConvertColorBuffer555To6665Opaque<true, true>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer555To6665Opaque<true, false>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer555To6665Opaque<false, true>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer555To6665Opaque<false, false>(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount);

template void ConvertColorBuffer8888To6665<true>(const u32 *src, u32 *dst, size_t pixCount);
template void ConvertColorBuffer8888To6665<false>(const u32 *src, u32 *dst, size_t pixCount);

template void ConvertColorBuffer6665To8888<true>(const u32 *src, u32 *dst, size_t pixCount);
template void ConvertColorBuffer6665To8888<false>(const u32 *src, u32 *dst, size_t pixCount);

template void ConvertColorBuffer8888To5551<true, true>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer8888To5551<true, false>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer8888To5551<false, true>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer8888To5551<false, false>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);

template void ConvertColorBuffer6665To5551<true, true>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer6665To5551<true, false>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer6665To5551<false, true>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
template void ConvertColorBuffer6665To5551<false, false>(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount);
