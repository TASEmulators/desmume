/*
	Copyright (C) 2009 Guillaume Duhamel
	Copyright (C) 2009-2024 DeSmuME team

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

#include <stdlib.h>
#include <GL/osmesa.h>

#include "osmesa_3Demu.h"
#include "OGLRender_3_2.h"

#warning OSMesa contexts are deprecated.


static OSMesaContext currContext = NULL;
static GLint currCtxFormat = GL_UNSIGNED_BYTE;
static GLint currCtxWidth = 0;
static GLint currCtxHeight = 0;
static void *currCtxBuffer = NULL;

static OSMesaContext prevContext = NULL;
static GLint prevCtxFormat = GL_UNSIGNED_BYTE;
static GLint prevCtxWidth = 0;
static GLint prevCtxHeight = 0;
static void *prevCtxBuffer = NULL;

static void *pendingCtxBuffer = NULL;
static GLint pendingCtxWidth = 0;
static GLint pendingCtxHeight = 0;

static bool __osmesa_initOpenGL(const int requestedProfile, const int requestedVersionMajor, const int requestedVersionMinor)
{
	GLboolean ret = GL_FALSE;

	if (currContext != NULL)
	{
		return true;
	}

#if (((OSMESA_MAJOR_VERSION * 100) + OSMESA_MINOR_VERSION) >= 1102) && defined(OSMESA_CONTEXT_MAJOR_VERSION)
	const int ctxAttributes[] = {
		OSMESA_FORMAT, OSMESA_RGBA,
		OSMESA_DEPTH_BITS,   ( (requestedProfile == OSMESA_CORE_PROFILE) && ((requestedVersionMajor >= 4) || ((requestedVersionMajor == 3) && (requestedVersionMinor >= 2))) ) ? 0 : 24,
		OSMESA_STENCIL_BITS, ( (requestedProfile == OSMESA_CORE_PROFILE) && ((requestedVersionMajor >= 4) || ((requestedVersionMajor == 3) && (requestedVersionMinor >= 2))) ) ? 0 : 8,
		OSMESA_ACCUM_BITS, 0,
		OSMESA_PROFILE, requestedProfile,
		OSMESA_CONTEXT_MAJOR_VERSION, requestedVersionMajor,
		OSMESA_CONTEXT_MINOR_VERSION, requestedVersionMinor,
		0
	};

	currContext = OSMesaCreateContextAttribs(ctxAttributes, NULL);
	if (currContext == NULL)
	{
		if (requestedProfile == OSMESA_CORE_PROFILE)
		{
			puts("OSMesa: OSMesaCreateContextAttribs() failed!");
			return false;
		}
		else if (requestedProfile == OSMESA_COMPAT_PROFILE)
		{
			puts("OSMesa: Could not create a context using OSMesaCreateContextAttribs(). Will attempt to use OSMesaCreateContextExt() instead...");
		}
	}

	if ( (currContext == NULL) && (requestedProfile == OSMESA_COMPAT_PROFILE) )
#endif
	{
		currContext = OSMesaCreateContextExt(OSMESA_RGBA, 24, 8, 0, NULL);
		if (currContext == NULL)
		{
			puts("OSMesa: OSMesaCreateContextExt() failed!");
			return false;
		}
	}

	currCtxFormat = GL_UNSIGNED_BYTE;
	currCtxWidth  = GPU_FRAMEBUFFER_NATIVE_WIDTH;
	currCtxHeight = GPU_FRAMEBUFFER_NATIVE_HEIGHT;

	currCtxBuffer = malloc_alignedPage(currCtxWidth * currCtxHeight * sizeof(uint32_t));
	if (currCtxBuffer == NULL)
	{
		OSMesaDestroyContext(currContext);
		currCtxFormat = GL_UNSIGNED_BYTE;
		currCtxWidth  = 0;
		currCtxHeight = 0;

		puts("OSMesa: Could not allocate enough memory for the context!");
		return false;
    }

	puts("OSMesa: Context creation successful.");
	return true;
}

bool osmesa_initOpenGL_StandardAuto()
{
	bool isContextCreated = __osmesa_initOpenGL(OSMESA_CORE_PROFILE, 4, 1);

    if (!isContextCreated)
    {
        isContextCreated = osmesa_initOpenGL_3_2_CoreProfile();
    }

    if (!isContextCreated)
    {
        isContextCreated = osmesa_initOpenGL_LegacyAuto();
    }

    return isContextCreated;
}

bool osmesa_initOpenGL_LegacyAuto()
{
	return __osmesa_initOpenGL(OSMESA_COMPAT_PROFILE, 1, 2);
}

bool osmesa_initOpenGL_3_2_CoreProfile()
{
	return __osmesa_initOpenGL(OSMESA_CORE_PROFILE, 3, 2);
}

void osmesa_deinitOpenGL()
{
	OSMesaMakeCurrent(NULL, NULL, GL_UNSIGNED_BYTE, 0, 0);

	if (currContext != NULL)
	{
		OSMesaDestroyContext(currContext);
		currContext = NULL;
		currCtxFormat = GL_UNSIGNED_BYTE;
	}

	if (currCtxBuffer != NULL)
	{
		free_aligned(currCtxBuffer);
		currCtxBuffer = NULL;
		currCtxWidth  = 0;
		currCtxHeight = 0;
	}

	if (pendingCtxBuffer != NULL)
	{
		free_aligned(pendingCtxBuffer);
		pendingCtxBuffer = NULL;
		pendingCtxWidth  = 0;
		pendingCtxHeight = 0;
	}

	prevContext   = NULL;
	prevCtxFormat = GL_UNSIGNED_BYTE;
	prevCtxWidth  = 0;
	prevCtxHeight = 0;
	prevCtxBuffer = NULL;
}

bool osmesa_beginOpenGL()
{
	GLboolean ret = GL_FALSE;
	OSMesaContext oldContext = OSMesaGetCurrentContext();

	if (oldContext != NULL)
	{
		prevContext = oldContext;
		OSMesaGetColorBuffer(prevContext, &prevCtxWidth, &prevCtxHeight, &prevCtxFormat, &prevCtxBuffer);
	}
	else
	{
		prevContext   = currContext;
		prevCtxFormat = currCtxFormat;
		prevCtxWidth  = currCtxWidth;
		prevCtxHeight = currCtxHeight;
		prevCtxBuffer = currCtxBuffer;
	}

	if (pendingCtxBuffer != NULL)
	{
		bool previousIsCurrent = (prevCtxBuffer == currCtxBuffer);

		ret = OSMesaMakeCurrent(currContext, pendingCtxBuffer, currCtxFormat, pendingCtxWidth, pendingCtxHeight);;
		if (ret == GL_TRUE)
		{
			free_aligned(currCtxBuffer);
			currCtxBuffer = pendingCtxBuffer;

			if (previousIsCurrent)
			{
				prevCtxBuffer = pendingCtxBuffer;
				prevCtxWidth  = pendingCtxWidth;
				prevCtxHeight = pendingCtxHeight;
			}

			pendingCtxBuffer = NULL;
			pendingCtxWidth  = 0;
			pendingCtxHeight = 0;
		}
	}
	else
	{
		ret = OSMesaMakeCurrent(currContext, currCtxBuffer, currCtxFormat, currCtxWidth, currCtxHeight);
	}

	return (ret == GL_TRUE);
}

void osmesa_endOpenGL()
{
	if (prevContext == NULL)
	{
		prevContext = currContext;
	}

	if (prevCtxBuffer == NULL)
	{
		prevCtxBuffer = currCtxBuffer;
	}

	if (prevCtxWidth == 0)
	{
		prevCtxWidth = currCtxWidth;
	}

	if (prevCtxHeight == 0)
	{
		prevCtxHeight = currCtxHeight;
	}

	if (prevCtxFormat == GL_UNSIGNED_BYTE)
	{
		prevCtxFormat = currCtxFormat;
	}

	if ( (prevContext   != currContext) ||
	     (prevCtxBuffer != currCtxBuffer) ||
	     (prevCtxFormat != currCtxFormat) ||
	     (prevCtxWidth  != currCtxWidth) ||
	     (prevCtxHeight != currCtxHeight) )
	{
		OSMesaMakeCurrent(prevContext, prevCtxBuffer, prevCtxFormat, prevCtxWidth, prevCtxHeight);
	}
}

bool osmesa_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h)
{
	bool result = false;

	if (isFBOSupported)
	{
		result = true;
		return result;
	}

	if ( (w == (size_t)currCtxWidth) && (h == (size_t)currCtxHeight) )
	{
		result = true;
		return result;
	}

	if (pendingCtxBuffer != NULL)
	{
		free_aligned(pendingCtxBuffer);
	}

	pendingCtxWidth  = (GLint)w;
	pendingCtxHeight = (GLint)h;
	pendingCtxBuffer = malloc_alignedPage(w * h * sizeof(uint32_t));

	result = true;
	return result;
}
