/*
	Copyright 2006 yopyop
	Copyright 2007 shash
	Copyright 2007-2021 DeSmuME team

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

#include "FIFO.h"

#include <string.h>

#include "armcpu.h"
#include "debug.h"
#include "mem.h"
#include "MMU.h"
#include "registers.h"
#include "NDSSystem.h"
#include "gfx3d.h"

#if defined(ENABLE_AVX512_1)
	#define USEVECTORSIZE_512
	#define VECTORSIZE 64
#elif defined(ENABLE_AVX2)
	#define USEVECTORSIZE_256
	#define VECTORSIZE 32
#elif defined(ENABLE_SSE2)
	#define USEVECTORSIZE_128
	#define VECTORSIZE 16
#elif defined(ENABLE_ALTIVEC)
	#define USEVECTORSIZE_128
	#define VECTORSIZE 16
	#include "./utils/colorspacehandler/colorspacehandler_AltiVec.h"
#endif

#if defined(USEVECTORSIZE_512) || defined(USEVECTORSIZE_256) || defined(USEVECTORSIZE_128)
	#define USEMANUALVECTORIZATION
#endif


// ========================================================= IPC FIFO
IPC_FIFO ipc_fifo[2];

void IPC_FIFOinit(u8 proc)
{
	memset(&ipc_fifo[proc], 0, sizeof(IPC_FIFO));
	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, 0x00000101);
}

void IPC_FIFOsend(u8 proc, u32 val)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & IPCFIFOCNT_FIFOENABLE)) return;			// FIFO disabled
	u8	proc_remote = proc ^ 1;

	if (ipc_fifo[proc].size > 15)
	{
		cnt_l |= IPCFIFOCNT_FIFOERROR;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
		return;
	}

	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc_remote][0x40], 0x184);

	//LOG("IPC%s send FIFO 0x%08X size %03i (l 0x%X, tail %02i) (r 0x%X, tail %02i)\n", 
	//	proc?"7":"9", val, ipc_fifo[proc].size, cnt_l, ipc_fifo[proc].tail, cnt_r, ipc_fifo[proc^1].tail);
	
	cnt_l &= 0xBFFC;		// clear send empty bit & full
	cnt_r &= 0xBCFF;		// set recv empty bit & full
	ipc_fifo[proc].buf[ipc_fifo[proc].tail] = val;
	ipc_fifo[proc].tail++;
	ipc_fifo[proc].size++;
	if (ipc_fifo[proc].tail > 15) ipc_fifo[proc].tail = 0;
	
	if (ipc_fifo[proc].size > 15)
	{
		cnt_l |= IPCFIFOCNT_SENDFULL;		// set send full bit
		cnt_r |= IPCFIFOCNT_RECVFULL;		// set recv full bit
	}

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc_remote][0x40], 0x184, cnt_r);

	if(cnt_r&IPCFIFOCNT_RECVIRQEN)
		NDS_makeIrq(proc_remote, IRQ_BIT_IPCFIFO_RECVNONEMPTY);

	NDS_Reschedule();
}

u32 IPC_FIFOrecv(u8 proc)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	if (!(cnt_l & IPCFIFOCNT_FIFOENABLE)) return (0);									// FIFO disabled
	u8	proc_remote = proc ^ 1;

	u32 val = 0;

	if ( ipc_fifo[proc_remote].size == 0 )		// remote FIFO error
	{
		cnt_l |= IPCFIFOCNT_FIFOERROR;
		T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
		return (0);
	}

	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc_remote][0x40], 0x184);

	cnt_l &= 0xBCFF;		// clear send full bit & empty
	cnt_r &= 0xBFFC;		// set recv full bit & empty

	val = ipc_fifo[proc_remote].buf[ipc_fifo[proc_remote].head];
	ipc_fifo[proc_remote].head++;
	ipc_fifo[proc_remote].size--;
	if (ipc_fifo[proc_remote].head > 15) ipc_fifo[proc_remote].head = 0;
	
	//LOG("IPC%s recv FIFO 0x%08X size %03i (l 0x%X, tail %02i) (r 0x%X, tail %02i)\n", 
	//	proc?"7":"9", val, ipc_fifo[proc].size, cnt_l, ipc_fifo[proc].tail, cnt_r, ipc_fifo[proc^1].tail);

	if ( ipc_fifo[proc_remote].size == 0 )		// FIFO empty
	{
		cnt_l |= IPCFIFOCNT_RECVEMPTY;
		cnt_r |= IPCFIFOCNT_SENDEMPTY;

		if(cnt_r&IPCFIFOCNT_SENDIRQEN)
			NDS_makeIrq(proc_remote, IRQ_BIT_IPCFIFO_SENDEMPTY);
	}

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc_remote][0x40], 0x184, cnt_r);

	NDS_Reschedule();

	return (val);
}

void IPC_FIFOcnt(u8 proc, u16 val)
{
	u16 cnt_l = T1ReadWord(MMU.MMU_MEM[proc][0x40], 0x184);
	u16 cnt_r = T1ReadWord(MMU.MMU_MEM[proc^1][0x40], 0x184);

	if (val & IPCFIFOCNT_FIFOERROR)
	{
		//at least SPP uses this, maybe every retail game
		cnt_l &= ~IPCFIFOCNT_FIFOERROR;
	}

	if (val & IPCFIFOCNT_SENDCLEAR)
	{
		ipc_fifo[proc].head = 0; ipc_fifo[proc].tail = 0; ipc_fifo[proc].size = 0;

		cnt_l |= IPCFIFOCNT_SENDEMPTY;
		cnt_r |= IPCFIFOCNT_RECVEMPTY;
		
		cnt_l &= ~IPCFIFOCNT_SENDFULL;
		cnt_r &= ~IPCFIFOCNT_RECVFULL;

	}
	cnt_l &= ~IPCFIFOCNT_WRITEABLE;
	cnt_l |= val & IPCFIFOCNT_WRITEABLE;

	//IPCFIFOCNT_SENDIRQEN may have been set (and/or the fifo may have been cleared) so we may need to trigger this irq
	//(this approach is used by libnds fifo system on occasion in fifoInternalSend, and began happening frequently for value32 with r4326)
	if(cnt_l&IPCFIFOCNT_SENDIRQEN) if(cnt_l & IPCFIFOCNT_SENDEMPTY)
		NDS_makeIrq(proc, IRQ_BIT_IPCFIFO_SENDEMPTY);

	//IPCFIFOCNT_RECVIRQEN may have been set so we may need to trigger this irq
	if(cnt_l&IPCFIFOCNT_RECVIRQEN) if(!(cnt_l & IPCFIFOCNT_RECVEMPTY))
		NDS_makeIrq(proc, IRQ_BIT_IPCFIFO_RECVNONEMPTY);

	T1WriteWord(MMU.MMU_MEM[proc][0x40], 0x184, cnt_l);
	T1WriteWord(MMU.MMU_MEM[proc^1][0x40], 0x184, cnt_r);

	NDS_Reschedule();
}

// ========================================================= GFX FIFO
GFX_PIPE	gxPIPE;
GFX_FIFO	gxFIFO;

void GFX_PIPEclear()
{
	gxPIPE.head = 0;
	gxPIPE.tail = 0;
	gxPIPE.size = 0;
	gxFIFO.matrix_stack_op_size = 0;
}

void GFX_FIFOclear()
{
	gxFIFO.head = 0;
	gxFIFO.tail = 0;
	gxFIFO.size = 0;
	gxFIFO.matrix_stack_op_size = 0;
}

static void GXF_FIFO_handleEvents()
{
	bool low = gxFIFO.size <= 127;
	bool lowchange = MMU_new.gxstat.fifo_low ^ low;
	MMU_new.gxstat.fifo_low = low;
	if(low) triggerDma(EDMAMode_GXFifo);

	bool empty = gxFIFO.size == 0;
	bool emptychange = MMU_new.gxstat.fifo_empty ^ empty;
	MMU_new.gxstat.fifo_empty = empty;


	MMU_new.gxstat.sb = gxFIFO.matrix_stack_op_size != 0;

	if(emptychange||lowchange) NDS_Reschedule();
}

static bool IsMatrixStackCommand(u8 cmd)
{
	return cmd == 0x11 || cmd == 0x12;
}

void GFX_FIFOsend(u8 cmd, u32 param)
{
	//INFO("gxFIFO: send 0x%02X = 0x%08X (size %03i/0x%02X) gxstat 0x%08X\n", cmd, param, gxFIFO.size, gxFIFO.size, gxstat);
	//printf("fifo recv: %02X: %08X upto:%d\n",cmd,param,gxFIFO.size+1);

	//TODO - WOAH ! NOT HANDLING A TOO-BIG FIFO RIGHT NOW!
	//if (gxFIFO.size > 255)
	//{
	//	GXF_FIFO_handleEvents();
	//	//NEED TO HANDLE THIS!!!!!!!!!!!!!!!!!!!!!!!!!!

	//	//gxstat |= 0x08000000;			// busy
	//	NDS_RescheduleGXFIFO(1);
	//	//INFO("ERROR: gxFIFO is full (cmd 0x%02X = 0x%08X) (prev cmd 0x%02X = 0x%08X)\n", cmd, param, gxFIFO.cmd[255], gxFIFO.param[255]);
	//	return;		
	//}


	gxFIFO.cmd[gxFIFO.tail] = cmd;
	gxFIFO.param[gxFIFO.tail] = param;
	gxFIFO.tail++;
	gxFIFO.size++;
	if (gxFIFO.tail > HACK_GXIFO_SIZE-1) gxFIFO.tail = 0;

	//if a matrix op is entering the pipeline, do accounting for it
	//(this is tested by wild west, which will jam a few ops in the fifo and then wait for the matrix stack to be 
	//un-busy so it can read back the current matrix stack position).
	//it is definitely only pushes and pops which set this flag.
	//seems like it would be less work in the HW to make a counter than do cmps on all the command bytes, so maybe we're even doing it right.
	if(IsMatrixStackCommand(cmd))
		gxFIFO.matrix_stack_op_size++;

	//along the same lines:
	//american girls julie finds a way will put a bunch of stuff and then a box test into the fifo and then immediately test the busy flag
	//so we need to set the busy flag here.
	//does it expect the fifo to be running then? well, it's definitely jammed -- making it unjammed at one point did fix this bug.
	//it's still not clear whether we're handling the immediate vs fifo commands properly at all :(
	//anyway, here we go, similar treatment. consider this a hack.
	if(cmd == 0x70) MMU_new.gxstat.tb = 1; //just set the flag--youre insane if you queue more than one of these anyway
	if(cmd == 0x71) MMU_new.gxstat.tb = 1;

	if(gxFIFO.size>=HACK_GXIFO_SIZE) {
		printf("--FIFO FULL-- : %d\n",gxFIFO.size);
	}
	
	//gxstat |= 0x08000000;		// set busy flag

	GXF_FIFO_handleEvents();

	NDS_RescheduleGXFIFO(1);
}

// this function used ONLY in gxFIFO
BOOL GFX_PIPErecv(u8 *cmd, u32 *param)
{
	//gxstat &= 0xF7FFFFFF;		// clear busy flag

	if (gxFIFO.size == 0)
	{
		GXF_FIFO_handleEvents();
		return FALSE;
	}

	*cmd = gxFIFO.cmd[gxFIFO.head];
	*param = gxFIFO.param[gxFIFO.head];

	//see the associated increment in another function
	if(IsMatrixStackCommand(*cmd))
	{
		gxFIFO.matrix_stack_op_size--;
		if(gxFIFO.matrix_stack_op_size>0x10000000)
			printf("bad news disaster in matrix_stack_op_size\n");
	}

	gxFIFO.head++;
	gxFIFO.size--;
	if (gxFIFO.head > HACK_GXIFO_SIZE-1) gxFIFO.head = 0;

	GXF_FIFO_handleEvents();

	return (TRUE);
}

void GFX_FIFOcnt(u32 val)
{
	////INFO("gxFIFO: write cnt 0x%08X (prev 0x%08X) FIFO size %03i PIPE size %03i\n", val, gxstat, gxFIFO.size, gxPIPE.size);

	if (val & (1<<29))		// clear? (only in homebrew?)
	{
		GFX_PIPEclear();
		GFX_FIFOclear();
		return;
	}

	//zeromus says: what happened to clear stack?
	//if (val & (1<<15))		// projection stack pointer reset
	//{
	//	gfx3d_ClearStack();
	//	val &= 0xFFFF5FFF;		// clear reset (bit15) & stack level (bit13)
	//}

	T1WriteLong(MMU.ARM9_REG, 0x600, val);
}

// ========================================================= DISP FIFO
DISP_FIFO disp_fifo;

void DISP_FIFOinit()
{
	memset(&disp_fifo, 0, sizeof(DISP_FIFO));
}

void DISP_FIFOsend_u32(u32 val)
{
	//INFO("DISP_FIFO send value 0x%08X (head 0x%06X, tail 0x%06X)\n", val, disp_fifo.head, disp_fifo.tail);
	disp_fifo.buf[disp_fifo.tail] = val;
	
	disp_fifo.tail++;
	if (disp_fifo.tail >= 0x6000)
	{
		disp_fifo.tail = 0;
	}
}

u32 DISP_FIFOrecv_u32()
{
	//if (disp_fifo.tail == disp_fifo.head) return (0); // FIFO is empty
	u32 val = disp_fifo.buf[disp_fifo.head];
	
	disp_fifo.head++;
	if (disp_fifo.head >= 0x6000)
	{
		disp_fifo.head = 0;
	}

	return val;
}

static void _DISP_FIFOrecv_LineAdvance()
{
	disp_fifo.head += (GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) / sizeof(u32);
	if (disp_fifo.head >= 0x6000)
	{
		disp_fifo.head -= 0x6000;
	}
}

void DISP_FIFOrecv_Line16(u16 *__restrict dst)
{
#ifdef USEMANUALVECTORIZATION
	if ( (disp_fifo.head + (GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) / sizeof(u32) <= 0x6000) && (disp_fifo.head == (disp_fifo.head & ~(VECTORSIZE - 1))) )
	{
#ifdef ENABLE_ALTIVEC
		// Big-endian systems read the pixels in their correct bit order, but swap 16-bit chunks
		// within 32-bit lanes, and so we can't use a standard buffer copy function here.
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16); i+=sizeof(v128u16))
		{
			v128u16 fifoColor = vec_ld(i, disp_fifo.buf + disp_fifo.head);
			fifoColor = vec_perm( fifoColor, fifoColor, ((v128u8){2,3, 0,1, 6,7, 4,5, 10,11, 8,9, 14,15, 12,13}) );
			vec_st(fifoColor, i, dst);
		}
#else
		buffer_copy_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)>(dst, disp_fifo.buf + disp_fifo.head);
#endif // ENABLE_ALTIVEC
		
		_DISP_FIFOrecv_LineAdvance();
	}
	else
#endif // USEMANUALVECTORIZATION
	{
		for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
		{
			const u32 src = DISP_FIFOrecv_u32();
#ifdef MSB_FIRST
			((u32 *)dst)[i] = (src >> 16) | (src << 16);
#else
			((u32 *)dst)[i] = src;
#endif
		}
	}
}

#ifdef USEMANUALVECTORIZATION

template <NDSColorFormat OUTPUTFORMAT>
void _DISP_FIFOrecv_LineOpaque16_vec(u32 *__restrict dst)
{
#ifdef ENABLE_ALTIVEC
	// Big-endian systems read the pixels in their correct bit order, but swap 16-bit chunks
	// within 32-bit lanes, and so we can't use a standard buffer copy function here.
	for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16); i+=sizeof(v128u16))
	{
		v128u16 fifoColor = vec_ld(i, disp_fifo.buf + disp_fifo.head);
		fifoColor = vec_perm( fifoColor, fifoColor, ((v128u8){2,3, 0,1, 6,7, 4,5, 10,11, 8,9, 14,15, 12,13}) );
		fifoColor = vec_or(fifoColor, ((v128u16){0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000,0x8000}));
		vec_st(fifoColor, i, dst);
	}
#else
	buffer_copy_or_constant_s16_fast<GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16), false>(dst, disp_fifo.buf + disp_fifo.head, 0x8000);
#endif // ENABLE_ALTIVEC
	
	_DISP_FIFOrecv_LineAdvance();
}

template <NDSColorFormat OUTPUTFORMAT>
void _DISP_FIFOrecv_LineOpaque32_vec(u32 *__restrict dst)
{
#ifdef ENABLE_ALTIVEC
	for (size_t i = 0, d = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16); i+=16, d+=32)
	{
		v128u16 fifoColor = vec_ld(0, disp_fifo.buf + disp_fifo.head);
		
		disp_fifo.head += (sizeof(v128u16)/sizeof(u32));
		if (disp_fifo.head >= 0x6000)
		{
			disp_fifo.head -= 0x6000;
		}
		
		v128u32 dstLo = ((v128u32){0,0,0,0});
		v128u32 dstHi = ((v128u32){0,0,0,0});
		fifoColor = vec_perm( fifoColor, fifoColor, ((v128u8){10,11, 8,9, 14,15, 12,13, 2,3, 0,1, 6,7, 4,5}) );
		
		if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
		{
			ColorspaceConvert555To6665Opaque_AltiVec<false, BESwapDst>(fifoColor, dstLo, dstHi);
		}
		else if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
		{
			ColorspaceConvert555To8888Opaque_AltiVec<false, BESwapDst>(fifoColor, dstLo, dstHi);
		}
		
		vec_st(dstLo, d +  0, dst);
		vec_st(dstHi, d + 16, dst);
	}
#else
	if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
	{
		ColorspaceConvertBuffer555To6665Opaque<false, false, BESwapDst>((u16 *)(disp_fifo.buf + disp_fifo.head), dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	}
	else if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
	{
		ColorspaceConvertBuffer555To8888Opaque<false, false, BESwapDst>((u16 *)(disp_fifo.buf + disp_fifo.head), dst, GPU_FRAMEBUFFER_NATIVE_WIDTH);
	}
	_DISP_FIFOrecv_LineAdvance();
#endif // ENABLE_ALTIVEC
}

#endif // USEMANUALVECTORIZATION

template <NDSColorFormat OUTPUTFORMAT>
void DISP_FIFOrecv_LineOpaque(u32 *__restrict dst)
{
#ifdef USEMANUALVECTORIZATION
	if ( (disp_fifo.head + (GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16)) / sizeof(u32) <= 0x6000) && (disp_fifo.head == (disp_fifo.head & ~(VECTORSIZE - 1))) )
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			_DISP_FIFOrecv_LineOpaque16_vec<OUTPUTFORMAT>(dst);
		}
		else
		{
			_DISP_FIFOrecv_LineOpaque32_vec<OUTPUTFORMAT>(dst);
		}
	}
	else
#endif
	{
		if (OUTPUTFORMAT == NDSColorFormat_BGR555_Rev)
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH * sizeof(u16) / sizeof(u32); i++)
			{
				const u32 src = DISP_FIFOrecv_u32();
#ifdef MSB_FIRST
				dst[i] = (src >> 16) | (src << 16) | 0x80008000;
#else
				dst[i] = src | 0x80008000;
#endif
			}
		}
		else
		{
			for (size_t i = 0; i < GPU_FRAMEBUFFER_NATIVE_WIDTH; i+=2)
			{
				const u32 src = DISP_FIFOrecv_u32();
				
				if (OUTPUTFORMAT == NDSColorFormat_BGR666_Rev)
				{
					dst[i+0] = LE_TO_LOCAL_32( ColorspaceConvert555To6665Opaque<false>((src >>  0) & 0x7FFF) );
					dst[i+1] = LE_TO_LOCAL_32( ColorspaceConvert555To6665Opaque<false>((src >> 16) & 0x7FFF) );
				}
				else if (OUTPUTFORMAT == NDSColorFormat_BGR888_Rev)
				{
					dst[i+0] = LE_TO_LOCAL_32( ColorspaceConvert555To8888Opaque<false>((src >>  0) & 0x7FFF) );
					dst[i+1] = LE_TO_LOCAL_32( ColorspaceConvert555To8888Opaque<false>((src >> 16) & 0x7FFF) );
				}
			}
		}
	}
}

void DISP_FIFOreset()
{
	disp_fifo.head = 0;
	disp_fifo.tail = 0;
}

template void DISP_FIFOrecv_LineOpaque<NDSColorFormat_BGR555_Rev>(u32 *__restrict dst);
template void DISP_FIFOrecv_LineOpaque<NDSColorFormat_BGR666_Rev>(u32 *__restrict dst);
template void DISP_FIFOrecv_LineOpaque<NDSColorFormat_BGR888_Rev>(u32 *__restrict dst);
