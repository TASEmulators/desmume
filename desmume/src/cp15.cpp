/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2021 DeSmuME team

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

#include "armcpu.h"
#include "cp15.h"
#include "debug.h"
#include "MMU.h"
#include "emufile.h"
#include "readwrite.h"
#include "utils/bits.h"

armcp15_t cp15;

void armcp15_init(armcp15_t *armcp15)
{
	armcp15->IDCode = 0x41059461;
	armcp15->cacheType = 0x0F0D2112;
	armcp15->TCMSize = 0x00140180;
	armcp15->ctrl = 0x00012078;
	armcp15->DCConfig = 0;
	armcp15->ICConfig = 0;
	armcp15->writeBuffCtrl = 0;
	armcp15->und = 0;
	armcp15->DaccessPerm = 0x22222222;
	armcp15->IaccessPerm = 0x22222222;
	armcp15->cacheOp = 0;
	armcp15->DcacheLock = 0;
	armcp15->IcacheLock = 0;
	armcp15->ITCMRegion = 0x0C;
	armcp15->DTCMRegion = 0x0080000A;
	armcp15->processID = 0;
	armcp15->RAM_TAG = 0;
	armcp15->testState = 0;
	armcp15->cacheDbg = 0;
	
	//printf("CP15 Reset\n");
	memset(&armcp15->protectBaseSize[0], 0, sizeof(armcp15->protectBaseSize));
	memset(&armcp15->regionWriteMask_USR[0], 0, sizeof(armcp15->regionWriteMask_USR));
	memset(&armcp15->regionWriteMask_SYS[0], 0, sizeof(armcp15->regionWriteMask_SYS));
	memset(&armcp15->regionReadMask_USR[0], 0, sizeof(armcp15->regionReadMask_USR));
	memset(&armcp15->regionReadMask_SYS[0], 0, sizeof(armcp15->regionReadMask_SYS));
	memset(&armcp15->regionExecuteMask_USR[0], 0, sizeof(armcp15->regionExecuteMask_USR));
	memset(&armcp15->regionExecuteMask_SYS[0], 0, sizeof(armcp15->regionExecuteMask_SYS));
	memset(&armcp15->regionWriteSet_USR[0], 0, sizeof(armcp15->regionWriteSet_USR));
	memset(&armcp15->regionWriteSet_SYS[0], 0, sizeof(armcp15->regionWriteSet_SYS));
	memset(&armcp15->regionReadSet_USR[0], 0, sizeof(armcp15->regionReadSet_USR));
	memset(&armcp15->regionReadSet_SYS[0], 0, sizeof(armcp15->regionReadSet_SYS));
	memset(&armcp15->regionExecuteSet_USR[0], 0, sizeof(armcp15->regionExecuteSet_USR));
	memset(&armcp15->regionExecuteSet_SYS[0], 0, sizeof(armcp15->regionExecuteSet_SYS));
}

#define CP15_ACCESSTYPE(val, n)   (((val) >> (4*n)) & 0x0F)
/* sets the precalculated regions to mask,set for the affected accesstypes */
void armcp15_setSingleRegionAccess(armcp15_t *armcp15, u8 num, u32 mask, u32 set)
{

	switch (CP15_ACCESSTYPE(armcp15->DaccessPerm, num)) {
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
	switch (CP15_ACCESSTYPE(armcp15->IaccessPerm, num)) {
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
	u32 mask = 0, set = 0xFFFFFFFF ; /* (x & 0) == 0xFF..FF is always false (disabled) */  \
	if (BIT_N(armcp15->protectBaseSize[num],0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
	mask = CP15_MASKFROMREG(armcp15->protectBaseSize[num]) ;   \
	set = CP15_SETFROMREG(armcp15->protectBaseSize[num]) ; \
	if (CP15_SIZEIDENTIFIER(armcp15->protectBaseSize[num])==0x1F)  \
	{   /* for the 4GB region, u32 suffers wraparound */   \
	mask = 0 ; set = 0 ;   /* (x & 0) == 0  is always true (enabled) */  \
} \
}  \
	armcp15_setSingleRegionAccess(armcp15, num, mask, set) ;  \
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

BOOL armcp15_isAccessAllowed(armcp15_t *armcp15, u32 address,u32 access)
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
	LOG("Unsupported CP15 operation : DataProcess\n");
	return FALSE;
}

BOOL armcp15_load(armcp15_t *armcp15, u8 CRd, u8 adr)
{
	LOG("Unsupported CP15 operation : Load\n");
	return FALSE;
}

BOOL armcp15_store(armcp15_t *armcp15, u8 CRd, u8 adr)
{
	LOG("Unsupported CP15 operation : Store\n");
	return FALSE;
}

BOOL armcp15_moveCP2ARM(armcp15_t *armcp15, u32 * R, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	if(NDS_ARM9.CPSR.bits.mode == USR) return FALSE;

	switch(CRn)
	{
	case 0:
		if((opcode1 == 0)&&(CRm==0))
		{
			switch(opcode2)
			{
			case 1:
				*R = armcp15->cacheType;
				return TRUE;
			case 2:
				*R = armcp15->TCMSize;
				return TRUE;
			default:
				*R = armcp15->IDCode;
				return TRUE;
			}
		}
		return FALSE;
	case 1:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			*R = armcp15->ctrl;
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
				*R = armcp15->DCConfig;
				return TRUE;
			case 1:
				*R = armcp15->ICConfig;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 3:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			*R = armcp15->writeBuffCtrl;
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
				*R = armcp15->DaccessPerm;
				return TRUE;
			case 3:
				*R = armcp15->IaccessPerm;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 6:
		if((opcode1==0) && (opcode2==0))
		{
			if (CRm < 8)
			{
				*R = armcp15->protectBaseSize[CRm];
				return TRUE;
			}
		}
		return FALSE;
	case 9:
		if(opcode1==0)
		{
			switch(CRm)
			{
			case 0:
				switch(opcode2)
				{
				case 0:
					*R = armcp15->DcacheLock;
					return TRUE;
				case 1:
					*R = armcp15->IcacheLock;
					return TRUE;
				default:
					return FALSE;
				}
			case 1:
				switch(opcode2)
				{
				case 0:
					*R = armcp15->DTCMRegion;
					return TRUE;
				case 1:
					*R = armcp15->ITCMRegion;
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

BOOL armcp15_moveARM2CP(armcp15_t *armcp15, u32 val, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2)
{
	if(NDS_ARM9.CPSR.bits.mode == USR) return FALSE;

	switch(CRn)
	{
	case 1:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{

			//On the NDS bit0,2,7,12..19 are R/W, Bit3..6 are always set, all other bits are always zero.
			armcp15->ctrl = (val & 0x000FF085) | 0x00000078;
			MMU.ARM9_RW_MODE = BIT7(val);
			//zero 31-jan-2010: change from 0x0FFF0000 to 0xFFFF0000 per gbatek
			NDS_ARM9.intVector = 0xFFFF0000 * (BIT13(val));
			NDS_ARM9.LDTBit = !BIT15(val); //TBit
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
				armcp15->DCConfig = val;
				return TRUE;
			case 1:
				armcp15->ICConfig = val;
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 3:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{
			armcp15->writeBuffCtrl = val;
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
				armcp15->DaccessPerm = val;
				armcp15_maskPrecalc(armcp15);
				return TRUE;
			case 3:
				armcp15->IaccessPerm = val;
				armcp15_maskPrecalc(armcp15);
				return TRUE;
			default:
				return FALSE;
			}
		}
		return FALSE;
	case 6:
		if((opcode1==0) && (opcode2==0))
		{
			if (CRm < 8)
			{
				armcp15->protectBaseSize[CRm] = val;
				armcp15_maskPrecalc(armcp15);
				return TRUE;
			}
		}
		return FALSE;
	case 7:
		if((CRm==0)&&(opcode1==0)&&((opcode2==4)))
		{
			//CP15wait4IRQ;
			NDS_ARM9.freeze = CPU_FREEZE_IRQ_IE_IF;
			//IME set deliberately omitted: only SWI sets IME to 1
			return TRUE;
		}
		return FALSE;
	case 9:
		if(opcode1 == 0)
		{
			switch(CRm)
			{
			case 0:
				switch(opcode2)
				{
				case 0:
					armcp15->DcacheLock = val;
					return TRUE;
				case 1:
					armcp15->IcacheLock = val;
					return TRUE;
				default:
					return FALSE;
				}
			case 1:
				switch(opcode2)
				{
				case 0:
					MMU.DTCMRegion = armcp15->DTCMRegion = val & 0x0FFFF000;
					return TRUE;
				case 1:
					armcp15->ITCMRegion = val;
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
void armcp15_saveone(armcp15_t *armcp15, EMUFILE &os)
{
	os.write_32LE(armcp15->IDCode);
	os.write_32LE(armcp15->cacheType);
    os.write_32LE(armcp15->TCMSize);
    os.write_32LE(armcp15->ctrl);
    os.write_32LE(armcp15->DCConfig);
    os.write_32LE(armcp15->ICConfig);
    os.write_32LE(armcp15->writeBuffCtrl);
    os.write_32LE(armcp15->und);
    os.write_32LE(armcp15->DaccessPerm);
    os.write_32LE(armcp15->IaccessPerm);
	for (int i=0;i<8;i++) os.write_32LE(armcp15->protectBaseSize[i]);
    os.write_32LE(armcp15->cacheOp);
    os.write_32LE(armcp15->DcacheLock);
    os.write_32LE(armcp15->IcacheLock);
    os.write_32LE(armcp15->ITCMRegion);
    os.write_32LE(armcp15->DTCMRegion);
    os.write_32LE(armcp15->processID);
    os.write_32LE(armcp15->RAM_TAG);
    os.write_32LE(armcp15->testState);
    os.write_32LE(armcp15->cacheDbg);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionWriteMask_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionWriteMask_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionReadMask_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionReadMask_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionExecuteMask_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionExecuteMask_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionWriteSet_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionWriteSet_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionReadSet_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionReadSet_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionExecuteSet_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(armcp15->regionExecuteSet_SYS[i]);
}

bool armcp15_loadone(armcp15_t *armcp15, EMUFILE &is)
{
	if (!is.read_32LE(armcp15->IDCode)) return false;
	if (!is.read_32LE(armcp15->cacheType)) return false;
    if (!is.read_32LE(armcp15->TCMSize)) return false;
    if (!is.read_32LE(armcp15->ctrl)) return false;
    if (!is.read_32LE(armcp15->DCConfig)) return false;
    if (!is.read_32LE(armcp15->ICConfig)) return false;
    if (!is.read_32LE(armcp15->writeBuffCtrl)) return false;
    if (!is.read_32LE(armcp15->und)) return false;
    if (!is.read_32LE(armcp15->DaccessPerm)) return false;
    if (!is.read_32LE(armcp15->IaccessPerm)) return false;
	for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->protectBaseSize[i])) return false;
    if (!is.read_32LE(armcp15->cacheOp)) return false;
    if (!is.read_32LE(armcp15->DcacheLock)) return false;
    if (!is.read_32LE(armcp15->IcacheLock)) return false;
    if (!is.read_32LE(armcp15->ITCMRegion)) return false;
    if (!is.read_32LE(armcp15->DTCMRegion)) return false;
    if (!is.read_32LE(armcp15->processID)) return false;
    if (!is.read_32LE(armcp15->RAM_TAG)) return false;
    if (!is.read_32LE(armcp15->testState)) return false;
    if (!is.read_32LE(armcp15->cacheDbg)) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionWriteMask_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionWriteMask_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionReadMask_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionReadMask_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionExecuteMask_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionExecuteMask_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionWriteSet_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionWriteSet_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionReadSet_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionReadSet_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionExecuteSet_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(armcp15->regionExecuteSet_SYS[i])) return false;

    return true;
}
