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

#include "wgpu_present_win32.h"

#if defined(ENABLE_WGPU)

#include "webgpu.h"

Win32PresentWindow::Win32PresentWindow(HWND hwnd)
	: _hwnd(hwnd)
{
}

bool Win32PresentWindow::matches(HWND hwnd) const
{
	return (this->_hwnd == hwnd);
}

bool Win32PresentWindow::getClientSize(int *width, int *height) const
{
	RECT rc;

	if ( (this->_hwnd == NULL) || !GetClientRect(this->_hwnd, &rc) )
	{
		return false;
	}

	*width  = rc.right - rc.left;
	*height = rc.bottom - rc.top;

	return true;
}

void *Win32PresentWindow::createSurface(void *instance)
{
	WGPUSurfaceSourceWindowsHWND source = WGPU_SURFACE_SOURCE_WINDOWS_HWND_INIT;
	source.hinstance = GetModuleHandleW(NULL);
	source.hwnd = (void *)this->_hwnd;

	WGPUSurfaceDescriptor surfaceDesc = WGPU_SURFACE_DESCRIPTOR_INIT;
	surfaceDesc.nextInChain = &source.chain;

	return (void *)wgpuInstanceCreateSurface((WGPUInstance)instance, &surfaceDesc);
}

bool Win32PresentWindow::isAlive() const
{
	return (this->_hwnd != NULL) && (IsWindow(this->_hwnd) != FALSE);
}

#endif // ENABLE_WGPU
