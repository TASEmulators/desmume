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

#if defined(ENABLE_WGPU)

#include "wgpu_present_linux.h"
#include "wgpu_upscaler_linux.h"

WGPULinuxDisplay::WGPULinuxDisplay()
{
	this->active = false;
	this->_window = NULL;
	this->_wayland = false;
	this->_instanceBackends = 0;
	this->_useOutputFilterPending = false;
	this->_upscalerPending = WGPUPresentUpscaler_Off;
}

WGPULinuxDisplay::~WGPULinuxDisplay()
{
	this->kill();
}

bool WGPULinuxDisplay::beginX11(void *display, unsigned long window, uint32_t instanceBackends)
{
	if ( (this->_window != NULL) && !this->_wayland &&
		 static_cast<LinuxX11PresentWindow *>(this->_window)->matches((Display *)display, window) &&
		 (this->_instanceBackends == instanceBackends) &&
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
	this->_instanceBackends = instanceBackends;

	if (!this->_present.begin(this->_window, instanceBackends))
	{
		delete this->_window;
		this->_window = NULL;
		this->active = false;
		return false;
	}

	// The GPU filters are WGSL carried in the GResource bundle; handing the
	// source over here keeps the present layer free of GTK and GIO. Nothing is
	// compiled yet: the chain of the selected filter is built the first time a
	// frame is presented with it selected.
	this->_present.setUpscalerShaderSource(WGPUCreateLinuxUpscalerShaderSource());
	this->_present.setUpscaler(this->_upscalerPending);

	this->active = true;

	return true;
}

bool WGPULinuxDisplay::beginWayland(void *display, void *surface, int x, int y, uint32_t instanceBackends)
{
	if ( (this->_window != NULL) && this->_wayland &&
		 static_cast<LinuxWaylandPresentWindow *>(this->_window)->matches(display, surface) &&
		 (this->_instanceBackends == instanceBackends) &&
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

	LinuxWaylandPresentWindow *window = new LinuxWaylandPresentWindow(display, surface, x, y);
	this->_window = window;
	this->_wayland = true;
	this->_instanceBackends = instanceBackends;

	if (!this->_present.begin(window, instanceBackends))
	{
		delete this->_window;
		this->_window = NULL;
		this->active = false;
		return false;
	}

	// See beginX11(): source first, then the chain that is pending.
	this->_present.setUpscalerShaderSource(WGPUCreateLinuxUpscalerShaderSource());
	this->_present.setUpscaler(this->_upscalerPending);

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

	this->_instanceBackends = 0;
}

void WGPULinuxDisplay::setClientSize(int width, int height, int scale)
{
	if (this->_window == NULL)
	{
		return;
	}

	if (this->_wayland)
	{
		static_cast<LinuxWaylandPresentWindow *>(this->_window)->setClientSize(width, height, scale);
	}

	// X11 can query the size itself, so there is nothing to push there.
}

void WGPULinuxDisplay::setClientPosition(int x, int y)
{
	if ( (this->_window == NULL) || !this->_wayland )
	{
		return;
	}

	static_cast<LinuxWaylandPresentWindow *>(this->_window)->setClientPosition(x, y);
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

void WGPULinuxDisplay::setUpscaler(int upscaler)
{
	this->_upscalerPending = upscaler;
	this->_present.setUpscaler(upscaler);
}

int WGPULinuxDisplay::upscaler()
{
	return this->_upscalerPending;
}

bool WGPULinuxDisplay::upscalerCompiled()
{
	return this->_present.upscalerCompiled();
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
	this->_instanceBackends = 0;
	this->_useOutputFilterPending = false;
	this->_upscalerPending = WGPUPresentUpscaler_Off;
}

WGPULinuxDisplay::~WGPULinuxDisplay() {}

bool WGPULinuxDisplay::beginX11(void *, unsigned long, uint32_t) { return false; }
bool WGPULinuxDisplay::beginWayland(void *, void *, int, int, uint32_t) { return false; }
void WGPULinuxDisplay::kill() {}
void WGPULinuxDisplay::setClientSize(int, int, int) {}
void WGPULinuxDisplay::setClientPosition(int, int) {}
void WGPULinuxDisplay::setvsync(bool) {}
void WGPULinuxDisplay::setUseOutputFilter(bool theState) { this->_useOutputFilterPending = theState; }
bool WGPULinuxDisplay::useOutputFilter() { return this->_useOutputFilterPending; }
void WGPULinuxDisplay::setUpscaler(int upscaler) { this->_upscalerPending = upscaler; }
int WGPULinuxDisplay::upscaler() { return this->_upscalerPending; }
bool WGPULinuxDisplay::upscalerCompiled() { return false; }

bool WGPULinuxDisplay::present(const u32*, int, int, const WGPUPresentRect*,
							   const WGPUPresentRect*, const WGPUPresentRect*, int, u32)
{
	return false;
}

#endif
