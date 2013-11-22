/*
	Copyright 2006 yopyop
	Copyright 2007 shash
	Copyright 2007-2011 DeSmuME team

	This file is free software: you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation, either version 2 of the License, or
	(at your option) any later version.

	This file is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with the this software.  If not, see <http://www.gnu.org/licenses/>.
*/


#ifndef FIFO_H
#define FIFO_H

#include "types.h"

//=================================================== IPC FIFO
typedef struct
{
	u32		buf[16];
	
	u8		head;
	u8		tail;
	u8		size;
} IPC_FIFO;

extern IPC_FIFO ipc_fifo[2];
extern void IPC_FIFOinit(u8 proc);
extern void IPC_FIFOsend(u8 proc, u32 val);
extern u32 IPC_FIFOrecv(u8 proc);
extern void IPC_FIFOcnt(u8 proc, u16 val);

//=================================================== GFX FIFO

//yeah, its oversize for now. thats a simpler solution
//moon seems to overdrive the fifo with immediate dmas
//i think this might be nintendo code too
#define HACK_GXIFO_SIZE 200000

typedef struct
{
	u8		cmd[HACK_GXIFO_SIZE];
	u32		param[HACK_GXIFO_SIZE];

	u32		head;		// start position
	u32		tail;		// tail
	u32		size;		// size FIFO buffer
	u32		matrix_stack_op_size; //number of matrix stack items in the fifo (stack is busy when this is nonzero)
} GFX_FIFO;

typedef struct
{
	u8		cmd[4];
	u32		param[4];

	u8		head;
	u8		tail;
	u8		size;
} GFX_PIPE;

extern GFX_PIPE gxPIPE;
extern GFX_FIFO gxFIFO;
extern void GFX_PIPEclear();
extern void GFX_FIFOclear();
extern void GFX_FIFOsend(u8 cmd, u32 param);
extern BOOL GFX_PIPErecv(u8 *cmd, u32 *param);
extern void GFX_FIFOcnt(u32 val);

//=================================================== Display memory FIFO
typedef struct
{
	u32		buf[0x6000];			// 256x192 32K color
	u32		head;					// head
	u32		tail;					// tail
} DISP_FIFO;

extern DISP_FIFO disp_fifo;
extern void DISP_FIFOinit();
extern void DISP_FIFOsend(u32 val);
extern u32 DISP_FIFOrecv();

#endif
