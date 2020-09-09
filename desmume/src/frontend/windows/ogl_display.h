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

#ifndef _OGL_DISPLAY_H_
#define _OGL_DISPLAY_H_

#include "types.h"
#include "ogl.h"

#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/wglext.h>


struct GLDISPLAY
{
private:
	HGLRC privateContext;
	HDC privateDC;
	HWND hwnd;
	bool initialize(HWND hwnd);
	bool WGLExtensionSupported(const char *extension_name);
	void _setvsync(bool isVsyncEnabled);

public:
	bool active;
	bool wantVsync, haveVsync;
	bool filter;

	GLDISPLAY();
	void kill();
	bool begin(HWND hwnd);
	void end();
	void setvsync(bool vsync);
	void showPage();
};

#endif