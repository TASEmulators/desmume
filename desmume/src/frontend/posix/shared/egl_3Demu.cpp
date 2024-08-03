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
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include "egl_3Demu.h"
#include "OGLRender_3_2.h"


static EGLDisplay currDisplay = EGL_NO_DISPLAY;
static EGLSurface currSurface = EGL_NO_SURFACE;
static EGLContext currContext = EGL_NO_CONTEXT;

static EGLDisplay prevDisplay = EGL_NO_DISPLAY;
static EGLSurface prevDrawSurface = EGL_NO_SURFACE;
static EGLSurface prevReadSurface = EGL_NO_SURFACE;
static EGLContext prevContext = EGL_NO_CONTEXT;

static EGLSurface pendingSurface = EGL_NO_SURFACE;
static EGLint *lastConfigAttr = NULL;

static bool __egl_initOpenGL(const int requestedAPI, const int requestedProfile, const int requestedVersionMajor, const int requestedVersionMinor)
{
	if (currContext != EGL_NO_CONTEXT)
	{
		return true;
	}

	EGLint eglMajorVersion;
	EGLint eglMinorVersion;

	currDisplay = eglGetDisplay(EGL_DEFAULT_DISPLAY);
	if (eglInitialize(currDisplay, &eglMajorVersion, &eglMinorVersion) == EGL_FALSE)
	{
		puts("EGL: eglInitialize failed");
		return false;
	}

	// Our EGL context requires the following features:
	// - EGL_OPENGL_ES3_BIT for EGL_RENDERABLE_TYPE
	// - EGL_KHR_surfaceless_context
	if ( (eglMajorVersion <= 1) && (eglMinorVersion < 5) )
	{
		if (eglMinorVersion < 4)
		{
			eglTerminate(currDisplay);
			printf("EGL: Requires EGL v1.4 or later. Your EGL version (v%i.%i) is too old.\n", (int)eglMajorVersion, (int)eglMinorVersion);
			return false;
		}

		const char *extensionSet = eglQueryString(currDisplay, EGL_EXTENSIONS);

		const char *foundString = strstr(extensionSet, "EGL_KHR_create_context");
		if (foundString == NULL)
		{
			eglTerminate(currDisplay);
			puts("EGL: OpenGL ES 3.0 context creation is not available.");
			return false;
		}

		if ( ((requestedAPI == EGL_OPENGL_API) && (requestedProfile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR)) ||
		      (requestedAPI == EGL_OPENGL_ES_API) )
		{
			foundString = strstr(extensionSet, "EGL_KHR_surfaceless_context");
			if (foundString == NULL)
			{
				eglTerminate(currDisplay);
				puts("EGL: EGL_KHR_surfaceless_context is a required extension for 3.2 Core Profile and OpenGL ES.");
				return false;
			}
		}
	}

	// EGL config attributes, initially set up for standard OpenGL running in legacy mode
	EGLint confAttr[] = {
		EGL_RED_SIZE,     8,
		EGL_GREEN_SIZE,   8,
		EGL_BLUE_SIZE,    8,
		EGL_ALPHA_SIZE,   8,
		EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
		EGL_DEPTH_SIZE,   24,
		EGL_STENCIL_SIZE, 8,
		EGL_NONE
	};

	if ( (requestedAPI == EGL_OPENGL_API) && (requestedProfile == EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR) )
	{
		confAttr[8] = EGL_NONE;
		confAttr[9] = EGL_NONE;

		confAttr[10] = EGL_NONE;
		confAttr[11] = EGL_NONE;

		confAttr[12] = EGL_NONE;
		confAttr[13] = EGL_NONE;
	}
	else if (requestedAPI == EGL_OPENGL_ES_API)
	{
		confAttr[8] = EGL_RENDERABLE_TYPE;
		confAttr[9] = EGL_OPENGL_ES3_BIT_KHR;

		confAttr[10] = EGL_NONE;
		confAttr[11] = EGL_NONE;

		confAttr[12] = EGL_NONE;
		confAttr[13] = EGL_NONE;
	}

	// choose the first config, i.e. best config
	EGLConfig eglConf;
	EGLint numConfigs;
	eglChooseConfig(currDisplay, confAttr, &eglConf, 1, &numConfigs);
	if (numConfigs < 1)
	{
		eglTerminate(currDisplay);
		puts("EGL: eglChooseConfig failed");
		return false;
	}

	// Make a copy of the last working config attributes for legacy OpenGL running
	// the default framebuffer.
	if (lastConfigAttr == NULL)
	{
		lastConfigAttr = (EGLint *)malloc(sizeof(confAttr));
	}
	memcpy(lastConfigAttr, confAttr, sizeof(confAttr));

	eglBindAPI(requestedAPI);

	// EGL context attributes
	const EGLint ctxAttr[] = {
		EGL_CONTEXT_MAJOR_VERSION_KHR, requestedVersionMajor,
		EGL_CONTEXT_MINOR_VERSION_KHR, requestedVersionMinor,
		(requestedAPI == EGL_OPENGL_ES_API) ? EGL_NONE : EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR, requestedProfile,
		EGL_NONE
	};

	currContext = eglCreateContext(currDisplay, eglConf, EGL_NO_CONTEXT, ctxAttr);
	if (currContext == EGL_NO_CONTEXT)
	{
		eglTerminate(currDisplay);
		puts("EGL: eglCreateContext failed");
		return false;
	}

	if (requestedProfile == EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR)
	{
		// Do note that P-Buffer creation has to occur after context creation
		// since P-Buffers are dependent on a context with EGL_PBUFFER_BIT as
		// its EGL_SURFACE_TYPE.
		const EGLint pbufferAttr[] = {
			EGL_WIDTH,  (EGLint)GPU_FRAMEBUFFER_NATIVE_WIDTH,
			EGL_HEIGHT, (EGLint)GPU_FRAMEBUFFER_NATIVE_HEIGHT,
			EGL_NONE
		};

		currSurface = eglCreatePbufferSurface(currDisplay, eglConf, pbufferAttr);
		if (currSurface == EGL_NO_SURFACE)
		{
			puts("EGL: eglCreatePbufferSurface failed");
			return false;
		}
	}

	puts("EGL: OpenGL context creation successful.");
    return true;
}

bool egl_initOpenGL_StandardAuto()
{
	bool isContextCreated = egl_initOpenGL_3_2_CoreProfile();

    if (!isContextCreated)
    {
        isContextCreated = egl_initOpenGL_LegacyAuto();
    }

    return isContextCreated;
}

bool egl_initOpenGL_LegacyAuto()
{
    return __egl_initOpenGL(EGL_OPENGL_API, EGL_CONTEXT_OPENGL_COMPATIBILITY_PROFILE_BIT_KHR, 1, 2);
}

bool egl_initOpenGL_3_2_CoreProfile()
{
    return __egl_initOpenGL(EGL_OPENGL_API, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR, 3, 2);
}

bool egl_initOpenGL_ES_3_0()
{
    return __egl_initOpenGL(EGL_OPENGL_ES_API, EGL_NONE, 3, 0);
}

void egl_deinitOpenGL()
{
	if (currDisplay == EGL_NO_DISPLAY)
	{
		return;
	}

	eglMakeCurrent(EGL_NO_DISPLAY, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
	eglDestroyContext(currDisplay, currContext);

	if (currSurface != EGL_NO_SURFACE)
	{
		eglDestroySurface(currDisplay, currSurface);
	}

	eglTerminate(currDisplay);

	if (lastConfigAttr == NULL)
	{
		free(lastConfigAttr);
		lastConfigAttr = NULL;
	}

	currContext = EGL_NO_CONTEXT;
	currSurface = EGL_NO_SURFACE;
	currDisplay = EGL_NO_DISPLAY;

	prevDisplay     = EGL_NO_DISPLAY;
	prevDrawSurface = EGL_NO_SURFACE;
	prevReadSurface = EGL_NO_SURFACE;
	prevContext     = EGL_NO_CONTEXT;
}

bool egl_beginOpenGL()
{
	EGLBoolean ret = EGL_FALSE;

	prevDisplay     = eglGetCurrentDisplay();
	prevDrawSurface = eglGetCurrentSurface(0);
	prevReadSurface = eglGetCurrentSurface(1);
    prevContext     = eglGetCurrentContext();

	if (pendingSurface != EGL_NO_SURFACE)
	{
		bool previousIsCurrent = (prevDrawSurface == currSurface);

		ret = eglMakeCurrent(currDisplay, pendingSurface, pendingSurface, currContext);
		if (ret == EGL_TRUE)
		{
			eglDestroySurface(currDisplay, currSurface);
			currSurface = pendingSurface;

			if (previousIsCurrent)
			{
				prevDrawSurface = currSurface;
				prevReadSurface = currSurface;
			}

			pendingSurface = EGL_NO_SURFACE;
		}
	}
	else
	{
		ret = eglMakeCurrent(currDisplay, currSurface, currSurface, currContext);
	}

	return (ret == EGL_TRUE);
}

void egl_endOpenGL()
{
	if (prevDisplay == EGL_NO_DISPLAY)
	{
		prevDisplay = currDisplay;
	}

	if (prevContext == EGL_NO_CONTEXT)
	{
		prevContext = currContext;
	}

	if (prevDrawSurface == EGL_NO_SURFACE)
	{
		prevDrawSurface = currSurface;
	}

	if (prevReadSurface == EGL_NO_SURFACE)
	{
		prevReadSurface = currSurface;
	}

	if ( (prevDisplay     != currDisplay) ||
	     (prevContext     != currContext) ||
	     (prevDrawSurface != currSurface) ||
	     (prevReadSurface != currSurface) )
	{
		eglMakeCurrent(prevDisplay, prevDrawSurface, prevReadSurface, prevContext);
	}
}

bool egl_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h)
{
	bool result = false;

	if (isFBOSupported)
	{
		result = true;
		return result;
	}

	EGLint currWidth;
	EGLint currHeight;
	eglQuerySurface(currDisplay, currSurface, EGL_WIDTH,  &currWidth);
	eglQuerySurface(currDisplay, currSurface, EGL_HEIGHT, &currHeight);

	if ( (w == (size_t)currWidth) && (h == (size_t)currHeight) )
	{
		result = true;
		return result;
	}

	if (pendingSurface != EGL_NO_SURFACE)
	{
		// Destroy any existing pending surface now since we need it to
		// always have the latest framebuffer size.
		eglDestroySurface(currDisplay, pendingSurface);
	}

	// choose the first config, i.e. best config
	EGLConfig eglConf;
	EGLint numConfigs;
	eglChooseConfig(currDisplay, lastConfigAttr, &eglConf, 1, &numConfigs);

	const EGLint pbufferAttr[] = {
		EGL_WIDTH,  (EGLint)w,
		EGL_HEIGHT, (EGLint)h,
		EGL_NONE
	};

	pendingSurface = eglCreatePbufferSurface(currDisplay, eglConf, pbufferAttr);
	if (pendingSurface == EGL_NO_SURFACE)
	{
		puts("EGL: eglCreatePbufferSurface failed");
		return result;
	}

	result = true;
	return result;
}
