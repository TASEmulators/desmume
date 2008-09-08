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

#ifndef FIFO_H
#define FIFO_H

#include "types.h"

typedef struct
{
       u32 data[0x8000];
       u32 begin;
       u32 end;
       BOOL full;
       BOOL empty;
       BOOL error;
} FIFO;

void FIFOInit(FIFO * fifo);
void FIFOAdd(FIFO * fifo, u32 v);
u32 FIFOValue(FIFO * fifo);

//================== 3D GFX FIFO
typedef struct{
	u32 hits[640];
	u32 hits_count;
	u32 empty;
	u32	half;
	u32	full;
	u32 begin;
	u32 end;
	u32 irq;
} GFXFIFO;

#endif
