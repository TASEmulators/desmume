/*
	Copyright (C) 2024 DeSmuME team

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

#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/glxext.h>

#include "glx_3Demu.h"


static Display *currDisplay = NULL;
static GLXDrawable currDrawable = 0;
static GLXContext currContext = NULL;

static Display *prevDisplay = NULL;
static GLXDrawable prevDrawDrawable = 0;
static GLXDrawable prevReadDrawable = 0;
static GLXContext prevContext = NULL;

static GLXDrawable pendingDrawable = 0;

static GLXFBConfig bestFBConfig = NULL;

static bool didErrorOccur = false;

static int __GLXContextErrorHandler(Display *dpy, XErrorEvent *ev)
{
    didErrorOccur = true;
    return 0;
}

static void __GLXCurrentDisplayClose()
{
	if (currDisplay == NULL)
	{
		return;
	}

	XCloseDisplay(currDisplay);
	currDisplay = NULL;
}

static bool __glx_initOpenGL(const int requestedProfile, const int requestedVersionMajor, const int requestedVersionMinor)
{
	if (currContext != NULL)
	{
		return true;
	}

	currDisplay = XOpenDisplay(NULL);

	int currScreen = DefaultScreen(currDisplay);
	int glxMajorVersion = 0;
	int glxMinorVersion = 0;

	if (glXQueryVersion(currDisplay, &glxMajorVersion, &glxMinorVersion) == False)
	{
		__GLXCurrentDisplayClose();
		puts("GLX: glXQueryVersion failed");
		return false;
	}

	// Our GLX context requires the following features:
	// - GLX_ARB_create_context_profile
	PFNGLXCREATECONTEXTATTRIBSARBPROC glXCreateContextAttribsARB = NULL;

	if ( (glxMajorVersion >= 2) || ((glxMajorVersion == 1) && (glxMinorVersion >= 4)) )
	{
		const char *extensionSet = glXQueryExtensionsString(currDisplay, currScreen);

		const char *foundString = strstr(extensionSet, "GLX_ARB_create_context_profile");
		glXCreateContextAttribsARB = (PFNGLXCREATECONTEXTATTRIBSARBPROC)glXGetProcAddress( (const GLubyte *) "glXCreateContextAttribsARB");

		if ( (foundString == NULL) || (glXCreateContextAttribsARB == NULL) )
		{
			__GLXCurrentDisplayClose();
			puts("GLX: OpenGL context creation is not available.");
			return false;
		}
	}
	else
	{
		__GLXCurrentDisplayClose();
		printf("GLX: Requires GLX v1.4 or later. Your GLX version (v%i.%i) is too old.\n", (int)glxMajorVersion, (int)glxMinorVersion);
		return false;
	}

	// Begin the process of searching for the best framebuffer configuration.
	// This will be needed for creating the context and other related things.
	int fbConfigAttr[] = {
		GLX_RENDER_TYPE,   GLX_RGBA_BIT,
		GLX_RED_SIZE,      8,
		GLX_GREEN_SIZE,    8,
		GLX_BLUE_SIZE,     8,
		GLX_ALPHA_SIZE,    8,
		GLX_DRAWABLE_TYPE, GLX_PBUFFER_BIT,
		GLX_DEPTH_SIZE,    24,
		GLX_STENCIL_SIZE,  8,
		0
	};

	if (requestedProfile == GLX_CONTEXT_CORE_PROFILE_BIT_ARB)
	{
		// 3.2 Core Profile always uses FBOs, and so no P-Buffers, depth
		// buffers, or stencil buffers are needed for this kind of context.
		fbConfigAttr[10] = 0;
		fbConfigAttr[11] = 0;

		fbConfigAttr[12] = 0;
		fbConfigAttr[13] = 0;

		fbConfigAttr[14] = 0;
		fbConfigAttr[15] = 0;
	}

	int fbConfigCount = 0;
	GLXFBConfig *fbConfigList = glXChooseFBConfig(currDisplay, currScreen, fbConfigAttr, &fbConfigCount);
	if ( (fbConfigList == NULL) || (fbConfigCount < 1) )
	{
		__GLXCurrentDisplayClose();
		puts("GLX: glXChooseFBConfig found no configs");
		return false;
	}

	int bestFBConfigIndex = -1;
	int bestNumSamples = -1;
	int configHasMultisamples = 0;
	int numSamples = 0;

	// Discover the best framebuffer config for multisampled rendering on the
	// default framebuffer. This isn't needed if we're running FBOs, but is
	// needed for legacy OpenGL without FBO support.
	for (int i = 0; i < fbConfigCount; i++)
	{
		XVisualInfo *vi = glXGetVisualFromFBConfig(currDisplay, fbConfigList[i]);

		if (vi != NULL)
		{
			glXGetFBConfigAttrib(currDisplay, fbConfigList[i], GLX_SAMPLE_BUFFERS, &configHasMultisamples);
			glXGetFBConfigAttrib(currDisplay, fbConfigList[i], GLX_SAMPLES, &numSamples);

			if ( ((bestFBConfigIndex < 0) || configHasMultisamples) && (numSamples > bestNumSamples) )
			{
				bestFBConfigIndex = i;
				bestNumSamples = numSamples;
			}

			XFree(vi);
		}
	}

	bestFBConfig = fbConfigList[bestFBConfigIndex];
	XFree(fbConfigList);

	if (requestedProfile == GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB)
	{
		const int pbufferAttr[] = {
			GLX_PBUFFER_WIDTH,  256,
			GLX_PBUFFER_HEIGHT, 192,
			0
		};

		currDrawable = glXCreatePbuffer(currDisplay, bestFBConfig, pbufferAttr);
	}

	// Install an X error handler so the application won't exit if GL 3.0
	// context allocation fails.
	//
	// Note this error handler is global.  All display connections in all threads
	// of a process use the same error handler, so be sure to guard against other
	// threads issuing X commands while this code is running.
	didErrorOccur = false;
	int (*oldHandler)(Display*, XErrorEvent*) = XSetErrorHandler(&__GLXContextErrorHandler);

	// GLX context attributes
	const int ctxAttr[] = {
		GLX_CONTEXT_MAJOR_VERSION_ARB, requestedVersionMajor,
		GLX_CONTEXT_MINOR_VERSION_ARB, requestedVersionMinor,
		GLX_CONTEXT_PROFILE_MASK_ARB,  requestedProfile,
		0
	};

	currContext = glXCreateContextAttribsARB(currDisplay, bestFBConfig, NULL, True, ctxAttr);

	XSync(currDisplay, False); // Sync to ensure any errors generated are processed.
	XSetErrorHandler(oldHandler); // Restore the original error handler

	if (didErrorOccur || (currContext == NULL))
	{
		__GLXCurrentDisplayClose();
		puts("GLX: glXCreateContextAttribsARB failed");
		return false;
	}

	puts("GLX: OpenGL context creation successful.");
    return true;
}

bool glx_initOpenGL_StandardAuto()
{
	bool isContextCreated = glx_initOpenGL_3_2_CoreProfile();

    if (!isContextCreated)
    {
        isContextCreated = glx_initOpenGL_LegacyAuto();
    }

    return isContextCreated;
}

bool glx_initOpenGL_LegacyAuto()
{
    return __glx_initOpenGL(GLX_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB, 1, 2);
}

bool glx_initOpenGL_3_2_CoreProfile()
{
    return __glx_initOpenGL(GLX_CONTEXT_CORE_PROFILE_BIT_ARB, 3, 2);
}

void glx_deinitOpenGL()
{
	if (currDrawable != 0)
	{
		glXDestroyPbuffer(currDisplay, currDrawable);
		currDrawable = 0;
	}

	if (pendingDrawable != 0)
	{
		glXDestroyPbuffer(currDisplay, pendingDrawable);
		pendingDrawable = 0;
	}

	if ( (currDisplay != NULL) && (currContext != NULL) )
	{
		glXMakeContextCurrent(currDisplay, 0, 0, NULL);
		glXDestroyContext(currDisplay, currContext);
		currContext = NULL;

		__GLXCurrentDisplayClose();
	}

	prevDisplay      = NULL;
	prevDrawDrawable = 0;
	prevReadDrawable = 0;
	prevContext      = NULL;
}

bool glx_beginOpenGL()
{
	Bool ret = False;

	prevDisplay      = glXGetCurrentDisplay();
	prevDrawDrawable = glXGetCurrentDrawable();
	prevReadDrawable = glXGetCurrentReadDrawable();
    prevContext      = glXGetCurrentContext();

	if (pendingDrawable != 0)
	{
		bool previousIsCurrent = (prevDrawDrawable == currDrawable);

		ret = glXMakeContextCurrent(currDisplay, pendingDrawable, pendingDrawable, currContext);
		if (ret == True)
		{
			glXDestroyPbuffer(currDisplay, currDrawable);
			currDrawable = pendingDrawable;

			if (previousIsCurrent)
			{
				prevDrawDrawable = currDrawable;
				prevReadDrawable = currDrawable;
			}

			pendingDrawable = 0;
		}
	}
	else
	{
		ret = glXMakeContextCurrent(currDisplay, currDrawable, currDrawable, currContext);
	}

	return (ret == True);
}

void glx_endOpenGL()
{
	if (prevDisplay == NULL)
	{
		prevDisplay = currDisplay;
	}

	if (prevContext == NULL)
	{
		prevContext = currContext;
	}

	if (prevDrawDrawable == 0)
	{
		prevDrawDrawable = currDrawable;
	}

	if (prevReadDrawable == 0)
	{
		prevReadDrawable = currDrawable;
	}

	if ( (prevDisplay      != currDisplay) ||
	     (prevContext      != currContext) ||
	     (prevDrawDrawable != currDrawable) ||
	     (prevReadDrawable != currDrawable) )
	{
		glXMakeContextCurrent(prevDisplay, prevDrawDrawable, prevReadDrawable, prevContext);
	}
}

bool glx_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h)
{
	bool result = false;

	if (isFBOSupported)
	{
		result = true;
		return result;
	}

	unsigned int currWidth;
	unsigned int currHeight;
	glXQueryDrawable(currDisplay, currDrawable, GLX_WIDTH,  &currWidth);
	glXQueryDrawable(currDisplay, currDrawable, GLX_HEIGHT, &currHeight);

	if ( (w == (size_t)currWidth) && (h == (size_t)currHeight) )
	{
		result = true;
		return result;
	}

	if (pendingDrawable != 0)
	{
		glXDestroyPbuffer(currDisplay, pendingDrawable);
	}

	const int pbufferAttr[] = {
		GLX_PBUFFER_WIDTH,  (int)w,
		GLX_PBUFFER_HEIGHT, (int)h,
		0
	};

	pendingDrawable = glXCreatePbuffer(currDisplay, bestFBConfig, pbufferAttr);

	result = true;
	return result;
}
