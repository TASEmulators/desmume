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

#ifndef _WGPU_DISPLAY_H_
#define _WGPU_DISPLAY_H_

#include "types.h"

#include <windows.h>

#include "../shared/wgpu_present.h"

class Win32PresentWindow;

// WebGPU presentation layer for the Windows frontend.
//
// This is the third display method alongside DirectDraw and OpenGL. The work is
// split in two: WGPUPresent (src/frontend/shared) does all of the WebGPU work
// and knows nothing about Windows, while this class only translates Win32 types
// into the portable ones and owns the Win32PresentWindow adapter. Porting the
// display method to another frontend therefore means writing a new adapter and
// a small class like this one, not touching the renderer.
//
// On builds without ENABLE_WGPU everything is a stub and begin() returns false.
class WGPUDISPLAY
{
public:
	WGPUDISPLAY();
	~WGPUDISPLAY();

	// Creates the device, surface and pipeline on first use for this window.
	// `instanceBackends` pins wgpu-native to one backend (WGPUInstanceBackend
	// bits) so that each backend can appear as its own display method; zero lets
	// wgpu-native pick. Passing a different mask rebuilds everything.
	// Returns false when WebGPU is unavailable (build without ENABLE_WGPU, no
	// adapter, surface creation failure, ...).
	bool begin(HWND hwnd, u32 instanceBackends = 0);

	// Releases the swap chain (called when another display method takes over).
	void kill();

	void setvsync(bool vsync);
	void setUseOutputFilter(bool theState);
	bool useOutputFilter();

	// Presents one frame.
	//   pixels     : BGRA8888, top-down, width * height * 4 bytes
	//   srcRects   : source rectangles inside pixels (main/sub screen)
	//   dstRects   : destination rectangles, already in client coordinates
	//   gapRect    : screen gap rectangle in client coordinates (may be empty)
	//   rotation   : 0/90/180/270
	//   gapColor   : Windows COLORREF (0x00BBGGRR)
	// Returns false when the frame was dropped (surface lost/outdated).
	bool present(const u32* pixels, int width, int height,
				 const RECT* srcRects, const RECT* dstRects,
				 const RECT* gapRect, int rotation, u32 gapColor);

	bool active;

private:
	WGPUPresent _present;
	Win32PresentWindow* _window;

	// Backend the current surface was created for, as WGPUInstanceBackend bits.
	u32 _instanceBackends;

	bool _useOutputFilterPending;
};

#endif // _WGPU_DISPLAY_H_
