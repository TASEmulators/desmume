#ifndef DESMUME_WGL_EXT_H
#define DESMUME_WGL_EXT_H

// Minimal WGL declarations needed by the Windows OpenGL frontend.
// The Windows SDK ships only OpenGL 1.1 and no WGL extension header, so the
// project previously carried a full copy of wglext.h. This keeps just the
// subset actually used by ogl.cpp / ogl_display.cpp / main.cpp.

#include <windows.h>

#ifndef WGL_EXT_MINIMAL
#define WGL_EXT_MINIMAL

DECLARE_HANDLE(HPBUFFERARB);

#define WGL_ACCELERATION_ARB         0x2003
#define WGL_SUPPORT_OPENGL_ARB       0x2010
#define WGL_DOUBLE_BUFFER_ARB        0x2011
#define WGL_COLOR_BITS_ARB           0x2014
#define WGL_RED_BITS_ARB             0x2015
#define WGL_GREEN_BITS_ARB           0x2017
#define WGL_BLUE_BITS_ARB            0x2019
#define WGL_ALPHA_BITS_ARB           0x201B
#define WGL_DEPTH_BITS_ARB           0x2022
#define WGL_STENCIL_BITS_ARB         0x2023
#define WGL_FULL_ACCELERATION_ARB    0x2027
#define WGL_DRAW_TO_PBUFFER_ARB      0x202D
#define WGL_PBUFFER_WIDTH_ARB        0x2034
#define WGL_PBUFFER_HEIGHT_ARB       0x2035

typedef const char* (WINAPI* PFNWGLGETEXTENSIONSSTRINGARBPROC)(HDC hdc);
typedef const char* (WINAPI* PFNWGLGETEXTENSIONSSTRINGEXTPROC)(void);
typedef HPBUFFERARB (WINAPI* PFNWGLCREATEPBUFFERARBPROC)(HDC hDC, int iPixelFormat, int iWidth, int iHeight, const int* piAttribList);
typedef HDC (WINAPI* PFNWGLGETPBUFFERDCARBPROC)(HPBUFFERARB hPbuffer);
typedef int (WINAPI* PFNWGLRELEASEPBUFFERDCARBPROC)(HPBUFFERARB hPbuffer, HDC hDC);
typedef BOOL (WINAPI* PFNWGLDESTROYPBUFFERARBPROC)(HPBUFFERARB hPbuffer);
typedef BOOL (WINAPI* PFNWGLQUERYPBUFFERARBPROC)(HPBUFFERARB hPbuffer, int iAttribute, int* piValue);
typedef BOOL (WINAPI* PFNWGLGETPIXELFORMATATTRIBIVARBPROC)(HDC hdc, int iPixelFormat, int iLayerPlane, UINT nAttributes, const int* piAttributes, int* piValues);
typedef BOOL (WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC hdc, const int* piAttribIList, const FLOAT* pfAttribFList, UINT nMaxFormats, int* piFormats, UINT* nNumFormats);
typedef BOOL (WINAPI* PFNWGLBINDTEXIMAGEARBPROC)(HPBUFFERARB hPbuffer, int iBuffer);
typedef BOOL (WINAPI* PFNWGLRELEASETEXIMAGEARBPROC)(HPBUFFERARB hPbuffer, int iBuffer);
typedef BOOL (WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int interval);
typedef int (WINAPI* PFNWGLGETSWAPINTERVALEXTPROC)(void);

#endif
#endif
