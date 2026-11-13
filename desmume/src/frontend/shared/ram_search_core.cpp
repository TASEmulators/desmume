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

#include "ram_search_core.h"

#include <string.h>

#include "../../MMU.h"

namespace
{
	const uint32_t kRegionBase[3] =
	{
		RAMDebugCore::kMainMemAddress,
		RAMDebugCore::kItcmAddress,
		RAMDebugCore::kDtcmAddress
	};

	const uint32_t kRegionSize[3] =
	{
		RAMDebugCore::kMainMemSize,
		RAMDebugCore::kItcmSize,
		RAMDebugCore::kDtcmSize
	};

	// Resolved at call time rather than as a static initializer: MMU is an
	// extern global object, and these are its live RAM arrays.
	const uint8_t *RegionData(int index)
	{
		switch (index)
		{
			case 0:  return (const uint8_t *)MMU.MAIN_MEM;
			case 1:  return (const uint8_t *)MMU.ARM9_ITCM;
			default: return (const uint8_t *)MMU.ARM9_DTCM;
		}
	}

	// Only snapshots of reasonably small result sets are kept for Undo, so a
	// first search over four million candidates never doubles the memory use.
	const size_t kMaxUndoCandidates = 200000;
}

RAMSearchCore::RAMSearchCore()
	: _hasUndo(false)
	, _hasRedo(false)
	, _size('b')
	, _misaligned(false)
	, _littleEndian(true)
	, _searchType(SEARCH_SPECIFIC_VALUE)
	, _operator(COMPARE_EQUAL)
	, _valueType(VALUE_SIGNED)
	, _searchValue(0)
	, _parameter(0)
{
}

RAMSearchCore& RAMSearchCore::Get()
{
	static RAMSearchCore s_core;
	return s_core;
}

#pragma mark - Options

void RAMSearchCore::SetSize(char sizeCode)
{
	if (!RAMDebugCore::IsValidSize(sizeCode) || (sizeCode == _size))
	{
		return;
	}
	_size = sizeCode;
	SignalNewSize();
}

char RAMSearchCore::Size() const                  { return _size; }

void RAMSearchCore::SetMisaligned(bool enabled)
{
	if (enabled == _misaligned)
	{
		return;
	}
	_misaligned = enabled;
	SignalNewSize();
}

bool RAMSearchCore::Misaligned() const           { return _misaligned; }

void RAMSearchCore::SetLittleEndian(bool enabled)
{
	if (enabled == _littleEndian)
	{
		return;
	}
	_littleEndian = enabled;
	SignalNewSize();
}

bool RAMSearchCore::LittleEndian() const         { return _littleEndian; }

void RAMSearchCore::SetSearchType(SearchType type)                 { _searchType = type; }
RAMSearchCore::SearchType RAMSearchCore::GetSearchType() const    { return _searchType; }
void RAMSearchCore::SetCompareOperator(CompareOperator op)        { _operator = op; }
RAMSearchCore::CompareOperator RAMSearchCore::GetCompareOperator() const { return _operator; }
void RAMSearchCore::SetValueType(ValueType type)                  { _valueType = type; }
RAMSearchCore::ValueType RAMSearchCore::GetValueType() const      { return _valueType; }
void RAMSearchCore::SetSearchValue(int32_t value)                 { _searchValue = value; }
int32_t RAMSearchCore::SearchValue() const                        { return _searchValue; }
void RAMSearchCore::SetParameter(int32_t parameter)               { _parameter = parameter; }
int32_t RAMSearchCore::Parameter() const                          { return _parameter; }

#pragma mark - Lifecycle

uint32_t RAMSearchCore::ReadCandidate(const Candidate& candidate, int sizeInBytes, bool littleEndian)
{
	const uint8_t *pointer = RegionData(candidate.regionIndex) +
	                         (candidate.address - kRegionBase[candidate.regionIndex]);

	uint32_t value = 0;
	if (littleEndian)
	{
		for (int i = 0; i < sizeInBytes; i++)
		{
			value |= ((uint32_t)pointer[i]) << (8 * i);
		}
	}
	else
	{
		// Windows' GetRamValue() assembles big-endian by default.
		for (int i = 0; i < sizeInBytes; i++)
		{
			value = (value << 8) | pointer[i];
		}
	}
	return value;
}

void RAMSearchCore::Reset()
{
	_candidates.clear();
	_hasUndo = false;
	_hasRedo = false;
	_undoSnapshot.clear();
	_redoSnapshot.clear();

	const int sizeInBytes = RAMDebugCore::SizeInBytes(_size);
	if (sizeInBytes == 0)
	{
		return;
	}
	const uint32_t step = _misaligned ? 1u : (uint32_t)sizeInBytes;

	for (int region = 0; region < 3; region++)
	{
		const uint32_t base = kRegionBase[region];
		const uint32_t end = base + kRegionSize[region];
		for (uint32_t address = base; address + (uint32_t)sizeInBytes <= end; address += step)
		{
			Candidate candidate;
			memset(&candidate, 0, sizeof(candidate));
			candidate.address = address;
			candidate.regionIndex = (uint8_t)region;
			candidate.currentValue = ReadCandidate(candidate, sizeInBytes, _littleEndian);
			candidate.previousValue = candidate.currentValue;
			_candidates.push_back(candidate);
		}
	}
}

void RAMSearchCore::SignalNewFrame()
{
	const int sizeInBytes = RAMDebugCore::SizeInBytes(_size);
	if (sizeInBytes == 0)
	{
		return;
	}

	for (size_t i = 0; i < _candidates.size(); i++)
	{
		Candidate& candidate = _candidates[i];
		const uint32_t value = ReadCandidate(candidate, sizeInBytes, _littleEndian);
		if (value != candidate.currentValue)
		{
			candidate.currentValue = value;
			if (candidate.changeCount != 0xFFFF)
			{
				candidate.changeCount++;
			}
		}
	}
}

void RAMSearchCore::SignalNewSize()
{
	// Candidate addresses depend on the step size and alignment, so they are
	// rebuilt from scratch. The UI warns that this discards search progress.
	Reset();
}

void RAMSearchCore::ClearChangeCounts()
{
	for (size_t i = 0; i < _candidates.size(); i++)
	{
		_candidates[i].changeCount = 0;
	}
}

void RAMSearchCore::CompactAddresses()
{
	// The candidate vector is always compact; kept for API parity with the
	// Windows engine's CompactAddrs().
}

#pragma mark - Searching

bool RAMSearchCore::Compare(CompareOperator op, uint32_t x, uint32_t y,
                            uint32_t parameter, bool signedMode)
{
	if (op == COMPARE_DIFFERENT_BY)
	{
		const uint32_t delta = (x >= y) ? (x - y) : (y - x);
		return (delta == parameter);
	}
	if (op == COMPARE_MODULO)
	{
		return (parameter != 0) && ((x % parameter) == y);
	}

	if (signedMode)
	{
		const int32_t sx = (int32_t)x;
		const int32_t sy = (int32_t)y;
		switch (op)
		{
			case COMPARE_EQUAL:            return sx == sy;
			case COMPARE_NOT_EQUAL:        return sx != sy;
			case COMPARE_LESS_THAN:        return sx <  sy;
			case COMPARE_GREATER_THAN:     return sx >  sy;
			case COMPARE_LESS_OR_EQUAL:    return sx <= sy;
			case COMPARE_GREATER_OR_EQUAL: return sx >= sy;
			default:                       return false;
		}
	}

	switch (op)
	{
		case COMPARE_EQUAL:            return x == y;
		case COMPARE_NOT_EQUAL:        return x != y;
		case COMPARE_LESS_THAN:        return x <  y;
		case COMPARE_GREATER_THAN:     return x >  y;
		case COMPARE_LESS_OR_EQUAL:    return x <= y;
		case COMPARE_GREATER_OR_EQUAL: return x >= y;
		default:                       return false;
	}
}

void RAMSearchCore::Prune(SearchType search, CompareOperator op, ValueType type,
                          int32_t value, int32_t parameter)
{
	_hasUndo = (_candidates.size() <= kMaxUndoCandidates);
	if (_hasUndo)
	{
		_undoSnapshot = _candidates;
	}
	else
	{
		_undoSnapshot.clear();
	}

	// A fresh search invalidates the redo branch.
	_hasRedo = false;
	_redoSnapshot.clear();

	const bool signedMode = (type == VALUE_SIGNED) || (type == VALUE_FIXED_20_12);
	const uint32_t typedValue = (uint32_t)value;
	const uint32_t typedParameter = (uint32_t)parameter;

	std::vector<Candidate> kept;
	kept.reserve(_candidates.size());
	for (size_t i = 0; i < _candidates.size(); i++)
	{
		const Candidate& candidate = _candidates[i];
		uint32_t x = 0;
		uint32_t y = typedValue;
		uint32_t p = typedParameter;

		switch (search)
		{
			case SEARCH_SPECIFIC_VALUE:
				x = candidate.currentValue;
				break;

			case SEARCH_RELATIVE_VALUE:
				x = candidate.currentValue;
				y = candidate.previousValue;
				break;

			case SEARCH_SPECIFIC_ADDRESS:
				x = candidate.address;
				break;

			case SEARCH_NUMBER_OF_CHANGES:
				x = candidate.changeCount;
				break;
		}

		if (Compare(op, x, y, p, signedMode))
		{
			kept.push_back(candidate);
		}
	}
	_candidates.swap(kept);

	// The surviving values become the "previous" baseline for the next search.
	for (size_t i = 0; i < _candidates.size(); i++)
	{
		_candidates[i].previousValue = _candidates[i].currentValue;
	}
}

void RAMSearchCore::Search()
{
	Prune(_searchType, _operator, _valueType, _searchValue, _parameter);
}

bool RAMSearchCore::CanUndo() const
{
	return _hasUndo && !_undoSnapshot.empty();
}

bool RAMSearchCore::Undo()
{
	if (!CanUndo())
	{
		return false;
	}

	// The list being replaced becomes the redo branch.
	_redoSnapshot.swap(_candidates);
	_candidates.swap(_undoSnapshot);
	_undoSnapshot.clear();
	_hasUndo = false;
	_hasRedo = true;
	return true;
}

bool RAMSearchCore::CanRedo() const
{
	return _hasRedo && !_redoSnapshot.empty();
}

bool RAMSearchCore::Redo()
{
	if (!CanRedo())
	{
		return false;
	}

	_undoSnapshot.swap(_candidates);
	_candidates.swap(_redoSnapshot);
	_redoSnapshot.clear();
	_hasUndo = true;
	_hasRedo = false;
	return true;
}

#pragma mark - Results

size_t RAMSearchCore::ResultCount() const
{
	return _candidates.size();
}

RAMSearchCore::Result RAMSearchCore::ResultAt(size_t index) const
{
	Result result;
	memset(&result, 0, sizeof(result));
	if (index >= _candidates.size())
	{
		return result;
	}

	const Candidate& candidate = _candidates[index];
	result.address = candidate.address;
	result.currentValue = candidate.currentValue;
	result.previousValue = candidate.previousValue;
	result.changeCount = candidate.changeCount;
	return result;
}

bool RAMSearchCore::Eliminate(size_t index)
{
	if (index >= _candidates.size())
	{
		return false;
	}

	_candidates.erase(_candidates.begin() + (std::vector<Candidate>::difference_type)index);

	// The undo and redo snapshots belong to the last search. Keeping them
	// around would silently resurrect the eliminated row, so they are dropped.
	_hasUndo = false;
	_hasRedo = false;
	_undoSnapshot.clear();
	_redoSnapshot.clear();
	return true;
}

bool RAMSearchCore::AddResultToWatchList(size_t index, char typeCode,
                                          const std::string& comment, bool *outDuplicate)
{
	if (index >= _candidates.size())
	{
		return false;
	}

	const bool inserted = RAMSearchWatchBridge::AddResultToWatchList(
		_candidates[index].address, _size, typeCode, comment);
	if (outDuplicate != 0)
	{
		*outDuplicate = !inserted;
	}
	return inserted;
}
