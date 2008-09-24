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
#include "debug.h"

void FIFOclear(FIFO * fifo)
{
	memset(fifo,0,sizeof(FIFO));
	fifo->empty = true;
}

void FIFOadd(FIFO *fifo, u32 val)
{
	if (fifo->full)
	{
		//printlog("!!!!! FIFOadd full\n");
		fifo->error = true;
		return;
	}

	fifo->buf[fifo->sendPos] = val;
	fifo->sendPos = (fifo->sendPos+1) & 0x7FFF;
	fifo->half = (fifo->sendPos < (sizeof(fifo->buf)>>1));
	fifo->full = (fifo->sendPos == fifo->recvPos);
	fifo->empty = false;
	//printlog("-------------- FIFO add size=%i, val=%X\n",fifo->sendPos, val);
}

u32 FIFOget(FIFO * fifo)
{
	if (fifo->empty)
	{
		//printlog("!!!!! FIFOget empty\n");
		fifo->error = true;
		return 0;
	}

	u32 val;
	val = fifo->buf[fifo->recvPos];
	fifo->recvPos = (fifo->recvPos+1) & 0x7FFF;
	fifo->empty = (fifo->recvPos == fifo->sendPos);
	//printlog("-------------- FIFO get size=%i, val=%X\n",fifo->recvPos, val);
	return val;
}
