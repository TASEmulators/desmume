/*  Copyright 2005-2006 Guillaume Duhamel
    Copyright 2005 Theo Berkau

    This file is part of Yabause.

    Yabause is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Yabause is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Yabause; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef MEM_H
#define MEM_H

#include <stdlib.h>
#include "types.h"

/* Type 1 Memory, faster for byte (8 bits) accesses */

static INLINE u8 T1ReadByte(u8 * mem, u32 addr)
{
   return mem[addr];
}

static INLINE u16 T1ReadWord(u8 * mem, u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return (mem[addr + 1] << 8) | mem[addr];
#else
   return *((u16 *) (mem + addr));
#endif
}

static INLINE u32 T1ReadLong(u8 * mem, u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return (mem[addr + 3] << 24 | mem[addr + 2] << 16 |
           mem[addr + 1] << 8 | mem[addr]);
#else
   return *((u32 *)mem + (addr>>2));
#endif
}

static INLINE u64 T1ReadQuad(u8 * mem, u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return (mem[addr + 7] << 56 | mem[addr + 6] << 48 |
	   mem[addr + 5] << 40 | mem[addr + 4] << 32 |
           mem[addr + 3] << 24 | mem[addr + 2] << 16 |
           mem[addr + 1] << 8 | mem[addr]);
#else
   return *((u64 *) (mem + addr));
#endif
}

static INLINE void T1WriteByte(u8 * mem, u32 addr, u8 val)
{
   mem[addr] = val;
}

static INLINE void T1WriteWord(u8 * mem, u32 addr, u16 val)
{
#ifdef WORDS_BIGENDIAN
   mem[addr + 1] = val >> 8;
   mem[addr] = val & 0xFF;
#else
   *((u16 *) (mem + addr)) = val;
#endif
}

static INLINE void T1WriteLong(u8 * mem, u32 addr, u32 val)
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

/* Type 2 Memory, faster for word (16 bits) accesses */

static INLINE u8 T2ReadByte(u8 * mem, u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return mem[addr ^ 1];
#else
   return mem[addr];
#endif
}

static INLINE u16 T2ReadWord(u8 * mem, u32 addr)
{
   return *((u16 *) (mem + addr));
}

static INLINE u32 T2ReadLong(u8 * mem, u32 addr)
{
#ifdef WORDS_BIGENDIAN
   return *((u16 *) (mem + addr + 2)) << 16 | *((u16 *) (mem + addr));
#else
   return *((u32 *) (mem + addr));
#endif
}

static INLINE void T2WriteByte(u8 * mem, u32 addr, u8 val)
{
#ifdef WORDS_BIGENDIAN
   mem[addr ^ 1] = val;
#else
   mem[addr] = val;
#endif
}

static INLINE void T2WriteWord(u8 * mem, u32 addr, u16 val)
{
   *((u16 *) (mem + addr)) = val;
}

static INLINE void T2WriteLong(u8 * mem, u32 addr, u32 val)
{
#ifdef WORDS_BIGENDIAN
   *((u16 *) (mem + addr + 2)) = val >> 16;
   *((u16 *) (mem + addr)) = val & 0xFFFF;
#else
   *((u32 *) (mem + addr)) = val;
#endif
}

#endif
