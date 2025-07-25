/*
	Copyright (C) 2021-2025 DeSmuME team

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

#ifndef GPU_OPERATIONS_H
#define GPU_OPERATIONS_H

#include <stdio.h>

#include "types.h"
#include "./utils/colorspacehandler/colorspacehandler.h"

#include "GPU.h"


template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineExpandHinted(const void *__restrict srcBuffer, const size_t srcLineIndex,
						  void *__restrict dstBuffer, const size_t dstLineIndex, const size_t dstLineWidth, const size_t dstLineCount);

template <s32 INTEGERSCALEHINT, bool SCALEVERTICAL, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineExpandHinted(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer);

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineReduceHinted(const void *__restrict srcBuffer, const size_t srcLineIndex, const size_t srcLineWidth,
						  void *__restrict dstBuffer, const size_t dstLineIndex);

template <s32 INTEGERSCALEHINT, bool USELINEINDEX, bool NEEDENDIANSWAP, size_t ELEMENTSIZE>
void CopyLineReduceHinted(const GPUEngineLineInfo &lineInfo, const void *__restrict srcBuffer, void *__restrict dstBuffer);

class ColorOperation
{
public:
	ColorOperation() {};
	
	FORCEINLINE Color5551 blend(const Color5551 colA, const Color5551 colB, const u16 blendEVA, const u16 blendEVB) const;
	FORCEINLINE Color5551 blend(const Color5551 colA, const Color5551 colB, const TBlendTable *blendTable) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE Color4u8 blend(const Color4u8 colA, const Color4u8 colB, const u16 blendEVA, const u16 blendEVB) const;
	
	FORCEINLINE Color5551 blend3D(const Color4u8 colA, const Color5551 colB) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE Color4u8 blend3D(const Color4u8 colA, const Color4u8 colB) const;
	
	FORCEINLINE Color5551 increase(const Color5551 col, const u16 blendEVY) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE Color4u8 increase(const Color4u8 col, const u16 blendEVY) const;
	
	FORCEINLINE Color5551 decrease(const Color5551 col, const u16 blendEVY) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE Color4u8 decrease(const Color4u8 col, const u16 blendEVY) const;
};

class PixelOperation
{
private:
	template<GPULayerType LAYERTYPE> FORCEINLINE void __selectedEffect(const GPUEngineCompositorInfo &compInfo, const u8 &dstLayerID, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode, ColorEffect &selectedEffect, TBlendTable **selectedBlendTable, u8 &blendEVA, u8 &blendEVB) const;
	
protected:
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _copy16(GPUEngineCompositorInfo &compInfo, const Color5551 srcColor16) const;
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _copy32(GPUEngineCompositorInfo &compInfo, const Color4u8 srcColor32) const;
	
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessUp16(GPUEngineCompositorInfo &compInfo, const Color5551 srcColor16) const;
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessUp32(GPUEngineCompositorInfo &compInfo, const Color4u8 srcColor32) const;
	
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessDown16(GPUEngineCompositorInfo &compInfo, const Color5551 srcColor16) const;
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessDown32(GPUEngineCompositorInfo &compInfo, const Color4u8 srcColor32) const;
	
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void _unknownEffect16(GPUEngineCompositorInfo &compInfo, const Color5551 srcColor16, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode) const;
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void _unknownEffect32(GPUEngineCompositorInfo &compInfo, const Color4u8 srcColor32, const bool enableColorEffect, const u8 spriteAlpha, const OBJMode spriteMode) const;
	
public:
	static CACHE_ALIGN u8 BlendTable555[17][17][32][32];
	static CACHE_ALIGN Color5551 BrightnessUpTable555[17][0x8000];
	static CACHE_ALIGN Color4u8 BrightnessUpTable666[17][0x8000];
	static CACHE_ALIGN Color4u8 BrightnessUpTable888[17][0x8000];
	static CACHE_ALIGN Color5551 BrightnessDownTable555[17][0x8000];
	static CACHE_ALIGN Color4u8 BrightnessDownTable666[17][0x8000];
	static CACHE_ALIGN Color4u8 BrightnessDownTable888[17][0x8000];
	static void InitLUTs();
	
	PixelOperation() {};
	
	template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void Composite16(GPUEngineCompositorInfo &compInfo, const u16 srcColor16, const bool enableColorEffect, const u8 spriteAlpha, const u8 spriteMode) const;
	template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE> FORCEINLINE void Composite32(GPUEngineCompositorInfo &compInfo, Color4u8 srcColor32, const bool enableColorEffect, const u8 spriteAlpha, const u8 spriteMode) const;
};

#endif // GPU_OPERATIONS_H
