/*
Copyright (C) 2018 DeSmuME team

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

#include "ogl_display.h"

bool GLDISPLAY::initialize(HWND hwnd)
{
	//do we need to use another HDC?
	if (hwnd == this->hwnd) return true;
	else if (this->hwnd) return false;

	if (initContext(hwnd, &privateContext))
	{
		this->hwnd = hwnd;
		return true;
	}
	else
		return false;
}
//http://stackoverflow.com/questions/589064/how-to-enable-vertical-sync-in-opengl
bool GLDISPLAY::WGLExtensionSupported(const char *extension_name)
{
	// this is pointer to function which returns pointer to string with list of all wgl extensions
	PFNWGLGETEXTENSIONSSTRINGEXTPROC _wglGetExtensionsStringEXT = NULL;

	// determine pointer to wglGetExtensionsStringEXT function
	_wglGetExtensionsStringEXT = (PFNWGLGETEXTENSIONSSTRINGEXTPROC)wglGetProcAddress("wglGetExtensionsStringEXT");

	if (strstr(_wglGetExtensionsStringEXT(), extension_name) == NULL)
	{
		// string was not found
		return false;
	}

	// extension is supported
	return true;
}
void GLDISPLAY::_setvsync()
{
	//even if it doesn't work, we'll track it
	haveVsync = wantVsync;

	if (!WGLExtensionSupported("WGL_EXT_swap_control")) return;

	//http://stackoverflow.com/questions/589064/how-to-enable-vertical-sync-in-opengl
	PFNWGLSWAPINTERVALEXTPROC       wglSwapIntervalEXT = NULL;
	PFNWGLGETSWAPINTERVALEXTPROC    wglGetSwapIntervalEXT = NULL;
	{
		// Extension is supported, init pointers.
		wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

		// this is another function from WGL_EXT_swap_control extension
		wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");
	}

	wglSwapIntervalEXT(wantVsync ? 1 : 0);
}


GLDISPLAY::GLDISPLAY()
	: active(false)
	, wantVsync(false)
	, haveVsync(false)
{
}

void GLDISPLAY::kill()
{
	if (!hwnd) return;
	wglDeleteContext(privateContext);
	privateContext = NULL;
	hwnd = NULL;
	haveVsync = false;
}

bool GLDISPLAY::begin(HWND hwnd)
{
	DWORD myThread = GetCurrentThreadId();

	//always use another context for display logic
	//1. if this is a single threaded process (3d rendering and display in the main thread) then alternating contexts is benign
	//2. if this is a multi threaded process (3d rendernig and display in other threads) then the display needs some context

	if (!this->hwnd)
	{
		if (!initialize(hwnd)) return false;
	}

	privateDC = GetDC(hwnd);
	wglMakeCurrent(privateDC, privateContext);

	//go ahead and sync the vsync setting while we have the context
	if (wantVsync != haveVsync)
		_setvsync();

	if (filter)
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	}
	else
	{
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	}


	return active = true;
}

void GLDISPLAY::end()
{
	wglMakeCurrent(NULL, privateContext);
	ReleaseDC(hwnd, privateDC);
	privateDC = NULL;
	active = false;
}

void GLDISPLAY::setvsync(bool vsync)
{
	wantVsync = vsync;
}

void GLDISPLAY::showPage()
{
	SwapBuffers(privateDC);
}

