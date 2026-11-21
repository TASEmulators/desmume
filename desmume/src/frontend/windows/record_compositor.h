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

#ifndef _RECORD_COMPOSITOR_H_
#define _RECORD_COMPOSITOR_H_

#include <windows.h>

#include "types.h"
#include "GPU.h"

// Turns the emulator's two screens into the frames a recording is made of.
//
// Two shapes exist:
//
//   NATIVE   - the two screens stacked, one above the other, exactly as they
//              come out of the GPU.  This is what the AVI recorder has always
//              written and what the Media Foundation recorder wrote before
//              this file existed.
//
//   COMPOSED - what the window shows: the layout the user picked (stacked,
//              side by side, single screen), the screen gap, the rotation and
//              the CPU video filter, at a size frozen when the recording
//              started.
//
// The composed path is a CPU compositor on purpose.  Every display method this
// frontend has (DirectDraw, OpenGL, WebGPU) hands the composited frame *to* the
// GPU, so a readback would only add a copy of the same pixels plus a hard
// dependency on whichever display method happens to be running.  Doing it here
// works identically in all of them, and equally with no display at all.
//
// What cannot be reproduced here is the WebGPU upscaler chain (xbr-lv2,
// scalefx, ...): those are WGSL programs, and Media Foundation frames are
// produced on the CPU path.  That is why the filter menu this file honours is
// the Video > Filter one, not the GPU Filter one.

enum
{
	// RecordPicture::mode
	RECORD_PICTURE_NATIVE	= 0,
	RECORD_PICTURE_COMPOSED = 1
};

enum
{
	// RecordPicture::size.  Zero means "the window as it is when the recording
	// starts"; anything else is a whole multiple of the composed picture,
	// which is the only way to get an exact, resampling-free frame size.
	RECORD_SIZE_WINDOW = 0,
	RECORD_SIZE_1X	   = 1,
	RECORD_SIZE_2X	   = 2,
	RECORD_SIZE_3X	   = 3,
	RECORD_SIZE_4X	   = 4,

	RECORD_SIZE_MAX = RECORD_SIZE_4X
};

// Sizes the composed canvas is allowed to reach.  A 4K window is already more
// than any encoder here wants, and the pending-frame queue in aviout.cpp is
// sized in bytes, so an absurd canvas would starve it.
enum
{
	RECORD_CANVAS_MIN = 32,
	RECORD_CANVAS_MAX = 3840
};

struct RecordPicture
{
	int mode;	   // RECORD_PICTURE_*
	int size;	   // RECORD_SIZE_*, composed only
	int width;	   // composed only: canvas size, already decided by the caller
	int height;
	int layout;	   // video.layout    at the moment the recording started
	int rotation;  // video.rotation
	int screengap; // video.screengap
	int swap;	   // video.swap
	bool mainScreenIsEngineMain; // what video.swap 2 and 3 are about
	int filter;	   // VideoInfo::NONE or one of the CPU filters
	int gapColor;  // 0x00BBGGRR, the colour the window paints the gap with
};

class RecordCompositor
{
public:
	RecordCompositor();
	~RecordCompositor();

	// Freezes the geometry and sizes the working buffers.  srcWidth and
	// srcHeight are one screen's size in the GPU buffer (customWidth and
	// customHeight).  Returns false when the picture cannot be composed, in
	// which case the caller should fall back to the native path.
	bool Configure(const RecordPicture &picture, size_t srcWidth, size_t srcHeight);

	size_t SourceWidth() const { return this->_srcWidth; }
	size_t SourceHeight() const { return this->_srcHeight; }

	size_t OutputWidth() const { return this->_outWidth; }
	size_t OutputHeight() const { return this->_outHeight; }

	// One frame.  Copies the frame into the working buffer, applies the
	// backlight the display applies and runs the filter, all on this thread.
	// Cheap enough to sit on the emulation thread: one 32-bit conversion of
	// the frame, and the filter only when one was asked for.
	void BeginFrame(const void *src, const NDSColorFormat colorFormat,
	                float mainIntensity, float touchIntensity);

	// Rows [firstLine, lastLine] of the output, inclusive, at 24bpp BGR and
	// bottom-up, the way both writers in aviout.cpp expect a frame.  Split
	// across threads by the caller; every call reads the same source.
	void ComposeRows(u8 *dstBase, size_t firstLine, size_t lastLine) const;

private:
	struct ScreenRect
	{
		int srcX, srcY, srcW, srcH;
		int dstX, dstY, dstW, dstH;
	};

	size_t _srcWidth;	// one screen, in source pixels
	size_t _srcHeight;
	size_t _outWidth;
	size_t _outHeight;

	int _rotation;
	u32 _gapColor;
	int _screenCount;
	ScreenRect _screen[2];
	ScreenRect _gapRect;

	// 32-bit working copies.  Rows are tightly packed, which is what the CPU
	// filters require: they are handed a Pitch of width * 2 ("16-bit pitch")
	// and scale it themselves, so a padded stride shears the picture.  The
	// extra rows around the picture are over-allocation only -- several of the
	// filters read a row above and below the surface they are given.
	u32 *_src;
	u32 *_srcSurface;
	u32 *_filtered;
	u32 *_filteredSurface;
	size_t _filteredWidth;
	size_t _filteredHeight;
	int _filter;

	bool _ready;

	void FreeBuffers();
	void FilterFrame();
	void ComposeRowsTo(u8 *dstBase, size_t firstLine, size_t lastLine, const u32 *src, size_t srcPitchPx) const;
};

namespace recordCompose
{
	// The scale a CPU filter applies to the picture, as VideoInfo::setfilter()
	// maps it.  Kept here so the settings dialog can show the resulting size.
	double FilterScale(int filter);

	// Size of the picture the layout produces, before the record size
	// multiplier: two screens stacked, side by side or just the main one,
	// with the gap between them and the rotated orientation.  filterScale is
	// 1 for no filter, 2/3/4/5 for the integer upscalers and 1.5 for the four
	// that scale by three halves.
	void ContentSize(size_t screenWidth, size_t screenHeight, int layout, int screengap, int rotation,
	                 double filterScale, size_t *outWidth, size_t *outHeight);

	// Name of a filter, for the settings dialog.  NULL for an index this
	// build does not have.
	const char *FilterName(int filter);
	int FilterCount();
}

#endif // _RECORD_COMPOSITOR_H_
