/*
	Copyright (C) 2006-2013 DeSmuME team

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

#define WIN32_LEAN_AND_MEAN
#include "../common.h"
#include "../debug.h"
#include <gl/gl.h>
#include <gl/glext.h>
#include <GL/wglext.h>
#include "console.h"
#include "CWindow.h"
#include "OGLRender.h"

extern WINCLASS	*MainWindow;

static HWND hwndFake;
static bool oglAlreadyInit = false;

int CheckHardwareSupport(HDC hdc)
{
   int PixelFormat = GetPixelFormat(hdc);
   PIXELFORMATDESCRIPTOR pfd;

   DescribePixelFormat(hdc,PixelFormat,sizeof(PIXELFORMATDESCRIPTOR),&pfd);
   if ((pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED))
      return 0; // Software acceleration OpenGL

   else if ((pfd.dwFlags & PFD_GENERIC_FORMAT) && (pfd.dwFlags & PFD_GENERIC_ACCELERATED))
      return 1; // Half hardware acceleration OpenGL (MCD driver)

   else if ( !(pfd.dwFlags & PFD_GENERIC_FORMAT) && !(pfd.dwFlags & PFD_GENERIC_ACCELERATED))
      return 2; // Full hardware acceleration OpenGL
   return -1; // check error
}

bool initContext(HWND hwnd, HGLRC *hRC, HDC *hdc)
{
	*hRC = NULL;
	*hdc = NULL;

	HDC oglDC = GetDC (hwnd);

	GLuint PixelFormat;
  static PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 24;
	pfd.cAlphaBits = 8;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE ;
  PixelFormat = ChoosePixelFormat(oglDC, &pfd);
  SetPixelFormat(oglDC, PixelFormat, &pfd);

	*hRC = wglCreateContext(oglDC);
	if (!hRC)
	{
		DeleteObject(oglDC);
		return false;
	}

	*hdc = oglDC;

	return true;
}

static HGLRC main_hRC;
static HDC main_hDC;

static bool _begin()
{
	if(!wglMakeCurrent(main_hDC, main_hRC))
		return false;

	return true;
}

bool windows_opengl_init()
{
	static const char *opengl_modes[3]={"software","half hardware (MCD driver)","hardware"};

	if(oglAlreadyInit == true) return true;

	GLuint PixelFormat;
	static PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 24;
	pfd.cAlphaBits = 8;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	//make a fake, invisible window, to render into. it's unclear yet how well this works, but i refuse to give up yet on making opengl render headless without relying on PBO and the like.
	//the width and height of this window must be lager than 256,192 for some reason, to give old crappy opengl profiles room to render into
	//if we ever support 3d upscaling, this would have to be changed. perhaps we could re-initialize the video system to use an appropriate window size
	HWND fakeWindow = CreateWindow("EDIT", 0, 0, 0, 0, 512, 512, 0, 0, 0, 0);

	main_hDC = GetDC(fakeWindow);
	PixelFormat = ChoosePixelFormat(main_hDC, &pfd);
	SetPixelFormat(main_hDC, PixelFormat, &pfd);
	main_hRC = wglCreateContext(main_hDC);
	wglMakeCurrent(main_hDC, main_hRC);

	int res = CheckHardwareSupport(main_hDC);
	if (res>=0&&res<=2) 
		INFO("WGL OpenGL mode: %s\n",opengl_modes[res]); 
	else 
		INFO("WGL OpenGL mode: uknown\n");

	oglAlreadyInit = true;

	oglrender_beginOpenGL = _begin;

	return true;
}
