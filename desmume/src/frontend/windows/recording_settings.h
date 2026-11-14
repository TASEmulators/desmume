/*
	Copyright (C) 2026 DeSmuME team

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

#ifndef _RECORDING_SETTINGS_H_
#define _RECORDING_SETTINGS_H_

#pragma once

#include <windows.h>

#include "video_writer.h"

// The encoder picker shown before the save dialog.  Returns false when the
// user cancelled, in which case params is left untouched.
bool ShowRecordingSettings(HWND parent, RecordParams *params);

// What the ini remembers between runs.  A missing or unusable entry falls back
// to H.264 + AAC, which is what a first run records.
void LoadRecordingParams(RecordParams *params);
void SaveRecordingParams(const RecordParams &params);

#endif // _RECORDING_SETTINGS_H_
