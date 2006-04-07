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

#include "types.h"

#include "armcpu.h"

typedef struct
{
	unsigned long IDCode;
	unsigned long cacheType;
	unsigned long TCMSize;
	unsigned long ctrl;
	unsigned long DCConfig;
	unsigned long ICConfig;
	unsigned long writeBuffCtrl;
	unsigned long und;
	unsigned long DaccessPerm;
	unsigned long IaccessPerm;
	unsigned long protectBaseSize0;
	unsigned long protectBaseSize1;
	unsigned long protectBaseSize2;
	unsigned long protectBaseSize3;
	unsigned long protectBaseSize4;
	unsigned long protectBaseSize5;
	unsigned long protectBaseSize6;
	unsigned long protectBaseSize7;
	unsigned long cacheOp;
	unsigned long DcacheLock;
	unsigned long IcacheLock;
	unsigned long ITCMRegion;
	unsigned long DTCMRegion;
	unsigned long processID;
	unsigned long RAM_TAG;
	unsigned long testState;
	unsigned long cacheDbg;

	armcpu_t * cpu;

} armcp15_t;

armcp15_t *armcp15_new(armcpu_t *c);
bool armcp15_dataProcess(armcp15_t *armcp15, unsigned char CRd, unsigned char CRn, unsigned char CRm, unsigned char opcode1, unsigned char opcode2);
bool armcp15_load(armcp15_t *armcp15, unsigned char CRd, unsigned char adr);
bool armcp15_store(armcp15_t *armcp15, unsigned char CRd, unsigned char adr);
bool armcp15_moveCP2ARM(armcp15_t *armcp15, unsigned long * R, unsigned char CRn, unsigned char CRm, unsigned char opcode1, unsigned char opcode2);
bool armcp15_moveARM2CP(armcp15_t *armcp15, unsigned long val, unsigned char CRn, unsigned char CRm, unsigned char opcode1, unsigned char opcode2);

#endif /* __CP15_H__*/
