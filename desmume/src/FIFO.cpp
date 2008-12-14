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

void IPC_FIFOclear(IPC_FIFO * fifo)
{
	memset(fifo, 0, sizeof(fifo));

	fifo->empty = TRUE;
	//LOG("FIFO is cleared\n");
}
void IPC_FIFOadd(IPC_FIFO * fifo, u32 val)
{
	if (fifo->full)
	{
		//LOG("FIFO send is full\n");
		fifo->error = true;
		return;
	}

	//LOG("IPC FIFO add value 0x%08X in pos %i\n", val, fifo->size);

	fifo->buf[fifo->size] = val;
	fifo->size++;
	if (fifo->size == 16)
		fifo->full = TRUE;
	fifo->empty = FALSE;
}

extern void NDS_Pause();
u32 IPC_FIFOget(IPC_FIFO * fifo)
{
	if (fifo->empty)
	{
		fifo->error = true;
		//LOG("FIFO get is empty\n");
		return(0);
	}

	u32 val = fifo->buf[0];
	//LOG("IPC FIFO get value 0x%08X in pos %i\n", val, fifo->size);
	for (int i = 0; i < fifo->size; i++)
		fifo->buf[i] = fifo->buf[i+1];
	fifo->size--;
	if (fifo->size == 0)
		fifo->empty = TRUE;
	return val;
}

