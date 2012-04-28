/*	Copyright (C) 2006 yopyop
	Copyright (C) 2011 Loren Merritt
	Copyright (C) 2012 DeSmuME team

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

typedef u32 (FASTCALL* ArmOpCompiled)();

void arm_jit_reset(bool enable);
void arm_jit_sync();
template<int PROCNUM> u32 arm_jit_compile();

#ifndef HAVE_STATIC_CODE_BUFFER
struct JIT_struct 
{
	//Shared ram
	u32 MAIN_MEM[16*1024*1024];
	u32 SWIRAM[0x8000];
	
	//ARM9 mem
	u32 ARM9_ITCM[0x8000];
	u32 ARM9_DTCM[0x4000];
	u32 ARM9_LCD[0xC4000];
	u32 ARM9_BIOS[0x8000];
	//ARM7 mem
	u32 ARM7_BIOS[0x4000];
	u32 ARM7_ERAM[0x10000];
	u32 ARM7_WIRAM[0x10000];

	static u32 *JIT_MEM[2][256];
	static u32 JIT_MASK[2][256];
};
extern CACHE_ALIGN JIT_struct JIT;
#define JIT_COMPILED_FUNC(adr) JIT.JIT_MEM[PROCNUM][(adr&0x0FFFFFFE)>>20][(adr&0x0FFFFFFE)&JIT.JIT_MASK[PROCNUM][(adr&0x0FFFFFFE)>>20]]
#else
// actually an array of function pointers, but they fit in 32bit address space, so might as well save memory
extern u32 compiled_funcs[];
#define JIT_COMPILED_FUNC(adr) compiled_funcs[(adr & 0x0FFFFFFE) >> 1]
#endif


#endif
