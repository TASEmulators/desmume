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

#ifndef __CP15_H__
#define __CP15_H__

#include <string.h>
#include "types.h"

class EMUFILE;

#define CP15_ACCESS_WRITE         0
#define CP15_ACCESS_READ          2
#define CP15_ACCESS_EXECUTE       4
#define CP15_ACCESS_WRITEUSR      CP15_ACCESS_WRITE
#define CP15_ACCESS_WRITESYS      1
#define CP15_ACCESS_READUSR       CP15_ACCESS_READ
#define CP15_ACCESS_READSYS       3
#define CP15_ACCESS_EXECUSR       CP15_ACCESS_EXECUTE
#define CP15_ACCESS_EXECSYS       5

#define CP15_SIZEBINARY(val)     (1 << (CP15_SIZEIDENTIFIER(val)+1))
#define CP15_SIZEIDENTIFIER(val) ((((val) >> 1) & 0x1F))
#define CP15_MASKFROMREG(val)    (~((CP15_SIZEBINARY(val)-1) | 0x3F))
#define CP15_SETFROMREG(val)     ((val) & CP15_MASKFROMREG(val))

struct armcp15_t
{
        u32 IDCode;
        u32 cacheType;
        u32 TCMSize;
        u32 ctrl;
        u32 DCConfig;
        u32 ICConfig;
        u32 writeBuffCtrl;
        u32 und;
        u32 DaccessPerm;
        u32 IaccessPerm;
        u32 protectBaseSize[8];
        u32 cacheOp;
        u32 DcacheLock;
        u32 IcacheLock;
        u32 ITCMRegion;
        u32 DTCMRegion;
        u32 processID;
        u32 RAM_TAG;
        u32 testState;
        u32 cacheDbg;
        /* calculated bitmasks for the regions to decide rights uppon */
        /* calculation is done in the MCR instead of on mem access for performance */
        u32 regionWriteMask_USR[8] ;
        u32 regionWriteMask_SYS[8] ;
        u32 regionReadMask_USR[8] ;
        u32 regionReadMask_SYS[8] ;
        u32 regionExecuteMask_USR[8] ;
        u32 regionExecuteMask_SYS[8] ;
        u32 regionWriteSet_USR[8] ;
        u32 regionWriteSet_SYS[8] ;
        u32 regionReadSet_USR[8] ;
        u32 regionReadSet_SYS[8] ;
        u32 regionExecuteSet_USR[8] ;
        u32 regionExecuteSet_SYS[8] ;
};

void armcp15_init(armcp15_t *armcp15);

void armcp15_setSingleRegionAccess(armcp15_t *armcp15, u8 num, u32 mask, u32 set);
void armcp15_maskPrecalc(armcp15_t *armcp15);

BOOL armcp15_dataProcess(armcp15_t *armcp15, u8 CRd, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2);
BOOL armcp15_load(armcp15_t *armcp15, u8 CRd, u8 adr);
BOOL armcp15_store(armcp15_t *armcp15, u8 CRd, u8 adr);
BOOL armcp15_moveCP2ARM(armcp15_t *armcp15, u32 *R, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2);
BOOL armcp15_moveARM2CP(armcp15_t *armcp15, u32 val, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2);
BOOL armcp15_isAccessAllowed(armcp15_t *armcp15, u32 address, u32 access);
// savestate
void armcp15_saveone(armcp15_t *armcp15, EMUFILE &os);
bool armcp15_loadone(armcp15_t *armcp15, EMUFILE &is);

extern armcp15_t cp15;
#endif /* __CP15_H__*/
