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
// WGPU_PRESENT_LINUX turns these on. The POSIX meson build defines it whenever
// wgpu-native is found, and WGPU_PRESENT_X11 as well when X11 is available.
// Link a Linux build of wgpu-native (libwgpu_native.so) that was built with the
// x11 and/or wayland feature.
//
// The GTK frontend renders into the drawing area's GdkWindow, which is a native
// X11 window or wl_surface depending on the active GDK backend:
//   X11     : gdk_x11_window_get_xid() + GDK_DISPLAY_XDISPLAY()
//   Wayland : gdk_wayland_window_get_wl_surface() + gdk_wayland_display_get_wl_display()
// LinuxWaylandPresentWindow::setClientSize() has to be pushed by the frontend,
// since Wayland has no synchronous client-size query.
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

	bool matches(Display *display, unsigned long window) const;

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

	bool matches(void *display, void *surface) const;

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
