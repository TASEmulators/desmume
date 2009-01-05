/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

	Copyright (C) 2007 shash

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

#include "FIFO.h"
#include <string.h>
#include "armcpu.h"
#include "debug.h"
#include "mem.h"
#include "MMU.h"

// ========================================================= IPC FIFO
IPC_FIFO ipc_fifo[2];		// 0 - ARM9
							// 1 - ARM7

void IPC_FIFOinit(u8 proc)
{
	ipc_fifo[proc].head = 0;
	ipc_fifo[proc].tail = 0;
	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, 0x00000101);
	NDS_makeInt(proc^1, 18); 
}

#define FIFO_IS_FULL(proc) ((ipc_fifo[proc].head) && (ipc_fifo[proc].head == ipc_fifo[proc].tail+1)) ||\
							((!ipc_fifo[proc].head) && (ipc_fifo[proc].tail == 15))

void IPC_FIFOsend(u8 proc, u32 val)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & 0x8000)) return;			// FIFO disabled

	if (FIFO_IS_FULL(proc))					// FIFO error
	{
		cnt_l |= 0x4000;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
		return;
	}

	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);
	cnt_l &= 0xFFFE;		// clear send empty bit
	cnt_r &= 0xFEFF;		// set recv empty bit
	ipc_fifo[proc].buf[ipc_fifo[proc].tail] = val;
	ipc_fifo[proc].tail++;
	if (ipc_fifo[proc].tail == 16)
			ipc_fifo[proc].tail = 0;

	if (FIFO_IS_FULL(proc))
	{
		cnt_l |= 0x0002;		// set send full bit
		cnt_r |= 0x0200;		// set recv full bit
	}

	//LOG("IPC%s send FIFO 0x%08X (l 0x%X, r 0x%X), head %02i, tail %02i\n", proc?"7":"9", val, cnt_l, cnt_r, ipc_fifo[proc].head, ipc_fifo[proc].tail);

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, cnt_r);

	MMU.reg_IF[proc^1] |= ( (cnt_r & 0x0400) << 8 );
}

u32 IPC_FIFOrecv(u8 proc)
{
	u32 val = 0;
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & 0x8000)) return (val);					// FIFO disabled

	if ( ipc_fifo[proc].head == ipc_fifo[proc].tail )		// FIFO error
	{
		cnt_l |= 0x4000;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
		return (val);
	}

	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);

	cnt_l &= 0xFFFD;		// clear send full bit
	cnt_r &= 0xFDFF;		// set recv full bit

	val = ipc_fifo[proc].buf[ipc_fifo[proc].head];
	ipc_fifo[proc].head++;
	if (ipc_fifo[proc].head == 16)
			ipc_fifo[proc].head = 0;
	if ( ipc_fifo[proc].head == ipc_fifo[proc].tail )		// FIFO empty
	{
		cnt_l |= 0x0001;
		cnt_r |= 0x0100;
	}

	//LOG("IPC%s recv FIFO 0x%08X (l 0x%X, r 0x%X), head %02i, tail %02i\n", proc?"9":"7", val, cnt_l, cnt_r, ipc_fifo[proc].head, ipc_fifo[proc].tail);
	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, cnt_r);

	MMU.reg_IF[proc^1] |= ( (cnt_r & 0x0004) << 9 );
	return (val);
}

void IPC_FIFOcnt(u8 proc, u16 val)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);
	cnt_r &= 0x3FFF;
	cnt_r |= (val & 0x8000);

	//LOG("IPC%s FIFO context 0x%X (local 0x%X)\n", proc?"7":"9", val, cnt_l);
	if (val & 0x4008)
	{
		u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);
		ipc_fifo[proc].head = 0;
		ipc_fifo[proc].tail = 0;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, (cnt_l & 0x0301) | (val & 0x8404) | 1);
		T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, (cnt_r & 0x8407) | 0x100);
		MMU.reg_IF[proc] |= ((cnt_l & 0x0004) << 16);
		return;
	}

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l | (val & 0xBFF4));
	T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, cnt_r);
	MMU.reg_IF[proc] |= ((cnt_l & 0x0004) << 16);
}

// ========================================================= GFX FIFO
GFX_FIFO	gxFIFO;

void GFX_FIFOclear()
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);

	memset(&gxFIFO, 0, sizeof(GFX_FIFO));

	// TODO: irq handle
	gxstat &= 0x0000FF00;
	gxstat |= 0x00000002;			// this is hack
	gxstat |= 0x86000000;
	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
}

void GFX_FIFOsend(u32 cmd, u32 param)
{
	u32 gxstat = T1ReadLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600);
	gxstat &= 0x0000FF00;
	gxstat |= 0x00000002;		// this is hack

	if (gxFIFO.tail < 260)
	{
		gxFIFO.cmd[gxFIFO.tail] = cmd & 0xFF;
		gxFIFO.param[gxFIFO.tail] = param;
		gxFIFO.tail++;
		// TODO: irq handle
		if (gxFIFO.tail < 130)
			gxstat |= 0x72000000;
		if (gxFIFO.tail == 16)
			gxstat |= 0x01000000;
	}
	else
		gxstat |= 0x01000000;

	T1WriteLong(MMU.MMU_MEM[ARMCPU_ARM9][0x40], 0x600, gxstat);
}
