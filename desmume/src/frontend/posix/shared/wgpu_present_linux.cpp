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

// Untested skeleton: compiled only when WGPU_PRESENT_LINUX is defined, which the
// POSIX build does not do yet. See wgpu_present_linux.h for the wiring steps.

#include "wgpu_present_linux.h"

#if defined(WGPU_PRESENT_LINUX)

#include "webgpu.h"

#if defined(WGPU_PRESENT_X11)

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

#endif // WGPU_PRESENT_X11

// ---------------------------------------------------------------------------
// Wayland
// ---------------------------------------------------------------------------

LinuxWaylandPresentWindow::LinuxWaylandPresentWindow(void *display, void *surface)
	: _display(display)
	, _surface(surface)
	, _width(0)
	, _height(0)
{
}

void LinuxWaylandPresentWindow::setClientSize(int width, int height)
{
	this->_width  = width;
	this->_height = height;
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
	WGPUSurfaceSourceWaylandSurface source = WGPU_SURFACE_SOURCE_WAYLAND_SURFACE_INIT;
	source.display = this->_display;
	source.surface = this->_surface;

	WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
	surfaceDesc.nextInChain = &source.chain;

	return (void *)wgpuInstanceCreateSurface((WGPUInstance)instance, &surfaceDesc);
}

bool LinuxWaylandPresentWindow::isAlive() const
{
	return (this->_display != NULL) && (this->_surface != NULL);
}

#endif // WGPU_PRESENT_LINUX
