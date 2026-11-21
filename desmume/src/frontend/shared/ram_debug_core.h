/*
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

// Low-level, frontend-independent memory primitives shared by RAM Watch and
// RAM Search. Both tools used to duplicate these helpers inside
// windows/ramwatch.cpp and windows/ram_search.cpp; keeping one copy here means
// a future GTK/other port only has to write UI code.
//
// Deliberately does not include types.h (BOOL/TRUE/FALSE macros break Cocoa).

#ifndef RAM_DEBUG_CORE_H
#define RAM_DEBUG_CORE_H

#include <stdint.h>
#include <string>

class RAMDebugCore
{
public:
	// The three NDS RAM regions RAM Search walks. These match the
	// s_mainMemRegion / s_itcmRegion / s_dtcmRegion table in ram_search.cpp and
	// MAX_RAM_SIZE in ram_search.h.
	static const uint32_t kMainMemAddress = 0x02000000;
	static const uint32_t kMainMemSize    = 0x00400000;
	static const uint32_t kItcmAddress   = 0x01000000;
	static const uint32_t kItcmSize      = 0x00008000;
	static const uint32_t kDtcmAddress   = 0x027C0000;
	static const uint32_t kDtcmSize      = 0x00004000;
	static const uint32_t kMaxRamSize    = kMainMemSize + kItcmSize + kDtcmSize;

	// ---- Bus access -----------------------------------------------------

	// Reads 1/2/4 bytes from the ARM9 bus via MMU_DumpMemBlock(). This is the
	// implementation behind RAM Watch's GetCurrentValue() and RAM Search's
	// ReadValueAtHardwareAddress().
	static uint32_t ReadValue(uint32_t address, int sizeInBytes);

	// Writes 1/2/4 bytes through MMU_write8/16/32(). RAM Search uses this for
	// poking an address from its result list and for undo.
	static bool WriteValue(uint32_t address, uint32_t value, int sizeInBytes);

	// Software-address variants: RAM Search walks MMU.MAIN_MEM and friends
	// directly for speed instead of going through the bus. byteSwapped mirrors
	// the old GetRamValue()/WriteValueAtSoftwareAddress() behaviour.
	static uint32_t ReadSoftwareValue(const uint8_t *address, int sizeInBytes, bool byteSwapped);
	static void WriteSoftwareValue(uint8_t *address, uint32_t value, int sizeInBytes, bool byteSwapped);

	// ---- Address helpers ------------------------------------------------

	// 0xFFFFFFFF-style addresses are shorthand for their low 24 bits.
	static void NormalizeAddress(uint32_t& address);

	// RAM Watch accepts every address: Windows' IsHardwareRAMAddressValid()
	// is still a TODO that returns true, and unmapped addresses simply read 0.
	static bool IsRAMAddressValid(uint32_t address);

	// Strict version for RAM Search: true only inside the three searched
	// regions. Kept separate so RAM Watch does not reject I/O or ROM watches.
	static bool IsSearchedRAMAddress(uint32_t address);

	// ---- Size/type codes used by .wch and by the search dialog -----------

	static int SizeInBytes(char sizeCode);
	static char SizeCode(int sizeInBytes);
	static bool IsValidSize(char sizeCode);
	static bool IsValidType(char typeCode);

	// ---- Formatting ------------------------------------------------------

	// sizeCode 'b'/'w'/'d', typeCode 's'/'u'/'h'/'f'. Matches the Windows
	// LVN_GETDISPINFO handler, with explicit 8/16/32-bit truncation so the
	// result is identical on LLP64 Windows and LP64 macOS/GTK.
	static std::string FormatValue(char sizeCode, char typeCode, uint32_t value);
	static std::string FormatAddress(uint32_t address);

	// Parses a hexadecimal address (accepts an optional 0x prefix, tolerates
	// the letter O in place of zero, and uses the last 8 hex digits).
	static uint32_t ParseAddress(const std::string& text, bool *outValid = 0);

	static int32_t AsSigned(char sizeCode, uint32_t value);
};

#endif // RAM_DEBUG_CORE_H
