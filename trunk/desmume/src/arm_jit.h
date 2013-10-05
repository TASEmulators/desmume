/*	Copyright (C) 2006 yopyop
	Copyright (C) 2011 Loren Merritt
	Copyright (C) 2012-2013 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 3 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifndef ARM_JIT
#define ARM_JIT

#include "types.h"
#ifndef _MSC_VER
#include <stdint.h>
#endif

typedef u32 (FASTCALL* ArmOpCompiled)();

void arm_jit_reset(bool enable, bool suppress_msg = false);
void arm_jit_close();
void arm_jit_sync();
template<int PROCNUM> u32 arm_jit_compile();

#if defined(_WINDOWS) || defined(DESMUME_COCOA)
#define MAPPED_JIT_FUNCS
#endif
#ifdef MAPPED_JIT_FUNCS
struct JIT_struct 
{
	// only include the memory types that code can execute from
	uintptr_t MAIN_MEM[16*1024*1024/2];
	uintptr_t SWIRAM[0x8000/2];
	uintptr_t ARM9_ITCM[0x8000/2];
	uintptr_t ARM9_LCDC[0xA4000/2];
	uintptr_t ARM9_BIOS[0x8000/2];
	uintptr_t ARM7_BIOS[0x4000/2];
	uintptr_t ARM7_ERAM[0x10000/2];
	uintptr_t ARM7_WIRAM[0x10000/2];
	uintptr_t ARM7_WRAM[0x40000/2];

	static uintptr_t *JIT_MEM[2][0x4000];
};
extern CACHE_ALIGN JIT_struct JIT;
#define JIT_COMPILED_FUNC(adr, PROCNUM) JIT.JIT_MEM[PROCNUM][((adr)&0x0FFFC000)>>14][((adr)&0x00003FFE)>>1]
#define JIT_COMPILED_FUNC_PREMASKED(adr, PROCNUM, ofs) JIT.JIT_MEM[PROCNUM][(adr)>>14][(((adr)&0x00003FFE)>>1)+ofs]
#define JIT_COMPILED_FUNC_KNOWNBANK(adr, bank, mask, ofs) JIT.bank[(((adr)&(mask))>>1)+ofs]
#define JIT_MAPPED(adr, PROCNUM) JIT.JIT_MEM[PROCNUM][(adr)>>14]
#else
// actually an array of function pointers, but they fit in 32bit address space, so might as well save memory
extern uintptr_t compiled_funcs[];
// there isn't anything mapped between 07000000 and 0EFFFFFF, so we can mask off bit 27 and get away with a smaller array
#define JIT_COMPILED_FUNC(adr, PROCNUM) compiled_funcs[((adr) & 0x07FFFFFE) >> 1]
#define JIT_COMPILED_FUNC_PREMASKED(adr, PROCNUM, ofs) JIT_COMPILED_FUNC(adr+(ofs<<1), PROCNUM)
#define JIT_COMPILED_FUNC_KNOWNBANK(adr, bank, mask, ofs) JIT_COMPILED_FUNC(adr+(ofs<<1), PROCNUM)
#define JIT_MAPPED(adr, PROCNUM) true
#endif

extern u32 saveBlockSizeJIT;

#endif
