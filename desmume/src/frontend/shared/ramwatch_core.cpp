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

#include "ramwatch_core.h"

#include <algorithm>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RAMWatchCore::Watch::Watch()
	: Address(0)
	, Size('b')
	, Type('s')
	, WrongEndian(false)
	, CurValue(0)
{
}

RAMWatchCore::RAMWatchCore()
	: _dirty(false)
	, _autoLoad(false)
	, _saveWindowPos(false)
	, _windowX(0)
	, _windowY(0)
{
}

RAMWatchCore& RAMWatchCore::Get()
{
	static RAMWatchCore s_core;
	return s_core;
}

#pragma mark - Document state

size_t RAMWatchCore::Count() const
{
	return _watches.size();
}

const RAMWatchCore::Watch& RAMWatchCore::At(size_t index) const
{
	static const Watch s_empty;
	return (index < _watches.size()) ? _watches[index] : s_empty;
}

bool RAMWatchCore::IsSeparator(size_t index) const
{
	return (index < _watches.size()) && (_watches[index].Size == 'S');
}

bool RAMWatchCore::IsSameWatch(const Watch& lhs, const Watch& rhs)
{
	// Separators are never duplicates (Windows: IsSameWatch()).
	if (rhs.Size == 'S')
	{
		return false;
	}
	return (lhs.Address == rhs.Address) && (lhs.Size == rhs.Size) && (lhs.Type == rhs.Type);
}

bool RAMWatchCore::Insert(const Watch& watch, size_t atIndex)
{
	for (size_t i = 0; i < _watches.size(); i++)
	{
		if (IsSameWatch(_watches[i], watch))
		{
			return false;
		}
	}

	if (_watches.size() >= kMaxWatchCount)
	{
		return false;
	}

	Watch newWatch = watch;
	newWatch.CurValue = ReadValue(newWatch);

	if (atIndex == kNoIndex || atIndex > _watches.size())
	{
		_watches.push_back(newWatch);
	}
	else
	{
		_watches.insert(_watches.begin() + atIndex, newWatch);
	}

	_dirty = true;
	return true;
}

bool RAMWatchCore::Replace(size_t index, const Watch& watch)
{
	if (index >= _watches.size())
	{
		return false;
	}

	const Watch oldWatch = _watches[index];
	const bool wasDirty = _dirty;
	_watches.erase(_watches.begin() + index);

	if (!Insert(watch, index))
	{
		_watches.insert(_watches.begin() + index, oldWatch);
		_dirty = wasDirty;
		return false;
	}
	return true;
}

void RAMWatchCore::Remove(size_t index)
{
	if (index >= _watches.size())
	{
		return;
	}
	_watches.erase(_watches.begin() + index);
	_dirty = true;
}

bool RAMWatchCore::MoveUp(size_t index)
{
	if (index == 0 || index >= _watches.size())
	{
		return false;
	}
	std::swap(_watches[index], _watches[index - 1]);
	_dirty = true;
	return true;
}

bool RAMWatchCore::MoveDown(size_t index)
{
	if (index + 1 >= _watches.size())
	{
		return false;
	}
	std::swap(_watches[index], _watches[index + 1]);
	_dirty = true;
	return true;
}

void RAMWatchCore::AddSeparator()
{
	Watch separator;
	separator.Address = 0;
	separator.Size = 'S';
	separator.Type = 'S';
	separator.WrongEndian = false;
	separator.Comment = "----------------------------";
	Insert(separator);
}

void RAMWatchCore::Clear()
{
	_watches.clear();
	_currentFile.clear();
	_dirty = false;
}

bool RAMWatchCore::InsertFromSearch(uint32_t address, char sizeCode, char typeCode, const std::string& comment)
{
	uint32_t normalized = address;
	RAMDebugCore::NormalizeAddress(normalized);

	Watch watch;
	watch.Address = normalized;
	watch.Size = RAMDebugCore::IsValidSize(sizeCode) ? sizeCode : 'b';
	watch.Type = RAMDebugCore::IsValidType(typeCode) ? typeCode : 'u';
	watch.WrongEndian = false;
	watch.Comment = comment;
	return Insert(watch);
}

#pragma mark - Values

uint32_t RAMWatchCore::ReadValue(const Watch& watch)
{
	if (watch.Size == 'S')
	{
		return 0;
	}
	return RAMDebugCore::ReadValue(watch.Address, RAMDebugCore::SizeInBytes(watch.Size));
}

bool RAMWatchCore::RefreshValues(std::vector<bool>& changed)
{
	changed.assign(_watches.size(), false);

	bool anyChanged = false;
	for (size_t i = 0; i < _watches.size(); i++)
	{
		Watch& watch = _watches[i];
		if (watch.Size == 'S')
		{
			continue;
		}

		const uint32_t newValue = ReadValue(watch);
		if (newValue != watch.CurValue)
		{
			watch.CurValue = newValue;
			changed[i] = true;
			anyChanged = true;
		}
	}
	return anyChanged;
}

std::string RAMWatchCore::FormatValue(const Watch& watch, uint32_t value)
{
	return RAMDebugCore::FormatValue(watch.Size, watch.Type, value);
}

std::string RAMWatchCore::DisplayValueAt(size_t index) const
{
	if (index >= _watches.size())
	{
		return std::string();
	}
	return FormatValue(_watches[index], _watches[index].CurValue);
}

std::string RAMWatchCore::DisplayAddress(const Watch& watch)
{
	return RAMDebugCore::FormatAddress(watch.Address);
}

#pragma mark - .wch files

std::string RAMWatchCore::TrimLineEnding(const std::string& line)
{
	std::string::size_type end = line.size();
	while ((end > 0) && ((line[end - 1] == '\n') || (line[end - 1] == '\r')))
	{
		end--;
	}
	return line.substr(0, end);
}

bool RAMWatchCore::LoadFile(const std::string& path, bool append, std::string& error)
{
	FILE *file = fopen(path.c_str(), "rb");
	if (file == 0)
	{
		error = "Could not open the watch file.";
		return false;
	}

	char lineBuffer[4096];

	// First line is the legacy mode byte. Windows writes a blank line and
	// ignores whatever it reads; do the same.
	if (fgets(lineBuffer, (int)sizeof(lineBuffer), file) == 0)
	{
		error = "The watch file is empty.";
		fclose(file);
		return false;
	}

	// Second line holds the number of watch rows.
	if (fgets(lineBuffer, (int)sizeof(lineBuffer), file) == 0)
	{
		error = "The watch file has no watch count.";
		fclose(file);
		return false;
	}

	const long declaredCount = strtol(lineBuffer, 0, 10);
	if (declaredCount < 0 || declaredCount > (long)kMaxWatchCount)
	{
		error = "The watch file contains an invalid watch count.";
		fclose(file);
		return false;
	}

	if (!append)
	{
		_watches.clear();
	}

	// Row layout: index \t address \t size \t type \t endian \t comment.
	// The comment is everything after the last tab, matching strrchr() in the
	// Windows loader.
	std::vector<std::string> fields;
	for (long row = 0; row < declaredCount; row++)
	{
		std::string line;
		do
		{
			if (fgets(lineBuffer, (int)sizeof(lineBuffer), file) == 0)
			{
				line.clear();
				break;
			}
			line = TrimLineEnding(lineBuffer);
		} while (line.empty());

		if (line.empty())
		{
			break;
		}

		fields.clear();
		std::string::size_type start = 0;
		while (true)
		{
			const std::string::size_type tab = line.find('\t', start);
			if (tab == std::string::npos)
			{
				fields.push_back(line.substr(start));
				break;
			}
			fields.push_back(line.substr(start, tab - start));
			start = tab + 1;
		}

		if (fields.size() < 5)
		{
			continue;
		}

		Watch watch;
		watch.Address = (uint32_t)strtoul(fields[1].c_str(), 0, 16);
		watch.Size = fields[2].empty() ? 'b' : fields[2][0];
		watch.Type = fields[3].empty() ? 's' : fields[3][0];
		watch.WrongEndian = (atoi(fields[4].c_str()) != 0);
		watch.Comment = (fields.size() >= 6) ? fields.back() : std::string();
		RAMDebugCore::NormalizeAddress(watch.Address);

		if (!RAMDebugCore::IsValidSize(watch.Size) && watch.Size != 'S')
		{
			watch.Size = 'b';
		}
		if (!RAMDebugCore::IsValidType(watch.Type) && watch.Type != 'S')
		{
			watch.Type = 's';
		}

		Insert(watch);
	}

	fclose(file);

	_currentFile = path;
	AddRecentFile(path);
	_dirty = false;
	return true;
}

bool RAMWatchCore::SaveFile(const std::string& path, std::string& error) const
{
	// Always truncate: the Windows version opened with "r+b" first, which left
	// stale bytes behind when the new list was shorter than the old one.
	FILE *file = fopen(path.c_str(), "wb");
	if (file == 0)
	{
		error = "Could not write the watch file.";
		return false;
	}

	fputc('\n', file);
	fprintf(file, "%d\n", (int)_watches.size());
	for (size_t i = 0; i < _watches.size(); i++)
	{
		const Watch& watch = _watches[i];
		fprintf(file, "%05X\t%08X\t%c\t%c\t%d\t%s\n",
		        (unsigned int)i,
		        (unsigned int)watch.Address,
		        watch.Size,
		        watch.Type,
		        watch.WrongEndian ? 1 : 0,
		        watch.Comment.c_str());
	}

	const bool ok = (ferror(file) == 0);
	if (!ok)
	{
		error = "Could not write the watch file.";
	}
	fclose(file);
	return ok;
}

bool RAMWatchCore::SaveCurrentFile(std::string& error)
{
	if (_currentFile.empty())
	{
		return false;
	}
	if (!SaveFile(_currentFile, error))
	{
		return false;
	}
	_dirty = false;
	return true;
}

bool RAMWatchCore::SaveAsFile(const std::string& path, std::string& error)
{
	if (!SaveFile(path, error))
	{
		return false;
	}
	_currentFile = path;
	AddRecentFile(path);
	_dirty = false;
	return true;
}

const std::string& RAMWatchCore::CurrentFile() const
{
	return _currentFile;
}

void RAMWatchCore::SetCurrentFile(const std::string& path)
{
	_currentFile = path;
}

bool RAMWatchCore::IsDirty() const
{
	return _dirty;
}

void RAMWatchCore::SetDirty(bool dirty)
{
	_dirty = dirty;
}

#pragma mark - Recent files

const std::vector<std::string>& RAMWatchCore::RecentFiles() const
{
	return _recentFiles;
}

void RAMWatchCore::SetRecentFiles(const std::vector<std::string>& files)
{
	_recentFiles.assign(files.begin(), files.end());
	if (_recentFiles.size() > kMaxRecentFiles)
	{
		_recentFiles.resize(kMaxRecentFiles);
	}
}

void RAMWatchCore::AddRecentFile(const std::string& path)
{
	if (path.empty())
	{
		return;
	}

	std::vector<std::string>::iterator found = std::find(_recentFiles.begin(), _recentFiles.end(), path);
	if (found != _recentFiles.end())
	{
		_recentFiles.erase(found);
	}
	_recentFiles.insert(_recentFiles.begin(), path);
	if (_recentFiles.size() > kMaxRecentFiles)
	{
		_recentFiles.resize(kMaxRecentFiles);
	}
}

std::string RAMWatchCore::RecentFileAt(size_t index) const
{
	return (index < _recentFiles.size()) ? _recentFiles[index] : std::string();
}

bool RAMWatchCore::OpenRecentFile(size_t index, std::string& error)
{
	const std::string path = RecentFileAt(index);
	if (path.empty())
	{
		return false;
	}

	// Move the chosen file to the top, as the Windows menu did.
	if (index != 0)
	{
		AddRecentFile(path);
	}

	if (!LoadFile(path, false, error))
	{
		RemoveRecentFile(0);
		return false;
	}
	return true;
}

void RAMWatchCore::RemoveRecentFile(size_t index)
{
	if (index < _recentFiles.size())
	{
		_recentFiles.erase(_recentFiles.begin() + index);
	}
}

#pragma mark - Preferences

bool RAMWatchCore::AutoLoad() const               { return _autoLoad; }
void RAMWatchCore::SetAutoLoad(bool enabled)     { _autoLoad = enabled; }
bool RAMWatchCore::SaveWindowPos() const         { return _saveWindowPos; }
void RAMWatchCore::SetSaveWindowPos(bool enabled){ _saveWindowPos = enabled; }
int RAMWatchCore::WindowX() const                { return _windowX; }
int RAMWatchCore::WindowY() const                { return _windowY; }
void RAMWatchCore::SetWindowPosition(int x, int y){ _windowX = x; _windowY = y; }
