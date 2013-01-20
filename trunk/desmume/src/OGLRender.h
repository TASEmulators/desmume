/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2009 DeSmuME team

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

#ifndef OGLRENDER_H
#define OGLRENDER_H

#include "render3D.h"

// Define minimum required OpenGL version for GPU (Legacy Renderer)
#define OGLRENDER_LEGACY_MINIMUM_GPU_VERSION_REQUIRED_MAJOR		1
#define OGLRENDER_LEGACY_MINIMUM_GPU_VERSION_REQUIRED_MINOR		2
#define OGLRENDER_LEGACY_MINIMUM_GPU_VERSION_REQUIRED_REVISION	0

// Define minimum required OpenGL version for GPU (Modern Renderer)
#define OGLRENDER_MINIMUM_GPU_VERSION_REQUIRED_MAJOR			3
#define OGLRENDER_MINIMUM_GPU_VERSION_REQUIRED_MINOR			0
#define OGLRENDER_MINIMUM_GPU_VERSION_REQUIRED_REVISION			0

#define OGLRENDER_MAX_MULTISAMPLES			16
#define OGLRENDER_VERT_INDEX_BUFFER_SIZE	8192

extern GPU3DInterface gpu3Dgl;

//This is called by OGLRender whenever it initializes.
//Platforms, please be sure to set this up.
//return true if you successfully init.
extern bool (*oglrender_init)();

//This is called by OGLRender before it uses opengl.
//return true if youre OK with using opengl
extern bool (*oglrender_beginOpenGL)();

//This is called by OGLRender after it is done using opengl.
extern void (*oglrender_endOpenGL)();

#endif
