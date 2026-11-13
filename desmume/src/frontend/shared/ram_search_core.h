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

// Frontend-independent RAM Search engine, used by the Cocoa port and ready for
// a future GTK port. windows/ram_search.cpp keeps its legacy virtual-index
// engine for now (its dialog is tightly coupled to that state); the low-level
// bus reads/writes and the Add-to-Watch action are already shared with this
// engine through RAMDebugCore and RAMWatchCore, so switching Windows over later
// only requires replacing its result source.
//
// The engine walks the three NDS RAM regions (main RAM, ITCM, DTCM) through
// the live MMU arrays, keeps a candidate vector with previous/current values
// and a change counter, and filters it with the same search categories and
// comparison operators as the Windows dialog:
//
//   rs_c 's' -> SEARCH_SPECIFIC_VALUE       rs_o '='  -> COMPARE_EQUAL
//   rs_c 'r' -> SEARCH_RELATIVE_VALUE       rs_o '!'  -> COMPARE_NOT_EQUAL
//   rs_c 'a' -> SEARCH_SPECIFIC_ADDRESS    rs_o '<'/'>'/'l'/'m' -> ordering
//   rs_c 'n' -> SEARCH_NUMBER_OF_CHANGES   rs_o 'd'  -> COMPARE_DIFFERENT_BY
//                                           rs_o '%'  -> COMPARE_MODULO

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

class RAMSearchCore
{
public:
	enum SearchType
	{
		SEARCH_SPECIFIC_VALUE = 0,  // Windows rs_c 's': current value vs typed value
		SEARCH_RELATIVE_VALUE,      // Windows rs_c 'r': current value vs previous
		SEARCH_SPECIFIC_ADDRESS,    // Windows rs_c 'a': address vs typed value
		SEARCH_NUMBER_OF_CHANGES    // Windows rs_c 'n': change count vs typed value
	};

	enum CompareOperator
	{
		COMPARE_EQUAL = 0,          // '='
		COMPARE_NOT_EQUAL,          // '!'
		COMPARE_LESS_THAN,          // '<'
		COMPARE_GREATER_THAN,       // '>'
		COMPARE_LESS_OR_EQUAL,      // 'l'
		COMPARE_GREATER_OR_EQUAL,   // 'm'
		COMPARE_DIFFERENT_BY,       // 'd'  (uses parameter as the delta)
		COMPARE_MODULO              // '%'  (uses parameter as the modulus)
	};

	enum ValueType
	{
		VALUE_SIGNED = 0,           // 's'
		VALUE_UNSIGNED,             // 'u'
		VALUE_HEX,                  // 'h'
		VALUE_FIXED_20_12           // 'f'
	};

	struct Result
	{
		uint32_t address;
		uint32_t currentValue;
		uint32_t previousValue;
		uint16_t changeCount;
	};

	static RAMSearchCore& Get();

	// ---- Options --------------------------------------------------------

	void SetSize(char sizeCode);          // 'b' / 'w' / 'd'
	char Size() const;
	void SetMisaligned(bool enabled);     // allow unaligned addresses
	bool Misaligned() const;
	void SetLittleEndian(bool enabled);
	bool LittleEndian() const;

	void SetSearchType(SearchType type);
	SearchType GetSearchType() const;
	void SetCompareOperator(CompareOperator op);
	CompareOperator GetCompareOperator() const;
	void SetValueType(ValueType type);
	ValueType GetValueType() const;
	void SetSearchValue(int32_t value);
	int32_t SearchValue() const;
	void SetParameter(int32_t parameter);
	int32_t Parameter() const;

	// ---- Lifecycle -------------------------------------------------------

	// Enumerates every candidate address and snapshots the current values.
	void Reset();
	// Called once per emulated frame: refreshes current values and change counts.
	void SignalNewFrame();
	// Re-enumerates candidates after a size/alignment change.
	void SignalNewSize();

	// ---- Searching -------------------------------------------------------

	// Filters the candidate list with the current options.
	void Search();
	void Prune(SearchType search, CompareOperator op, ValueType type,
	           int32_t value, int32_t parameter);
	void CompactAddresses();
	bool CanUndo() const;
	bool Undo();

	// ---- Results ---------------------------------------------------------

	size_t ResultCount() const;
	Result ResultAt(size_t index) const;

	// Adds a result row to the shared watch list.
	bool AddResultToWatchList(size_t index, char typeCode, const std::string& comment,
	                          bool *outDuplicate = 0);

private:
	RAMSearchCore();
	RAMSearchCore(const RAMSearchCore&);
	RAMSearchCore& operator=(const RAMSearchCore&);

	struct Candidate
	{
		uint32_t address;
		uint32_t previousValue;
		uint32_t currentValue;
		uint16_t changeCount;
		uint8_t regionIndex;
		uint8_t reserved;
	};

	static uint32_t ReadCandidate(const Candidate& candidate, int sizeInBytes, bool littleEndian);
	static bool Compare(CompareOperator op, uint32_t x, uint32_t y, uint32_t parameter, bool signedMode);

	std::vector<Candidate> _candidates;
	std::vector<Candidate> _undoSnapshot;
	bool _hasUndo;

	char _size;
	bool _misaligned;
	bool _littleEndian;
	SearchType _searchType;
	CompareOperator _operator;
	ValueType _valueType;
	int32_t _searchValue;
	int32_t _parameter;
};

#endif // RAM_SEARCH_CORE_H
