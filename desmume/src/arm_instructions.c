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

#include "cp15.h"
#include "debug.h"
#include "MMU.h"


// Use this macros for reading/writing, so the GDB stub isn't broken
#ifdef GDB_STUB
	#define READ32(a,b)		cpu->mem_if->read32(a,b)
	#define WRITE32(a,b,c)	cpu->mem_if->write32(a,b,c)
	#define READ16(a,b)		cpu->mem_if->read16(a,b)
	#define WRITE16(a,b,c)	cpu->mem_if->write16(a,b,c)
	#define READ8(a,b)		cpu->mem_if->read8(a,b)
	#define WRITE8(a,b,c)	cpu->mem_if->write8(a,b,c)
#else
	#define READ32(a,b)		MMU_read32(cpu->proc_ID, b)
	#define WRITE32(a,b,c)	MMU_write32(cpu->proc_ID,b,c)
	#define READ16(a,b)		MMU_read16(cpu->proc_ID, b)
	#define WRITE16(a,b,c)	MMU_write16(cpu->proc_ID,b,c)
	#define READ8(a,b)		MMU_read8(cpu->proc_ID, b)
	#define WRITE8(a,b,c)	MMU_write8(cpu->proc_ID,b,c)
#endif



#define LSL_IMM shift_op = cpu->R[REG_POS(i,0)]<<((i>>7)&0x1F);

#define S_LSL_IMM u32 shift_op = ((i>>7)&0x1F);\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
          shift_op=cpu->R[REG_POS(i,0)];\
     else\
     {\
         c = BIT_N(cpu->R[REG_POS(i,0)], 32-shift_op);\
         shift_op = cpu->R[REG_POS(i,0)]<<((i>>7)&0x1F);\
     }
     
#define LSL_REG u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if(shift_op>=32)\
             shift_op=0;\
     else\
             shift_op=cpu->R[REG_POS(i,0)]<<shift_op;

#define S_LSL_REG u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
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
                 }                 

#define LSR_IMM      shift_op = ((i>>7)&0x1F);\
     if(shift_op!=0)\
            shift_op = cpu->R[REG_POS(i,0)]>>shift_op;

#define S_LSR_IMM      u32 shift_op = ((i>>7)&0x1F);\
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

#define LSR_REG      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if(shift_op>=32)\
             shift_op = 0;\
     else\
             shift_op = cpu->R[REG_POS(i,0)]>>shift_op;

#define S_LSR_REG      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
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
                  }

#define ASR_IMM      shift_op = ((i>>7)&0x1F);\
     if(shift_op==0)\
            shift_op=BIT31(cpu->R[REG_POS(i,0)])*0xFFFFFFFF;\
     else\
            shift_op = (u32)(((s32)(cpu->R[REG_POS(i,0)]))>>shift_op);

#define S_ASR_IMM      u32 shift_op = ((i>>7)&0x1F);\
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

#define ASR_REG      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if(shift_op==0)\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
             if(shift_op<32)\
                     shift_op = (u32)(((s32)(cpu->R[REG_POS(i,0)]))>>shift_op);\
             else\
                     shift_op=BIT31(cpu->R[REG_POS(i,0)])*0xFFFFFFFF;
                     
#define S_ASR_REG      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
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
                     
#define ROR_IMM      shift_op = ((i>>7)&0x1F);\
     if(shift_op==0)\
     {\
             u32 tmp = cpu->CPSR.bits.C;\
             shift_op = (tmp<<31)|(cpu->R[REG_POS(i,0)]>>1);\
     }\
     else\
             shift_op = ROR(cpu->R[REG_POS(i,0)],shift_op);
             
#define S_ROR_IMM      u32 shift_op = ((i>>7)&0x1F);\
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
             
#define ROR_REG      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     if((shift_op==0)||((shift_op&0xF)==0))\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
             shift_op = ROR(cpu->R[REG_POS(i,0)],(shift_op&0xF));

#define S_ROR_REG      u32 shift_op = (cpu->R[REG_POS(i,8)])&0xFF;\
     u32 c = cpu->CPSR.bits.C;\
     if(shift_op==0)\
             shift_op=cpu->R[REG_POS(i,0)];\
     else\
     {\
             shift_op&=0xF;\
             if(shift_op==0)\
             {\
                  shift_op=cpu->R[REG_POS(i,0)];\
                  c = BIT31(cpu->R[REG_POS(i,0)]);\
             }\
             else\
             {\
                 c = BIT_N(cpu->R[REG_POS(i,0)], shift_op-1);\
                  shift_op = ROR(cpu->R[REG_POS(i,0)],(shift_op&0xF));\
             }\
     }

#define IMM_VALUE      u32 shift_op = ROR((i&0xFF), (i>>7)&0x1E);

#define S_IMM_VALUE      u32 shift_op = ROR((i&0xFF), (i>>7)&0x1E);\
     u32 c = cpu->CPSR.bits.C;\
        if((i>>8)&0xF)\
             c = BIT31(shift_op);

#define IMM_OFF (((i>>4)&0xF0)+(i&0xF))

#define IMM_OFF_12 ((i)&0xFFF)

extern BOOL execute;

static u32 FASTCALL  OP_UND(armcpu_t *cpu)
{
	u32 i = cpu->instruction;
	LOG("Undefined instruction: %08X\n", i);
	execute = FALSE;
	return 1;
}
 
//-----------------------AND------------------------------------

#define OP_AND(a, b)      cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] & shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;

#define OP_ANDS(a, b)\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR;\
          cpu->R[15] = cpu->R[REG_POS(i,16)] & shift_op;\
          SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] & shift_op;\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     return a;
 
static u32 FASTCALL  OP_AND_LSL_IMM(register armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_AND(1, 3);
}

static u32 FASTCALL  OP_AND_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_AND(2, 4);
}

static u32 FASTCALL  OP_AND_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_AND(1, 3);
}

static u32 FASTCALL  OP_AND_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_AND(2, 4);
}

static u32 FASTCALL  OP_AND_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_AND(1, 3);
}

static u32 FASTCALL  OP_AND_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_AND(2, 4);
}

static u32 FASTCALL  OP_AND_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_AND(1, 3);
}

static u32 FASTCALL  OP_AND_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_AND(2, 4);
}

static u32 FASTCALL  OP_AND_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_AND(1, 3);
}

static u32 FASTCALL  OP_AND_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OP_ANDS(2, 4);
}

static u32 FASTCALL  OP_AND_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     OP_ANDS(3, 5);
}

static u32 FASTCALL  OP_AND_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     OP_ANDS(2, 4);
}

static u32 FASTCALL  OP_AND_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;             
     OP_ANDS(3, 5);
}

static u32 FASTCALL  OP_AND_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OP_ANDS(2, 4);
}

static u32 FASTCALL  OP_AND_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;                     
     OP_ANDS(3, 5);
}

static u32 FASTCALL  OP_AND_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OP_ANDS(2, 4);
}

static u32 FASTCALL  OP_AND_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OP_ANDS(3, 5);
}

static u32 FASTCALL  OP_AND_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OP_ANDS(2, 4);
}

//--------------EOR------------------------------

#define OP_EOR(a, b)    cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] ^ shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a; 
       
#define OP_EORS(a, b)          cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] ^ shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     return a;

static u32 FASTCALL  OP_EOR_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_EOR(1, 3); 
}

static u32 FASTCALL  OP_EOR_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_EOR(2, 4); 
}

static u32 FASTCALL  OP_EOR_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_EOR(1, 3); 
}

static u32 FASTCALL  OP_EOR_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_EOR(2, 4); 
}

static u32 FASTCALL  OP_EOR_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_EOR(1, 3); 
}

static u32 FASTCALL  OP_EOR_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_EOR(2, 4); 
}

static u32 FASTCALL  OP_EOR_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_EOR(1, 3); 
}

static u32 FASTCALL  OP_EOR_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_EOR(2, 4); 
}

static u32 FASTCALL  OP_EOR_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_EOR(1, 3); 
}

static u32 FASTCALL  OP_EOR_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OP_EORS(2, 4);
}

static u32 FASTCALL  OP_EOR_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     OP_EORS(3, 5);
}

static u32 FASTCALL  OP_EOR_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     OP_EORS(2, 4);
}

static u32 FASTCALL  OP_EOR_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;            
     OP_EORS(3, 5);
}

static u32 FASTCALL  OP_EOR_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OP_EORS(2, 4);
}

static u32 FASTCALL  OP_EOR_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;                     
     OP_EORS(3, 5);
}

static u32 FASTCALL  OP_EOR_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OP_EORS(2, 4);
}

static u32 FASTCALL  OP_EOR_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OP_EORS(3, 5);
}

static u32 FASTCALL  OP_EOR_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OP_EORS(2, 4);
}

//-------------SUB-------------------------------------

#define OP_SUB(a, b)      cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] - shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;

#define OPSUBS(a, b)          cpu->R[REG_POS(i,12)] = v - shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(v, shift_op, cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(v, shift_op, cpu->R[REG_POS(i,12)]);\
     return a;

static u32 FASTCALL  OP_SUB_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_SUB(1, 3);
}

static u32 FASTCALL  OP_SUB_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_SUB(2, 4);
}

static u32 FASTCALL  OP_SUB_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_SUB(1, 3);
}

static u32 FASTCALL  OP_SUB_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_SUB(2, 4);
}

static u32 FASTCALL  OP_SUB_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_SUB(1, 3);
}

static u32 FASTCALL  OP_SUB_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_SUB(2, 4);
}

static u32 FASTCALL  OP_SUB_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_SUB(1, 3);
}

static u32 FASTCALL  OP_SUB_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_SUB(2, 4);
}

static u32 FASTCALL  OP_SUB_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_SUB(1, 3);
}

static u32 FASTCALL  OP_SUB_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSL_IMM;
     OPSUBS(2, 4);
}

static u32 FASTCALL  OP_SUB_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSL_REG;
     OPSUBS(3, 5);
}

static u32 FASTCALL  OP_SUB_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSR_IMM;            
     OPSUBS(2, 4);
}

static u32 FASTCALL  OP_SUB_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSR_REG;             
     OPSUBS(3, 5);
}

static u32 FASTCALL  OP_SUB_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ASR_IMM;
     OPSUBS(2, 4);
}

static u32 FASTCALL  OP_SUB_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ASR_REG;                     
     OPSUBS(3, 5);
}

static u32 FASTCALL  OP_SUB_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ROR_IMM;
     OPSUBS(2, 4);
}

static u32 FASTCALL  OP_SUB_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ROR_REG;
     OPSUBS(3, 5);
}

static u32 FASTCALL  OP_SUB_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     IMM_VALUE;
     OPSUBS(2, 4);
}

//------------------RSB------------------------

#define OP_RSB(a, b)      cpu->R[REG_POS(i,12)] = shift_op - cpu->R[REG_POS(i,16)];\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;

#define OP_RSBS(a, b)      cpu->R[REG_POS(i,12)] = shift_op - v;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(shift_op, v, cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(shift_op, v, cpu->R[REG_POS(i,12)]);\
     return a;
     
static u32 FASTCALL  OP_RSB_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_RSB(1, 3);
}

static u32 FASTCALL  OP_RSB_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_RSB(2, 4);
}

static u32 FASTCALL  OP_RSB_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_RSB(1, 3);
}

static u32 FASTCALL  OP_RSB_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_RSB(2, 4);
}

static u32 FASTCALL  OP_RSB_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_RSB(1, 3);
}

static u32 FASTCALL  OP_RSB_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_RSB(2, 4);
}

static u32 FASTCALL  OP_RSB_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_RSB(1, 3);
}

static u32 FASTCALL  OP_RSB_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_RSB(2, 4);
}

static u32 FASTCALL  OP_RSB_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_RSB(1, 3);
}

static u32 FASTCALL  OP_RSB_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSL_IMM;
     OP_RSBS(2, 4);
}

static u32 FASTCALL  OP_RSB_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSL_REG;
     OP_RSBS(3, 5);
}

static u32 FASTCALL  OP_RSB_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSR_IMM;            
     OP_RSBS(2, 4);
}

static u32 FASTCALL  OP_RSB_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSR_REG;             
     OP_RSBS(3, 5);
}

static u32 FASTCALL  OP_RSB_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ASR_IMM;
     OP_RSBS(2, 4);
}

static u32 FASTCALL  OP_RSB_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ASR_REG;                     
     OP_RSBS(3, 5);
}

static u32 FASTCALL  OP_RSB_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ROR_IMM;
     OP_RSBS(2, 4);
}

static u32 FASTCALL  OP_RSB_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ROR_REG;
     OP_RSBS(3, 5);
}

static u32 FASTCALL  OP_RSB_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     IMM_VALUE;
     OP_RSBS(2, 4);
}

//------------------ADD-----------------------------------

#define OP_ADD(a, b)      cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] + shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;

static u32 FASTCALL  OP_ADD_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_ADD(1, 3);
}

static u32 FASTCALL  OP_ADD_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_ADD(2, 4);
}

static u32 FASTCALL  OP_ADD_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_ADD(1, 3);
}

static u32 FASTCALL  OP_ADD_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_ADD(2, 4);
}

static u32 FASTCALL  OP_ADD_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_ADD(1, 3);
}

static u32 FASTCALL  OP_ADD_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_ADD(2, 4);
}

static u32 FASTCALL  OP_ADD_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_ADD(1, 3);
}

static u32 FASTCALL  OP_ADD_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_ADD(2, 4);
}

static u32 FASTCALL  OP_ADD_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_ADD(1, 3);
}

#define OP_ADDS(a, b) cpu->R[REG_POS(i,12)] = v + shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(v, shift_op, cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(v, shift_op, cpu->R[REG_POS(i,12)]);\
     return a;
     
static u32 FASTCALL  OP_ADD_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSL_IMM;
     OP_ADDS(2, 4);
}

static u32 FASTCALL  OP_ADD_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSL_REG;
     OP_ADDS(3, 5);
}

static u32 FASTCALL  OP_ADD_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSR_IMM;            
     OP_ADDS(2, 4);
}

static u32 FASTCALL  OP_ADD_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSR_REG;             
     OP_ADDS(3, 5);
}

static u32 FASTCALL  OP_ADD_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ASR_IMM;
     OP_ADDS(2, 4);
}

static u32 FASTCALL  OP_ADD_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ASR_REG;                     
     OP_ADDS(3, 5);
}

static u32 FASTCALL  OP_ADD_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ROR_IMM;
     OP_ADDS(2, 4);
}

static u32 FASTCALL  OP_ADD_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ROR_REG;
     OP_ADDS(3, 5);
}

static u32 FASTCALL  OP_ADD_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     IMM_VALUE;
     OP_ADDS(2, 4);
}

//------------------ADC-----------------------------------

#define OP_ADC(a, b) cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] + shift_op + cpu->CPSR.bits.C;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;
     
static u32 FASTCALL  OP_ADC_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_ADC(1, 3);
}

static u32 FASTCALL  OP_ADC_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_ADC(2, 4);
}

static u32 FASTCALL  OP_ADC_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_ADC(1, 3);
}

static u32 FASTCALL  OP_ADC_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_ADC(2, 4);
}

static u32 FASTCALL  OP_ADC_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_ADC(1, 3);
}

static u32 FASTCALL  OP_ADC_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_ADC(2, 4);
}

static u32 FASTCALL  OP_ADC_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_ADC(1, 3);
}

static u32 FASTCALL  OP_ADC_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_ADC(2, 4);
}

static u32 FASTCALL  OP_ADC_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_ADC(1, 3);
}

#define OP_ADCS(a, b) \
     { \
     u32 tmp = shift_op + cpu->CPSR.bits.C;\
     cpu->R[REG_POS(i,12)] = v + tmp;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(shift_op, cpu->CPSR.bits.C, tmp) | UNSIGNED_OVERFLOW(v, tmp, cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(shift_op, cpu->CPSR.bits.C, tmp) | SIGNED_OVERFLOW(v, tmp, cpu->R[REG_POS(i,12)]);\
     return a; \
     }
     
static u32 FASTCALL  OP_ADC_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSL_IMM;
     OP_ADCS(2, 4);
}

static u32 FASTCALL  OP_ADC_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSL_REG;
     OP_ADCS(3, 5);
}

static u32 FASTCALL  OP_ADC_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSR_IMM;            
     OP_ADCS(2, 4);
}

static u32 FASTCALL  OP_ADC_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSR_REG;             
     OP_ADCS(3, 5);
}

static u32 FASTCALL  OP_ADC_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ASR_IMM;
     OP_ADCS(2, 4);
}

static u32 FASTCALL  OP_ADC_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ASR_REG;                     
     OP_ADCS(3, 5);
}

static u32 FASTCALL  OP_ADC_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ROR_IMM;
     OP_ADCS(2, 4);
}

static u32 FASTCALL  OP_ADC_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ROR_REG;
     OP_ADCS(3, 5);
}

static u32 FASTCALL  OP_ADC_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     IMM_VALUE;
     OP_ADCS(2, 4);
}

//-------------SBC-------------------------------------

#define OP_SBC(a, b) cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] - shift_op - (!cpu->CPSR.bits.C);\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;
     
static u32 FASTCALL  OP_SBC_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_SBC(1, 3);
}

static u32 FASTCALL  OP_SBC_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_SBC(2, 4);
}

static u32 FASTCALL  OP_SBC_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_SBC(1, 3);
}

static u32 FASTCALL  OP_SBC_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_SBC(2, 4);
}

static u32 FASTCALL  OP_SBC_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_SBC(1, 3);
}

static u32 FASTCALL  OP_SBC_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_SBC(2, 4);
}

static u32 FASTCALL  OP_SBC_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_SBC(1, 3);
}

static u32 FASTCALL  OP_SBC_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_SBC(2, 4);
}

static u32 FASTCALL  OP_SBC_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_SBC(1, 3);
}

#define OP_SBCS(a, b) \
     { \
     u32 tmp = v - (!cpu->CPSR.bits.C);\
     cpu->R[REG_POS(i,12)] = tmp - shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     cpu->CPSR.bits.C = (!UNSIGNED_UNDERFLOW(v, (!cpu->CPSR.bits.C), tmp)) & (!UNSIGNED_UNDERFLOW(tmp, shift_op, cpu->R[REG_POS(i,12)]));\
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(v, (!cpu->CPSR.bits.C), tmp) | SIGNED_UNDERFLOW(tmp, shift_op, cpu->R[REG_POS(i,12)]);\
     return a; \
     }

static u32 FASTCALL  OP_SBC_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSL_IMM;
     OP_SBCS(2, 4);
}

static u32 FASTCALL  OP_SBC_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSL_REG;
     OP_SBCS(3, 5);
}

static u32 FASTCALL  OP_SBC_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSR_IMM;            
     OP_SBCS(2, 4);
}

static u32 FASTCALL  OP_SBC_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSR_REG;             
     OP_SBCS(3, 5);
}

static u32 FASTCALL  OP_SBC_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ASR_IMM;
     OP_SBCS(2, 4);
}

static u32 FASTCALL  OP_SBC_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ASR_REG;                     
     OP_SBCS(3, 5);
}

static u32 FASTCALL  OP_SBC_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ROR_IMM;
     OP_SBCS(2, 4);
}

static u32 FASTCALL  OP_SBC_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ROR_REG;
     OP_SBCS(3, 5);
}

static u32 FASTCALL  OP_SBC_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     IMM_VALUE;
     OP_SBCS(2, 4);
}

//---------------RSC----------------------------------

#define OP_RSC(a, b) cpu->R[REG_POS(i,12)] =  shift_op - cpu->R[REG_POS(i,16)] - (!cpu->CPSR.bits.C);\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;
     
static u32 FASTCALL  OP_RSC_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_RSC(1, 3);
}

static u32 FASTCALL  OP_RSC_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_RSC(2, 4);
}

static u32 FASTCALL  OP_RSC_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_RSC(1, 3);
}

static u32 FASTCALL  OP_RSC_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_RSC(2, 4);
}

static u32 FASTCALL  OP_RSC_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_RSC(1, 3);
}

static u32 FASTCALL  OP_RSC_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_RSC(2, 4);
}

static u32 FASTCALL  OP_RSC_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_RSC(1, 3);
}

static u32 FASTCALL  OP_RSC_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_RSC(2, 4);
}

static u32 FASTCALL  OP_RSC_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_RSC(1, 3);
}

#define OP_RSCS(a,b) \
     { \
     u32 tmp = shift_op - (!cpu->CPSR.bits.C);\
     cpu->R[REG_POS(i,12)] = tmp - v;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
          }\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     cpu->CPSR.bits.C = (!UNSIGNED_UNDERFLOW(shift_op, (!cpu->CPSR.bits.C), tmp)) & (!UNSIGNED_UNDERFLOW(tmp, v, cpu->R[REG_POS(i,12)]));\
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(shift_op, (!cpu->CPSR.bits.C), tmp) | SIGNED_UNDERFLOW(tmp, v, cpu->R[REG_POS(i,12)]);\
     return a; \
     }

static u32 FASTCALL  OP_RSC_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSL_IMM;
     OP_RSCS(2,4);
}

static u32 FASTCALL  OP_RSC_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSL_REG;
     OP_RSCS(3,5);
}

static u32 FASTCALL  OP_RSC_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     LSR_IMM;            
     OP_RSCS(2,4);
}

static u32 FASTCALL  OP_RSC_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     LSR_REG;             
     OP_RSCS(3,5);
}

static u32 FASTCALL  OP_RSC_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ASR_IMM;
     OP_RSCS(2,4);
}

static u32 FASTCALL  OP_RSC_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ASR_REG;                     
     OP_RSCS(3,5);
}

static u32 FASTCALL  OP_RSC_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     u32 shift_op;
     ROR_IMM;
     OP_RSCS(2,4);
}

static u32 FASTCALL  OP_RSC_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     ROR_REG;
     OP_RSCS(3,5);
}

static u32 FASTCALL  OP_RSC_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,16)];
     IMM_VALUE;
     OP_RSCS(2,4);
}

//-------------------TST----------------------------

#define OP_TST(a) \
     { \
     unsigned tmp = cpu->R[REG_POS(i,16)] & shift_op;\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(tmp);\
     cpu->CPSR.bits.Z = (tmp==0);\
     return a; \
     }
     
static u32 FASTCALL  OP_TST_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OP_TST(1);
}

static u32 FASTCALL  OP_TST_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     OP_TST(2);
}

static u32 FASTCALL  OP_TST_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     OP_TST(1);
}

static u32 FASTCALL  OP_TST_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;             
     OP_TST(2);
}

static u32 FASTCALL  OP_TST_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OP_TST(1);
}

static u32 FASTCALL  OP_TST_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;                     
     OP_TST(2);
}

static u32 FASTCALL  OP_TST_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OP_TST(1);
}

static u32 FASTCALL  OP_TST_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OP_TST(2);
}

static u32 FASTCALL  OP_TST_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OP_TST(1);
}

//-------------------TEQ----------------------------

#define OP_TEQ(a) \
     { \
     unsigned tmp = cpu->R[REG_POS(i,16)] ^ shift_op;\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(tmp);\
     cpu->CPSR.bits.Z = (tmp==0);\
     return a; \
     }
     
static u32 FASTCALL  OP_TEQ_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OP_TEQ(1);
}

static u32 FASTCALL  OP_TEQ_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     OP_TEQ(2);
}

static u32 FASTCALL  OP_TEQ_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     OP_TEQ(1);
}

static u32 FASTCALL  OP_TEQ_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;             
     OP_TEQ(2);
}

static u32 FASTCALL  OP_TEQ_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OP_TEQ(1);
}

static u32 FASTCALL  OP_TEQ_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;                     
     OP_TEQ(2);
}

static u32 FASTCALL  OP_TEQ_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OP_TEQ(1);
}

static u32 FASTCALL  OP_TEQ_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OP_TEQ(2);
}

static u32 FASTCALL  OP_TEQ_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OP_TEQ(1);
}

//-------------CMP-------------------------------------

#define OP_CMP(a) \
     { \
     u32 tmp = cpu->R[REG_POS(i,16)] - shift_op;\
     cpu->CPSR.bits.N = BIT31(tmp);\
     cpu->CPSR.bits.Z = (tmp==0);\
     cpu->CPSR.bits.C = !UNSIGNED_UNDERFLOW(cpu->R[REG_POS(i,16)], shift_op, tmp);\
     cpu->CPSR.bits.V = SIGNED_UNDERFLOW(cpu->R[REG_POS(i,16)], shift_op, tmp);\
     return a; \
     }
     
static u32 FASTCALL  OP_CMP_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_CMP(1);
}

static u32 FASTCALL  OP_CMP_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_CMP(2);
}

static u32 FASTCALL  OP_CMP_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_CMP(1);
}

static u32 FASTCALL  OP_CMP_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_CMP(2);
}

static u32 FASTCALL  OP_CMP_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_CMP(1);
}

static u32 FASTCALL  OP_CMP_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_CMP(2);
}

static u32 FASTCALL  OP_CMP_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_CMP(1);
}

static u32 FASTCALL  OP_CMP_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_CMP(2);
}

static u32 FASTCALL  OP_CMP_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_CMP(1);
}

//---------------CMN---------------------------

#define OP_CMN(a) \
     { \
     u32 tmp = cpu->R[REG_POS(i,16)] + shift_op;\
     cpu->CPSR.bits.N = BIT31(tmp);\
     cpu->CPSR.bits.Z = (tmp==0);\
     cpu->CPSR.bits.C = UNSIGNED_OVERFLOW(cpu->R[REG_POS(i,16)], shift_op, tmp);\
     cpu->CPSR.bits.V = SIGNED_OVERFLOW(cpu->R[REG_POS(i,16)], shift_op, tmp);\
     return a; \
     }

static u32 FASTCALL  OP_CMN_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_CMN(1);
}

static u32 FASTCALL  OP_CMN_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_CMN(2);
}

static u32 FASTCALL  OP_CMN_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_CMN(1);
}

static u32 FASTCALL  OP_CMN_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_CMN(2);
}

static u32 FASTCALL  OP_CMN_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_CMN(1);
}

static u32 FASTCALL  OP_CMN_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_CMN(2);
}

static u32 FASTCALL  OP_CMN_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_CMN(1);
}

static u32 FASTCALL  OP_CMN_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_CMN(2);
}

static u32 FASTCALL  OP_CMN_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_CMN(1);
}

//------------------ORR-------------------

#define OP_ORR(a, b)  cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;

static u32 FASTCALL  OP_ORR_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_ORR(1, 3);
}

static u32 FASTCALL  OP_ORR_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OP_ORR(2, 4);
}

static u32 FASTCALL  OP_ORR_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_ORR(1, 3);
}

static u32 FASTCALL  OP_ORR_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OP_ORR(2, 4);
}

static u32 FASTCALL  OP_ORR_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_ORR(1, 3);
}

static u32 FASTCALL  OP_ORR_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_ORR(2, 4);
}

static u32 FASTCALL  OP_ORR_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_ORR(1, 3);
}

static u32 FASTCALL  OP_ORR_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_ORR(2, 4);
}

static u32 FASTCALL  OP_ORR_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_ORR(1, 3);
}

static u32 FASTCALL  OP_ORR_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 4;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 2;
}

static u32 FASTCALL  OP_ORR_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 5;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 3;
}

static u32 FASTCALL  OP_ORR_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 4;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 2;
}

static u32 FASTCALL  OP_ORR_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;             
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 5;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 3;
}

static u32 FASTCALL  OP_ORR_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 4;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 2;
}

static u32 FASTCALL  OP_ORR_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;                     
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 5;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 3;
}

static u32 FASTCALL  OP_ORR_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 4;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 2;
}

static u32 FASTCALL  OP_ORR_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 5;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 3;
}

static u32 FASTCALL  OP_ORR_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] | shift_op;
     if(REG_POS(i,12)==15)
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));
          cpu->next_instruction = cpu->R[15];
          return 4;
     }
     cpu->CPSR.bits.C = c;
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);
     return 2;
}

//------------------MOV-------------------

#define OP_MOV(a, b)      cpu->R[REG_POS(i,12)] = shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = shift_op;\
          return b;\
     }\
     return a;
     
#define OP_MOV_S(a, b)    cpu->R[REG_POS(i,12)] = shift_op;\
     if(BIT20(i) && REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     return a;\

static u32 FASTCALL  OP_MOV_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OP_MOV(1,3);
}

static u32 FASTCALL  OP_MOV_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
	 if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV(2,4);
}

static u32 FASTCALL  OP_MOV_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OP_MOV(1,3);
}

static u32 FASTCALL  OP_MOV_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;  
	 if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV(2,4);
}

static u32 FASTCALL  OP_MOV_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OP_MOV(1,3);
}

static u32 FASTCALL  OP_MOV_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OP_MOV(2,4);
}

static u32 FASTCALL  OP_MOV_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OP_MOV(2,4);
}

static u32 FASTCALL  OP_MOV_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OP_MOV(2,4);
}

static u32 FASTCALL  OP_MOV_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OP_MOV(1,3);
}

static u32 FASTCALL  OP_MOV_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OP_MOV_S(2,4);
}

static u32 FASTCALL  OP_MOV_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
	 if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV_S(3,5);
}

static u32 FASTCALL  OP_MOV_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;   
     OP_MOV_S(2,4);
}

static u32 FASTCALL  OP_MOV_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;  
	 if (REG_POS(i,0) == 15) shift_op += 4;
     OP_MOV_S(3,5);           
}

static u32 FASTCALL  OP_MOV_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OP_MOV_S(2,4);
}

static u32 FASTCALL  OP_MOV_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;  
     OP_MOV_S(3,5);                   
}

static u32 FASTCALL  OP_MOV_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OP_MOV_S(2,4);
}

static u32 FASTCALL  OP_MOV_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OP_MOV_S(3,5);
}

static u32 FASTCALL  OP_MOV_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OP_MOV_S(2,4);
}

//------------------BIC-------------------
#define OPP_BIC(a, b)      cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] & (~shift_op);\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;
     
#define OPP_BIC_S(a, b)    cpu->R[REG_POS(i,12)] = cpu->R[REG_POS(i,16)] & (~shift_op);\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     return a;
     
static u32 FASTCALL  OP_BIC_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OPP_BIC(1,3);
}

static u32 FASTCALL  OP_BIC_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OPP_BIC(2,4);
}

static u32 FASTCALL  OP_BIC_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OPP_BIC(1,3);
}

static u32 FASTCALL  OP_BIC_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;             
     OPP_BIC(2,4);
}

static u32 FASTCALL  OP_BIC_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OPP_BIC(1,3);
}

static u32 FASTCALL  OP_BIC_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;                     
     OPP_BIC(2,4);
}

static u32 FASTCALL  OP_BIC_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OPP_BIC(1,3);
}

static u32 FASTCALL  OP_BIC_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OPP_BIC(2,4);
}

static u32 FASTCALL  OP_BIC_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OPP_BIC(1,3);
}

static u32 FASTCALL  OP_BIC_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OPP_BIC_S(2,4);
}

static u32 FASTCALL  OP_BIC_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     OPP_BIC_S(3,5);
}

static u32 FASTCALL  OP_BIC_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     OPP_BIC_S(2,4);
}

static u32 FASTCALL  OP_BIC_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;
     OPP_BIC_S(3,5);
}

static u32 FASTCALL  OP_BIC_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OPP_BIC_S(2,4);
}

static u32 FASTCALL  OP_BIC_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;
     OPP_BIC_S(3,5);
}

static u32 FASTCALL  OP_BIC_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OPP_BIC_S(2,4);
}

static u32 FASTCALL  OP_BIC_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OPP_BIC_S(3,5);
}

static u32 FASTCALL  OP_BIC_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OPP_BIC_S(2,4);
}

//------------------MVN-------------------
#define OPP_MVN(a, b)     cpu->R[REG_POS(i,12)] = ~shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     return a;
     
#define OPP_MVN_S(a, b)   cpu->R[REG_POS(i,12)] = ~shift_op;\
     if(REG_POS(i,12)==15)\
     {\
          Status_Reg SPSR = cpu->SPSR;\
          armcpu_switchMode(cpu, SPSR.bits.mode);\
          cpu->CPSR=SPSR;\
          cpu->R[15] &= (0XFFFFFFFC|(((u32)SPSR.bits.T)<<1));\
          cpu->next_instruction = cpu->R[15];\
          return b;\
     }\
     cpu->CPSR.bits.C = c;\
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,12)]);\
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,12)]==0);\
     return a;
     
static u32 FASTCALL  OP_MVN_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSL_IMM;
     OPP_MVN(1,3);
}

static u32 FASTCALL  OP_MVN_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSL_REG;
     OPP_MVN(2,4);
}

static u32 FASTCALL  OP_MVN_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     LSR_IMM;            
     OPP_MVN(1,3);
}

static u32 FASTCALL  OP_MVN_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     LSR_REG;
     OPP_MVN(2,4);
}

static u32 FASTCALL  OP_MVN_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ASR_IMM;
     OPP_MVN(1,3);
}

static u32 FASTCALL  OP_MVN_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ASR_REG;
     OPP_MVN(2,4);
}

static u32 FASTCALL  OP_MVN_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 shift_op;
     ROR_IMM;
     OPP_MVN(1,3);
}

static u32 FASTCALL  OP_MVN_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     ROR_REG;
     OPP_MVN(2,4);
}

static u32 FASTCALL  OP_MVN_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     OPP_MVN(1,3);
}

static u32 FASTCALL  OP_MVN_S_LSL_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_IMM;
     OPP_MVN_S(2,4);
}

static u32 FASTCALL  OP_MVN_S_LSL_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSL_REG;
     OPP_MVN_S(3,5);
}

static u32 FASTCALL  OP_MVN_S_LSR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_IMM;            
     OPP_MVN_S(2,4);
}

static u32 FASTCALL  OP_MVN_S_LSR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_LSR_REG;
     OPP_MVN_S(3,5);
}

static u32 FASTCALL  OP_MVN_S_ASR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_IMM;
     OPP_MVN_S(2,4);
}

static u32 FASTCALL  OP_MVN_S_ASR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ASR_REG;
     OPP_MVN_S(3,5);
}

static u32 FASTCALL  OP_MVN_S_ROR_IMM(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_IMM;
     OPP_MVN_S(2,4);
}

static u32 FASTCALL  OP_MVN_S_ROR_REG(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_ROR_REG;
     OPP_MVN_S(3,5);
}

static u32 FASTCALL  OP_MVN_S_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     S_IMM_VALUE;
     OPP_MVN_S(2,4);
}

//-------------MUL------------------------
#define OPP_M(a,b)     v >>= 8;\
     if((v==0)||(v==0xFFFFFF))\
          return b;\
     v >>= 8;\
     if((v==0)||(v==0xFFFF))\
          return b+1;\
     v >>= 8;\
     if((v==0)||(v==0xFF))\
          return b+2;\
     return a;\

static u32 FASTCALL  OP_MUL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,16)] = cpu->R[REG_POS(i,8)] * v;
     OPP_M(5,2);
}

static u32 FASTCALL  OP_MLA(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     u32 a = cpu->R[REG_POS(i,8)];
     u32 b = cpu->R[REG_POS(i,12)];
     cpu->R[REG_POS(i,16)] = a * v + b;
     
     OPP_M(6,3);
}

static u32 FASTCALL  OP_MUL_S(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,16)] = cpu->R[REG_POS(i,8)] * v;
     
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,16)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,16)]==0);
     
     OPP_M(6,3);
}

static u32 FASTCALL  OP_MLA_S(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,16)] = cpu->R[REG_POS(i,8)] * v + cpu->R[REG_POS(i,12)];
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,16)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,16)]==0);
     OPP_M(7,4);
}

//----------UMUL--------------------------

static u32 FASTCALL  OP_UMULL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     u64 res = (u64)v * (u64)cpu->R[REG_POS(i,8)];
     
     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] = (u32)(res>>32);   
       
     OPP_M(6,3);
}

static u32 FASTCALL  OP_UMLAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     u64 res = (u64)v * (u64)cpu->R[REG_POS(i,8)] + (u64)cpu->R[REG_POS(i,12)];
     
     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] += (u32)(res>>32);     
     
     OPP_M(7,4);
}

static u32 FASTCALL  OP_UMULL_S(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     u64 res = (u64)v * (u64)cpu->R[REG_POS(i,8)];
     
     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] = (u32)(res>>32);     
     
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,16)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,16)]==0) & (cpu->R[REG_POS(i,12)]==0);

     OPP_M(7,4);
}

static u32 FASTCALL  OP_UMLAL_S(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 v = cpu->R[REG_POS(i,0)];
     u64 res = (u64)v * (u64)cpu->R[REG_POS(i,8)] + (u64)cpu->R[REG_POS(i,12)];
     
     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] += (u32)(res>>32);    
      
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,16)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,16)]==0) & (cpu->R[REG_POS(i,12)]==0);
     
     OPP_M(8,5);
}

//----------SMUL--------------------------

static u32 FASTCALL  OP_SMULL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 v = (s32)cpu->R[REG_POS(i,0)];
     s64 b = (s32)cpu->R[REG_POS(i,8)];
     s64 res = v * b;
     
     cpu->R[REG_POS(i,12)] = (u32)(res&0xFFFFFFFF);
     cpu->R[REG_POS(i,16)] = (u32)(res>>32);     
     
     v &= 0xFFFFFFFF;
          
     OPP_M(6,3);
}

static u32 FASTCALL  OP_SMLAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     
     s64 v = (s32)cpu->R[REG_POS(i,0)];
     s64 b = (s32)cpu->R[REG_POS(i,8)];
     s64 res = v * b + (u64)cpu->R[REG_POS(i,12)];
     
     //LOG("%08X * %08X + %08X%08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], cpu->R[REG_POS(i,16)], cpu->R[REG_POS(i,12)]);

     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] += (u32)(res>>32);     
     
     //LOG("= %08X%08X  %08X%08X\r\n", cpu->R[REG_POS(i,16)], cpu->R[REG_POS(i,12)], res);
     
     v &= 0xFFFFFFFF;
          
     OPP_M(7,4);
}

static u32 FASTCALL  OP_SMULL_S(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 v = (s32)cpu->R[REG_POS(i,0)];
     s64 b = (s32)cpu->R[REG_POS(i,8)];
     s64 res = v * b;
     
     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] = (u32)(res>>32);     
     
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,16)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,16)]==0) & (cpu->R[REG_POS(i,12)]==0);

     v &= 0xFFFFFFFF;
          
     OPP_M(7,4);
}

static u32 FASTCALL  OP_SMLAL_S(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 v = (s32)cpu->R[REG_POS(i,0)];
     s64 b = (s32)cpu->R[REG_POS(i,8)];
     s64 res = v * b + (u64)cpu->R[REG_POS(i,12)];
     
     cpu->R[REG_POS(i,12)] = (u32)res;
     cpu->R[REG_POS(i,16)] += (u32)(res>>32);    
      
     cpu->CPSR.bits.N = BIT31(cpu->R[REG_POS(i,16)]);
     cpu->CPSR.bits.Z = (cpu->R[REG_POS(i,16)]==0) & (cpu->R[REG_POS(i,12)]==0);

     v &= 0xFFFFFFFF;
          
     OPP_M(8,5);
}

//---------------SWP------------------------------

static u32 FASTCALL  OP_SWP(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     u32 tmp = ROR(READ32(cpu->mem_if->data, adr), ((cpu->R[REG_POS(i,16)]&3)<<3));
     
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,0)]);
     cpu->R[REG_POS(i,12)] = tmp;
     
     return 4 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF]*2;
}

static u32 FASTCALL  OP_SWPB(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     u8 tmp = READ8(cpu->mem_if->data, adr);
     WRITE8(cpu->mem_if->data, adr, (u8)(cpu->R[REG_POS(i,0)]&0xFF));
     cpu->R[REG_POS(i,12)] = tmp;

     return 4 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF]*2;
}

//------------LDRH-----------------------------

static u32 FASTCALL  OP_LDRH_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
    return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_PRE_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_PRE_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_PRE_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] =(u32)READ16(cpu->mem_if->data, adr);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_PRE_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_POS_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] += IMM_OFF;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_POS_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] -= IMM_OFF;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_POS_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] += cpu->R[REG_POS(i,0)];
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRH_POS_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (u32)READ16(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] -= cpu->R[REG_POS(i,0)];
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//------------STRH-----------------------------

static u32 FASTCALL  OP_STRH_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_PRE_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
	 cpu->R[REG_POS(i,16)] = adr;
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_PRE_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_PRE_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_PRE_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_POS_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] += IMM_OFF;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_POS_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] -= IMM_OFF;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_POS_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] += cpu->R[REG_POS(i,0)];
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRH_POS_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE16(cpu->mem_if->data, adr, (u16)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] -= cpu->R[REG_POS(i,0)];
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//----------------LDRSH--------------------------

static u32 FASTCALL  OP_LDRSH_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_PRE_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_PRE_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_PRE_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_PRE_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_POS_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] += IMM_OFF;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_POS_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] -= IMM_OFF;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_POS_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] += cpu->R[REG_POS(i,0)];
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSH_POS_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s16)READ16(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] -= cpu->R[REG_POS(i,0)];
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//----------------------LDRSB----------------------

static u32 FASTCALL  OP_LDRSB_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_PRE_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_PRE_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF;
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_PRE_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_PRE_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - cpu->R[REG_POS(i,0)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_POS_INDE_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] += IMM_OFF;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_POS_INDE_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] -= IMM_OFF;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_POS_INDE_P_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] += cpu->R[REG_POS(i,0)];
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRSB_POS_INDE_M_REG_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     cpu->R[REG_POS(i,12)] = (s32)((s8)READ8(cpu->mem_if->data, adr));
     cpu->R[REG_POS(i,16)] -= cpu->R[REG_POS(i,0)];
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//--------------MRS--------------------------------

static u32 FASTCALL  OP_MRS_CPSR(armcpu_t *cpu)
{
     cpu->R[REG_POS(cpu->instruction,12)] = cpu->CPSR.val;
     
     return 1;
}

static u32 FASTCALL  OP_MRS_SPSR(armcpu_t *cpu)
{
     cpu->R[REG_POS(cpu->instruction,12)] = cpu->SPSR.val;
     
     return 1;
}

//--------------MSR--------------------------------

static u32 FASTCALL  OP_MSR_CPSR(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 operand = cpu->R[REG_POS(i,0)];
          
     if(cpu->CPSR.bits.mode!=USR)
     {
          if(BIT16(i))
          {
               armcpu_switchMode(cpu, operand & 0x1F);
               cpu->CPSR.val = (cpu->CPSR.val & 0xFFFFFF00) | (operand & 0xFF);
          }
          if(BIT17(i))
               cpu->CPSR.val = (cpu->CPSR.val & 0xFFFF00FF) | (operand & 0xFF00);
          if(BIT18(i))
               cpu->CPSR.val = (cpu->CPSR.val & 0xFF00FFFF) | (operand & 0xFF0000);
     }
     if(BIT19(i))
          cpu->CPSR.val = (cpu->CPSR.val & 0x00FFFFFF) | (operand & 0xFF000000);
          
     return 1;
}

static u32 FASTCALL  OP_MSR_SPSR(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 operand = cpu->R[REG_POS(i,0)];
          
     if(cpu->CPSR.bits.mode!=USR)
     {
          if(BIT16(i))
          {
               cpu->SPSR.val = (cpu->SPSR.val & 0xFFFFFF00) | (operand & 0XFF);
          }
          if(BIT17(i))
               cpu->SPSR.val = (cpu->SPSR.val & 0xFFFF00FF) | (operand & 0XFF00);
          if(BIT18(i))
               cpu->SPSR.val = (cpu->SPSR.val & 0xFF00FFFF) | (operand & 0XFF0000);
     }
     if(BIT19(i))
          cpu->SPSR.val = (cpu->SPSR.val & 0x00FFFFFF) | (operand & 0XFF000000);
     
     return 1;
}

static u32 FASTCALL  OP_MSR_CPSR_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;
     
     if(cpu->CPSR.bits.mode!=USR)
     {
          if(BIT16(i))
          {
               armcpu_switchMode(cpu, shift_op & 0x1F);
               cpu->CPSR.val = (cpu->CPSR.val & 0xFFFFFF00) | (shift_op & 0XFF);
          }
          if(BIT17(i))
               cpu->CPSR.val = (cpu->CPSR.val & 0xFFFF00FF) | (shift_op & 0XFF00);
          if(BIT18(i))
               cpu->CPSR.val = (cpu->CPSR.val & 0xFF00FFFF) | (shift_op & 0XFF0000);
     }
     if(BIT19(i))
	  {
          //cpu->CPSR.val = (cpu->CPSR.val & 0xFF000000) | (shift_op & 0XFF000000);
		  cpu->CPSR.val = (cpu->CPSR.val & 0x00FFFFFF) | (shift_op & 0xFF000000);
	  }
     
     return 1;
}

static u32 FASTCALL  OP_MSR_SPSR_IMM_VAL(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     IMM_VALUE;

     if(cpu->CPSR.bits.mode!=USR)
     {
          if(BIT16(i))
          {
               cpu->SPSR.val = (cpu->SPSR.val & 0xFFFFFF00) | (shift_op & 0XFF);
          }
          if(BIT17(i))
               cpu->SPSR.val = (cpu->SPSR.val & 0xFFFF00FF) | (shift_op & 0XFF00);
          if(BIT18(i))
               cpu->SPSR.val = (cpu->SPSR.val & 0xFF00FFFF) | (shift_op & 0XFF0000);
     }
     if(BIT19(i))
          cpu->SPSR.val = (cpu->SPSR.val & 0xFF000000) | (shift_op & 0XFF000000);
     
     return 1;
}

//-----------------BRANCH--------------------------

static u32 FASTCALL  OP_BX(armcpu_t *cpu)
{
     u32 tmp = cpu->R[REG_POS(cpu->instruction, 0)];
     
     cpu->CPSR.bits.T = BIT0(tmp);
     cpu->R[15] = tmp & 0xFFFFFFFE;
     cpu->next_instruction = cpu->R[15];
     return 3;
}

static u32 FASTCALL  OP_BLX_REG(armcpu_t *cpu)
{
     u32 tmp = cpu->R[REG_POS(cpu->instruction, 0)];
     
     cpu->R[14] = cpu->next_instruction;
     cpu->CPSR.bits.T = BIT0(tmp);
     cpu->R[15] = tmp & 0xFFFFFFFE;
     cpu->next_instruction = cpu->R[15];
     return 3;
}

#define SIGNEXTEND_24(i) (((s32)((i)<<8))>>8)

static u32 FASTCALL  OP_B(armcpu_t *cpu)
{
	u32 off = SIGNEXTEND_24(cpu->instruction);
	if(CONDITION(cpu->instruction)==0xF)
	{
		cpu->R[14] = cpu->next_instruction;
		cpu->CPSR.bits.T = 1;
	}
	cpu->R[15] += (off<<2);
	cpu->next_instruction = cpu->R[15];

    return 3;
}

static u32 FASTCALL  OP_BL(armcpu_t *cpu)
{
	u32 off = SIGNEXTEND_24(cpu->instruction);
	if(CONDITION(cpu->instruction)==0xF)
	{
		cpu->CPSR.bits.T = 1;
		cpu->R[15] += 2;
	}
	cpu->R[14] = cpu->next_instruction;
	cpu->R[15] += (off<<2);
	cpu->next_instruction = cpu->R[15];

	return 3;
}

//----------------CLZ-------------------------------

u8 CLZ_TAB[16]=
{
     0,                     // 0000
     1,                     // 0001
     2, 2,                  // 001X
     3, 3, 3, 3,            // 01XX
     4, 4, 4, 4, 4, 4, 4, 4 // 1XXX
};
     
static u32 FASTCALL  OP_CLZ(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 Rm = cpu->R[REG_POS(i,0)];
     u32 pos;
     
     if(Rm==0)
     {
          cpu->R[REG_POS(i,12)]=32;
          return 2;
     }
     
     Rm |= (Rm >>1);
     Rm |= (Rm >>2);
     Rm |= (Rm >>4);
     Rm |= (Rm >>8);
     Rm |= (Rm >>16);
     
     pos =      
          CLZ_TAB[Rm&0xF] +
          CLZ_TAB[(Rm>>4)&0xF] +
          CLZ_TAB[(Rm>>8)&0xF] +
          CLZ_TAB[(Rm>>12)&0xF] +
          CLZ_TAB[(Rm>>16)&0xF] +
          CLZ_TAB[(Rm>>20)&0xF] +
          CLZ_TAB[(Rm>>24)&0xF] +
          CLZ_TAB[(Rm>>28)&0xF];
     
     cpu->R[REG_POS(i,12)]=32 - pos;
     
     return 2;
}

//--------------------QADD--QSUB------------------------------

static u32 FASTCALL  OP_QADD(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
        u32 res = cpu->R[REG_POS(i,16)]+cpu->R[REG_POS(i,0)];

    LOG("spe add\r\n");
	if(SIGNED_OVERFLOW(cpu->R[REG_POS(i,16)],cpu->R[REG_POS(i,0)], res))
	{
		cpu->CPSR.bits.Q=1;
		cpu->R[REG_POS(i,12)]=0x80000000-BIT31(res);
		return 2;
	}
	cpu->R[REG_POS(i,12)]=res;
	if(REG_POS(i,12)==15)
	{
         cpu->R[15] &= 0XFFFFFFFC;
	     cpu->next_instruction = cpu->R[15];
	     return 3;
    }
    return 2;
}

static u32 FASTCALL  OP_QSUB(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
        u32 res = cpu->R[REG_POS(i,0)]-cpu->R[REG_POS(i,16)];

    LOG("spe add\r\n");
	if(SIGNED_UNDERFLOW(cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,16)], res))
	{
		cpu->CPSR.bits.Q=1;
		cpu->R[REG_POS(i,12)]=0x80000000-BIT31(res);
		return 2;
	}
	cpu->R[REG_POS(i,12)]=res;
	if(REG_POS(i,12)==15)
	{
         cpu->R[15] &= 0XFFFFFFFC;
	     cpu->next_instruction = cpu->R[15];
	     return 3;
    }
    return 2;
}

static u32 FASTCALL  OP_QDADD(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
        u32 mul = cpu->R[REG_POS(i,16)]<<1;
        u32 res;


    LOG("spe add\r\n");
	if(BIT31(cpu->R[REG_POS(i,16)])!=BIT31(mul))
	{
		cpu->CPSR.bits.Q=1;
		mul = 0x80000000-BIT31(mul);
	}

	res = mul + cpu->R[REG_POS(i,0)];
	if(SIGNED_OVERFLOW(cpu->R[REG_POS(i,0)],mul, res))
	{
		cpu->CPSR.bits.Q=1;
		cpu->R[REG_POS(i,12)]=0x80000000-BIT31(res);
		return 2;
	}
	cpu->R[REG_POS(i,12)]=res;
	if(REG_POS(i,12)==15)
	{
         cpu->R[15] &= 0XFFFFFFFC;
	     cpu->next_instruction = cpu->R[15];
	     return 3;
    }
    return 2;
}

static u32 FASTCALL  OP_QDSUB(armcpu_t *cpu)
{
    u32 i = cpu->instruction;
        u32 mul = cpu->R[REG_POS(i,16)]<<1;
        u32 res;


    LOG("spe add\r\n");
	if(BIT31(cpu->R[REG_POS(i,16)])!=BIT31(mul))
	{
		cpu->CPSR.bits.Q=1;
		mul = 0x80000000-BIT31(mul);
	}

	res = cpu->R[REG_POS(i,0)] - mul;
	if(SIGNED_UNDERFLOW(cpu->R[REG_POS(i,0)], mul, res))
	{
		cpu->CPSR.bits.Q=1;
		cpu->R[REG_POS(i,12)]=0x80000000-BIT31(res);
		return 2;
	}
	cpu->R[REG_POS(i,12)]=res;
	if(REG_POS(i,12)==15)
	{
         cpu->R[15] &= 0XFFFFFFFC;
	     cpu->next_instruction = cpu->R[15];
	     return 3;
    }
    return 2;
}

//-----------------SMUL-------------------------------

#define HWORD(i)   ((s32)(((s32)(i))>>16))
#define LWORD(i)   (s32)(((s32)((i)<<16))>>16)

static u32 FASTCALL  OP_SMUL_B_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     
     cpu->R[REG_POS(i,16)] = (u32)(LWORD(cpu->R[REG_POS(i,0)])* LWORD(cpu->R[REG_POS(i,8)]));

     return 2;
}

static u32 FASTCALL  OP_SMUL_B_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;

     cpu->R[REG_POS(i,16)] = (u32)(LWORD(cpu->R[REG_POS(i,0)])* HWORD(cpu->R[REG_POS(i,8)]));

     return 2;
}

static u32 FASTCALL  OP_SMUL_T_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;

     cpu->R[REG_POS(i,16)] = (u32)(HWORD(cpu->R[REG_POS(i,0)])* LWORD(cpu->R[REG_POS(i,8)]));

     return 2;
}

static u32 FASTCALL  OP_SMUL_T_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;

     cpu->R[REG_POS(i,16)] = (u32)(HWORD(cpu->R[REG_POS(i,0)])* HWORD(cpu->R[REG_POS(i,8)]));

     return 2;
}

//-----------SMLA----------------------------

static u32 FASTCALL  OP_SMLA_B_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 tmp = (u32)(LWORD(cpu->R[REG_POS(i,0)])* LWORD(cpu->R[REG_POS(i,8)]));
     u32 a = cpu->R[REG_POS(i,12)];

     //LOG("SMLABB %08X * %08X + %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], a, tmp + a);
     cpu->R[REG_POS(i,16)] = tmp + a;
     
     if(SIGNED_OVERFLOW(tmp, a, cpu->R[REG_POS(i,16)]))
          cpu->CPSR.bits.Q = 1;

     return 2;
}

static u32 FASTCALL  OP_SMLA_B_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 tmp = (u32)(LWORD(cpu->R[REG_POS(i,0)])* HWORD(cpu->R[REG_POS(i,8)]));
     u32 a = cpu->R[REG_POS(i,12)];

     //LOG("SMLABT %08X * %08X + %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], a, tmp + a);
     cpu->R[REG_POS(i,16)] = tmp + a;
     
     if(SIGNED_OVERFLOW(tmp, a, cpu->R[REG_POS(i,16)]))
          cpu->CPSR.bits.Q = 1;

     return 2;
}

static u32 FASTCALL  OP_SMLA_T_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 tmp = (u32)(HWORD(cpu->R[REG_POS(i,0)])* LWORD(cpu->R[REG_POS(i,8)]));
     u32 a = cpu->R[REG_POS(i,12)];

     //LOG("SMLATB %08X * %08X + %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], a, tmp + a);
     cpu->R[REG_POS(i,16)] = tmp + a;
     
     if(SIGNED_OVERFLOW(tmp, a, cpu->R[REG_POS(i,16)]))
          cpu->CPSR.bits.Q = 1;

     return 2;
}

static u32 FASTCALL  OP_SMLA_T_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 tmp = (u32)(HWORD(cpu->R[REG_POS(i,0)])* HWORD(cpu->R[REG_POS(i,8)]));
     u32 a = cpu->R[REG_POS(i,12)];

     //LOG("SMLATT %08X * %08X + %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], a, tmp + a);
     cpu->R[REG_POS(i,16)] = tmp + a;
     
     if(SIGNED_OVERFLOW(tmp, a, cpu->R[REG_POS(i,16)]))
          cpu->CPSR.bits.Q = 1;

     return 2;
}

//--------------SMLAL---------------------------------------

static u32 FASTCALL  OP_SMLAL_B_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)(LWORD(cpu->R[REG_POS(i,0)])* LWORD(cpu->R[REG_POS(i,8)]));
     u64 res = (u64)tmp + cpu->R[REG_POS(i,12)];

     LOG("SMLALBB %08X * %08X + %08X%08X = %08X%08X\r\n", (int)cpu->R[REG_POS(i,0)], (int)cpu->R[REG_POS(i,8)], (int)cpu->R[REG_POS(i,16)], (int)cpu->R[REG_POS(i,12)], (int)(cpu->R[REG_POS(i,16)] + (res + ((tmp<0)*0xFFFFFFFF))), (int)(u32) res);

     cpu->R[REG_POS(i,12)] = (u32) res;
     cpu->R[REG_POS(i,16)] += (res + ((tmp<0)*0xFFFFFFFF));
     
     return 2;
}

static u32 FASTCALL  OP_SMLAL_B_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)(LWORD(cpu->R[REG_POS(i,0)])* HWORD(cpu->R[REG_POS(i,8)]));
     u64 res = (u64)tmp + cpu->R[REG_POS(i,12)];
     
     LOG("SMLALBT %08X * %08X + %08X%08X = %08X%08X\r\n", (int)cpu->R[REG_POS(i,0)], (int)cpu->R[REG_POS(i,8)], (int)cpu->R[REG_POS(i,16)], (int)cpu->R[REG_POS(i,12)], (int)(cpu->R[REG_POS(i,16)] + res + ((tmp<0)*0xFFFFFFFF)), (int)(u32) res);

     cpu->R[REG_POS(i,12)] = (u32) res;
     cpu->R[REG_POS(i,16)] += res + ((tmp<0)*0xFFFFFFFF);

     return 2;
}

static u32 FASTCALL  OP_SMLAL_T_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)(HWORD(cpu->R[REG_POS(i,0)])* (s64)LWORD(cpu->R[REG_POS(i,8)]));
     u64 res = (u64)tmp + cpu->R[REG_POS(i,12)];
     
     LOG("SMLALTB %08X * %08X + %08X%08X = %08X%08X\r\n", (int)cpu->R[REG_POS(i,0)], (int)cpu->R[REG_POS(i,8)], (int)cpu->R[REG_POS(i,16)], (int)cpu->R[REG_POS(i,12)], (int)(cpu->R[REG_POS(i,16)] + res + ((tmp<0)*0xFFFFFFFF)), (int)(u32) res);

     cpu->R[REG_POS(i,12)] = (u32) res;
     cpu->R[REG_POS(i,16)] += res + ((tmp<0)*0xFFFFFFFF);

     return 2;
}

static u32 FASTCALL  OP_SMLAL_T_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)(HWORD(cpu->R[REG_POS(i,0)])* HWORD(cpu->R[REG_POS(i,8)]));
     u64 res = (u64)tmp + cpu->R[REG_POS(i,12)];
     
     LOG("SMLALTT %08X * %08X + %08X%08X = %08X%08X\r\n", (int)cpu->R[REG_POS(i,0)], (int)cpu->R[REG_POS(i,8)], (int)cpu->R[REG_POS(i,16)], (int)cpu->R[REG_POS(i,12)], (int)(cpu->R[REG_POS(i,16)] + res + ((tmp<0)*0xFFFFFFFF)), (int)(u32) res);
     
     cpu->R[REG_POS(i,12)] = (u32) res;
     cpu->R[REG_POS(i,16)] += res + ((tmp<0)*0xFFFFFFFF);

     return 2;
}

//--------------SMULW--------------------

static u32 FASTCALL  OP_SMULW_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)LWORD(cpu->R[REG_POS(i,8)]) * (s64)((s32)cpu->R[REG_POS(i,0)]);
     
     //LOG("SMULWB %08X * %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], ((tmp>>16)&0xFFFFFFFF);
     
     cpu->R[REG_POS(i,16)] = ((tmp>>16)&0xFFFFFFFF);

     return 2;
}

static u32 FASTCALL  OP_SMULW_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)HWORD(cpu->R[REG_POS(i,8)]) * (s64)((s32)cpu->R[REG_POS(i,0)]);
     
     //LOG("SMULWT %08X * %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], ((tmp>>16)&0xFFFFFFFF));
     
     cpu->R[REG_POS(i,16)] = ((tmp>>16)&0xFFFFFFFF);

     return 2;
}

//--------------SMLAW-------------------
static u32 FASTCALL  OP_SMLAW_B(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)LWORD(cpu->R[REG_POS(i,8)]) * (s64)((s32)cpu->R[REG_POS(i,0)]);
     u32 a = cpu->R[REG_POS(i,12)];
     
     //LOG("SMLAWB %08X * %08X + %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], a, (tmp>>16) + a);
     
     tmp = (tmp>>16);
     
     cpu->R[REG_POS(i,16)] = tmp + a;
          
     if(SIGNED_OVERFLOW(tmp, a, cpu->R[REG_POS(i,16)]))
          cpu->CPSR.bits.Q = 1;

     return 2;
}

static u32 FASTCALL  OP_SMLAW_T(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     s64 tmp = (s64)HWORD(cpu->R[REG_POS(i,8)]) * (s64)((s32)cpu->R[REG_POS(i,0)]);
     u32 a = cpu->R[REG_POS(i,12)];
     
     //LOG("SMLAWT %08X * %08X + %08X = %08X\r\n", cpu->R[REG_POS(i,0)], cpu->R[REG_POS(i,8)], a, ((tmp>>16)&0xFFFFFFFF) + a);

     tmp = ((tmp>>16)&0xFFFFFFFF);
     cpu->R[REG_POS(i,16)] = tmp + a;

     if(SIGNED_OVERFLOW(tmp, a, cpu->R[REG_POS(i,16)]))
          cpu->CPSR.bits.Q = 1;

     return 2;
}

//------------LDR---------------------------

static u32 FASTCALL  OP_LDR_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     u32 val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     u32 val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     u32 val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;    
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     u32 val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     u32 val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

//------------------------------------------------------------
static u32 FASTCALL  OP_LDR_P_IMM_OFF_POSTIND2(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     
     u32 adr = cpu->R[REG_POS(i,16)];
     u32 val = READ32(cpu->mem_if->data, adr);
     u32 old;
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     old = armcpu_switchMode(cpu, USR);
     cpu->R[REG_POS(i,12)] = val;
     armcpu_switchMode(cpu, old);
     
     cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

//------------------------------------------------------------

static u32 FASTCALL  OP_LDR_M_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     u32 val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr + shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr - shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr + shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr - shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr + shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr - shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
     cpu->R[REG_POS(i,16)] = adr - shift_op;
	 cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_P_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr + shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDR_M_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ32(cpu->mem_if->data, adr);
     
     if(adr&3)
         val = ROR(val, 8*(adr&3));
     
     if(REG_POS(i,12)==15)
     {
          cpu->R[15] = val & (0XFFFFFFFC | (((u32)cpu->LDTBit)<<1));
          cpu->CPSR.bits.T = BIT0(val) & cpu->LDTBit;
	      cpu->next_instruction = cpu->R[15];
          cpu->R[REG_POS(i,16)] = adr - shift_op;
          return 5 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
     }
     
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

//-----------------LDRB-------------------------------------------

static u32 FASTCALL  OP_LDRB_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     u32 val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     u32 val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     u32 val = READ8(cpu->mem_if->data, adr);

	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     u32 val = READ8(cpu->mem_if->data, adr);

	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);

	 cpu->R[REG_POS(i,16)] = adr;
     cpu->R[REG_POS(i,12)] = val;
     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,16)] = adr;
	 cpu->R[REG_POS(i,12)] = val;
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     u32 val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     u32 val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_P_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr + shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRB_M_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 val;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
	 cpu->R[REG_POS(i,16)] = adr - shift_op;
     cpu->R[REG_POS(i,12)] = val;     
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//----------------------STR--------------------------------

static u32 FASTCALL  OP_STR_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);

//	 execute = false;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_P_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STR_M_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE32(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT32[cpu->proc_ID][(adr>>24)&0xF];
}

//-----------------------STRB-------------------------------------

static u32 FASTCALL  OP_STRB_P_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_LSL_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_LSR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_ASR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_ROR_IMM_OFF(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] + IMM_OFF_12;
     WRITE8(cpu->mem_if->data, adr, cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)] - IMM_OFF_12;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_LSL_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_LSR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_ASR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] + shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_ROR_IMM_OFF_PREIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)] - shift_op;
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_P_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRB_M_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 adr;
     u32 shift_op;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//-----------------------LDRBT-------------------------------------

static u32 FASTCALL  OP_LDRBT_P_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;     
     u32 i;
     u32 adr;
     u32 val;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     oldmode = armcpu_switchMode(cpu, SYS);
     
     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     
     armcpu_switchMode(cpu, oldmode);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_M_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     oldmode = armcpu_switchMode(cpu, SYS);

     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_M_IMM_OFF_POSTIND\n");
     
     
     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
     
     armcpu_switchMode(cpu, oldmode);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_P_REG_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_P_REG_OFF_POSTIND");
          

     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr + cpu->R[REG_POS(i,0)];
     
     armcpu_switchMode(cpu, oldmode);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_P_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_P_LSL_IMM_OFF_POSTIND");  
          

     i = cpu->instruction;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_M_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_M_LSL_IMM_OFF_POSTIND");  
          

     i = cpu->instruction;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_P_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_P_LSR_IMM_OFF_POSTIND");  
          

     i = cpu->instruction;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);
     
     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_M_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_M_LSR_IMM_OFF_POSTIND");
          

     i = cpu->instruction;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_P_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_P_ASR_IMM_OFF_POSTIND");
          

     i = cpu->instruction;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_M_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_M_ASR_IMM_OFF_POSTIND");
          

     i = cpu->instruction;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_P_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_P_ROR_IMM_OFF_POSTIND");
          

     i = cpu->instruction;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_LDRBT_M_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 val;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDRBT_M_ROR_IMM_OFF_POSTIND");
          

     i = cpu->instruction;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     val = READ8(cpu->mem_if->data, adr);
     cpu->R[REG_POS(i,12)] = val;
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 3 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

//----------------------STRBT----------------------------

static u32 FASTCALL  OP_STRBT_P_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + IMM_OFF_12;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_M_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - IMM_OFF_12;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_P_REG_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + cpu->R[REG_POS(i,0)];
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_M_REG_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - cpu->R[REG_POS(i,0)];
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_P_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_M_LSL_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     LSL_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_P_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_M_LSR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     LSR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_P_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_M_ASR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     ASR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_P_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr + shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
}

static u32 FASTCALL  OP_STRBT_M_ROR_IMM_OFF_POSTIND(armcpu_t *cpu)
{
     u32 oldmode;
     u32 i;
     u32 adr;
     u32 shift_op;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     oldmode = armcpu_switchMode(cpu, SYS);
          

     i = cpu->instruction;
     ROR_IMM; 
     adr = cpu->R[REG_POS(i,16)];
     WRITE8(cpu->mem_if->data, adr, (u8)cpu->R[REG_POS(i,12)]);
     cpu->R[REG_POS(i,16)] = adr - shift_op;
     
     armcpu_switchMode(cpu, oldmode);

     return 2 + MMU.MMU_WAIT16[cpu->proc_ID][(adr>>24)&0xF];
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

static u32 FASTCALL  OP_LDMIA(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
     u32 start = cpu->R[REG_POS(i,16)];
     
     u32 * registres = cpu->R;
     u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     OP_L_IA(0, start);
     OP_L_IA(1, start);
     OP_L_IA(2, start);
     OP_L_IA(3, start);
     OP_L_IA(4, start);
     OP_L_IA(5, start);
     OP_L_IA(6, start);
     OP_L_IA(7, start);
     OP_L_IA(8, start);
     OP_L_IA(9, start);
     OP_L_IA(10, start);
     OP_L_IA(11, start);
     OP_L_IA(12, start);
     OP_L_IA(13, start);
     OP_L_IA(14, start);
     
     if(BIT15(i))
     {
          u32 tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR.bits.T = BIT0(tmp);
          //start += 4;
	      cpu->next_instruction = registres[15];
          c += waitState[(start>>24)&0xF];
     }
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMIB(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
     u32 start = cpu->R[REG_POS(i,16)];
     
     u32 * registres = cpu->R;
     u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     OP_L_IB(0, start);
     OP_L_IB(1, start);
     OP_L_IB(2, start);
     OP_L_IB(3, start);
     OP_L_IB(4, start);
     OP_L_IB(5, start);
     OP_L_IB(6, start);
     OP_L_IB(7, start);
     OP_L_IB(8, start);
     OP_L_IB(9, start);
     OP_L_IB(10, start);
     OP_L_IB(11, start);
     OP_L_IB(12, start);
     OP_L_IB(13, start);
     OP_L_IB(14, start);
     
     if(BIT15(i))
     {
          u32 tmp;
          start += 4;
          c += waitState[(start>>24)&0xF];
          tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR.bits.T = BIT0(tmp);
	      cpu->next_instruction = registres[15];
          c += 2 + (c==0);
     }
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMDA(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
     u32 start = cpu->R[REG_POS(i,16)];
     
     u32 * registres = cpu->R;
     u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     if(BIT15(i))
     {
          u32 tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR.bits.T = BIT0(tmp);
          c += waitState[(start>>24)&0xF];
          start -= 4;
	      cpu->next_instruction = registres[15];
     }

     OP_L_DA(14, start);
     OP_L_DA(13, start);
     OP_L_DA(12, start);
     OP_L_DA(11, start);
     OP_L_DA(10, start);
     OP_L_DA(9, start);
     OP_L_DA(8, start);
     OP_L_DA(7, start);
     OP_L_DA(6, start);
     OP_L_DA(5, start);
     OP_L_DA(4, start);
     OP_L_DA(3, start);
     OP_L_DA(2, start);
     OP_L_DA(1, start);
     OP_L_DA(0, start);
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMDB(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
     u32 start = cpu->R[REG_POS(i,16)];
     
     u32 * registres = cpu->R;
     u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     if(BIT15(i))
     {
          u32 tmp;
          start -= 4;
          tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR.bits.T = BIT0(tmp);
	      cpu->next_instruction = registres[15];
          c += waitState[(start>>24)&0xF];
     }

     OP_L_DB(14, start);
     OP_L_DB(13, start);
     OP_L_DB(12, start);
     OP_L_DB(11, start);
     OP_L_DB(10, start);
     OP_L_DB(9, start);
     OP_L_DB(8, start);
     OP_L_DB(7, start);
     OP_L_DB(6, start);
     OP_L_DB(5, start);
     OP_L_DB(4, start);
     OP_L_DB(3, start);
     OP_L_DB(2, start);
     OP_L_DB(1, start);
     OP_L_DB(0, start);
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMIA_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, count;
     u32 start = cpu->R[REG_POS(i,16)];
	 u32 bitList = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
     
     u32 * registres = cpu->R;
     u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     OP_L_IA(0, start);
     OP_L_IA(1, start);
     OP_L_IA(2, start);
     OP_L_IA(3, start);
     OP_L_IA(4, start);
     OP_L_IA(5, start);
     OP_L_IA(6, start);
     OP_L_IA(7, start);
     OP_L_IA(8, start);
     OP_L_IA(9, start);
     OP_L_IA(10, start);
     OP_L_IA(11, start);
     OP_L_IA(12, start);
     OP_L_IA(13, start);
     OP_L_IA(14, start);
     
     if(BIT15(i))
     {
          u32 tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR.bits.T = BIT0(tmp);
          c += waitState[(start>>24)&0xF];
          start += 4;
	      cpu->next_instruction = registres[15];
     }

	if(i & (1 << REG_POS(i,16))) {
		if(i & bitList)
			cpu->R[REG_POS(i,16)] = start;
	}
	else
		cpu->R[REG_POS(i,16)] = start;

     return c + 2;
}

static u32 FASTCALL  OP_LDMIB_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, count;
     u32 start = cpu->R[REG_POS(i,16)];
	 u32 bitList = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
     
     u32 * registres = cpu->R;
     u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];

	 OP_L_IB(0, start);
     OP_L_IB(1, start);
     OP_L_IB(2, start);
     OP_L_IB(3, start);
     OP_L_IB(4, start);
     OP_L_IB(5, start);
     OP_L_IB(6, start);
     OP_L_IB(7, start);
     OP_L_IB(8, start);
     OP_L_IB(9, start);
     OP_L_IB(10, start);
     OP_L_IB(11, start);
     OP_L_IB(12, start);
     OP_L_IB(13, start);
     OP_L_IB(14, start);
     
     if(BIT15(i))
     {
          u32 tmp;
          start += 4;
          c += waitState[(start>>24)&0xF];
          tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR.bits.T = BIT0(tmp);
	      cpu->next_instruction = registres[15];
          c += 2 + (c==0);
     }

	if(i & (1 << REG_POS(i,16))) {
		if(i & bitList)
			cpu->R[REG_POS(i,16)] = start;
	}
	else
		cpu->R[REG_POS(i,16)] = start;
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMDA_W(armcpu_t *cpu)
{
	u32 i = cpu->instruction, c = 0, count;
	u32 start = cpu->R[REG_POS(i,16)];
	u32 bitList = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;

	u32 * registres = cpu->R;
	u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];

	if(BIT15(i))
	{
		u32 tmp = READ32(cpu->mem_if->data, start);
		registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
		cpu->CPSR.bits.T = BIT0(tmp);
		c += waitState[(start>>24)&0xF];
		start -= 4;
		cpu->next_instruction = registres[15];
	}

	OP_L_DA(14, start);
	OP_L_DA(13, start);
	OP_L_DA(12, start);
	OP_L_DA(11, start);
	OP_L_DA(10, start);
	OP_L_DA(9, start);
	OP_L_DA(8, start);
	OP_L_DA(7, start);
	OP_L_DA(6, start);
	OP_L_DA(5, start);
	OP_L_DA(4, start);
	OP_L_DA(3, start);
	OP_L_DA(2, start);
	OP_L_DA(1, start);
	OP_L_DA(0, start);

	if(i & (1 << REG_POS(i,16))) {
		if(i & bitList)
			cpu->R[REG_POS(i,16)] = start;
	}
	else
		cpu->R[REG_POS(i,16)] = start;
	
	return c + 2;
}

static u32 FASTCALL  OP_LDMDB_W(armcpu_t *cpu)
{
	u32 i = cpu->instruction, c = 0, count;
	u32 start = cpu->R[REG_POS(i,16)];
	u32 bitList = (~((2 << REG_POS(i,16))-1)) & 0xFFFF;
	u32 * registres = cpu->R;
	u32 * waitState = MMU.MMU_WAIT32[cpu->proc_ID];

	if(BIT15(i))
	{
		u32 tmp;
		start -= 4;
		tmp = READ32(cpu->mem_if->data, start);
		registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
		cpu->CPSR.bits.T = BIT0(tmp);
		cpu->next_instruction = registres[15];
		c += waitState[(start>>24)&0xF];
	}

	OP_L_DB(14, start);
	OP_L_DB(13, start);
	OP_L_DB(12, start);
	OP_L_DB(11, start);
	OP_L_DB(10, start);
	OP_L_DB(9, start);
	OP_L_DB(8, start);
	OP_L_DB(7, start);
	OP_L_DB(6, start);
	OP_L_DB(5, start);
	OP_L_DB(4, start);
	OP_L_DB(3, start);
	OP_L_DB(2, start);
	OP_L_DB(1, start);
	OP_L_DB(0, start);

	if(i & (1 << REG_POS(i,16))) {
		if(i & bitList)
			cpu->R[REG_POS(i,16)] = start;
	}
	else
		cpu->R[REG_POS(i,16)] = start;

	return c + 2;
}

static u32 FASTCALL  OP_LDMIA2(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 oldmode;
     
     u32 c = 0;
     
     u32 start = cpu->R[REG_POS(i,16)];
     u32 * registres;
     u32 * waitState;

     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 1;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
               
     OP_L_IA(0, start);
     OP_L_IA(1, start);
     OP_L_IA(2, start);
     OP_L_IA(3, start);
     OP_L_IA(4, start);
     OP_L_IA(5, start);
     OP_L_IA(6, start);
     OP_L_IA(7, start);
     OP_L_IA(8, start);
     OP_L_IA(9, start);
     OP_L_IA(10, start);
     OP_L_IA(11, start);
     OP_L_IA(12, start);
     OP_L_IA(13, start);
     OP_L_IA(14, start);
     
     if(BIT15(i))
     {
          u32 tmp = READ32(cpu->mem_if->data, start);
          Status_Reg SPSR;
          cpu->R[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
          //start += 4;
	      cpu->next_instruction = cpu->R[15];
          c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
     }
     else
     {
         armcpu_switchMode(cpu, oldmode);
     }
     return c + 2;
}

static u32 FASTCALL  OP_LDMIB2(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 oldmode;
     u32 c = 0;
     
     u32 start = cpu->R[REG_POS(i,16)];
     u32 * registres;
     u32 * waitState;
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDMIB2");

     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
               
     OP_L_IB(0, start);
     OP_L_IB(1, start);
     OP_L_IB(2, start);
     OP_L_IB(3, start);
     OP_L_IB(4, start);
     OP_L_IB(5, start);
     OP_L_IB(6, start);
     OP_L_IB(7, start);
     OP_L_IB(8, start);
     OP_L_IB(9, start);
     OP_L_IB(10, start);
     OP_L_IB(11, start);
     OP_L_IB(12, start);
     OP_L_IB(13, start);
     OP_L_IB(14, start);
     
     if(BIT15(i))
     {
          u32 tmp;
          Status_Reg SPSR;
          start += 4;
          tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
	      cpu->next_instruction = registres[15];
          c += waitState[(start>>24)&0xF];
     }
     else
     {
         armcpu_switchMode(cpu, oldmode);
     }
     return c + 2;
}

static u32 FASTCALL  OP_LDMDA2(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
          
     u32 oldmode;
     u32 c = 0;
     u32 * registres;
     u32 * waitState;
     
     u32 start = cpu->R[REG_POS(i,16)];
     //execute = FALSE;
	 LOG("Untested opcode: OP_LDMDA2");

     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }
     
     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     if(BIT15(i))
     {
          u32 tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR = cpu->SPSR;
          c += waitState[(start>>24)&0xF];
          start -= 4;
	      cpu->next_instruction = registres[15];
     }
 
     OP_L_DA(14, start);
     OP_L_DA(13, start);
     OP_L_DA(12, start);
     OP_L_DA(11, start);
     OP_L_DA(10, start);
     OP_L_DA(9, start);
     OP_L_DA(8, start);
     OP_L_DA(7, start);
     OP_L_DA(6, start);
     OP_L_DA(5, start);
     OP_L_DA(4, start);
     OP_L_DA(3, start);
     OP_L_DA(2, start);
     OP_L_DA(1, start);
     OP_L_DA(0, start);
     
     if(BIT15(i)==0)
     {
          armcpu_switchMode(cpu, oldmode);
     }
     else
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
     }
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMDB2(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
          
     u32 oldmode;
     u32 c = 0;
     u32 * registres;
     u32 * waitState;
     
     u32 start = cpu->R[REG_POS(i,16)];
     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     if(BIT15(i))
     {
          u32 tmp;
          start -= 4;
          tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR = cpu->SPSR;
	      cpu->next_instruction = registres[15];
          c += waitState[(start>>24)&0xF];
     }

     OP_L_DB(14, start);
     OP_L_DB(13, start);
     OP_L_DB(12, start);
     OP_L_DB(11, start);
     OP_L_DB(10, start);
     OP_L_DB(9, start);
     OP_L_DB(8, start);
     OP_L_DB(7, start);
     OP_L_DB(6, start);
     OP_L_DB(5, start);
     OP_L_DB(4, start);
     OP_L_DB(3, start);
     OP_L_DB(2, start);
     OP_L_DB(1, start);
     OP_L_DB(0, start);
     
     if(BIT15(i)==0)
     {
          armcpu_switchMode(cpu, oldmode);
     }
     else
     {
          Status_Reg SPSR = cpu->SPSR;
          armcpu_switchMode(cpu, SPSR.bits.mode);
          cpu->CPSR=SPSR;
     }
     
     return 2 + c;
}

static u32 FASTCALL  OP_LDMIA2_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
          
     u32 oldmode;
     u32 start = cpu->R[REG_POS(i,16)];
     u32 * registres;
     u32 * waitState;
     u32 tmp;
     Status_Reg SPSR;
//     execute = FALSE;     
     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     OP_L_IA(0, start);
     OP_L_IA(1, start);
     OP_L_IA(2, start);
     OP_L_IA(3, start);
     OP_L_IA(4, start);
     OP_L_IA(5, start);
     OP_L_IA(6, start);
     OP_L_IA(7, start);
     OP_L_IA(8, start);
     OP_L_IA(9, start);
     OP_L_IA(10, start);
     OP_L_IA(11, start);
     OP_L_IA(12, start);
     OP_L_IA(13, start);
     OP_L_IA(14, start);
     
     if(BIT15(i)==0)
     {
          registres[REG_POS(i,16)] = start;
          armcpu_switchMode(cpu, oldmode);
          return c + 2;
     }

     registres[REG_POS(i,16)] = start + 4;
     tmp = READ32(cpu->mem_if->data, start);
     registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
     SPSR = cpu->SPSR;
     armcpu_switchMode(cpu, SPSR.bits.mode);
     cpu->CPSR=SPSR;
     cpu->next_instruction = registres[15];
     c += waitState[(start>>24)&0xF];

     return c + 2;
}

static u32 FASTCALL  OP_LDMIB2_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
          
     u32 oldmode;
     u32 start = cpu->R[REG_POS(i,16)];
     u32 * registres;
     u32 * waitState;
     u32 tmp;
     Status_Reg SPSR;

     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];

     OP_L_IB(0, start);
     OP_L_IB(1, start);
     OP_L_IB(2, start);
     OP_L_IB(3, start);
     OP_L_IB(4, start);
     OP_L_IB(5, start);
     OP_L_IB(6, start);
     OP_L_IB(7, start);
     OP_L_IB(8, start);
     OP_L_IB(9, start);
     OP_L_IB(10, start);
     OP_L_IB(11, start);
     OP_L_IB(12, start);
     OP_L_IB(13, start);
     OP_L_IB(14, start);
     
     if(BIT15(i)==0)
     {
          armcpu_switchMode(cpu, oldmode);
          registres[REG_POS(i,16)] = start;
          
          return c + 2;
     }

     registres[REG_POS(i,16)] = start + 4;
     tmp = READ32(cpu->mem_if->data, start + 4);
     registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
     cpu->CPSR = cpu->SPSR;
     cpu->next_instruction = registres[15];
     SPSR = cpu->SPSR;
     armcpu_switchMode(cpu, SPSR.bits.mode);
     cpu->CPSR=SPSR;
     c += waitState[(start>>24)&0xF];
     
     return c + 2;
}

static u32 FASTCALL  OP_LDMDA2_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
          
     u32 oldmode;
     u32 start = cpu->R[REG_POS(i,16)];
     u32 * registres;
     u32 * waitState;
     Status_Reg SPSR;
//     execute = FALSE;     
     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     if(BIT15(i))
     {
          u32 tmp = READ32(cpu->mem_if->data, start);
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          c += waitState[(start>>24)&0xF];
          start -= 4;
          cpu->next_instruction = registres[15];
     }

     OP_L_DA(14, start);
     OP_L_DA(13, start);
     OP_L_DA(12, start);
     OP_L_DA(11, start);
     OP_L_DA(10, start);
     OP_L_DA(9, start);
     OP_L_DA(8, start);
     OP_L_DA(7, start);
     OP_L_DA(6, start);
     OP_L_DA(5, start);
     OP_L_DA(4, start);
     OP_L_DA(3, start);
     OP_L_DA(2, start);
     OP_L_DA(1, start);
     OP_L_DA(0, start);
     
     registres[REG_POS(i,16)] = start;
     
     if(BIT15(i)==0)
     {
          armcpu_switchMode(cpu, oldmode);
          return c + 2;
     }

     SPSR = cpu->SPSR;
     armcpu_switchMode(cpu, SPSR.bits.mode);
     cpu->CPSR=SPSR;
     return c + 2;
}

static u32 FASTCALL  OP_LDMDB2_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 c = 0;
          
     u32 oldmode;
     u32 start = cpu->R[REG_POS(i,16)];
     u32 * registres;
     u32 * waitState;
     Status_Reg SPSR;
//     execute = FALSE;     
     if(BIT15(i)==0)
     {  
          if(cpu->CPSR.bits.mode==USR)
               return 2;
          oldmode = armcpu_switchMode(cpu, SYS);
     }

     registres = cpu->R;
     waitState = MMU.MMU_WAIT32[cpu->proc_ID];
     
     if(BIT15(i))
     {
          u32 tmp;
          start -= 4;
          tmp = READ32(cpu->mem_if->data, start);
          c += waitState[(start>>24)&0xF];
          registres[15] = tmp & (0XFFFFFFFC | (BIT0(tmp)<<1));
          cpu->CPSR = cpu->SPSR;
          cpu->next_instruction = registres[15];
     }

     OP_L_DB(14, start);
     OP_L_DB(13, start);
     OP_L_DB(12, start);
     OP_L_DB(11, start);
     OP_L_DB(10, start);
     OP_L_DB(9, start);
     OP_L_DB(8, start);
     OP_L_DB(7, start);
     OP_L_DB(6, start);
     OP_L_DB(5, start);
     OP_L_DB(4, start);
     OP_L_DB(3, start);
     OP_L_DB(2, start);
     OP_L_DB(1, start);
     OP_L_DB(0, start);
     
     registres[REG_POS(i,16)] = start;
     
     if(BIT15(i)==0)
     {
          armcpu_switchMode(cpu, oldmode);
          return c + 2;
     }

     SPSR = cpu->SPSR;
     armcpu_switchMode(cpu, SPSR.bits.mode);
     cpu->CPSR=SPSR;
     return c + 2;
}

//------------------------------STM----------------------------------

static u32 FASTCALL  OP_STMIA(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start += 4;
          }
     }
     return c + 1;
}

static u32 FASTCALL  OP_STMIB(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               start += 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }
     return c + 1;
}

static u32 FASTCALL  OP_STMDA(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start -= 4;
          }
     }     
     return c + 1;
}

static u32 FASTCALL  OP_STMDB(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               start -= 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }     
     return c + 1;
}

static u32 FASTCALL  OP_STMIA_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start += 4;
          }
     }
     
     cpu->R[REG_POS(i,16)] = start;
     return c + 1;
}

static u32 FASTCALL  OP_STMIB_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               start += 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }
     cpu->R[REG_POS(i,16)] = start;
     return c + 1;
}

static u32 FASTCALL  OP_STMDA_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start -= 4;
          }
     }     
     
     cpu->R[REG_POS(i,16)] = start;
     return c + 1;
}

static u32 FASTCALL  OP_STMDB_W(armcpu_t *cpu)
{
     u32 i = cpu->instruction, c = 0, b;
     u32 start = cpu->R[REG_POS(i,16)];
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               start -= 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }     
     
     cpu->R[REG_POS(i,16)] = start;
     return c + 1;
}

static u32 FASTCALL  OP_STMIA2(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     i = cpu->instruction;
     c = 0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);

     //execute = FALSE;
	 LOG("Untested opcode: OP_STMIA2");

     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start += 4;
          }
     }
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

static u32 FASTCALL  OP_STMIB2(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     i = cpu->instruction;
     c = 0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);
     
     //execute = FALSE;
	 LOG("Untested opcode: OP_STMIB2");
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               start += 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

static u32 FASTCALL  OP_STMDA2(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;

     i = cpu->instruction;
     c = 0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);     
     
     //execute = FALSE;
	 LOG("Untested opcode: OP_STMDA2");  
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start -= 4;
          }
     }     
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

static u32 FASTCALL  OP_STMDB2(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     i = cpu->instruction;
     c=0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               start -= 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }     
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

static u32 FASTCALL  OP_STMIA2_W(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     
     i = cpu->instruction;
     c=0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);
     
     //execute = FALSE;
	 LOG("Untested opcode: OP_STMIA2_W");
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start += 4;
          }
     }
     
     cpu->R[REG_POS(i,16)] = start;
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

static u32 FASTCALL  OP_STMIB2_W(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
     i = cpu->instruction;
     c=0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, b))
          {
               start += 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }
     armcpu_switchMode(cpu, oldmode);
     cpu->R[REG_POS(i,16)] = start;
     
     return c + 1;
}

static u32 FASTCALL  OP_STMDA2_W(armcpu_t *cpu)
{
     u32 i, c, b;
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     i = cpu->instruction;
     c = 0;
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);
     //execute = FALSE;
	 LOG("Untested opcode: OP_STMDA2_W");
     
     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
               start -= 4;
          }
     }     
     
     cpu->R[REG_POS(i,16)] = start;
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

static u32 FASTCALL  OP_STMDB2_W(armcpu_t *cpu)
{
     u32 i, c, b;     
     u32 start;
     u32 oldmode;

     if(cpu->CPSR.bits.mode==USR)
          return 2;
          
     i = cpu->instruction;
     c = 0;
     
     start = cpu->R[REG_POS(i,16)];
     oldmode = armcpu_switchMode(cpu, SYS);

     //execute = FALSE;
	 LOG("Untested opcode: OP_STMDB2_W");   

     for(b=0; b<16; ++b)
     {
          if(BIT_N(i, 15-b))
          {
               start -= 4;
               WRITE32(cpu->mem_if->data, start, cpu->R[15-b]);
               c += MMU.MMU_WAIT32[cpu->proc_ID][(start>>24)&0xF];
          }
     }     
     
     cpu->R[REG_POS(i,16)] = start;
     
     armcpu_switchMode(cpu, oldmode);
     return c + 1;
}

/*
 *
 * The Enhanced DSP Extension LDRD and STRD instructions.
 *
 */
static u32 FASTCALL
OP_LDRD_STRD_POST_INDEX( armcpu_t *cpu) {
  u32 i = cpu->instruction;
  u32 Rd_num = REG_POS( i, 12);
  u32 addr = cpu->R[REG_POS(i,16)];
  u32 index;

  /* I bit - immediate or register */
  if ( BIT22(i))
    index = IMM_OFF;
  else
    index = cpu->R[REG_POS(i,0)];

  /* U bit - add or subtract */
  if ( BIT23(i))
    cpu->R[REG_POS(i,16)] += index;
  else
    cpu->R[REG_POS(i,16)] -= index;

  if ( !(Rd_num & 0x1)) {
    /* Store/Load */
    if ( BIT5(i)) {
      WRITE32(cpu->mem_if->data, addr, cpu->R[Rd_num]);
      WRITE32(cpu->mem_if->data, addr + 4, cpu->R[Rd_num + 1]);
    }
    else {
      cpu->R[Rd_num] = READ32(cpu->mem_if->data, addr);
      cpu->R[Rd_num + 1] = READ32(cpu->mem_if->data, addr + 4);
    }
  }

  return 3 + (MMU.MMU_WAIT32[cpu->proc_ID][(addr>>24)&0xF] * 2);
}

static u32 FASTCALL
OP_LDRD_STRD_OFFSET_PRE_INDEX( armcpu_t *cpu) {
  u32 i = cpu->instruction;
  u32 Rd_num = REG_POS( i, 12);
  u32 addr = cpu->R[REG_POS(i,16)];
  u32 index;

  /* I bit - immediate or register */
  if ( BIT22(i))
    index = IMM_OFF;
  else
    index = cpu->R[REG_POS(i,0)];

  /* U bit - add or subtract */
  if ( BIT23(i)) {
    addr += index;

    /* W bit - writeback */
    if ( BIT21(i))
      cpu->R[REG_POS(i,16)] = addr;
  }
  else {
    addr -= index;

    /* W bit - writeback */
    if ( BIT21(i))
      cpu->R[REG_POS(i,16)] = addr;
  }

  if ( !(Rd_num & 0x1)) {
    /* Store/Load */
    if ( BIT5(i)) {
      WRITE32(cpu->mem_if->data, addr, cpu->R[Rd_num]);
      WRITE32(cpu->mem_if->data, addr + 4, cpu->R[Rd_num + 1]);
    }
    else {
      cpu->R[Rd_num] = READ32(cpu->mem_if->data, addr);
      cpu->R[Rd_num + 1] = READ32(cpu->mem_if->data, addr + 4);
    }
  }

  return 3 + (MMU.MMU_WAIT32[cpu->proc_ID][(addr>>24)&0xF] * 2);
}



//---------------------STC----------------------------------

static u32 FASTCALL  OP_STC_P_IMM_OFF(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_STC_M_IMM_OFF(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_STC_P_PREIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_STC_M_PREIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_STC_P_POSTIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_STC_M_POSTIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_STC_OPTION(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a STC, no feedback is given to the arm */
          return 2;
     }
}

//---------------------LDC----------------------------------

static u32 FASTCALL  OP_LDC_P_IMM_OFF(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_LDC_M_IMM_OFF(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_LDC_P_PREIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_LDC_M_PREIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_LDC_P_POSTIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_LDC_M_POSTIND(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

static u32 FASTCALL  OP_LDC_OPTION(armcpu_t *cpu)
{
     {
		/* the NDS has no coproc that responses to a LDC, no feedback is given to the arm */
          return 2;
     }
}

//----------------MCR-----------------------

static u32 FASTCALL  OP_MCR(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 cpnum = REG_POS(i, 8);
     
     if(!cpu->coproc[cpnum])
     {
          execute = FALSE;
          return 2;
     }
     
	  armcp15_moveARM2CP((armcp15_t*)cpu->coproc[cpnum], cpu->R[REG_POS(i, 12)], REG_POS(i, 16), REG_POS(i, 0), (i>>21)&7, (i>>5)&7);
			  //cpu->coproc[cpnum]->moveARM2CP(cpu->R[REG_POS(i, 12)], REG_POS(i, 16), REG_POS(i, 0), (i>>21)&7, (i>>5)&7);
     return 2;
}

//----------------MRC-----------------------

static u32 FASTCALL  OP_MRC(armcpu_t *cpu)
{
     u32 i = cpu->instruction;
     u32 cpnum = REG_POS(i, 8);
     
     if(!cpu->coproc[cpnum])
     {
          execute = FALSE;
          return 2;
     }
     
	  armcp15_moveCP2ARM((armcp15_t*)cpu->coproc[cpnum], &cpu->R[REG_POS(i, 12)], REG_POS(i, 16), REG_POS(i, 0), (i>>21)&7, (i>>5)&7);
     //cpu->coproc[cpnum]->moveCP2ARM(&cpu->R[REG_POS(i, 12)], REG_POS(i, 16), REG_POS(i, 0), (i>>21)&7, (i>>5)&7);
     return 4;
}

//--------------SWI-------------------------------
static u32 FASTCALL  OP_SWI(armcpu_t *cpu)
{
     if (((cpu->intVector != 0) ^ (cpu->proc_ID == ARMCPU_ARM9)))
     {
        /* TODO (#1#): translocated SWI vectors */
        /* we use an irq thats not in the irq tab, as
           it was replaced duie to a changed intVector */
        Status_Reg tmp = cpu->CPSR;
        armcpu_switchMode(cpu, SVC);            /* enter svc mode */
        cpu->R[14] = cpu->R[15] - 4;            /* jump to swi Vector */
        cpu->SPSR = tmp;                        /* save old CPSR as new SPSR */
        cpu->CPSR.bits.T = 0;                   /* handle as ARM32 code */
        cpu->CPSR.bits.I = cpu->SPSR.bits.I;    /* keep int disable flag */
        cpu->R[15] = cpu->intVector + 0x08;
        cpu->next_instruction = cpu->R[15];
        return 4;
     }
     else
     {
        u32 swinum = (cpu->instruction>>16)&0x1F;
        return cpu->swi_tab[swinum](cpu) + 3;
     }
}

//----------------BKPT-------------------------
static u32 FASTCALL OP_BKPT(armcpu_t *cpu)
{
     execute = FALSE;
     return 4;
}

//----------------CDP-----------------------

static u32 FASTCALL  OP_CDP(armcpu_t *cpu)
{
     execute = FALSE;
     return 4;
}

#define TYPE_RETOUR u32
#define PARAMETRES  armcpu_t *cpu
#define CALLTYPE    FASTCALL 
#define NOM_TAB     arm_instructions_set

#include "instruction_tabdef.inc"
