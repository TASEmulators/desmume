/*  
    Copyright (C) 2013 The Lemon Man

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifdef HAVE_GL_GLX
#include <stdio.h>
#include <X11/Xlib.h>
#include <GL/glx.h>
#include "../OGLRender.h"

static bool glx_beginOpenGL(void) { return 1; }
static void glx_endOpenGL(void) { }
static bool glx_init(void) { return true; }

static GLXContext ctx;
static GLXPbuffer pbuf;

int deinit_glx_3Demu(void)
{
    Display *dpy = glXGetCurrentDisplay();

    if (dpy)
    {
        glXDestroyPbuffer(dpy, pbuf);
        glXDestroyContext(dpy, ctx);

        XCloseDisplay(dpy);

        return true;
    }

    return false;
}

int init_glx_3Demu(void) 
{
    Display *dpy = XOpenDisplay(NULL);
    XVisualInfo *vis;
    GLXFBConfig *cfg;
    int maj, min;

    if (!dpy)
        return false;

    // Check if GLX is present
    if (!glXQueryVersion(dpy, &maj, &min))
        return false;

    // We need GLX 1.3 at least
    if (maj < 1 || (maj == 1 && min < 3))
        return false;

    const int vis_attr[] =  {
      GLX_RGBA, 
      GLX_RED_SIZE,       8, 
      GLX_GREEN_SIZE,     8, 
      GLX_BLUE_SIZE,      8, 
      GLX_ALPHA_SIZE,     8,
      GLX_DEPTH_SIZE,     24,
      GLX_STENCIL_SIZE,   8,
      GLX_DOUBLEBUFFER, 
      None
    };
    vis = glXChooseVisual(dpy, DefaultScreen(dpy), (int *)&vis_attr);

    if (!vis)
        return false;

    const int fb_attr[] = { 
      GLX_RENDER_TYPE,    GLX_RGBA_BIT,
      GLX_DRAWABLE_TYPE,  GLX_PBUFFER_BIT,
      GLX_X_VISUAL_TYPE,  GLX_TRUE_COLOR,
      GLX_DOUBLEBUFFER,   true,
      GLX_X_RENDERABLE,   true,
      GLX_RED_SIZE,       8,
      GLX_GREEN_SIZE,     8,
      GLX_BLUE_SIZE,      8,
      GLX_ALPHA_SIZE,     8,
      GLX_DEPTH_SIZE,     24,
      GLX_STENCIL_SIZE,   8,
      None 
    };
    int configs;
    cfg = glXChooseFBConfig(dpy, DefaultScreen(dpy), (int *)&fb_attr, &configs);

    if (!cfg)
      return false;

    const int pbuf_attr[] = {
      GLX_PBUFFER_WIDTH,  256,
      //GLX_PBUFFER_HEIGHT, 192, // Use a square size to prevent possible incompatibilities
      GLX_PBUFFER_HEIGHT, 256,
      None
    };
    // The first should match exactly, otherwise is the least wrong one
    pbuf = glXCreatePbuffer(dpy, cfg[0], (int *)&pbuf_attr);

    XFree(cfg);

    ctx = glXCreateContext(dpy, vis, NULL, true);

    if (!ctx)
      return false;

    if (!glXMakeContextCurrent(dpy, pbuf, pbuf, ctx))
      return false;

    printf("OGL/GLX Renderer has finished the initialization.\n");

    oglrender_init = glx_init;
    oglrender_beginOpenGL = glx_beginOpenGL;
    oglrender_endOpenGL = glx_endOpenGL;

    return true;
}

#endif // HAVE_GLX
