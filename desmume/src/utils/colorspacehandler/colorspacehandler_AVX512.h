/*
	Copyright (C) 2016-2019 DeSmuME team
 
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

#ifndef COLORSPACEHANDLER_AVX512_H
#define COLORSPACEHANDLER_AVX512_H

#include "colorspacehandler.h"

#ifndef ENABLE_AVX512_1
	#warning This header requires AVX-512 Tier-1 support.
#else

template<bool SWAP_RB> void ColorspaceConvert555To8888_AVX512(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555XTo888X_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To6665_AVX512(const v512u16 &srcColor, const v512u16 &srcAlphaBits, v512u32 &dstLo, v512u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555XTo666X_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To8888Opaque_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To6665Opaque_AVX512(const v512u16 &srcColor, v512u32 &dstLo, v512u32 &dstHi);
template<bool SWAP_RB> v512u32 ColorspaceConvert8888To6665_AVX512(const v512u32 &src);
template<bool SWAP_RB> v512u32 ColorspaceConvert6665To8888_AVX512(const v512u32 &src);
template<bool SWAP_RB> v512u16 ColorspaceConvert8888To5551_AVX512(const v512u32 &srcLo, const v512u32 &srcHi);
template<bool SWAP_RB> v512u16 ColorspaceConvert6665To5551_AVX512(const v512u32 &srcLo, const v512u32 &srcHi);
template<bool SWAP_RB> v512u32 ColorspaceConvert888XTo8888Opaque_AVX512(const v512u32 &src);

template<bool SWAP_RB> v512u16 ColorspaceCopy16_AVX512(const v512u16 &src);
template<bool SWAP_RB> v512u32 ColorspaceCopy32_AVX512(const v512u32 &src);

template<bool SWAP_RB> v512u16 ColorspaceApplyIntensity16_AVX512(const v512u16 &src, float intensity);
template<bool SWAP_RB> v512u32 ColorspaceApplyIntensity32_AVX512(const v512u32 &src, float intensity);

class ColorspaceHandler_AVX512 : public ColorspaceHandler
{
public:
	ColorspaceHandler_AVX512() {};
	
	size_t ConvertBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To8888Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To8888Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To6665Opaque_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To6665Opaque_SwapRB_IsUnaligned(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
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
	
	size_t ConvertBuffer888XTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo8888Opaque_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo8888Opaque_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer555XTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555XTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555XTo888_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555XTo888_SwapRB_IsUnaligned(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer888XTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo888_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo888_SwapRB_IsUnaligned(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	
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

#endif // ENABLE_AVX512_1

#endif // COLORSPACEHANDLER_AVX512_H
