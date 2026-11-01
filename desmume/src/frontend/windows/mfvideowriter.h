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

#ifndef _MFVIDEOWRITER_H_
#define _MFVIDEOWRITER_H_

#pragma once

#include <windows.h>

#include "types.h"
#include "video_writer.h"

// Writes what the recording dialog asked for through an IMFSinkWriter:
// MP4 with H.264/HEVC/AV1, or an audio-only .m4a / .mp3 / .flac.
//
// The sink writer owns the encoder, so no CLSID is handed to it; the only
// choice that reaches it is MF_READWRITE_ENABLE_HARDWARE_TRANSFORMS, which
// is what the "Hardware" row of the encoder list sets.
//
// Everything Media Foundation is behind a pimpl: the header stays usable from
// the rest of the Win32 layer, which still builds at the XP API level for the
// MSVC XP toolsets and must not see <mfapi.h>.
class MFVideoWriter
{
public:
	MFVideoWriter(const RecordParams &params);
	~MFVideoWriter();

	bool Open(const wchar_t *filename);

	bool AddFrame(const u8 *bgr24BottomUp);
	void SetVideoFormat(BITMAPINFOHEADER *bh);
	int	 videoWidth() const;
	int	 videoHeight() const;
	int	 videoFrames();

	bool IsSoundAdded();
	void SetSoundFormat(WAVEFORMATEX *format);
	bool AddSound(const u8 *sound, u32 len);

	void Pause(bool pause);
	bool IsPaused();

	// True once a Media Foundation call has failed.  The sink in aviout.cpp
	// watches this to end the recording the same way an AVI write error does.
	bool HasFailed();

private:
	struct Impl;
	Impl *m_impl;

	MFVideoWriter(const MFVideoWriter &);
	MFVideoWriter &operator=(const MFVideoWriter &);
};

#endif // _MFVIDEOWRITER_H_
