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

#ifndef _OGL_DISPLAY_H_
#define _OGL_DISPLAY_H_

#include "types.h"
#include "GPU.h"
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

	bool _isVsyncSupported;

	GLuint *_texVideo;
	GLuint *_texHud;

	bool _useOutputFilterPending;
	bool _useOutputFilterApplied;
	bool _useVsyncPending;
	bool _useVsyncApplied;

public:
	GLDISPLAY();
	~GLDISPLAY();

	bool active;

	void initVideoTexture(NDSColorFormat colorFormat, GLsizei videoWidth, GLsizei videoHeight);
	GLuint getTexVideoID(u8 bufferIndex);
	GLuint getTexHudID(u8 bufferIndex);

	void setUseOutputFilter(bool theState);
	bool useOutputFilter();
	void applyOutputFilterOGL();

	void kill();
	bool begin(HWND hwnd);
	void end();
	void setvsync(bool vsync);
	void showPage();
};

#endif