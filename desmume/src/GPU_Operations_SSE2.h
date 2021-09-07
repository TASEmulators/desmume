/*
	Copyright (C) 2021 DeSmuME team

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

#ifndef GPU_OPERATIONS_SSE2_H
#define GPU_OPERATIONS_SSE2_H

#include "GPU_Operations.h"

#ifndef ENABLE_SSE2
	#warning This header requires SSE2 support.
#else

class ColorOperation_SSE2
{
public:
	ColorOperation_SSE2() {};
	
	FORCEINLINE v128u16 blend(const v128u16 &colA, const v128u16 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const;
	template<NDSColorFormat COLORFORMAT, bool USECONSTANTBLENDVALUESHINT> FORCEINLINE v128u32 blend(const v128u32 &colA, const v128u32 &colB, const v128u16 &blendEVA, const v128u16 &blendEVB) const;
	
	FORCEINLINE v128u16 blend3D(const v128u32 &colA_Lo, const v128u32 &colA_Hi, const v128u16 &colB) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE v128u32 blend3D(const v128u32 &colA, const v128u32 &colB) const;
	
	FORCEINLINE v128u16 increase(const v128u16 &col, const v128u16 &blendEVY) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE v128u32 increase(const v128u32 &col, const v128u16 &blendEVY) const;
	
	FORCEINLINE v128u16 decrease(const v128u16 &col, const v128u16 &blendEVY) const;
	template<NDSColorFormat COLORFORMAT> FORCEINLINE v128u32 decrease(const v128u32 &col, const v128u16 &blendEVY) const;
};

class PixelOperation_SSE2
{
protected:
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _copy16(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const;
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _copy32(GPUEngineCompositorInfo &compInfo, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const;
	
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _copyMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const;
	template<NDSColorFormat OUTPUTFORMAT, bool ISDEBUGRENDER> FORCEINLINE void _copyMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const;
	
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessUp16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const;
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessUp32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const;
	
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessUpMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const;
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessUpMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const;
	
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessDown16(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const;
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessDown32(GPUEngineCompositorInfo &compInfo, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const;
	
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessDownMask16(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u16 &src1, const v128u16 &src0) const;
	template<NDSColorFormat OUTPUTFORMAT> FORCEINLINE void _brightnessDownMask32(GPUEngineCompositorInfo &compInfo, const v128u8 &passMask8, const v128u16 &evy16, const v128u8 &srcLayerID, const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0) const;
	
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
	FORCEINLINE void _unknownEffectMask16(GPUEngineCompositorInfo &compInfo,
										  const v128u8 &passMask8,
										  const v128u16 &evy16,
										  const v128u8 &srcLayerID,
										  const v128u16 &src1, const v128u16 &src0,
										  const v128u8 &srcEffectEnableMask,
										  const v128u8 &dstBlendEnableMaskLUT,
										  const v128u8 &enableColorEffectMask,
										  const v128u8 &spriteAlpha,
										  const v128u8 &spriteMode) const;
	
	template<NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE>
	FORCEINLINE void _unknownEffectMask32(GPUEngineCompositorInfo &compInfo,
										  const v128u8 &passMask8,
										  const v128u16 &evy16,
										  const v128u8 &srcLayerID,
										  const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0,
										  const v128u8 &srcEffectEnableMask,
										  const v128u8 &dstBlendEnableMaskLUT,
										  const v128u8 &enableColorEffectMask,
										  const v128u8 &spriteAlpha,
										  const v128u8 &spriteMode) const;
	
public:
	PixelOperation_SSE2() {};
	
	template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
	FORCEINLINE void Composite16(GPUEngineCompositorInfo &compInfo,
								 const bool didAllPixelsPass,
								 const v128u8 &passMask8,
								 const v128u16 &evy16,
								 const v128u8 &srcLayerID,
								 const v128u16 &src1, const v128u16 &src0,
								 const v128u8 &srcEffectEnableMask,
								 const v128u8 &dstBlendEnableMaskLUT,
								 const u8 *__restrict enableColorEffectPtr,
								 const u8 *__restrict sprAlphaPtr,
								 const u8 *__restrict sprModePtr) const;
	
	template <GPUCompositorMode COMPOSITORMODE, NDSColorFormat OUTPUTFORMAT, GPULayerType LAYERTYPE, bool WILLPERFORMWINDOWTEST>
	FORCEINLINE void Composite32(GPUEngineCompositorInfo &compInfo,
								 const bool didAllPixelsPass,
								 const v128u8 &passMask8,
								 const v128u16 &evy16,
								 const v128u8 &srcLayerID,
								 const v128u32 &src3, const v128u32 &src2, const v128u32 &src1, const v128u32 &src0,
								 const v128u8 &srcEffectEnableMask,
								 const v128u8 &dstBlendEnableMaskLUT,
								 const u8 *__restrict enableColorEffectPtr,
								 const u8 *__restrict sprAlphaPtr,
								 const u8 *__restrict sprModePtr) const;
};

#endif // ENABLE_SSE2

#endif // GPU_OPERATIONS_SSE2_H
