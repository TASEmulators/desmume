/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2026 DeSmuME team

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

// Frontend-independent memory access layer for the Memory Viewer. This is a
// port of the region model used by the Windows frontend's memView.cpp so that
// both frontends read and write emulated memory in exactly the same way.

#ifndef MEMORY_VIEWER_CORE_H
#define MEMORY_VIEWER_CORE_H

#include "../../../types.h"

enum MemoryViewerRegionType
{
	MEMVIEW_ARM9 = 0,
	MEMVIEW_ARM7,
	MEMVIEW_FIRMWARE,
	MEMVIEW_ROM,
	MEMVIEW_FULL,
	MEMVIEW_REGION_COUNT
};

typedef struct
{
	const char *name;			// Short name, e.g. "ARM9" (region popup)
	const char *longName;		// Long name, e.g. "ARM9 memory" (window title)
	u32 hardwareAddress;		// Address of the first byte of the region
	u32 size;					// Number of bytes up to the end of the region
} MemoryViewerRegionInfo;

// Region table access. The returned pointer refers to the live table, so
// hardwareAddress may change when the user jumps to another bus region.
const MemoryViewerRegionInfo *MemoryViewerRegionGet(MemoryViewerRegionType type);
const char *MemoryViewerRegionGetName(MemoryViewerRegionType type);
const char *MemoryViewerRegionGetLongName(MemoryViewerRegionType type);

// ARM9/ARM7 memory is mirrored over several 16MB banks, so the region base can
// be moved by the caller. Returns true if the base address actually changed.
bool MemoryViewerRegionSetBaseAddress(MemoryViewerRegionType type, u32 address);

// Resets the table to its startup state. Called once when the viewer opens.
void MemoryViewerResetRegions(void);

// Bounds helper: true if address falls inside the region's address window.
bool MemoryViewerRegionContains(MemoryViewerRegionType type, u32 address);

// Number of bytes that can meaningfully be dumped for this region. Returns 0
// when the region spans the whole address space (CartROM without a loaded
// cartridge, Full dump), where dumping "everything" makes no sense.
u32 MemoryViewerRegionDumpSize(MemoryViewerRegionType type);

// Reads. Out-of-range bytes read as zero, matching the Windows frontend.
u8 MemoryViewerRead8(MemoryViewerRegionType type, u32 address);
void MemoryViewerRead(MemoryViewerRegionType type, u32 address, u32 size, u8 *buffer);

// Returns false for addresses the region cannot service (e.g. past the end of
// the cartridge, or ARM7's unmapped 0x04800000 block). Used to reject edits.
bool MemoryViewerIsAvailable(MemoryViewerRegionType type, u32 address);

// Writes. Callers must check MemoryViewerIsAvailable() first.
void MemoryViewerWrite8(MemoryViewerRegionType type, u32 address, u8 value);
void MemoryViewerWrite16(MemoryViewerRegionType type, u32 address, u16 value);
void MemoryViewerWrite32(MemoryViewerRegionType type, u32 address, u32 value);

// Breakpoints. These operate on the emulator core's real breakpoint lists, so
// adding one stops execution the next time the address is touched, which is
// exactly what the Windows frontend's Memory Viewer does.
size_t MemoryViewerReadBreakpointCount(void);
u32 MemoryViewerReadBreakpointAt(size_t index);
void MemoryViewerAddReadBreakpoint(u32 address);
void MemoryViewerRemoveReadBreakpoint(u32 address);
void MemoryViewerClearReadBreakpoints(void);
bool MemoryViewerIsReadBreakpoint(u32 address);

size_t MemoryViewerWriteBreakpointCount(void);
u32 MemoryViewerWriteBreakpointAt(size_t index);
void MemoryViewerAddWriteBreakpoint(u32 address);
void MemoryViewerRemoveWriteBreakpoint(u32 address);
void MemoryViewerClearWriteBreakpoints(void);
bool MemoryViewerIsWriteBreakpoint(u32 address);

// True when either list contains address; used to paint the hex cells.
bool MemoryViewerIsBreakpoint(u32 address);

#endif // MEMORY_VIEWER_CORE_H
