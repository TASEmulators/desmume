#define WIN32_LEAN_AND_MEAN
#include "../common.h"
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
			LOG("OpenGL mode: %s\n",opengl_modes[res]); 
		else 
			LOG("OpenGL mode: uknown\n");

	oglAlreadyInit = true;

	return true;
}