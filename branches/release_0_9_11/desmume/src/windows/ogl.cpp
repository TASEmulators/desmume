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

//in case we ever need glew
//#define GLEW_STATIC
//#include <GL/glew.c> //yeah, build the file here.
//#include <GL/glew.h>
//#include <GL/wglew.h>

#include "OGLRender.h"

#include <gl/gl.h>
#include <gl/glext.h>
#include <GL/wglext.h>
#include "console.h"
#include "CWindow.h"

extern WINCLASS	*MainWindow;

static HWND hwndFake;
static bool oglAlreadyInit = false;

static void check_init_glew()
{
	static bool initialized = false;
	if(initialized) return;
	initialized = true;
//	glewInit();
}

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

bool initContext(HWND hwnd, HGLRC *hRC)
{
	*hRC = NULL;

	HDC oglDC = GetDC (hwnd);

	GLuint PixelFormat;
	static PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cAlphaBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;
	PixelFormat = ChoosePixelFormat(oglDC, &pfd);
	SetPixelFormat(oglDC, PixelFormat, &pfd);
	printf("GL display context pixel format: %d\n",PixelFormat);

	*hRC = wglCreateContext(oglDC);
	if (!hRC)
	{
		DeleteObject(oglDC);
		return false;
	}

	wglMakeCurrent(NULL,NULL);

	return true;
}

static HGLRC main_hRC;
static HDC main_hDC;
static HWND main_hWND;

static bool _begin()
{
	//wglMakeCurrent is slow in some environments. so, check if the desired context is already current
	if(wglGetCurrentContext() == main_hRC)
		return true;

	if(!wglMakeCurrent(main_hDC, main_hRC))
		return false;

	return true;
}

static bool makeBootstrapContext()
{
	//not sure how relevant all this is, since it is just a context for bootstrapping, but we may as well make it as normal as we can, just to be safe
	PIXELFORMATDESCRIPTOR pfd;
	memset(&pfd,0, sizeof(PIXELFORMATDESCRIPTOR));
	pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
	pfd.nVersion = 1;
	pfd.dwFlags = PFD_SUPPORT_OPENGL;
	pfd.iPixelType = PFD_TYPE_RGBA;
	pfd.cColorBits = 24;
	pfd.cAlphaBits = 8;
	pfd.cDepthBits = 24;
	pfd.cStencilBits = 8;
	pfd.iLayerType = PFD_MAIN_PLANE;

	//it seems we may have to specify some non-zero win
	int width, height;
	width = height = 512; //something safe, but irrelevant. just to be super-safe, lets try making it big enough for the pbuffer we'll make eventually
	HWND fakeWindow = CreateWindow("EDIT", 0, 0, 0, 0, width, height, 0, 0, 0, 0);
	main_hWND = fakeWindow;

	main_hDC = GetDC(fakeWindow);
	GLuint PixelFormat = ChoosePixelFormat(main_hDC, &pfd);
	SetPixelFormat(main_hDC, PixelFormat, &pfd);
	main_hRC = wglCreateContext(main_hDC);
	wglMakeCurrent(main_hDC, main_hRC);

	//report info on the driver we found
	static const char *opengl_modes[3]={"software","half hardware (MCD driver)","hardware"};
	int res = CheckHardwareSupport(main_hDC);
	if (res>=0&&res<=2) 
		INFO("WGL OpenGL mode: %s\n",opengl_modes[res]); 
	else 
		INFO("WGL OpenGL mode: uknown\n");

	return true;
}

bool windows_opengl_init()
{
	if(oglAlreadyInit) return true;

	if(!makeBootstrapContext())
	{
		printf("GL bootstrap context failed\n");
		return false;
	}

	//thx will perone

	PFNWGLGETEXTENSIONSSTRINGARBPROC wglGetExtensionsStringARB = (PFNWGLGETEXTENSIONSSTRINGARBPROC)wglGetProcAddress("wglGetExtensionsStringARB");
	PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB = (PFNWGLCHOOSEPIXELFORMATARBPROC)wglGetProcAddress("wglChoosePixelFormatARB");
	PFNWGLCREATEPBUFFERARBPROC wglCreatePbufferARB = (PFNWGLCREATEPBUFFERARBPROC)wglGetProcAddress("wglCreatePbufferARB");
	PFNWGLGETPBUFFERDCARBPROC wglGetPbufferDCARB = (PFNWGLGETPBUFFERDCARBPROC)wglGetProcAddress("wglGetPbufferDCARB");
	PFNWGLQUERYPBUFFERARBPROC wglQueryPbufferARB = (PFNWGLQUERYPBUFFERARBPROC)wglGetProcAddress("wglQueryPbufferARB");
	PFNWGLDESTROYPBUFFERARBPROC wglDestroyPbufferARB = (PFNWGLDESTROYPBUFFERARBPROC)wglGetProcAddress("wglDestroyPbufferARB");
	PFNWGLRELEASEPBUFFERDCARBPROC wglReleasePbufferDCARB = (PFNWGLRELEASEPBUFFERDCARBPROC)wglGetProcAddress("wglReleasePbufferDCARB");
	PFNWGLBINDTEXIMAGEARBPROC wglBindTexImageARB = (PFNWGLBINDTEXIMAGEARBPROC)wglGetProcAddress("wglBindTexImageARB");
	PFNWGLRELEASETEXIMAGEARBPROC wglReleaseTexImageARB = (PFNWGLRELEASETEXIMAGEARBPROC)wglGetProcAddress("wglReleaseTexImageARB");
  PFNWGLGETPIXELFORMATATTRIBIVARBPROC wglGetPixelFormatAttribivARB = (PFNWGLGETPIXELFORMATATTRIBIVARBPROC)wglGetProcAddress("wglGetPixelFormatAttribivARB");

	if(!wglCreatePbufferARB)
	{
		printf("no PBuffer support on this video driver. sorry!");
		return false;
	}

	int intAttrs[32] ={
		WGL_COLOR_BITS_ARB,24,
		WGL_RED_BITS_ARB,8,
		WGL_GREEN_BITS_ARB,8,
		WGL_BLUE_BITS_ARB,8,
		WGL_ALPHA_BITS_ARB,8,
		WGL_DEPTH_BITS_ARB,24,
		WGL_STENCIL_BITS_ARB,8,
		WGL_DRAW_TO_PBUFFER_ARB, GL_TRUE,
		WGL_SUPPORT_OPENGL_ARB,GL_TRUE,
		WGL_ACCELERATION_ARB,WGL_FULL_ACCELERATION_ARB,
		WGL_DOUBLE_BUFFER_ARB,GL_FALSE,
		0};

	//setup pixel format
	UINT numFormats;
	int pixelFormat;
	if(!wglChoosePixelFormatARB(main_hDC, intAttrs, NULL, 1, &pixelFormat, &numFormats) || numFormats == 0)
	{
		printf("problem finding pixel format in wglChoosePixelFormatARB\n");
		return false;
	}

	//pbuf attributes
	int pbuf_width = 256;  //try 192 later, but i think it has to be square
	int pbuf_height = 256;
	static const int pbuf_attributes[]= {0};

	HPBUFFERARB pbuffer = wglCreatePbufferARB(main_hDC, pixelFormat, pbuf_width, pbuf_height, pbuf_attributes);
	HDC hdc = wglGetPbufferDCARB(pbuffer);
	HGLRC hGlRc = wglCreateContext(hdc);		

	//doublecheck dimensions
	int width, height;
	wglQueryPbufferARB(pbuffer, WGL_PBUFFER_WIDTH_ARB, &width);
	wglQueryPbufferARB(pbuffer, WGL_PBUFFER_HEIGHT_ARB, &height);
	if(width != pbuf_height || height != pbuf_height)
	{
		printf("wglCreatePbufferARB created some wrongly sized nonsense\n");
		return false;
	}

	//cleanup the bootstrap context
	wglDeleteContext(main_hRC);
	DeleteObject(main_hDC);
	DestroyWindow(main_hWND);

	main_hDC = hdc;
	main_hRC = hGlRc;
	oglAlreadyInit = true;
	oglrender_beginOpenGL = _begin;
	
	//use the new pbuffer context for further extension interrogation in shared opengl init
	_begin();

	return true;
}
