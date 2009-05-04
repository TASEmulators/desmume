/*  
	Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Code added on 18/08/2006 by shash
		- Missing missaligned addresses correction
			(reference in http://nocash.emubase.de/gbatek.htm#cpumemoryalignments)

    This file is part of DeSmuME 

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include "bios.h"
#include "debug.h"
#include "MMU.h"
#include "NDSSystem.h"
#include <assert.h>

#define cpu (&ARMPROC)
#define TEMPLATE template<int PROCNUM> 

#define REG_NUM(i, n) (((i)>>n)&0x7)

extern volatile BOOL execute;

TEMPLATE static  u32 FASTCALL OP_UND_THUMB()
{
     emu_halt();
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_LSL_0()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] = cpu->R[REG_NUM(i, 3)];
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	  
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_LSL()
{
     const u32 &i = cpu->instruction;
     u32 v = (i>>6) & 0x1F;
     cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 3)], 32-v);
     cpu->R[REG_NUM(i, 0)] = (cpu->R[REG_NUM(i, 3)] << v);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	  
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_LSR_0()
{
     const u32 &i = cpu->instruction;
	  // cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 3)]);
     cpu->R[REG_NUM(i, 0)] = 0;
     cpu->CPSR.bits.N = 0;
     cpu->CPSR.bits.Z = 1;
	  
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_LSR()
{
     const u32 &i = cpu->instruction;
     u32 v = (i>>6) & 0x1F;
     cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
     cpu->R[REG_NUM(i, 0)] = (cpu->R[REG_NUM(i, 3)] >> v);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ASR_0()
{
     const u32 &i = cpu->instruction;
     cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 3)]);
     cpu->R[REG_NUM(i, 0)] = BIT31(cpu->R[REG_NUM(i, 3)])*0xFFFFFFFF;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ASR()
{
     const u32 &i = cpu->instruction;
     u32 v = (i>>6) & 0x1F;
     cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 3)], v-1);
     cpu->R[REG_NUM(i, 0)] = (((s32)cpu->R[REG_NUM(i, 3)]) >> v);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ADD_REG()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 3)];
     u32 b = cpu->R[REG_NUM(i, 6)];
     cpu->R[REG_NUM(i, 0)] = a + b;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(a, b, cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(a, b, cpu->R[REG_NUM(i, 0)]);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_SUB_REG()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 3)];
     u32 b = cpu->R[REG_NUM(i, 6)];
     cpu->R[REG_NUM(i, 0)] = a - b;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(a, b, cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(a, b, cpu->R[REG_NUM(i, 0)]);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ADD_IMM3()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 3)];
     cpu->R[REG_NUM(i, 0)] = a + REG_NUM(i, 6);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(a, REG_NUM(i, 6), cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(a, REG_NUM(i, 6), cpu->R[REG_NUM(i, 0)]);
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_SUB_IMM3()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 3)];
     cpu->R[REG_NUM(i, 0)] = a - REG_NUM(i, 6);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(a, REG_NUM(i, 6), cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(a, REG_NUM(i, 6), cpu->R[REG_NUM(i, 0)]);
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_MOV_IMM8()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 8)] = i & 0xFF;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 8)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 8)] == 0;
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_CMP_IMM8()
{
     const u32 &i = cpu->instruction;
     u32 tmp = cpu->R[REG_NUM(i, 8)] - (i & 0xFF);
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(cpu->R[REG_NUM(i, 8)], (i & 0xFF), tmp);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(cpu->R[REG_NUM(i, 8)], (i & 0xFF), tmp);
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ADD_IMM8()
{
     const u32 &i = cpu->instruction;
     u32 tmp = cpu->R[REG_NUM(i, 8)] + (i & 0xFF);
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(cpu->R[REG_NUM(i, 8)], (i & 0xFF), tmp);
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(cpu->R[REG_NUM(i, 8)], (i & 0xFF), tmp);
     cpu->R[REG_NUM(i, 8)] = tmp;
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_SUB_IMM8()
{
     const u32 &i = cpu->instruction;
     u32 tmp = cpu->R[REG_NUM(i, 8)] - (i & 0xFF);
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(cpu->R[REG_NUM(i, 8)], (i & 0xFF), tmp);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(cpu->R[REG_NUM(i, 8)], (i & 0xFF), tmp);
     cpu->R[REG_NUM(i, 8)] = tmp;
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_AND()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] &= cpu->R[REG_NUM(i, 3)];
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_EOR()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] ^= cpu->R[REG_NUM(i, 3)];
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_LSL_REG()
{
     const u32 &i = cpu->instruction;
     u32 v = cpu->R[REG_NUM(i, 3)]&0xFF;
     
     if(!v)
     {
          cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
          cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
          return 3;
     }     
     if(v<32)
     {
          cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], 32-v);
          cpu->R[REG_NUM(i, 0)] <<= v;
          cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
          cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
          return 3;
     }
     if(v==32)
          cpu->CPSR.bits.C = BIT0(cpu->R[REG_NUM(i, 0)]);
     else
          cpu->CPSR.bits.C = 0;
     cpu->R[REG_NUM(i, 0)] = 0;
     cpu->CPSR.bits.N = 0;
     cpu->CPSR.bits.Z = 1;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_LSR_REG()
{
     const u32 &i = cpu->instruction;
     u32 v = cpu->R[REG_NUM(i, 3)]&0xFF;
     
     if(!v)
     {
          cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
          cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
          return 3;
     }     
     if(v<32)
     {
          cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
          cpu->R[REG_NUM(i, 0)] >>= v;
          cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
          cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
          return 3;
     }
     if(v==32)
          cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
     else
          cpu->CPSR.bits.C = 0;
     cpu->R[REG_NUM(i, 0)] = 0;
     cpu->CPSR.bits.N = 0;
     cpu->CPSR.bits.Z = 1;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ASR_REG()
{
     const u32 &i = cpu->instruction;
     u32 v = cpu->R[REG_NUM(i, 3)]&0xFF;
     
     if(!v)
     {
          cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
          cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
          return 3;
     }     
     if(v<32)
     {
          cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
          cpu->R[REG_NUM(i, 0)] = (u32)(((s32)cpu->R[REG_NUM(i, 0)]) >> v);
          cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
          cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
          return 3;
     }
     
     cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->R[REG_NUM(i, 0)] = BIT31(cpu->R[REG_NUM(i, 0)])*0xFFFFFFFF;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ADC_REG()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 0)];
     u32 b = cpu->R[REG_NUM(i, 3)];
     u32 tmp = b + cpu->CPSR.bits.C;
     u32 res = a + tmp;

     cpu->R[REG_NUM(i, 0)] = res;
     
     cpu->CPSR.bits.N = BIT31(res);
     cpu->CPSR.bits.Z = res == 0;

	 //the below UNSIGNED_OVERFLOW calculation is the clever way of doing it
	 //but just to keep from making a mistake, lets assert that it matches the precise definition of unsigned overflow
	 static long passcount = 0;
	 assert(++passcount);
	assert(
		((((u64)a+(u64)b+cpu->CPSR.bits.C)>>32)&1)
		== (UNSIGNED_OVERFLOW(b, (u32) cpu->CPSR.bits.C, tmp) | UNSIGNED_OVERFLOW(tmp, a, res))
		);

     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(b, (u32) cpu->CPSR.bits.C, tmp) | UNSIGNED_OVERFLOW(tmp, a, res);
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(b, (u32) cpu->CPSR.bits.C, tmp) | SIGNED_OVERFLOW(tmp, a, res);

    
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_SBC_REG()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 0)];
     u32 b = cpu->R[REG_NUM(i, 3)];
     u32 tmp = a - (!cpu->CPSR.bits.C);
     u32 res = tmp - b;
     cpu->R[REG_NUM(i, 0)] = res;
     
     cpu->CPSR.bits.N = BIT31(res);
     cpu->CPSR.bits.Z = res == 0;

	 //the below UNSIGNED_UNDERFLOW calculation is the clever way of doing it
	 //but just to keep from making a mistake, lets assert that it matches the precise definition of unsigned overflow
	 static long passcount = 0;
	 assert(++passcount);
	assert(
		((((u64)a-(u64)b-(!cpu->CPSR.bits.C))>>32)&1)
		== UNSIGNED_UNDERFLOW(a, b, res)
		);
     
	 //zero 31-dec-2008 - apply normatt's fixed logic from the arm SBC instruction
	 //although it seemed a bit odd to me and to whomever wrote this for SBC not to work similar to ADC..
	 //but thats how it is.
	 cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(a, b, res);
	 cpu->CPSR.bits.V = SIGNED_UNDERFLOW(a, b, res);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ROR_REG()
{
     const u32 &i = cpu->instruction;
     u32 v = cpu->R[REG_NUM(i, 3)]&0xFF;
     
     if(v == 0)
     {
               cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
               cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
               return 3;
     }
     v &= 0x1F;
     if(v == 0)
     {
               cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
               cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
               cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
               return 3;
     }
     cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
     cpu->R[REG_NUM(i, 0)] = ROR(cpu->R[REG_NUM(i, 0)], v);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_TST()
{
     const u32 &i = cpu->instruction;
     u32 tmp = cpu->R[REG_NUM(i, 0)] & cpu->R[REG_NUM(i, 3)];
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_NEG()
{
     const u32 &i = cpu->instruction;
     u32 a = cpu->R[REG_NUM(i, 3)];
     cpu->R[REG_NUM(i, 0)] = -((signed int)a);
     
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW((u32)0, a, cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW((u32)0, a, cpu->R[REG_NUM(i, 0)]);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_CMP()
{
     const u32 &i = cpu->instruction;
     u32 tmp = cpu->R[REG_NUM(i, 0)] -cpu->R[REG_NUM(i, 3)];
     
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)], tmp);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)], tmp);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_CMN()
{
     const u32 &i = cpu->instruction;
     u32 tmp = cpu->R[REG_NUM(i, 0)] + cpu->R[REG_NUM(i, 3)];
     
     //emu_halt();
     //log::ajouter("OP_CMN THUMB");
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)], tmp);
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)], tmp);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ORR()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] |= cpu->R[REG_NUM(i, 3)];
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_MUL_REG()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] *= cpu->R[REG_NUM(i, 3)];
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_BIC()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] &= (~cpu->R[REG_NUM(i, 3)]);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_MVN()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 0)] = (~cpu->R[REG_NUM(i, 3)]);
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
     cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ADD_SPE()
{
     const u32 &i = cpu->instruction;
     u32 Rd = (i&7) | ((i>>4)&8);
     cpu->R[Rd] += cpu->R[REG_POS(i, 3)];
     
     if(Rd==15)
          cpu->next_instruction = cpu->R[15];
          
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_CMP_SPE()
{
     const u32 &i = cpu->instruction;
     u32 Rn = (i&7) | ((i>>4)&8);
     u32 tmp = cpu->R[Rn] -cpu->R[REG_POS(i, 3)];
     
     cpu->CPSR.bits.N = BIT31(tmp);
     cpu->CPSR.bits.Z = tmp == 0;
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(cpu->R[Rn], cpu->R[REG_POS(i, 3)], tmp);
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(cpu->R[Rn], cpu->R[REG_POS(i, 3)], tmp);
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_MOV_SPE()
{
     const u32 &i = cpu->instruction;
     u32 Rd = (i&7) | ((i>>4)&8);
     cpu->R[Rd] = cpu->R[REG_POS(i, 3)];
     
     if(Rd==15)
          cpu->next_instruction = cpu->R[15];
          
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_BX_THUMB()
{
     u32 Rm = cpu->R[REG_POS(cpu->instruction, 3)];
     
     cpu->CPSR.bits.T = BIT0(Rm);
     cpu->R[15] = (Rm & 0xFFFFFFFE);
     cpu->next_instruction = cpu->R[15];
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_BLX_THUMB()
{
     u32 Rm = cpu->R[REG_POS(cpu->instruction, 3)];
     
     cpu->CPSR.bits.T = BIT0(Rm);
     cpu->R[14] = cpu->next_instruction | 1;
     cpu->R[15] = (Rm & 0xFFFFFFFE);
     cpu->next_instruction = cpu->R[15];
     
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_LDR_PCREL()
{
	u32 adr = (cpu->R[15]&0xFFFFFFFC) + ((cpu->instruction&0xFF)<<2);
     
    cpu->R[REG_NUM(cpu->instruction, 8)] = READ32(cpu->mem_if->data, adr);
               
    return 3 + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STR_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 6)] + cpu->R[REG_NUM(i, 3)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_NUM(i, 0)]);
               
     return 2 + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STRH_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
     WRITE16(cpu->mem_if->data, adr, ((u16)cpu->R[REG_NUM(i, 0)]));
               
     return 2 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STRB_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
     WRITE8(cpu->mem_if->data, adr, ((u8)cpu->R[REG_NUM(i, 0)]));
               
     return 2 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRSB_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
     cpu->R[REG_NUM(i, 0)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
               
     return 3 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDR_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = (cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)]);
	 u32 tempValue = READ32(cpu->mem_if->data, adr&0xFFFFFFFC);

	 adr = (adr&3)*8;
	 tempValue = (tempValue>>adr) | (tempValue<<(32-adr));
	 cpu->R[REG_NUM(i, 0)] = tempValue;
               
     return 3 + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRH_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
     cpu->R[REG_NUM(i, 0)] = (u32)READ16(cpu->mem_if->data, adr);
               
     return 3 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRB_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
     cpu->R[REG_NUM(i, 0)] = (u32)READ8(cpu->mem_if->data, adr);
               
     return 3 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRSH_REG_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
     cpu->R[REG_NUM(i, 0)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
               
     return 3 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STR_IMM_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>4)&0x7C);
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_NUM(i, 0)]);
               
     return 2 + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDR_IMM_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>4)&0x7C);
     u32 tempValue = READ32(cpu->mem_if->data, adr&0xFFFFFFFC);
	 adr = (adr&3)*8;
	 tempValue = (tempValue>>adr) | (tempValue<<(32-adr));
	 cpu->R[REG_NUM(i, 0)] = tempValue;
               
     return 3 + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STRB_IMM_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>6)&0x1F);
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_NUM(i, 0)]);
               
     return 2 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRB_IMM_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>6)&0x1F);
     cpu->R[REG_NUM(i, 0)] = READ8(cpu->mem_if->data, adr);
               
     return 3 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STRH_IMM_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>5)&0x3E);
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_NUM(i, 0)]);
               
     return 2 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRH_IMM_OFF()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>5)&0x3E);
     cpu->R[REG_NUM(i, 0)] = READ16(cpu->mem_if->data, adr);
               
     return 3 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STR_SPREL()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[13] + ((i&0xFF)<<2);
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_NUM(i, 8)]);
               
     return 2 + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDR_SPREL()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[13] + ((i&0xFF)<<2);
	 cpu->R[REG_NUM(i, 8)] = READ32(cpu->mem_if->data, adr);
               
     return 3 + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_ADD_2PC()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 8)] = (cpu->R[15]&0xFFFFFFFC) + ((i&0xFF)<<2);
               
     return 5;
}

TEMPLATE static  u32 FASTCALL OP_ADD_2SP()
{
     const u32 &i = cpu->instruction;
     cpu->R[REG_NUM(i, 8)] = cpu->R[13] + ((i&0xFF)<<2);
     
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ADJUST_P_SP()
{
     cpu->R[13] += ((cpu->instruction&0x7F)<<2);
     
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADJUST_M_SP()
{
     cpu->R[13] -= ((cpu->instruction&0x7F)<<2);
     
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_PUSH()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[13] - 4;
     u32 c = 0, j;
     
     for(j = 0; j<8; ++j)
          if(BIT_N(i, 7-j))
          {
               WRITE32(cpu->mem_if->data, adr, cpu->R[7-j]);
               c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
               adr -= 4;
          }
     cpu->R[13] = adr + 4;
     
     return c + 3;
}

TEMPLATE static  u32 FASTCALL OP_PUSH_LR()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[13] - 4;
     u32 c = 0, j;
     
     WRITE32(cpu->mem_if->data, adr, cpu->R[14]);
     c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
     adr -= 4;
          
     for(j = 0; j<8; ++j)
          if(BIT_N(i, 7-j))
          {
               WRITE32(cpu->mem_if->data, adr, cpu->R[7-j]);
               c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
               adr -= 4;
          }
     cpu->R[13] = adr + 4;
     
     return c + 4;
}

TEMPLATE static  u32 FASTCALL OP_POP()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[13];
     u32 c = 0, j;

     for(j = 0; j<8; ++j)
          if(BIT_N(i, j))
          {
               cpu->R[j] = READ32(cpu->mem_if->data, adr);
               c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
               adr += 4;
          }
     cpu->R[13] = adr;       
     
     return c + 2;
}

TEMPLATE static  u32 FASTCALL OP_POP_PC()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[13];
     u32 c = 0, j;
     u32 v;

     for(j = 0; j<8; ++j)
          if(BIT_N(i, j))
          {
               cpu->R[j] = READ32(cpu->mem_if->data, adr);
               c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
               adr += 4;
          }

     v = READ32(cpu->mem_if->data, adr);
     c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
     cpu->R[15] = v & 0xFFFFFFFE;
     cpu->next_instruction = v & 0xFFFFFFFE;
     if(PROCNUM==0)
          cpu->CPSR.bits.T = BIT0(v);
     adr += 4;

     cpu->R[13] = adr;   
     return c + 5;    
}

TEMPLATE static  u32 FASTCALL OP_BKPT_THUMB()
{
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_STMIA_THUMB()
{
     const u32 &i = cpu->instruction;
     u32 adr = cpu->R[REG_NUM(i, 8)];
     u32 c = 0, j;

     for(j = 0; j<8; ++j)
          if(BIT_N(i, j))
          {
               WRITE32(cpu->mem_if->data, adr, cpu->R[j]);
               c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
               adr += 4;
          }
     cpu->R[REG_NUM(i, 8)] = adr;
     return c + 2;
}

TEMPLATE static  u32 FASTCALL OP_LDMIA_THUMB()
{
     const u32 &i = cpu->instruction;
	 u32 regIndex = REG_NUM(i, 8);
     u32 adr = cpu->R[regIndex];
     u32 c = 0, j;

     for(j = 0; j<8; ++j)
          if(BIT_N(i, j))
          {
               cpu->R[j] = READ32(cpu->mem_if->data, adr);
               c += MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
               adr += 4;
          }

	// Only over-write if not on the read list
	if(!BIT_N(i, regIndex))
		cpu->R[regIndex] = adr;
   
	return c + 3;
}

TEMPLATE static  u32 FASTCALL OP_B_COND()
{
     const u32 &i = cpu->instruction;
     if(!TEST_COND((i>>8)&0xF, 0, cpu->CPSR))
          return 1;
     
     cpu->R[15] += ((s32)((s8)(i&0xFF)))<<1;
     cpu->next_instruction = cpu->R[15];
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_SWI_THUMB()
{
	if(cpu->swi_tab) {
		 //zero 25-dec-2008 - in arm, we were masking to 0x1F. 
		 //this is probably safer since an invalid opcode could crash the emu
		 //u32 swinum = cpu->instruction & 0xFF;
		u32 swinum = cpu->instruction & 0x1F;
        return cpu->swi_tab[swinum]() + 3;  
	}
	else {
        /* we use an irq thats not in the irq tab, as
        it was replaced duie to a changed intVector */
        Status_Reg tmp = cpu->CPSR;
        armcpu_switchMode(cpu, SVC);            /* enter svc mode */
        cpu->R[14] = cpu->next_instruction;            /* jump to swi Vector */
        cpu->SPSR = tmp;                        /* save old CPSR as new SPSR */
        cpu->CPSR.bits.T = 0;                   /* handle as ARM32 code */
        cpu->CPSR.bits.I = 1;
        cpu->R[15] = cpu->intVector + 0x08;
        cpu->next_instruction = cpu->R[15];
        return 3;
     }
}

#define SIGNEEXT_IMM11(i)     (((i)&0x7FF) | (BIT10(i) * 0xFFFFF800))

TEMPLATE static  u32 FASTCALL OP_B_UNCOND()
{
     const u32 &i = cpu->instruction;
     cpu->R[15] += (SIGNEEXT_IMM11(i)<<1);
     cpu->next_instruction = cpu->R[15];
     return 3;
}
 
TEMPLATE static  u32 FASTCALL OP_BLX()
{
     const u32 &i = cpu->instruction;
     cpu->R[15] = (cpu->R[14] + ((i&0x7FF)<<1))&0xFFFFFFFC;
     cpu->R[14] = cpu->next_instruction | 1;
     cpu->next_instruction = cpu->R[15];
     cpu->CPSR.bits.T = 0;
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_BL_10()
{
     const u32 &i = cpu->instruction;
     cpu->R[14] = cpu->R[15] + (SIGNEEXT_IMM11(i)<<12);
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_BL_THUMB()
{
     const u32 &i = cpu->instruction;
     cpu->R[15] = (cpu->R[14] + ((i&0x7FF)<<1));
     cpu->R[14] = cpu->next_instruction | 1;
     cpu->next_instruction = cpu->R[15];
     return 3;
}

#define TYPE_RETOUR u32
#define CALLTYPE FASTCALL 
#define PARAMETRES
#define NOM_THUMB_TAB     thumb_instructions_set_0
#define TABDECL(x)		x<0>

#include "thumb_tabdef.inc"

#undef TYPE_RETOUR
#undef PARAMETRES  
#undef CALLTYPE
#undef NOM_THUMB_TAB
#undef TABDECL

#define TYPE_RETOUR u32
#define PARAMETRES  
#define CALLTYPE    FASTCALL 
#define NOM_THUMB_TAB     thumb_instructions_set_1
#define TABDECL(x)		x<1>

#include "thumb_tabdef.inc"
