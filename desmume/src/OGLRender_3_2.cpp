/*
	Copyright (C) 2006 yopyop
	Copyright (C) 2006-2007 shash
	Copyright (C) 2008-2015 DeSmuME team

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

#include "OGLRender_3_2.h"

#include <stdlib.h>
#include <string.h>

#include "debug.h"
#include "gfx3d.h"
#include "NDSSystem.h"
#include "texcache.h"

//------------------------------------------------------------

// Basic Functions
OGLEXT(PFNGLGETSTRINGIPROC, glGetStringi) // Core in v3.0

// Shaders
OGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation) // Core in v3.0

// FBO
OGLEXT(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) // Core in v3.0
OGLEXT(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) // Core in v3.0
OGLEXT(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) // Core in v3.0
OGLEXT(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) // Core in v3.0
OGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) // Core in v3.0
OGLEXT(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) // Core in v3.0
OGLEXT(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) // Core in v3.0
OGLEXT(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) // Core in v3.0
OGLEXT(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) // Core in v3.0
OGLEXT(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) // Core in v3.0
OGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) // Core in v3.0
OGLEXT(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) // Core in v3.0

void OGLLoadEntryPoints_3_2()
{
	// Basic Functions
	INITOGLEXT(PFNGLGETSTRINGIPROC, glGetStringi)
	
	// Shaders
	INITOGLEXT(PFNGLBINDFRAGDATALOCATIONPROC, glBindFragDataLocation)
	
	// FBO
	INITOGLEXT(PFNGLGENFRAMEBUFFERSPROC, glGenFramebuffers) // Promote to core version
	INITOGLEXT(PFNGLBINDFRAMEBUFFERPROC, glBindFramebuffer) // Promote to core version
	INITOGLEXT(PFNGLFRAMEBUFFERRENDERBUFFERPROC, glFramebufferRenderbuffer) // Promote to core version
	INITOGLEXT(PFNGLFRAMEBUFFERTEXTURE2DPROC, glFramebufferTexture2D) // Promote to core version
	INITOGLEXT(PFNGLCHECKFRAMEBUFFERSTATUSPROC, glCheckFramebufferStatus) // Promote to core version
	INITOGLEXT(PFNGLDELETEFRAMEBUFFERSPROC, glDeleteFramebuffers) // Promote to core version
	INITOGLEXT(PFNGLBLITFRAMEBUFFERPROC, glBlitFramebuffer) // Promote to core version
	INITOGLEXT(PFNGLGENRENDERBUFFERSPROC, glGenRenderbuffers) // Promote to core version
	INITOGLEXT(PFNGLBINDRENDERBUFFERPROC, glBindRenderbuffer) // Promote to core version
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEPROC, glRenderbufferStorage) // Promote to core version
	INITOGLEXT(PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC, glRenderbufferStorageMultisample) // Promote to core version
	INITOGLEXT(PFNGLDELETERENDERBUFFERSPROC, glDeleteRenderbuffers) // Promote to core version
}

// Vertex Shader GLSL 1.50
static const char *vertexShader_150 = {"\
	#version 150 \n\
	\n\
	in vec4 inPosition; \n\
	in vec2 inTexCoord0; \n\
	in vec3 inColor; \n\
	\n\
	uniform float polyAlpha; \n\
	uniform vec2 polyTexScale; \n\
	\n\
	out vec4 vtxPosition; \n\
	out vec2 vtxTexCoord; \n\
	out vec4 vtxColor; \n\
	\n\
	void main() \n\
	{ \n\
		mat2 texScaleMtx	= mat2(	vec2(polyTexScale.x,            0.0), \n\
									vec2(           0.0, polyTexScale.y)); \n\
		\n\
		vtxPosition = inPosition; \n\
		vtxTexCoord = texScaleMtx * inTexCoord0; \n\
		vtxColor = vec4(inColor * 4.0, polyAlpha); \n\
		\n\
		gl_Position = vtxPosition; \n\
	} \n\
"};

// Fragment Shader GLSL 1.50
static const char *fragmentShader_150 = {"\
	#version 150 \n\
	\n\
	in vec4 vtxPosition; \n\
	in vec2 vtxTexCoord; \n\
	in vec4 vtxColor; \n\
	\n\
	uniform sampler2D texRenderObject; \n\
	uniform sampler1D texToonTable; \n\
	\n\
	uniform int stateToonShadingMode; \n\
	uniform bool stateEnableAlphaTest; \n\
	uniform bool stateEnableAntialiasing;\n\
	uniform bool stateEnableEdgeMarking;\n\
	uniform bool stateEnableFogAlphaOnly;\n\
	uniform bool stateUseWDepth; \n\
	uniform float stateAlphaTestRef; \n\
	\n\
	uniform int polyMode; \n\
	uniform bool polySetNewDepthForTranslucent;\n\
	uniform int polyID; \n\
	\n\
	uniform bool polyEnableTexture; \n\
	uniform bool polyEnableFog;\n\
	\n\
	out vec4 outFragColor; \n\
	out vec4 outFragDepth;\n\
	out vec4 outFogAttributes;\n\
	\n\
	void main() \n\
	{ \n\
		vec4 mainTexColor = (polyEnableTexture) ? texture(texRenderObject, vtxTexCoord) : vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 newFragColor = mainTexColor * vtxColor; \n\
		\n\
		if(polyMode == 1) \n\
		{ \n\
			newFragColor.rgb = (polyEnableTexture) ? mix(vtxColor.rgb, mainTexColor.rgb, mainTexColor.a) : vtxColor.rgb; \n\
			newFragColor.a = vtxColor.a; \n\
		} \n\
		else if(polyMode == 2) \n\
		{ \n\
			vec3 toonColor = vec3(texture(texToonTable, vtxColor.r).rgb); \n\
			newFragColor.rgb = (stateToonShadingMode == 0) ? mainTexColor.rgb * toonColor.rgb : min((mainTexColor.rgb * vtxColor.rgb) + toonColor.rgb, 1.0); \n\
		} \n\
		else if(polyMode == 3) \n\
		{ \n\
			if (polyID != 0) \n\
			{ \n\
				newFragColor = vtxColor; \n\
			} \n\
		} \n\
		\n\
		if (newFragColor.a == 0.0 || (stateEnableAlphaTest && newFragColor.a < stateAlphaTestRef)) \n\
		{ \n\
			discard; \n\
		} \n\
		\n\
		float vertW = (vtxPosition.w == 0.0) ? 0.00000001 : vtxPosition.w; \n\
		float newFragDepth = (stateUseWDepth) ? vtxPosition.w/4096.0 : clamp((vtxPosition.z/vertW) * 0.5 + 0.5, 0.0, 1.0); \n\
		\n\
		outFragColor = newFragColor;\n\
		outFragDepth = vec4( vec3(newFragDepth), 1.0);\n\
		outFogAttributes = vec4( float(polyEnableFog), float(stateEnableFogAlphaOnly), 0.0, 1.0);\n\
		gl_FragDepth = newFragDepth;\n\
	} \n\
"};

void OGLCreateRenderer_3_2(OpenGLRenderer **rendererPtr)
{
	if (IsVersionSupported(3, 2, 0))
	{
		*rendererPtr = new OpenGLRenderer_3_2;
		(*rendererPtr)->SetVersion(3, 2, 0);
	}
}

OpenGLRenderer_3_2::~OpenGLRenderer_3_2()
{
	glFinish();
	
	DestroyVAOs();
	DestroyFBOs();
	DestroyMultisampledFBO();
}

Render3DError OpenGLRenderer_3_2::InitExtensions()
{
	Render3DError error = OGLERROR_NOERR;
	
	// Get OpenGL extensions
	std::set<std::string> oglExtensionSet;
	this->GetExtensionSet(&oglExtensionSet);
	
	// Initialize OpenGL
	this->InitTables();
	
	// Load and create shaders. Return on any error, since v3.2 Core Profile makes shaders mandatory.
	this->isShaderSupported	= true;
	
	std::string vertexShaderProgram;
	std::string fragmentShaderProgram;
	error = this->LoadShaderPrograms(&vertexShaderProgram, &fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	error = this->CreateShaders(&vertexShaderProgram, &fragmentShaderProgram);
	if (error != OGLERROR_NOERR)
	{
		this->isShaderSupported = false;
		return error;
	}
	
	this->CreateToonTable();
	
	this->isVBOSupported = true;
	this->CreateVBOs();
	
	this->isPBOSupported = true;
	this->CreatePBOs();
	
	this->isVAOSupported = true;
	this->CreateVAOs();
	
	// Load and create FBOs. Return on any error, since v3.2 Core Profile makes FBOs mandatory.
	this->isFBOSupported = true;
	error = this->CreateFBOs();
	if (error != OGLERROR_NOERR)
	{
		this->isFBOSupported = false;
		return error;
	}
	
	this->isMultisampledFBOSupported = true;
	error = this->CreateMultisampledFBO();
	if (error != OGLERROR_NOERR)
	{
		this->isMultisampledFBOSupported = false;
		
		if (error == OGLERROR_FBO_CREATE_ERROR)
		{
			// Return on OGLERROR_FBO_CREATE_ERROR, since a v3.0 driver should be able to
			// support FBOs.
			return error;
		}
	}
	
	this->InitTextures();
	this->InitFinalRenderStates(&oglExtensionSet); // This must be done last
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::CreateFBOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenTextures(1, &OGLRef.texGColorID);
	glGenTextures(1, &OGLRef.texGDepthStencilID);
	glGenTextures(1, &OGLRef.texGDepthID);
	glGenTextures(1, &OGLRef.texGFogAttrID);
	glGenTextures(1, &OGLRef.texPostprocessID);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGColorID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthStencilID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGDepthID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texGFogAttrID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, OGLRef.texPostprocessID);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8_REV, NULL);
	
	glBindTexture(GL_TEXTURE_2D, 0);
	
	// Set up FBOs
	glGenFramebuffers(1, &OGLRef.fboRenderID);
	glGenFramebuffers(1, &OGLRef.fboPostprocessID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboRenderID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texGColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_TEXTURE_2D, OGLRef.texGDepthID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_TEXTURE_2D, OGLRef.texGFogAttrID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texGDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texPostprocessID);
		
		OGLRef.fboRenderID = 0;
		OGLRef.fboPostprocessID = 0;
		OGLRef.fboOutputID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffers(3, RenderDrawList);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboPostprocessID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texPostprocessID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboRenderID);
		glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
		glDeleteTextures(1, &OGLRef.texGColorID);
		glDeleteTextures(1, &OGLRef.texGDepthStencilID);
		glDeleteTextures(1, &OGLRef.texGDepthID);
		glDeleteTextures(1, &OGLRef.texGFogAttrID);
		glDeleteTextures(1, &OGLRef.texPostprocessID);
		
		OGLRef.fboRenderID = 0;
		OGLRef.fboPostprocessID = 0;
		OGLRef.fboOutputID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glDrawBuffer(GL_COLOR_ATTACHMENT0);
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
	OGLRef.fboOutputID = OGLRef.fboPostprocessID;
	OGLRef.selectedRenderingFBO = OGLRef.fboRenderID;
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	INFO("OpenGL: Successfully created FBOs.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::DestroyFBOs()
{
	if (!this->isFBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &OGLRef.fboRenderID);
	glDeleteFramebuffers(1, &OGLRef.fboPostprocessID);
	glDeleteTextures(1, &OGLRef.texGColorID);
	glDeleteTextures(1, &OGLRef.texGDepthStencilID);
	glDeleteTextures(1, &OGLRef.texGDepthID);
	glDeleteTextures(1, &OGLRef.texGFogAttrID);
	glDeleteTextures(1, &OGLRef.texPostprocessID);
	
	OGLRef.fboRenderID = 0;
	OGLRef.fboPostprocessID = 0;
	OGLRef.fboOutputID = 0;
	
	this->isFBOSupported = false;
}

Render3DError OpenGLRenderer_3_2::CreateMultisampledFBO()
{
	// Check the maximum number of samples that the GPU supports and use that.
	// Since our target resolution is only 256x192 pixels, using the most samples
	// possible is the best thing to do.
	GLint maxSamples = 0;
	glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
	
	if (maxSamples < 2)
	{
		INFO("OpenGL: GPU does not support at least 2x multisampled FBOs. Multisample antialiasing will be disabled.\n");
		return OGLERROR_FEATURE_UNSUPPORTED;
	}
	else if (maxSamples > OGLRENDER_MAX_MULTISAMPLES)
	{
		maxSamples = OGLRENDER_MAX_MULTISAMPLES;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	// Set up FBO render targets
	glGenRenderbuffers(1, &OGLRef.rboMSGColorID);
	glGenRenderbuffers(1, &OGLRef.rboMSGDepthID);
	glGenRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glGenRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGColorID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_DEPTH24_STENCIL8, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT);
	
	// Set up multisampled rendering FBO
	glGenFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OGLRef.rboMSGColorID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT1, GL_RENDERBUFFER, OGLRef.rboMSGDepthID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT2, GL_RENDERBUFFER, OGLRef.rboMSGFogAttrID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMSGDepthStencilID);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGColorID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
		glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
		
		OGLRef.fboMSIntermediateRenderID = 0;
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboOutputID);
	INFO("OpenGL: Successfully created multisampled FBO.\n");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::DestroyMultisampledFBO()
{
	if (!this->isMultisampledFBOSupported)
	{
		return;
	}
	
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &OGLRef.fboMSIntermediateRenderID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGColorID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGFogAttrID);
	glDeleteRenderbuffers(1, &OGLRef.rboMSGDepthStencilID);
	
	OGLRef.fboMSIntermediateRenderID = 0;
	
	this->isMultisampledFBOSupported = false;
}

Render3DError OpenGLRenderer_3_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoMainStatesID);
	glBindVertexArray(OGLRef.vaoMainStatesID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboGeometryVtxID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboGeometryIndexID);
	
	glEnableVertexAttribArray(OGLVertexAttributeID_Position);
	glEnableVertexAttribArray(OGLVertexAttributeID_TexCoord0);
	glEnableVertexAttribArray(OGLVertexAttributeID_Color);
	
	glVertexAttribPointer(OGLVertexAttributeID_Position, 4, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, coord));
	glVertexAttribPointer(OGLVertexAttributeID_TexCoord0, 2, GL_FLOAT, GL_FALSE, sizeof(VERT), (const GLvoid *)offsetof(VERT, texcoord));
	glVertexAttribPointer(OGLVertexAttributeID_Color, 3, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(VERT), (const GLvoid *)offsetof(VERT, color));
	
	glBindVertexArray(0);
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::DestroyVAOs()
{
	if (!this->isVAOSupported)
	{
		return;
	}
	
	glBindVertexArray(0);
	glDeleteVertexArrays(1, &this->ref->vaoMainStatesID);
	
	this->isVAOSupported = false;
}

Render3DError OpenGLRenderer_3_2::LoadShaderPrograms(std::string *outVertexShaderProgram, std::string *outFragmentShaderProgram)
{
	*outVertexShaderProgram = std::string(vertexShader_150);
	*outFragmentShaderProgram = std::string(fragmentShader_150);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::SetupShaderIO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.programGeometryID, OGLVertexAttributeID_Color, "inColor");
	glBindFragDataLocation(OGLRef.programGeometryID, 0, "outFragColor");
	glBindFragDataLocation(OGLRef.programGeometryID, 1, "outFragDepth");
	glBindFragDataLocation(OGLRef.programGeometryID, 2, "outFogAttributes");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::GetExtensionSet(std::set<std::string> *oglExtensionSet)
{
	GLint extensionCount = 0;
	
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
	for (size_t i = 0; i < extensionCount; i++)
	{
		std::string extensionName = std::string((const char *)glGetStringi(GL_EXTENSIONS, i));
		oglExtensionSet->insert(extensionName);
	}
}

Render3DError OpenGLRenderer_3_2::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const size_t vertIndexCount)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindVertexArray(OGLRef.vaoMainStatesID);
	glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(VERT) * vertList->count, vertList);
	glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, 0, vertIndexCount * sizeof(GLushort), indexBuffer);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DisableVertexAttributes()
{
	glBindVertexArray(0);
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::SelectRenderingFramebuffer()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	OGLRef.selectedRenderingFBO = (CommonSettings.GFX3D_Renderer_Multisample) ? OGLRef.fboMSIntermediateRenderID : OGLRef.fboRenderID;
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glDrawBuffers(3, RenderDrawList);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboOutputID);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboOutputID);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingImage(const u16 *__restrict colorBuffer, const u32 *__restrict depthStencilBuffer, const bool *__restrict fogBuffer)
{
	OGLRenderRef &OGLRef = *this->ref;
	
	this->UploadClearImage(colorBuffer, depthStencilBuffer, fogBuffer);
	
	if (OGLRef.selectedRenderingFBO == OGLRef.fboMSIntermediateRenderID)
	{
		glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboRenderID);
		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboMSIntermediateRenderID);
		
		// Blit the color buffer
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffer(GL_COLOR_ATTACHMENT0);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
		
		// Blit the working depth buffer
		glReadBuffer(GL_COLOR_ATTACHMENT1);
		glDrawBuffer(GL_COLOR_ATTACHMENT1);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Blit the fog buffer
		glReadBuffer(GL_COLOR_ATTACHMENT2);
		glDrawBuffer(GL_COLOR_ATTACHMENT2);
		glBlitFramebuffer(0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, 0, 0, GFX3D_FRAMEBUFFER_WIDTH, GFX3D_FRAMEBUFFER_HEIGHT, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		
		// Reset framebuffer targets
		glReadBuffer(GL_COLOR_ATTACHMENT0);
		glDrawBuffers(3, RenderDrawList);
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	}
	
	return OGLERROR_NOERR;
}
