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
IPC_FIFO ipc_fifo;

void IPC_FIFOclear()
{
	memset(&ipc_fifo, 0, sizeof(IPC_FIFO));
	//LOG("FIFO is cleared\n");
}

void IPC_FIFOsend(u8 proc, u32 val)
{
	//LOG("IPC%s send FIFO 0x%08X\n", proc?"7":"9", val);
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & 0x8000)) return;			// FIFO disabled
	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);

	if (ipc_fifo.sendTail[proc] < 16)		// last full == error
	{
		ipc_fifo.sendBuf[proc][ipc_fifo.sendTail[proc]] = val;
		ipc_fifo.sendTail[proc]++;
		if (ipc_fifo.sendTail[proc] == 16) cnt_l |= 0x02;	// full
		cnt_l &= 0xFFFE;

		if (ipc_fifo.recvTail[proc^1] < 16)	// last full == error
		{
			ipc_fifo.recvBuf[proc^1][ipc_fifo.recvTail[proc^1]] = val;
			ipc_fifo.recvTail[proc^1]++;
			if (ipc_fifo.recvTail[proc^1] == 16) cnt_r |= 0x0200;	// full
			cnt_r &= 0xFEFF;
		}
		else
			cnt_r |= 0x4200;
	}
	else
		cnt_l |= 0x4002;

	// save in mem
	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, cnt_r);

	if ((cnt_r & (1<<10)))
		NDS_makeInt(proc^1, 18);
}

u32 IPC_FIFOrecv(u8 proc)
{
	//LOG("IPC%s recv FIFO:\n", proc?"7":"9");
	u32 val = 0;
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);

	if (ipc_fifo.recvTail[proc] > 0)		// not empty
	{
		val = ipc_fifo.recvBuf[proc][0];
		for (int i = 0; i < ipc_fifo.recvTail[proc]; i++)
			ipc_fifo.recvBuf[proc][i] = ipc_fifo.recvBuf[proc][i+1];
		ipc_fifo.recvTail[proc]--;
		if (ipc_fifo.recvTail[proc] == 0)	// empty
			cnt_l |= 0x0100;

		// remove from head
		for (int i = 0; i < ipc_fifo.sendTail[proc^1]; i++)
			ipc_fifo.sendBuf[proc^1][i] = ipc_fifo.sendBuf[proc^1][i+1];
		ipc_fifo.sendTail[proc^1]--;
		if (ipc_fifo.sendTail[proc^1] == 0)	// empty
			cnt_r |= 0x0001;
	}
	else
		cnt_l |= 0x4100;

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, cnt_r);

	if ((cnt_l & (1<<3)))
		NDS_makeInt(proc, 19);
	return (val);
}

void IPC_FIFOcnt(u8 proc, u16 val)
{
	//LOG("IPC%s FIFO context 0x%X\n", proc?"7":"9", val);

	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);

	cnt_l &= ~0x8404;
	cnt_l |= (val & 0x8404);
	cnt_l &= (~(val & 0x4000));
	if (val & 0x0008)
	{
		IPC_FIFOclear();
		cnt_l |= 0x0101;
	}
	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);

	if ((cnt_l & 0x0004))
		NDS_makeInt(proc, 18);
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
