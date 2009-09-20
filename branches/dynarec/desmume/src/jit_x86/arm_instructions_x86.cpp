/*  Copyright (C) 2006 yopyop
    Copyright (C) 2006 shash
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright (C) 2006-2007 shash

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

#include "../cp15.h"
#include "../debug.h"
#include "../MMU.h"
#include "../armcpu.h"
#include "../NDSSystem.h"
#include "../arm_instructions.h"
#include "jit_x86.h"

using namespace JIT_x86;
using namespace JIT_x86::Emitter;

//#define cpu (&ARMPROC)
#define TEMPLATE template<u32 PROCNUM> 
#define ARM_INTERPRETER_INSTRUCTION_SET (PROCNUM ? arm_instructions_set_1 : arm_instructions_set_0)


TEMPLATE u32 __fastcall x86_ARM_FallbackToInterpreter(u32 instruction)
{
	if (PROCNUM == 0)
	{
		NDS_ARM9.instruction = instruction;
		return arm_instructions_set_0[INSTRUCTION_INDEX(instruction)]();
	}
	else
	{
		NDS_ARM7.instruction = instruction;
		return arm_instructions_set_1[INSTRUCTION_INDEX(instruction)]();
	}
}

//#define X86_ARM_DEFAULT  MOV(ECX, instruction); CALL((void*)(TFallbackFunc)x86_ARM_FallbackToInterpreter<PROCNUM>);
#define X86_ARM_DEFAULT  MOV(&ARMPROC.instruction, instruction); CALL((void*)ARM_INTERPRETER_INSTRUCTION_SET[INSTRUCTION_INDEX(instruction)]);



#define LSL_IMM //shift_op = cpu->R[REG_POS(i,0)]<<((i>>7)&0x1F);

#define S_LSL_IMM /*u32 shift_op = ((i>>7)&0x1F);\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
          shift_op=cpu->R[REG_POS(i,0)];\
     else\
     {\
         c = BIT_N(cpu->R[REG_POS(i,0)], 32-shift_op);\
         shift_op = cpu->R[REG_POS(i,0)]<<((i>>7)&0x1F);\
     }*/
     
#define LSL_REG/* u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if(shift_op>=32)\
             shift_op=0;\
     else\
             shift_op=cpu->R[REG_POS(i,0)]<<shift_op;*/

#define S_LSL_REG/* u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
             if(shift_op<32)\
             {\
                  c = BIT_N(cpu->R[REG_POS(i,0)], 32-shift_op);\
                  shift_op = cpu->R[REG_POS(i,0)]<<shift_op;\
             }\
             else\
                 if(shift_op==32)\
                 {\
                      shift_op = 0;\
                      c = BIT0(cpu->R[REG_POS(i,0)]);\
                 }\
                 else\
                 {\
                      shift_op = 0;\
                      c = 0;\
                 } */                

#define LSR_IMM /*     shift_op = ((i>>7)&0x1F);\
     if(shift_op!=0)\
            shift_op = cpu->R[REG_POS(i,0)]>>shift_op;*/

#define S_LSR_IMM  /*   u32 shift_op = ((i>>7)&0x1F);\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
     {\
          c = BIT31(cpu->R[REG_POS(i,0)]);\
     }\
     else\
     {\
         c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
         shift_op = cpu->R[REG_POS(i,0)]>>shift_op;\
     }
*/
#define LSR_REG/*    u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if(shift_op>=32)\
             shift_op = 0;\
     else\
             shift_op = cpu->R[REG_POS(i,0)]>>shift_op;*/

#define S_LSR_REG/*      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
     {\
             shift_op = cpu->R[REG_POS(i,0)];\
     }\
     else\
             if(shift_op<32)\
             {\
                  c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
                  shift_op = cpu->R[REG_POS(i,0)]>>shift_op;\
             }\
             else\
                  if(shift_op==32)\
                  {\
                       c = BIT31(cpu->R[REG_POS(i,0)]);\
                       shift_op = 0;\
                  }\
                  else\
                  {\
                       c = 0;\
                       shift_op = 0;\
                  }*/

#define ASR_IMM   /*   shift_op = ((i>>7)&0x1F);\
     if(shift_op==0)\
            shift_op=BIT31(cpu->R[REG_POS(i,0)])*0xFFFFFFFF;\
     else\
            shift_op = (u32)(((s32)(cpu->R[REG_POS(i,0)]))>>shift_op);
*/
#define S_ASR_IMM  /*   u32 shift_op = ((i>>7)&0x1F);\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
     {\
            shift_op=BIT31(cpu->R[REG_POS(i,0)])*0xFFFFFFFF;\
            c = BIT31(cpu->R[REG_POS(i,0)]);\
     }\
     else\
     {\
            c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
            shift_op = (u32)(((s32)(cpu->R[REG_POS(i,0)]))>>shift_op);\
     }
*/
#define ASR_REG /*   u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if(shift_op==0)\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
             if(shift_op<32)\
                     shift_op = (u32)(((s32)(cpu->R[REG_POS(i,0)]))>>shift_op);\
             else\
                     shift_op=BIT31(cpu->R[REG_POS(i,0)])*0xFFFFFFFF;
                     */
#define S_ASR_REG  /*   u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
             if(shift_op<32)\
             {\
                     c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
                     shift_op = (u32)(((s32)(cpu->R[REG_POS(i,0)]))>>shift_op);\
             }\
             else\
             {\
                     c = BIT31(cpu->R[REG_POS(i,0)]);\
                     shift_op=BIT31(cpu->R[REG_POS(i,0)])*0xFFFFFFFF;\
             }
                 */    
#define ROR_IMM   /*   shift_op = ((i>>7)&0x1F);\
     if(shift_op==0)\
     {\
             u32 tmp = cpu->CPSR.bits.C;\
             shift_op = (tmp<<31)|(cpu->R[REG_POS(i,0)]>>1);\
     }\
     else\
             shift_op = ROR(cpu->R[REG_POS(i,0)],shift_op);
             */
#define S_ROR_IMM  /*   u32 shift_op = ((i>>7)&0x1F);\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
     {\
             u32 tmp = cpu->CPSR.bits.C;\
             shift_op = (tmp<<31)|(cpu->R[REG_POS(i,0)]>>1);\
             c = BIT0(cpu->R[REG_POS(i,0)]);\
     }\
     else\
     {\
             c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
             shift_op = ROR(cpu->R[REG_POS(i,0)],shift_op);\
     }
             */
#define ROR_REG   /*  u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if((shift_op==0)||((shift_op&0x1F)==0))\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
             shift_op = ROR(cpu->R[REG_POS(i,0)],(shift_op&0x1F));
*/
#define S_ROR_REG   /*   u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
     {\
             shift_op&=0x1F;\
             if(shift_op==0)\
             {\
                  shift_op=cpu->R[REG_POS(i,0)];\
                  c = BIT31(cpu->R[REG_POS(i,0)]);\
             }\
             else\
             {\
                 c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
                  shift_op = ROR(cpu->R[REG_POS(i,0)],(shift_op&0x1F));\
             }\
     }
*/
#define IMM_VALUE //     u32 shift_op = ROR((i&0xFF), (i>>7)&0x1E);

#define S_IMM_VALUE /*    u32 shift_op = ROR((i&0xFF), (i>>7)&0x1E);\
     u32 c = cpu->CPSR.bits.C;\
        if((i>>8)&0xF)\
             c = BIT31(shift_op);
*/
#define IMM_OFF (((i>>4)&0xF0)+(i&0xF))

#define IMM_OFF_12 ((i)&0xFFF)

TEMPLATE static u32 FASTCALL  OP_UND(u32 pc, u32 instruction)
{
	X86_ARM_DEFAULT
	return 1;
}
 
#define TRAPUNDEF() \
 	LOG("Undefined instruction: %#08X PC = %#08X\n", instruction, cpu->instruct_adr);	\
	X86_ARM_DEFAULT \
	return 4;
 												\
 	/*if (((cpu->intVector != 0) ^ (PROCNUM == ARMCPU_ARM9))){				\
 		X86_ARM_DEFAULT\
 		return 4;									\
 	}											\
 	else											\
 	{											\
 		X86_ARM_DEFAULT							\
 		return 4;									\
 	}											\*/

//-----------------------AND------------------------------------

#define OP_AND(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT \
		  StopCodeBlock();\
          return b;\
     }\
	 X86_ARM_DEFAULT \
     return a;

#define OP_ANDS(a, b)\
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT \
		  StopCodeBlock();\
          return b;\
     }\
     X86_ARM_DEFAULT \
     return a;
 
TEMPLATE static u32 FASTCALL  OP_AND_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_AND(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_AND_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_AND(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_AND(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_AND_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_AND(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_AND(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_AND_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_AND(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_AND(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_AND_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_AND(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_AND(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OP_ANDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OP_ANDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OP_ANDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;             
     OP_ANDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OP_ANDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;                     
     OP_ANDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OP_ANDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OP_ANDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_AND_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OP_ANDS(2, 4);
}

//--------------EOR------------------------------

#define OP_EOR(a, b)   \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock();\
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a; 
       
#define OP_EORS(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock();\
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;

TEMPLATE static u32 FASTCALL  OP_EOR_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_EOR(1, 3); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_EOR(2, 4); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_EOR(1, 3); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_EOR(2, 4); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_EOR(1, 3); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_EOR(2, 4); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_EOR(1, 3); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_EOR(2, 4); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_EOR(1, 3); 
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OP_EORS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OP_EORS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OP_EORS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;            
     OP_EORS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OP_EORS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;                     
     OP_EORS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OP_EORS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OP_EORS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_EOR_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OP_EORS(2, 4);
}

//-------------SUB-------------------------------------

#define OP_SUB(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;

#define OPSUBS(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;

TEMPLATE static u32 FASTCALL  OP_SUB_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_SUB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SUB_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_SUB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_SUB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SUB_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_SUB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_SUB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SUB_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_SUB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_SUB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SUB_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_SUB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_SUB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSL_IMM;
     OPSUBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSL_REG;
     OPSUBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSR_IMM;            
     OPSUBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSR_REG;             
     OPSUBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ASR_IMM;
     OPSUBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ASR_REG;                     
     OPSUBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ROR_IMM;
     OPSUBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ROR_REG;
     OPSUBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SUB_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     IMM_VALUE;
     OPSUBS(2, 4);
}

//------------------RSB------------------------

#define OP_RSB(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;

#define OP_RSBS(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		   StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;
     
TEMPLATE static u32 FASTCALL  OP_RSB_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_RSB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSB_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_RSB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_RSB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSB_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_RSB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_RSB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSB_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_RSB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_RSB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSB_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_RSB(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_RSB(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSL_IMM;
     OP_RSBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSL_REG;
     OP_RSBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSR_IMM;            
     OP_RSBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSR_REG;             
     OP_RSBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ASR_IMM;
     OP_RSBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ASR_REG;                     
     OP_RSBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ROR_IMM;
     OP_RSBS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ROR_REG;
     OP_RSBS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_RSB_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     IMM_VALUE;
     OP_RSBS(2, 4);
}

//------------------ADD-----------------------------------

#define OP_ADD(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		   StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;

TEMPLATE static u32 FASTCALL  OP_ADD_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_ADD(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADD_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_ADD(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_ADD(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADD_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_ADD(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_ADD(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADD_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_ADD(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_ADD(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADD_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_ADD(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_ADD(1, 3);
}

#define OP_ADDS(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		   StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;
     
TEMPLATE static u32 FASTCALL  OP_ADD_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSL_IMM;
     OP_ADDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSL_REG;
     OP_ADDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSR_IMM;            
     OP_ADDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSR_REG;             
     OP_ADDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ASR_IMM;
     OP_ADDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ASR_REG;                     
     OP_ADDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ROR_IMM;
     OP_ADDS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ROR_REG;
     OP_ADDS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADD_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     IMM_VALUE;
     OP_ADDS(2, 4);
}

//------------------ADC-----------------------------------

#define OP_ADC(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;
     
TEMPLATE static u32 FASTCALL  OP_ADC_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_ADC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADC_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_ADC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_ADC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADC_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_ADC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_ADC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADC_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_ADC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_ADC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ADC_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_ADC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_ADC(1, 3);
}

#define OP_ADCS(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a; \
     
TEMPLATE static u32 FASTCALL  OP_ADC_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSL_IMM;
     OP_ADCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSL_REG;
     OP_ADCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSR_IMM;            
     OP_ADCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSR_REG;             
     OP_ADCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ASR_IMM;
     OP_ADCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ASR_REG;                     
     OP_ADCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ROR_IMM;
     OP_ADCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ROR_REG;
     OP_ADCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_ADC_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     IMM_VALUE;
     OP_ADCS(2, 4);
}

//-------------SBC-------------------------------------

#define OP_SBC(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT \
     return a;
     
TEMPLATE static u32 FASTCALL  OP_SBC_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_SBC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SBC_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_SBC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_SBC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SBC_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_SBC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_SBC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SBC_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_SBC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_SBC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_SBC_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_SBC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_SBC(1, 3);
}

//zero 14-feb-2009 - reverting flag logic to fix zoning bug in ff4
#define OP_SBCS(a, b) \
     { \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a; \
     }


TEMPLATE static u32 FASTCALL  OP_SBC_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSL_IMM;
     OP_SBCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSL_REG;
     OP_SBCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSR_IMM;            
     OP_SBCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSR_REG;             
     OP_SBCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ASR_IMM;
     OP_SBCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ASR_REG;                     
     OP_SBCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ROR_IMM;
     OP_SBCS(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ROR_REG;
     OP_SBCS(3, 5);
}

TEMPLATE static u32 FASTCALL  OP_SBC_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     IMM_VALUE;
     OP_SBCS(2, 4);
}

//---------------RSC----------------------------------

#define OP_RSC(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;
     
TEMPLATE static u32 FASTCALL  OP_RSC_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_RSC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSC_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_RSC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_RSC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSC_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_RSC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_RSC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSC_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_RSC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_RSC(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_RSC_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_RSC(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_RSC(1, 3);
}

//zero 14-feb-2009 - reverting flag logic to fix zoning bug in ff4
#define OP_RSCS(a,b) \
     { \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
          }\
     X86_ARM_DEFAULT\
     return a; \
     }

TEMPLATE static u32 FASTCALL  OP_RSC_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSL_IMM;
     OP_RSCS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSL_REG;
     OP_RSCS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     LSR_IMM;            
     OP_RSCS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     LSR_REG;             
     OP_RSCS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ASR_IMM;
     OP_RSCS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ASR_REG;                     
     OP_RSCS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     u32 shift_op;
     ROR_IMM;
     OP_RSCS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     ROR_REG;
     OP_RSCS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_RSC_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     IMM_VALUE;
     OP_RSCS(2,4);
}

//-------------------TST----------------------------

#define OP_TST(a) \
     { \
     X86_ARM_DEFAULT\
     return a; \
     }
     
TEMPLATE static u32 FASTCALL  OP_TST_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OP_TST(1);
}

TEMPLATE static u32 FASTCALL  OP_TST_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OP_TST(2);
}

TEMPLATE static u32 FASTCALL  OP_TST_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OP_TST(1);
}

TEMPLATE static u32 FASTCALL  OP_TST_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;             
     OP_TST(2);
}

TEMPLATE static u32 FASTCALL  OP_TST_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OP_TST(1);
}

TEMPLATE static u32 FASTCALL  OP_TST_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;                     
     OP_TST(2);
}

TEMPLATE static u32 FASTCALL  OP_TST_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OP_TST(1);
}

TEMPLATE static u32 FASTCALL  OP_TST_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OP_TST(2);
}

TEMPLATE static u32 FASTCALL  OP_TST_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OP_TST(1);
}

//-------------------TEQ----------------------------

#define OP_TEQ(a) \
     { \
     X86_ARM_DEFAULT\
     return a; \
     }
     
TEMPLATE static u32 FASTCALL  OP_TEQ_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OP_TEQ(1);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OP_TEQ(2);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OP_TEQ(1);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;             
     OP_TEQ(2);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OP_TEQ(1);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;                     
     OP_TEQ(2);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OP_TEQ(1);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OP_TEQ(2);
}

TEMPLATE static u32 FASTCALL  OP_TEQ_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OP_TEQ(1);
}

//-------------CMP-------------------------------------

#define OP_CMP(a) \
     { \
     X86_ARM_DEFAULT\
     return a; \
     }
     
TEMPLATE static u32 FASTCALL  OP_CMP_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_CMP(1);
}

TEMPLATE static u32 FASTCALL  OP_CMP_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_CMP(2);
}

TEMPLATE static u32 FASTCALL  OP_CMP_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_CMP(1);
}

TEMPLATE static u32 FASTCALL  OP_CMP_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_CMP(2);
}

TEMPLATE static u32 FASTCALL  OP_CMP_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_CMP(1);
}

TEMPLATE static u32 FASTCALL  OP_CMP_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_CMP(2);
}

TEMPLATE static u32 FASTCALL  OP_CMP_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_CMP(1);
}

TEMPLATE static u32 FASTCALL  OP_CMP_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_CMP(2);
}

TEMPLATE static u32 FASTCALL  OP_CMP_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_CMP(1);
}

//---------------CMN---------------------------

#define OP_CMN(a) \
     { \
     X86_ARM_DEFAULT\
     return a; \
     }

TEMPLATE static u32 FASTCALL  OP_CMN_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_CMN(1);
}

TEMPLATE static u32 FASTCALL  OP_CMN_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_CMN(2);
}

TEMPLATE static u32 FASTCALL  OP_CMN_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_CMN(1);
}

TEMPLATE static u32 FASTCALL  OP_CMN_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_CMN(2);
}

TEMPLATE static u32 FASTCALL  OP_CMN_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_CMN(1);
}

TEMPLATE static u32 FASTCALL  OP_CMN_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_CMN(2);
}

TEMPLATE static u32 FASTCALL  OP_CMN_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_CMN(1);
}

TEMPLATE static u32 FASTCALL  OP_CMN_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_CMN(2);
}

TEMPLATE static u32 FASTCALL  OP_CMN_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_CMN(1);
}

//------------------ORR-------------------

#define OP_ORR(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;

TEMPLATE static u32 FASTCALL  OP_ORR_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_ORR(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ORR_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OP_ORR(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_ORR(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ORR_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OP_ORR(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_ORR(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ORR_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_ORR(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_ORR(1, 3);
}

TEMPLATE static u32 FASTCALL  OP_ORR_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_ORR(2, 4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_ORR(1, 3);
}

#define OP_ORRS(a,b) \
     { \
     if(REG_POS(i,12)==15) \
     { \
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b; \
     } \
     X86_ARM_DEFAULT\
     return a; \
     }

TEMPLATE static u32 FASTCALL  OP_ORR_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OP_ORRS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OP_ORRS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OP_ORRS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;             
     OP_ORRS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OP_ORRS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;                     
     OP_ORRS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OP_ORRS(2,4);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OP_ORRS(3,5);
}

TEMPLATE static u32 FASTCALL  OP_ORR_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OP_ORRS(2,4);
}

//------------------MOV-------------------

#define OP_MOV(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;
     
#define OP_MOV_S(a, b) \
     if(BIT20(i) && REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;\

TEMPLATE static u32 FASTCALL  OP_MOV_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OP_MOV(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MOV_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
//	 if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_MOV(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MOV_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;  
	// if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OP_MOV(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MOV_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OP_MOV(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OP_MOV(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OP_MOV(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OP_MOV(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OP_MOV_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
	// if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;   
     OP_MOV_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;  
	// if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV_S(3,5);           
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OP_MOV_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;  
     OP_MOV_S(3,5);                   
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OP_MOV_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OP_MOV_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_MOV_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OP_MOV_S(2,4);
}

//------------------BIC-------------------
#define OPP_BIC(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;
     
#define OPP_BIC_S(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;
     
TEMPLATE static u32 FASTCALL  OP_BIC_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OPP_BIC(1,3);
}

TEMPLATE static u32 FASTCALL  OP_BIC_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OPP_BIC(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OPP_BIC(1,3);
}

TEMPLATE static u32 FASTCALL  OP_BIC_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;             
     OPP_BIC(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OPP_BIC(1,3);
}

TEMPLATE static u32 FASTCALL  OP_BIC_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;                     
     OPP_BIC(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OPP_BIC(1,3);
}

TEMPLATE static u32 FASTCALL  OP_BIC_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OPP_BIC(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OPP_BIC(1,3);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OPP_BIC_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OPP_BIC_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OPP_BIC_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;
     OPP_BIC_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OPP_BIC_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;
     OPP_BIC_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OPP_BIC_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OPP_BIC_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_BIC_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OPP_BIC_S(2,4);
}

//------------------MVN-------------------
#define OPP_MVN(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
	 X86_ARM_DEFAULT\
     return a;
     
#define OPP_MVN_S(a, b) \
     if(REG_POS(i,12)==15)\
     {\
          X86_ARM_DEFAULT\
		  StopCodeBlock(); \
          return b;\
     }\
     X86_ARM_DEFAULT\
     return a;
     
TEMPLATE static u32 FASTCALL  OP_MVN_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSL_IMM;
     OPP_MVN(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MVN_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSL_REG;
     OPP_MVN(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     LSR_IMM;            
     OPP_MVN(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MVN_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     LSR_REG;
     OPP_MVN(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ASR_IMM;
     OPP_MVN(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MVN_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ASR_REG;
     OPP_MVN(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     u32 shift_op;
     ROR_IMM;
     OPP_MVN(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MVN_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     ROR_REG;
     OPP_MVN(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     IMM_VALUE;
     OPP_MVN(1,3);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_LSL_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_IMM;
     OPP_MVN_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_LSL_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSL_REG;
     OPP_MVN_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_LSR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_IMM;            
     OPP_MVN_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_LSR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_LSR_REG;
     OPP_MVN_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_ASR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_IMM;
     OPP_MVN_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_ASR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ASR_REG;
     OPP_MVN_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_ROR_IMM(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_IMM;
     OPP_MVN_S(2,4);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_ROR_REG(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_ROR_REG;
     OPP_MVN_S(3,5);
}

TEMPLATE static u32 FASTCALL  OP_MVN_S_IMM_VAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     S_IMM_VALUE;
     OPP_MVN_S(2,4);
}

//-------------MUL------------------------
#define OPP_M(a,b) X86_ARM_DEFAULT \
	return 6;

TEMPLATE static u32 FASTCALL  OP_MUL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     u32 v = cpu->R[REG_POS(i,0)];

     OPP_M(5,2);
}

TEMPLATE static u32 FASTCALL  OP_MLA(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 v = cpu->R[REG_POS(i,0)];
     
     OPP_M(6,3);
}

TEMPLATE static u32 FASTCALL  OP_MUL_S(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 v = cpu->R[REG_POS(i,0)];
     
     OPP_M(6,3);
}

TEMPLATE static u32 FASTCALL  OP_MLA_S(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 v = cpu->R[REG_POS(i,0)];

     OPP_M(7,4);
}

//----------UMUL--------------------------

TEMPLATE static u32 FASTCALL  OP_UMULL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 v = cpu->R[REG_POS(i,0)];
       
     OPP_M(6,3);
}

TEMPLATE static u32 FASTCALL  OP_UMLAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 v = cpu->R[REG_POS(i,0)];  
     
     OPP_M(7,4);
}

TEMPLATE static u32 FASTCALL  OP_UMULL_S(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // u32 v = cpu->R[REG_POS(i,0)];

     OPP_M(7,4);
}

TEMPLATE static u32 FASTCALL  OP_UMLAL_S(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  u32 v = cpu->R[REG_POS(i,0)];
     
     OPP_M(8,5);
}

//----------SMUL--------------------------

TEMPLATE static u32 FASTCALL  OP_SMULL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  s64 v = (s32)cpu->R[REG_POS(i,0)];
          
     OPP_M(6,3);
}

TEMPLATE static u32 FASTCALL  OP_SMLAL(u32 pc, u32 instruction)
{
     const u32 &i = instruction;   
    // s64 v = (s32)cpu->R[REG_POS(i,0)];
          
     OPP_M(7,4);
}

TEMPLATE static u32 FASTCALL  OP_SMULL_S(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //s64 v = (s32)cpu->R[REG_POS(i,0)];
          
     OPP_M(7,4);
}

TEMPLATE static u32 FASTCALL  OP_SMLAL_S(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
    // s64 v = (s32)cpu->R[REG_POS(i,0)];
          
     OPP_M(8,5);
}

//---------------SWP------------------------------

TEMPLATE static u32 FASTCALL  OP_SWP(u32 pc, u32 instruction)
{
     u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];

	 X86_ARM_DEFAULT
     
     return 4;// + MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF]*2;
}

TEMPLATE static u32 FASTCALL  OP_SWPB(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
     X86_ARM_DEFAULT

     return 4;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF]*2;
}

//------------LDRH-----------------------------

TEMPLATE static u32 FASTCALL  OP_LDRH_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
     X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
     X86_ARM_DEFAULT
     
    return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_PRE_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
	X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_PRE_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
X86_ARM_DEFAULT
     
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_PRE_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_PRE_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
	X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_POS_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_POS_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_POS_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRH_POS_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
 X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

//------------STRH-----------------------------

TEMPLATE static u32 FASTCALL  OP_STRH_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
  X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
  X86_ARM_DEFAULT

     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
  X86_ARM_DEFAULT

     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
X86_ARM_DEFAULT

     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_PRE_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
	X86_ARM_DEFAULT

     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_PRE_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
  X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_PRE_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
   X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_PRE_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
   X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_POS_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_POS_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_POS_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRH_POS_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

//----------------LDRSH--------------------------

TEMPLATE static u32 FASTCALL  OP_LDRSH_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
     X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
 X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_PRE_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
    X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_PRE_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_PRE_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
    X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_PRE_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
  X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_POS_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_POS_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_POS_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     //u32 //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSH_POS_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

//----------------------LDRSB----------------------

TEMPLATE static u32 FASTCALL  OP_LDRSB_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
   //  //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
     X86_ARM_DEFAULT
     
     return 3;// + MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
    X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
    X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_PRE_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF;
    X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_PRE_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF;
  X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_PRE_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_PRE_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_POS_INDE_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT

     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_POS_INDE_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_POS_INDE_P_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRSB_POS_INDE_M_REG_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

//--------------MRS--------------------------------

TEMPLATE static u32 FASTCALL  OP_MRS_CPSR(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
     
     return 1;
}

TEMPLATE static u32 FASTCALL  OP_MRS_SPSR(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
     return 1;
}

//--------------MSR--------------------------------

TEMPLATE static u32 FASTCALL  OP_MSR_CPSR(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 1;
}

TEMPLATE static u32 FASTCALL  OP_MSR_SPSR(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 1;
}

TEMPLATE static u32 FASTCALL  OP_MSR_CPSR_IMM_VAL(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 1;
}

TEMPLATE static u32 FASTCALL  OP_MSR_SPSR_IMM_VAL(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 1;
}

//-----------------BRANCH--------------------------

TEMPLATE static u32 FASTCALL  OP_BX(u32 pc, u32 instruction)
{
	X86_ARM_DEFAULT
	StopCodeBlock();
	//if (REG_POS(instruction, 0) == 12) printf("BX R12 (~%08X)\n", cpu->instruct_adr);
	//if (REG_POS(instruction, 0) == 3) printf("BX R3 (~%08X)\n", cpu->instruct_adr);
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_BLX_REG(u32 pc, u32 instruction)
{
	X86_ARM_DEFAULT
	StopCodeBlock();
	return 3;
}

#define SIGNEXTEND_24(i) (((s32)((i)<<8))>>8)

TEMPLATE static u32 FASTCALL  OP_B(u32 pc, u32 instruction)
{
	X86_ARM_DEFAULT
	StopCodeBlock();
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_BL(u32 pc, u32 instruction)
{
	X86_ARM_DEFAULT
	StopCodeBlock();
	return 3;
}

//----------------CLZ-------------------------------

/*u8 CLZ_TAB[16]=
{
     0,                     // 0000
     1,                     // 0001
     2, 2,                  // 001X
     3, 3, 3, 3,            // 01XX
     4, 4, 4, 4, 4, 4, 4, 4 // 1XXX
};*/
     
TEMPLATE static u32 FASTCALL  OP_CLZ(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
//     u32 Rm = cpu->R[REG_POS(i,0)];
     u32 pos;
     
   /*  if(Rm==0)
     {
          X86_ARM_DEFAULT
          return 2;
     }*/
     
     X86_ARM_DEFAULT
     
     return 2;
}

//--------------------QADD--QSUB------------------------------

TEMPLATE static u32 FASTCALL  OP_QADD(u32 pc, u32 instruction)
{
	if(REG_POS(instruction,12)==15)
	{
        X86_ARM_DEFAULT
			StopCodeBlock();
	     return 3;
    }
X86_ARM_DEFAULT
    return 2;
}

TEMPLATE static u32 FASTCALL  OP_QSUB(u32 pc, u32 instruction)
{
	if(REG_POS(instruction,12)==15)
	{
        X86_ARM_DEFAULT
			StopCodeBlock();
	     return 3;
    }
X86_ARM_DEFAULT
    return 2;
}

TEMPLATE static u32 FASTCALL  OP_QDADD(u32 pc, u32 instruction)
{
	if(REG_POS(instruction,12)==15)
	{
        X86_ARM_DEFAULT
			StopCodeBlock();
	     return 3;
    }
X86_ARM_DEFAULT
    return 2;
}

TEMPLATE static u32 FASTCALL  OP_QDSUB(u32 pc, u32 instruction)
{
	if(REG_POS(instruction,12)==15)
	{
        X86_ARM_DEFAULT
			StopCodeBlock();
	     return 3;
    }
X86_ARM_DEFAULT
    return 2;
}

//-----------------SMUL-------------------------------

#define HWORD(i)   ((s32)(((s32)(i))>>16))
#define LWORD(i)   (s32)(((s32)((i)<<16))>>16)

TEMPLATE static u32 FASTCALL  OP_SMUL_B_B(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMUL_B_T(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMUL_T_B(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMUL_T_T(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
     return 2;
}

//-----------SMLA----------------------------

TEMPLATE static u32 FASTCALL  OP_SMLA_B_B(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLA_B_T(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLA_T_B(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLA_T_T(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
     return 2;
}

//--------------SMLAL---------------------------------------

TEMPLATE static u32 FASTCALL  OP_SMLAL_B_B(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLAL_B_T(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLAL_T_B(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLAL_T_T(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 2;
}

//--------------SMULW--------------------

TEMPLATE static u32 FASTCALL  OP_SMULW_B(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMULW_T(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
     return 2;
}

//--------------SMLAW-------------------
TEMPLATE static u32 FASTCALL  OP_SMLAW_B(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
     return 2;
}

TEMPLATE static u32 FASTCALL  OP_SMLAW_T(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
     return 2;
}

//------------LDR---------------------------

TEMPLATE static u32 FASTCALL  OP_LDR_P_IMM_OFF(u32 pc, u32 instruction)
{
	const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     X86_ARM_DEFAULT
	/*MOV(EBX, (u32*)&(cpu?NDS_ARM7:NDS_ARM9).R[REG_POS(i,16)];
	ADD(EBX, (instruction & 0xFFF));
	PUSH(EBX);
	PUSH(cpu);
	CALL((void*)MMU_read32);*/
	if (REG_POS(i,12) == 15) StopCodeBlock();
	return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     X86_ARM_DEFAULT
	/* PUSH(adr);
	 PUSH(cpu->proc_ID);
	 CALL((void*)MMU_read32);*/
	 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
		 if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
X86_ARM_DEFAULT
	if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
	if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

//------------------------------------------------------------
TEMPLATE static u32 FASTCALL  OP_LDR_P_IMM_OFF_POSTIND2(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
	if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

//------------------------------------------------------------

TEMPLATE static u32 FASTCALL  OP_LDR_M_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
	if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
	if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
	if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
	  if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
	  if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
	   if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
	   if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_P_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
		if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDR_M_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
	   if (REG_POS(i,12) == 15) StopCodeBlock();
     return 3;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

//-----------------LDRB-------------------------------------------

TEMPLATE static u32 FASTCALL  OP_LDRB_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
 X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
     X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
 X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
 X86_ARM_DEFAULT
     
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
 X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_P_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_LDRB_M_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 3;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

//----------------------STR--------------------------------

TEMPLATE static u32 FASTCALL  OP_STR_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
  X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
 X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_P_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STR_M_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT32[PROCNUM][(adr>>24)&0xF];
}

//-----------------------STRB-------------------------------------

TEMPLATE static u32 FASTCALL  OP_STRB_P_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
  X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_LSL_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_LSR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
     X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_ASR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_ROR_IMM_OFF(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] + IMM_OFF_12;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)] - IMM_OFF_12;
  X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_LSL_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
  X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_LSR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
 X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_ASR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] + shift_op;
X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_ROR_IMM_OFF_PREIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)] - shift_op;
X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSL_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
  X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     LSR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ASR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
   X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_P_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

TEMPLATE static u32 FASTCALL  OP_STRB_M_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     const u32 &i = instruction;
     //u32 adr;
     u32 shift_op;
     ROR_IMM; 
     //adr =  cpu->R[REG_POS(i,16)];
    X86_ARM_DEFAULT
     return 2;// +MMU.MMU_WAIT16[PROCNUM][(adr>>24)&0xF];
}

//-----------------------LDRBT-------------------------------------

TEMPLATE static u32 FASTCALL  OP_LDRBT_P_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_M_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_P_REG_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_P_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_M_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_P_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_M_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_P_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_M_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_P_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL  OP_LDRBT_M_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 3;
}

//----------------------STRBT----------------------------

TEMPLATE static u32 FASTCALL  OP_STRBT_P_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_M_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_P_REG_OFF_POSTIND(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_M_REG_OFF_POSTIND(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_P_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_M_LSL_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_P_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_M_LSR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_P_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_M_ASR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_P_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	return 2;
}

TEMPLATE static u32 FASTCALL  OP_STRBT_M_ROR_IMM_OFF_POSTIND(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
	return 2;
}

//---------------------LDM-----------------------------

#define OP_L_IA(reg, adr)  if(BIT##reg(i))\
     {\
          registres[reg] = READ32(cpu->mem_if->data, start);\
          c += waitState[(start>>24)&0xF];\
          adr += 4;\
     }

#define OP_L_IB(reg, adr)  if(BIT##reg(i))\
     {\
          adr += 4;\
          registres[reg] = READ32(cpu->mem_if->data, start);\
          c += waitState[(start>>24)&0xF];\
     }

#define OP_L_DA(reg, adr)  if(BIT##reg(i))\
     {\
          registres[reg] = READ32(cpu->mem_if->data, start);\
          c += waitState[(start>>24)&0xF];\
          adr -= 4;\
     }

#define OP_L_DB(reg, adr)  if(BIT##reg(i))\
     {\
          adr -= 4;\
          registres[reg] = READ32(cpu->mem_if->data, start);\
          c += waitState[(start>>24)&0xF];\
     }

TEMPLATE static u32 FASTCALL  OP_LDMIA(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIB(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
		if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDA(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	   if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDB(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
		if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIA_W(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	   if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIB_W(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	   if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDA_W(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
	if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDB_W(u32 pc, u32 instruction)
{
X86_ARM_DEFAULT
	if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIA2(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
		if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIB2(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
	 if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDA2(u32 pc, u32 instruction)
{
  X86_ARM_DEFAULT
	  if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDB2(u32 pc, u32 instruction)
{
 X86_ARM_DEFAULT
	 if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIA2_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
		if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMIB2_W(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	   if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDA2_W(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	   if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

TEMPLATE static u32 FASTCALL  OP_LDMDB2_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
		if (instruction & 0x8000) StopCodeBlock();
	return 5;
}

//------------------------------STM----------------------------------

TEMPLATE static u32 FASTCALL  OP_STMIA(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIB(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDA(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDB(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIA_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIB_W(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDA_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDB_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIA2(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIB2(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDA2(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDB2(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIA2_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMIB2_W(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDA2_W(u32 pc, u32 instruction)
{
   X86_ARM_DEFAULT
	return 4;
}

TEMPLATE static u32 FASTCALL  OP_STMDB2_W(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	return 4;
}

/*
 *
 * The Enhanced DSP Extension LDRD and STRD instructions.
 *
 */
TEMPLATE static u32 FASTCALL
OP_LDRD_STRD_POST_INDEX(u32 pc, u32 instruction) {

  X86_ARM_DEFAULT
	return 3;
}

TEMPLATE static u32 FASTCALL
OP_LDRD_STRD_OFFSET_PRE_INDEX(u32 pc, u32 instruction) {
 
  X86_ARM_DEFAULT
	return 3;
}



//---------------------STC----------------------------------
/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */

TEMPLATE static u32 FASTCALL  OP_STC_P_IMM_OFF(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_STC_M_IMM_OFF(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_STC_P_PREIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_STC_M_PREIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_STC_P_POSTIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_STC_M_POSTIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_STC_OPTION(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

//---------------------LDC----------------------------------
/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */

TEMPLATE static u32 FASTCALL  OP_LDC_P_IMM_OFF(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_LDC_M_IMM_OFF(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_LDC_P_PREIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_LDC_M_PREIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_LDC_P_POSTIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_LDC_M_POSTIND(u32 pc, u32 instruction)
{
          TRAPUNDEF();
}

TEMPLATE static u32 FASTCALL  OP_LDC_OPTION(u32 pc, u32 instruction)
{
          TRAPUNDEF();
          return 2;
}

//----------------MCR-----------------------

TEMPLATE static u32 FASTCALL  OP_MCR(u32 pc, u32 instruction)
{
     X86_ARM_DEFAULT
	StopCodeBlock();
     return 2;
}

//----------------MRC-----------------------

TEMPLATE static u32 FASTCALL  OP_MRC(u32 pc, u32 instruction)
{
    X86_ARM_DEFAULT
		StopCodeBlock();
     return 4;
}

//--------------SWI-------------------------------
TEMPLATE static u32 FASTCALL  OP_SWI(u32 pc, u32 instruction)
{
	X86_ARM_DEFAULT
	StopCodeBlock();
	return 4;
}

//----------------BKPT-------------------------
TEMPLATE static u32 FASTCALL OP_BKPT(u32 pc, u32 instruction)
{
	/*LOG("Stopped (OP_BKPT)\n");
	TRAPUNDEF();*/
	return 4;
}

//----------------CDP-----------------------

TEMPLATE static u32 FASTCALL  OP_CDP(u32 pc, u32 instruction)
{
	LOG("Stopped (OP_CDP)\n");
	TRAPUNDEF();
}

#define TYPE_RETOUR 	u32
#define PARAMETRES  	u32 pc, u32 instruction
#define CALLTYPE    	FASTCALL 
#define NOM_TAB     	arm_instructions_set_x86_arm9
#define TABDECL(x)		x<0>

#include "../instruction_tabdef.inc"

#undef TYPE_RETOUR
#undef PARAMETRES  
#undef CALLTYPE
#undef NOM_TAB
#undef TABDECL

#define TYPE_RETOUR 	u32
#define PARAMETRES  	u32 pc, u32 instruction
#define CALLTYPE    	FASTCALL 
#define NOM_TAB     	arm_instructions_set_x86_arm7
#define TABDECL(x)		x<1>

#include "../instruction_tabdef.inc"
