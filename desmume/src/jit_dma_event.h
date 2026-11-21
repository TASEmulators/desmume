/*
	Copyright (C) 2026 DeSmuME team

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

#ifndef JIT_DMA_EVENT_H
#define JIT_DMA_EVENT_H

#include "types.h"

// Instructions that write to memory.
//
// A store is the only way the CPU can arm a DMA channel, and arming one is the
// one thing that has to cut a compiled block short. Writing DMAxCNT in a mode
// that starts immediately queues the transfer at the current timestamp
// (DmaController::doSchedule() sets nextEvent = nds_timer), so it is due the
// moment the store retires. The interpreter goes back to the sequencer after
// every instruction and therefore always runs the transfer before the CPU can
// touch the DMA registers again; a compiled block does not, so when two writes
// to the same channel land inside one block the second overwrites the
// parameters that the first latched and the transfer never happens.
//
// Both JITs emit a check of mmu_dma_event_pending after every store and leave
// the block when it is raised. DmaController::write32() raises it only for the
// CPU's own programming writes; the reschedule at the end of a transfer is due
// in time_elapsed cycles rather than now, so it must not raise it.
//
// This lives in a header because arm_jit.cpp (x86 back end) and
// utils/arm_jit/arm_jit_arm.cpp (ARM32/AArch64 back ends) cannot see each
// other's copy of the block compiler. Both must classify instructions the same
// way, so the classification is shared even though the code emission is not.
static inline bool instr_is_store(u32 opcode, bool thumb)
{
	if(thumb)
	{
		switch(opcode & 0xF800)
		{
			case 0x5000: return true; // STR/STRB/STRH (register offset, L clear)
			case 0x6000: return true; // STR   (immediate offset)
			case 0x7000: return true; // STRB  (immediate offset)
			case 0x8000: return true; // STRH  (immediate offset)
			case 0x9000: return true; // STR   (SP relative)
			default: break;
		}
		if((opcode & 0xF000) == 0xC000) return true; // STMIA
		if((opcode & 0xFE00) == 0xB400) return true; // PUSH
		return false;
	}

	// ARM: single data transfer, L bit clear
	if((opcode & 0x0C000000) == 0x04000000)
		return (opcode & 0x00100000) == 0;
	// ARM: block data transfer, L bit clear
	if((opcode & 0x0E000000) == 0x08000000)
		return (opcode & 0x00100000) == 0;
	// ARM: SWP / SWPB
	if((opcode & 0x0FB00000) == 0x01000000)
		return true;
	// ARM: STRD
	if((opcode & 0x0E000000) == 0 && (opcode & 0x00400000) && (opcode & 0x00100000) == 0
		&& ((opcode >> 4) & 0xF) == 0xD)
		return true;

	return false;
}

#endif // JIT_DMA_EVENT_H
