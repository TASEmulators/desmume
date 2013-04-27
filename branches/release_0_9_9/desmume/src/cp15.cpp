/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2011 DeSmuME team

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

#include <stdlib.h>

#include "cp15.h"
#include "debug.h"
#include "MMU.h"

armcp15_t cp15;

bool armcp15_t::reset(armcpu_t * c)
{
	//printf("CP15 Reset\n");
	cpu = c;
	IDCode = 0x41059461;
	cacheType = 0x0F0D2112;
	TCMSize = 0x00140180;
	ctrl = 0x00012078;
	DCConfig = 0x0;    
	ICConfig = 0x0;    
	writeBuffCtrl = 0x0;
	und = 0x0;
	DaccessPerm = 0x22222222;
	IaccessPerm = 0x22222222;
	protectBaseSize0 = 0x0;
	protectBaseSize1 = 0x0;
	protectBaseSize2 = 0x0;
	protectBaseSize3 = 0x0;
	protectBaseSize4 = 0x0;
	protectBaseSize5 = 0x0;
	protectBaseSize6 = 0x0;
	protectBaseSize7 = 0x0;
	cacheOp = 0x0;
	DcacheLock = 0x0;
	IcacheLock = 0x0;
	ITCMRegion = 0x0C;
	DTCMRegion = 0x0080000A;
	processID = 0;

	MMU.ARM9_RW_MODE = BIT7(ctrl);
	cpu->intVector = 0xFFFF0000 * (BIT13(ctrl));
	cpu->LDTBit = !BIT15(ctrl); //TBit

	/* preset calculated regionmasks */	
	for (u8 i=0;i<8;i++) {
		regionWriteMask_USR[i] = 0 ;
		regionWriteMask_SYS[i] = 0 ;
		regionReadMask_USR[i] = 0 ;
		regionReadMask_SYS[i] = 0 ;
		regionExecuteMask_USR[i] = 0 ;
		regionExecuteMask_SYS[i] = 0 ;
		regionWriteSet_USR[i] = 0 ;
		regionWriteSet_SYS[i] = 0 ;
		regionReadSet_USR[i] = 0 ;
		regionReadSet_SYS[i] = 0 ;
		regionExecuteSet_USR[i] = 0 ;
		regionExecuteSet_SYS[i] = 0 ;
	} ;

	return true;
}

#define ACCESSTYPE(val,n)   (((val) >> (4*n)) & 0x0F)
#define SIZEIDENTIFIER(val) ((((val) >> 1) & 0x1F))
#define SIZEBINARY(val)     (1 << (SIZEIDENTIFIER(val)+1))
#define MASKFROMREG(val)    (~((SIZEBINARY(val)-1) | 0x3F))
#define SETFROMREG(val)     ((val) & MASKFROMREG(val))
/* sets the precalculated regions to mask,set for the affected accesstypes */
void armcp15_t::setSingleRegionAccess(u32 dAccess,u32 iAccess,unsigned char num, u32 mask,u32 set) {

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
			regionWriteMask_USR[num] = 0 ;
			regionWriteSet_USR[num] = 0xFFFFFFFF ;
			regionReadMask_USR[num] = 0 ;
			regionReadSet_USR[num] = 0xFFFFFFFF ;
			regionWriteMask_SYS[num] = 0 ;
			regionWriteSet_SYS[num] = 0xFFFFFFFF ;
			regionReadMask_SYS[num] = 0 ;
			regionReadSet_SYS[num] = 0xFFFFFFFF ;
			break ;
		case 1: /* no access at USR, all to sys */
			regionWriteMask_USR[num] = 0 ;
			regionWriteSet_USR[num] = 0xFFFFFFFF ;
			regionReadMask_USR[num] = 0 ;
			regionReadSet_USR[num] = 0xFFFFFFFF ;
			regionWriteMask_SYS[num] = mask ;
			regionWriteSet_SYS[num] = set ;
			regionReadMask_SYS[num] = mask ;
			regionReadSet_SYS[num] = set ;
			break ;
		case 2: /* read at USR, all to sys */
			regionWriteMask_USR[num] = 0 ;
			regionWriteSet_USR[num] = 0xFFFFFFFF ;
			regionReadMask_USR[num] = mask ;
			regionReadSet_USR[num] = set ;
			regionWriteMask_SYS[num] = mask ;
			regionWriteSet_SYS[num] = set ;
			regionReadMask_SYS[num] = mask ;
			regionReadSet_SYS[num] = set ;
			break ;
		case 3: /* all to USR, all to sys */
			regionWriteMask_USR[num] = mask ;
			regionWriteSet_USR[num] = set ;
			regionReadMask_USR[num] = mask ;
			regionReadSet_USR[num] = set ;
			regionWriteMask_SYS[num] = mask ;
			regionWriteSet_SYS[num] = set ;
			regionReadMask_SYS[num] = mask ;
			regionReadSet_SYS[num] = set ;
			break ;
		case 5: /* no access at USR, read to sys */
			regionWriteMask_USR[num] = 0 ;
			regionWriteSet_USR[num] = 0xFFFFFFFF ;
			regionReadMask_USR[num] = 0 ;
			regionReadSet_USR[num] = 0xFFFFFFFF ;
			regionWriteMask_SYS[num] = 0 ;
			regionWriteSet_SYS[num] = 0xFFFFFFFF ;
			regionReadMask_SYS[num] = mask ;
			regionReadSet_SYS[num] = set ;
			break ;
		case 6: /* read at USR, read to sys */
			regionWriteMask_USR[num] = 0 ;
			regionWriteSet_USR[num] = 0xFFFFFFFF ;
			regionReadMask_USR[num] = mask ;
			regionReadSet_USR[num] = set ;
			regionWriteMask_SYS[num] = 0 ;
			regionWriteSet_SYS[num] = 0xFFFFFFFF ;
			regionReadMask_SYS[num] = mask ;
			regionReadSet_SYS[num] = set ;
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
			regionExecuteMask_USR[num] = 0 ;
			regionExecuteSet_USR[num] = 0xFFFFFFFF ;
			regionExecuteMask_SYS[num] = 0 ;
			regionExecuteSet_SYS[num] = 0xFFFFFFFF ;
			break ;
		case 1:
			regionExecuteMask_USR[num] = 0 ;
			regionExecuteSet_USR[num] = 0xFFFFFFFF ;
			regionExecuteMask_SYS[num] = mask ;
			regionExecuteSet_SYS[num] = set ;
			break ;
		case 2:
		case 3:
		case 6:
			regionExecuteMask_USR[num] = mask ;
			regionExecuteSet_USR[num] = set ;
			regionExecuteMask_SYS[num] = mask ;
			regionExecuteSet_SYS[num] = set ;
			break ;
	}
} ;

/* precalculate region masks/sets from cp15 register */
void armcp15_t::maskPrecalc()
{
#define precalc(num) {  \
	u32 mask = 0, set = 0xFFFFFFFF ; /* (x & 0) == 0xFF..FF is allways false (disabled) */  \
	if (BIT_N(protectBaseSize##num,0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
	mask = MASKFROMREG(protectBaseSize##num) ;   \
	set = SETFROMREG(protectBaseSize##num) ; \
	if (SIZEIDENTIFIER(protectBaseSize##num)==0x1F)  \
	{   /* for the 4GB region, u32 suffers wraparound */   \
	mask = 0 ; set = 0 ;   /* (x & 0) == 0  is allways true (enabled) */  \
} \
}  \
	setSingleRegionAccess(DaccessPerm,IaccessPerm,num,mask,set) ;  \
}
	precalc(0) ;
	precalc(1) ;
	precalc(2) ;
	precalc(3) ;
	precalc(4) ;
	precalc(5) ;
	precalc(6) ;
	precalc(7) ;
#undef precalc
}

BOOL armcp15_t::isAccessAllowed(u32 address,u32 access)
{
	int i ;
	if (!(ctrl & 1)) return TRUE ;        /* protection checking is not enabled */
	for (i=0;i<8;i++) {
		switch (access) {
		case CP15_ACCESS_WRITEUSR:
			if ((address & regionWriteMask_USR[i]) == regionWriteSet_USR[i]) return TRUE ;
			break ;
		case CP15_ACCESS_WRITESYS:
			if ((address & regionWriteMask_SYS[i]) == regionWriteSet_SYS[i]) return TRUE ;
			break ;
		case CP15_ACCESS_READUSR:
			if ((address & regionReadMask_USR[i]) == regionReadSet_USR[i]) return TRUE ;
			break ;
		case CP15_ACCESS_READSYS:
			if ((address & regionReadMask_SYS[i]) == regionReadSet_SYS[i]) return TRUE ;
			break ;
		case CP15_ACCESS_EXECUSR:
			if ((address & regionExecuteMask_USR[i]) == regionExecuteSet_USR[i]) return TRUE ;
			break ;
		case CP15_ACCESS_EXECSYS:
			if ((address & regionExecuteMask_SYS[i]) == regionExecuteSet_SYS[i]) return TRUE ;
			break ;
		}
	}
	/* when protections are enabled, but no region allows access, deny access */
	return FALSE ;
}

BOOL armcp15_t::dataProcess(u8 CRd, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	LOG("Unsupported CP15 operation : DataProcess\n");
	return FALSE;
}

BOOL armcp15_t::load(u8 CRd, u8 adr)
{
	LOG("Unsupported CP15 operation : Load\n");
	return FALSE;
}

BOOL armcp15_t::store(u8 CRd, u8 adr)
{
	LOG("Unsupported CP15 operation : Store\n");
	return FALSE;
}

BOOL armcp15_t::moveCP2ARM(u32 * R, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	if (!cpu)
	{
		printf("ERROR: cp15 don\'t allocated\n");
		return FALSE;
	}
	if(cpu->CPSR.bits.mode == USR) return FALSE;

	switch(CRn)
	{
	case 0:
		if((opcode1 == 0)&&(CRm==0))
		{
			switch(opcode2)
			{
			case 1:
				*R = cacheType;
				return TRUE;
			case 2:
				*R = TCMSize;
				return TRUE;
			default:
				*R = IDCode;
				return TRUE;
			}
		}
		return FALSE;
	case 1:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			*R = ctrl;
			//LOG("CP15: CPtoARM ctrl %08X\n", ctrl);
			return TRUE;
		}
		return FALSE;

	case 2:
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
			case 0:
				*R = DCConfig;
				return TRUE;
			case 1:
				*R = ICConfig;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 3:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			*R = writeBuffCtrl;
			//LOG("CP15: CPtoARM writeBuffer ctrl %08X\n", writeBuffCtrl);
			return TRUE;
		}
		return FALSE;
	case 5:
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
			case 2:
				*R = DaccessPerm;
				return TRUE;
			case 3:
				*R = IaccessPerm;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 6:
		if((opcode1==0) && (opcode2==0))
		{
			switch(CRm)
			{
			case 0:
				*R = protectBaseSize0;
				return TRUE;
			case 1:
				*R = protectBaseSize1;
				return TRUE;
			case 2:
				*R = protectBaseSize2;
				return TRUE;
			case 3:
				*R = protectBaseSize3;
				return TRUE;
			case 4:
				*R = protectBaseSize4;
				return TRUE;
			case 5:
				*R = protectBaseSize5;
				return TRUE;
			case 6:
				*R = protectBaseSize6;
				return TRUE;
			case 7:
				*R = protectBaseSize7;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 9:
		if((opcode1==0))
		{
			switch(CRm)
			{
			case 0:
				switch(opcode2)
				{
				case 0:
					*R = DcacheLock;
					return TRUE;
				case 1:
					*R = IcacheLock;
					return TRUE;
				default:
					return FALSE;
				}
			case 1:
				switch(opcode2)
				{
				case 0:
					*R = DTCMRegion;
					return TRUE;
				case 1:
					*R = ITCMRegion;
					return TRUE;
				default:
					return FALSE;
				}
			}
		}
		return FALSE;
	default:
		LOG("Unsupported CP15 operation : MRC\n");
		return FALSE;
	}
}

BOOL armcp15_t::moveARM2CP(u32 val, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	if (!cpu)
	{
		printf("ERROR: cp15 don\'t allocated\n");
		return FALSE;
	}
	if(cpu->CPSR.bits.mode == USR) return FALSE;

	switch(CRn)
	{
	case 1:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{

			//On the NDS bit0,2,7,12..19 are R/W, Bit3..6 are always set, all other bits are always zero.
			ctrl = (val & 0x000FF085) | 0x00000078;
			MMU.ARM9_RW_MODE = BIT7(val);
			//zero 31-jan-2010: change from 0x0FFF0000 to 0xFFFF0000 per gbatek
			cpu->intVector = 0xFFFF0000 * (BIT13(val));
			cpu->LDTBit = !BIT15(val); //TBit
			//LOG("CP15: ARMtoCP ctrl %08X (val %08X)\n", ctrl, val);
			return TRUE;
		}
		return FALSE;
	case 2:
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
			case 0:
				DCConfig = val;
				return TRUE;
			case 1:
				ICConfig = val;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 3:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			writeBuffCtrl = val;
			//LOG("CP15: ARMtoCP writeBuffer ctrl %08X\n", writeBuffCtrl);
			return TRUE;
		}
		return FALSE;
	case 5:
		if((opcode1==0) && (CRm==0))
		{
			switch(opcode2)
			{
			case 2:
				DaccessPerm = val;
				maskPrecalc();
				return TRUE;
			case 3:
				IaccessPerm = val;
				maskPrecalc();
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 6:
		if((opcode1==0) && (opcode2==0))
		{
			switch(CRm)
			{
			case 0:
				protectBaseSize0 = val;
				maskPrecalc();
				return TRUE;
			case 1:
				protectBaseSize1 = val;
				maskPrecalc();
				return TRUE;
			case 2:
				protectBaseSize2 = val;
				maskPrecalc();
				return TRUE;
			case 3:
				protectBaseSize3 = val;
				maskPrecalc();
				return TRUE;
			case 4:
				protectBaseSize4 = val;
				maskPrecalc();
				return TRUE;
			case 5:
				protectBaseSize5 = val;
				maskPrecalc();
				return TRUE;
			case 6:
				protectBaseSize6 = val;
				maskPrecalc();
				return TRUE;
			case 7:
				protectBaseSize7 = val;
				maskPrecalc();
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 7:
		if((CRm==0)&&(opcode1==0)&&((opcode2==4)))
		{
			//CP15wait4IRQ;
			cpu->waitIRQ = TRUE;
			cpu->halt_IE_and_IF = TRUE;
			//IME set deliberately omitted: only SWI sets IME to 1
			return TRUE;
		}
		return FALSE;
	case 9:
		if((opcode1==0))
		{
			switch(CRm)
			{
			case 0:
				switch(opcode2)
				{
				case 0:
					DcacheLock = val;
					return TRUE;
				case 1:
					IcacheLock = val;
					return TRUE;
				default:
					return FALSE;
				}
			case 1:
				switch(opcode2)
				{
				case 0:
					MMU.DTCMRegion = DTCMRegion = val & 0x0FFFF000;
					return TRUE;
				case 1:
					ITCMRegion = val;
					//ITCM base is not writeable!
					MMU.ITCMRegion = 0;
					return TRUE;
				default:
					return FALSE;
				}
			}
		}
		return FALSE;
	default:
		return FALSE;
	}
}

// Save state
void armcp15_t::saveone(EMUFILE* os)
{
	write32le(IDCode,os);
	write32le(cacheType,os);
    write32le(TCMSize,os);
    write32le(ctrl,os);
    write32le(DCConfig,os);
    write32le(ICConfig,os);
    write32le(writeBuffCtrl,os);
    write32le(und,os);
    write32le(DaccessPerm,os);
    write32le(IaccessPerm,os);
    write32le(protectBaseSize0,os);
    write32le(protectBaseSize1,os);
    write32le(protectBaseSize2,os);
    write32le(protectBaseSize3,os);
    write32le(protectBaseSize4,os);
    write32le(protectBaseSize5,os);
    write32le(protectBaseSize6,os);
    write32le(protectBaseSize7,os);
    write32le(cacheOp,os);
    write32le(DcacheLock,os);
    write32le(IcacheLock,os);
    write32le(ITCMRegion,os);
    write32le(DTCMRegion,os);
    write32le(processID,os);
    write32le(RAM_TAG,os);
    write32le(testState,os);
    write32le(cacheDbg,os);
    for(int i=0;i<8;i++) write32le(regionWriteMask_USR[i],os);
    for(int i=0;i<8;i++) write32le(regionWriteMask_SYS[i],os);
    for(int i=0;i<8;i++) write32le(regionReadMask_USR[i],os);
    for(int i=0;i<8;i++) write32le(regionReadMask_SYS[i],os);
    for(int i=0;i<8;i++) write32le(regionExecuteMask_USR[i],os);
    for(int i=0;i<8;i++) write32le(regionExecuteMask_SYS[i],os);
    for(int i=0;i<8;i++) write32le(regionWriteSet_USR[i],os);
    for(int i=0;i<8;i++) write32le(regionWriteSet_SYS[i],os);
    for(int i=0;i<8;i++) write32le(regionReadSet_USR[i],os);
    for(int i=0;i<8;i++) write32le(regionReadSet_SYS[i],os);
    for(int i=0;i<8;i++) write32le(regionExecuteSet_USR[i],os);
    for(int i=0;i<8;i++) write32le(regionExecuteSet_SYS[i],os);
}

bool armcp15_t::loadone(EMUFILE* is)
{
	if(!read32le(&IDCode,is)) return false;
	if(!read32le(&cacheType,is)) return false;
    if(!read32le(&TCMSize,is)) return false;
    if(!read32le(&ctrl,is)) return false;
    if(!read32le(&DCConfig,is)) return false;
    if(!read32le(&ICConfig,is)) return false;
    if(!read32le(&writeBuffCtrl,is)) return false;
    if(!read32le(&und,is)) return false;
    if(!read32le(&DaccessPerm,is)) return false;
    if(!read32le(&IaccessPerm,is)) return false;
    if(!read32le(&protectBaseSize0,is)) return false;
    if(!read32le(&protectBaseSize1,is)) return false;
    if(!read32le(&protectBaseSize2,is)) return false;
    if(!read32le(&protectBaseSize3,is)) return false;
    if(!read32le(&protectBaseSize4,is)) return false;
    if(!read32le(&protectBaseSize5,is)) return false;
    if(!read32le(&protectBaseSize6,is)) return false;
    if(!read32le(&protectBaseSize7,is)) return false;
    if(!read32le(&cacheOp,is)) return false;
    if(!read32le(&DcacheLock,is)) return false;
    if(!read32le(&IcacheLock,is)) return false;
    if(!read32le(&ITCMRegion,is)) return false;
    if(!read32le(&DTCMRegion,is)) return false;
    if(!read32le(&processID,is)) return false;
    if(!read32le(&RAM_TAG,is)) return false;
    if(!read32le(&testState,is)) return false;
    if(!read32le(&cacheDbg,is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionWriteMask_USR[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionWriteMask_SYS[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionReadMask_USR[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionReadMask_SYS[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionExecuteMask_USR[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionExecuteMask_SYS[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionWriteSet_USR[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionWriteSet_SYS[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionReadSet_USR[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionReadSet_SYS[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionExecuteSet_USR[i],is)) return false;
    for(int i=0;i<8;i++) if(!read32le(&regionExecuteSet_SYS[i],is)) return false;

    return true;
}

/* precalculate region masks/sets from cp15 register ----- JIT */
void maskPrecalc()
{
#define precalc(num) {  \
	u32 mask = 0, set = 0xFFFFFFFF ; /* (x & 0) == 0xFF..FF is allways false (disabled) */  \
	if (BIT_N(cp15.protectBaseSize##num,0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
	mask = MASKFROMREG(cp15.protectBaseSize##num) ;   \
	set = SETFROMREG(cp15.protectBaseSize##num) ; \
	if (SIZEIDENTIFIER(cp15.protectBaseSize##num)==0x1F)  \
	{   /* for the 4GB region, u32 suffers wraparound */   \
	mask = 0 ; set = 0 ;   /* (x & 0) == 0  is allways true (enabled) */  \
} \
}  \
	cp15.setSingleRegionAccess(cp15.DaccessPerm,cp15.IaccessPerm,num,mask,set) ;  \
}
	precalc(0) ;
	precalc(1) ;
	precalc(2) ;
	precalc(3) ;
	precalc(4) ;
	precalc(5) ;
	precalc(6) ;
	precalc(7) ;
#undef precalc
}

