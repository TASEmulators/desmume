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

// Linux adapters for the portable WebGPU presentation layer. This file is only
// compiled when the POSIX build enables the "wgpu" option, which also brings in
// wgpu-native and the Wayland client library. See wgpu_present_linux.h for the
// wiring.

#include "wgpu_present_linux.h"

#include <string.h>

#include <wayland-client.h>

#include "webgpu.h"

// ---------------------------------------------------------------------------
// X11
// ---------------------------------------------------------------------------

LinuxX11PresentWindow::LinuxX11PresentWindow(Display *display, unsigned long window)
	: _display(display)
	, _window(window)
{
}

LinuxX11PresentWindow::~LinuxX11PresentWindow()
{
	// The Display connection belongs to the frontend; do not close it here.
}

bool LinuxX11PresentWindow::matches(Display *display, unsigned long window) const
{
	return (this->_display == display) && (this->_window == window);
}

bool LinuxX11PresentWindow::getClientSize(int *width, int *height) const
{
	if ( (this->_display == NULL) || (this->_window == 0) )
	{
		return false;
	}

	XWindowAttributes attributes;

	if (XGetWindowAttributes(this->_display, (Window)this->_window, &attributes) == 0)
	{
		return false;
	}

	*width  = attributes.width;
	*height = attributes.height;

	return true;
}

void *LinuxX11PresentWindow::createSurface(void *instance)
{
	WGPUSurfaceSourceXlibWindow source = WGPU_SURFACE_SOURCE_XLIB_WINDOW_INIT;
	source.display = (void *)this->_display;
	source.window = (uint64_t)this->_window;

	WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
	surfaceDesc.nextInChain = &source.chain;

	return (void *)wgpuInstanceCreateSurface((WGPUInstance)instance, &surfaceDesc);
}

bool LinuxX11PresentWindow::isAlive() const
{
	// TODO: hook XSetIOErrorHandler() to notice a dropped connection; checking
	// the handles is enough for a window that is only destroyed by the app.
	return (this->_display != NULL) && (this->_window != 0);
}

// ---------------------------------------------------------------------------
// Wayland
// ---------------------------------------------------------------------------
//
// GDK renders the drawing area through its own wl_surface and commits a Cairo
// frame to it on every redraw. wgpu cannot attach its swap chain to that same
// surface without racing those commits, which the compositor reports as a
// protocol error. Present into a child wl_surface instead, attached as a
// wl_subsurface of GDK's surface, so each side owns its own surface.

namespace
{

struct WaylandGlobals
{
	struct wl_compositor *compositor;
	struct wl_subcompositor *subcompositor;
	int compositorVersion;
};

void HandleRegistryGlobal(void *data, struct wl_registry *registry, uint32_t name,
						  const char *interface, uint32_t version)
{
	WaylandGlobals *globals = static_cast<WaylandGlobals *>(data);

	if (strcmp(interface, wl_compositor_interface.name) == 0)
	{
		const uint32_t bindVersion = (version < 4) ? version : 4;
		globals->compositor = static_cast<struct wl_compositor *>(
			wl_registry_bind(registry, name, &wl_compositor_interface, bindVersion));
		globals->compositorVersion = (int)bindVersion;
	}
	else if (strcmp(interface, wl_subcompositor_interface.name) == 0)
	{
		globals->subcompositor = static_cast<struct wl_subcompositor *>(
			wl_registry_bind(registry, name, &wl_subcompositor_interface, 1));
	}
}

void HandleRegistryGlobalRemove(void *, struct wl_registry *, uint32_t)
{
}

const struct wl_registry_listener kRegistryListener = {
	HandleRegistryGlobal,
	HandleRegistryGlobalRemove,
};

} // namespace

LinuxWaylandPresentWindow::LinuxWaylandPresentWindow(void *display, void *surface, int x, int y)
	: _display(display)
	, _surface(surface)
	, _childSurface(NULL)
	, _subsurface(NULL)
	, _registry(NULL)
	, _queue(NULL)
	, _compositor(NULL)
	, _subcompositor(NULL)
	, _compositorVersion(0)
	, _x(x)
	, _y(y)
	, _width(0)
	, _height(0)
	, _scale(1)
{
	if ( (this->_display == NULL) || (this->_surface == NULL) )
	{
		return;
	}

	struct wl_display *wlDisplay = static_cast<struct wl_display *>(this->_display);

	// Bind the globals on a private queue: wl_display_roundtrip() on the
	// default queue would dispatch GDK's own events from inside a GTK
	// callback.
	this->_queue = wl_display_create_queue(wlDisplay);
	if (this->_queue == NULL)
	{
		return;
	}

	this->_registry = wl_display_get_registry(wlDisplay);
	if (this->_registry == NULL)
	{
		return;
	}

	wl_proxy_set_queue(static_cast<struct wl_proxy *>(this->_registry),
					   static_cast<struct wl_event_queue *>(this->_queue));

	WaylandGlobals globals = { NULL, NULL, 0 };
	wl_registry_add_listener(static_cast<struct wl_registry *>(this->_registry),
							 &kRegistryListener, &globals);

	const int roundtripResult = wl_display_roundtrip_queue(wlDisplay,
		static_cast<struct wl_event_queue *>(this->_queue));

	this->_compositor = globals.compositor;
	this->_subcompositor = globals.subcompositor;
	this->_compositorVersion = globals.compositorVersion;

	if ( (roundtripResult < 0) || (this->_compositor == NULL) || (this->_subcompositor == NULL) )
	{
		return;
	}

	// Neither global has events we need after binding; keep them on the
	// default queue so the private one can be torn down now.
	wl_proxy_set_queue(static_cast<struct wl_proxy *>(this->_compositor), NULL);
	wl_proxy_set_queue(static_cast<struct wl_proxy *>(this->_subcompositor), NULL);

	wl_registry_destroy(static_cast<struct wl_registry *>(this->_registry));
	this->_registry = NULL;
	wl_event_queue_destroy(static_cast<struct wl_event_queue *>(this->_queue));
	this->_queue = NULL;

	this->_childSurface = wl_compositor_create_surface(
		static_cast<struct wl_compositor *>(this->_compositor));
	if (this->_childSurface == NULL)
	{
		return;
	}

	// A wl_surface's input region defaults to infinite, so the overlay would
	// swallow pointer and touch events before they reach GDK's toplevel. Give
	// it an empty input region to make it click-through; the events then go to
	// the drawing area as usual.
	struct wl_region *emptyRegion = wl_compositor_create_region(
		static_cast<struct wl_compositor *>(this->_compositor));
	if (emptyRegion != NULL)
	{
		wl_surface_set_input_region(static_cast<struct wl_surface *>(this->_childSurface), emptyRegion);
		wl_region_destroy(emptyRegion);
	}

	this->_subsurface = wl_subcompositor_get_subsurface(
		static_cast<struct wl_subcompositor *>(this->_subcompositor),
		static_cast<struct wl_surface *>(this->_childSurface),
		static_cast<struct wl_surface *>(this->_surface));

	if (this->_subsurface == NULL)
	{
		wl_surface_destroy(static_cast<struct wl_surface *>(this->_childSurface));
		this->_childSurface = NULL;
		return;
	}

	// Present independently of GDK's frame commits, and sit exactly over the
	// drawing area. The buffer scale is applied by setClientSize().
	wl_subsurface_set_desync(static_cast<struct wl_subsurface *>(this->_subsurface));
	wl_subsurface_set_position(static_cast<struct wl_subsurface *>(this->_subsurface), this->_x, this->_y);

	if (this->_compositorVersion >= 3)
	{
		wl_surface_set_buffer_scale(static_cast<struct wl_surface *>(this->_childSurface), this->_scale);
	}

	wl_surface_commit(static_cast<struct wl_surface *>(this->_childSurface));
}

LinuxWaylandPresentWindow::~LinuxWaylandPresentWindow()
{
	if (this->_subsurface != NULL)
	{
		wl_subsurface_destroy(static_cast<struct wl_subsurface *>(this->_subsurface));
	}
	if (this->_childSurface != NULL)
	{
		wl_surface_destroy(static_cast<struct wl_surface *>(this->_childSurface));
	}
	if (this->_subcompositor != NULL)
	{
		wl_subcompositor_destroy(static_cast<struct wl_subcompositor *>(this->_subcompositor));
	}
	if (this->_compositor != NULL)
	{
		wl_compositor_destroy(static_cast<struct wl_compositor *>(this->_compositor));
	}
	if (this->_registry != NULL)
	{
		wl_registry_destroy(static_cast<struct wl_registry *>(this->_registry));
	}
	if (this->_queue != NULL)
	{
		wl_event_queue_destroy(static_cast<struct wl_event_queue *>(this->_queue));
	}
}

bool LinuxWaylandPresentWindow::matches(void *display, void *surface) const
{
	return (this->_display == display) && (this->_surface == surface);
}

void LinuxWaylandPresentWindow::setClientSize(int width, int height, int scale)
{
	this->_width  = width;
	this->_height = height;

	const int newScale = (scale >= 1) ? scale : 1;
	if ( (this->_childSurface != NULL) && (newScale != this->_scale) )
	{
		this->_scale = newScale;
		if (this->_compositorVersion >= 3)
		{
			// Leave the commit to wgpu's next frame: it owns the surface's
			// presentation thread.
			wl_surface_set_buffer_scale(static_cast<struct wl_surface *>(this->_childSurface), this->_scale);
		}
	}
}

void LinuxWaylandPresentWindow::setClientPosition(int x, int y)
{
	if ( (this->_x == x) && (this->_y == y) )
	{
		return;
	}

	this->_x = x;
	this->_y = y;

	if (this->_subsurface != NULL)
	{
		// wgpu's next present commits the new position together with its
		// buffer, so there is no need to commit from the GTK thread.
		wl_subsurface_set_position(static_cast<struct wl_subsurface *>(this->_subsurface), this->_x, this->_y);
	}
}

bool LinuxWaylandPresentWindow::getClientSize(int *width, int *height) const
{
	if ( (this->_width <= 0) || (this->_height <= 0) )
	{
		return false;
	}

	*width  = this->_width;
	*height = this->_height;

	return true;
}

void *LinuxWaylandPresentWindow::createSurface(void *instance)
{
	if (this->_childSurface == NULL)
	{
		return NULL;
	}

	WGPUSurfaceSourceWaylandSurface source = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
	source.display = this->_display;
	source.surface = this->_childSurface;

	WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
	surfaceDesc.nextInChain = &source.chain;

	return (void *)wgpuInstanceCreateSurface((WGPUInstance)instance, &surfaceDesc);
}

bool LinuxWaylandPresentWindow::isAlive() const
{
	return (this->_display != NULL) && (this->_surface != NULL) && (this->_childSurface != NULL);
}
