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
// The POSIX meson build compiles this file only with -Dwgpu=true (the default),
// which also links the Linux build of wgpu-native (libwgpu_native.so) and the
// Wayland client library. Both adapters are then available.
//
// The GTK frontend renders into the drawing area's GdkWindow, which is a native
// X11 window or wl_surface depending on the active GDK backend:
//   X11     : gdk_x11_window_get_xid() + GDK_DISPLAY_XDISPLAY()
//   Wayland : gdk_wayland_window_get_wl_surface() + gdk_wayland_display_get_wl_display()
// LinuxWaylandPresentWindow::setClientSize() has to be pushed by the frontend,
// since Wayland has no synchronous client-size query.
//
// On Wayland, GDK and wgpu cannot both attach buffers to the drawing area's
// wl_surface: GDK commits its Cairo frame there every frame, and the compositor
// rejects the competing commits with a protocol error. The adapter therefore
// creates a child wl_surface, attaches it as a wl_subsurface of GDK's surface,
// and presents into that instead. The subsurface is positioned over the drawing
// area inside GDK's toplevel and takes the same buffer scale as the parent.
//
// Those are frontend questions only. WGPUPresent itself does not change.

#include <X11/Xlib.h>

#include "../../shared/wgpu_present.h"

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

// The Wayland handles are kept as void* (wl_display*, wl_surface*,
// wl_subsurface*, ...) so that the frontend does not have to expose
// wayland-client.h through this header.
class LinuxWaylandPresentWindow : public WGPUPresentWindow
{
public:
	LinuxWaylandPresentWindow(void *display, void *surface, int x, int y);
	~LinuxWaylandPresentWindow();

	bool matches(void *display, void *surface) const;

	// Wayland reports the size asynchronously through xdg_surface.configure(),
	// so the frontend has to push it in. `scale` is the integer GDK buffer
	// scale; the subsurface uses it so that its physical buffer size matches
	// the parent's logical size.
	void setClientSize(int width, int height, int scale);

	// Position of the drawing area inside GDK's top-level wl_surface. GDK's
	// Wayland backend returns the toplevel surface for child windows, so the
	// subsurface has to be moved below the menu and toolbar.
	void setClientPosition(int x, int y);

	bool getClientSize(int *width, int *height) const;
	void *createSurface(void *instance);
	bool isAlive() const;

private:
	void *_display;
	void *_surface;       // GDK's drawing-area wl_surface
	void *_childSurface;  // wl_surface of the subsurface wgpu presents into
	void *_subsurface;    // wl_subsurface
	void *_registry;
	void *_queue;         // dedicated queue used only while binding globals
	void *_compositor;
	void *_subcompositor;
	int _compositorVersion;
	int _x;
	int _y;
	int _width;
	int _height;
	int _scale;
};

#endif // _WGPU_PRESENT_LINUX_H_
