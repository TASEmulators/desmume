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

#ifndef _WGPU_DISPLAY_LINUX_H_
#define _WGPU_DISPLAY_LINUX_H_

#include "types.h"

#include "../../shared/wgpu_present.h"

class WGPUPresentWindow;

// WebGPU presentation layer for the POSIX/GTK frontend.
//
// This is the Linux counterpart of the Windows WGPUDISPLAY: WGPUPresent does all
// of the WebGPU work and knows nothing about X11, Wayland or GTK, while this
// class only owns the native-window adapter and forwards the frame.
//
// The native handles cross the boundary as void* so this header does not drag
// Xlib or wayland-client into the frontend. The Linux adapters are always
// compiled; beginX11()/beginWayland() still return false when wgpu-native is
// unavailable or the surface cannot be created, and the caller falls back to
// Cairo.
class WGPULinuxDisplay
{
public:
	WGPULinuxDisplay();
	~WGPULinuxDisplay();

	// Creates the device, surface and pipeline on first use for this window.
	// `display` is a Display* and `window` an X11 Window; the handle pair is
	// remembered so that presenting to the same window is free.
	bool beginX11(void *display, unsigned long window);

	// Wayland counterpart. `display` is a wl_display* and `surface` GDK's
	// drawing-area wl_surface*; the adapter presents through a child
	// wl_subsurface. `x`/`y` are the drawing area's position inside GDK's
	// toplevel surface. The size has to be pushed through setClientSize().
	bool beginWayland(void *display, void *surface, int x, int y);

	// Releases the swap chain. Called when the widget is unrealized or another
	// presentation path takes over.
	void kill();

	// Wayland only: GTK knows the new size from its configure event. The size
	// is in physical pixels; `scale` is GDK's integer buffer scale.
	void setClientSize(int width, int height, int scale);

	// Wayland only: position of the drawing area inside the toplevel surface.
	void setClientPosition(int x, int y);

	void setvsync(bool vsync);
	void setUseOutputFilter(bool theState);
	bool useOutputFilter();

	// The GPU filter the frame goes through, one of WGPUPresentUpscaler. Its
	// WGSL is compiled the first time a frame is presented with it selected,
	// and only for the filter that is selected.
	void setUpscaler(int upscaler);
	int upscaler();
	bool upscalerCompiled();

	// Presents one already composited BGRA frame. The rectangles use the same
	// conventions as WGPUPresent::present().
	bool present(const u32* pixels, int width, int height,
				 const WGPUPresentRect* srcRects, const WGPUPresentRect* dstRects,
				 const WGPUPresentRect* gapRect, int rotation, u32 gapColor);

	bool active;

private:
	WGPUPresent _present;
	WGPUPresentWindow* _window;

	bool _wayland;
	bool _useOutputFilterPending;
	int _upscalerPending;

	WGPULinuxDisplay(const WGPULinuxDisplay&);
	WGPULinuxDisplay& operator=(const WGPULinuxDisplay&);
};

#endif // _WGPU_DISPLAY_LINUX_H_
