/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2013 DeSmuME team

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

#ifndef OGLRENDER_3_2_H
#define OGLRENDER_3_2_H

#if defined(_WIN32)
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <GL/gl.h>
	#include <GL/glcorearb.h>

	#define OGLEXT(procPtr, func)		procPtr func = NULL;
	#define INITOGLEXT(procPtr, func)	func = (procPtr)wglGetProcAddress(#func);
	#define EXTERNOGLEXT(procPtr, func)	extern procPtr func;
#elif defined(__APPLE__)
	#include <OpenGL/gl3.h>
	#include <OpenGL/gl3ext.h>

	// Ignore dynamic linking on Apple OS
	#define OGLEXT(procPtr, func)
	#define INITOGLEXT(procPtr, func)
	#define EXTERNOGLEXT(procPtr, func)
#else
	#include <GL/gl.h>
	#include <GL/glcorearb.h>
	#include <GL/glx.h>

	#define OGLEXT(procPtr, func)		procPtr func = NULL;
	#define INITOGLEXT(procPtr, func)	func = (procPtr)glXGetProcAddress((const GLubyte *) #func);
	#define EXTERNOGLEXT(procPtr, func)	extern procPtr func;
#endif

// Check minimum OpenGL header version
#if !defined(GL_VERSION_3_2)
	#error OpenGL requires v3.2 headers or later.
#endif

#include "OGLRender.h"

void OGLLoadEntryPoints_3_2();
void OGLCreateRenderer_3_2(OpenGLRenderer **rendererPtr);

class OpenGLRenderer_3_2 : public OpenGLRenderer_2_1
{
protected:
	virtual Render3DError InitExtensions();
	virtual Render3DError CreateFBOs();
	virtual void DestroyFBOs();
	virtual Render3DError CreateMultisampledFBO();
	virtual void DestroyMultisampledFBO();
	virtual Render3DError CreateVAOs();
	virtual void DestroyVAOs();
	
	virtual Render3DError LoadShaderPrograms(std::string *outVertexShaderProgram, std::string *outFragmentShaderProgram);
	virtual Render3DError SetupShaderIO();
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual Render3DError EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount);
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError SelectRenderingFramebuffer();
	virtual Render3DError DownsampleFBO();
	
	virtual Render3DError ClearUsingImage() const;
	
public:
	~OpenGLRenderer_3_2();
};

#endif
