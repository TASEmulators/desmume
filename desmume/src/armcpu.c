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

#include "arm_instructions.h"
#include "thumb_instructions.h"
#include "cp15.h"
#include "bios.h"
#include <stdlib.h>
#include <stdio.h>

armcpu_t NDS_ARM7;
armcpu_t NDS_ARM9;

#define SWAP(a, b, c) do      \
	              {       \
                         c=a; \
                         a=b; \
                         b=c; \
		      }       \
                      while(0)
                      
int armcpu_new(armcpu_t *armcpu, u32 id)
{
	
	armcpu->proc_ID = id;
	
	if(id==0) armcpu->swi_tab = ARM9_swi_tab;
	else armcpu->swi_tab = ARM7_swi_tab;
	
	armcpu_init(armcpu, 0);
	
	return 0;
} 

void armcpu_init(armcpu_t *armcpu, u32 adr)
{
   u32 i;

	armcpu->LDTBit = (armcpu->proc_ID==0); //Si ARM9 utiliser le syte v5 pour le load
	armcpu->intVector = 0xFFFF0000 * (armcpu->proc_ID==0);
	armcpu->waitIRQ = FALSE;
	armcpu->wirq = FALSE;
	
	if(armcpu->coproc[15]) free(armcpu->coproc[15]);
	
   for(i = 0; i < 15; ++i)
	{
		armcpu->R[i] = 0;
		armcpu->coproc[i] = NULL;
   }
	
	armcpu->CPSR.val = armcpu->SPSR.val = SYS;
	
	armcpu->R13_usr = armcpu->R14_usr = 0;
	armcpu->R13_svc = armcpu->R14_svc = 0;
	armcpu->R13_abt = armcpu->R14_abt = 0;
	armcpu->R13_und = armcpu->R14_und = 0;
	armcpu->R13_irq = armcpu->R14_irq = 0;
	armcpu->R8_fiq = armcpu->R9_fiq = armcpu->R10_fiq = armcpu->R11_fiq = armcpu->R12_fiq = armcpu->R13_fiq = armcpu->R14_fiq = 0;
	
	armcpu->SPSR_svc.val = armcpu->SPSR_abt.val = armcpu->SPSR_und.val = armcpu->SPSR_irq.val = armcpu->SPSR_fiq.val = 0;
	armcpu->next_instruction = adr;
	armcpu->R[15] = adr;
	armcpu->coproc[15] = (armcp_t*)armcp15_new(armcpu);
	
	armcpu_prefetch(armcpu);
}

u32 armcpu_switchMode(armcpu_t *armcpu, u8 mode)
{
        u32 oldmode = armcpu->CPSR.bits.mode;
	
	switch(oldmode)
	{
		case USR :
		case SYS :
			armcpu->R13_usr = armcpu->R[13];
			armcpu->R14_usr = armcpu->R[14];
			break;
			
		case FIQ :
			{
                                u32 tmp;
				SWAP(armcpu->R[8], armcpu->R8_fiq, tmp);
				SWAP(armcpu->R[9], armcpu->R9_fiq, tmp);
				SWAP(armcpu->R[10], armcpu->R10_fiq, tmp);
				SWAP(armcpu->R[11], armcpu->R11_fiq, tmp);
				SWAP(armcpu->R[12], armcpu->R12_fiq, tmp);
				armcpu->R13_fiq = armcpu->R[13];
				armcpu->R14_fiq = armcpu->R[14];
				armcpu->SPSR_fiq = armcpu->SPSR;
				break;
			}
		case IRQ :
			armcpu->R13_irq = armcpu->R[13];
			armcpu->R14_irq = armcpu->R[14];
			armcpu->SPSR_irq = armcpu->SPSR;
			break;
			
		case SVC :
			armcpu->R13_svc = armcpu->R[13];
			armcpu->R14_svc = armcpu->R[14];
			armcpu->SPSR_svc = armcpu->SPSR;
			break;
		
		case ABT :
			armcpu->R13_abt = armcpu->R[13];
			armcpu->R14_abt = armcpu->R[14];
			armcpu->SPSR_abt = armcpu->SPSR;
			break;
			
		case UND :
			armcpu->R13_und = armcpu->R[13];
			armcpu->R14_und = armcpu->R[14];
			armcpu->SPSR_und = armcpu->SPSR;
			break;
		default :
			break;
		}
		
		switch(mode)
		{
			case USR :
			case SYS :
				armcpu->R[13] = armcpu->R13_usr;
				armcpu->R[14] = armcpu->R14_usr;
				//SPSR = CPSR;
				break;
				
			case FIQ :
				{
                                        u32 tmp;
					SWAP(armcpu->R[8], armcpu->R8_fiq, tmp);
					SWAP(armcpu->R[9], armcpu->R9_fiq, tmp);
					SWAP(armcpu->R[10], armcpu->R10_fiq, tmp);
					SWAP(armcpu->R[11], armcpu->R11_fiq, tmp);
					SWAP(armcpu->R[12], armcpu->R12_fiq, tmp);
					armcpu->R[13] = armcpu->R13_fiq;
					armcpu->R[14] = armcpu->R14_fiq;
					armcpu->SPSR = armcpu->SPSR_fiq;
					break;
				}
				
			case IRQ :
				armcpu->R[13] = armcpu->R13_irq;
				armcpu->R[14] = armcpu->R14_irq;
				armcpu->SPSR = armcpu->SPSR_irq;
				break;
				
			case SVC :
				armcpu->R[13] = armcpu->R13_svc;
				armcpu->R[14] = armcpu->R14_svc;
				armcpu->SPSR = armcpu->SPSR_svc;
				break;
				
			case ABT :
				armcpu->R[13] = armcpu->R13_abt;
				armcpu->R[14] = armcpu->R14_abt;
				armcpu->SPSR = armcpu->SPSR_abt;
				break;
				
          case UND :
				armcpu->R[13] = armcpu->R13_und;
				armcpu->R[14] = armcpu->R14_und;
				armcpu->SPSR = armcpu->SPSR_und;
				break;
				
				default :
					break;
	}
	
	armcpu->CPSR.bits.mode = mode & 0x1F;
	return oldmode;
}

u32 armcpu_prefetch(armcpu_t *armcpu)
{
	if(armcpu->CPSR.bits.T == 0)
	{
		armcpu->instruction = MMU_readWordACL(armcpu->proc_ID, armcpu->next_instruction,CP15_ACCESS_EXECUTE);
		armcpu->instruct_adr = armcpu->next_instruction;
		armcpu->next_instruction += 4;
		armcpu->R[15] = armcpu->next_instruction + 4;
		return MMU.MMU_WAIT32[armcpu->proc_ID][(armcpu->instruct_adr>>24)&0xF];
	}
	armcpu->instruction = MMU_readHWordACL(armcpu->proc_ID, armcpu->next_instruction,CP15_ACCESS_EXECUTE);
	armcpu->instruct_adr = armcpu->next_instruction;
	armcpu->next_instruction = armcpu->next_instruction + 2;
	armcpu->R[15] = armcpu->next_instruction + 2;
	return MMU.MMU_WAIT16[armcpu->proc_ID][(armcpu->instruct_adr>>24)&0xF];
}
 

static BOOL FASTCALL test_EQ(Status_Reg CPSR) { return ( CPSR.bits.Z); }
static BOOL FASTCALL test_NE(Status_Reg CPSR) { return (!CPSR.bits.Z); }
static BOOL FASTCALL test_CS(Status_Reg CPSR) { return ( CPSR.bits.C); }
static BOOL FASTCALL test_CC(Status_Reg CPSR) { return (!CPSR.bits.C); }
static BOOL FASTCALL test_MI(Status_Reg CPSR) { return ( CPSR.bits.N); }
static BOOL FASTCALL test_PL(Status_Reg CPSR) { return (!CPSR.bits.N); }
static BOOL FASTCALL test_VS(Status_Reg CPSR) { return ( CPSR.bits.V); }
static BOOL FASTCALL test_VC(Status_Reg CPSR) { return (!CPSR.bits.V); }
static BOOL FASTCALL test_HI(Status_Reg CPSR) { return (CPSR.bits.C) && (!CPSR.bits.Z); }
static BOOL FASTCALL test_LS(Status_Reg CPSR) { return (CPSR.bits.Z) || (!CPSR.bits.C); }
static BOOL FASTCALL test_GE(Status_Reg CPSR) { return (CPSR.bits.N==CPSR.bits.V); }
static BOOL FASTCALL test_LT(Status_Reg CPSR) { return (CPSR.bits.N!=CPSR.bits.V); }
static BOOL FASTCALL test_GT(Status_Reg CPSR) { return (!CPSR.bits.Z) && (CPSR.bits.N==CPSR.bits.V); }
static BOOL FASTCALL test_LE(Status_Reg CPSR) { return ( CPSR.bits.Z) || (CPSR.bits.N!=CPSR.bits.V); }
static BOOL FASTCALL test_AL(Status_Reg CPSR) { return 1; }

static BOOL (*FASTCALL test_conditions[])(Status_Reg CPSR)= {
	test_EQ , test_NE ,
	test_CS , test_CC ,
	test_MI , test_PL ,
	test_VS , test_VC ,
	test_HI , test_LS ,
	test_GE , test_LT ,
	test_GT , test_LE ,
	test_AL
};
#define TEST_COND2(cond, CPSR) \
	(cond<15&&test_conditions[cond](CPSR))

u32 armcpu_exec(armcpu_t *armcpu)
{
        u32 c = 1;
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

BOOL armcpu_irqExeption(armcpu_t *armcpu)
{
        Status_Reg tmp;
	if(armcpu->CPSR.bits.I) return FALSE;
        tmp = armcpu->CPSR;
	armcpu_switchMode(armcpu, IRQ);
	armcpu->R[14] = armcpu->instruct_adr + 4;
	armcpu->SPSR = tmp;
	armcpu->CPSR.bits.T = 0;
	armcpu->CPSR.bits.I = 1;
	armcpu->next_instruction = armcpu->intVector + 0x18;
	armcpu->R[15] = armcpu->next_instruction;
	armcpu->waitIRQ = 0;
	armcpu_prefetch(armcpu);
	return TRUE;
}

BOOL armcpu_prefetchExeption(armcpu_t *armcpu)
{
        Status_Reg tmp;
	if(armcpu->CPSR.bits.I) return FALSE;
        tmp = armcpu->CPSR;
	armcpu_switchMode(armcpu, ABT);
	armcpu->R[14] = armcpu->instruct_adr + 4;
	armcpu->SPSR = tmp;
	armcpu->CPSR.bits.T = 0;
	armcpu->CPSR.bits.I = 1;
	armcpu->next_instruction = armcpu->intVector + 0xC;
	armcpu->R[15] = armcpu->next_instruction;
	armcpu->waitIRQ = 0;
	armcpu_prefetch(armcpu);
	return TRUE;
}

