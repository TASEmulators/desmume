/*  Copyright (C) 2006 yopyop
    yopyop156@ifrance.com
    yopyop156.ifrance.com

    Copyright 2007 shash
	Copyright 2007-2009 DeSmuME team

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
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
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
	memset(&ipc_fifo[proc], 0, sizeof(IPC_FIFO));
	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, 0x00000101);
}

void IPC_FIFOsend(u8 proc, u32 val)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & 0x8000)) return;			// FIFO disabled
	u8	proc_remote = proc ^ 1;

	if (ipc_fifo[proc].tail > 15)
	{
		cnt_l |= 0x4000;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
		return;
	}

	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc_remote][0x40], 0x184);

	//LOG("IPC%s send FIFO 0x%08X (l 0x%X, tail %02i) (r 0x%X, tail %02i)\n", 
	//	proc?"7":"9", val, cnt_l, ipc_fifo[proc].tail, cnt_r, ipc_fifo[proc^1].tail);
	
	cnt_l &= 0xBFFC;		// clear send empty bit & full
	cnt_r &= 0xBCFF;		// set recv empty bit & full
	ipc_fifo[proc].buf[ipc_fifo[proc].tail++] = val;
	
	if (ipc_fifo[proc].tail > 15)
	{
		cnt_l |= 0x0002;		// set send full bit
		cnt_r |= 0x0200;		// set recv full bit
	}

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc_remote][0x40], 0x184, cnt_r);

	MMU.reg_IF[proc_remote] |= ( (cnt_l & 0x0400) << 8 );
}

u32 IPC_FIFOrecv(u8 proc)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & 0x8000)) return (0);									// FIFO disabled
	u8	proc_remote = proc ^ 1;

	u32 val = 0;

	if ( ipc_fifo[proc_remote].tail == 0 )		// remote FIFO error
	{
		cnt_l |= 0x4000;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
		return (0);
	}

	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc_remote][0x40], 0x184);

	cnt_l &= 0xBCFF;		// clear send full bit & empty
	cnt_r &= 0xBFFC;		// set recv full bit & empty

	val = ipc_fifo[proc_remote].buf[0];
	
	//LOG("IPC%s recv FIFO 0x%08X (l 0x%X, tail %02i) (r 0x%X, tail %02i)\n", 
	//	proc?"7":"9", val, cnt_l, ipc_fifo[proc].tail, cnt_r, ipc_fifo[proc^1].tail);

	ipc_fifo[proc_remote].tail--;
	for (int i = 0; i < ipc_fifo[proc_remote].tail; i++)
		ipc_fifo[proc_remote].buf[i] = ipc_fifo[proc_remote].buf[i+1];;

	if ( ipc_fifo[proc_remote].tail == 0 )		// FIFO empty
	{
		cnt_l |= 0x0100;
		cnt_r |= 0x0001;
	}

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc_remote][0x40], 0x184, cnt_r);

	MMU.reg_IF[proc_remote] |= ( (cnt_l & 0x0004) << 15);

	return (val);
}

void IPC_FIFOcnt(u8 proc, u16 val)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);

	//LOG("IPC%s FIFO context 0x%X (local 0x%04X, remote 0x%04X)\n", proc?"7":"9", val, cnt_l, cnt_r);
	if (val & 0x4008)
	{
		ipc_fifo[proc].tail = 0;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, (cnt_l & 0x0301) | (val & 0x8404) | 1);
		T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, (cnt_r & 0x8407) | 0x100);
		MMU.reg_IF[proc^1] |= ((val & 0x0004) << 15);
		return;
	}

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, val);
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

// ========================================================= DISP FIFO
DISP_FIFO	disp_fifo;

void DISP_FIFOinit()
{
	memset(&disp_fifo, 0, sizeof(DISP_FIFO));
}

void DISP_FIFOsend(u32 val)
{
	//INFO("DISP_FIFO send value 0x%08X (head 0x%06X, tail 0x%06X)\n", val, disp_fifo.head, disp_fifo.tail);
	disp_fifo.buf[disp_fifo.tail] = val;
	disp_fifo.tail++;
	if (disp_fifo.tail > 0x5FFF)
		disp_fifo.tail = 0;
}

u32 DISP_FIFOrecv()
{
	if (disp_fifo.tail == disp_fifo.head) return (0); // FIFO is empty
	u32 val = disp_fifo.buf[disp_fifo.head];
	disp_fifo.head++;
	if (disp_fifo.head > 0x5FFF)
		disp_fifo.head = 0;
	return (val);
}