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

#include "disassembly_core.h"

#include <stdio.h>
#include <string.h>

#include "../modules/Disassembler.h"
#include "../../MMU.h"
#include "../../armcpu.h"
#include "../../NDSSystem.h"

// Same table index the Windows dialog used: bits 27-20 and 7-4 of the ARM
// instruction select one of the 4096 decoder entries.
#define DISASM_ARM_INDEX(i) ((((i) >> 16) & 0xFF0) | (((i) >> 4) & 0xF))

namespace
{
	armcpu_t* CpuForIndex(int cpu)
	{
		return (cpu == DisassemblyCore::CPU_ARM7) ? &NDS_ARM7 : &NDS_ARM9;
	}

	bool ContainsAddress(const std::vector<uint32_t>* list, uint32_t address)
	{
		if (list == NULL)
		{
			return false;
		}

		for (size_t i = 0; i < list->size(); ++i)
		{
			if ((*list)[i] == address)
			{
				return true;
			}
		}
		return false;
	}

	int HexDigitValue(char c)
	{
		if (c >= '0' && c <= '9')
		{
			return c - '0';
		}
		if (c >= 'a' && c <= 'f')
		{
			return c - 'a' + 10;
		}
		if (c >= 'A' && c <= 'F')
		{
			return c - 'A' + 10;
		}
		return -1;
	}
}

DisassemblyCore::Line::Line()
	: Address(0)
	, Opcode(0)
	, Current(false)
	, Breakpoint(false)
{
}

DisassemblyCore::Registers::Registers()
	: CPSR(0)
	, SPSR(0)
	, Mode(0)
	, N(false)
	, Z(false)
	, C(false)
	, V(false)
	, Q(false)
	, I(false)
	, F(false)
	, T(false)
{
	memset(R, 0, sizeof(R));
}

DisassemblyCore::ViewState::ViewState()
	: Mode(ViewMode_Auto)
	, Anchor(0)
{
}

DisassemblyCore::DisassemblyCore()
	: _cpu(CPU_ARM9)
{
}

DisassemblyCore& DisassemblyCore::Get()
{
	static DisassemblyCore s_core;
	return s_core;
}

// ---------------------------------------------------------------------------
// CPU and view mode
// ---------------------------------------------------------------------------

DisassemblyCore::Cpu DisassemblyCore::GetCpu() const
{
	return _cpu;
}

void DisassemblyCore::SetCpu(Cpu cpu)
{
	if (cpu != CPU_ARM9 && cpu != CPU_ARM7)
	{
		cpu = CPU_ARM9;
	}

	_cpu = cpu;
}

DisassemblyCore::ViewMode DisassemblyCore::GetViewMode() const
{
	return _views[_cpu].Mode;
}

void DisassemblyCore::SetViewMode(ViewMode mode)
{
	if (mode != ViewMode_Auto && mode != ViewMode_ARM && mode != ViewMode_Thumb)
	{
		mode = ViewMode_Auto;
	}

	// The anchor is a byte address, so switching modes only re-aligns it. The
	// Windows dialogs shifted a line counter by one *or two* bits depending on
	// the direction, which is what made the ARM9 view jump to a stray address
	// after switching from Thumb back to ARM.
	_views[_cpu].Mode = mode;
	_views[_cpu].Anchor = AlignAddress(_views[_cpu].Anchor, InstructionSize());
}

bool DisassemblyCore::IsThumbMode() const
{
	switch (_views[_cpu].Mode)
	{
		case ViewMode_ARM:
			return false;
		case ViewMode_Thumb:
			return true;
		case ViewMode_Auto:
		default:
			return (CpuForIndex(_cpu)->CPSR.bits.T != 0);
	}
}

uint32_t DisassemblyCore::InstructionSize() const
{
	return IsThumbMode() ? 2 : 4;
}

// ---------------------------------------------------------------------------
// Address navigation
// ---------------------------------------------------------------------------

uint32_t DisassemblyCore::AlignAddress(uint32_t address, uint32_t instructionSize)
{
	const uint32_t mask = instructionSize - 1;
	return (ClampAddress(address)) & ~mask;
}

uint32_t DisassemblyCore::ClampAddress(uint32_t address)
{
	return (address > kMaxAddress) ? kMaxAddress : address;
}

uint32_t DisassemblyCore::AnchorAddress() const
{
	return _views[_cpu].Anchor;
}

void DisassemblyCore::SetAnchorAddress(uint32_t address)
{
	_views[_cpu].Anchor = AlignAddress(address, InstructionSize());
}

void DisassemblyCore::ScrollLines(int lineDelta)
{
	const int64_t size = (int64_t)InstructionSize();
	int64_t address = (int64_t)_views[_cpu].Anchor + ((int64_t)lineDelta * size);

	if (address < 0)
	{
		address = 0;
	}
	else if (address > (int64_t)kMaxAddress)
	{
		address = (int64_t)kMaxAddress;
	}

	_views[_cpu].Anchor = AlignAddress((uint32_t)address, InstructionSize());
}

void DisassemblyCore::ScrollPages(int pageDelta, size_t linesPerPage)
{
	ScrollLines(pageDelta * (int)linesPerPage);
}

void DisassemblyCore::FollowPC()
{
	SetAnchorAddress(CpuForIndex(_cpu)->instruct_adr);
}

size_t DisassemblyCore::BuildLines(size_t count, std::vector<Line>& lines) const
{
	lines.clear();
	if (count == 0)
	{
		return 0;
	}

	const armcpu_t* cpu = CpuForIndex(_cpu);
	const bool thumb = IsThumbMode();
	const uint32_t instructionSize = thumb ? 2 : 4;
	const uint32_t pc = cpu->instruct_adr & kMaxAddress;
	const std::vector<uint32_t>* breakpoints = cpu->breakPoints;

	lines.reserve(count);

	uint32_t address = AlignAddress(_views[_cpu].Anchor, instructionSize);
	char mnemonic[192];
	char text[256];

	for (size_t i = 0; i < count; ++i, address += instructionSize)
	{
		if (address > kMaxAddress)
		{
			break;
		}

		Line line;
		line.Address = address;

		if (thumb)
		{
			const uint32_t instruction = _MMU_read16(cpu->proc_ID, MMU_AT_DEBUG, address);
			des_thumb_instructions_set[instruction >> 6](address, instruction, mnemonic);
			snprintf(text, sizeof(text), "%04X:%04X  %04X  %s",
			         (unsigned int)(address >> 16), (unsigned int)(address & 0xFFFF),
			         (unsigned int)instruction, mnemonic);
			line.Opcode = instruction;
		}
		else
		{
			const uint32_t instruction = _MMU_read32(cpu->proc_ID, MMU_AT_DEBUG, address);
			des_arm_instructions_set[DISASM_ARM_INDEX(instruction)](address, instruction, mnemonic);
			snprintf(text, sizeof(text), "%04X:%04X  %08X  %s",
			         (unsigned int)(address >> 16), (unsigned int)(address & 0xFFFF),
			         (unsigned int)instruction, mnemonic);
			line.Opcode = instruction;
		}

		line.Text = text;
		line.Current = (address == pc);
		line.Breakpoint = ContainsAddress(breakpoints, address);
		lines.push_back(line);
	}

	return lines.size();
}

// ---------------------------------------------------------------------------
// Registers
// ---------------------------------------------------------------------------

DisassemblyCore::Registers DisassemblyCore::ReadCpuRegisters(Cpu cpu)
{
	const armcpu_t* armcpu = CpuForIndex((int)cpu);

	Registers regs;
	memcpy(regs.R, armcpu->R, sizeof(regs.R));
	regs.CPSR = armcpu->CPSR.val;
	regs.SPSR = armcpu->SPSR.val;
	regs.Mode = armcpu->CPSR.bits.mode;
	regs.N = (armcpu->CPSR.bits.N != 0);
	regs.Z = (armcpu->CPSR.bits.Z != 0);
	regs.C = (armcpu->CPSR.bits.C != 0);
	regs.V = (armcpu->CPSR.bits.V != 0);
	regs.Q = (armcpu->CPSR.bits.Q != 0);
	regs.I = (armcpu->CPSR.bits.I != 0);
	regs.F = (armcpu->CPSR.bits.F != 0);
	regs.T = (armcpu->CPSR.bits.T != 0);
	return regs;
}

bool DisassemblyCore::ReadRegisters(Registers& regs) const
{
	regs = ReadCpuRegisters(_cpu);
	return true;
}

const char* DisassemblyCore::CpuModeName(unsigned int mode)
{
	switch (mode & 0x1F)
	{
		case 0x10: return "User";
		case 0x11: return "FIQ";
		case 0x12: return "IRQ";
		case 0x13: return "Supervisor";
		case 0x17: return "Abort";
		case 0x1B: return "Undefined";
		case 0x1F: return "System";
		default: break;
	}

	return "Unknown";
}

bool DisassemblyCore::WriteRegister(unsigned int index, uint32_t value)
{
	if (index >= 15)
	{
		return false;
	}

	CpuForIndex(_cpu)->R[index] = value;
	return true;
}

bool DisassemblyCore::ParseHex32(const char* text, uint32_t& value)
{
	value = 0;
	if (text == NULL)
	{
		return false;
	}

	const char* cursor = text;
	while (*cursor == ' ' || *cursor == '\t')
	{
		++cursor;
	}

	if ((cursor[0] == '0') && (cursor[1] == 'x' || cursor[1] == 'X'))
	{
		cursor += 2;
	}

	bool any = false;
	while (*cursor != '\0')
	{
		const int digit = HexDigitValue(*cursor);
		if (digit < 0)
		{
			break;
		}

		value = (value << 4) | (uint32_t)digit;
		any = true;
		++cursor;
	}

	// The list shows addresses as "BANK:OFFSET", so accept that spelling too
	// instead of silently keeping just the bank. "0200:1234" is 0x02001234.
	if (any && (*cursor == ':'))
	{
		++cursor;

		uint32_t offset = 0;
		bool anyOffset = false;
		while (*cursor != '\0')
		{
			const int digit = HexDigitValue(*cursor);
			if (digit < 0)
			{
				break;
			}

			offset = (offset << 4) | (uint32_t)digit;
			anyOffset = true;
			++cursor;
		}

		if (anyOffset)
		{
			value = ((value & 0xFFFF) << 16) | (offset & 0xFFFF);
		}
	}

	return any;
}

// ---------------------------------------------------------------------------
// Breakpoints
// ---------------------------------------------------------------------------

size_t DisassemblyCore::BreakpointCount() const
{
	const std::vector<uint32_t>* list = CpuForIndex(_cpu)->breakPoints;
	return (list != NULL) ? list->size() : 0;
}

uint32_t DisassemblyCore::BreakpointAt(size_t index) const
{
	const std::vector<uint32_t>* list = CpuForIndex(_cpu)->breakPoints;
	if ((list == NULL) || (index >= list->size()))
	{
		return 0;
	}
	return (*list)[index];
}

bool DisassemblyCore::HasBreakpoint(uint32_t address) const
{
	return ContainsAddress(CpuForIndex(_cpu)->breakPoints, address);
}

bool DisassemblyCore::AddBreakpoint(uint32_t address)
{
	std::vector<uint32_t>* list = CpuForIndex(_cpu)->breakPoints;
	if (list == NULL)
	{
		return false;
	}

	if (ContainsAddress(list, address))
	{
		return false;
	}

	list->push_back(address);
	return true;
}

bool DisassemblyCore::RemoveBreakpointAt(size_t index)
{
	std::vector<uint32_t>* list = CpuForIndex(_cpu)->breakPoints;
	if ((list == NULL) || (index >= list->size()))
	{
		return false;
	}

	list->erase(list->begin() + index);
	return true;
}

bool DisassemblyCore::RemoveBreakpoint(uint32_t address)
{
	std::vector<uint32_t>* list = CpuForIndex(_cpu)->breakPoints;
	if (list == NULL)
	{
		return false;
	}

	for (size_t i = 0; i < list->size(); ++i)
	{
		if ((*list)[i] == address)
		{
			list->erase(list->begin() + i);
			return true;
		}
	}
	return false;
}

// ---------------------------------------------------------------------------
// Run control
// ---------------------------------------------------------------------------

void DisassemblyCore::StepInstruction()
{
	armcpu_t* cpu = CpuForIndex(_cpu);
	cpu->debugStep = true;
	NDS_debug_continue();
}

void DisassemblyCore::Continue()
{
	NDS_debug_continue();
}

void DisassemblyCore::RunToReturn()
{
	CpuForIndex(_cpu)->runToRet = true;
}

void DisassemblyCore::StepOver()
{
	armcpu_t* cpu = CpuForIndex(_cpu);
	cpu->stepOverBreak = cpu->instruct_adr + 4;
}
