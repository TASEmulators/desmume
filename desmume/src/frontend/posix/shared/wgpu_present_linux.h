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

#ifndef _WGPU_PRESENT_LINUX_H_
#define _WGPU_PRESENT_LINUX_H_

// Linux adapters for the portable WebGPU presentation layer.
//
// Not built yet: the POSIX frontend defines neither WGPU_PRESENT_LINUX nor
// WGPU_PRESENT_X11, so this header and its .cpp compile to nothing. They exist
// so that porting only needs build-system work plus the frontend integration
// described below, instead of another rewrite of the renderer.
//
// Wiring it up requires:
//   1. Define WGPU_PRESENT_LINUX (plus WGPU_PRESENT_X11 when X11 is available)
//      and compile this file together with src/frontend/shared/wgpu_present.cpp.
//   2. Link a Linux build of wgpu-native (libwgpu_native.so) that was built with
//      the x11 and/or wayland feature.
//   3. Decide how the GTK frontend hands a window to WebGPU. A GTK widget cannot
//      be used as a surface directly, so either render into the toplevel X11
//      window (gdk_x11_window_get_xid) or host a native child window. On Wayland
//      gdk_wayland_window_get_wl_surface provides the wl_surface.
//   4. Drive LinuxWaylandPresentWindow::setClientSize() from the configure event
//      handler, since Wayland has no synchronous client-size query.
//
// Those are frontend questions only. WGPUPresent itself does not change.

#if defined(WGPU_PRESENT_LINUX)

#include "../../shared/wgpu_present.h"

#if defined(WGPU_PRESENT_X11)

#include <X11/Xlib.h>

class LinuxX11PresentWindow : public WGPUPresentWindow
{
public:
	LinuxX11PresentWindow(Display *display, unsigned long window);
	~LinuxX11PresentWindow();

	bool getClientSize(int *width, int *height) const;
	void *createSurface(void *instance);
	bool isAlive() const;

private:
	Display *_display;
	unsigned long _window;
};

#endif // WGPU_PRESENT_X11

// The Wayland handles are kept as void* (wl_display* and wl_surface*) so that
// the frontend does not have to expose wayland-client.h through this header.
class LinuxWaylandPresentWindow : public WGPUPresentWindow
{
public:
	LinuxWaylandPresentWindow(void *display, void *surface);

	// Wayland reports the size asynchronously through xdg_surface.configure(),
	// so the frontend has to push it in.
	void setClientSize(int width, int height);

	bool getClientSize(int *width, int *height) const;
	void *createSurface(void *instance);
	bool isAlive() const;

private:
	void *_display;
	void *_surface;
	int _width;
	int _height;
};

#endif // WGPU_PRESENT_LINUX

#endif // _WGPU_PRESENT_LINUX_H_
