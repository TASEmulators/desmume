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

#ifndef DSMEDITOR_H
#define DSMEDITOR_H

#include <windows.h>

// File > DSM Editor...: the recorded input of the loaded .dsm as a frame grid.
//
// Rows are frames, columns are the inputs the movie actually contains: the 13
// pad bits, the stylus, the microphone button, the lid and (when the movie has
// one) the reset command.  The window is a view over the timeline - it reads
// and rewrites individual records and can insert or delete whole frames - but
// it never seeks, never pauses, never changes movieMode and never starts or
// stops a recording.  See the contract above FCEUMOV_SetRecord() in movie.cpp.
//
// Editing one cell works whenever a movie is loaded; inserting, deleting and
// pasting frames need the emulator paused, because those move the frame the
// playhead points at and the running emulation would otherwise replay a
// different timeline from one frame to the next.  The window keeps refreshing
// while the movie plays, so it doubles as a viewer - the playhead follows
// currFrameCounter.
//
// The list is a virtual list (LVS_OWNERDATA): only the rows on screen ask for
// their text, so a three-hour movie costs exactly as much as a three-second
// one and nothing here grows with the frame count.
//
// It is multi-select, but the selection is only a *target set* for the row
// commands (delete, cut/copy, mark): a plain click on a cell still flips one
// input of one frame, and Ctrl/Shift are treated as selection gestures, never
// as a bulk edit.  The frame box always shows the focused row.
//
// The transport buttons are not a second player: they forward the File menu's
// own Pause and the frame-advance hotkey to the main window, so the emulator
// keeps exactly one playback path.
void DSMEditorOpen(HWND parent);

// The window handle, or NULL while the editor is closed.  The message loop in
// main.cpp asks for it so Tab, Enter and the arrow keys reach the dialog.
HWND DSMEditorGetHwnd();

#endif // DSMEDITOR_H
