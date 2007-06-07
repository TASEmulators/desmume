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

void FIFOInit(FIFO * fifo)
{
        u32 i;

	fifo->begin = 0;
        fifo->end = 0;
        for(i = 0; i<0x8000; ++i)
        	fifo->data[i] = 0;
        fifo->full = FALSE;
        fifo->empty = TRUE;
        fifo->error = FALSE;
}

void FIFOAdd(FIFO * fifo, u32 v)
{
	if(fifo->full) 
        {
        	fifo->error = TRUE;
                return;
        }
        fifo->data[fifo->end] = v;
        fifo->end = (fifo->end + 1)& 0x7FFF;
        fifo->full = (fifo->end == fifo->begin);
        fifo->empty = FALSE;
}
       
u32 FIFOValue(FIFO * fifo)
{
        u32 v;

	if(fifo->empty)
        {
        	fifo->error = TRUE;
                return 0;
        }
        v = fifo->data[fifo->begin];
        fifo->begin = (fifo->begin + 1)& 0x7FFF;
        fifo->empty = (fifo->begin == fifo->end);
        return v;
}
