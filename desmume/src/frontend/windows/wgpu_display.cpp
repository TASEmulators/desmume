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

#include "wgpu_display.h"

#if defined(ENABLE_WGPU)

#include "wgpu_present_win32.h"

static inline WGPUPresentRect ToPresentRect(const RECT *rc)
{
	WGPUPresentRect out;
	out.left   = (int)rc->left;
	out.top    = (int)rc->top;
	out.right  = (int)rc->right;
	out.bottom = (int)rc->bottom;
	return out;
}

WGPUDISPLAY::WGPUDISPLAY()
{
	active = false;
	_window = NULL;
	_instanceBackends = 0;
	_useOutputFilterPending = false;
}

WGPUDISPLAY::~WGPUDISPLAY()
{
	kill();
}

bool WGPUDISPLAY::begin(HWND hwnd, u32 instanceBackends)
{
	if ( (_window != NULL) && _window->matches(hwnd) && _present.isReady() &&
		 (_instanceBackends == instanceBackends) )
	{
		active = true;
		return true;
	}

	// A different window, a different backend (or a previous failure) means the
	// surface has to be rebuilt from scratch.
	kill();

	if (hwnd == NULL)
	{
		return false;
	}

	_window = new Win32PresentWindow(hwnd);

	if (!_present.begin(_window, (uint32_t)instanceBackends))
	{
		delete _window;
		_window = NULL;
		active = false;
		return false;
	}

	_instanceBackends = instanceBackends;
	active = true;

	return true;
}

void WGPUDISPLAY::kill()
{
	active = false;

	_present.kill();

	if (_window != NULL)
	{
		delete _window;
		_window = NULL;
	}

	_instanceBackends = 0;
}

void WGPUDISPLAY::setvsync(bool vsync)
{
	_present.setvsync(vsync);
}

void WGPUDISPLAY::setUseOutputFilter(bool theState)
{
	_useOutputFilterPending = theState;
	_present.setUseOutputFilter(theState);
}

bool WGPUDISPLAY::useOutputFilter()
{
	return _useOutputFilterPending;
}

bool WGPUDISPLAY::present(const u32 *pixels, int width, int height,
						  const RECT *srcRects, const RECT *dstRects,
						  const RECT *gapRect, int rotation, u32 gapColor)
{
	if ( (_window == NULL) || !_present.isReady() )
	{
		return false;
	}

	const WGPUPresentRect src[2] = { ToPresentRect(&srcRects[0]), ToPresentRect(&srcRects[1]) };
	const WGPUPresentRect dst[2] = { ToPresentRect(&dstRects[0]), ToPresentRect(&dstRects[1]) };

	WGPUPresentRect gap;
	const WGPUPresentRect *gapPtr = NULL;

	if (gapRect != NULL)
	{
		gap = ToPresentRect(gapRect);
		gapPtr = &gap;
	}

	return _present.present(pixels, width, height, src, dst, gapPtr, rotation, (uint32_t)gapColor);
}

#else

// WebGPU is only built where wgpu-native is available. Keep the class linkable
// on other configurations so the display method can still be selected; it just
// fails and the caller falls back to DirectDraw.

WGPUDISPLAY::WGPUDISPLAY()
{
	active = false;
	_window = NULL;
	_instanceBackends = 0;
	_useOutputFilterPending = false;
}

WGPUDISPLAY::~WGPUDISPLAY() {}

bool WGPUDISPLAY::begin(HWND, u32) { return false; }
void WGPUDISPLAY::kill() {}
void WGPUDISPLAY::setvsync(bool vsync) {}
void WGPUDISPLAY::setUseOutputFilter(bool theState) { _useOutputFilterPending = theState; }
bool WGPUDISPLAY::useOutputFilter() { return _useOutputFilterPending; }

bool WGPUDISPLAY::present(const u32*, int, int, const RECT*, const RECT*,
						  const RECT*, int, u32)
{
	return false;
}

#endif // ENABLE_WGPU
