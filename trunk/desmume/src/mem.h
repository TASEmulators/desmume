/*
	Copyright (C) 2005 Theo Berkau
	Copyright (C) 2005-2006 Guillaume Duhamel
	Copyright (C) 2008-2010 DeSmuME team

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

#ifndef MEM_H
#define MEM_H

#include <stdlib.h>
#include <assert.h>
#include "types.h"

//this was originally declared in MMU.h but we suffered some organizational problems and had to remove it
enum MMU_ACCESS_TYPE
{
	MMU_AT_CODE, //used for cpu prefetches
	MMU_AT_DATA, //used for cpu read/write
	MMU_AT_GPU, //used for gpu read/write
	MMU_AT_DMA, //used for dma read/write (blocks access to TCM)
	MMU_AT_DEBUG, //used for emulator debugging functions (bypasses some debug handling)
};

static INLINE u8 T1ReadByte(u8* const mem, const u32 addr)
{
   return mem[addr];
}

static INLINE u16 T1ReadWord_guaranteedAligned(void* const mem, const u32 addr)
{
	assert((addr&1)==0);
#ifdef WORDS_BIGENDIAN
   return (((u8*)mem)[addr + 1] << 8) | ((u8*)mem)[addr];
#else
   return *(u16*)((u8*)mem + addr);
#endif
}

static INLINE u16 T1ReadWord(void* const mem, const u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return (((u8*)mem)[addr + 1] << 8) | ((u8*)mem)[addr];
#else
   return *((u16 *) ((u8*)mem + addr));
#endif
}

static INLINE u32 T1ReadLong_guaranteedAligned(u8* const  mem, const u32 addr)
{
	assert((addr&3)==0);
#ifdef WORDS_BIGENDIAN
   return (mem[addr + 3] << 24 | mem[addr + 2] << 16 |
           mem[addr + 1] << 8 | mem[addr]);
#else
	return *(u32*)(mem + addr);
#endif
}


static INLINE u32 T1ReadLong(u8* const  mem, u32 addr)
{
   addr &= ~3;
#ifdef WORDS_BIGENDIAN
   return (mem[addr + 3] << 24 | mem[addr + 2] << 16 |
           mem[addr + 1] << 8 | mem[addr]);
#else
   return *(u32*)(mem + addr);
#endif
}

static INLINE u64 T1ReadQuad(u8* const mem, const u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return (u64(mem[addr + 7]) << 56 | u64(mem[addr + 6]) << 48 |
           u64(mem[addr + 5]) << 40 | u64(mem[addr + 4]) << 32 |
           u64(mem[addr + 3]) << 24 | u64(mem[addr + 2]) << 16 |
           u64(mem[addr + 1]) << 8  | u64(mem[addr    ]));
#else
   return *((u64 *) (mem + addr));
#endif
}

static INLINE void T1WriteByte(u8* const mem, const u32 addr, const u8 val)
{
   mem[addr] = val;
}

static INLINE void T1WriteWord(u8* const mem, const u32 addr, const u16 val)
{
#ifdef WORDS_BIGENDIAN
   mem[addr + 1] = val >> 8;
   mem[addr] = val & 0xFF;
#else
   *((u16 *) (mem + addr)) = val;
#endif
}

static INLINE void T1WriteLong(u8* const mem, const u32 addr, const u32 val)
{
#ifdef WORDS_BIGENDIAN
   mem[addr + 3] = val >> 24;
   mem[addr + 2] = (val >> 16) & 0xFF;
   mem[addr + 1] = (val >> 8) & 0xFF;
   mem[addr] = val & 0xFF;
#else
   *((u32 *) (mem + addr)) = val;
#endif
}

static INLINE void T1WriteQuad(u8* const mem, const u32 addr, const u64 val)
{
#ifdef WORDS_BIGENDIAN
	mem[addr + 7] = (val >> 56);
	mem[addr + 6] = (val >> 48) & 0xFF;
	mem[addr + 5] = (val >> 40) & 0xFF;
	mem[addr + 4] = (val >> 32) & 0xFF;
	mem[addr + 3] = (val >> 24) & 0xFF;
    mem[addr + 2] = (val >> 16) & 0xFF;
    mem[addr + 1] = (val >> 8) & 0xFF;
    mem[addr] = val & 0xFF;
#else
	*((u64 *) (mem + addr)) = val;
#endif
}

//static INLINE u8 T2ReadByte(u8* const  mem, const u32 addr)
//{
//#ifdef WORDS_BIGENDIAN
//   return mem[addr ^ 1];
//#else
//   return mem[addr];
//#endif
//}
//

static INLINE u16 HostReadWord(u8* const mem, const u32 addr)
{
   return *((u16 *) (mem + addr));
}

//
//static INLINE u32 T2ReadLong(u8* const mem, const u32 addr)
//{
//#ifdef WORDS_BIGENDIAN
//   return *((u16 *) (mem + addr + 2)) << 16 | *((u16 *) (mem + addr));
//#else
//   return *((u32 *) (mem + addr));
//#endif
//}
//
//static INLINE void T2WriteByte(u8* const mem, const u32 addr, const u8 val)
//{
//#ifdef WORDS_BIGENDIAN
//   mem[addr ^ 1] = val;
//#else
//   mem[addr] = val;
//#endif
//}

static INLINE void HostWriteWord(u8* const mem, const u32 addr, const u16 val)
{
   *((u16 *) (mem + addr)) = val;
}


static INLINE void HostWriteLong(u8* const mem, const u32 addr, const u32 val)
{
   *((u32 *) (mem + addr)) = val;
}

static INLINE void HostWriteTwoWords(u8* const mem, const u32 addr, const u32 val)
{
#ifdef WORDS_BIGENDIAN
   *((u16 *) (mem + addr + 2)) = val >> 16;
   *((u16 *) (mem + addr)) = val & 0xFFFF;
#else
   *((u32 *) (mem + addr)) = val;
#endif
}

#endif
