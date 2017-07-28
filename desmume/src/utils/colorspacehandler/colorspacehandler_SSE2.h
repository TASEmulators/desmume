/*
	Copyright (C) 2016-2017 DeSmuME team
 
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

#ifndef COLORSPACEHANDLER_SSE2_H
#define COLORSPACEHANDLER_SSE2_H

#include "colorspacehandler.h"

#ifndef ENABLE_SSE2
	#warning This header requires SSE2 support.
#else

template<bool SWAP_RB> void ColorspaceConvert555To8888_SSE2(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To6665_SSE2(const v128u16 &srcColor, const v128u32 &srcAlphaBits32Lo, const v128u32 &srcAlphaBits32Hi, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To8888Opaque_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To6665Opaque_SSE2(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> v128u32 ColorspaceConvert8888To6665_SSE2(const v128u32 &src);
template<bool SWAP_RB> v128u32 ColorspaceConvert6665To8888_SSE2(const v128u32 &src);
template<bool SWAP_RB> v128u16 ColorspaceConvert8888To5551_SSE2(const v128u32 &srcLo, const v128u32 &srcHi);
template<bool SWAP_RB> v128u16 ColorspaceConvert6665To5551_SSE2(const v128u32 &srcLo, const v128u32 &srcHi);
template<bool SWAP_RB> v128u32 ColorspaceConvert888XTo8888Opaque_SSE2(const v128u32 &src);

template<bool SWAP_RB> v128u16 ColorspaceCopy16_SSE2(const v128u16 &src);
template<bool SWAP_RB> v128u32 ColorspaceCopy32_SSE2(const v128u32 &src);

class ColorspaceHandler_SSE2 : public ColorspaceHandler
{
public:
	ColorspaceHandler_SSE2() {};
	
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
	
	size_t CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const;
	size_t CopyBuffer16_SwapRB_IsUnaligned(const u16 *src, u16 *dst, size_t pixCount) const;
	
	size_t CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t CopyBuffer32_SwapRB_IsUnaligned(const u32 *src, u32 *dst, size_t pixCount) const;
};

#endif // ENABLE_SSE2

#endif /* COLORSPACEHANDLER_SSE2_H */
