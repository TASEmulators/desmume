/*
	Copyright (C) 2009 Guillaume Duhamel
	Copyright (C) 2009-2017 DeSmuME team
 
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

#ifdef HAVE_LIBOSMESA

#include <stdlib.h>
#include <GL/osmesa.h>
#include "../OGLRender.h"
#include "../OGLRender_3_2.h"

#include "osmesa_3Demu.h"

static void *buffer = NULL;
static OSMesaContext ctx = NULL;

static bool osmesa_beginOpenGL(void) { return 1; }
static void osmesa_endOpenGL(void) { }
static bool osmesa_init(void) { return is_osmesa_initialized(); }

void deinit_osmesa_3Demu (void)
{
    free(buffer);
    buffer = NULL;
    
    OSMesaDestroyContext(ctx);
    ctx = NULL;
}

bool init_osmesa_3Demu(void)
{
#if (((OSMESA_MAJOR_VERSION * 100) + OSMESA_MINOR_VERSION) >= 1102) && defined(OSMESA_CONTEXT_MAJOR_VERSION)
	static const int attributes_3_2_core_profile[] = {
		OSMESA_FORMAT, OSMESA_RGBA,
		OSMESA_DEPTH_BITS, 24,
		OSMESA_STENCIL_BITS, 8,
		OSMESA_ACCUM_BITS, 0,
		OSMESA_PROFILE, OSMESA_CORE_PROFILE,
		OSMESA_CONTEXT_MAJOR_VERSION, 3,
		OSMESA_CONTEXT_MINOR_VERSION, 2,
		0 };
	
	ctx = OSMesaCreateContextAttribs(attributes_3_2_core_profile, NULL);
	if (ctx == NULL)
	{
		printf("OSMesa: Could not create a 3.2 Core Profile context. Will attempt to create a 2.1 compatibility context...\n");
		
		static const int attributes_2_1[] = {
			OSMESA_FORMAT, OSMESA_RGBA,
			OSMESA_DEPTH_BITS, 24,
			OSMESA_STENCIL_BITS, 8,
			OSMESA_ACCUM_BITS, 0,
			OSMESA_PROFILE, OSMESA_COMPAT_PROFILE,
			OSMESA_CONTEXT_MAJOR_VERSION, 2,
			OSMESA_CONTEXT_MINOR_VERSION, 1,
			0 };
		
		ctx = OSMesaCreateContextAttribs(attributes_2_1, NULL);
	}
#endif

	if (ctx == NULL)
	{
		ctx = OSMesaCreateContextExt(OSMESA_RGBA, 24, 8, 0, NULL);
		if (ctx == NULL)
		{
			printf("OSMesa: OSMesaCreateContextExt() failed!\n");
			return false;
		}
	}
	
    buffer = malloc(GPU_FRAMEBUFFER_NATIVE_WIDTH * GPU_FRAMEBUFFER_NATIVE_HEIGHT * sizeof(uint32_t));
    if (!buffer)
    {
        printf("OSMesa: Could not allocate enough memory for the context!\n");
        return false;
    }

    if (!OSMesaMakeCurrent(ctx, buffer, GL_UNSIGNED_BYTE, GPU_FRAMEBUFFER_NATIVE_WIDTH, GPU_FRAMEBUFFER_NATIVE_HEIGHT))
    {
        printf("OSMesa: OSMesaMakeCurrent() failed!\n");
        free(buffer);
        return false;
    }

    oglrender_init = osmesa_init;
    oglrender_beginOpenGL = osmesa_beginOpenGL;
    oglrender_endOpenGL = osmesa_endOpenGL;

    return true;
}

bool is_osmesa_initialized(void)
{
	return (ctx != NULL);
}

#endif
