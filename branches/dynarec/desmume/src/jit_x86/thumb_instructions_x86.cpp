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

#include "../bios.h"
#include "../debug.h"
#include "../MMU.h"
#include "../NDSSystem.h"
#include "../thumb_instructions.h"
#include "jit_x86.h"
#include <assert.h>

using namespace JIT_x86;
using namespace JIT_x86::Emitter;

//#define cpu (&ARMPROC)
#define TEMPLATE template<u32 PROCNUM>
#define THUMB_INTERPRETER_INSTRUCTION_SET (PROCNUM ? thumb_instructions_set_1 : thumb_instructions_set_0)

#define REG_NUM(i, n) (((i)>>n)&0x7)


TEMPLATE u32 __fastcall x86_THUMB_FallbackToInterpreter(u32 instruction)
{
	if (PROCNUM == 0)
	{
		NDS_ARM9.instruction = instruction;
		return thumb_instructions_set_0[instruction >> 6]();
	}
	else
	{
		NDS_ARM7.instruction = instruction;
		return thumb_instructions_set_1[instruction >> 6]();
	}
}

//#define X86_THUMB_DEFAULT  MOV(ECX, instruction); CALL((void*)(TFallbackFunc)x86_THUMB_FallbackToInterpreter<PROCNUM>);
#define X86_THUMB_DEFAULT  MOV(&ARMPROC.instruction, instruction); CALL((void*)THUMB_INTERPRETER_INSTRUCTION_SET[instruction >> 6]);



TEMPLATE static  u32 FASTCALL OP_UND_THUMB(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_LSL_0(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_LSL(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_LSR_0(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_LSR(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ASR_0(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ASR(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ADD_REG(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_SUB_REG(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ADD_IMM3(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_SUB_IMM3(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_MOV_IMM8(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_CMP_IMM8(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ADD_IMM8(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_SUB_IMM8(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_AND(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_EOR(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_LSL_REG(u32 pc, u32 instruction)
{
    X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_LSR_REG(u32 pc, u32 instruction)
{
   X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ASR_REG(u32 pc, u32 instruction)
{
   X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ADC_REG(u32 pc, u32 instruction)
{
  X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_SBC_REG(u32 pc, u32 instruction)
{
  X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ROR_REG(u32 pc, u32 instruction)
{
    X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_TST(u32 pc, u32 instruction)
{
    X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_NEG(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_CMP(u32 pc, u32 instruction)
{X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_CMN(u32 pc, u32 instruction)
{
   X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ORR(u32 pc, u32 instruction)
{
    X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_MUL_REG(u32 pc, u32 instruction)
{
   X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_BIC(u32 pc, u32 instruction)
{
    X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_MVN(u32 pc, u32 instruction)
{
   X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_ADD_SPE(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT 
		 StopCodeBlock();
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_CMP_SPE(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 3;
}

TEMPLATE static  u32 FASTCALL OP_MOV_SPE(u32 pc, u32 instruction)
{
  X86_THUMB_DEFAULT 
	  StopCodeBlock();
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_BX_THUMB(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_BLX_THUMB(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_LDR_PCREL(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_STR_REG_OFF(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	return 2;
}

TEMPLATE static  u32 FASTCALL OP_STRH_REG_OFF(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	return 2;
}

TEMPLATE static  u32 FASTCALL OP_STRB_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
   X86_THUMB_DEFAULT   
     return 2;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRSB_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
    X86_THUMB_DEFAULT       
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDR_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 adr = (cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)]);
	// u32 tempValue = READ32(cpu->mem_if->data, adr&0xFFFFFFFC);

	// adr = (adr&3)*8;
	X86_THUMB_DEFAULT      
     return 3;// + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRH_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
    X86_THUMB_DEFAULT     
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRB_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
   X86_THUMB_DEFAULT         
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRSH_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 adr = cpu->R[REG_NUM(i, 3)] + cpu->R[REG_NUM(i, 6)];
   X86_THUMB_DEFAULT        
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>4)&0x7C);
   X86_THUMB_DEFAULT        
     return 2;// + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>4)&0x7C);
   //  u32 tempValue = READ32(cpu->mem_if->data, adr&0xFFFFFFFC);
	// adr = (adr&3)*8;
	// tempValue = (tempValue>>adr) | (tempValue<<(32-adr));
	X86_THUMB_DEFAULT        
     return 3;// + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STRB_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>6)&0x1F);
    X86_THUMB_DEFAULT         
     return 2;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRB_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>6)&0x1F);
     X86_THUMB_DEFAULT         
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STRH_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
  //   u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>5)&0x3E);
    X86_THUMB_DEFAULT         
     return 2;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDRH_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 adr = cpu->R[REG_NUM(i, 3)] + ((i>>5)&0x3E);
    X86_THUMB_DEFAULT        
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_STR_SPREL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 adr = cpu->R[13] + ((i&0xFF)<<2);
   X86_THUMB_DEFAULT        
     return 2;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_LDR_SPREL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 adr = cpu->R[13] + ((i&0xFF)<<2);
	X86_THUMB_DEFAULT         
     return 3;// + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static  u32 FASTCALL OP_ADD_2PC(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    X86_THUMB_DEFAULT         
     return 5;
}

TEMPLATE static  u32 FASTCALL OP_ADD_2SP(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    X86_THUMB_DEFAULT
     return 2;
}

TEMPLATE static  u32 FASTCALL OP_ADJUST_P_SP(u32 pc, u32 instruction)
{
     X86_THUMB_DEFAULT
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_ADJUST_M_SP(u32 pc, u32 instruction)
{
    X86_THUMB_DEFAULT
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_PUSH(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     u32 adr = cpu->R[13] - 4;
     u32 c = 0, j;
  X86_THUMB_DEFAULT
     return c + 3;
}

TEMPLATE static  u32 FASTCALL OP_PUSH_LR(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     u32 adr = cpu->R[13] - 4;
     u32 c = 0, j;
  X86_THUMB_DEFAULT
     return c + 4;
}

TEMPLATE static  u32 FASTCALL OP_POP(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
  //   u32 adr = cpu->R[13];
     u32 c = 0, j;
X86_THUMB_DEFAULT
     return c + 2;
}

TEMPLATE static  u32 FASTCALL OP_POP_PC(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     u32 adr = cpu->R[13];
     u32 c = 0, j;
     u32 v;
X86_THUMB_DEFAULT
	StopCodeBlock();
     return c + 5;    
}

TEMPLATE static  u32 FASTCALL OP_BKPT_THUMB(u32 pc, u32 instruction)
{
     return 1;
}

TEMPLATE static  u32 FASTCALL OP_STMIA_THUMB(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
  //   u32 adr = cpu->R[REG_NUM(i, 8)];
     u32 c = 0, j;
X86_THUMB_DEFAULT
     return c + 2;
}

TEMPLATE static  u32 FASTCALL OP_LDMIA_THUMB(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
	 u32 regIndex = REG_NUM(i, 8);
 //    u32 adr = cpu->R[regIndex];
     u32 c = 0, j;

    X86_THUMB_DEFAULT
	return c + 3;
}

TEMPLATE static  u32 FASTCALL OP_B_COND(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	//if (pc==0x2F0E) printf("cond branch\n");
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_SWI_THUMB(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 3;
}

#define SIGNEEXT_IMM11(i)     (((i)&0x7FF) | (BIT10(i) * 0xFFFFF800))

TEMPLATE static  u32 FASTCALL OP_B_UNCOND(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 3;
}
 
TEMPLATE static  u32 FASTCALL OP_BLX(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 3;
}

TEMPLATE static  u32 FASTCALL OP_BL_10(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 1;
}

TEMPLATE static  u32 FASTCALL OP_BL_THUMB(u32 pc, u32 instruction)
{
	X86_THUMB_DEFAULT
	StopCodeBlock();
	return 3;
}

#define TYPE_RETOUR 	u32
#define PARAMETRES  	u32 pc, u32 instruction
#define CALLTYPE    	FASTCALL 
#define NOM_THUMB_TAB   thumb_instructions_set_x86_arm9
#define TABDECL(x)		x<0>

#include "../thumb_tabdef.inc"

#undef TYPE_RETOUR
#undef PARAMETRES
#undef CALLTYPE
#undef NOM_THUMB_TAB
#undef TABDECL

#define TYPE_RETOUR 	u32
#define PARAMETRES  	u32 pc, u32 instruction
#define CALLTYPE    	FASTCALL 
#define NOM_THUMB_TAB   thumb_instructions_set_x86_arm7
#define TABDECL(x)		x<1>

#include "../thumb_tabdef.inc"
