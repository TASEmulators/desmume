/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2008 shash
	Copyright (C) 2008-2013 DeSmuME team

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

#include "bios.h"
#include "debug.h"
#include "MMU.h"
#include "NDSSystem.h"
#include "MMU_timing.h"
#include <assert.h>

#define cpu (&ARMPROC)
#define TEMPLATE template<int PROCNUM> 

#define REG_NUM(i, n) (((i)>>n)&0x7)

//-----------------------------------------------------------------------------
//   Undefined instruction
//-----------------------------------------------------------------------------
TEMPLATE static  u32 FASTCALL OP_UND_THUMB(const u32 i)
{
	INFO("THUMB%c: Undefined instruction: 0x%08X (%s) PC=0x%08X\n", cpu->proc_ID?'7':'9', cpu->instruction, decodeIntruction(true, cpu->instruction), cpu->instruct_adr);
	TRAPUNDEF(cpu);
	return 1;
}

//-----------------------------------------------------------------------------
//   LSL
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_LSL_0(const u32 i)
{
	cpu->R[REG_NUM(i, 0)] = cpu->R[REG_NUM(i, 3)];
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_LSL(const u32 i)
{
	u32 v = (i>>6) & 0x1F;
	cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 3)], 32-v);
	cpu->R[REG_NUM(i, 0)] = (cpu->R[REG_NUM(i, 3)] << v);
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_LSL_REG(const u32 i)
{
	u32 v = cpu->R[REG_NUM(i, 3)] & 0xFF;

	if(v == 0)
	{
		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		return 2;
	}	
	if(v<32)
	{
		cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], 32-v);
		cpu->R[REG_NUM(i, 0)] <<= v;
		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		return 2;
	}
	if(v==32)
		cpu->CPSR.bits.C = BIT0(cpu->R[REG_NUM(i, 0)]);
	else
		cpu->CPSR.bits.C = 0;

	cpu->R[REG_NUM(i, 0)] = 0;
	cpu->CPSR.bits.N = 0;
	cpu->CPSR.bits.Z = 1;

	return 2;
}

//-----------------------------------------------------------------------------
//   LSR
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_LSR_0(const u32 i)
{
	cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 3)]);
	cpu->R[REG_NUM(i, 0)] = 0;
	cpu->CPSR.bits.N = 0;
	cpu->CPSR.bits.Z = 1;

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_LSR(const u32 i)
{
	u32 v = (i>>6) & 0x1F;
	cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 3)], v-1);
	cpu->R[REG_NUM(i, 0)] = (cpu->R[REG_NUM(i, 3)] >> v);
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_LSR_REG(const u32 i)
{
	u32 v = cpu->R[REG_NUM(i, 3)] & 0xFF;
	
	if(v == 0)
	{
		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		return 2;
	}	
	if(v<32)
	{
		cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
		cpu->R[REG_NUM(i, 0)] >>= v;
		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		return 2;
	}
	if(v==32)
		cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
	else
		cpu->CPSR.bits.C = 0;
	cpu->R[REG_NUM(i, 0)] = 0;
	cpu->CPSR.bits.N = 0;
	cpu->CPSR.bits.Z = 1;
	
	return 2;
}

//-----------------------------------------------------------------------------
//   ASR
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_ASR_0(const u32 i)
{
	cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 3)]);
	cpu->R[REG_NUM(i, 0)] = BIT31(cpu->R[REG_NUM(i, 3)])*0xFFFFFFFF;
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ASR(const u32 i)
{
	u32 v = (i>>6) & 0x1F;
	cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 3)], v-1);
	cpu->R[REG_NUM(i, 0)] = (u32)(((s32)cpu->R[REG_NUM(i, 3)]) >> v);
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ASR_REG(const u32 i)
{
	u32 v = cpu->R[REG_NUM(i, 3)] & 0xFF;
	
	if(v == 0)
	{
		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		return 2;
	}	
	if(v<32)
	{
		cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
		cpu->R[REG_NUM(i, 0)] = (u32)(((s32)cpu->R[REG_NUM(i, 0)]) >> v);
		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		return 2;
	}
	
	cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->R[REG_NUM(i, 0)] = BIT31(cpu->R[REG_NUM(i, 0)])*0xFFFFFFFF;
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	
	return 2;
}

//-----------------------------------------------------------------------------
//   ADD
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_ADD_IMM3(const u32 i)
{
	u32 imm3 = (i >> 6) & 0x07;
	u32 Rn = cpu->R[REG_NUM(i, 3)];

	if (imm3 == 0)	// mov 2
	{
		cpu->R[REG_NUM(i, 0)] = Rn;

		cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
		cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
		cpu->CPSR.bits.C = 0;
		cpu->CPSR.bits.V = 0;
		return 1;
	}

	cpu->R[REG_NUM(i, 0)] = Rn + imm3;
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	cpu->CPSR.bits.C = CarryFrom(Rn, imm3);
	cpu->CPSR.bits.V = OverflowFromADD(cpu->R[REG_NUM(i, 0)], Rn, imm3);

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADD_IMM8(const u32 i)
{
	u32 imm8 = (i & 0xFF);
	u32 Rd = cpu->R[REG_NUM(i, 8)];

	cpu->R[REG_NUM(i, 8)] = Rd + imm8;
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 8)]);
	cpu->CPSR.bits.Z = (cpu->R[REG_NUM(i, 8)] == 0);
	cpu->CPSR.bits.C = CarryFrom(Rd, imm8);
	cpu->CPSR.bits.V = OverflowFromADD(cpu->R[REG_NUM(i, 8)], Rd, imm8);

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADD_REG(const u32 i)
{
	u32 Rn = cpu->R[REG_NUM(i, 3)];
	u32 Rm = cpu->R[REG_NUM(i, 6)];

	cpu->R[REG_NUM(i, 0)] = Rn + Rm;
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	cpu->CPSR.bits.C = CarryFrom(Rn, Rm);
	cpu->CPSR.bits.V = OverflowFromADD(cpu->R[REG_NUM(i, 0)], Rn, Rm);

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADD_SPE(const u32 i)
{
	u32 Rd = REG_NUM(i, 0) | ((i>>4)&8);
	
	cpu->R[Rd] += cpu->R[REG_POS(i, 3)];
	
	if(Rd==15)
	{
		cpu->next_instruction = cpu->R[15];
		return 3;
	}
		
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADD_2PC(const u32 i)
{
	cpu->R[REG_NUM(i, 8)] = (cpu->R[15]&0xFFFFFFFC) + ((i&0xFF)<<2);
			
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADD_2SP(const u32 i)
{
	cpu->R[REG_NUM(i, 8)] = cpu->R[13] + ((i&0xFF)<<2);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   SUB
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_SUB_IMM3(const u32 i)
{
	u32 imm3 = (i>>6) & 0x07;
	u32 Rn = cpu->R[REG_NUM(i, 3)];
	u32 tmp = Rn - imm3;

	cpu->R[REG_NUM(i, 0)] = tmp;
	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = (tmp == 0);
	cpu->CPSR.bits.C = !BorrowFrom(Rn, imm3);
	cpu->CPSR.bits.V = OverflowFromSUB(tmp, Rn, imm3);

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_SUB_IMM8(const u32 i)
{
	u32 imm8 = (i & 0xFF);
	u32 Rd = cpu->R[REG_NUM(i, 8)];
	u32 tmp = Rd - imm8;

	cpu->R[REG_NUM(i, 8)] = tmp;
	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = (tmp == 0);
	cpu->CPSR.bits.C = !BorrowFrom(Rd, imm8);
	cpu->CPSR.bits.V = OverflowFromSUB(tmp, Rd, imm8);

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_SUB_REG(const u32 i)
{
	u32 Rn = cpu->R[REG_NUM(i, 3)];
	u32 Rm = cpu->R[REG_NUM(i, 6)];
	u32 tmp = Rn - Rm;

	cpu->R[REG_NUM(i, 0)] = tmp;
	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = (tmp == 0);
	cpu->CPSR.bits.C = !BorrowFrom(Rn, Rm);
	cpu->CPSR.bits.V = OverflowFromSUB(tmp, Rn, Rm);

	return 1;
}

//-----------------------------------------------------------------------------
//   MOV
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_MOV_IMM8(const u32 i)
{
	cpu->R[REG_NUM(i, 8)] = (i & 0xFF);
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 8)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 8)] == 0;
	
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_MOV_SPE(const u32 i)
{
	u32 Rd = REG_NUM(i, 0) | ((i>>4)&8);
	
	cpu->R[Rd] = cpu->R[REG_POS(i, 3)];
	
	if(Rd==15)
	{
		cpu->next_instruction = cpu->R[15];
		return 3;
	}
		
	return 1;
}

//-----------------------------------------------------------------------------
//   CMP
//-----------------------------------------------------------------------------
TEMPLATE static  u32 FASTCALL OP_CMP_IMM8(const u32 i)
{
	u32 tmp = cpu->R[REG_NUM(i, 8)] - (i & 0xFF);

	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = tmp == 0;
	cpu->CPSR.bits.C = !BorrowFrom(cpu->R[REG_NUM(i, 8)], (i & 0xFF));
	cpu->CPSR.bits.V = OverflowFromSUB(tmp, cpu->R[REG_NUM(i, 8)], (i & 0xFF));

	return 1;
}

TEMPLATE static  u32 FASTCALL OP_CMP(const u32 i)
{
	u32 tmp = cpu->R[REG_NUM(i, 0)] - cpu->R[REG_NUM(i, 3)];

	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = tmp == 0;
	cpu->CPSR.bits.C = !BorrowFrom(cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)]);
	cpu->CPSR.bits.V = OverflowFromSUB(tmp, cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)]);
	
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_CMP_SPE(const u32 i)
{
	u32 Rn = (i&7) | ((i>>4)&8);

	u32 tmp = cpu->R[Rn] - cpu->R[REG_POS(i, 3)];
	
	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = tmp == 0;
	cpu->CPSR.bits.C = !BorrowFrom(cpu->R[Rn], cpu->R[REG_POS(i, 3)]);
	cpu->CPSR.bits.V = OverflowFromSUB(tmp, cpu->R[Rn], cpu->R[REG_POS(i, 3)]);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   AND
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_AND(const u32 i)
{
	cpu->R[REG_NUM(i, 0)] &= cpu->R[REG_NUM(i, 3)];
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	return 1;
}

//-----------------------------------------------------------------------------
//   EOR
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_EOR(const u32 i)
{
	cpu->R[REG_NUM(i, 0)] ^= cpu->R[REG_NUM(i, 3)];
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	
	return 1;
}

//-----------------------------------------------------------------------------
//   ADC
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_ADC_REG(const u32 i)
{
	u32 Rd = cpu->R[REG_NUM(i, 0)];
	u32 Rm = cpu->R[REG_NUM(i, 3)];

	if (!cpu->CPSR.bits.C)
	{
		cpu->R[REG_NUM(i, 0)] = Rd + Rm;
		cpu->CPSR.bits.C = cpu->R[REG_NUM(i, 0)] < Rm;
	}
	else
	{
		cpu->R[REG_NUM(i, 0)] = Rd + Rm + 1;
		cpu->CPSR.bits.C =  cpu->R[REG_NUM(i, 0)] <= Rm;
	}
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = (cpu->R[REG_NUM(i, 0)] == 0);
	cpu->CPSR.bits.V = BIT31((Rd ^ Rm ^ -1) & (Rd ^ cpu->R[REG_NUM(i, 0)]));
	
	return 1;
}

//-----------------------------------------------------------------------------
//   SBC
//-----------------------------------------------------------------------------
TEMPLATE static  u32 FASTCALL OP_SBC_REG(const u32 i)
{
	u32 Rd = cpu->R[REG_NUM(i, 0)];
	u32 Rm = cpu->R[REG_NUM(i, 3)];

	if (!cpu->CPSR.bits.C)
	{
		cpu->R[REG_NUM(i, 0)] = Rd - Rm - 1;
		cpu->CPSR.bits.C = Rd > Rm;
	}
	else
	{
		cpu->R[REG_NUM(i, 0)] = Rd - Rm;
		cpu->CPSR.bits.C = Rd >= Rm;
	}

	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = (cpu->R[REG_NUM(i, 0)] == 0);
	cpu->CPSR.bits.V = BIT31((Rd ^ Rm) & (Rd ^ cpu->R[REG_NUM(i, 0)]));
	
	return 1;
}

//-----------------------------------------------------------------------------
//   ROR
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_ROR_REG(const u32 i)
{
	u32 v = cpu->R[REG_NUM(i, 3)] & 0xFF;
	
	if(v == 0)
	{
			cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
			cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
			return 2;
	}
	
	v &= 0x1F;
	if(v == 0)
	{
			cpu->CPSR.bits.C = BIT31(cpu->R[REG_NUM(i, 0)]);
			cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
			cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
			return 2;
	}
	cpu->CPSR.bits.C = BIT_N(cpu->R[REG_NUM(i, 0)], v-1);
	cpu->R[REG_NUM(i, 0)] = ROR(cpu->R[REG_NUM(i, 0)], v);
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	
	return 2;
}

//-----------------------------------------------------------------------------
//   TST
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_TST(const u32 i)
{
	u32 tmp = cpu->R[REG_NUM(i, 0)] & cpu->R[REG_NUM(i, 3)];
	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = (tmp == 0);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   NEG
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_NEG(const u32 i)
{
	u32 Rm = cpu->R[REG_NUM(i, 3)];

	cpu->R[REG_NUM(i, 0)] = (u32)((s32)0 - (s32)Rm);
	
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = (cpu->R[REG_NUM(i, 0)] == 0);
	cpu->CPSR.bits.C = !BorrowFrom(0, Rm);
	cpu->CPSR.bits.V = OverflowFromSUB(cpu->R[REG_NUM(i, 0)], 0, Rm);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   CMN
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_CMN(const u32 i)
{
	u32 tmp = cpu->R[REG_NUM(i, 0)] + cpu->R[REG_NUM(i, 3)];

	cpu->CPSR.bits.N = BIT31(tmp);
	cpu->CPSR.bits.Z = tmp == 0;
	cpu->CPSR.bits.C = CarryFrom(cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)]);
	cpu->CPSR.bits.V = OverflowFromADD(tmp, cpu->R[REG_NUM(i, 0)], cpu->R[REG_NUM(i, 3)]);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   ORR
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_ORR(const u32 i)
{
	cpu->R[REG_NUM(i, 0)] |= cpu->R[REG_NUM(i, 3)];

	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = (cpu->R[REG_NUM(i, 0)] == 0);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   BIC
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_BIC(const u32 i)
{
	cpu->R[REG_NUM(i, 0)] &= (~cpu->R[REG_NUM(i, 3)]);
	
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = (cpu->R[REG_NUM(i, 0)] == 0);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   MVN
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_MVN(const u32 i)
{
	cpu->R[REG_NUM(i, 0)] = (~cpu->R[REG_NUM(i, 3)]);
	
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	
	return 1;
}

//-----------------------------------------------------------------------------
//   MUL
//-----------------------------------------------------------------------------

#define MUL_Mxx_END_THUMB(c) \
	v >>= 8; \
	if((v==0)||(v==0xFFFFFF)) \
		return c+1; \
	v >>= 8; \
	if((v==0)||(v==0xFFFF)) \
		return c+2; \
	v >>= 8; \
	if((v==0)||(v==0xFF)) \
		return c+3; \
	return c+4; \

TEMPLATE static  u32 FASTCALL OP_MUL_REG(const u32 i)
{
	u32 v = cpu->R[REG_NUM(i, 3)];

	// FIXME:
	//------ Rd = (Rm * Rd)[31:0]
	//------ u64 res = ((u64)cpu->R[REG_NUM(i, 0)] * (u64)v));
	//------ cpu->R[REG_NUM(i, 0)] = (u32)(res  & 0xFFFFFFFF);
	//------ 
	
	cpu->R[REG_NUM(i, 0)] *= v;
	cpu->CPSR.bits.N = BIT31(cpu->R[REG_NUM(i, 0)]);
	cpu->CPSR.bits.Z = cpu->R[REG_NUM(i, 0)] == 0;
	//The MUL instruction is defined to leave the C flag unchanged in ARMv5 and above.
	//In earlier versions of the architecture, the value of the C flag was UNPREDICTABLE
	//after a MUL instruction.
	
	if (PROCNUM == 1)	// ARM4T 1S + mI, m = 3
		return 4;

	MUL_Mxx_END_THUMB(1);
}

//-----------------------------------------------------------------------------
//   STRB / LDRB
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_STRB_IMM_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>6)&0x1F);
	WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_NUM(i, 0)]);
			
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDRB_IMM_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>6)&0x1F);
	cpu->R[REG_NUM(i, 0)] = (u32)READ8(cpu->mem_if->data, adr);
			
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(3, adr);
}


TEMPLATE static  u32 FASTCALL OP_STRB_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_NUM(i, 0)]);
			
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDRB_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	cpu->R[REG_NUM(i, 0)] = (u32)READ8(cpu->mem_if->data, adr);
			
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(3, adr);
}

//-----------------------------------------------------------------------------
//   LDRSB
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_LDRSB_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	cpu->R[REG_NUM(i, 0)] = (u32)((s8)READ8(cpu->mem_if->data, adr));
			
	return MMU_aluMemAccessCycles<PROCNUM,8,MMU_AD_READ>(3, adr);
}

//-----------------------------------------------------------------------------
//   STRH / LDRH
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_STRH_IMM_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>5)&0x3E);
	WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_NUM(i, 0)]);
			
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDRH_IMM_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>5)&0x3E);
	cpu->R[REG_NUM(i, 0)] = (u32)READ16(cpu->mem_if->data, adr);
			
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(3, adr);
}


TEMPLATE static  u32 FASTCALL OP_STRH_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	WRITE16(cpu->mem_if->data, adr, ((u16)cpu->R[REG_NUM(i, 0)]));
			
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDRH_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	cpu->R[REG_NUM(i, 0)] = (u32)READ16(cpu->mem_if->data, adr);
			
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(3, adr);
}

//-----------------------------------------------------------------------------
//   LDRSH
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_LDRSH_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
	cpu->R[REG_NUM(i, 0)] = (u32)((s16)READ16(cpu->mem_if->data, adr));
			
	return MMU_aluMemAccessCycles<PROCNUM,16,MMU_AD_READ>(3, adr);
}

//-----------------------------------------------------------------------------
//   STR / LDR
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_STR_IMM_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>4)&0x7C);
	WRITE32(cpu->mem_if->data, adr, cpu->R[REG_NUM(i, 0)]);
			
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDR_IMM_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>4)&0x7C);
	u32 tempValue = READ32(cpu->mem_if->data, adr);
	adr = (adr&3)*8;
	tempValue = (tempValue>>adr) | (tempValue<<(32-adr));
	cpu->R[REG_NUM(i, 0)] = tempValue;
		
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_READ>(3, adr);
}


TEMPLATE static  u32 FASTCALL OP_STR_REG_OFF(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 6)] + cpu->R[REG_NUM(i, 3)];
	WRITE32(cpu->mem_if->data, adr, cpu->R[REG_NUM(i, 0)]);
			
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDR_REG_OFF(const u32 i)
{
	u32 adr = (cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)]);
	u32 tempValue = READ32(cpu->mem_if->data, adr);
	adr = (adr&3)*8;
	tempValue = (tempValue>>adr) | (tempValue<<(32-adr));
	cpu->R[REG_NUM(i, 0)] = tempValue;
			
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_READ>(3, adr);
}

TEMPLATE static  u32 FASTCALL OP_STR_SPREL(const u32 i)
{
	u32 adr = cpu->R[13] + ((i&0xFF)<<2);
	WRITE32(cpu->mem_if->data, adr, cpu->R[REG_NUM(i, 8)]);
			
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_WRITE>(2, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDR_SPREL(const u32 i)
{
	u32 adr = cpu->R[13] + ((i&0xFF)<<2);
	cpu->R[REG_NUM(i, 8)] = READ32(cpu->mem_if->data, adr);
			
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_READ>(3, adr);
}

TEMPLATE static  u32 FASTCALL OP_LDR_PCREL(const u32 i)
{
	u32 adr = (cpu->R[15]&0xFFFFFFFC) + ((i&0xFF)<<2);
	
	cpu->R[REG_NUM(i, 8)] = READ32(cpu->mem_if->data, adr);
			
	return MMU_aluMemAccessCycles<PROCNUM,32,MMU_AD_READ>(3, adr);
}

//-----------------------------------------------------------------------------
//   Adjust SP
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_ADJUST_P_SP(const u32 i)
{
	cpu->R[13] += ((i&0x7F)<<2);
	
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADJUST_M_SP(const u32 i)
{
	cpu->R[13] -= ((i&0x7F)<<2);
	
	return 1;
}

//-----------------------------------------------------------------------------
//   PUSH / POP
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_PUSH(const u32 i)
{
	u32 adr = cpu->R[13] - 4;
	u32 c = 0, j;
	
	for(j = 0; j<8; j++)
		if(BIT_N(i, 7-j))
		{
			WRITE32(cpu->mem_if->data, adr, cpu->R[7-j]);
			c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr);
			adr -= 4;
		}
	cpu->R[13] = adr + 4;
	
	 return MMU_aluMemCycles<PROCNUM>(3, c);
}

TEMPLATE static  u32 FASTCALL OP_PUSH_LR(const u32 i)
{
	u32 adr = cpu->R[13] - 4;
	u32 c = 0, j;
	
	WRITE32(cpu->mem_if->data, adr, cpu->R[14]);
	c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr);
	adr -= 4;
		
	for(j = 0; j<8; j++)
		if(BIT_N(i, 7-j))
		{
			WRITE32(cpu->mem_if->data, adr, cpu->R[7-j]);
			c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr);
			adr -= 4;
		}
	cpu->R[13] = adr + 4;
	
	 return MMU_aluMemCycles<PROCNUM>(4, c);
}

TEMPLATE static  u32 FASTCALL OP_POP(const u32 i)
{
	u32 adr = cpu->R[13];
	u32 c = 0, j;

	for(j = 0; j<8; j++)
		if(BIT_N(i, j))
		{
			cpu->R[j] = READ32(cpu->mem_if->data, adr);
			c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr);
			adr += 4;
		}
	cpu->R[13] = adr;	  
	
	 return MMU_aluMemCycles<PROCNUM>(2, c);
}

// In ARMv5 and above, bit[0] of the loaded value
// determines whether execution continues after this branch in ARM state or in Thumb state, as though the
// following instruction had been executed:
// BX (loaded_value)
// In T variants of ARMv4, bit[0] of the loaded value is ignored and execution continues in Thumb state, as
// though the following instruction had been executed:
// MOV PC,(loaded_value)
TEMPLATE static  u32 FASTCALL OP_POP_PC(const u32 i)
{
	u32 adr = cpu->R[13];
	u32 c = 0, j;
	u32 v = 0;

	for(j = 0; j<8; j++)
		if(BIT_N(i, j))
		{
			cpu->R[j] = READ32(cpu->mem_if->data, adr);
			c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr);
			adr += 4;
		}

	v = READ32(cpu->mem_if->data, adr);
	c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr);
	if(PROCNUM==0)
		cpu->CPSR.bits.T = BIT0(v);

	cpu->R[15] = v & 0xFFFFFFFE;
	cpu->next_instruction = cpu->R[15];
	
	cpu->R[13] = adr + 4;
	 return MMU_aluMemCycles<PROCNUM>(5, c);
}

//-----------------------------------------------------------------------------
//   STMIA / LDMIA
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_STMIA_THUMB(const u32 i)
{
	u32 adr = cpu->R[REG_NUM(i, 8)];
	u32 c = 0, j;
	u32 erList = 1; //Empty Register List

	// ------ ARM_REF:
	// ------ If <Rn> is specified in <registers>:
	// ------	* If <Rn> is the lowest-numbered register specified in <registers>, the original value of <Rn> is stored.
	// ------	* Otherwise, the stored value of <Rn> is UNPREDICTABLE.
	if (BIT_N(i, REG_NUM(i, 8)))
		printf("STMIA with Rb in Rlist\n");

	for(j = 0; j<8; j++)
	{
		if(BIT_N(i, j))
		{
			WRITE32(cpu->mem_if->data, adr, cpu->R[j]);
			c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_WRITE>(adr);
			adr += 4;
			erList = 0; //Register List isnt empty
		}
	}

	if (erList)
		 printf("STMIA with Empty Rlist\n");

	cpu->R[REG_NUM(i, 8)] = adr;
	return MMU_aluMemCycles<PROCNUM>(2, c);
}

TEMPLATE static  u32 FASTCALL OP_LDMIA_THUMB(const u32 i)
{
	u32 regIndex = REG_NUM(i, 8);
	u32 adr = cpu->R[regIndex];
	u32 c = 0, j;
	u32 erList = 1; //Empty Register List

	//if (BIT_N(i, regIndex))
	//	 printf("LDMIA with Rb in Rlist at %08X\n",cpu->instruct_adr);

	for(j = 0; j<8; j++)
	{
		if(BIT_N(i, j))
		{
			cpu->R[j] = READ32(cpu->mem_if->data, adr);
			c += MMU_memAccessCycles<PROCNUM,32,MMU_AD_READ>(adr);
			adr += 4;
			erList = 0; //Register List isnt empty
		}
	}

	if (erList)
		 printf("LDMIA with Empty Rlist\n");

	// ARM_REF:	THUMB: Causes base register write-back, and is not optional
	// ARM_REF:	If the base register <Rn> is specified in <registers>, the final value of <Rn> is the loaded value
	//			(not the written-back value).
	if (!BIT_N(i, regIndex))
		cpu->R[regIndex] = adr;
   
	return MMU_aluMemCycles<PROCNUM>(3, c);
}

//-----------------------------------------------------------------------------
//   BKPT
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_BKPT_THUMB(const u32 i)
{
	printf("THUMB%c: OP_BKPT triggered\n", PROCNUM?'7':'9');
	Status_Reg tmp = cpu->CPSR;
	armcpu_switchMode(cpu, ABT);				// enter abt mode
	cpu->R[14] = cpu->instruct_adr + 4;
	cpu->SPSR = tmp;							// save old CPSR as new SPSR
	cpu->CPSR.bits.T = 0;						// handle as ARM32 code
	cpu->CPSR.bits.I = 1;
	cpu->changeCPSR();
	cpu->R[15] = cpu->intVector + 0x0C;
	cpu->next_instruction = cpu->R[15];
	return 1;
}

//-----------------------------------------------------------------------------
//   SWI
//-----------------------------------------------------------------------------

TEMPLATE static  u32 FASTCALL OP_SWI_THUMB(const u32 i)
{
	u32 swinum = i & 0xFF;

	//ideas-style debug prints (execute this SWI with the null terminated string address in R0)
	if(swinum==0xFC) {
		IdeasLog(cpu);
		return 0;
	}

	//if the user has changed the intVector to point away from the nds bioses,
	//then it doesn't really make any sense to use the builtin SWI's since 
	//the bios ones aren't getting called anyway
	bool bypassBuiltinSWI = 
		(cpu->intVector == 0x00000000 && PROCNUM==0)
		|| (cpu->intVector == 0xFFFF0000 && PROCNUM==1);

	//printf("THUMB%c SWI %02X\t; %s\n", PROCNUM?'7':'9', (swinum & 0x1F), ARM_swi_names[PROCNUM][(swinum & 0x1F)]);

	if(cpu->swi_tab && !bypassBuiltinSWI) {
		//zero 25-dec-2008 - in arm, we were masking to 0x1F. 
		//this is probably safer since an invalid opcode could crash the emu
		//zero 30-jun-2009 - but they say that the ideas 0xFF should crash the device...
		//u32 swinum = cpu->instruction & 0xFF;
		swinum &= 0x1F;
		return cpu->swi_tab[swinum]() + 3;  
	}
	else {
		/* we use an irq thats not in the irq tab, as
		it was replaced due to a changed intVector */
		Status_Reg tmp = cpu->CPSR;
		armcpu_switchMode(cpu, SVC);		  /* enter svc mode */
		cpu->R[14] = cpu->next_instruction;		  /* jump to swi Vector */
		cpu->SPSR = tmp;					/* save old CPSR as new SPSR */
		cpu->CPSR.bits.T = 0;				/* handle as ARM32 code */
		cpu->CPSR.bits.I = 1;
		cpu->changeCPSR();
		cpu->R[15] = cpu->intVector + 0x08;
		cpu->next_instruction = cpu->R[15];
		return 3;
	}
}

//-----------------------------------------------------------------------------
//   Branch
//-----------------------------------------------------------------------------
#define SIGNEEXT_IMM11(i)	(((i)&0x7FF) | (BIT10(i) * 0xFFFFF800))
#define SIGNEXTEND_11(i) (((s32)i<<21)>>21)

TEMPLATE static  u32 FASTCALL OP_B_COND(const u32 i)
{
	if(!TEST_COND((i>>8)&0xF, 0, cpu->CPSR))
		return 1;
	
	cpu->R[15] += (u32)((s8)(i&0xFF))<<1;
	cpu->next_instruction = cpu->R[15];
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_B_UNCOND(const u32 i)
{
	//nocash message detection
	const u16 last = _MMU_read16<PROCNUM,MMU_AT_DEBUG>(cpu->instruct_adr-2);
	const u16 next = _MMU_read16<PROCNUM,MMU_AT_DEBUG>(cpu->instruct_adr+2);
	static const u16 mov_r12_r12 = 0x46E4;
	if(last == mov_r12_r12 && next == 0x6464)
	{
		NocashMessage(cpu,6);
	}

	cpu->R[15] += (SIGNEEXT_IMM11(i)<<1);
	cpu->next_instruction = cpu->R[15];
	return 1;
}
 
TEMPLATE static  u32 FASTCALL OP_BLX(const u32 i)
{
	cpu->R[15] = (cpu->R[14] + ((i&0x7FF)<<1))&0xFFFFFFFC;
	cpu->R[14] = cpu->next_instruction | 1;
	cpu->next_instruction = cpu->R[15];
	cpu->CPSR.bits.T = 0;
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_BL_10(const u32 i)
{
	cpu->R[14] = cpu->R[15] + (SIGNEXTEND_11(i)<<12);
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_BL_11(const u32 i)
{
	cpu->R[15] = (cpu->R[14] + ((i&0x7FF)<<1));
	cpu->R[14] = cpu->next_instruction | 1;
	cpu->next_instruction = cpu->R[15];
	return 4;
}

TEMPLATE static  u32 FASTCALL OP_BX_THUMB(const u32 i)
{
	// When using PC as operand with BX opcode, switch to ARM state and jump to (instruct_adr+4)
	// Reference: http://nocash.emubase.de/gbatek.htm#thumb5hiregisteroperationsbranchexchange

#if 0
	if (REG_POS(i, 3) == 15)
	{
		 cpu->CPSR.bits.T = 0;
		 cpu->R[15] &= 0xFFFFFFFC;
		 cpu->next_instruction = cpu->R[15];
	}
	else
	{
		u32 Rm = cpu->R[REG_POS(i, 3)];

		cpu->CPSR.bits.T = BIT0(Rm);
		cpu->R[15] = (Rm & 0xFFFFFFFE);
		cpu->next_instruction = cpu->R[15];
	}
#else
	u32 Rm = cpu->R[REG_POS(i, 3)];
	//----- ARM_REF:
	//----- Register 15 can be specified for <Rm>. If this is done, R15 is read as normal for Thumb code,
	//----- that is, it is the address of the BX instruction itself plus 4. If the BX instruction is at a
	//----- word-aligned address, this results in a branch to the next word, executing in ARM state.
	//----- However, if the BX instruction is not at a word-aligned address, this means that the results of
	//----- the instruction are UNPREDICTABLE (because the value read for R15 has bits[1:0]==0b10).
	if (Rm == 15)
	{
		//printf("THUMB%c: BX using PC as operand\n", PROCNUM?'7':'9');
		//emu_halt();
	}
	cpu->CPSR.bits.T = BIT0(Rm);
	cpu->R[15] = (Rm & (0xFFFFFFFC|(1<<cpu->CPSR.bits.T)));
	cpu->next_instruction = cpu->R[15];
#endif
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_BLX_THUMB(const u32 i)
{
	u32 Rm = cpu->R[REG_POS(i, 3)];
	cpu->CPSR.bits.T = BIT0(Rm);
	cpu->R[15] = Rm & 0xFFFFFFFE;
	cpu->R[14] = cpu->next_instruction | 1;
	cpu->next_instruction = cpu->R[15];
	
	return 4;
}

//-----------------------------------------------------------------------------
//   The End
//-----------------------------------------------------------------------------

const OpFunc thumb_instructions_set[2][1024] = {{
#define TABDECL(x) x<0>
#include "thumb_tabdef.inc"
#undef TABDECL
},{
#define TABDECL(x) x<1>
#include "thumb_tabdef.inc"
#undef TABDECL
}};

#define TABDECL(x) #x
const char* thumb_instruction_names[1024] = {
#include "thumb_tabdef.inc"
};
#undef TABDECL
