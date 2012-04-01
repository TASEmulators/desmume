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

#include <stdlib.h>

#include "cp15.h"
#include "debug.h"
#include "MMU.h"

armcp15_t *armcp15_new(armcpu_t * c)
{
	int i;
	armcp15_t *armcp15 = (armcp15_t*)malloc(sizeof(armcp15_t));
	if(!armcp15) return NULL;
	
	armcp15->cpu = c;
	armcp15->IDCode = 0x41049460;
	armcp15->cacheType = 0x0F0D2112;
	armcp15->TCMSize = 0x00140140;
	armcp15->ctrl = 0x00000000;
	armcp15->DCConfig = 0x0;    
	armcp15->ICConfig = 0x0;    
	armcp15->writeBuffCtrl = 0x0;
	armcp15->und = 0x0;
	armcp15->DaccessPerm = 0x22222222;
	armcp15->IaccessPerm = 0x22222222;
	armcp15->protectBaseSize0 = 0x0;
	armcp15->protectBaseSize1 = 0x0;
	armcp15->protectBaseSize2 = 0x0;
	armcp15->protectBaseSize3 = 0x0;
	armcp15->protectBaseSize4 = 0x0;
	armcp15->protectBaseSize5 = 0x0;
	armcp15->protectBaseSize6 = 0x0;
	armcp15->protectBaseSize7 = 0x0;
	armcp15->cacheOp = 0x0;
	armcp15->DcacheLock = 0x0;
	armcp15->IcacheLock = 0x0;
	armcp15->ITCMRegion = 0x0C;
	armcp15->DTCMRegion = 0x0080000A;
	armcp15->processID = 0;

    /* preset calculated regionmasks */	
	for (i=0;i<8;i++) {
        armcp15->regionWriteMask_USR[i] = 0 ;
        armcp15->regionWriteMask_SYS[i] = 0 ;
        armcp15->regionReadMask_USR[i] = 0 ;
        armcp15->regionReadMask_SYS[i] = 0 ;
        armcp15->regionExecuteMask_USR[i] = 0 ;
        armcp15->regionExecuteMask_SYS[i] = 0 ;
        armcp15->regionWriteSet_USR[i] = 0 ;
        armcp15->regionWriteSet_SYS[i] = 0 ;
        armcp15->regionReadSet_USR[i] = 0 ;
        armcp15->regionReadSet_SYS[i] = 0 ;
        armcp15->regionExecuteSet_USR[i] = 0 ;
        armcp15->regionExecuteSet_SYS[i] = 0 ;
    } ;
	
	return armcp15;
}

#define ACCESSTYPE(val,n)   (((val) >> (4*n)) & 0x0F)
#define SIZEIDENTIFIER(val) ((((val) >> 1) & 0x1F))
#define SIZEBINARY(val)     (1 << (SIZEIDENTIFIER(val)+1))
#define MASKFROMREG(val)    (~((SIZEBINARY(val)-1) | 0x3F))
#define SETFROMREG(val)     ((val) & MASKFROMREG(val))
/* sets the precalculated regions to mask,set for the affected accesstypes */
void armcp15_setSingleRegionAccess(armcp15_t *armcp15,unsigned long dAccess,unsigned long iAccess,unsigned char num, unsigned long mask,unsigned long set) {

	switch (ACCESSTYPE(dAccess,num)) {
		case 4: /* UNP */
		case 7: /* UNP */
		case 8: /* UNP */
		case 9: /* UNP */
		case 10: /* UNP */
		case 11: /* UNP */
		case 12: /* UNP */
		case 13: /* UNP */
		case 14: /* UNP */
		case 15: /* UNP */
		case 0: /* no access at all */
			armcp15->regionWriteMask_USR[num] = 0 ;
			armcp15->regionWriteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_USR[num] = 0 ;
			armcp15->regionReadSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionWriteMask_SYS[num] = 0 ;
			armcp15->regionWriteSet_SYS[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_SYS[num] = 0 ;
			armcp15->regionReadSet_SYS[num] = 0xFFFFFFFF ;
			break ;
		case 1: /* no access at USR, all to sys */
			armcp15->regionWriteMask_USR[num] = 0 ;
			armcp15->regionWriteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_USR[num] = 0 ;
			armcp15->regionReadSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionWriteMask_SYS[num] = mask ;
			armcp15->regionWriteSet_SYS[num] = set ;
			armcp15->regionReadMask_SYS[num] = mask ;
			armcp15->regionReadSet_SYS[num] = set ;
			break ;
		case 2: /* read at USR, all to sys */
			armcp15->regionWriteMask_USR[num] = 0 ;
			armcp15->regionWriteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_USR[num] = mask ;
			armcp15->regionReadSet_USR[num] = set ;
			armcp15->regionWriteMask_SYS[num] = mask ;
			armcp15->regionWriteSet_SYS[num] = set ;
			armcp15->regionReadMask_SYS[num] = mask ;
			armcp15->regionReadSet_SYS[num] = set ;
			break ;
		case 3: /* all to USR, all to sys */
			armcp15->regionWriteMask_USR[num] = mask ;
			armcp15->regionWriteSet_USR[num] = set ;
			armcp15->regionReadMask_USR[num] = mask ;
			armcp15->regionReadSet_USR[num] = set ;
			armcp15->regionWriteMask_SYS[num] = mask ;
			armcp15->regionWriteSet_SYS[num] = set ;
			armcp15->regionReadMask_SYS[num] = mask ;
			armcp15->regionReadSet_SYS[num] = set ;
			break ;
		case 5: /* no access at USR, read to sys */
			armcp15->regionWriteMask_USR[num] = 0 ;
			armcp15->regionWriteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_USR[num] = 0 ;
			armcp15->regionReadSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionWriteMask_SYS[num] = 0 ;
			armcp15->regionWriteSet_SYS[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_SYS[num] = mask ;
			armcp15->regionReadSet_SYS[num] = set ;
			break ;
		case 6: /* read at USR, read to sys */
			armcp15->regionWriteMask_USR[num] = 0 ;
			armcp15->regionWriteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_USR[num] = mask ;
			armcp15->regionReadSet_USR[num] = set ;
			armcp15->regionWriteMask_SYS[num] = 0 ;
			armcp15->regionWriteSet_SYS[num] = 0xFFFFFFFF ;
			armcp15->regionReadMask_SYS[num] = mask ;
			armcp15->regionReadSet_SYS[num] = set ;
			break ;
	}
	switch (ACCESSTYPE(iAccess,num)) {
		case 4: /* UNP */
		case 7: /* UNP */
		case 8: /* UNP */
		case 9: /* UNP */
		case 10: /* UNP */
		case 11: /* UNP */
		case 12: /* UNP */
		case 13: /* UNP */
		case 14: /* UNP */
		case 15: /* UNP */
		case 0: /* no access at all */
			armcp15->regionExecuteMask_USR[num] = 0 ;
			armcp15->regionExecuteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionExecuteMask_SYS[num] = 0 ;
			armcp15->regionExecuteSet_SYS[num] = 0xFFFFFFFF ;
			break ;
		case 1:
			armcp15->regionExecuteMask_USR[num] = 0 ;
			armcp15->regionExecuteSet_USR[num] = 0xFFFFFFFF ;
			armcp15->regionExecuteMask_SYS[num] = mask ;
			armcp15->regionExecuteSet_SYS[num] = set ;
			break ;
		case 2:
		case 3:
		case 6:
			armcp15->regionExecuteMask_USR[num] = mask ;
			armcp15->regionExecuteSet_USR[num] = set ;
			armcp15->regionExecuteMask_SYS[num] = mask ;
			armcp15->regionExecuteSet_SYS[num] = set ;
			break ;
	}
} ;

/* precalculate region masks/sets from cp15 register */
void armcp15_maskPrecalc(armcp15_t *armcp15)
{
	#define precalc(num) {  \
	u32 mask = 0, set = 0xFFFFFFFF ; /* (x & 0) == 0xFF..FF is allways false (disabled) */  \
	if (BIT_N(armcp15->protectBaseSize##num,0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
		mask = MASKFROMREG(armcp15->protectBaseSize##num) ;   \
		set = SETFROMREG(armcp15->protectBaseSize##num) ; \
		if (SIZEIDENTIFIER(armcp15->protectBaseSize##num)==0x1F)  \
		{   /* for the 4GB region, u32 suffers wraparound */   \
		mask = 0 ; set = 0 ;   /* (x & 0) == 0  is allways true (enabled) */  \
		} \
	}  \
		armcp15_setSingleRegionAccess(armcp15,armcp15->DaccessPerm,armcp15->IaccessPerm,num,mask,set) ;  \
	}
	precalc(0) ;
	precalc(1) ;
	precalc(2) ;
	precalc(3) ;
	precalc(4) ;
	precalc(5) ;
	precalc(6) ;
	precalc(7) ;
}

INLINE BOOL armcp15_isAccessAllowed(armcp15_t *armcp15,u32 address,u32 access)
{
	int i ;
	if (!(armcp15->ctrl & 1)) return TRUE ;        /* protection checking is not enabled */
	for (i=0;i<8;i++) {
		switch (access) {
		case CP15_ACCESS_WRITEUSR:
			if ((address & armcp15->regionWriteMask_USR[i]) == armcp15->regionWriteSet_USR[i]) return TRUE ;
			break ;
		case CP15_ACCESS_WRITESYS:
			if ((address & armcp15->regionWriteMask_SYS[i]) == armcp15->regionWriteSet_SYS[i]) return TRUE ;
			break ;
		case CP15_ACCESS_READUSR:
			if ((address & armcp15->regionReadMask_USR[i]) == armcp15->regionReadSet_USR[i]) return TRUE ;
			break ;
		case CP15_ACCESS_READSYS:
			if ((address & armcp15->regionReadMask_SYS[i]) == armcp15->regionReadSet_SYS[i]) return TRUE ;
			break ;
		case CP15_ACCESS_EXECUSR:
			if ((address & armcp15->regionExecuteMask_USR[i]) == armcp15->regionExecuteSet_USR[i]) return TRUE ;
			break ;
		case CP15_ACCESS_EXECSYS:
			if ((address & armcp15->regionExecuteMask_SYS[i]) == armcp15->regionExecuteSet_SYS[i]) return TRUE ;
			break ;
		}
	}
	/* when protections are enabled, but no region allows access, deny access */
	return FALSE ;
}

BOOL armcp15_dataProcess(armcp15_t *armcp15, u8 CRd, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
   return FALSE;
}

BOOL armcp15_load(armcp15_t *armcp15, u8 CRd, u8 adr)
{
   return FALSE;
}

BOOL armcp15_store(armcp15_t *armcp15, u8 CRd, u8 adr)
{
   return FALSE;
}

BOOL armcp15_moveCP2ARM(armcp15_t *armcp15, u32 * R, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	if(armcp15->cpu->CPSR.bits.mode == USR) return FALSE;
	
	switch(CRn)
	{
		case 0 :
			if((opcode1 == 0)&&(CRm==0))
			{
			switch(opcode2)
			{
				case 1 :
					*R = armcp15->cacheType;
					return TRUE;
				case 2 :
					*R = armcp15->TCMSize;
					return TRUE;
				default :
					*R = armcp15->IDCode;
					return TRUE;
			}
			}
			return FALSE;
		case 1 :
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				*R = armcp15->ctrl;
				return TRUE;
			}
			return FALSE;
			
		case 2 :
			if((opcode1==0) && (CRm==0))
			{
				switch(opcode2)
				{
					case 0 :
						*R = armcp15->DCConfig;
					return TRUE;
					case 1 :
						*R = armcp15->ICConfig;
					return TRUE;
					default :
					return FALSE;
				}
			}
			return FALSE;
		case 3 :
			if((opcode1==0) && (opcode2==0) && (CRm==0))
			{
				*R = armcp15->writeBuffCtrl;
				return TRUE;
			}
			return FALSE;
		case 5 :
			if((opcode1==0) && (CRm==0))
			{
				switch(opcode2)
				{
					case 2 :
						*R = armcp15->DaccessPerm;
					    return TRUE;
					case 3 :
						*R = armcp15->IaccessPerm;
					return TRUE;
					default :
					return FALSE;
				}
			}
			return FALSE;
		case 6 :
			if((opcode1==0) && (opcode2==0))
			{
				switch(CRm)
				{
					case 0 :
						*R = armcp15->protectBaseSize0;
					return TRUE;
					case 1 :
						*R = armcp15->protectBaseSize1;
					return TRUE;
					case 2 :
						*R = armcp15->protectBaseSize2;
					return TRUE;
					case 3 :
						*R = armcp15->protectBaseSize3;
					return TRUE;
					case 4 :
						*R = armcp15->protectBaseSize4;
					return TRUE;
					case 5 :
						*R = armcp15->protectBaseSize5;
					return TRUE;
					case 6 :
						*R = armcp15->protectBaseSize6;
					return TRUE;
					case 7 :
						*R = armcp15->protectBaseSize7;
					return TRUE;
					default :
					return FALSE;
				}
			}
			return FALSE;
		case 9 :
			if((opcode1==0))
			{
				switch(CRm)
				{
					case 0 :
						switch(opcode2)
						{
							case 0 :
							*R = armcp15->DcacheLock;
							return TRUE;
							case 1 :
							*R = armcp15->IcacheLock;
							return TRUE;
							default :
								return FALSE;
						}
					case 1 :
						switch(opcode2)
						{
						case 0 :
						*R = armcp15->DTCMRegion;
						return TRUE;
						case 1 :
						*R = armcp15->ITCMRegion;
						return TRUE;
						default :
							return FALSE;
						}
				}
			}
		return FALSE;
		default :
			return FALSE;
	}
}


u32 CP15wait4IRQ(armcpu_t *cpu)
{
	/* on the first call, wirq is not set */
	if(cpu->wirq)
	{
		/* check wether an irq was issued */
		if(!cpu->waitIRQ)
		{
			cpu->waitIRQ = 0;
			cpu->wirq = 0;
			return 1;   /* return execution */
		}
		/* otherwise, repeat this instruction */
		cpu->R[15] = cpu->instruct_adr;
		cpu->next_instruction = cpu->R[15];
		return 1;
	}
	/* first run, set us into waiting state */
	cpu->waitIRQ = 1;
	cpu->wirq = 1;
	/* and set next instruction to repeat this */
	cpu->R[15] = cpu->instruct_adr;
	cpu->next_instruction = cpu->R[15];
	/* CHECKME: IME shouldn't be modified (?) */
	MMU.reg_IME[0] = 1;
	return 1;
}

BOOL armcp15_moveARM2CP(armcp15_t *armcp15, u32 val, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	if(armcp15->cpu->CPSR.bits.mode == USR) return FALSE;
	
	switch(CRn)
	{
		case 1 :
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			armcp15->ctrl = val;
			MMU.ARM9_RW_MODE = BIT7(val);
			armcp15->cpu->intVector = 0x0FFF0000 * (BIT13(val));
			armcp15->cpu->LDTBit = !BIT15(val); //TBit
			/*if(BIT17(val))
			{
				log::ajouter("outch !!!!!!!");
			}
			if(BIT19(val))
			{
				log::ajouter("outch !!!!!!!");
			}*/
			return TRUE;
		}
		return FALSE;
		case 2 :
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
				case 0 :
					armcp15->DCConfig = val;
					return TRUE;
				case 1 :
					armcp15->ICConfig = val;
					return TRUE;
				default :
					return FALSE;
			}
		}
		return FALSE;
		case 3 :
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			armcp15->writeBuffCtrl = val;
			return TRUE;
		}
		return FALSE;
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
				case 2 :
					armcp15->DaccessPerm = val;
					armcp15_maskPrecalc(armcp15);
 					return TRUE;
				case 3 :
					armcp15->IaccessPerm = val;
					armcp15_maskPrecalc(armcp15);
					return TRUE;
				default :
					return FALSE;
			}
		}
		return FALSE;
		case 6 :
		if((opcode1==0) && (opcode2==0))
		{
			switch(CRm)
			{
				case 0 :
					armcp15->protectBaseSize0 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 1 :
					armcp15->protectBaseSize1 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 2 :
					armcp15->protectBaseSize2 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 3 :
					armcp15->protectBaseSize3 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 4 :
					armcp15->protectBaseSize4 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 5 :
					armcp15->protectBaseSize5 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 6 :
					armcp15->protectBaseSize6 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				case 7 :
					armcp15->protectBaseSize7 = val;
					armcp15_maskPrecalc(armcp15) ;
					return TRUE;
				default :
					return FALSE;
			}
		}
		return FALSE;
		case 7 :
		if((CRm==0)&&(opcode1==0)&&((opcode2==4)))
		{
			CP15wait4IRQ(armcp15->cpu);
			return TRUE;
		}
		return FALSE;
		case 9 :
		if((opcode1==0))
		{
			switch(CRm)
			{
				case 0 :
				switch(opcode2)
				{
					case 0 :
						armcp15->DcacheLock = val;
						return TRUE;
					case 1 :
						armcp15->IcacheLock = val;
						return TRUE;
					default :
						return FALSE;
			}
			case 1 :
			switch(opcode2)
			{
				case 0 :
					armcp15->DTCMRegion = val;
					MMU.DTCMRegion = val & 0x0FFFFFFC0;
					/*sprintf(logbuf, "%08X", val);
					log::ajouter(logbuf);*/
					return TRUE;
				case 1 :
					armcp15->ITCMRegion = val;
					/* ITCM base is not writeable! */
					MMU.ITCMRegion = 0;
					return TRUE;
				default :
					return FALSE;
				}
			}
		}
		return FALSE;
		default :
			return FALSE;
	}
}



