/*
	Copyright (C) 2016-2024 DeSmuME team
 
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

#ifndef COLORSPACEHANDLER_NEON_H
#define COLORSPACEHANDLER_NEON_H

#include "colorspacehandler.h"

#ifndef ENABLE_NEON_A64
	#warning This header requires ARM64 NEON support.
#else

template<bool SWAP_RB> void ColorspaceConvert555aTo8888_NEON(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555xTo888x_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555aTo6665_NEON(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555xTo666x_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555xTo8888Opaque_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555xTo6665Opaque_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert5551To8888_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert5551To6665_NEON(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> v128u32 ColorspaceConvert8888To6665_NEON(const v128u32 &src);
template<bool SWAP_RB> v128u32 ColorspaceConvert6665To8888_NEON(const v128u32 &src);
template<bool SWAP_RB> v128u16 ColorspaceConvert8888To5551_NEON(const v128u32 &srcLo, const v128u32 &srcHi);
template<bool SWAP_RB> v128u16 ColorspaceConvert6665To5551_NEON(const v128u32 &srcLo, const v128u32 &srcHi);
template<bool SWAP_RB> v128u32 ColorspaceConvert888xTo8888Opaque_NEON(const v128u32 &src);

template<bool SWAP_RB> v128u16 ColorspaceCopy16_NEON(const v128u16 &src);
template<bool SWAP_RB> v128u32 ColorspaceCopy32_NEON(const v128u32 &src);

template<bool SWAP_RB> v128u16 ColorspaceApplyIntensity16_NEON(const v128u16 &src, float intensity);
template<bool SWAP_RB> v128u32 ColorspaceApplyIntensity32_NEON(const v128u32 &src, float intensity);

class ColorspaceHandler_NEON : public ColorspaceHandler
{
public:
	ColorspaceHandler_NEON() {};
	
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo8888Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo8888Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo6665Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer555xTo6665Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To8888(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To8888_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To8888_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To8888_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To6665(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To6665_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To6665_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	template<BESwapFlags BE_BYTESWAP> size_t ConvertBuffer5551To6665_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer8888To6665_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer8888To6665_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer6665To8888_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer6665To8888_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer8888To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer8888To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer6665To5551_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer6665To5551_SwapRB_IsUnaligned(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer888xTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888xTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888xTo8888Opaque_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888xTo8888Opaque_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer555xTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555xTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555xTo888_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555xTo888_SwapRB_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer888xTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888xTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888xTo888_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888xTo888_SwapRB_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	
	size_t CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const;
	size_t CopyBuffer16_SwapRB_IsUnaligned(const u16 *src, u16 *dst, size_t pixCount) const;
	
	size_t CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t CopyBuffer32_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ApplyIntensityToBuffer16(u16 *dst, size_t pixCount, float intensity) const;
	size_t ApplyIntensityToBuffer16_SwapRB(u16 *dst, size_t pixCount, float intensity) const;
	size_t ApplyIntensityToBuffer16_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const;
	size_t ApplyIntensityToBuffer16_SwapRB_IsUnaligned(u16 *dst, size_t pixCount, float intensity) const;
	
	size_t ApplyIntensityToBuffer32(u32 *dst, size_t pixCount, float intensity) const;
	size_t ApplyIntensityToBuffer32_SwapRB(u32 *dst, size_t pixCount, float intensity) const;
	size_t ApplyIntensityToBuffer32_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const;
	size_t ApplyIntensityToBuffer32_SwapRB_IsUnaligned(u32 *dst, size_t pixCount, float intensity) const;
};

#endif // ENABLE_NEON_A64

#endif // COLORSPACEHANDLER_NEON_H
