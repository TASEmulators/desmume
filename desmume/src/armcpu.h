/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

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

#ifndef ARM_CPU
#define ARM_CPU

#include "types.h"

#include "ICoProc.hpp"

#include "arm_instructions.hpp"
#include "thumb_instructions.hpp"
#include "MMU.hpp"
#include "CP15.hpp"
#include "bios.hpp"

#define BIT_N(i,n)  (((i)>>(n))&1)
#define CONDITION(i)  (i)>>28
#define CODE(i)     (((i)>>25)&0X7)
#define OPCODE(i)   (((i)>>21)&0xF)
#define SIGNEBIT(i) BIT_N(i,20)
#define BIT0(i)     ((i)&1)
#define BIT1(i)     BIT_N(i,1)
#define BIT2(i)     BIT_N(i,2)
#define BIT3(i)     BIT_N(i,3)
#define BIT4(i)     BIT_N(i,4)
#define BIT5(i)     BIT_N(i,5)
#define BIT6(i)     BIT_N(i,6)
#define BIT7(i)     BIT_N(i,7)
#define BIT8(i)     BIT_N(i,8)
#define BIT9(i)     BIT_N(i,9)
#define BIT10(i)     BIT_N(i,10)
#define BIT11(i)     BIT_N(i,11)
#define BIT12(i)     BIT_N(i,12)
#define BIT13(i)     BIT_N(i,13)
#define BIT14(i)     BIT_N(i,14)
#define BIT15(i)     BIT_N(i,15)
#define BIT16(i)     BIT_N(i,16)
#define BIT17(i)     BIT_N(i,17)
#define BIT18(i)     BIT_N(i,18)
#define BIT19(i)     BIT_N(i,19)
#define BIT20(i)     BIT_N(i,20)
#define BIT21(i)     BIT_N(i,21)
#define BIT22(i)     BIT_N(i,22)
#define BIT23(i)     BIT_N(i,23)
#define BIT24(i)     BIT_N(i,24)
#define BIT25(i)     BIT_N(i,25)
#define BIT26(i)     BIT_N(i,26)
#define BIT27(i)     BIT_N(i,27)
#define BIT28(i)     BIT_N(i,28)
#define BIT29(i)     BIT_N(i,29)
#define BIT30(i)     BIT_N(i,30)
#define BIT31(i)    ((i)>>31)
 
#define INSTRUCTION_INDEX(i) ((((i)>>16)&0xFF0)|(((i)>>4)&0xF))

#define REG_POS(i,n)         (((i)>>n)&0xF)

#define ROR(i, j)   ((((unsigned long)(i))>>(j)) | (((unsigned long)(i))<<(32-(j))))

#define UNSIGNED_OVERFLOW(a,b,c) ((BIT31(a)&BIT31(b)) | \
								  ((BIT31(a)|BIT31(b))&BIT31(~c)))

#define UNSIGNED_UNDERFLOW(a,b,c) ((BIT31(~a)&BIT31(b)) | \
								  ((BIT31(~a)|BIT31(b))&BIT31(c)))

#define SIGNED_OVERFLOW(a,b,c) ((BIT31(a)&BIT31(b)&BIT31(~c))|\
								(BIT31(~a)&BIT31(~(b))&BIT31(c)))

#define SIGNED_UNDERFLOW(a,b,c) ((BIT31(a)&BIT31(~(b))&BIT31(~c))|\
								(BIT31(~a)&BIT31(b)&BIT31(c)))

#define EQ	0x0
#define NE	0x1
#define CS	0x2
#define CC	0x3
#define MI	0x4
#define PL	0x5
#define VS	0x6
#define VC	0x7
#define HI	0x8
#define LS	0x9
#define GE	0xA
#define LT	0xB
#define GT	0xC
#define LE	0xD
#define AL	0xE	

#define TEST_COND(cond, CPSR)	(((cond)==AL) ||\
					 (((cond)==EQ) && ( CPSR.bits.Z))||\
					 (((cond)==NE) && (!CPSR.bits.Z))||\
					 (((cond)==CS) && ( CPSR.bits.C))||\
					 (((cond)==CC) && (!CPSR.bits.C))||\
					 (((cond)==MI) && ( CPSR.bits.N))||\
					 (((cond)==PL) && (!CPSR.bits.N))||\
					 (((cond)==VS) && ( CPSR.bits.V))||\
					 (((cond)==VC) && (!CPSR.bits.V))||\
					 (((cond)==HI) && (CPSR.bits.C) && (!CPSR.bits.Z))||\
					 (((cond)==LS) && ((CPSR.bits.Z) || (!CPSR.bits.C)))||\
					 (((cond)==GE) && (CPSR.bits.N==CPSR.bits.V))||\
					 (((cond)==LT) && (CPSR.bits.N!=CPSR.bits.V))||\
					 (((cond)==GT) && (CPSR.bits.Z==0) && (CPSR.bits.N==CPSR.bits.V))||\
					 (((cond)==LE) && ((CPSR.bits.Z) || (CPSR.bits.N!=CPSR.bits.V))))

enum Mode
{
	USR = 0x10,
	FIQ = 0x11,
	IRQ = 0x12,
	SVC = 0x13,
	ABT = 0x17,
	UND = 0x1B,
	SYS = 0x1F
};

extern bool execute;

union Status_Reg
{
	struct 
	{
		unsigned long mode : 5,
		T : 1,
		F : 1,
		I : 1,
		RAZ : 19,
		Q : 1,
		V : 1,
		C : 1, 
		Z : 1, 
		N : 1;
	} bits;
	unsigned long val;
};

typedef struct
{
	unsigned long proc_ID;
	unsigned long instruction; //4
	unsigned long instruct_adr; //8
	unsigned long next_instruction; //12
       
	unsigned long R[16]; //16
	Status_Reg CPSR;  //80
	Status_Reg SPSR;
       
	unsigned long R13_usr, R14_usr;
	unsigned long R13_svc, R14_svc;
	unsigned long R13_abt, R14_abt;
	unsigned long R13_und, R14_und;
	unsigned long R13_irq, R14_irq;
	unsigned long R8_fiq, R9_fiq, R10_fiq, R11_fiq, R12_fiq, R13_fiq, R14_fiq;
	Status_Reg SPSR_svc, SPSR_abt, SPSR_und, SPSR_irq, SPSR_fiq;
	
	ICoProc * coproc[16];
       
	unsigned long intVector;
	unsigned char LDTBit;  //1 : ARMv5 style 0 : non ARMv5
	bool waitIRQ;
	bool wIRQ;
	bool wirq;


	unsigned long (* *swi_tab)(ARMCPU * cpu);
	
} armcpu_t;
	
void armcpu_init(armcpu_t *armcpu, unsigned long adr);
unsigned long armcpu_switchMode(armcpu_t *armcpu, unsigned char mode);

inline unsigned long armcpu_prefetch(armcpu_t *armcpu)
{
	if(armcpu->CPSR.bits.T == 0)
	{
		armcpu->instruction = MMU::readWord(armcpu->proc_ID, armcpu->next_instruction);
		armcpu->instruct_adr = armcpu->next_instruction;
		armcpu->next_instruction += 4;
		armcpu->R[15] = armcpu->next_instruction + 4;
		return MMU::MMU_WAIT32[armcpu->proc_ID][(armcpu->instruct_adr>>24)&0xF];
	}
	armcpu->instruction = MMU::readHWord(armcpu->proc_ID, armcpu->next_instruction);
	armcpu->instruct_adr = armcpu->next_instruction;
	armcpu->next_instruction = armcpu->next_instruction + 2;
	armcpu->R[15] = armcpu->next_instruction + 2;
	return MMU::MMU_WAIT16[armcpu->proc_ID][(armcpu->instruct_adr>>24)&0xF];
}
 
inline unsigned long armcpu_exec(armcpu_t *armcpu)
{
	unsigned long c = 1;
	if(armcpu->CPSR.bits.T == 0)
	{
		if((TEST_COND(CONDITION(armcpu->instruction), armcpu->CPSR)) || ((CONDITION(armcpu->instruction)==0xF)&&(CODE(armcpu->instruction)==0x5)))
		{
			c = arm_instructions_set[INSTRUCTION_INDEX(armcpu->instruction)](armcpu);
		}
		c += armcpu_prefetch(armcpu);
		return c;
	}
	c = thumb_instructions_set[armcpu->instruction>>6](armcpu);
	c += armcpu_prefetch(armcpu);
	return c;
}

inline bool irqExeption(armcpu_t *armcpu)
{
	if(armcpu->CPSR.bits.I) return false;
	Status_Reg tmp = armcpu->CPSR;
	armcpu_switchMode(armcpu, IRQ);
	armcpu->R[14] = armcpu->instruct_adr + 4;
	armcpu->SPSR = tmp;
	armcpu->CPSR.bits.T = 0;
	armcpu->CPSR.bits.I = 1;
	armcpu->next_instruction = armcpu->intVector + 0x18;
	armcpu->R[15] = armcpu->next_instruction;
	armcpu->waitIRQ = 0;
	armcpu_prefetch(armcpu);
	return true;
}

inline bool prefetchExeption(armcpu_t *armcpu)
{
	if(armcpu->CPSR.bits.I) return false;
	Status_Reg tmp = armcpu->CPSR;
	armcpu_switchMode(armcpu, ABT);
	armcpu->R[14] = armcpu->instruct_adr + 4;
	armcpu->SPSR = tmp;
	armcpu->CPSR.bits.T = 0;
	armcpu->CPSR.bits.I = 1;
	armcpu->next_instruction = armcpu->intVector + 0xC;
	armcpu->R[15] = armcpu->next_instruction;
	armcpu->waitIRQ = 0;
	armcpu_prefetch(armcpu);
	return true;
}



#endif 
