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

#ifndef COLORSPACEHANDLER_ALTIVEC_H
#define COLORSPACEHANDLER_ALTIVEC_H

#include "colorspacehandler.h"

#ifndef ENABLE_ALTIVEC
	#warning This header requires PowerPC AltiVec support.
#else

template<bool SWAP_RB> void ColorspaceConvert555To8888_AltiVec(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555XTo888X_AltiVec(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To6665_AltiVec(const v128u16 &srcColor, const v128u16 &srcAlphaBits, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555XTo666X_AltiVec(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To8888Opaque_AltiVec(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> void ColorspaceConvert555To6665Opaque_AltiVec(const v128u16 &srcColor, v128u32 &dstLo, v128u32 &dstHi);
template<bool SWAP_RB> v128u32 ColorspaceConvert8888To6665_AltiVec(const v128u32 &src);
template<bool SWAP_RB> v128u32 ColorspaceConvert6665To8888_AltiVec(const v128u32 &src);
template<bool SWAP_RB> v128u16 ColorspaceConvert8888To5551_AltiVec(const v128u32 &srcLo, const v128u32 &srcHi);
template<bool SWAP_RB> v128u16 ColorspaceConvert6665To5551_AltiVec(const v128u32 &srcLo, const v128u32 &srcHi);
template<bool SWAP_RB> v128u32 ColorspaceConvert888XTo8888Opaque_AltiVec(const v128u32 &src);

template<bool SWAP_RB> v128u16 ColorspaceCopy16_AltiVec(const v128u16 &src);
template<bool SWAP_RB> v128u32 ColorspaceCopy32_AltiVec(const v128u32 &src);

// AltiVec has very poor support for dealing with unaligned addresses (it's possible, just
// very obtuse), so we're not even going to bother dealing with any unaligned addresses.
class ColorspaceHandler_AltiVec : public ColorspaceHandler
{
public:
	ColorspaceHandler_AltiVec() {};
	
	size_t ConvertBuffer555To8888Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To8888Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer555To6665Opaque(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555To6665Opaque_SwapRB(const u16 *__restrict src, u32 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer8888To6665(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer8888To6665_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer6665To8888(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer6665To8888_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer8888To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer8888To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer6665To5551(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer6665To5551_SwapRB(const u32 *__restrict src, u16 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer888XTo8888Opaque(const u32 *src, u32 *dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo8888Opaque_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
	
	size_t ConvertBuffer555XTo888(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer555XTo888_SwapRB(const u16 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	
	size_t ConvertBuffer888XTo888(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	size_t ConvertBuffer888XTo888_SwapRB(const u32 *__restrict src, u8 *__restrict dst, size_t pixCount) const;
	
	size_t CopyBuffer16_SwapRB(const u16 *src, u16 *dst, size_t pixCount) const;
	
	size_t CopyBuffer32_SwapRB(const u32 *src, u32 *dst, size_t pixCount) const;
};

#endif // ENABLE_ALTIVEC

#endif /* COLORSPACEHANDLER_ALTIVEC_H */
