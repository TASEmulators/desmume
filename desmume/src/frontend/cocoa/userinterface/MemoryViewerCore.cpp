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

#include "MemoryViewerCore.h"

#include <algorithm>
#include <stdio.h>
#include <string.h>

#include "../../../MMU.h"
#include "../../../NDSSystem.h"
#include "../../../ROMReader.h"

static const u32 kArm9InitialAddress = 0x02000000;
static const u32 kArm7InitialAddress = 0x02000000;

// The ROM and full-dump regions claim the whole 32-bit space; the real
// cartridge limit is enforced separately by MemoryViewerIsAvailable().
static MemoryViewerRegionInfo s_memoryRegions[MEMVIEW_REGION_COUNT] =
{
	{ "ARM9",     "ARM9 memory",    kArm9InitialAddress, 0x01000000 },
	{ "ARM7",     "ARM7 memory",    kArm7InitialAddress, 0x01000000 },
	{ "Firmware", "Firmware",       0x00000000,          0x00040000 },
	{ "CartROM",  "Cartridge ROM",  0x00000000,          0xFFFFFFF0 },
	{ "Full",     "Full dump",      0x00000000,          0xFFFFFFF0 }
};

void MemoryViewerResetRegions(void)
{
	s_memoryRegions[MEMVIEW_ARM9].hardwareAddress = kArm9InitialAddress;
	s_memoryRegions[MEMVIEW_ARM7].hardwareAddress = kArm7InitialAddress;
}

const MemoryViewerRegionInfo *MemoryViewerRegionGet(MemoryViewerRegionType type)
{
	if (type < 0 || type >= MEMVIEW_REGION_COUNT)
	{
		return &s_memoryRegions[MEMVIEW_ARM9];
	}

	return &s_memoryRegions[type];
}

const char *MemoryViewerRegionGetName(MemoryViewerRegionType type)
{
	return MemoryViewerRegionGet(type)->name;
}

const char *MemoryViewerRegionGetLongName(MemoryViewerRegionType type)
{
	return MemoryViewerRegionGet(type)->longName;
}

bool MemoryViewerRegionSetBaseAddress(MemoryViewerRegionType type, u32 address)
{
	if (type != MEMVIEW_ARM9 && type != MEMVIEW_ARM7)
	{
		return false;
	}

	if (s_memoryRegions[type].hardwareAddress == address)
	{
		return false;
	}

	s_memoryRegions[type].hardwareAddress = address;
	return true;
}

bool MemoryViewerRegionContains(MemoryViewerRegionType type, u32 address)
{
	const MemoryViewerRegionInfo *region = MemoryViewerRegionGet(type);
	return (address >= region->hardwareAddress) &&
	       (address < (u32)(region->hardwareAddress + region->size));
}

u32 MemoryViewerRegionDumpSize(MemoryViewerRegionType type)
{
	switch (type)
	{
		case MEMVIEW_ARM9:
		case MEMVIEW_ARM7:
		case MEMVIEW_FIRMWARE:
			return s_memoryRegions[type].size;

		case MEMVIEW_ROM:
			return gameInfo.romsize;

		default:
			return 0;
	}
}

u8 MemoryViewerRead8(MemoryViewerRegionType type, u32 address)
{
	if (!MemoryViewerRegionContains(type, address))
	{
		return 0;
	}

	u8 value = 0;
	switch (type)
	{
		case MEMVIEW_ARM9:
			MMU_DumpMemBlock(ARMCPU_ARM9, address, 1, &value);
			return value;

		case MEMVIEW_ARM7:
			MMU_DumpMemBlock(ARMCPU_ARM7, address, 1, &value);
			return value;

		case MEMVIEW_FIRMWARE:
			value = MMU.fw.data._raw[address];
			return value;

		case MEMVIEW_ROM:
			// The Windows frontend leaves single-byte ROM reads unimplemented;
			// serve them from the cartridge reader instead so that the word/
			// halfword paths (which fall back to this function) stay coherent.
			if (gameInfo.reader == NULL || gameInfo.fROM == NULL || address >= gameInfo.romsize)
			{
				return 0;
			}
			gameInfo.reader->Seek(gameInfo.fROM, (int)address, SEEK_SET);
			gameInfo.reader->Read(gameInfo.fROM, &value, 1);
			return value;

		case MEMVIEW_FULL:
			MMU_DumpMemBlock(ARMCPU_ARM9, address, 1, &value);
			return value;

		default:
			break;
	}

	return 0;
}

void MemoryViewerRead(MemoryViewerRegionType type, u32 address, u32 size, u8 *buffer)
{
	if (buffer == NULL || size == 0)
	{
		return;
	}

	switch (type)
	{
		case MEMVIEW_ARM9:
			MMU_DumpMemBlock(ARMCPU_ARM9, address, size, buffer);
			break;

		case MEMVIEW_ARM7:
			MMU_DumpMemBlock(ARMCPU_ARM7, address, size, buffer);
			break;

		case MEMVIEW_ROM:
			if (gameInfo.reader == NULL || gameInfo.fROM == NULL)
			{
				memset(buffer, 0, size);
				break;
			}

			gameInfo.reader->Seek(gameInfo.fROM, (int)address, SEEK_SET);
			gameInfo.reader->Read(gameInfo.fROM, buffer, size);
			break;

		default:
			for (u32 i = 0; i < size; i++)
			{
				buffer[i] = MemoryViewerRead8(type, address + i);
			}
			break;
	}
}

bool MemoryViewerIsAvailable(MemoryViewerRegionType type, u32 address)
{
	if (type == MEMVIEW_ARM7 && (address & 0xFFFF0000) == 0x04800000)
	{
		return false;
	}

	if (type == MEMVIEW_ROM && (gameInfo.reader == NULL || address > gameInfo.romsize))
	{
		return false;
	}

	return true;
}

void MemoryViewerWrite8(MemoryViewerRegionType type, u32 address, u8 value)
{
	switch (type)
	{
		case MEMVIEW_ARM9:
			MMU_write8(ARMCPU_ARM9, address, value);
			break;

		case MEMVIEW_ARM7:
			MMU_write8(ARMCPU_ARM7, address, value);
			break;

		case MEMVIEW_FIRMWARE:
			MMU.fw.data._raw[address] = value;
			break;

		case MEMVIEW_ROM:
			if (gameInfo.reader == NULL || gameInfo.fROM == NULL)
			{
				break;
			}
			gameInfo.reader->Seek(gameInfo.fROM, (int)address, SEEK_SET);
			gameInfo.reader->Write(gameInfo.fROM, &value, 1);
			break;

		case MEMVIEW_FULL:
			MMU_write8(ARMCPU_ARM9, address, value);
			MMU_write8(ARMCPU_ARM7, address, value);
			break;

		default:
			break;
	}
}

void MemoryViewerWrite16(MemoryViewerRegionType type, u32 address, u16 value)
{
	switch (type)
	{
		case MEMVIEW_ARM9:
			MMU_write16(ARMCPU_ARM9, address, value);
			break;

		case MEMVIEW_ARM7:
			MMU_write16(ARMCPU_ARM7, address, value);
			break;

		case MEMVIEW_FIRMWARE:
			*((u16 *)&MMU.fw.data._raw[address]) = value;
			break;

		case MEMVIEW_ROM:
			if (gameInfo.reader == NULL || gameInfo.fROM == NULL)
			{
				break;
			}
			gameInfo.reader->Seek(gameInfo.fROM, (int)address, SEEK_SET);
			gameInfo.reader->Write(gameInfo.fROM, &value, 2);
			break;

		case MEMVIEW_FULL:
			MMU_write16(ARMCPU_ARM9, address, value);
			MMU_write16(ARMCPU_ARM7, address, value);
			break;

		default:
			break;
	}
}

void MemoryViewerWrite32(MemoryViewerRegionType type, u32 address, u32 value)
{
	switch (type)
	{
		case MEMVIEW_ARM9:
			MMU_write32(ARMCPU_ARM9, address, value);
			break;

		case MEMVIEW_ARM7:
			MMU_write32(ARMCPU_ARM7, address, value);
			break;

		case MEMVIEW_FIRMWARE:
			*((u32 *)&MMU.fw.data._raw[address]) = value;
			break;

		case MEMVIEW_ROM:
			if (gameInfo.reader == NULL || gameInfo.fROM == NULL)
			{
				break;
			}
			gameInfo.reader->Seek(gameInfo.fROM, (int)address, SEEK_SET);
			gameInfo.reader->Write(gameInfo.fROM, &value, 4);
			break;

		case MEMVIEW_FULL:
			MMU_write32(ARMCPU_ARM9, address, value);
			MMU_write32(ARMCPU_ARM7, address, value);
			break;

		default:
			break;
	}
}

#pragma mark - Breakpoints

static bool ContainsAddress(const std::vector<u32> &list, u32 address)
{
	return std::find(list.begin(), list.end(), address) != list.end();
}

size_t MemoryViewerReadBreakpointCount(void)
{
	return memReadBreakPoints.size();
}

u32 MemoryViewerReadBreakpointAt(size_t index)
{
	return (index < memReadBreakPoints.size()) ? memReadBreakPoints[index] : 0;
}

void MemoryViewerAddReadBreakpoint(u32 address)
{
	if (!ContainsAddress(memReadBreakPoints, address))
	{
		memReadBreakPoints.push_back(address);
	}
}

void MemoryViewerRemoveReadBreakpoint(u32 address)
{
	memReadBreakPoints.erase(std::remove(memReadBreakPoints.begin(), memReadBreakPoints.end(), address),
							 memReadBreakPoints.end());
}

void MemoryViewerClearReadBreakpoints(void)
{
	memReadBreakPoints.clear();
}

bool MemoryViewerIsReadBreakpoint(u32 address)
{
	return ContainsAddress(memReadBreakPoints, address);
}

size_t MemoryViewerWriteBreakpointCount(void)
{
	return memWriteBreakPoints.size();
}

u32 MemoryViewerWriteBreakpointAt(size_t index)
{
	return (index < memWriteBreakPoints.size()) ? memWriteBreakPoints[index] : 0;
}

void MemoryViewerAddWriteBreakpoint(u32 address)
{
	if (!ContainsAddress(memWriteBreakPoints, address))
	{
		memWriteBreakPoints.push_back(address);
	}
}

void MemoryViewerRemoveWriteBreakpoint(u32 address)
{
	memWriteBreakPoints.erase(std::remove(memWriteBreakPoints.begin(), memWriteBreakPoints.end(), address),
							  memWriteBreakPoints.end());
}

void MemoryViewerClearWriteBreakpoints(void)
{
	memWriteBreakPoints.clear();
}

bool MemoryViewerIsWriteBreakpoint(u32 address)
{
	return ContainsAddress(memWriteBreakPoints, address);
}

bool MemoryViewerIsBreakpoint(u32 address)
{
	return MemoryViewerIsReadBreakpoint(address) || MemoryViewerIsWriteBreakpoint(address);
}
