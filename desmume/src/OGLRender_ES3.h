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

#ifndef OGLRENDER_ES3_H
#define OGLRENDER_ES3_H

#include "OGLRender_3_2.h"

// A port that wants to use the OpenGL ES renderer must assign the two following functions
// to OGLLoadEntryPoints_ES_3_0_Func and OGLCreateRenderer_ES_3_0_Func, respectively.
//
// In addition, the port must add the following GPU3DInterface objects to core3DList:
// - gpu3Dgl_ES_3_0: Selects the OpenGL ES 3.0 renderer, and returns an error if it is
//                   not available on the host system.
//
// Finally, the port must call GPU->Set3DRendererByID() and pass in the index where
// gpu3Dgl_ES_3_0 exists in core3DList so that the emulator can create the appropriate
// OpenGLRenderer object.
//
// Example code:
//    OGLLoadEntryPoints_ES_3_0_Func = &OGLLoadEntryPoints_ES_3_0;
//    OGLCreateRenderer_ES_3_0_Func = &OGLCreateRenderer_ES_3_0;
//    GPU3DInterface *core3DList[] = { &gpu3DNull, &gpu3DRasterize, &gpu3Dgl_ES_3_0, NULL };
//    GPU->Set3DRendererByID(2);

void OGLLoadEntryPoints_ES_3_0();
void OGLCreateRenderer_ES_3_0(OpenGLRenderer **rendererPtr);

class OpenGLESRenderer_3_0 : public OpenGLRenderer_3_2
{
protected:
	virtual Render3DError CreateGeometryPrograms();
	virtual Render3DError CreateGeometryZeroDstAlphaProgram(const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateEdgeMarkProgram(const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFogProgram(const OGLFogProgramKey fogProgramKey, const bool isMultisample, const char *vtxShaderCString, const char *fragShaderCString);
	virtual Render3DError CreateFramebufferOutput6665Program(const size_t outColorIndex, const char *vtxShaderCString, const char *fragShaderCString);
	
public:
	OpenGLESRenderer_3_0();
	
	virtual Render3DError InitExtensions();
};

#endif // OGLRENDER_ES3_H
