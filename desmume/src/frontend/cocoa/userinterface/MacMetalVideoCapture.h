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

#ifndef _MAC_METAL_VIDEO_CAPTURE_H_
#define _MAC_METAL_VIDEO_CAPTURE_H_

#include <stddef.h>

#include "../ClientDisplayView.h"

// One recording's worth of capture geometry.  These are exactly the options the
// screenshot tool offers; the capture presenter applies them through the same
// ClientDisplayPresenter properties the on-screen view uses, which is what
// makes rotation / view scale / hybrid layouts work for video as well.
struct MacVideoCaptureGeometry
{
	size_t width;
	size_t height;
	ClientDisplayMode mode;
	ClientDisplayLayout layout;
	ClientDisplayOrder order;
	double gapScale;
	double rotation;
	bool useDeposterize;
	OutputFilterTypeID outputFilterID;
	VideoFilterTypeID pixelScalerID;
};

// Returns the MetalDisplayViewSharedData behind a Metal display presenter, or
// NULL when the presenter is not a Metal one (OpenGL front end, for example).
// Declared as void* so that plain C++ translation units (ClientVideoOutput.cpp)
// never have to include the Objective-C headers.
void *MacMetalPresenterGetSharedData(void *presenter);

// Creates a dedicated capture presenter that shares the display's Metal device,
// command queue and fetch textures, but owns its own geometry, filter chain and
// render target pool.  Returns NULL on failure.
void *MacMetalVideoCaptureCreate(void *metalSharedData, const MacVideoCaptureGeometry &geometry);
void MacMetalVideoCaptureDestroy(void *capture);

bool MacMetalVideoCaptureIsValid(void *capture);

// Renders one frame into a freshly acquired IOSurface-backed CVPixelBuffer and
// returns it RETAINED (the caller owns a reference), or NULL when no frame
// could be produced (pool exhausted, invalid state).  A NULL result means
// "drop this frame" -- it must never be turned into a CPU-composited frame,
// because that would mix two different geometries in one recording.
void *MacMetalVideoCaptureRenderFrame(void *capture);

// Releases a pixel buffer obtained from MacMetalVideoCaptureRenderFrame().
void MacMetalVideoCaptureReleasePixelBuffer(void *pixelBuffer);

#endif // _MAC_METAL_VIDEO_CAPTURE_H_
