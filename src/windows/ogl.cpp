/*  ogl.cpp

    Copyright (C) 2006-2009 DeSmuME team

    This file is part of DeSmuME

    DeSmuME is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    DeSmuME is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with DeSmuME; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
*/

#define WIN32_LEAN_AND_MEAN
#include "../common.h"
#include "../debug.h"
#include <gl/gl.h>
#include <gl/glext.h>
#include "console.h"
#include "CWindow.h"

extern WINCLASS	*MainWindow;

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

bool windows_opengl_init()
{
	HDC						oglDC = NULL;
	HGLRC					hRC = NULL;
	int						pixelFormat;
	PIXELFORMATDESCRIPTOR	pfd;
	int						res;
	char					*opengl_modes[3]={"software","half hardware (MCD driver)","hardware"};

	if(oglAlreadyInit == true) return true;

	oglDC = GetDC (MainWindow->getHWnd());

	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags =  PFD_DRAW_TO_BITMAP | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cDepthBits = 24;
	pfd.cAlphaBits = 8;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE ;

	pixelFormat = ChoosePixelFormat(oglDC, &pfd);
	if (pixelFormat == 0)
		return false;

	if(!SetPixelFormat(oglDC, pixelFormat, &pfd))
		return false;

	hRC = wglCreateContext(oglDC);
	if (!hRC)
		return false;

	if(!wglMakeCurrent(oglDC, hRC))
		return false;

	res=CheckHardwareSupport(oglDC);
	if (res>=0&&res<=2) 
			INFO("OpenGL mode: %s\n",opengl_modes[res]); 
		else 
			INFO("OpenGL mode: uknown\n");

	oglAlreadyInit = true;

	return true;
}
