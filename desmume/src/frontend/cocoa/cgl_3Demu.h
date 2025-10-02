/*
	Copyright (C) 2025 DeSmuME team

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

#ifndef _CGL_3DEMU_H_
#define _CGL_3DEMU_H_

#include <stddef.h>
#include <stdint.h>

// Struct to hold renderer info
struct ClientRendererInfo
{
	int32_t rendererID;      // Renderer ID, used to associate a renderer with a display device or virtual screen
	int32_t accelerated;     // Hardware acceleration flag, 0 = Software only, 1 = Has hardware acceleration
	int32_t displayID;       // Display ID, used to associate a display device with a renderer
	int32_t online;          // Online flag, 0 = No display device associated, 1 = Display device associated
	int32_t removable;       // Removable flag, used to indicate if the renderer is removable (like an eGPU), 0 = Fixed, 1 = Removable
	int32_t virtualScreen;   // Virtual screen index, used to associate a virtual screen with a renderer
	int32_t videoMemoryMB;   // The total amount of VRAM available to this renderer
	int32_t textureMemoryMB; // The amount of VRAM available to this renderer for texture usage
	char vendor[256];        // C-string copy of the host renderer's vendor
	char name[256];          // C-string copy of the host renderer's name
	const void *vendorStr;   // Pointer to host renderer's vendor string (parsing this is implementation dependent)
	const void *nameStr;     // Pointer to host renderer's name string (parsing this is implementation dependent)
};
typedef struct ClientRendererInfo ClientRendererInfo;

bool cgl_initOpenGL_StandardAuto();
bool cgl_initOpenGL_LegacyAuto();
bool cgl_initOpenGL_3_2_CoreProfile();

void cgl_deinitOpenGL();
bool cgl_beginOpenGL();
void cgl_endOpenGL();
bool cgl_framebufferDidResizeCallback(bool isFBOSupported, size_t w, size_t h);

int cgl_getHostRendererID();
const char* cgl_getHostRendererString();

#endif // _CGL_3DEMU_H_
