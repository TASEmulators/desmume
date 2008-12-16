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

//=================================================== IPC FIFO
typedef struct
{
	u32		buf[16];		// 64K
	u8		size;			// tail

	BOOL	empty;
	BOOL	full;
	BOOL	error;
} IPC_FIFO;

extern void IPC_FIFOclear(IPC_FIFO * fifo);
extern void IPC_FIFOadd(IPC_FIFO * fifo, u32 val);
extern u32 IPC_FIFOget(IPC_FIFO * fifo);

//=================================================== GFX FIFO
typedef struct
{
	u32		size;					// tail

	BOOL	empty;
	BOOL	half;
	BOOL	full;
	BOOL	error;
	u8		irq;

	u32		pipe[4];		// additional 4 entries
	u8		pipe_size;		// pipe tail

	BOOL	pipe_empty;
	BOOL	pipe_half;
	BOOL	pipe_full;
} GFX_FIFO;
extern void GFX_FIFOclear(GFX_FIFO * fifo);
extern void GFX_FIFOadd(GFX_FIFO * fifo);

//=================================================== Display memory FIFO
#if 0
typedef struct
{
	u32		buf[16];		// 16 words
	u8		size;			// tail

	BOOL	empty;
	BOOL	full;
	BOOL	error;
} DISP_FIFO;

extern void DISP_FIFOclear(DISP_FIFO * fifo);
extern void DISP_FIFOadd(DISP_FIFO * fifo, u32 val);
extern u32 DISP_FIFOget(DISP_FIFO * fifo);
#endif

#endif
