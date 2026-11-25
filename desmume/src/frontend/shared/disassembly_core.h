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

// Frontend-independent ARM disassembler view, shared by the Windows, Cocoa and
// GTK3 frontends. This is the logic that used to live inside windows/disView.cpp
// (plus one copy of it per CPU): address navigation, ARM/Thumb/auto mode, line
// decoding, register snapshots and the breakpoint list. The Win32 dialogs, the
// NSTableView and the GtkTreeView are only hosts.
//
// The scroll position is kept as a byte address instead of a line index, which
// is what the Windows code got wrong: it stored "curr_ligne" and multiplied by
// 2 or 4 depending on the mode, so switching between ARM and Thumb views
// (and the ARM9 "auto" path, which shifted by 2 instead of 1) landed on the
// wrong address. Re-aligning an address is mode-independent.
//
// Low-level disassembly text comes from frontend/modules/Disassembler.cpp;
// this header stays free of platform headers and does not include types.h
// (its BOOL/TRUE/FALSE macros break Cocoa declarations).

#ifndef DISASSEMBLY_CORE_H
#define DISASSEMBLY_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

class DisassemblyCore
{
public:
	enum Cpu
	{
		CPU_ARM9 = 0,
		CPU_ARM7 = 1,
		CPU_COUNT = 2
	};

	enum ViewMode
	{
		ViewMode_Auto = 0,  // follow CPSR.T
		ViewMode_ARM = 1,
		ViewMode_Thumb = 2
	};

	// Upper bound of the 28-bit ARM address space, matching the Windows
	// dialog's 0x3FFFFF7 line limit once multiplied by the instruction size.
	static const uint32_t kMaxAddress = 0x0FFFFFFF;

	struct Line
	{
		uint32_t Address;
		uint32_t Opcode;
		std::string Text;      // "0200:1234  E3A00001  mov r0,#1"
		bool Current;          // this is the instruction the CPU is about to run
		bool Breakpoint;

		Line();
	};

	struct Registers
	{
		uint32_t R[16];
		uint32_t CPSR;
		uint32_t SPSR;
		unsigned int Mode;
		bool N, Z, C, V, Q, I, F, T;

		Registers();
	};

	static DisassemblyCore& Get();

	// ---- CPU and view mode ----------------------------------------------

	Cpu GetCpu() const;
	void SetCpu(Cpu cpu);

	ViewMode GetViewMode() const;
	void SetViewMode(ViewMode mode);

	// Resolves ViewMode_Auto against CPSR.T of the selected CPU.
	bool IsThumbMode() const;
	uint32_t InstructionSize() const; // 2 or 4

	// ---- Address navigation ---------------------------------------------

	uint32_t AnchorAddress() const;                 // first visible instruction
	void SetAnchorAddress(uint32_t address);        // aligns to the current mode
	void ScrollLines(int lineDelta);
	void ScrollPages(int pageDelta, size_t linesPerPage);
	void FollowPC();                                 // jump to instruct_adr

	// Fills `lines` with `count` decoded instructions starting at the anchor.
	// Returns the number of lines written (less than count at the address
	// space limit).
	size_t BuildLines(size_t count, std::vector<Line>& lines) const;

	// ---- Registers -------------------------------------------------------

	bool ReadRegisters(Registers& regs) const;

	// Writes R0-R14. R15 is read-only: the Windows dialog skipped index 15 as
	// well, because overwriting PC from a text field has no sane semantics
	// while the CPU is running.
	bool WriteRegister(unsigned int index, uint32_t value);

	// Accepts "1234ABCD", "0x1234abcd" and ignores anything after the digits.
	static bool ParseHex32(const char* text, uint32_t& value);

	// ---- Breakpoints -----------------------------------------------------

	size_t BreakpointCount() const;
	uint32_t BreakpointAt(size_t index) const;
	bool HasBreakpoint(uint32_t address) const;
	bool AddBreakpoint(uint32_t address);          // false on duplicate
	bool RemoveBreakpointAt(size_t index);
	bool RemoveBreakpoint(uint32_t address);

	// ---- Run control -----------------------------------------------------
	//
	// These arm the core; the host is responsible for actually resuming
	// emulation afterwards (Windows sets execute/paused, Cocoa calls
	// -executeCore, GTK3 calls desmume_resume()).

	void StepInstruction(); // one instruction of the selected CPU
	void Continue();
	void RunToReturn();
	void StepOver();

	static Registers ReadCpuRegisters(Cpu cpu);

	// Textual name of an ARM operating mode ("User", "IRQ", ...), so the three
	// frontends do not each carry their own switch. Returns "Unknown" for the
	// reserved encodings.
	static const char* CpuModeName(unsigned int mode);

private:
	DisassemblyCore();
	DisassemblyCore(const DisassemblyCore&);
	DisassemblyCore& operator=(const DisassemblyCore&);

	struct ViewState
	{
		ViewMode Mode;
		uint32_t Anchor;

		ViewState();
	};

	static uint32_t AlignAddress(uint32_t address, uint32_t instructionSize);
	static uint32_t ClampAddress(uint32_t address);

	ViewState _views[CPU_COUNT];
	Cpu _cpu;
};

#endif // DISASSEMBLY_CORE_H
