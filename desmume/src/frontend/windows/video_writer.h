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

#ifndef _VIDEO_WRITER_H_
#define _VIDEO_WRITER_H_

#pragma once

// What the recording settings dialog decided, and what the ini remembers
// between runs.  A zero GUID means "not recorded": video and audio can each
// be switched off, but the dialog never allows both at once.
//
// Ported from vba-rerecording's src/win32/VideoWriter.h; the IVideoWriter
// interface that lived beside it is not needed here, because the Windows
// capture pipeline already owns the writer thread and the sink it drives is
// abstracted in aviout.h instead.

#include <windows.h>

// What the bit rate box accepts, in kbps.
enum
{
	VIDEO_BITRATE_MIN	  = 100,
	VIDEO_BITRATE_MAX	  = 100000,
	VIDEO_BITRATE_DEFAULT = 8000,

	// The Low / Medium / High entries of the preset box.
	VIDEO_BITRATE_LOW	 = 2000,
	VIDEO_BITRATE_MEDIUM = 4000,
	VIDEO_BITRATE_HIGH	 = 8000
};

enum
{
	AUDIO_BITRATE_MIN	  = 32,
	AUDIO_BITRATE_MAX	  = 512,
	AUDIO_BITRATE_DEFAULT = 192,

	AUDIO_BITRATE_LOW	 = 64,
	AUDIO_BITRATE_MEDIUM  = 128,
	AUDIO_BITRATE_HIGH	  = 192
};

// H.264 profiles, as eAVEncH264VProfile_* carries them.
//
// HEVC and AV1 have their own profile numbers, but there is nothing to pick:
// both offer exactly one 8-bit 4:2:0 profile (Main), and asking this machine
// for HEVC Main 10 makes the sink writer fail outright, because the frames
// reaching the encoder are 8-bit NV12.  So the dialog only offers a choice for
// H.264 and leaves the box on Auto and disabled for the other two.
enum
{
	VIDEO_PROFILE_AUTO = 0,

	H264_PROFILE_BASELINE = 66,
	H264_PROFILE_MAIN	  = 77,
	H264_PROFILE_HIGH	  = 100
};

// Frame skipping.  Frames are only ever produced at the DS LCD's own rate
// (33513982 / 560190 = 59.8261 Hz), so there is no fps to choose -- what the
// box really picks is a decimation factor: a step of N writes one frame out
// of every N and folds the dropped frames' time into the one that is kept, so
// the file plays at the right speed either way.
enum
{
	FRAME_STEP_NONE	   = 1,	// no skipping, every frame is written
	FRAME_STEP_DEFAULT = 1,
	FRAME_STEP_MAX	   = 4	// one frame in four, about 14.96 fps
};

struct RecordParams
{
	GUID videoSubtype;	 // MFVideoFormat_H264 / HEVC / AV1, or all zero
	bool videoHardware;	 // ask the sink writer for a hardware encoder MFT
	GUID audioSubtype;	 // MFAudioFormat_*, or all zero
	int	 videoBitrate;	 // kbps, VIDEO_BITRATE_*
	int	 audioBitrate;	 // kbps, AUDIO_BITRATE_*
	int	 profile;		 // profile value of the chosen codec, 0 = auto
	int	 frameStep;		 // FRAME_STEP_*, 1 = every frame
	bool async;			 // hand the frames to a worker thread to encode

	// What goes into the video stream.  RECORD_PICTURE_NATIVE writes the two
	// screens stacked, which is what this recorder always wrote; the composed
	// mode writes what the window shows.  The values live in
	// record_compositor.h as RECORD_PICTURE_* and RECORD_SIZE_*.
	int pictureMode;	 // RECORD_PICTURE_*
	int pictureSize;	 // RECORD_SIZE_*, composed only
	int pictureFilter;	 // a VideoInfo filter id, composed only

	// Client area of the emulator window when the recording starts, filled in
	// by RecordVideoTo().  Only read when pictureSize is RECORD_SIZE_WINDOW.
	int pictureWidth;
	int pictureHeight;
};

#endif // _VIDEO_WRITER_H_
