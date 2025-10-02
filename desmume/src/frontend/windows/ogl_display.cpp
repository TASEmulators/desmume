/*
Copyright (C) 2018-2025 DeSmuME team

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
#include "display.h"


PFNWGLSWAPINTERVALEXTPROC    wglSwapIntervalEXT    = NULL;
PFNWGLGETSWAPINTERVALEXTPROC wglGetSwapIntervalEXT = NULL;

bool GLDISPLAY::initialize(HWND hwnd)
{
	//do we need to use another HDC?
	if (hwnd == this->hwnd) return true;
	else if (this->hwnd) return false;

	if (initContext(hwnd, &privateContext))
	{
		this->hwnd = hwnd;
		privateDC = GetDC(hwnd);
		wglMakeCurrent(privateDC, privateContext);

		// http://stackoverflow.com/questions/589064/how-to-enable-vertical-sync-in-opengl
		this->_isVsyncSupported = WGLExtensionSupported("WGL_EXT_swap_control");

		if (this->_isVsyncSupported)
		{
			// Extension is supported, init pointers.
			wglSwapIntervalEXT = (PFNWGLSWAPINTERVALEXTPROC)wglGetProcAddress("wglSwapIntervalEXT");

			// this is another function from WGL_EXT_swap_control extension
			wglGetSwapIntervalEXT = (PFNWGLGETSWAPINTERVALEXTPROC)wglGetProcAddress("wglGetSwapIntervalEXT");

			// Certain video drivers may try to set the V-sync setting to whatever they want on
			// initialization, and so we can't assume that _useVsyncPending will match whatever the video
			// driver is doing.
			//
			// And so we need to force the V-sync to be whatever default value _useVsyncPending is in
			// order to ensure that the actual V-sync setting in OpenGL matches _useVsyncPending.
			wglSwapIntervalEXT((this->_useVsyncApplied) ? 1 : 0);
		}

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

GLDISPLAY::GLDISPLAY()
{
	_texVideo = (GLuint*)malloc(WIN32_FRAMEBUFFER_COUNT * sizeof(GLuint));
	_texHud   = (GLuint*)malloc(WIN32_FRAMEBUFFER_COUNT * sizeof(GLuint));

	for (size_t i = 0; i < WIN32_FRAMEBUFFER_COUNT; i++)
	{
		_texVideo[i] = 0;
		_texHud[i]   = 0;
	}

	active = false;
	_isVsyncSupported = false;

	_useOutputFilterPending = false;
	_useOutputFilterApplied = false;

	_useVsyncPending = false;
	_useVsyncApplied = false;
}

GLDISPLAY::~GLDISPLAY()
{
	free(this->_texVideo);
	this->_texVideo = NULL;

	free(this->_texHud);
	this->_texHud = NULL;
}

void GLDISPLAY::initVideoTexture(NDSColorFormat colorFormat, GLsizei videoWidth, GLsizei videoHeight)
{
	const GLint samplerType = (this->_useOutputFilterApplied) ? GL_LINEAR : GL_NEAREST;
	const size_t pixBytes = (colorFormat == NDSColorFormat_BGR555_Rev) ? sizeof(u16) : sizeof(u32);

	if (this->_texVideo[0] == 0)
	{
		//the ds screen fills the texture entirely, so we dont have garbage at edge to worry about,
		//but we need to make sure this is clamped for when filtering is selected
		glGenTextures(WIN32_FRAMEBUFFER_COUNT, this->_texVideo);

		for (size_t i = 0; i < WIN32_FRAMEBUFFER_COUNT; i++)
		{
			glBindTexture(GL_TEXTURE_2D, this->_texVideo[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoWidth, videoHeight, 0, GL_RGBA, (colorFormat == NDSColorFormat_BGR555_Rev) ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, samplerType);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, samplerType);
		}
	}
	else
	{
		for (size_t i = 0; i < WIN32_FRAMEBUFFER_COUNT; i++)
		{
			glBindTexture(GL_TEXTURE_2D, this->_texVideo[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, videoWidth, videoHeight, 0, GL_RGBA, (colorFormat == NDSColorFormat_BGR555_Rev) ? GL_UNSIGNED_SHORT_1_5_5_5_REV : GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
		}
	}

	if (this->_texHud[0] == 0)
	{
		glGenTextures(WIN32_FRAMEBUFFER_COUNT, this->_texHud);

		for (size_t i = 0; i < WIN32_FRAMEBUFFER_COUNT; i++)
		{
			glBindTexture(GL_TEXTURE_2D, this->_texHud[i]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT * 2, 0, GL_BGRA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, samplerType);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, samplerType);
		}
	}
}

GLuint GLDISPLAY::getTexVideoID(u8 bufferIndex)
{
	return this->_texVideo[bufferIndex];
}

GLuint GLDISPLAY::getTexHudID(u8 bufferIndex)
{
	return this->_texHud[bufferIndex];
}

void GLDISPLAY::setUseOutputFilter(bool theState)
{
	this->_useOutputFilterPending = theState;
}

bool GLDISPLAY::useOutputFilter()
{
	return this->_useOutputFilterPending;
}

void GLDISPLAY::applyOutputFilterOGL()
{
	if (this->_useOutputFilterPending == this->_useOutputFilterApplied)
	{
		return;
	}

	GLint samplerType = (this->_useOutputFilterPending) ? GL_LINEAR : GL_NEAREST;
	this->_useOutputFilterApplied = this->_useOutputFilterPending;

	for (size_t i = 0; i < WIN32_FRAMEBUFFER_COUNT; i++)
	{
		glBindTexture(GL_TEXTURE_2D, this->_texVideo[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, samplerType);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, samplerType);

		glBindTexture(GL_TEXTURE_2D, this->_texHud[i]);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, samplerType);
		glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, samplerType);
	}
}

void GLDISPLAY::kill()
{
	if (!hwnd) return;

	privateDC = GetDC(hwnd);
	wglMakeCurrent(privateDC, privateContext);

	glDeleteTextures(WIN32_FRAMEBUFFER_COUNT, this->_texVideo);
	glDeleteTextures(WIN32_FRAMEBUFFER_COUNT, this->_texHud);

	for (size_t i = 0; i < WIN32_FRAMEBUFFER_COUNT; i++)
	{
		this->_texVideo[i] = 0;
		this->_texHud[i]   = 0;
	}

	wglMakeCurrent(NULL, privateContext);
	ReleaseDC(hwnd, privateDC);
	wglDeleteContext(privateContext);
	privateContext = NULL;
	hwnd = NULL;
	this->_isVsyncSupported = false;
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
	if (this->_useVsyncPending != this->_useVsyncApplied)
	{
		this->_useVsyncApplied = this->_useVsyncPending;
		if (this->_isVsyncSupported)
		{
			wglSwapIntervalEXT((this->_useVsyncApplied) ? 1 : 0);
		}
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
	this->_useVsyncPending = vsync;
}

void GLDISPLAY::showPage()
{
	SwapBuffers(privateDC);
}

