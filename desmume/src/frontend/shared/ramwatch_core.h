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

// Frontend-independent RAM Watch document, shared by the Windows, Cocoa and
// (future) GTK frontends. This is the logic that used to live inside
// windows/ramwatch.cpp, minus the Win32 dialogs, global arrays, malloc'd
// comment strings and hand-rolled sscanf parsing.
//
// The host frontend owns everything platform specific: file dialogs, message
// boxes, list/tree view refresh and preference persistence. It calls into this
// class and then refreshes its own widgets. Persistence is deliberately not
// done here (INI on Windows, NSUserDefaults on macOS, GSettings on GTK): the
// host reads/writes the preference values through the accessors below.
//
// Low-level memory access and value formatting live in ram_debug_core.h, which
// is shared with RAM Search. This header must stay free of platform headers;
// in particular it does not include types.h (BOOL/TRUE/FALSE break Cocoa).

#ifndef RAMWATCH_CORE_H
#define RAMWATCH_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <string>
#include <vector>

#include "ram_debug_core.h"

class RAMWatchCore
{
public:
	// Size: 'b' = 1 byte, 'w' = 2 bytes, 'd' = 4 bytes, 'S' = separator row.
	// Type: 's' = signed, 'u' = unsigned, 'h' = hexadecimal, 'f' = 20.12 fixed
	//       point, 'S' = separator row.
	// These character codes are part of the .wch file format, so they are kept
	// verbatim from the Windows implementation.
	struct Watch
	{
		uint32_t Address;
		char Size;
		char Type;
		bool WrongEndian;
		uint32_t CurValue;
		std::string Comment;

		Watch();
	};

	static const size_t kMaxWatchCount = 256;
	static const size_t kMaxRecentFiles = 5;
	static const size_t kNoIndex = (size_t)-1;

	static RAMWatchCore& Get();

	// ---- Document state -------------------------------------------------

	size_t Count() const;
	const Watch& At(size_t index) const;

	bool IsSeparator(size_t index) const;

	// Insert rejects duplicates (same address/size/type) and honours the
	// 256-entry cap, exactly like the old InsertWatch(). kNoIndex appends.
	bool Insert(const Watch& watch, size_t atIndex = kNoIndex);

	// Edit in place. The old row is removed first so an unchanged edit is not
	// rejected by duplicate detection; it is restored if the new row collides.
	bool Replace(size_t index, const Watch& watch);

	void Remove(size_t index);
	bool MoveUp(size_t index);
	bool MoveDown(size_t index);
	void AddSeparator();

	// Empties the list and drops the associated file. Asking the user about
	// unsaved changes is the host's job (see AskSave() on Windows).
	void Clear();

	// Integration point for RAM Search: "Add to Watch List" from a search
	// result becomes a single call, on every frontend.
	bool InsertFromSearch(uint32_t address, char sizeCode, char typeCode, const std::string& comment);

	// ---- Values ----------------------------------------------------------

	// Re-reads every watch through RAMDebugCore. changed receives one flag per
	// watch; returns true when at least one cached value changed.
	bool RefreshValues(std::vector<bool>& changed);

	std::string DisplayValueAt(size_t index) const;
	static std::string DisplayAddress(const Watch& watch);
	static uint32_t ReadValue(const Watch& watch);
	static std::string FormatValue(const Watch& watch, uint32_t value);

	// ---- .wch files -----------------------------------------------------

	// Append == false replaces the current list. The on-disk format is the one
	// produced by Save_Watches() on Windows, so files stay interchangeable.
	bool LoadFile(const std::string& path, bool append, std::string& error);
	bool SaveFile(const std::string& path, std::string& error) const;
	// Saves to the associated file; returns false when there is none (the host
	// should then present a Save As dialog).
	bool SaveCurrentFile(std::string& error);
	// Saves to path and makes it the associated file.
	bool SaveAsFile(const std::string& path, std::string& error);

	const std::string& CurrentFile() const;
	void SetCurrentFile(const std::string& path);

	bool IsDirty() const;
	void SetDirty(bool dirty);

	// ---- Recent files ----------------------------------------------------

	const std::vector<std::string>& RecentFiles() const;
	void SetRecentFiles(const std::vector<std::string>& files);
	void AddRecentFile(const std::string& path);
	std::string RecentFileAt(size_t index) const;
	// Clears and loads RecentFileAt(index), moving it to the top of the list.
	bool OpenRecentFile(size_t index, std::string& error);
	void RemoveRecentFile(size_t index);

	// ---- Preferences (values only; persistence belongs to the host) ------

	bool AutoLoad() const;
	void SetAutoLoad(bool enabled);
	bool SaveWindowPos() const;
	void SetSaveWindowPos(bool enabled);
	int WindowX() const;
	int WindowY() const;
	void SetWindowPosition(int x, int y);

private:
	RAMWatchCore();
	RAMWatchCore(const RAMWatchCore&);
	RAMWatchCore& operator=(const RAMWatchCore&);

	static bool IsSameWatch(const Watch& lhs, const Watch& rhs);
	static std::string TrimLineEnding(const std::string& line);

	std::vector<Watch> _watches;
	std::string _currentFile;
	std::vector<std::string> _recentFiles;
	bool _dirty;

	bool _autoLoad;
	bool _saveWindowPos;
	int _windowX;
	int _windowY;
};

#endif // RAMWATCH_CORE_H
