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
	u32		buf[16];		// 16 words
	u8		size;			// tail

	BOOL	empty;
	BOOL	full;
	BOOL	error;
} IPC_FIFO;

typedef struct
{
	u32		buf[16];		// 16 words
	u8		size;			// tail

	BOOL	empty;
	BOOL	full;
	BOOL	error;
} GFX_FIFO;

typedef struct
{
	u32		buf[16];		// 16 words
	u8		size;			// tail

	BOOL	empty;
	BOOL	full;
	BOOL	error;
} DISP_FIFO;

extern void IPC_FIFOclear(IPC_FIFO * fifo);
extern void IPC_FIFOadd(IPC_FIFO * fifo, u32 val);
extern u32 IPC_FIFOget(IPC_FIFO * fifo);

//extern void GFX_FIFOclear(GFX_FIFO * fifo);
//extern void GFX_FIFOadd(GFX_FIFO * fifo, u32 val);
//extern u32 GFX_FIFOget(GFX_FIFO * fifo);

#endif
