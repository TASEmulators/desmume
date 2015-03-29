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

#ifndef __CP15_H__
#define __CP15_H__

#include "armcpu.h"

typedef struct
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
        u32 protectBaseSize0;
        u32 protectBaseSize1;
        u32 protectBaseSize2;
        u32 protectBaseSize3;
        u32 protectBaseSize4;
        u32 protectBaseSize5;
        u32 protectBaseSize6;
        u32 protectBaseSize7;
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

	armcpu_t * cpu;

} armcp15_t;

armcp15_t *armcp15_new(armcpu_t *c);
BOOL armcp15_dataProcess(armcp15_t *armcp15, u8 CRd, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2);
BOOL armcp15_load(armcp15_t *armcp15, u8 CRd, u8 adr);
BOOL armcp15_store(armcp15_t *armcp15, u8 CRd, u8 adr);
BOOL armcp15_moveCP2ARM(armcp15_t *armcp15, u32 * R, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2);
BOOL armcp15_moveARM2CP(armcp15_t *armcp15, u32 val, u8 CRn, u8 CRm, u8 opcode1, u8 opcode2);
INLINE BOOL armcp15_isAccessAllowed(armcp15_t *armcp15,u32 address,u32 access) ;


#define CP15_ACCESS_WRITE         0
#define CP15_ACCESS_READ          2
#define CP15_ACCESS_EXECUTE       4
#define CP15_ACCESS_WRITEUSR      CP15_ACCESS_WRITE
#define CP15_ACCESS_WRITESYS      1
#define CP15_ACCESS_READUSR       CP15_ACCESS_READ
#define CP15_ACCESS_READSYS       3
#define CP15_ACCESS_EXECUSR       CP15_ACCESS_EXECUTE
#define CP15_ACCESS_EXECSYS       5

#endif /* __CP15_H__*/
