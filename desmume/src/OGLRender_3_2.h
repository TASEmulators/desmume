/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2024 DeSmuME team

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

#include "OGLRender.h"

#define MAX_CLIPPED_POLY_COUNT_FOR_UBO 16384

extern const char *GeometryVtxShader_150;
extern const char *GeometryFragShader_150;
extern const char *GeometryZeroDstAlphaPixelMaskVtxShader_150;
extern const char *GeometryZeroDstAlphaPixelMaskFragShader_150;
extern const char *MSGeometryZeroDstAlphaPixelMaskFragShader_150;
extern const char *EdgeMarkVtxShader_150;
extern const char *EdgeMarkFragShader_150;
extern const char *FogVtxShader_150;
extern const char *FogFragShader_150;
extern const char *FramebufferOutputVtxShader_150;
extern const char *FramebufferOutput6665FragShader_150;

// A port that wants to use the OpenGL 3.2 renderer must assign the two following functions
// to OGLLoadEntryPoints_3_2_Func and OGLCreateRenderer_3_2_Func, respectively.
//
// In addition, the port must add the following GPU3DInterface objects to core3DList:
// - gpu3Dgl: Automatically selects the most fully featured version of standard OpenGL that
//            is available on the host system, prefering OpenGL 3.2 Core Profile.
// - gpu3Dgl_3_2: Selects the OpenGL 3.2 Core Profile renderer, and returns an error if it
//                is not available on the host system.
//
// Finally, the port must call GPU->Set3DRendererByID() and pass in the index where
// gpu3Dgl_3_2 exists in core3DList so that the emulator can create the appropriate
// OpenGLRenderer object.
//
// Example code:
//    OGLLoadEntryPoints_3_2_Func = &OGLLoadEntryPoints_3_2;
//    OGLCreateRenderer_3_2_Func = &OGLCreateRenderer_3_2;
//    GPU3DInterface *core3DList[] = { &gpu3DNull, &gpu3DRasterize, &gpu3Dgl_3_2, NULL };
//    GPU->Set3DRendererByID(2);

void OGLLoadEntryPoints_3_2();
void OGLCreateRenderer_3_2(OpenGLRenderer **rendererPtr);

class OpenGLRenderer_3_2 : public OpenGLRenderer_2_1
{
protected:
	bool _is64kUBOSupported;
	bool _isTBOSupported;
	bool _isShaderFixedLocationSupported;
	bool _isConservativeDepthSupported;
	bool _isConservativeDepthAMDSupported;
	
	GLsync _syncBufferSetup;
	CACHE_ALIGN OGLPolyStates _pendingPolyStates[CLIPPED_POLYLIST_SIZE];
	
	virtual Render3DError CreatePBOs();
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
	virtual Render3DError CreateEdgeMarkProgram(const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFramebufferOutput8888Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	
	virtual void GetExtensionSet(std::set<std::string> *oglExtensionSet);
	virtual void _SetupGeometryShaders(const OGLGeometryFlags flags);
	virtual Render3DError EnableVertexAttributes();
	virtual Render3DError DisableVertexAttributes();
	virtual Render3DError ZeroDstAlphaPass(const POLY *rawPolyList, const CPoly *clippedPolyList, const size_t clippedPolyCount, const size_t clippedPolyOpaqueCount, bool enableAlphaBlending, size_t indexOffset, POLYGON_ATTR lastPolyAttr);
	virtual void _ResolveWorkingBackFacing();
	virtual void _ResolveGeometry();
	virtual void _ResolveFinalFramebuffer();
	virtual Render3DError ReadBackPixels();
	virtual Render3DError BeginRender(const GFX3D_State &renderState, const GFX3D_GeometryList &renderGList);
	virtual Render3DError PostprocessFramebuffer();
	
	virtual Render3DError ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthBuffer, const u8 *__restrict fogBuffer, const u8 opaquePolyID);
	virtual Render3DError ClearUsingValues(const Color4u8 &clearColor6665, const FragmentAttributes &clearAttributes);
	
	virtual void SetPolygonIndex(const size_t index);
	virtual Render3DError SetupPolygon(const POLY &thePoly, bool treatAsTranslucent, bool willChangeStencilBuffer, bool isBackFacing);
	virtual Render3DError SetupTexture(const POLY &thePoly, size_t polyRenderIndex);
	
public:
	OpenGLRenderer_3_2();
	~OpenGLRenderer_3_2();
	
	virtual Render3DError InitExtensions();
	virtual Render3DError RenderFinish();
	virtual Render3DError RenderPowerOff();
	virtual Render3DError SetFramebufferSize(size_t w, size_t h);
};

#endif
