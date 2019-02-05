/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2019 DeSmuME team

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
	#include <GL/glext.h>
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
	#include <GL/glext.h>
	#include <GL/glx.h>
	#include "utils/glcorearb.h"

	#define OGLEXT(procPtr, func)		procPtr func = NULL;
	#define INITOGLEXT(procPtr, func)	func = (procPtr)glXGetProcAddress((const GLubyte *) #func);
	#define EXTERNOGLEXT(procPtr, func)	extern procPtr func;
#endif

// Check minimum OpenGL header version
#if !defined(GL_VERSION_3_2)
	#error OpenGL requires v3.2 headers or later.
#endif

#include "OGLRender.h"

#define MAX_CLIPPED_POLY_COUNT_FOR_UBO 16384

void OGLLoadEntryPoints_3_2();
void OGLCreateRenderer_3_2(OpenGLRenderer **rendererPtr);

class OpenGLRenderer_3_2 : public OpenGLRenderer_2_1
{
protected:
	bool _is64kUBOSupported;
	bool _isDualSourceBlendingSupported;
	bool _isSampleShadingSupported;
	bool _isConservativeDepthSupported;
	bool _isConservativeDepthAMDSupported;
	
	GLsync _syncBufferSetup;
	CACHE_ALIGN OGLPolyStates _pendingPolyStates[POLYLIST_SIZE];
	
	virtual Render3DError InitExtensions();
	
	virtual Render3DError CreateFBOs();
	virtual void DestroyFBOs();
	virtual Render3DError CreateMultisampledFBO(GLsizei numSamples);
	virtual void DestroyMultisampledFBO();
	virtual void ResizeMultisampledFBOs(GLsizei numSamples);
	virtual Render3DError CreateVAOs();
	virtual void DestroyVAOs();
	
	virtual Render3DError CreateGeometryPrograms();
	virtual void DestroyGeometryPrograms();
	virtual Render3DError CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateMSGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual void DestroyMSGeometryZeroDstAlphaProgram();
	virtual Render3DError CreateEdgeMarkProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFramebufferOutput8888Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual Render3DError InitFinalRenderStates(const std::set<std::string> *oglExtensionSet);
	virtual void _SetupGeometryShaders(const OGLGeometryFlags flags);
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError ZeroDstAlphaPass(const CPoly *clippedPolyList, const size_t clippedPolyCount, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr);
	virtual void _ResolveWorkingBackFacing();
	virtual void _ResolveGeometry();
	virtual Render3DError ReadBackPixels();
	virtual Render3DError BeginRender(const GFX3D &engine);
	virtual Render3DError PostprocessFramebuffer();
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	virtual Render3DError ClearUsingValues(const FragmentColor &clearColor6665, const FragmentAttributes &clearAttributes);
	
	virtual void SetPolygonIndex(const size_t index);
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer);
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
	
public:
	OpenGLRenderer_3_2();
	~OpenGLRenderer_3_2();
	
	virtual Render3DError RenderPowerOff();
};

#endif
