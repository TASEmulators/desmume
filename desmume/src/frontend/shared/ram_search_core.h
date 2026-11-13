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

// Integration seam for the RAM Search port, deliberately created together with
// the RAM Watch port so the second port does not need to redesign anything.
//
// Today windows/ram_search.cpp still owns the search engine (a Win32 dialog
// plus a set of globals: ResultCount, rs_type_size, s_activeMemoryRegions,
// s_prevValues/s_curValues, prune(), CompactAddrs(), reset_address_info(),
// signal_new_frame(), sizeConv(), Update_RAM_Search(), ...). When RAM Search is
// ported, that engine moves into ram_search_core.cpp behind the interface
// below, and every frontend only implements a result list view. The low-level
// pieces it needs are already shared:
//
//   * RAMDebugCore                 - bus/software reads and writes, address
//                                   regions (kMainMemAddress, kItcmAddress,
//                                   kDtcmAddress, kMaxRamSize), value codes
//                                   and formatting
//   * RAMWatchCore::InsertFromSearch - "Add to Watch List" from a result row
//   * RAMSearchWatchBridge        - thin adapter below, so the search UI has
//                                   no dependency on the watch UI
//
// Nothing here is compiled into the search yet: the declarations only pin down
// the shape of the engine so the Cocoa/GTK work can proceed independently.

#ifndef RAM_SEARCH_CORE_H
#define RAM_SEARCH_CORE_H

#include <stdint.h>
#include <string>
#include <vector>

#include "ram_debug_core.h"
#include "ramwatch_core.h"

class RAMSearchWatchBridge
{
public:
	// Shared implementation of the "Add Watch" action in RAM Search. The host
	// dialog may still ask the user for a comment before calling this.
	static bool AddResultToWatchList(uint32_t address, char sizeCode, char typeCode,
	                                const std::string& comment)
	{
		return RAMWatchCore::Get().InsertFromSearch(address, sizeCode, typeCode, comment);
	}

	static uint32_t ReadResultValue(uint32_t address, char sizeCode)
	{
		return RAMDebugCore::ReadValue(address, RAMDebugCore::SizeInBytes(sizeCode));
	}
};

// ---------------------------------------------------------------------------
// Interface to be implemented by ram_search_core.cpp during the RAM Search
// port. Kept as a declaration on purpose: no frontend links against it yet.
// ---------------------------------------------------------------------------
class RAMSearchCore
{
public:
	enum SearchType
	{
		SEARCH_UNKNOWN_VALUE = 0,   // Windows: "Unknown" / reset_address_info()
		SEARCH_SPECIFIC_VALUE,      // Windows: "Specific Value"
		SEARCH_RELATIVE_VALUE,      // Windows: "Relative" (previous values)
		SEARCH_NUMBER_OF_CHANGES    // Windows: "Number of Changes"
	};

	enum CompareOperator
	{
		COMPARE_EQUAL = 0,
		COMPARE_NOT_EQUAL,
		COMPARE_GREATER_THAN,
		COMPARE_LESS_THAN,
		COMPARE_GREATER_OR_EQUAL,
		COMPARE_LESS_OR_EQUAL,
		COMPARE_DIFFERENT_BY,       // Windows: "Different By"
		COMPARE_MODULO              // Windows: "Modulo"
	};

	enum ValueType
	{
		VALUE_SIGNED = 0,           // rs_t 's'
		VALUE_UNSIGNED,             // rs_t 'u'
		VALUE_HEX,                  // rs_t 'h'
		VALUE_FIXED_20_12           // rs_t 'f'
	};

	struct MemoryRegion
	{
		uint32_t hardwareAddress;
		uint32_t size;
		uint8_t *softwareAddress;   // live MMU.MAIN_MEM / ARM9_ITCM / ARM9_DTCM
		bool isDtcm;
		uint32_t virtualIndex;
		uint32_t itemIndex;
	};

	struct Result
	{
		uint32_t address;
		uint32_t currentValue;
		uint32_t previousValue;
		uint16_t changeCount;
	};

	static RAMSearchCore& Get();

	// Lifecycle: reset_address_info(), signal_new_frame(), signal_new_size().
	void Reset();
	void SignalNewFrame();
	void SignalNewSize();

	// prune() / CompactAddrs() / undo.
	void Prune(SearchType search, CompareOperator op, ValueType type,
	           int32_t value, int32_t parameter);
	void CompactAddresses();
	bool Undo();

	// Results.
	size_t ResultCount() const;
	Result ResultAt(size_t index) const;
	void SetSize(char sizeCode);
	char Size() const;
	void SetMisaligned(bool enabled);
	void SetLittleEndian(bool enabled);

	// Adds a result row to the shared watch list.
	bool AddResultToWatchList(size_t index, char typeCode, const std::string& comment,
	                          bool *outDuplicate = 0);

private:
	RAMSearchCore();
};

#endif // RAM_SEARCH_CORE_H
