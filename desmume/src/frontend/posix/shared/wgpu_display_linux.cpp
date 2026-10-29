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

#include "wgpu_display_linux.h"

#if defined(ENABLE_WGPU) && defined(WGPU_PRESENT_LINUX)

#include "wgpu_present_linux.h"

WGPULinuxDisplay::WGPULinuxDisplay()
{
	this->active = false;
	this->_window = NULL;
	this->_wayland = false;
	this->_useOutputFilterPending = false;
}

WGPULinuxDisplay::~WGPULinuxDisplay()
{
	this->kill();
}

bool WGPULinuxDisplay::beginX11(void *display, unsigned long window)
{
#if defined(WGPU_PRESENT_X11)
	if ( (this->_window != NULL) && !this->_wayland &&
		 static_cast<LinuxX11PresentWindow *>(this->_window)->matches((Display *)display, window) &&
		 this->_present.isReady() )
	{
		this->active = true;
		return true;
	}

	this->kill();

	if ( (display == NULL) || (window == 0) )
	{
		return false;
	}

	this->_window = new LinuxX11PresentWindow((Display *)display, window);
	this->_wayland = false;
#else
	(void)display;
	(void)window;
	return false;
#endif

	if (!this->_present.begin(this->_window))
	{
		delete this->_window;
		this->_window = NULL;
		this->active = false;
		return false;
	}

	this->active = true;

	return true;
}

bool WGPULinuxDisplay::beginWayland(void *display, void *surface)
{
	if ( (this->_window != NULL) && this->_wayland &&
		 static_cast<LinuxWaylandPresentWindow *>(this->_window)->matches(display, surface) &&
		 this->_present.isReady() )
	{
		this->active = true;
		return true;
	}

	this->kill();

	if ( (display == NULL) || (surface == NULL) )
	{
		return false;
	}

	LinuxWaylandPresentWindow *window = new LinuxWaylandPresentWindow(display, surface);
	this->_window = window;
	this->_wayland = true;

	if (!this->_present.begin(window))
	{
		delete this->_window;
		this->_window = NULL;
		this->active = false;
		return false;
	}

	this->active = true;

	return true;
}

void WGPULinuxDisplay::kill()
{
	this->active = false;

	this->_present.kill();

	if (this->_window != NULL)
	{
		delete this->_window;
		this->_window = NULL;
	}
}

void WGPULinuxDisplay::setClientSize(int width, int height)
{
	if (this->_window == NULL)
	{
		return;
	}

	if (this->_wayland)
	{
		static_cast<LinuxWaylandPresentWindow *>(this->_window)->setClientSize(width, height);
	}

	// X11 can query the size itself, so there is nothing to push there.
}

void WGPULinuxDisplay::setvsync(bool vsync)
{
	this->_present.setvsync(vsync);
}

void WGPULinuxDisplay::setUseOutputFilter(bool theState)
{
	this->_useOutputFilterPending = theState;
	this->_present.setUseOutputFilter(theState);
}

bool WGPULinuxDisplay::useOutputFilter()
{
	return this->_useOutputFilterPending;
}

bool WGPULinuxDisplay::present(const u32 *pixels, int width, int height,
							   const WGPUPresentRect *srcRects, const WGPUPresentRect *dstRects,
							   const WGPUPresentRect *gapRect, int rotation, u32 gapColor)
{
	if ( (this->_window == NULL) || !this->_present.isReady() )
	{
		return false;
	}

	return this->_present.present(pixels, width, height, srcRects, dstRects, gapRect, rotation, (uint32_t)gapColor);
}

#else

// WebGPU is optional. Keep the class linkable on configurations without
// wgpu-native so the GTK frontend does not need its own #ifdefs everywhere.

WGPULinuxDisplay::WGPULinuxDisplay()
{
	this->active = false;
	this->_window = NULL;
	this->_wayland = false;
	this->_useOutputFilterPending = false;
}

WGPULinuxDisplay::~WGPULinuxDisplay() {}

bool WGPULinuxDisplay::beginX11(void *, unsigned long) { return false; }
bool WGPULinuxDisplay::beginWayland(void *, void *) { return false; }
void WGPULinuxDisplay::kill() {}
void WGPULinuxDisplay::setClientSize(int, int) {}
void WGPULinuxDisplay::setvsync(bool) {}
void WGPULinuxDisplay::setUseOutputFilter(bool theState) { this->_useOutputFilterPending = theState; }
bool WGPULinuxDisplay::useOutputFilter() { return this->_useOutputFilterPending; }

bool WGPULinuxDisplay::present(const u32*, int, int, const WGPUPresentRect*,
							   const WGPUPresentRect*, const WGPUPresentRect*, int, u32)
{
	return false;
}

#endif
