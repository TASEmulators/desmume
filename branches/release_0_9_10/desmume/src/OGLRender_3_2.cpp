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
	uniform vec2 texScale; \n\
	\n\
	out vec4 vtxPosition; \n\
	out vec2 vtxTexCoord; \n\
	out vec4 vtxColor; \n\
	\n\
	void main() \n\
	{ \n\
		mat2 texScaleMtx	= mat2(	vec2(texScale.x,        0.0), \n\
									vec2(       0.0, texScale.y)); \n\
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
	uniform sampler2D texMainRender; \n\
	uniform sampler1D texToonTable; \n\
	uniform int polyID; \n\
	uniform bool hasTexture; \n\
	uniform int polygonMode; \n\
	uniform int toonShadingMode; \n\
	uniform int oglWBuffer; \n\
	uniform bool enableAlphaTest; \n\
	uniform float alphaTestRef; \n\
	\n\
	out vec4 outFragColor; \n\
	\n\
	void main() \n\
	{ \n\
		vec4 texColor = vec4(1.0, 1.0, 1.0, 1.0); \n\
		vec4 fragColor; \n\
		float fragDepth; \n\
		\n\
		if(hasTexture) \n\
		{ \n\
			texColor = texture(texMainRender, vtxTexCoord); \n\
		} \n\
		\n\
		fragColor = texColor; \n\
		\n\
		if(polygonMode == 0) \n\
		{ \n\
			fragColor = vtxColor * texColor; \n\
		} \n\
		else if(polygonMode == 1) \n\
		{ \n\
			if (texColor.a == 0.0 || !hasTexture) \n\
			{ \n\
				fragColor.rgb = vtxColor.rgb; \n\
			} \n\
			else if (texColor.a == 1.0) \n\
			{ \n\
				fragColor.rgb = texColor.rgb; \n\
			} \n\
			else \n\
			{ \n\
				fragColor.rgb = texColor.rgb * (1.0-texColor.a) + vtxColor.rgb * texColor.a; \n\
			} \n\
			\n\
			fragColor.a = vtxColor.a; \n\
		} \n\
		else if(polygonMode == 2) \n\
		{ \n\
			vec3 toonColor = vec3(texture(texToonTable, vtxColor.r).rgb); \n\
			\n\
			if (toonShadingMode == 0) \n\
			{ \n\
				fragColor.rgb = texColor.rgb * toonColor.rgb; \n\
			} \n\
			else \n\
			{ \n\
				fragColor.rgb = texColor.rgb * vtxColor.rgb + toonColor.rgb; \n\
			} \n\
			\n\
			fragColor.a = texColor.a * vtxColor.a; \n\
		} \n\
		else if(polygonMode == 3) \n\
		{ \n\
			if (polyID != 0) \n\
			{ \n\
				fragColor = vtxColor; \n\
			} \n\
		} \n\
		\n\
		if (fragColor.a == 0.0 || (enableAlphaTest && fragColor.a < alphaTestRef)) \n\
		{ \n\
			discard; \n\
		} \n\
		\n\
		if (oglWBuffer == 1) \n\
		{ \n\
			// TODO \n\
			fragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		else \n\
		{ \n\
			fragDepth = (vtxPosition.z / vtxPosition.w) * 0.5 + 0.5; \n\
		} \n\
		\n\
		outFragColor = fragColor; \n\
		gl_FragDepth = fragDepth; \n\
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
	OGLRenderRef &OGLRef = *this->ref;
	
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
		OGLRef.fboFinalOutputID = 0;
		this->isFBOSupported = false;
		return error;
	}
	
	this->isMultisampledFBOSupported = true;
	error = this->CreateMultisampledFBO();
	if (error != OGLERROR_NOERR)
	{
		OGLRef.selectedRenderingFBO = 0;
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
	this->CreateClearImage();
	
	glGenRenderbuffers(1, &OGLRef.rboFinalOutputColorID);
	glGenRenderbuffers(1, &OGLRef.rboFinalOutputDepthStencilID);
	
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboFinalOutputColorID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_RGBA, 256, 192);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboFinalOutputDepthStencilID);
	glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, 256, 192);
	
	// Set up FBOs
	glGenFramebuffers(1, &OGLRef.fboClearImageID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboClearImageID);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, OGLRef.texClearImageColorID, 0);
	glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, OGLRef.texClearImageDepthStencilID, 0);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
		this->DestroyClearImage();
		
		glDeleteRenderbuffers(1, &OGLRef.rboFinalOutputColorID);
		glDeleteRenderbuffers(1, &OGLRef.rboFinalOutputDepthStencilID);
		
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	// Set up final output FBO
	glGenFramebuffers(1, &OGLRef.fboFinalOutputID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboFinalOutputID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OGLRef.rboFinalOutputColorID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboFinalOutputDepthStencilID);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		INFO("OpenGL: Failed to created FBOs. Some emulation features will be disabled.\n");
		
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		
		glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
		this->DestroyClearImage();
		
		glDeleteFramebuffers(1, &OGLRef.fboFinalOutputID);
		glDeleteRenderbuffers(1, &OGLRef.rboFinalOutputColorID);
		glDeleteRenderbuffers(1, &OGLRef.rboFinalOutputDepthStencilID);
		
		OGLRef.fboFinalOutputID = 0;
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glReadBuffer(GL_COLOR_ATTACHMENT0);
	
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
	
	glDeleteFramebuffers(1, &OGLRef.fboClearImageID);
	this->DestroyClearImage();
	
	glDeleteFramebuffers(1, &OGLRef.fboFinalOutputID);
	glDeleteRenderbuffers(1, &OGLRef.rboFinalOutputColorID);
	glDeleteRenderbuffers(1, &OGLRef.rboFinalOutputDepthStencilID);
	
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
	glGenRenderbuffers(1, &OGLRef.rboMultisampleColorID);
	glGenRenderbuffers(1, &OGLRef.rboMultisampleDepthStencilID);
	
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMultisampleColorID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_RGBA, 256, 192);
	glBindRenderbuffer(GL_RENDERBUFFER, OGLRef.rboMultisampleDepthStencilID);
	glRenderbufferStorageMultisample(GL_RENDERBUFFER, maxSamples, GL_DEPTH24_STENCIL8, 256, 192);
	
	// Set up multisampled rendering FBO
	glGenFramebuffers(1, &OGLRef.fboMultisampleRenderID);
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboMultisampleRenderID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, OGLRef.rboMultisampleColorID);
	glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, OGLRef.rboMultisampleDepthStencilID);
	
	if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
	{
		glBindFramebuffer(GL_FRAMEBUFFER, 0);
		glDeleteFramebuffers(1, &OGLRef.fboMultisampleRenderID);
		glDeleteRenderbuffers(1, &OGLRef.rboMultisampleColorID);
		glDeleteRenderbuffers(1, &OGLRef.rboMultisampleDepthStencilID);
		
		INFO("OpenGL: Failed to create multisampled FBO. Multisample antialiasing will be disabled.\n");
		return OGLERROR_FBO_CREATE_ERROR;
	}
	
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboFinalOutputID);
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
	glDeleteFramebuffers(1, &OGLRef.fboMultisampleRenderID);
	glDeleteRenderbuffers(1, &OGLRef.rboMultisampleColorID);
	glDeleteRenderbuffers(1, &OGLRef.rboMultisampleDepthStencilID);
	
	this->isMultisampledFBOSupported = false;
}

Render3DError OpenGLRenderer_3_2::CreateVAOs()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glGenVertexArrays(1, &OGLRef.vaoMainStatesID);
	glBindVertexArray(OGLRef.vaoMainStatesID);
	
	glBindBuffer(GL_ARRAY_BUFFER, OGLRef.vboVertexID);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, OGLRef.iboIndexID);
	
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
	
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_Position, "inPosition");
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_TexCoord0, "inTexCoord0");
	glBindAttribLocation(OGLRef.shaderProgram, OGLVertexAttributeID_Color, "inColor");
	glBindFragDataLocation(OGLRef.shaderProgram, 0, "outFragColor");
	
	return OGLERROR_NOERR;
}

void OpenGLRenderer_3_2::GetExtensionSet(std::set<std::string> *oglExtensionSet)
{
	GLint extensionCount = 0;
	
	glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);
	for (unsigned int i = 0; i < extensionCount; i++)
	{
		std::string extensionName = std::string((const char *)glGetStringi(GL_EXTENSIONS, i));
		oglExtensionSet->insert(extensionName);
	}
}

Render3DError OpenGLRenderer_3_2::EnableVertexAttributes(const VERTLIST *vertList, const GLushort *indexBuffer, const unsigned int vertIndexCount)
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
	
	if (this->isMultisampledFBOSupported)
	{
		OGLRef.selectedRenderingFBO = CommonSettings.GFX3D_Renderer_Multisample ? OGLRef.fboMultisampleRenderID : OGLRef.fboFinalOutputID;
		glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
		
		const GLenum drawBufferList = {GL_COLOR_ATTACHMENT0};
		glDrawBuffers(1, &drawBufferList);
	}
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::DownsampleFBO()
{
	OGLRenderRef &OGLRef = *this->ref;
	
	if (!this->isMultisampledFBOSupported || OGLRef.selectedRenderingFBO != OGLRef.fboMultisampleRenderID)
	{
		return OGLERROR_NOERR;
	}
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.fboFinalOutputID);
	glBlitFramebuffer(0, 0, 256, 192, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.fboFinalOutputID);
	
	return OGLERROR_NOERR;
}

Render3DError OpenGLRenderer_3_2::ClearUsingImage() const
{
	OGLRenderRef &OGLRef = *this->ref;
	
	glBindFramebuffer(GL_READ_FRAMEBUFFER, OGLRef.fboClearImageID);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	glBlitFramebuffer(0, 0, 256, 192, 0, 0, 256, 192, GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT, GL_NEAREST);
	glBindFramebuffer(GL_FRAMEBUFFER, OGLRef.selectedRenderingFBO);
	
	return OGLERROR_NOERR;
}
