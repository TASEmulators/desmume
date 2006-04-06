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

#include "ARM_CPU.hpp"
#include "arm_instructions.hpp"
#include "thumb_instructions.hpp"
#include "MMU.hpp"
#include "CP15.hpp"
#include "bios.hpp"
#include <stdlib.h>

#define SWAP(a, b, c) c=a;\
                      a=b;\
                      b=c;
                      
armcpu_t *armcpu_new(unsigned long id)
{
	armcpu_t *armcpu;
	
	armcpu = (armcpu_t*)malloc(sizeof(armcpu_t));
	if(!armcpu) return NULL;
	
	armcpu->proc_ID = id;
	
	if(id==0) armcpu->swi_tab = ARM9_swi_tab;
	else armcpu->swi_tab = ARM7_swi_tab;
	
	armcpu_init(armcpu, 0);
	
	return armcpu;
} 

void armcpu_init(armcpu_t *armcpu, unsigned long adr)
{
	armcpu->LDTBit = (armcpu->proc_ID==0); //Si ARM9 utiliser le syte v5 pour le load
	armcpu->intVector = 0xFFFF0000 * (armcpu->proc_ID==0);
	armcpu->waitIRQ = false;
	armcpu->wirq = false;
	
	if(armcpu->coproc[15]) delete armcpu->coproc[15];
	
   for(unsigned long i = 0; i < 15; ++i)
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
	armcpu->coproc[15] = new CP15(armcpu);
	
	armcpu_prefetch(armcpu);
}

unsigned long armcpu_switchMode(armcpu_t *armcpu, unsigned char mode)
{
	unsigned long oldmode = armcpu->CPSR.bits.mode;
	
	switch(oldmode)
	{
		case USR :
		case SYS :
			armcpu->R13_usr = armcpu->R[13];
			armcpu->R14_usr = armcpu->R[14];
			break;
			
		case FIQ :
			{
				unsigned long tmp;
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
					unsigned long tmp;
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

