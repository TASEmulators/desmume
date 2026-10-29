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

#ifndef _WGPU_PRESENT_H_
#define _WGPU_PRESENT_H_

#include <stdint.h>

// ---------------------------------------------------------------------------
// Platform-independent WebGPU presentation layer.
//
// The emulation core composites the DS frame into CPU memory, so presenting it
// only requires a device, a surface and a single textured-quad pipeline. This
// file owns all of that in a way that knows nothing about the host window
// system: the window is reached exclusively through WGPUPresentWindow.
//
// Nothing here includes <windows.h>, webgpu.h or any frontend header, so the
// same translation unit can be added to the Linux or macOS build as-is.
// ---------------------------------------------------------------------------

struct WGPUPresentRect
{
	int left;
	int top;
	int right;
	int bottom;
};

// The only part a platform has to implement. Keep adapters dumb: they translate
// between the native window and WebGPU, nothing else.
class WGPUPresentWindow
{
public:
	virtual ~WGPUPresentWindow() {}

	// Client area size in physical pixels, excluding any window chrome. Called
	// once per frame so window resizes need no extra plumbing.
	virtual bool getClientSize(int *width, int *height) const = 0;

	// Wraps this window into a WGPUSurface.
	//
	// Both the argument and the return value are WGPUInstance / WGPUSurface
	// carried as void* so that webgpu.h never leaks into frontend headers; the
	// adapter reinterpret_casts them back to their real types.
	virtual void *createSurface(void *instance) = 0;

	// Return false once the native window is gone, so the core can tear the
	// surface down instead of drawing into it (dropped X11 connection, ...).
	virtual bool isAlive() const { return true; }
};

class WGPUPresent
{
public:
	WGPUPresent();
	~WGPUPresent();

	// Creates the instance, device, surface, pipeline and buffers. `window` is
	// not owned and must outlive every present() call. Returns false when
	// WebGPU is unavailable (build without ENABLE_WGPU, no adapter, surface
	// creation failure, ...), in which case the caller should fall back.
	//
	// `instanceBackends` pins the instance to a set of WGPUInstanceBackend bits,
	// so a frontend can offer one present path per backend ("WebGPU - Vulkan"
	// next to "WebGPU - D3D12"). Zero lets wgpu-native pick. Calling begin()
	// again with a different mask rebuilds everything.
	bool begin(WGPUPresentWindow *window, uint32_t instanceBackends = 0);

	// Releases everything, including the surface. Safe to call at any time and
	// repeatedly; begin() starts from a clean slate afterwards.
	void kill();

	void setvsync(bool vsync);
	void setUseOutputFilter(bool theState);
	bool useOutputFilter() const;

	// Presents one frame.
	//   pixels   : BGRA8888, top-down, tightly packed, width * height * 4 bytes
	//   srcRects : source rectangles inside `pixels` (main and sub screen)
	//   dstRects : destination rectangles, already in client coordinates
	//   gapRect  : screen gap rectangle in client coordinates (may be empty)
	//   rotation : 0/90/180/270
	//   gapColor : 0x00BBGGRR, i.e. red in the low byte
	// Returns false when the frame was dropped (surface lost or outdated).
	bool present(const uint32_t *pixels, int width, int height,
				 const WGPUPresentRect *srcRects,
				 const WGPUPresentRect *dstRects,
				 const WGPUPresentRect *gapRect,
				 int rotation, uint32_t gapColor);

	bool isReady() const;

private:
	struct Impl;
	Impl *_impl;

	WGPUPresent(const WGPUPresent &);
	WGPUPresent &operator=(const WGPUPresent &);
};

// ---------------------------------------------------------------------------
// Porting notes
//
// Adding a platform means adding one adapter and nothing else:
//
//   1. Subclass WGPUPresentWindow and implement getClientSize()/createSurface()
//      (plus isAlive() if the window can outlive the connection).
//   2. createSurface() builds the matching WGPUSurfaceSource* struct and calls
//      wgpuInstanceCreateSurface():
//        Windows : WGPUSurfaceSourceWindowsHWND  (hinstance + hwnd)
//        X11     : WGPUSurfaceSourceXlibWindow   (Display* + Window)
//        XCB     : WGPUSurfaceSourceXCBWindow    (xcb_connection_t* + xcb_window_t)
//        Wayland : WGPUSurfaceSourceWaylandSurface (wl_display* + wl_surface*)
//        macOS   : WGPUSurfaceSourceMetalLayer   (CAMetalLayer*)
//      wgpu-native has to be built with the matching feature enabled (x11,
//      wayland and metal are separate build features of wgpu-native).
//   3. Build this file and the adapter, and link wgpu_native.
//   4. Feed the frontend's screen rectangles in, as WGPUPresentRect, plus the
//      already composited BGRA frame.
//
// The Linux X11 and Wayland adapters live in
// src/frontend/posix/shared/wgpu_present_linux.cpp, with WGPULinuxDisplay in
// wgpu_display_linux.cpp. The POSIX meson build compiles them automatically
// when a Linux wgpu-native is present (see the wgpu-native-path option).
// ---------------------------------------------------------------------------

#endif // _WGPU_PRESENT_H_
