//RamWatch dialog was copied and adapted from GENS11: http://code.google.com/p/gens-rerecording/
//Authors: adelikat, Upthorn, Nitsuja

/*
	Modifications Copyright (C) 2009-2026 DeSmuME team

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

#ifndef RAMWATCH_H
#define RAMWATCH_H

#include <windows.h>

// All document logic (watch list, .wch files, memory reads, value formatting,
// recent files and preference values) now lives in the frontend-independent
// core, shared with the Cocoa port and any future GTK port. This header is the
// Win32 adapter only: dialogs, menus, list view and file pickers.
#include "../shared/ramwatch_core.h"

typedef RAMWatchCore::Watch AddressWatcher;

static const int MAX_RECENT_WATCHES = (int)RAMWatchCore::kMaxRecentFiles;
static const int MAX_WATCH_COUNT = (int)RAMWatchCore::kMaxWatchCount;

extern HWND RamWatchHWnd;

LRESULT CALLBACK RamWatchProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

// Fills a report-style list view with the given column headers.  Shared with
// the RAM Search dialog, which builds its own column set on top of it.
void init_list_box(HWND Box, const char* Strs[], int numColumns, int *columnWidths);

// Host-side operations. These are thin wrappers around RAMWatchCore that also
// drive the Win32 user interface (message boxes, file dialogs, list refresh).
bool ResetWatches();
void OpenRWRecentFile(int recentFileNumber);
bool AskSave();

bool InsertWatch(const AddressWatcher& watch, const char *comment, int atIndex = -1);
bool InsertWatch(const AddressWatcher& watch, HWND parent = NULL); // asks the user for a comment

void Update_RAM_Watch();
bool Load_Watches(bool clear, const char *filename);
bool Load_Watches(bool clear);   // opens the file dialog
bool Save_Watches();             // Save As
bool QuickSaveWatches();         // Save, falling back to Save As

#endif
