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

#ifndef _WGPU_PRESENT_WIN32_H_
#define _WGPU_PRESENT_WIN32_H_

#include <windows.h>

#include "../shared/wgpu_present.h"

#if defined(ENABLE_WGPU)

// Win32 half of the portable presentation layer: everything the shared core
// needs to know about an HWND lives here, and nothing else does.
class Win32PresentWindow : public WGPUPresentWindow
{
public:
	explicit Win32PresentWindow(HWND hwnd);

	bool matches(HWND hwnd) const;

	bool getClientSize(int *width, int *height) const;
	void *createSurface(void *instance);
	bool isAlive() const;

private:
	HWND _hwnd;
};

#endif // ENABLE_WGPU

#endif // _WGPU_PRESENT_WIN32_H_
