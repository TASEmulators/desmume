/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2017 DeSmuME team

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

#define CP15_ACCESSTYPE(val, n)   (((val) >> (4*n)) & 0x0F)
/* sets the precalculated regions to mask,set for the affected accesstypes */
void armcp15_t::setSingleRegionAccess(u8 num, u32 mask, u32 set) {

	switch (CP15_ACCESSTYPE(DaccessPerm, num)) {
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
	switch (CP15_ACCESSTYPE(IaccessPerm, num)) {
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
	if (BIT_N(protectBaseSize[num],0)) /* if region is enabled */ \
	{    /* reason for this define: naming includes var */  \
	mask = CP15_MASKFROMREG(protectBaseSize[num]) ;   \
	set = CP15_SETFROMREG(protectBaseSize[num]) ; \
	if (CP15_SIZEIDENTIFIER(protectBaseSize[num])==0x1F)  \
	{   /* for the 4GB region, u32 suffers wraparound */   \
	mask = 0 ; set = 0 ;   /* (x & 0) == 0  is allways true (enabled) */  \
} \
}  \
	setSingleRegionAccess(num, mask, set) ;  \
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
	if(NDS_ARM9.CPSR.bits.mode == USR) return FALSE;

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
			if (CRm < 8)
			{
				*R = protectBaseSize[CRm];
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
	if(NDS_ARM9.CPSR.bits.mode == USR) return FALSE;

	switch(CRn)
	{
	case 1:
		if((opcode1==0) && (opcode2==0) && (CRm==0))
		{

			//On the NDS bit0,2,7,12..19 are R/W, Bit3..6 are always set, all other bits are always zero.
			ctrl = (val & 0x000FF085) | 0x00000078;
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
			if (CRm < 8)
			{
				protectBaseSize[CRm] = val;
				maskPrecalc();
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
void armcp15_t::saveone(EMUFILE &os)
{
	os.write_32LE(IDCode);
	os.write_32LE(cacheType);
    os.write_32LE(TCMSize);
    os.write_32LE(ctrl);
    os.write_32LE(DCConfig);
    os.write_32LE(ICConfig);
    os.write_32LE(writeBuffCtrl);
    os.write_32LE(und);
    os.write_32LE(DaccessPerm);
    os.write_32LE(IaccessPerm);
	for (int i=0;i<8;i++) os.write_32LE(protectBaseSize[i]);
    os.write_32LE(cacheOp);
    os.write_32LE(DcacheLock);
    os.write_32LE(IcacheLock);
    os.write_32LE(ITCMRegion);
    os.write_32LE(DTCMRegion);
    os.write_32LE(processID);
    os.write_32LE(RAM_TAG);
    os.write_32LE(testState);
    os.write_32LE(cacheDbg);
    for (int i=0;i<8;i++) os.write_32LE(regionWriteMask_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionWriteMask_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionReadMask_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionReadMask_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionExecuteMask_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionExecuteMask_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionWriteSet_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionWriteSet_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionReadSet_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionReadSet_SYS[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionExecuteSet_USR[i]);
    for (int i=0;i<8;i++) os.write_32LE(regionExecuteSet_SYS[i]);
}

bool armcp15_t::loadone(EMUFILE &is)
{
	if (!is.read_32LE(IDCode)) return false;
	if (!is.read_32LE(cacheType)) return false;
    if (!is.read_32LE(TCMSize)) return false;
    if (!is.read_32LE(ctrl)) return false;
    if (!is.read_32LE(DCConfig)) return false;
    if (!is.read_32LE(ICConfig)) return false;
    if (!is.read_32LE(writeBuffCtrl)) return false;
    if (!is.read_32LE(und)) return false;
    if (!is.read_32LE(DaccessPerm)) return false;
    if (!is.read_32LE(IaccessPerm)) return false;
	for (int i=0;i<8;i++) if (!is.read_32LE(protectBaseSize[i])) return false;
    if (!is.read_32LE(cacheOp)) return false;
    if (!is.read_32LE(DcacheLock)) return false;
    if (!is.read_32LE(IcacheLock)) return false;
    if (!is.read_32LE(ITCMRegion)) return false;
    if (!is.read_32LE(DTCMRegion)) return false;
    if (!is.read_32LE(processID)) return false;
    if (!is.read_32LE(RAM_TAG)) return false;
    if (!is.read_32LE(testState)) return false;
    if (!is.read_32LE(cacheDbg)) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionWriteMask_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionWriteMask_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionReadMask_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionReadMask_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionExecuteMask_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionExecuteMask_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionWriteSet_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionWriteSet_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionReadSet_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionReadSet_SYS[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionExecuteSet_USR[i])) return false;
    for (int i=0;i<8;i++) if (!is.read_32LE(regionExecuteSet_SYS[i])) return false;

    return true;
}
